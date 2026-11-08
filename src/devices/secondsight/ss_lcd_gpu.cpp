#include "ss_lcd_gpu.hpp"
#include "ss_lcd.hpp"

#include <cstdint>
#include <cstdio>

#if !defined(__EMSCRIPTEN__)

bool SecondSightLcdGpu::init(SDL_Renderer *renderer, SDL_GPUDevice *device) {
    renderer_ = renderer;
    ready_ = pass_.create(renderer, device, "lcd", 1, 1);
    return ready_;
}

void SecondSightLcdGpu::shutdown(SDL_GPUDevice *device) {
    pass_.destroy(device);
    ready_ = false;
}

bool SecondSightLcdGpu::run(SDL_Texture *source, SDL_Texture *target, const Geometry &geometry) {
    if (!ready_) return false;
    // lcd.frag reads the column taps and the row taps from these rows.
    const int32_t tables[4] = {geometry.capture_height, geometry.capture_height + ss_lcd::Panel::gpu_taps, 0, 0};
    SDL_SetGPURenderStateFragmentUniforms(pass_.state, 0, tables, sizeof tables);
    SDL_Texture *saved = SDL_GetRenderTarget(renderer_);
    const SDL_FRect area = {0, 0, float(geometry.out_width), float(geometry.out_height)};
    bool ok = SDL_SetRenderTarget(renderer_, target) && SDL_SetRenderGPUState(renderer_, pass_.state);
    ok = ok && SDL_RenderTexture(renderer_, source, nullptr, &area);
    SDL_SetRenderGPUState(renderer_, nullptr);
    SDL_SetRenderTarget(renderer_, saved);
    return ok;
}

bool ss_lcd_offered() {
    return true;
}

#else

#include <GLES2/gl2.h>
#include <emscripten.h>
#include <cstring>

namespace {

const char *vertex_source =
    "attribute vec2 a_position;\n"
    "void main() { gl_Position = vec4(a_position, 0.0, 1.0); }\n";

// The filter of lcd.frag.metal in GLSL ES 1.00 for WebGL 1, the GL version
// SDL's renderer uses. Its loops need constant bounds, so they stop at the tap
// count with break.
const char *fragment_source = R"(precision highp float;
uniform sampler2D u_source;
uniform vec2 u_size;
uniform vec2 u_tables;
const int TAPS = 15;
vec4 texel(float x, float y) { return texture2D(u_source, (vec2(x, y) + 0.5) / u_size); }
float bytes(vec2 b) { return floor(b.x * 255.0 + 0.5) + 256.0 * floor(b.y * 255.0 + 0.5); }
void main() {
    vec2 o = floor(gl_FragCoord.xy);
    float vx = bytes(texel(o.x, u_tables.x).ba);
    float vy = bytes(texel(o.y, u_tables.y).ba);
    float first_x = mod(vx, 4096.0), count_x = floor(vx / 4096.0);
    float first_y = mod(vy, 4096.0), count_y = floor(vy / 4096.0);
    float wx[TAPS];
    for (int i = 0; i < TAPS; ++i)
        wx[i] = float(i) < count_x ? (bytes(texel(o.x, u_tables.x + float(i)).rg) - 32768.0) / 4096.0 : 0.0;
    vec3 sum = vec3(0.0);
    for (int j = 0; j < TAPS; ++j) {
        if (float(j) >= count_y) break;
        float wy = (bytes(texel(o.y, u_tables.y + float(j)).rg) - 32768.0) / 4096.0;
        vec3 row = vec3(0.0);
        for (int i = 0; i < TAPS; ++i) {
            if (float(i) >= count_x) break;
            row += wx[i] * texel(first_x + float(i), first_y + float(j)).bgr;
        }
        sum += wy * row;
    }
    gl_FragColor = vec4(clamp(sum, 0.0, 1.0), 1.0);
}
)";

GLuint compile(GLenum type, const char *source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        fprintf(stderr, "Second Sight LCD shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

bool SecondSightLcdGpu::init(SDL_Renderer *renderer, SDL_GPUDevice *) {
    renderer_ = renderer;
    ready_ = false;
    const char *name = SDL_GetRendererName(renderer);
    if (!name || std::strcmp(name, "opengles2") != 0 || !ss_lcd_offered()) {
        fprintf(stderr, "Second Sight LCD GPU filter: needs WebGL on a hardware GPU (renderer %s)\n", name ? name : "none");
        return false;
    }
    // SDL keeps its GL context current while it draws; flushing makes sure.
    SDL_FlushRenderer(renderer);
    const GLuint vertex = compile(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glBindAttribLocation(program_, 0, "a_position");
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        fprintf(stderr, "Second Sight LCD GPU filter: shader program did not link\n");
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    u_source_ = glGetUniformLocation(program_, "u_source");
    u_size_ = glGetUniformLocation(program_, "u_size");
    u_tables_ = glGetUniformLocation(program_, "u_tables");
    GLint bound = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound);
    static const float corners[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, GLuint(bound));
    glGenFramebuffers(1, &framebuffer_);
    ready_ = true;
    return true;
}

void SecondSightLcdGpu::shutdown(SDL_GPUDevice *) {
    if (!ready_) return;
    glDeleteProgram(program_);
    glDeleteBuffers(1, &buffer_);
    glDeleteFramebuffers(1, &framebuffer_);
    program_ = buffer_ = framebuffer_ = 0;
    ready_ = false;
}

bool SecondSightLcdGpu::run(SDL_Texture *source, SDL_Texture *target, const Geometry &geometry) {
    if (!ready_) return false;
    const GLuint source_id = GLuint(SDL_GetNumberProperty(SDL_GetTextureProperties(source),
        SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0));
    const GLuint target_id = GLuint(SDL_GetNumberProperty(SDL_GetTextureProperties(target),
        SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER, 0));
    float width = 0, height = 0;
    if (!source_id || !target_id || !SDL_GetTextureSize(source, &width, &height)) {
        fprintf(stderr, "Second Sight LCD GPU filter: no GL texture (source %u, target %u)\n", source_id, target_id);
        return false;
    }
    // Run SDL's queued drawing first. SDL then sets its GL state again before
    // it draws, all but the framebuffer binding; that one, and the rest of
    // what this pass changes, is put back below.
    if (!SDL_FlushRenderer(renderer_)) return false;
    GLint framebuffer = 0, viewport[4] = {}, program = 0, active = 0, texture = 0, buffer = 0, attribute = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribute);
    const GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target_id, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const bool ok = status == GL_FRAMEBUFFER_COMPLETE;
    if (!ok) fprintf(stderr, "Second Sight LCD GPU filter: framebuffer incomplete (0x%x)\n", status);
    if (ok) {
        glViewport(0, 0, geometry.out_width, geometry.out_height);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glUseProgram(program_);
        glBindTexture(GL_TEXTURE_2D, source_id);
        // SDL sets these only on textures it draws. WebGL 1 reads a texture
        // whose size is not a power of two as black unless it clamps.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glUniform1i(u_source_, 0);
        glUniform2f(u_size_, width, height);
        glUniform2f(u_tables_, float(geometry.capture_height),
            float(geometry.capture_height + ss_lcd::Panel::gpu_taps));
        glBindBuffer(GL_ARRAY_BUFFER, buffer_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(framebuffer));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(GLuint(program));
    glBindTexture(GL_TEXTURE_2D, GLuint(texture));
    glActiveTexture(GLenum(active));
    glBindBuffer(GL_ARRAY_BUFFER, GLuint(buffer));
    if (!attribute) glDisableVertexAttribArray(0);
    if (blend) glEnable(GL_BLEND);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    return ok;
}

bool ss_lcd_offered() {
    // A context that would draw in software fails with this option.
    static const int offered = MAIN_THREAD_EM_ASM_INT({
        try {
            var gl = document.createElement('canvas').getContext('webgl', {failIfMajorPerformanceCaveat: true});
            if (!gl) return 0;
            var lose = gl.getExtension('WEBGL_lose_context');
            if (lose) lose.loseContext();
            return 1;
        } catch (e) {
            return 0;
        }
    });
    return offered != 0;
}

#endif

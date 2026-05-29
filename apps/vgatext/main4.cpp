#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <cstdio>

// GPU fragment-shader variant of the vgatext harness (macOS / Metal).
//
// Each frame: pack 80x25 char+attr into a small texture, upload via
// SDL_UpdateTexture, then draw a full-window quad with a custom fragment
// shader that samples the text page, looks up glyph bitmasks in a second
// texture, and applies the VGA palette from a uniform block.

static inline uint32_t argb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

struct VgaFragUniforms {
    uint32_t palette[16];
    uint32_t cols;
    uint32_t rows;
    uint32_t cell_w;
    uint32_t cell_h;
};

static const char *kVgaFragMsl = R"msl(
#include <metal_stdlib>
using namespace metal;

struct VgaUniforms {
    uint palette[16];
    uint cols;
    uint rows;
    uint cell_w;
    uint cell_h;
};

struct PSIn {
    float4 in_var_COLOR0 [[user(locn0)]];
    float2 in_var_TEXCOORD0 [[user(locn1)]];
};

fragment float4 main0(
    PSIn in [[stage_in]],
    constant VgaUniforms& uniforms [[buffer(0)]],
    texture2d<float> u_texture [[texture(0)]],
    texture2d<float> u_glyph [[texture(1)]],
    sampler u_sampler [[sampler(0)]],
    sampler u_glyph_sampler [[sampler(1)]]
) {
    float2 uv = in.in_var_TEXCOORD0;
    float cols = float(uniforms.cols);
    float rows = float(uniforms.rows);
    float cw = float(uniforms.cell_w);
    float cell_h = float(uniforms.cell_h);

    // Integer logical pixels (not fract-per-cell) so scaled output has no cell seams.
    int pcw = int(cols * cw);
    int prh = int(rows * cell_h);
    int px = min(int(floor(uv.x * float(pcw))), pcw - 1);
    int py = min(int(floor(uv.y * float(prh))), prh - 1);
    int col = px / int(cw);
    int row = py / int(cell_h);
    int gx = px - col * int(cw);
    int gy = py - row * int(cell_h);

    float2 text_uv = (float2(float(col) + 0.5, float(row) + 0.5)) / float2(cols, rows);
    float4 cell_data = u_texture.sample(u_sampler, text_uv, level(0));
    uint glyph = uint(cell_data.r * 255.0 + 0.5);
    uint attr = uint(cell_data.g * 255.0 + 0.5);

    float2 glyph_uv = (float2(float(glyph) + 0.5, float(gy) + 0.5)) / float2(256.0, 16.0);
    float4 glyph_texel = u_glyph.sample(u_glyph_sampler, glyph_uv, level(0));
    ushort bits = ushort(glyph_texel.r * 255.0) | (ushort(glyph_texel.g * 255.0) << 8);

    // Block-art (0xC0–0xDF): 9th column repeats the 8th.
    int sample_gx = gx;
    if (glyph >= 192u && glyph <= 223u && gx == int(cw) - 1) {
        sample_gx = gx - 1;
    }
    uint shift = uint(cw - 1u) - uint(sample_gx);
    bool on = ((bits >> shift) & 1u) != 0u;

    uint fg_idx = attr & 0xFu;
    uint bg_idx = (attr >> 4) & 0xFu;
    uint color = on ? uniforms.palette[fg_idx] : uniforms.palette[bg_idx];

    float a = float((color >> 24) & 0xFFu) / 255.0;
    float r = float((color >> 16) & 0xFFu) / 255.0;
    float g = float((color >> 8) & 0xFFu) / 255.0;
    float b = float(color & 0xFFu) / 255.0;
    return float4(r, g, b, a) * in.in_var_COLOR0;
}
)msl";

static bool UploadGpuTexture(
    SDL_GPUDevice *device,
    SDL_GPUTexture *dst,
    int w,
    int h,
    const void *pixels,
    int pitch)
{
    const Uint32 row_bytes = (Uint32)w * 4u;
    const Uint32 size = row_bytes * (Uint32)h;

    SDL_GPUTransferBufferCreateInfo tbci;
    SDL_zero(tbci);
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = size;

    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tbuf) {
        return false;
    }

    void *map = SDL_MapGPUTransferBuffer(device, tbuf, false);
    if (!map) {
        SDL_ReleaseGPUTransferBuffer(device, tbuf);
        return false;
    }

    if (pitch == (int)row_bytes) {
        SDL_memcpy(map, pixels, size);
    } else {
        const uint8_t *src = (const uint8_t *)pixels;
        uint8_t *dst_row = (uint8_t *)map;
        for (int y = 0; y < h; y++) {
            SDL_memcpy(dst_row, src, row_bytes);
            src += pitch;
            dst_row += row_bytes;
        }
    }
    SDL_UnmapGPUTransferBuffer(device, tbuf);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(device, tbuf);
        return false;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_ReleaseGPUTransferBuffer(device, tbuf);
        return false;
    }

    SDL_GPUTextureTransferInfo tex_src;
    SDL_zero(tex_src);
    tex_src.transfer_buffer = tbuf;
    tex_src.rows_per_layer = h;
    tex_src.pixels_per_row = w;

    SDL_GPUTextureRegion tex_dst;
    SDL_zero(tex_dst);
    tex_dst.texture = dst;
    tex_dst.w = w;
    tex_dst.h = h;
    tex_dst.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, tbuf);
    return true;
}

static SDL_GPUSampler *CreateNearestSampler(SDL_GPUDevice *device)
{
    SDL_GPUSamplerCreateInfo sci;
    SDL_zero(sci);
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    return SDL_CreateGPUSampler(device, &sci);
}

int main(int argc, char *argv[]) {
    bool bench = false;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--bench") == 0 || SDL_strcmp(argv[i], "-b") == 0) {
            bench = true;
        }
    }

    constexpr int COLS = 80;
    constexpr int ROWS = 25;
    constexpr int CELL_W = 9;
    constexpr int CELL_H = 16;
    constexpr int SCREEN_W = COLS * CELL_W;
    constexpr int SCREEN_H = ROWS * CELL_H;
    constexpr int ATLAS_COL_STRIDE = 9;
    constexpr int ATLAS_ROW_STRIDE = 17;
    constexpr int NUM_CELLS = COLS * ROWS;
    constexpr int GLYPH_TEX_W = 256;
    constexpr int GLYPH_TEX_H = CELL_H;

    alignas(64) uint8_t framebuf[2048];
    alignas(64) uint8_t attrbuf[2048];
    for (int i = 0; i < 2048; i++) {
        framebuf[i] = i & 0xFF;
        attrbuf[i] = (i + 1) & 0xFF;
    }

    alignas(64) const uint32_t palette[16] = {
        argb(0x00,0x00,0x00), argb(0x00,0x00,0xAA), argb(0x00,0xAA,0x00), argb(0x00,0xAA,0xAA),
        argb(0xAA,0x00,0x00), argb(0xAA,0x00,0xAA), argb(0xAA,0x55,0x00), argb(0xAA,0xAA,0xAA),
        argb(0x55,0x55,0x55), argb(0x55,0x55,0xFF), argb(0x55,0xFF,0x55), argb(0x55,0xFF,0xFF),
        argb(0xFF,0x55,0x55), argb(0xFF,0x55,0xFF), argb(0xFF,0xFF,0x55), argb(0xFF,0xFF,0xFF),
    };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("vgatext4", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowAspectRatio(window, 1.8f, 1.8f);

    SDL_GPUDevice *device = nullptr;
    SDL_Renderer *renderer = SDL_CreateGPURenderer(
        window, SDL_GPU_SHADERFORMAT_MSL, &device);
    if (renderer == NULL || device == NULL) {
        printf("GPU renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (!SDL_SetRenderVSync(renderer, 0)) {
        printf("SDL_SetRenderVSync failed: %s\n", SDL_GetError());
    }

    SDL_Texture *text_tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, COLS, ROWS);
    if (text_tex == NULL) {
        printf("Text texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(text_tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(text_tex, SDL_BLENDMODE_NONE);
    SDL_SetTextureColorMod(text_tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(text_tex, 255);

    SDL_Surface *font_surface = IMG_Load("apps/vgatext/IBM_VGA_8x16.png");
    if (font_surface == NULL) {
        printf("Font bitmap could not be loaded! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Surface *fs = SDL_ConvertSurface(font_surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(font_surface);
    if (fs == NULL) {
        printf("Font surface conversion failed! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    alignas(64) static uint16_t glyph_masks[256][CELL_H];
    {
        const uint8_t *base = (const uint8_t *)fs->pixels;
        const int pitch = fs->pitch;
        for (int g = 0; g < 256; g++) {
            const int sx = (g & 0x0F) * ATLAS_COL_STRIDE;
            const int sy = (g >> 4)   * ATLAS_ROW_STRIDE;
            for (int gy = 0; gy < CELL_H; gy++) {
                uint16_t bits = 0;
                const uint8_t *p = base + (sy + gy) * pitch + sx * 4;
                const int src_cols = (g >= 0xC0 && g <= 0xDF) ? (CELL_W - 1) : CELL_W;
                for (int gx = 0; gx < src_cols; gx++) {
                    uint8_t r = p[0], gc = p[1], b = p[2], a = p[3];
                    bool on = (a > 127) && ((r > 127) || (gc > 127) || (b > 127));
                    if (on) bits |= uint16_t(1u << (CELL_W - 1 - gx));
                    p += 4;
                }
                // Block-art glyphs (0xC0–0xDF): 9th column repeats the 8th (atlas slot is 8px wide).
                if (g >= 0xC0 && g <= 0xDF) {
                    bits = (bits & ~1u) | ((bits >> 1) & 1u);
                }
                glyph_masks[g][gy] = bits;
            }
        }
    }
    SDL_DestroySurface(fs);

    // 256x16 glyph atlas: x = glyph id, y = scanline; RG = 16-bit mask (little-endian).
    alignas(64) uint8_t glyph_pixels[GLYPH_TEX_W * GLYPH_TEX_H * 4];
    for (int g = 0; g < GLYPH_TEX_W; g++) {
        for (int gy = 0; gy < GLYPH_TEX_H; gy++) {
            const uint16_t bits = glyph_masks[g][gy];
            uint8_t *p = glyph_pixels + (gy * GLYPH_TEX_W + g) * 4;
            p[0] = (uint8_t)(bits & 0xFF);
            p[1] = (uint8_t)((bits >> 8) & 0xFF);
            p[2] = 0;
            p[3] = 255;
        }
    }

    SDL_GPUTextureCreateInfo gtci;
    SDL_zero(gtci);
    gtci.type = SDL_GPU_TEXTURETYPE_2D;
    gtci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    gtci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    gtci.width = GLYPH_TEX_W;
    gtci.height = GLYPH_TEX_H;
    gtci.layer_count_or_depth = 1;
    gtci.num_levels = 1;
    SDL_GPUTexture *glyph_gpu_tex = SDL_CreateGPUTexture(device, &gtci);
    if (!glyph_gpu_tex) {
        printf("Glyph GPU texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    if (!UploadGpuTexture(device, glyph_gpu_tex, GLYPH_TEX_W, GLYPH_TEX_H,
                          glyph_pixels, GLYPH_TEX_W * 4)) {
        printf("Glyph texture upload failed! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GPUSampler *nearest_sampler = CreateNearestSampler(device);
    if (!nearest_sampler) {
        printf("Sampler could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GPUTextureSamplerBinding glyph_binding;
    SDL_zero(glyph_binding);
    glyph_binding.texture = glyph_gpu_tex;
    glyph_binding.sampler = nearest_sampler;

    SDL_GPUShaderCreateInfo sinfo;
    SDL_zero(sinfo);
    sinfo.code = (const Uint8 *)kVgaFragMsl;
    sinfo.code_size = SDL_strlen(kVgaFragMsl);
    sinfo.format = SDL_GPU_SHADERFORMAT_MSL;
    sinfo.entrypoint = "main0";
    sinfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    sinfo.num_samplers = 2;
    sinfo.num_uniform_buffers = 1;
    sinfo.num_storage_buffers = 0;

    SDL_GPUShader *frag_shader = SDL_CreateGPUShader(device, &sinfo);
    if (!frag_shader) {
        printf("Fragment shader could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GPURenderStateDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.fragment_shader = frag_shader;
    desc.num_sampler_bindings = 1;
    desc.sampler_bindings = &glyph_binding;

    SDL_GPURenderState *gpu_state = SDL_CreateGPURenderState(renderer, &desc);
    if (!gpu_state) {
        printf("GPU render state could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    VgaFragUniforms uniforms;
    SDL_zero(uniforms);
    for (int i = 0; i < 16; i++) {
        uniforms.palette[i] = palette[i];
    }
    uniforms.cols = COLS;
    uniforms.rows = ROWS;
    uniforms.cell_w = CELL_W;
    uniforms.cell_h = CELL_H;
    if (!SDL_SetGPURenderStateFragmentUniforms(
            gpu_state, 0, &uniforms, sizeof(uniforms))) {
        printf("Could not set fragment uniforms! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    alignas(64) uint8_t text_pixels[NUM_CELLS * 4];

    uint64_t framestats[300];
    uint64_t rasterstats[300];
    int framecount = 0;
    SDL_Event event;

    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return 0;
            }
        }

        uint64_t start = SDL_GetTicksNS();
        uint64_t raster_start = SDL_GetTicksNS();

        for (int i = 0; i < NUM_CELLS; i++) {
            text_pixels[i * 4 + 0] = framebuf[i];
            text_pixels[i * 4 + 1] = attrbuf[i];
            text_pixels[i * 4 + 2] = 0;
            text_pixels[i * 4 + 3] = 255;
        }
        if (!SDL_UpdateTexture(text_tex, NULL, text_pixels, COLS * 4)) {
            printf("SDL_UpdateTexture failed: %s\n", SDL_GetError());
            return 1;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderGPUState(renderer, gpu_state);
        SDL_RenderTexture(renderer, text_tex, NULL, NULL);
        SDL_SetRenderGPUState(renderer, NULL);

        uint64_t raster_end = SDL_GetTicksNS();

        SDL_RenderPresent(renderer);

        uint64_t end = SDL_GetTicksNS();

        framestats[framecount] = end - start;
        rasterstats[framecount] = raster_end - raster_start;
        framecount++;
        if (framecount == 300) {
            uint64_t frametotal = 0;
            uint64_t rastertotal = 0;
            for (int i = 0; i < 300; i++) {
                frametotal += framestats[i];
                rastertotal += rasterstats[i];
            }
            printf("Average raster time: %llu ns\n", rastertotal / 300);
            printf("Average frame time:  %llu ns\n", frametotal / 300);
            if (bench) {
                return 0;
            }
            framecount = 0;
        }
        if (!bench) {
            SDL_Delay(16);
        }
    }

    return 0;
}

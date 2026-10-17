#include "display/GsCompositeGpu.hpp"
#include "display/ntsc.hpp"

static_assert(GS_NUM_TAPS + 1 == 16, "gscomposite.frag has 16 taps per side");

bool GsCompositeGpu::init(SDL_Renderer *r, SDL_GPUDevice *device) {
    renderer = r;
    return pass.create(renderer, device, "gscomposite", 1, 1);
}

void GsCompositeGpu::shutdown(SDL_GPUDevice *device) {
    pass.destroy(device);
}

void GsCompositeGpu::decode(SDL_Texture *src, const VideoScanGenerator_RGB::LineInfo *lines,
                            SDL_Texture *target, bool force_mono) {
    build_gscomposite_uniforms(uniforms.b14, uniforms.b16, uniforms.f14, uniforms.f16,
                               uniforms.m, uniforms.enc);
    for (int y = 0; y < LINES; y++) {
        uniforms.lines[y] = (lines[y].burst ? 1u : 0u) | (lines[y].clock16 ? 2u : 0u);
    }
    float sw = 0, sh = 0;
    SDL_GetTextureSize(src, &sw, &sh);
    uniforms.src_size[0] = (int32_t)sw;
    uniforms.src_size[1] = (int32_t)sh;
    uniforms.force_mono = force_mono ? 1 : 0;
    SDL_SetGPURenderStateFragmentUniforms(pass.state, 0, &uniforms, sizeof(uniforms));

    SDL_Texture *saved_target = SDL_GetRenderTarget(renderer);
    SDL_ScaleMode saved_scale = SDL_SCALEMODE_NEAREST;
    SDL_GetTextureScaleMode(src, &saved_scale);
    SDL_SetTextureScaleMode(src, SDL_SCALEMODE_NEAREST);

    SDL_SetRenderTarget(renderer, target);
    SDL_FRect dst = { 0.0f, 0.0f, sw, sh };
    SDL_SetRenderGPUState(renderer, pass.state);
    SDL_RenderTexture(renderer, src, nullptr, &dst);
    SDL_SetRenderGPUState(renderer, nullptr);

    SDL_SetTextureScaleMode(src, saved_scale);
    SDL_SetRenderTarget(renderer, saved_target);
}

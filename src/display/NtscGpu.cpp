#include "display/NtscGpu.hpp"
#include "display/ntsc.hpp"

#include <cstdio>

bool NtscGpu::init(SDL_Renderer *r, SDL_GPUDevice *device) {
    renderer = r;
    if (!pass.create(renderer, device, "ntsc", 1, 1)) {
        return false;
    }
    bits_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                 FrameBits::WORDS, FrameBits::LINES);
    if (!bits_tex) {
        printf("NTSC shader: bits texture creation failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(bits_tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(bits_tex, SDL_BLENDMODE_NONE);
    uniforms.black_padding = 1;
    return true;
}

void NtscGpu::shutdown(SDL_GPUDevice *device) {
    if (bits_tex) SDL_DestroyTexture(bits_tex);
    bits_tex = nullptr;
    pass.destroy(device);
}

void NtscGpu::decode(const FrameBits &bits, SDL_Texture *target, uint32_t shift, bool force_mono) {
    SDL_UpdateTexture(bits_tex, nullptr, bits.words, FrameBits::WORDS * sizeof(uint32_t));

    build_ntsc_gpu_weights(uniforms.w, uniforms.m);
    uniforms.region_origin[0] = (float)(168 - shift);
    uniforms.region_origin[1] = 35.0f;
    uniforms.force_mono = force_mono ? 1 : 0;
    SDL_SetGPURenderStateFragmentUniforms(pass.state, 0, &uniforms, sizeof(uniforms));

    SDL_Texture *saved_target = SDL_GetRenderTarget(renderer);
    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(renderer, &cr, &cg, &cb, &ca);

    SDL_SetRenderTarget(renderer, target);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(renderer);

    SDL_FRect dst = { (float)(168 - shift), 35.0f, (float)(560 + shift), (float)FrameBits::LINES };
    SDL_SetRenderGPUState(renderer, pass.state);
    SDL_RenderTexture(renderer, bits_tex, nullptr, &dst);
    SDL_SetRenderGPUState(renderer, nullptr);

    SDL_SetRenderTarget(renderer, saved_target);
    SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
}

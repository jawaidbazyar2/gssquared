#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

#include "devices/displaypp/frame/FrameBits.hpp"
#include "display/shaders/GpuFragmentPass.hpp"

// GPU composite / monochrome decoder: uploads a FrameBits and draws the decoded
// display region into a 910x263 render-target texture with ntsc.frag.
class NtscGpu {
public:
    // Uniform block of ntsc.frag.hlsl (cbuffer Ntsc).
    struct Uniforms {
        float w[4][15][4];
        float m[3][4];
        float region_origin[2];
        uint32_t black_padding;
        uint32_t force_mono;
    };
    static_assert(sizeof(Uniforms) == 1024, "must match cbuffer Ntsc");

    bool init(SDL_Renderer *renderer, SDL_GPUDevice *device);
    void shutdown(SDL_GPUDevice *device);
    bool ok() const { return pass.ok() && bits_tex != nullptr; }

    // Draws rows 35..226 of target (columns 168-shift .. 168+560) from bits;
    // shift is 7 when the line-start shift is enabled (IIe), else 0. Pixels
    // outside the region are cleared to black. Restores the previous render target.
    void decode(const FrameBits &bits, SDL_Texture *target, uint32_t shift, bool force_mono);

private:
    SDL_Renderer *renderer = nullptr;
    GpuFragmentPass pass;
    SDL_Texture *bits_tex = nullptr;
    Uniforms uniforms = {};
};

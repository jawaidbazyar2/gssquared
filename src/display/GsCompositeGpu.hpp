#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

#include "devices/displaypp/VideoScanGenerator_RGB.hpp"
#include "display/shaders/GpuFragmentPass.hpp"

// IIgs composite: draws the 910x263 RGB frame through gscomposite.frag (an
// RGB-to-composite encoder followed by the NTSC decoder) into a 910x263
// render-target texture.
class GsCompositeGpu {
public:
    static constexpr int LINES = 263;

    // Uniform block of gscomposite.frag.hlsl (cbuffer GsComposite).
    struct Uniforms {
        float b14[4][4];
        float b16[32][4];
        float f14[16][4];
        float f16[16][4];
        float m[3][4];
        float enc[3][4];
        uint32_t lines[66 * 4];  // per line: bit 0 colorburst, bit 1 16-dot clock
        int32_t src_size[2];
        uint32_t force_mono;
        uint32_t pad;
    };
    static_assert(sizeof(Uniforms) == 2256, "must match cbuffer GsComposite");

    bool init(SDL_Renderer *renderer, SDL_GPUDevice *device);
    void shutdown(SDL_GPUDevice *device);
    bool ok() const { return pass.ok(); }

    // src and target are 910x263; lines has LINES entries. force_mono decodes
    // every line as luma. Restores the previous render target.
    void decode(SDL_Texture *src, const VideoScanGenerator_RGB::LineInfo *lines,
                SDL_Texture *target, bool force_mono);

private:
    SDL_Renderer *renderer = nullptr;
    GpuFragmentPass pass;
    Uniforms uniforms = {};
};

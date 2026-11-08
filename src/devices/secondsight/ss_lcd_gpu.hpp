/*
 *   The LCD panel's scaler (ss_lcd::Panel) on the GPU. On the CPU the filter
 *   takes about 5 weighted samples across and 5 down for each color of each
 *   shown pixel: some 18 million multiply-adds a frame at a 1000 x 750 view,
 *   too slow for a browser on a laptop. Here a fragment shader computes each
 *   shown pixel from the same integer taps.
 *
 *   One texture carries the input: the monitor's samples in its top rows and
 *   the tap tables from Panel::gpu_tables below them. The pass draws the
 *   filtered picture into a target texture of the shown size. Both textures
 *   are SDL_PIXELFORMAT_ABGR8888, so every backend reads bytes 0-3 of a texel
 *   as its red, green, blue and alpha channels.
 *
 *   Native builds use SDL's GPU renderer with shaders/lcd.frag.{metal,spv}.
 *   The web build runs a small WebGL program inside SDL's own GL context.
 */

#pragma once

#include <SDL3/SDL.h>
#if !defined(__EMSCRIPTEN__)
#include "display/shaders/GpuFragmentPass.hpp"
#endif

class SecondSightLcdGpu {
public:
    struct Geometry {
        // The samples fill rows 0 to capture_height - 1; the column taps
        // start at row capture_height, the row taps gpu_taps rows later.
        int capture_height = 0, out_width = 0, out_height = 0;
    };
    // Prepare the pass for renderer. device is SDL's GPU device, or null on
    // the web and with the classic renderer.
    bool init(SDL_Renderer *renderer, SDL_GPUDevice *device);
    void shutdown(SDL_GPUDevice *device);
    bool ready() const { return ready_; }
    SDL_Renderer *renderer() const { return renderer_; }
    // Filter source into target, a render target of out_width x out_height.
    bool run(SDL_Texture *source, SDL_Texture *target, const Geometry &geometry);

private:
    SDL_Renderer *renderer_ = nullptr;
    bool ready_ = false;
#if defined(__EMSCRIPTEN__)
    unsigned program_ = 0, buffer_ = 0, framebuffer_ = 0;
    int u_source_ = -1, u_size_ = -1, u_tables_ = -1;
#else
    GpuFragmentPass pass_;
#endif
};

// Whether this build offers the VGA LCD monitor. The web build offers it only
// with WebGL on a hardware GPU; native builds always do and fall back to the
// CPU filter where the GPU pass is not available.
bool ss_lcd_offered();

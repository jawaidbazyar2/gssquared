#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <cstdint>
#include <cstdio>

#include "devices/secondsight/vga_render_text_9x16.hpp"

// CPU rasterizer variant of the vgatext harness.
//
// Instead of asking the GPU to blit per-cell glyphs, we rasterize the whole
// 720x400 text page into a STREAMING texture on the CPU (scanline order, so
// the writes into write-combined mapped memory stay sequential) and then issue
// a single SDL_RenderTexture to scale it to the window.

int main(int argc, char *argv[]) {
    // --bench / -b : run exactly 300 frames, print the average frame time, then
    // exit. Used for non-interactive perf measurement (skips the frame-pacing
    // delay so the run completes as fast as possible).
    bool bench = false;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--bench") == 0 || SDL_strcmp(argv[i], "-b") == 0) {
            bench = true;
        }
    }

    constexpr int NUM_CELLS = VGA_TEXT_COLS * VGA_TEXT_ROWS;
    constexpr int VRAM_BYTES = NUM_CELLS * 2;

    alignas(64) uint8_t vram[VRAM_BYTES];
    for (int i = 0; i < NUM_CELLS; i++) {
        vram[i * 2 + 0] = i & 0xFF;
        vram[i * 2 + 1] = (i + 1) & 0xFF;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("vgatext2", VGA_TEXT_SCREEN_W, VGA_TEXT_SCREEN_H, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowAspectRatio(window, 1.8f, 1.8f);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, VGA_TEXT_SCREEN_W, VGA_TEXT_SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (!SDL_SetRenderVSync(renderer, 0)) {
        printf("SDL_SetRenderVSync failed: %s\n", SDL_GetError());
    }

    SDL_Texture *screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, VGA_TEXT_SCREEN_W, VGA_TEXT_SCREEN_H);
    if (screen_tex == NULL) {
        printf("Screen texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(screen_tex, SDL_SCALEMODE_NEAREST);

    const char *font_paths[] = {
        "apps/vgatext/IBM_VGA_8x16.png",
        "resources/img/IBM_VGA_8x16.png",
    };
    bool font_ok = false;
    for (const char *path : font_paths) {
        if (vga_text_9x16_init(path)) {
            font_ok = true;
            break;
        }
    }
    if (!font_ok) {
        printf("Font bitmap could not be loaded from any path\n");
        return 1;
    }

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

        void *pixels = nullptr;
        int pitch = 0;
        uint64_t raster_start = SDL_GetTicksNS();
        if (SDL_LockTexture(screen_tex, NULL, &pixels, &pitch)) {
            vga_raster_text_9x16(vram, VGA_TEXT_FB_PITCH, (uint32_t *)pixels, pitch,
                vga_text_vram_layout_t::Interleaved);
            SDL_UnlockTexture(screen_tex);
        }
        uint64_t raster_end = SDL_GetTicksNS();

        SDL_RenderTexture(renderer, screen_tex, NULL, NULL);
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

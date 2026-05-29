#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <cstdio>

// CPU rasterizer variant of the vgatext harness.
//
// Instead of asking the GPU to blit per-cell glyphs, we rasterize the whole
// 720x400 text page into a STREAMING texture on the CPU (scanline order, so
// the writes into write-combined mapped memory stay sequential) and then issue
// a single SDL_RenderTexture to scale it to the window.

static inline uint32_t argb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

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

    constexpr int COLS = 80;
    constexpr int ROWS = 25;
    constexpr int CELL_W = 9;
    constexpr int CELL_H = 16;
    constexpr int SCREEN_W = COLS * CELL_W;   // 720
    constexpr int SCREEN_H = ROWS * CELL_H;   // 400
    constexpr int ATLAS_COL_STRIDE = 9;
    constexpr int ATLAS_ROW_STRIDE = 17;

    alignas(64) uint8_t framebuf[2048];
    alignas(64) uint8_t attrbuf[2048];
    for (int i = 0; i < 2048; i++) {
        framebuf[i] = i & 0xFF;
        attrbuf[i] = (i + 1) & 0xFF;
    }

    // Standard VGA 16-color palette (ARGB8888). Bits 0-3 of the attribute byte
    // select the foreground; bits 4-7 select the background (blink ignored).
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

    SDL_Window *window = SDL_CreateWindow("vgatext2", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE);
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
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    // Disable vsync so SDL_RenderPresent doesn't block on the display refresh;
    // lets us measure raster + present cost without the VBL wait.
    if (!SDL_SetRenderVSync(renderer, 0)) {
        printf("SDL_SetRenderVSync failed: %s\n", SDL_GetError());
    }

    // The CPU-rasterized framebuffer lives here. STREAMING so we can lock and
    // write directly into the mapped pixels each frame.
    SDL_Texture *screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (screen_tex == NULL) {
        printf("Screen texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(screen_tex, SDL_SCALEMODE_NEAREST);

    // Load the font atlas and bake it into a per-glyph bitmask so the hot loop
    // never touches the source image. glyph_masks[g][row] holds CELL_W bits;
    // bit (CELL_W-1-gx) is set when that pixel is part of the glyph.
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
                for (int gx = 0; gx < CELL_W; gx++) {
                    uint8_t r = p[0], gc = p[1], b = p[2], a = p[3];
                    bool on = (a > 127) && ((r > 127) || (gc > 127) || (b > 127));
                    if (on) bits |= uint16_t(1u << (CELL_W - 1 - gx));
                    p += 4;
                }
                glyph_masks[g][gy] = bits;
            }
        }
    }
    SDL_DestroySurface(fs);

    uint64_t framestats[300];   // full frame (raster + present, vsync-bound)
    uint64_t rasterstats[300];  // CPU raster only (lock -> unlock)
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
            for (int sy = 0; sy < SCREEN_H; sy++) {
                const uint16_t trow = sy / CELL_H;   // text row
                const uint16_t gy   = sy % CELL_H;   // scanline within the glyph
                const uint32_t cellbase = trow * COLS;
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + sy * pitch);

                for (uint16_t x = 0; x < COLS; x++) {
                    const uint32_t cell = cellbase + x;
                    const uint8_t ch   = framebuf[cell];
                    const uint8_t attr = attrbuf[cell];
                    const uint32_t fg = palette[attr & 0x0F];
                    const uint32_t bg = palette[(attr >> 4) & 0x0F];
                    uint16_t bits = glyph_masks[ch][gy];

                    // Branchless fg/bg select per pixel, MSB first.
                    for (int gx = CELL_W - 1; gx >= 0; gx--) {
                        const uint32_t m = uint32_t(-(int32_t)((bits >> gx) & 1u));
                        *dst++ = (fg & m) | (bg & ~m);
                    }
                }
            }
            SDL_UnlockTexture(screen_tex);
        }
        uint64_t raster_end = SDL_GetTicksNS();

        SDL_RenderTexture(renderer, screen_tex, NULL, NULL);
        SDL_RenderPresent(renderer);

        uint64_t end = SDL_GetTicksNS();
        // 300 frames = 5 seconds
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

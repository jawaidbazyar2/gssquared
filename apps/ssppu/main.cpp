/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Standalone test harness for SecondSight PPU renderer.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "devices/secondsight/ppu_render.hpp"

// NTSC pixel aspect is 8:7 (not square). 256*4 x 240*3 implies 4:3 pixels, which
// is wider than a real CRT and makes Mario look squat/wide. Scale 3x vertically and
// apply 8:7 horizontally instead.
static constexpr int VIDEO_SCALE = 3;
static constexpr int NTSC_PAR_W = 8;
static constexpr int NTSC_PAR_H = 7;
static constexpr int WINDOW_W = PPU_FB_W * NTSC_PAR_W * VIDEO_SCALE / NTSC_PAR_H;
static constexpr int WINDOW_H = PPU_FB_H * VIDEO_SCALE;

static bool read_file(const char *path, std::vector<uint8_t> &out) {
    FILE *f = std::fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    if (size < 0) {
        std::fclose(f);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);
    out.resize((size_t)size);
    if (size > 0) {
        const size_t got = std::fread(out.data(), 1, (size_t)size, f);
        if (got != (size_t)size) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

static void init_palette(uint8_t palette_rgb[256][3]) {
    for (int i = 0; i < 256; i++) {
        palette_rgb[i][0] = 0;
        palette_rgb[i][1] = 0;
        palette_rgb[i][2] = 0;
    }
    // Index 0: whole-screen background (sky blue).
    palette_rgb[PPU_PAL_SCREEN][0] = 0x5C;
    palette_rgb[PPU_PAL_SCREEN][1] = 0x94;
    palette_rgb[PPU_PAL_SCREEN][2] = 0xFC;
    // Sprite tile colors: 0 transparent, 1 blue, 2 skin, 3 red (CHR 2bpp indices).
    palette_rgb[PPU_PAL_SPRITE_BLUE][0] = 0x00;
    palette_rgb[PPU_PAL_SPRITE_BLUE][1] = 0x00;
    palette_rgb[PPU_PAL_SPRITE_BLUE][2] = 0xFF;
    palette_rgb[PPU_PAL_SPRITE_SKIN][0] = 0xFF;
    palette_rgb[PPU_PAL_SPRITE_SKIN][1] = 0xDF;
    palette_rgb[PPU_PAL_SPRITE_SKIN][2] = 0xB5;
    palette_rgb[PPU_PAL_SPRITE_RED][0] = 0xC8;
    palette_rgb[PPU_PAL_SPRITE_RED][1] = 0x00;
    palette_rgb[PPU_PAL_SPRITE_RED][2] = 0x00;
    // Background tile colors (tile indices 0-3 -> fb 0,4-6; 0 transparent).
    palette_rgb[PPU_PAL_BG_TAN][0] = 0xFC;
    palette_rgb[PPU_PAL_BG_TAN][1] = 0xD8;
    palette_rgb[PPU_PAL_BG_TAN][2] = 0xA8;
    palette_rgb[PPU_PAL_BG_BROWN][0] = 0x88;
    palette_rgb[PPU_PAL_BG_BROWN][1] = 0x58;
    palette_rgb[PPU_PAL_BG_BROWN][2] = 0x00;
    palette_rgb[PPU_PAL_BG_WHITE][0] = 0xFF;
    palette_rgb[PPU_PAL_BG_WHITE][1] = 0xFF;
    palette_rgb[PPU_PAL_BG_WHITE][2] = 0xFF;
}

static void fill_procedural_sprite_tileset(uint8_t *tileset) {
    for (int t = 0; t < PPU_TILES_PER_SET; t++) {
        uint8_t *tile = tileset + (size_t)t * PPU_TILE_BYTES;
        const uint8_t color = (uint8_t)((t % 3) + 1);
        for (int y = 0; y < PPU_TILE_H; y++) {
            for (int x = 0; x < PPU_TILE_W; x++) {
                bool on = false;
                if (t == 0) {
                    on = (x >= 2 && x <= 5 && y >= 1 && y <= 6);
                } else if (t == 1) {
                    on = (x == 1 || x == 6) && y >= 2 && y <= 5;
                } else if (t == 2) {
                    on = (x >= 3 && x <= 4 && y >= 6);
                } else {
                    on = ((x + y + t) & 3) == 0;
                }
                tile[(size_t)y * PPU_TILE_W + (size_t)x] = on ? color : 0;
            }
        }
    }
}

static void fill_procedural_bg_tileset(uint8_t *tileset) {
    // Background uses unified tiles 310-400 -> tileset1 indices 54-144.
    static constexpr int BG_FIRST = 310;
    static constexpr int BG_LAST = 400;

    for (int t = 0; t < PPU_TILES_PER_SET; t++) {
        uint8_t *tile = tileset + (size_t)t * PPU_TILE_BYTES;
        const bool in_bg_range = (t >= BG_FIRST - PPU_TILE_BG_BASE && t <= BG_LAST - PPU_TILE_BG_BASE);
        const int bg_slot = in_bg_range ? (t - (BG_FIRST - PPU_TILE_BG_BASE)) : t;
        const uint8_t color = (uint8_t)((bg_slot % 3) + 1);

        for (int y = 0; y < PPU_TILE_H; y++) {
            for (int x = 0; x < PPU_TILE_W; x++) {
                bool on = false;
                if (in_bg_range) {
                    // Varied patterns; index 0 stays transparent (black bg shows through).
                    switch (bg_slot % 7) {
                        case 0: on = (y == 7); break;                          // ground line
                        case 1: on = (x == 0 || x == 7); break;                // vertical edges
                        case 2: on = ((x ^ y) & 1) != 0 && y >= 2; break;        // checker
                        case 3: on = (x >= 2 && x <= 5 && y >= 1 && y <= 4); break; // block
                        case 4: on = (x + y) == 7; break;                      // diagonal
                        case 5: on = (y <= 1 || y >= 6); break;                // band
                        default: on = (x >= 1 && x <= 6 && y >= 3 && y <= 5); break;
                    }
                } else if (t == 0) {
                    on = true;
                } else {
                    on = false;
                }
                tile[(size_t)y * PPU_TILE_W + (size_t)x] = on ? color : 0;
            }
        }
    }
}

static uint8_t unified_bg_to_tileset1_index(int unified_tile) {
    return (uint8_t)(unified_tile - PPU_TILE_BG_BASE);
}

static void ns_set_tile(uint8_t *ns, int row, int col, int unified_tile) {
    if (row < 0 || row >= PPU_NS_ROWS || col < 0 || col >= PPU_NS_COLS) {
        return;
    }
    ns[(size_t)row * PPU_NS_COLS + (size_t)col] = unified_bg_to_tileset1_index(unified_tile);
}

static void place_cloud_bank(uint8_t *ns, int col) {
    // 2x2 cloud (unified tiles):
    //  310 311
    //  313 316
    ns_set_tile(ns, 8, col, 310);
    ns_set_tile(ns, 8, col + 1, 311);
    ns_set_tile(ns, 9, col, 313);
    ns_set_tile(ns, 9, col + 1, 316);
}

static void place_ground_segment(uint8_t *ns, int col) {
    // 2x2 ground block (unified tiles); two layers = rows 26-29:
    //  436 437
    //  438 439
    for (int row = 26; row <= 29; row += 2) {
        ns_set_tile(ns, row, col, 436);
        ns_set_tile(ns, row, col + 1, 437);
        ns_set_tile(ns, row + 1, col, 438);
        ns_set_tile(ns, row + 1, col + 1, 439);
    }
}

static void fill_tilemap(uint8_t *ns) {
    std::memset(ns, 0, PPU_NAMESPACE_BYTES);
    place_cloud_bank(ns, 4);
    place_cloud_bank(ns, 14);
    place_cloud_bank(ns, 24);
    for (int col = 0; col < PPU_NS_COLS; col += 2) {
        place_ground_segment(ns, col);
    }
}

static void write_sprite(uint8_t *oam, int index, uint8_t tile, uint8_t attr, uint8_t x, uint8_t y) {
    uint8_t *s = oam + (size_t)index * PPU_SPRITE_BYTES;
    s[0] = tile;
    s[1] = attr;
    s[2] = x;
    s[3] = y;
}

static constexpr uint8_t MARIO_X = 96;
static constexpr uint8_t MARIO_BASE_Y = 24 * 8; // rows 24-25 (16px sprite above ground at row 26)
// sin((i * pi/64) + pi) * 24 for i = scroll_x - 64 (0..63).
static const int8_t MARIO_Y_SIN_LUT[64] = {
     0,  -1,  -2,  -4,  -5,  -6,  -7,  -8,  -9, -10, -11, -12, -13, -14, -15, -16,
   -17, -18, -19, -19, -20, -21, -21, -22, -22, -23, -23, -23, -24, -24, -24, -24,
   -24, -24, -24, -24, -24, -23, -23, -23, -22, -22, -21, -21, -20, -19, -19, -18,
   -17, -16, -15, -14, -13, -12, -11, -10,  -9,  -8,  -7,  -6,  -5,  -4,  -2,  -1,
};

static void update_mario_sprites(uint8_t *oam, uint16_t scroll_x) {
    int my = MARIO_BASE_Y;
    if (scroll_x & 64) {
        const unsigned idx = (unsigned)(scroll_x - 64) & 63u;
        my += MARIO_Y_SIN_LUT[idx];
    }
    const uint8_t y = (uint8_t)my;

    // 16x16 Mario from original CHR tiles 50-53 (0-based):
    //  50 51
    //  52 53
    write_sprite(oam, 0, 50, 0, MARIO_X, y);
    write_sprite(oam, 1, 51, 0, (uint8_t)(MARIO_X + 8), y);
    write_sprite(oam, 2, 52, 0, MARIO_X, (uint8_t)(y + 8));
    write_sprite(oam, 3, 53, 0, (uint8_t)(MARIO_X + 8), (uint8_t)(y + 8));
}

static void fill_sprites(uint8_t *oam) {
    std::memset(oam, 0, PPU_OAM_BYTES);
    update_mario_sprites(oam, 0);
}

static bool load_tiles_file(const char *path, uint8_t *tileset0, uint8_t *tileset1) {
    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        std::fprintf(stderr, "ssppu: cannot read tile file '%s'\n", path);
        return false;
    }
    // Tiles 0-255 -> tileset0 (sprites); tiles 256-511 -> tileset1 (background).
    const size_t copy0 = data.size() < (size_t)PPU_TILESET_BYTES ? data.size() : (size_t)PPU_TILESET_BYTES;
    std::memcpy(tileset0, data.data(), copy0);
    if (data.size() > (size_t)PPU_TILESET_BYTES) {
        const size_t remain = data.size() - (size_t)PPU_TILESET_BYTES;
        const size_t copy1 = remain < (size_t)PPU_TILESET_BYTES ? remain : (size_t)PPU_TILESET_BYTES;
        std::memcpy(tileset1, data.data() + PPU_TILESET_BYTES, copy1);
    }
    std::printf("ssppu: loaded %zu bytes from '%s'\n", data.size(), path);
    return true;
}

static void blit_indexed_to_argb8888(const uint8_t *indexed, int width, int height,
    uint32_t *pixels, int pitch_bytes, const uint8_t palette_rgb[256][3])
{
    const int pitch_pixels = pitch_bytes / (int)sizeof(uint32_t);
    for (int y = 0; y < height; y++) {
        uint32_t *row = pixels + (size_t)y * (size_t)pitch_pixels;
        for (int x = 0; x < width; x++) {
            const uint8_t idx = indexed[(size_t)y * (size_t)width + (size_t)x];
            const uint8_t *rgb = palette_rgb[idx];
            row[x] = 0xFF000000u
                | ((uint32_t)rgb[0] << 16)
                | ((uint32_t)rgb[1] << 8)
                | (uint32_t)rgb[2];
        }
    }
}

int main(int argc, char *argv[]) {
    bool bench = false;
    const char *tiles_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--bench") == 0 || SDL_strcmp(argv[i], "-b") == 0) {
            bench = true;
        } else if (tiles_path == nullptr) {
            tiles_path = argv[i];
        }
    }

    alignas(64) uint8_t namespace0[PPU_NAMESPACE_BYTES] = {};
    alignas(64) uint8_t namespace1[PPU_NAMESPACE_BYTES] = {};
    alignas(64) uint8_t tileset0[PPU_TILESET_BYTES] = {};
    alignas(64) uint8_t tileset1[PPU_TILESET_BYTES] = {};
    alignas(64) uint8_t sprite_table[PPU_OAM_BYTES] = {};
    alignas(64) uint8_t framebuffer[PPU_FB_W * PPU_FB_H] = {};
    uint8_t palette_rgb[256][3] = {};

    init_palette(palette_rgb);
    fill_procedural_sprite_tileset(tileset0);
    fill_procedural_bg_tileset(tileset1);
    if (tiles_path != nullptr) {
        if (!load_tiles_file(tiles_path, tileset0, tileset1)) {
            return 1;
        }
    }
    fill_tilemap(namespace0);
    fill_tilemap(namespace1);
    fill_sprites(sprite_table);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "ssppu: SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("ssppu", WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::fprintf(stderr, "ssppu: window create failed: %s\n", SDL_GetError());
        return 1;
    }
    const float aspect = (float)WINDOW_W / (float)WINDOW_H;
    SDL_SetWindowAspectRatio(window, aspect, aspect);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        std::fprintf(stderr, "ssppu: renderer create failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, PPU_FB_W, PPU_FB_H, SDL_LOGICAL_PRESENTATION_STRETCH);
    SDL_SetRenderVSync(renderer, 0);

    SDL_Texture *screen_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, PPU_FB_W, PPU_FB_H);
    if (screen_tex == nullptr) {
        std::fprintf(stderr, "ssppu: texture create failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureScaleMode(screen_tex, SDL_SCALEMODE_NEAREST);

    PPURender ppu;
    ppu_config_t cfg = {};
    cfg.framebuffer = framebuffer;
    cfg.namespace0 = namespace0;
    cfg.namespace1 = namespace1;
    cfg.tileset0 = tileset0;
    cfg.tileset1 = tileset1;
    cfg.sprite_table = sprite_table;
    cfg.bg_color = PPU_PAL_SCREEN;

    uint64_t framestats[300];
    uint64_t rasterstats[300];
    int framecount = 0;
    uint32_t tick = 0;
    SDL_Event event;

    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return 0;
            }
        }

        const uint64_t start = SDL_GetTicksNS();
        cfg.scroll_x = (uint16_t)(tick % 512);
        cfg.scroll_y = 0;
        update_mario_sprites(sprite_table, cfg.scroll_x);

        void *pixels = nullptr;
        int pitch = 0;
        const uint64_t raster_start = SDL_GetTicksNS();
        ppu.render(cfg);
        if (SDL_LockTexture(screen_tex, nullptr, &pixels, &pitch)) {
            blit_indexed_to_argb8888(framebuffer, PPU_FB_W, PPU_FB_H,
                (uint32_t *)pixels, pitch, palette_rgb);
            SDL_UnlockTexture(screen_tex);
        }
        const uint64_t raster_end = SDL_GetTicksNS();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        const uint64_t end = SDL_GetTicksNS();
        framestats[framecount] = end - start;
        rasterstats[framecount] = raster_end - raster_start;
        framecount++;
        tick++;

        if (framecount == 300) {
            uint64_t frametotal = 0;
            uint64_t rastertotal = 0;
            for (int i = 0; i < 300; i++) {
                frametotal += framestats[i];
                rastertotal += rasterstats[i];
            }
            std::printf("Average raster time: %llu ns\n", rastertotal / 300);
            std::printf("Average frame time:  %llu ns\n", frametotal / 300);
            if (bench) {
                return 0;
            }
            framecount = 0;
        }

        if (!bench) {
            SDL_Delay(16);
        }
    }
}

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   SecondSight PPU mode renderer (phase 1).
 *   Future integration: secondsight.cpp mode flag $02, namespace from A2 MMU
 *   ($2000-$3FFF), tile sets from card VRAM.
 */

#include "ppu_render.hpp"

#include <cstring>

void PPURender::render(const ppu_config_t &cfg) {
    if (cfg.framebuffer == nullptr) {
        return;
    }
    clear(cfg);
    render_background(cfg);
    render_sprites(cfg);
}

void PPURender::clear(const ppu_config_t &cfg) {
    std::memset(cfg.framebuffer, cfg.bg_color, PPU_FB_W * PPU_FB_H);
}

uint8_t PPURender::tile_pixel(const uint8_t *tileset, uint8_t tile_num,
    int tx, int ty, bool hflip, bool vflip)
{
    if (tileset == nullptr) {
        return 0;
    }
    if (hflip) {
        tx = PPU_TILE_W - 1 - tx;
    }
    if (vflip) {
        ty = PPU_TILE_H - 1 - ty;
    }
    const uint8_t *tile = tileset + (size_t)tile_num * PPU_TILE_BYTES;
    return tile[(size_t)ty * PPU_TILE_W + (size_t)tx];
}

void PPURender::render_background(const ppu_config_t &cfg) {
    if (cfg.namespace0 == nullptr || cfg.tileset1 == nullptr) {
        return;
    }

    const int scroll_x = cfg.scroll_x & 0x1FF;
    const int scroll_y = cfg.scroll_y & 0xFF;

    for (int y = 0; y < PPU_FB_H; y++) {
        const int world_y = y + scroll_y;
        const int tile_row = (world_y / PPU_TILE_H) % PPU_NS_ROWS;
        const int fine_y = world_y & 7;

        for (int x = 0; x < PPU_FB_W; x++) {
            const int world_x = x + scroll_x;
            const int tile_col = (world_x / PPU_TILE_W) % PPU_NS_COLS;
            const int fine_x = world_x & 7;

            const bool use_ns1 = (world_x / PPU_TILE_W) >= PPU_NS_COLS;
            const uint8_t *ns = use_ns1 ? cfg.namespace1 : cfg.namespace0;
            if (ns == nullptr) {
                continue;
            }

            const int ns_col = tile_col;
            // Namespace stores 0-255; background tiles are 256-511 (tileset1).
            const uint8_t tile_idx = ns[(size_t)tile_row * PPU_NS_COLS + (size_t)ns_col];
            const uint8_t pixel = tile_pixel(cfg.tileset1, tile_idx, fine_x, fine_y, false, false);

            if (pixel != 0) {
                // Tile indices 1-3 map to separate bg palette entries (4-6).
                cfg.framebuffer[(size_t)y * PPU_FB_W + (size_t)x] =
                    (uint8_t)(pixel + PPU_PAL_BG_BASE - 1);
            }
        }
    }
}

void PPURender::render_sprites(const ppu_config_t &cfg) {
    if (cfg.sprite_table == nullptr || cfg.tileset0 == nullptr) {
        return;
    }

    for (int i = 0; i < PPU_NUM_SPRITES; i++) {
        const uint8_t *oam = cfg.sprite_table + (size_t)i * PPU_SPRITE_BYTES;
        const uint8_t tile_num = oam[0]; // sprites: tile numbers 0-255 -> tileset0
        const uint8_t attr = oam[1];
        const int sx = (int)oam[2];
        const int sy = (int)oam[3];

        if (attr & PPU_ATTR_BEHIND_BG) {
            // Phase 1: behind-background priority deferred.
            continue;
        }

        const bool hflip = (attr & PPU_ATTR_HFLIP) != 0;
        const bool vflip = (attr & PPU_ATTR_VFLIP) != 0;

        for (int ty = 0; ty < PPU_TILE_H; ty++) {
            const int py = sy + ty;
            if (py < 0 || py >= PPU_FB_H) {
                continue;
            }
            for (int tx = 0; tx < PPU_TILE_W; tx++) {
                const int px = sx + tx;
                if (px < 0 || px >= PPU_FB_W) {
                    continue;
                }
                const uint8_t pixel = tile_pixel(cfg.tileset0, tile_num, tx, ty, hflip, vflip);
                if (pixel != 0) {
                    cfg.framebuffer[(size_t)py * PPU_FB_W + (size_t)px] = pixel;
                }
            }
        }
    }
}

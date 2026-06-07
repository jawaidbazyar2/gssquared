/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstdint>

// NES-style PPU renderer for SecondSight mode 2 (future integration).
// Phase 1: standalone class with pointer-based inputs; caller owns the output
// framebuffer. Future secondsight.cpp wiring will pass namespace from A2 main
// memory ($2000-$3FFF hires page 1 via MMU) and tile sets from VRAM.

static constexpr int PPU_FB_W = 256;
static constexpr int PPU_FB_H = 240;
static constexpr int PPU_TILE_W = 8;
static constexpr int PPU_TILE_H = 8;
static constexpr int PPU_NS_COLS = 32;
static constexpr int PPU_NS_ROWS = 30;
static constexpr int PPU_TILES_PER_SET = 256;
static constexpr int PPU_TILE_SPRITE_BASE = 0;    // sprites: tile numbers 0-255 -> tileset0
static constexpr int PPU_TILE_BG_BASE = 256;    // background: 256-511 -> tileset1
static constexpr int PPU_TILE_BYTES = 64;
static constexpr int PPU_TILESET_BYTES = 16384;
static constexpr int PPU_NUM_SPRITES = 64;
static constexpr int PPU_SPRITE_BYTES = 4;
static constexpr int PPU_NAMESPACE_BYTES = 1024;
static constexpr int PPU_OAM_BYTES = 256;

// Fixed SecondSight / Apple II memory map (mode 2).
static constexpr uint32_t SS_PPU_FB_PAGE1_ADDR = 0x00000;
static constexpr uint32_t SS_PPU_FB_PAGE2_ADDR = 0x10000;
static constexpr uint32_t SS_PPU_FB_ADDR = SS_PPU_FB_PAGE1_ADDR;
static constexpr uint32_t SS_PPU_TILESET0_ADDR = 0x20000; // sprites, tiles 0-255
static constexpr uint32_t SS_PPU_TILESET1_ADDR = 0x24000; // background, tiles 256-511
static constexpr uint32_t SS_PPU_NS0_ADDR = 0x2000;
static constexpr uint32_t SS_PPU_NS1_ADDR = 0x2400;
static constexpr uint32_t SS_PPU_PALETTE_ADDR = 0x2800;
static constexpr uint32_t SS_PPU_PALETTE_BYTES = 768;
static constexpr uint32_t SS_PPU_OAM_ADDR = 0x2B00;
static constexpr uint32_t SS_PPU_REGS_ADDR = 0x2C00;
static constexpr uint32_t SS_PPU_REGS_BYTES = 256;
static constexpr uint8_t SS_PPU_REG_SCROLL_X = 0x00;
static constexpr uint8_t SS_PPU_REG_SCROLL_Y = 0x02;
static constexpr uint8_t SS_PPU_REG_SPLIT = 0x04;
static constexpr uint8_t SS_PPU_REG_BG_COLOR = 0x05;

static inline uint16_t ss_ppu_read_reg16(const uint8_t *regs, uint8_t off) {
    return (uint16_t)regs[off] | ((uint16_t)regs[off + 1] << 8);
}

static constexpr uint8_t PPU_ATTR_HFLIP = 0x01;
static constexpr uint8_t PPU_ATTR_VFLIP = 0x02;
static constexpr uint8_t PPU_ATTR_BEHIND_BG = 0x04;
static constexpr uint8_t PPU_ATTR_TILESET1 = 0x08; // reserved (sprites always use tileset0)

// Framebuffer palette indices (harness / mode-2 convention).
static constexpr uint8_t PPU_PAL_SCREEN = 0;       // whole-screen background (sky)
static constexpr uint8_t PPU_PAL_SPRITE_RED = 1;  // sprite tile index 1
static constexpr uint8_t PPU_PAL_SPRITE_SKIN = 2;  // sprite tile index 2 (beige)
static constexpr uint8_t PPU_PAL_SPRITE_BLUE = 3;   // sprite tile index 3
static constexpr uint8_t PPU_PAL_BG_TAN = 4;         // bg tile index 1
static constexpr uint8_t PPU_PAL_BG_BROWN = 5;       // bg tile index 2
static constexpr uint8_t PPU_PAL_BG_WHITE = 6;       // bg tile index 3
static constexpr uint8_t PPU_PAL_BG_BASE = PPU_PAL_BG_TAN; // bg tile N -> palette N+3

struct ppu_config_t {
    uint8_t       *framebuffer;  // caller-owned 256x240 (PPU_FB_W*PPU_FB_H) 8bpp output
    const uint8_t *namespace0;   // 32x30 tile indices (A2 main mem hires pg1 on real HW)
    const uint8_t *namespace1;   // second namespace for horizontal scroll (may be null)
    const uint8_t *tileset0;     // 16 KB sprite tile set (tile numbers 0-255)
    const uint8_t *tileset1;     // 16 KB background tile set (tile numbers 256-511)
    const uint8_t *sprite_table; // 256-byte OAM
    uint8_t  bg_color;           // palette index for border/background
    uint16_t scroll_x;           // 0..511 (two namespaces wide)
    uint16_t scroll_y;           // vertical scroll in pixels
};

class PPURender {
public:
    void render(const ppu_config_t &cfg);

private:
    void clear(const ppu_config_t &cfg);
    void render_background(const ppu_config_t &cfg);
    void render_sprites(const ppu_config_t &cfg);

    static uint8_t tile_pixel(const uint8_t *tileset, uint8_t tile_num,
        int tx, int ty, bool hflip, bool vflip);
};

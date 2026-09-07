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
#include "vga_render_text_9x16.hpp"

static constexpr int VGA_TEXT_8X8_CELL_W = 8;
static constexpr int VGA_TEXT_8X8_CELL_H = 8;
static constexpr int VGA_TEXT_8X8_COLS = 80;
static constexpr int VGA_TEXT_8X8_ROWS = 43;
static constexpr int VGA_TEXT_8X8_SCREEN_W = VGA_TEXT_8X8_COLS * VGA_TEXT_8X8_CELL_W; // 640
static constexpr int VGA_TEXT_8X8_SCREEN_H = VGA_TEXT_8X8_ROWS * VGA_TEXT_8X8_CELL_H; // 344
static constexpr int SS_VRAM_FONT_8X8_GLYPH_BYTES = 8;
static constexpr int SS_VRAM_FONT_8X8_SIZE = 256 * SS_VRAM_FONT_8X8_GLYPH_BYTES; // 2048

/** Load 256×8×8 ANSI ROM font (2048 bytes). */
bool vga_text_8x8_load_rom_font(const char *ansi_path);

/** Select the ROM 8x8 bank (no-op if not loaded). */
void vga_text_8x8_select_rom_font();

/** Bake 8x8 glyphs from VRAM / user font (first 8 rows of each glyph). */
bool vga_text_8x8_load_font_from_vram(const uint8_t *font_base,
    int glyph_stride = SS_VRAM_FONT_GLYPH_BYTES);

/**
 * Raster 8x8 VGA text into a uint32_t ARGB buffer.
 * Palette is the shared 16-color table from vga_text_9x16.
 */
void vga_raster_text_8x8(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout = vga_text_vram_layout_t::Interleaved,
    int cols = VGA_TEXT_8X8_COLS, int rows = VGA_TEXT_8X8_ROWS);

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

static constexpr int VGA_TEXT_COLS = 80;
static constexpr int VGA_TEXT_ROWS = 25;
static constexpr int VGA_TEXT_CELL_W = 9;
static constexpr int VGA_TEXT_CELL_H = 16;
static constexpr int VGA_TEXT_SCREEN_W = VGA_TEXT_COLS * VGA_TEXT_CELL_W;   // 720
static constexpr int VGA_TEXT_SCREEN_H = VGA_TEXT_ROWS * VGA_TEXT_CELL_H;   // 400

// IBM VGA mode 03h / Oak OTI-087 host memory layout (same as linear view of segment 0xB800):
// - Plane 0 = character, plane 1 = attribute; CPU byte stream is interleaved (char, attr) per cell.
// - 80 x 25 cells, 2 bytes per cell, 160 bytes per scanline.
// - CRTC reg 0x13 (offset) = 0x28 for 80 cols: line pitch in bytes = 4 * offset = 160.
// - CRTC reg 0x01 (horizontal display end) = 0x4F for 80 character clocks.
// See OSDev VGA_Hardware mode 3h table; FreeVGA CRT offset register (2 * offset * word_size).
static constexpr int VGA_TEXT_CELL_BYTES = 2;
static constexpr int VGA_TEXT_FB_PITCH = VGA_TEXT_COLS * VGA_TEXT_CELL_BYTES;   // 160
static constexpr int VGA_TEXT_CRTC_OFFSET = 0x28;
static constexpr int VGA_TEXT_CRTC_HDISPLAY_END = 0x4F;
/** Bytes between plane-0 and plane-1 bases in a planar linear dump (B8000-style). */
static constexpr int VGA_TEXT_PLANE1_DELTA = 0x2000;

enum class vga_text_vram_layout_t {
    Interleaved,   // char, attr, char, attr... (standard PC/VGA host view)
    PlanarSplit,   // plane0[row*80+col], plane1 at base + 0x2000
};

/** 8x16 font in Second Sight VRAM: 256 glyphs, 16 bytes/scanline per glyph. */
static constexpr int SS_VRAM_FONT_GLYPH_BYTES = 16;
static constexpr int SS_VRAM_FONT_SIZE = 256 * SS_VRAM_FONT_GLYPH_BYTES;   // 4096
static constexpr uint32_t SS_VRAM_FONT_DEFAULT_BASE = 0x20;

/** Load font atlas from PNG (vgatext harness fallback). */
bool vga_text_9x16_init(const char *font_path);

/** Bake glyph masks from 8x16 font bytes already in card VRAM (upload/DMA). */
bool vga_text_9x16_load_font_from_vram(const uint8_t *font_base, int glyph_stride = SS_VRAM_FONT_GLYPH_BYTES);

/** CRTC offset reg -> byte pitch for standard VGA text (offset * 4). */
inline int vga_text_pitch_from_crtc_offset(uint8_t crtc_offset) {
    return crtc_offset > 0 ? (int)crtc_offset * 4 : VGA_TEXT_FB_PITCH;
}

/** Raster mode 03h text into a uint32_t ARGB buffer (4 bytes per pixel, bytes per row = pitch). */
void vga_raster_text_9x16(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout = vga_text_vram_layout_t::Interleaved);

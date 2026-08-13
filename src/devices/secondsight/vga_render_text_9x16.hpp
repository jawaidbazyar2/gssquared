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
static constexpr int VGA_TEXT_COLS_40 = 40;
static constexpr int VGA_TEXT_ROWS = 25;
static constexpr int VGA_TEXT_CELL_W = 9;
static constexpr int VGA_TEXT_CELL_H = 16;
static constexpr int VGA_TEXT_SCREEN_W = VGA_TEXT_COLS * VGA_TEXT_CELL_W;   // 720
static constexpr int VGA_TEXT_SCREEN_W_40 = VGA_TEXT_COLS_40 * VGA_TEXT_CELL_W; // 360
static constexpr int VGA_TEXT_SCREEN_H = VGA_TEXT_ROWS * VGA_TEXT_CELL_H;   // 400

// IBM VGA mode 03h / Oak OTI-087 host memory layout (same as linear view of segment 0xB800):
// - Plane 0 = character, plane 1 = attribute; CPU byte stream is interleaved (char, attr) per cell.
// - 80 x 25 cells, 2 bytes per cell, 160 bytes per scanline.
// - CRTC reg 0x13 (offset) = 0x28 for 80 cols: line pitch in bytes = 4 * offset = 160.
// - CRTC reg 0x01 (horizontal display end) = 0x4F for 80 character clocks.
// Mode 01h is 40 cols: offset 0x14 → pitch 80, hdisplay end 0x27.
// See OSDev VGA_Hardware mode 3h table; FreeVGA CRT offset register (2 * offset * word_size).
static constexpr int VGA_TEXT_CELL_BYTES = 2;
static constexpr int VGA_TEXT_FB_PITCH = VGA_TEXT_COLS * VGA_TEXT_CELL_BYTES;   // 160
static constexpr int VGA_TEXT_FB_PITCH_40 = VGA_TEXT_COLS_40 * VGA_TEXT_CELL_BYTES; // 80
static constexpr int VGA_TEXT_CRTC_OFFSET = 0x28;
static constexpr int VGA_TEXT_CRTC_HDISPLAY_END = 0x4F;
/** Bytes between plane-0 and plane-1 bases in a planar linear dump (B8000-style). */
static constexpr int VGA_TEXT_PLANE1_DELTA = 0x2000;

enum class vga_text_vram_layout_t {
    Interleaved,   // char, attr, char, attr... (standard PC/VGA host view)
    PlanarSplit,   // plane0[row*cols+col], plane1 at base + 0x2000
};

/** 8x16 font: 256 glyphs, 16 bytes/scanline per glyph (4096 bytes on disk). */
static constexpr int SS_VRAM_FONT_GLYPH_BYTES = 16;
static constexpr int SS_VRAM_FONT_SIZE = 256 * SS_VRAM_FONT_GLYPH_BYTES;   // 4096
static constexpr uint32_t SS_VRAM_FONT_DEFAULT_BASE = 0x20;

/** SetTextFont bank: $00/$01 apple, $02 ANSI (IBM). */
enum class vga_text_font_bank_t : uint8_t {
    Apple = 0,
    Ansi = 2,
};

/**
 * Load both ROM font bins from disk and bake mask banks.
 * Paths are relative to the resources base (e.g. roms/cards/secondsight/…).
 */
bool vga_text_9x16_load_rom_fonts(const char *apple_path, const char *ansi_path);

/** Select active ROM bank (Apple or Ansi). No-op if banks not loaded. */
void vga_text_9x16_select_rom_font(vga_text_font_bank_t bank);

/** Bake glyph masks from 8x16 font bytes (SetTextFont $03 user font). */
bool vga_text_9x16_load_font_from_vram(const uint8_t *font_base, int glyph_stride = SS_VRAM_FONT_GLYPH_BYTES);

/**
 * A2 text overlay DAC (ROM `set_textmode_palette` / `setup_textmode_palette`):
 * slots 0=bg, 1=fg, 2/3=flash pair. Attr classes 0x01 / 0x10 / 0x23 select them.
 * flash_on matches VideoScanGenerator (true → flash cells inverse).
 */
void vga_text_9x16_set_a2_text_dac(uint32_t bg_argb, uint32_t fg_argb, bool flash_on);

/** Restore IBM CGA/VGA 16-color text palette (host VGA text mode). */
void vga_text_9x16_restore_ibm_palette();

/** CRTC offset reg -> byte pitch for standard VGA text (offset * 4). */
inline int vga_text_pitch_from_crtc_offset(uint8_t crtc_offset) {
    return crtc_offset > 0 ? (int)crtc_offset * 4 : VGA_TEXT_FB_PITCH;
}

/** Raster VGA text (40 or 80 cols) into a uint32_t ARGB buffer. */
void vga_raster_text_9x16(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout = vga_text_vram_layout_t::Interleaved,
    int cols = VGA_TEXT_COLS);

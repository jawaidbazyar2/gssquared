/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_text_8x8.hpp"

#include <cstdio>
#include <cstring>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

using GlyphBank8 = uint8_t[256][VGA_TEXT_8X8_CELL_H];

alignas(64) static GlyphBank8 glyph_masks_ansi8;
alignas(64) static GlyphBank8 glyph_masks_user8;
static GlyphBank8 *glyph_masks_active8 = &glyph_masks_ansi8;
static bool rom_font_8x8_loaded = false;

static void copy_glyphs_8x8(GlyphBank8 *dst, const uint8_t *font_base, int glyph_stride) {
    for (int g = 0; g < 256; g++) {
        const uint8_t *glyph = font_base + g * glyph_stride;
        memcpy((*dst)[g], glyph, VGA_TEXT_8X8_CELL_H);
    }
}

bool vga_text_8x8_load_rom_font(const char *ansi_path) {
    if (!ansi_path) {
        return false;
    }
    FILE *fp = fopen(ansi_path, "rb");
    if (!fp) {
        printf("SecondSight: could not open 8x8 font %s\n", ansi_path);
        return false;
    }
    uint8_t buf[SS_VRAM_FONT_8X8_SIZE];
    const size_t n = fread(buf, 1, SS_VRAM_FONT_8X8_SIZE, fp);
    fclose(fp);
    if (n != SS_VRAM_FONT_8X8_SIZE) {
        printf("SecondSight: 8x8 font %s size %zu (expected %d)\n",
            ansi_path, n, SS_VRAM_FONT_8X8_SIZE);
        return false;
    }
    copy_glyphs_8x8(&glyph_masks_ansi8, buf, SS_VRAM_FONT_8X8_GLYPH_BYTES);
    rom_font_8x8_loaded = true;
    glyph_masks_active8 = &glyph_masks_ansi8;
    return true;
}

void vga_text_8x8_select_rom_font() {
    if (!rom_font_8x8_loaded) {
        return;
    }
    glyph_masks_active8 = &glyph_masks_ansi8;
}

bool vga_text_8x8_load_font_from_vram(const uint8_t *font_base, int glyph_stride) {
    if (font_base == nullptr || glyph_stride < VGA_TEXT_8X8_CELL_H) {
        return false;
    }
    copy_glyphs_8x8(&glyph_masks_user8, font_base, glyph_stride);
    glyph_masks_active8 = &glyph_masks_user8;
    return true;
}

void vga_raster_text_8x8(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout, int cols, int rows)
{
    if (cols <= 0) {
        cols = VGA_TEXT_8X8_COLS;
    }
    if (cols > VGA_TEXT_8X8_COLS) {
        cols = VGA_TEXT_8X8_COLS;
    }
    if (rows <= 0) {
        rows = VGA_TEXT_8X8_ROWS;
    }
    const int screen_h = rows * VGA_TEXT_8X8_CELL_H;
    const GlyphBank8 &masks = *glyph_masks_active8;
    const uint32_t *palette = vga_text_palette();
#if defined(__ARM_NEON)
    const uint32x4_t sel0 = {0x80u, 0x40u, 0x20u, 0x10u};
    const uint32x4_t sel1 = {0x08u, 0x04u, 0x02u, 0x01u};
#endif
    for (int sy = 0; sy < screen_h; sy++) {
        const uint16_t trow = (uint16_t)(sy / VGA_TEXT_8X8_CELL_H);
        const uint16_t gy = (uint16_t)(sy % VGA_TEXT_8X8_CELL_H);
        const uint32_t cellbase = trow * (uint32_t)cols;
        const uint8_t *row_base = (layout == vga_text_vram_layout_t::Interleaved)
            ? (vram + trow * vram_pitch)
            : (vram + cellbase);
        uint32_t *dst = (uint32_t *)((uint8_t *)pixels + sy * pitch);

        for (int x = 0; x < cols; x++) {
            uint8_t ch;
            uint8_t attr;
            if (layout == vga_text_vram_layout_t::Interleaved) {
                const uint32_t vram_off = (uint32_t)x * 2u;
                ch = row_base[vram_off];
                attr = row_base[vram_off + 1];
            } else {
                ch = row_base[x];
                attr = vram[VGA_TEXT_PLANE1_DELTA + cellbase + (uint32_t)x];
            }
            const uint32_t fg = palette[attr & 0x0F];
            const uint32_t bg = palette[(attr >> 4) & 0x0F];
            const uint8_t bits = masks[ch][gy];

#if defined(__ARM_NEON)
            const uint32x4_t vbits = vdupq_n_u32(bits);
            const uint32x4_t m0 = vtstq_u32(vbits, sel0);
            const uint32x4_t m1 = vtstq_u32(vbits, sel1);
            const uint32x4_t fgv = vdupq_n_u32(fg);
            const uint32x4_t bgv = vdupq_n_u32(bg);
            vst1q_u32(dst, vbslq_u32(m0, fgv, bgv));
            vst1q_u32(dst + 4, vbslq_u32(m1, fgv, bgv));
            dst += VGA_TEXT_8X8_CELL_W;
#else
            for (int gx = VGA_TEXT_8X8_CELL_W - 1; gx >= 0; gx--) {
                const uint32_t m = uint32_t(-(int32_t)((bits >> gx) & 1u));
                *dst++ = (fg & m) | (bg & ~m);
            }
#endif
        }
    }
}

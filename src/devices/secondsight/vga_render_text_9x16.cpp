/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_text_9x16.hpp"

#include <cstdio>
#include <cstring>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

static constexpr uint32_t argb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

using GlyphBank = uint16_t[256][VGA_TEXT_CELL_H];

alignas(64) static GlyphBank glyph_masks_apple;
alignas(64) static GlyphBank glyph_masks_ansi;
alignas(64) static GlyphBank glyph_masks_user;
/** Active bank for rasterization (points into apple/ansi/user). */
static GlyphBank *glyph_masks_active = &glyph_masks_apple;

static constexpr uint32_t kIbmTextPalette[16] = {
    argb(0x00,0x00,0x00), argb(0x00,0x00,0xAA), argb(0x00,0xAA,0x00), argb(0x00,0xAA,0xAA),
    argb(0xAA,0x00,0x00), argb(0xAA,0x00,0xAA), argb(0xAA,0x55,0x00), argb(0xAA,0xAA,0xAA),
    argb(0x55,0x55,0x55), argb(0x55,0x55,0xFF), argb(0x55,0xFF,0x55), argb(0x55,0xFF,0xFF),
    argb(0xFF,0x55,0x55), argb(0xFF,0x55,0xFF), argb(0xFF,0xFF,0x55), argb(0xFF,0xFF,0xFF),
};

alignas(64) static uint32_t text_palette[16] = {
    argb(0x00,0x00,0x00), argb(0x00,0x00,0xAA), argb(0x00,0xAA,0x00), argb(0x00,0xAA,0xAA),
    argb(0xAA,0x00,0x00), argb(0xAA,0x00,0xAA), argb(0xAA,0x55,0x00), argb(0xAA,0xAA,0xAA),
    argb(0x55,0x55,0x55), argb(0x55,0x55,0xFF), argb(0x55,0xFF,0x55), argb(0x55,0xFF,0xFF),
    argb(0xFF,0x55,0x55), argb(0xFF,0x55,0xFF), argb(0xFF,0xFF,0x55), argb(0xFF,0xFF,0xFF),
};

static bool rom_fonts_loaded = false;

static void bake_glyph_masks_into(GlyphBank *dst, const uint8_t *font_base, int glyph_stride) {
    for (int g = 0; g < 256; g++) {
        const uint8_t *glyph = font_base + g * glyph_stride;
        for (int gy = 0; gy < VGA_TEXT_CELL_H; gy++) {
            uint16_t bits = 0;
            const uint8_t row = glyph[gy];
            const int src_cols = (g >= 0xC0 && g <= 0xDF) ? (VGA_TEXT_CELL_W - 1) : VGA_TEXT_CELL_W;
            for (int gx = 0; gx < src_cols && gx < 8; gx++) {
                if (row & (0x80u >> gx)) {
                    bits |= uint16_t(1u << (VGA_TEXT_CELL_W - 1 - gx));
                }
            }
            if (g >= 0xC0 && g <= 0xDF) {
                bits = (bits & ~1u) | ((bits >> 1) & 1u);
            }
            (*dst)[g][gy] = bits;
        }
    }
}

static bool load_bin_font(const char *path, GlyphBank *dst) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("SecondSight: could not open font %s\n", path);
        return false;
    }
    uint8_t buf[SS_VRAM_FONT_SIZE];
    const size_t n = fread(buf, 1, SS_VRAM_FONT_SIZE, fp);
    fclose(fp);
    if (n != SS_VRAM_FONT_SIZE) {
        printf("SecondSight: font %s size %zu (expected %d)\n", path, n, SS_VRAM_FONT_SIZE);
        return false;
    }
    bake_glyph_masks_into(dst, buf, SS_VRAM_FONT_GLYPH_BYTES);
    return true;
}

bool vga_text_9x16_load_rom_fonts(const char *apple_path, const char *ansi_path) {
    if (!apple_path || !ansi_path) {
        return false;
    }
    if (!load_bin_font(apple_path, &glyph_masks_apple)) {
        return false;
    }
    if (!load_bin_font(ansi_path, &glyph_masks_ansi)) {
        return false;
    }
    rom_fonts_loaded = true;
    glyph_masks_active = &glyph_masks_apple;
    return true;
}

void vga_text_9x16_select_rom_font(vga_text_font_bank_t bank) {
    if (!rom_fonts_loaded) {
        return;
    }
    glyph_masks_active = (bank == vga_text_font_bank_t::Ansi) ? &glyph_masks_ansi : &glyph_masks_apple;
}

bool vga_text_9x16_load_font_from_vram(const uint8_t *font_base, int glyph_stride) {
    if (font_base == nullptr || glyph_stride < 8) {
        return false;
    }
    bake_glyph_masks_into(&glyph_masks_user, font_base, glyph_stride);
    glyph_masks_active = &glyph_masks_user;
    return true;
}

void vga_text_9x16_set_a2_text_dac(uint32_t bg_argb, uint32_t fg_argb, bool flash_on) {
    // ROM set_textmode_palette: DAC0=bg, DAC1=fg, DAC2=fg, DAC3=bg.
    // setup_textmode_palette swaps DAC2/3 each blink; flash_on → inverse phase.
    text_palette[0] = bg_argb;
    text_palette[1] = fg_argb;
    if (flash_on) {
        text_palette[2] = fg_argb;
        text_palette[3] = bg_argb; // attr 0x23 → bg on fg
    } else {
        text_palette[2] = bg_argb;
        text_palette[3] = fg_argb; // attr 0x23 → fg on bg
    }
}

void vga_text_9x16_restore_ibm_palette() {
    for (int i = 0; i < 16; i++) {
        text_palette[i] = kIbmTextPalette[i];
    }
}

void vga_raster_text_9x16(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout, int cols)
{
    if (cols <= 0) {
        cols = VGA_TEXT_COLS;
    }
    if (cols > VGA_TEXT_COLS) {
        cols = VGA_TEXT_COLS;
    }
    const GlyphBank &masks = *glyph_masks_active;
#if defined(__ARM_NEON)
    const uint32x4_t sel0 = {0x100u, 0x80u, 0x40u, 0x20u};
    const uint32x4_t sel1 = {0x10u, 0x8u, 0x4u, 0x2u};
#endif
    for (int sy = 0; sy < VGA_TEXT_SCREEN_H; sy++) {
        const uint16_t trow = sy / VGA_TEXT_CELL_H;
        const uint16_t gy   = sy % VGA_TEXT_CELL_H;
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
                ch   = row_base[vram_off];
                attr = row_base[vram_off + 1];
            } else {
                ch   = row_base[x];
                attr = vram[VGA_TEXT_PLANE1_DELTA + cellbase + (uint32_t)x];
            }
            const uint32_t fg = text_palette[attr & 0x0F];
            const uint32_t bg = text_palette[(attr >> 4) & 0x0F];
            uint16_t bits = masks[ch][gy];

#if defined(__ARM_NEON)
            const uint32x4_t vbits = vdupq_n_u32(bits);
            const uint32x4_t m0 = vtstq_u32(vbits, sel0);
            const uint32x4_t m1 = vtstq_u32(vbits, sel1);
            const uint32x4_t fgv = vdupq_n_u32(fg);
            const uint32x4_t bgv = vdupq_n_u32(bg);
            vst1q_u32(dst, vbslq_u32(m0, fgv, bgv));
            vst1q_u32(dst + 4, vbslq_u32(m1, fgv, bgv));
            dst[8] = (bits & 1u) ? fg : bg;
            dst += VGA_TEXT_CELL_W;
#else
            for (int gx = VGA_TEXT_CELL_W - 1; gx >= 0; gx--) {
                const uint32_t m = uint32_t(-(int32_t)((bits >> gx) & 1u));
                *dst++ = (fg & m) | (bg & ~m);
            }
#endif
        }
    }
}

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_text_9x16.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdio>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

static constexpr int ATLAS_COL_STRIDE = 9;
static constexpr int ATLAS_ROW_STRIDE = 17;

static inline uint32_t argb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

alignas(64) static uint16_t glyph_masks[256][VGA_TEXT_CELL_H];
alignas(64) static const uint32_t text_palette[16] = {
    argb(0x00,0x00,0x00), argb(0x00,0x00,0xAA), argb(0x00,0xAA,0x00), argb(0x00,0xAA,0xAA),
    argb(0xAA,0x00,0x00), argb(0xAA,0x00,0xAA), argb(0xAA,0x55,0x00), argb(0xAA,0xAA,0xAA),
    argb(0x55,0x55,0x55), argb(0x55,0x55,0xFF), argb(0x55,0xFF,0x55), argb(0x55,0xFF,0xFF),
    argb(0xFF,0x55,0x55), argb(0xFF,0x55,0xFF), argb(0xFF,0xFF,0x55), argb(0xFF,0xFF,0xFF),
};

static bool font_ready = false;

static void bake_glyph_masks_from_vram_8x16(const uint8_t *font_base, int glyph_stride) {
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
            glyph_masks[g][gy] = bits;
        }
    }
}

bool vga_text_9x16_load_font_from_vram(const uint8_t *font_base, int glyph_stride) {
    if (font_base == nullptr || glyph_stride < 8) {
        return false;
    }
    bake_glyph_masks_from_vram_8x16(font_base, glyph_stride);
    font_ready = true;
    return true;
}

static bool bake_glyph_masks(SDL_Surface *fs) {
    const uint8_t *base = (const uint8_t *)fs->pixels;
    const int spitch = fs->pitch;
    for (int g = 0; g < 256; g++) {
        const int sx = (g & 0x0F) * ATLAS_COL_STRIDE;
        const int sy = (g >> 4)   * ATLAS_ROW_STRIDE;
        for (int gy = 0; gy < VGA_TEXT_CELL_H; gy++) {
            uint16_t bits = 0;
            const uint8_t *p = base + (sy + gy) * spitch + sx * 4;
            const int src_cols = (g >= 0xC0 && g <= 0xDF) ? (VGA_TEXT_CELL_W - 1) : VGA_TEXT_CELL_W;
            for (int gx = 0; gx < src_cols; gx++) {
                uint8_t r = p[0], gc = p[1], b = p[2], a = p[3];
                bool on = (a > 127) && ((r > 127) || (gc > 127) || (b > 127));
                if (on) bits |= uint16_t(1u << (VGA_TEXT_CELL_W - 1 - gx));
                p += 4;
            }
            if (g >= 0xC0 && g <= 0xDF) {
                bits = (bits & ~1u) | ((bits >> 1) & 1u);
            }
            glyph_masks[g][gy] = bits;
        }
    }
    return true;
}

bool vga_text_9x16_init(const char *font_path) {
    if (font_ready) {
        return true;
    }
    SDL_Surface *font_surface = IMG_Load(font_path);
    if (font_surface == NULL) {
        printf("SecondSight: font bitmap could not be loaded (%s): %s\n", font_path, SDL_GetError());
        return false;
    }
    SDL_Surface *fs = SDL_ConvertSurface(font_surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(font_surface);
    if (fs == NULL) {
        printf("SecondSight: font surface conversion failed: %s\n", SDL_GetError());
        return false;
    }
    if (!bake_glyph_masks(fs)) {
        SDL_DestroySurface(fs);
        return false;
    }
    SDL_DestroySurface(fs);
    font_ready = true;
    return true;
}

void vga_raster_text_9x16(const uint8_t *vram, int vram_pitch, uint32_t *pixels, int pitch,
    vga_text_vram_layout_t layout)
{
#if defined(__ARM_NEON)
    const uint32x4_t sel0 = {0x100u, 0x80u, 0x40u, 0x20u};
    const uint32x4_t sel1 = {0x10u, 0x8u, 0x4u, 0x2u};
#endif
    for (int sy = 0; sy < VGA_TEXT_SCREEN_H; sy++) {
        const uint16_t trow = sy / VGA_TEXT_CELL_H;
        const uint16_t gy   = sy % VGA_TEXT_CELL_H;
        const uint32_t cellbase = trow * VGA_TEXT_COLS;
        const uint8_t *row_base = (layout == vga_text_vram_layout_t::Interleaved)
            ? (vram + trow * vram_pitch)
            : (vram + cellbase);
        uint32_t *dst = (uint32_t *)((uint8_t *)pixels + sy * pitch);

        for (uint16_t x = 0; x < VGA_TEXT_COLS; x++) {
            uint8_t ch;
            uint8_t attr;
            if (layout == vga_text_vram_layout_t::Interleaved) {
                const uint32_t vram_off = x * 2u;
                ch   = row_base[vram_off];
                attr = row_base[vram_off + 1];
            } else {
                ch   = row_base[x];
                attr = vram[VGA_TEXT_PLANE1_DELTA + cellbase + x];
            }
            const uint32_t fg = text_palette[attr & 0x0F];
            const uint32_t bg = text_palette[(attr >> 4) & 0x0F];
            uint16_t bits = glyph_masks[ch][gy];

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

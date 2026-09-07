/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_text_8x8_present.hpp"
#include "vga_render_text_8x8.hpp"
#include "videosystem.hpp"

#include <SDL3/SDL.h>

void vga_render_text_8x8(video_system_t *vs, SDL_Texture *tex_text, const uint8_t *vram, int vram_pitch,
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
    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(tex_text, nullptr, &pixels, &pitch)) {
        vga_raster_text_8x8(vram, vram_pitch, (uint32_t *)pixels, pitch, layout, cols, rows);
        SDL_UnlockTexture(tex_text);
    }
    const float src_w = (float)(cols * VGA_TEXT_8X8_CELL_W);
    const float src_h = (float)(rows * VGA_TEXT_8X8_CELL_H);
    SDL_FRect src = { 0.0f, 0.0f, src_w, src_h };
    vs->render_frame(tex_text, &src, nullptr);
}

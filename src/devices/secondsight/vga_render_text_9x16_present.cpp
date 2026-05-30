/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_text_9x16_present.hpp"
#include "vga_render_text_9x16.hpp"
#include "videosystem.hpp"

#include <SDL3/SDL.h>

void vga_render_text_9x16(video_system_t *vs, SDL_Texture *tex_text, const uint8_t *vram, int vram_pitch,
    vga_text_vram_layout_t layout)
{
    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(tex_text, nullptr, &pixels, &pitch)) {
        vga_raster_text_9x16(vram, vram_pitch, (uint32_t *)pixels, pitch, layout);
        SDL_UnlockTexture(tex_text);
    }
    SDL_FRect src = { 0.0f, 0.0f, (float)VGA_TEXT_SCREEN_W, (float)VGA_TEXT_SCREEN_H };
    vs->render_frame(tex_text, &src, nullptr);
}

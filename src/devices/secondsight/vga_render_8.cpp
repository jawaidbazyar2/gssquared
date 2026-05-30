/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_8.hpp"
#include "videosystem.hpp"

#include <SDL3/SDL.h>

void expand_8bpp_to_rgb24(uint8_t *rgb24_buffer, const uint8_t palette_rgb[256][3],
    const uint8_t *src, int src_pitch, int width, int height)
{
    for (int y = 0; y < height; y++) {
        const uint8_t *row = src + (y * src_pitch);
        uint8_t *out = rgb24_buffer + (y * SS_RGB24_PITCH);
        for (int x = 0; x < width; x++) {
            const uint8_t *rgb = palette_rgb[row[x]];
            out[x * 3 + 0] = rgb[0];
            out[x * 3 + 1] = rgb[1];
            out[x * 3 + 2] = rgb[2];
        }
    }
}

void vga_render_8bpp(video_system_t *vs, SDL_Texture *tex_24bpp, uint8_t *rgb24_buffer,
    const uint8_t palette_rgb[256][3], const uint8_t *display_base, int fb_pitch,
    int width, int height)
{
    expand_8bpp_to_rgb24(rgb24_buffer, palette_rgb, display_base, fb_pitch, width, height);
    SDL_UpdateTexture(tex_24bpp, nullptr, rgb24_buffer, SS_RGB24_PITCH);
    SDL_FRect src = { 0.0f, 0.0f, (float)width, (float)height };
    vs->render_frame(tex_24bpp, &src, nullptr);
}

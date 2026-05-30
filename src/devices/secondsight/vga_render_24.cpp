/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "vga_render_24.hpp"
#include "videosystem.hpp"

#include <SDL3/SDL.h>

void vga_render_24bpp(video_system_t *vs, SDL_Texture *tex_24bpp, const uint8_t *display_base,
    int fb_pitch, int width, int height)
{
    SDL_UpdateTexture(tex_24bpp, nullptr, display_base, fb_pitch);
    SDL_FRect src = { 0.0f, 0.0f, (float)width, (float)height };
    vs->render_frame(tex_24bpp, &src, nullptr);
}

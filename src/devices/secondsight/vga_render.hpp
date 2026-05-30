/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <cstdint>

struct video_system_t;
struct SDL_Texture;

static constexpr int SS_MAX_WIDTH = 1024;
static constexpr int SS_MAX_HEIGHT = 768;
static constexpr int SS_RGB24_PITCH = SS_MAX_WIDTH * 3;
static constexpr size_t SS_RGB24_BUFFER_SIZE = SS_RGB24_PITCH * SS_MAX_HEIGHT;

void expand_8bpp_to_rgb24(uint8_t *rgb24_buffer, const uint8_t palette_rgb[256][3],
    const uint8_t *src, int src_pitch, int width, int height);

void vga_render_8bpp(video_system_t *vs, SDL_Texture *tex_24bpp, uint8_t *rgb24_buffer,
    const uint8_t palette_rgb[256][3], const uint8_t *display_base, int fb_pitch,
    int width, int height);

void vga_render_16bpp(video_system_t *vs, SDL_Texture *tex_16bpp, const uint8_t *display_base,
    int fb_pitch, int width, int height);

void vga_render_24bpp(video_system_t *vs, SDL_Texture *tex_24bpp, const uint8_t *display_base,
    int fb_pitch, int width, int height);

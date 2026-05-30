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
#include "vga_render_text_9x16.hpp"

struct video_system_t;
struct SDL_Texture;

/** Lock tex, raster, unlock, and present at 720x400. */
void vga_render_text_9x16(video_system_t *vs, SDL_Texture *tex_text, const uint8_t *vram, int vram_pitch,
    vga_text_vram_layout_t layout = vga_text_vram_layout_t::Interleaved);

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

/** Style_t */

struct Style_t {
    // Colors
    uint32_t background_color = 0x00000000;
    uint32_t border_color = 0xFFFFFFFF;
    uint32_t hover_color = 0x008080FF;
    
    // Layout
    uint32_t padding = 5;
    uint32_t border_width = 1;
    
    // Button-specific
    uint32_t text_color = 0x000000FF;  // For text buttons
};

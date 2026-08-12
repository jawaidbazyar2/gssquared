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

/**
 * Second Sight ROM font assembled as `copy_font` lays out the VGA charset:
 * 0x00-0x7F font_table_8x16, 0x81-0x9E language/extended, 0xC0 unk_8875,
 * 0xC1-0xDF MouseText 1-31 (mirrors 0x01-0x1F). Source: ROM_BEEF.asm.
 */
extern const uint8_t SS_ROM_FONT_8X16[SS_VRAM_FONT_SIZE];

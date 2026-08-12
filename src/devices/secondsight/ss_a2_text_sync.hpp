/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

struct display_state_t;

/** True when Apple II soft switches select full-screen TEXT (not SHR). */
bool ss_apple2_fullscreen_text(const display_state_t *ds);

/**
 * Copy Apple II text page into a VGA mode-03h interleaved text buffer
 * (char, attr pairs; pitch typically 160; 80 cols × 25 rows).
 * Rows 0–23 come from the A2 page; row 24 is cleared to spaces.
 */
void ss_sync_a2_text_to_vga(const uint8_t *a2_ram, uint8_t *vga_base, int pitch,
                            display_state_t *ds);

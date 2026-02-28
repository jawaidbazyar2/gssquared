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

#include <stdio.h>

#include "cpu.hpp"
#include "devices/game/gamecontroller.hpp"

/**
 * This tries to simulate paddle(0) using the mouse wheel. It's not great.
 */

void handle_mouse_wheel(gamec_state_t *ds, const SDL_Event &event) {
    int wheel_y = event.wheel.y;

    int new_pos = ds->mouse_wheel_pos_0 + (wheel_y * 2); // sensitivity 2.
    if (new_pos < 0) new_pos = 0;
    if (new_pos > 255) new_pos = 255;
    ds->mouse_wheel_pos_0 = new_pos;
    printf("mouse wheel diff: %d, position: %d\n", wheel_y, ds->mouse_wheel_pos_0);
}

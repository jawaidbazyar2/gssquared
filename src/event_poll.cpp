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

#include <SDL3/SDL.h>

#include "cpu.hpp"
#include "computer.hpp"

#include "devices/speaker/speaker.hpp"


// Loops until there are no events in queue waiting to be read.

bool handle_sdl_keydown(computer_t *computer, cpu_state *cpu, SDL_Event event) {

    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;

    if (key == SDLK_F8) {
        toggle_speaker_recording(cpu);
        //debug_dump_disk_images(cpu);
        return true;
    }

    return false;
}

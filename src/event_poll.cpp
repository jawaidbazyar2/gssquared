/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include "debug.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "devices/keyboard/keyboard.hpp"
#include "display/display.hpp"
#include "devices/game/mousewheel.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/speaker/speaker.hpp"
#include "devices/loader.hpp"
#include "devices/diskii/diskii.hpp"
#include "display/ntsc.hpp"

// Loops until there are no events in queue waiting to be read.

bool handle_sdl_keydown(computer_t *computer, cpu_state *cpu, SDL_Event event) {

    // Ignore if only shift is pressed
    /* uint16_t mod = event.key.keysym.mod;
    SDL_Keycode key = event.key.keysym.sym; */
    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;

 /*    if ((mod & SDL_KMOD_CTRL) && (key == SDLK_F10)) {
        if (mod & SDL_KMOD_ALT) {
            computer->reset(true); 
        } else {
            computer->reset(false); 
        }
        return true;
    } */

  /*   if (key == SDLK_F12) { 
        cpu->halt = HLT_USER; 
        return true;
    } */
/*     if (key == SDLK_F9) { 
        toggle_clock_mode(cpu);
        return true; 
    } */
    if (key == SDLK_F8) {
        toggle_speaker_recording(cpu);
        //debug_dump_disk_images(cpu);
        return true;
    }

   /*  if (key == SDLK_F6) {
        if (mod & SDL_KMOD_CTRL) {
            // dump hires image page 1
            display_dump_text_page(cpu, 1);
            return true;
        }
        if (mod & SDL_KMOD_SHIFT) {
            // dump hires image page 2
            display_dump_text_page(cpu, 2);
            return true;
        }
    } */
    /* if (key == SDLK_F7) {
        if (mod & SDL_KMOD_CTRL) {
            // dump hires image page 1
            display_dump_hires_page(cpu, 1);
            return true;
        }
        if (mod & SDL_KMOD_SHIFT) {
            // dump hires image page 2
            display_dump_hires_page(cpu, 2);
            return true;
        }
    } */

    return false;
}

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

#include <SDL3/SDL.h>
#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "mbus/KeyboardMessage.hpp"

#define KB_LATCH_ADDRESS 0xC000
#define KB_CLEAR_LATCH_ADDRESS 0xC010

struct keyboard_state_t {
    uint8_t kb_key_strobe = 0x41;
    std::string paste_buffer;
    message_keyboard_t *mk = nullptr;
    MMU_II *mmu = nullptr;
    ResetController *reset_control = nullptr;
    int key_down_count = 0;
    // Monotonic count of "empty" keyboard polls: reads of $C000 that returned
    // no key (bit 7 clear). A rapidly rising count is the deterministic
    // signal that the machine is spinning in an input-wait loop (GETLN/KEYIN)
    // — used to trigger on machine behaviour rather than wall-clock time, so
    // it holds at any emulation speed. Read-only outside the keyboard device.
    uint64_t kb_poll_empty = 0;
} ;

/* uint8_t kb_memory_read(uint16_t address);
void kb_memory_write(uint16_t address, uint8_t value); */
void kb_key_pressed(keyboard_state_t *kb_state, uint8_t key);
void handle_keydown_iiplus(cpu_state *cpu, const SDL_Event &event);
void init_mb_iiplus_keyboard(computer_t *computer, SlotType_t slot);
void init_mb_iie_keyboard(computer_t *computer, SlotType_t slot);

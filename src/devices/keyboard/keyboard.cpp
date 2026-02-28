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

#include <cstdio>
#include <SDL3/SDL.h>
#include "SDL3/SDL_keycode.h"
#include "gs2.hpp"
#include "computer.hpp"
#include "debug.hpp"
#include "keyboard.hpp"
#include "util/applekeys.hpp"
#include "mbus/KeyboardMessage.hpp"

// Software should be able to:
// Read keyboard from register at $C000.
// Write to the keyboard clear latch at $C010.

inline void kb_key_pressed(keyboard_state_t *kb_state, uint8_t key) {
    kb_state->kb_key_strobe = key | 0x80;
    if (kb_state->mk) kb_state->mk->last_key_val = key;
}

inline void kb_clear_strobe(keyboard_state_t *kb_state) {
    kb_state->kb_key_strobe = kb_state->kb_key_strobe & 0x7F;
    if (kb_state->mk) kb_state->mk->last_key_val = kb_state->kb_key_strobe;
}

uint8_t kb_read_C00X(void *context, uint32_t address) {
    //fprintf(stderr, "kb_memory_read %04X\n", address);
    keyboard_state_t *kb_state = (keyboard_state_t *)context;

    if ((kb_state->kb_key_strobe & 0x80) == 0) { // if keyboard does not already have a buffered character.. 
        if (kb_state->paste_buffer.length() > 0) { // if there is a paste buffer, use it.
            uint8_t key = kb_state->paste_buffer[0];
            if (key == '\n') {
                key = '\r'; // apple 2's like \r :)
            }
            // TODO: do we need to handle Windows-style \r\n?
            kb_key_pressed(kb_state, key);
            kb_state->paste_buffer = kb_state->paste_buffer.substr(1);
        }
    }
    uint8_t key = kb_state->kb_key_strobe;
    return key;
}

uint8_t kb_read_C01X(void *context, uint32_t address) {
    keyboard_state_t *kb_state = (keyboard_state_t *)context;

    // Clear the keyboard latch
    kb_clear_strobe(kb_state);
    return kb_state->mmu->floating_bus_read();
}

// iie only
uint8_t kb_read_C010(void *context, uint32_t address) {
    keyboard_state_t *kb_state = (keyboard_state_t *)context;

    // Clear the keyboard latch
    kb_clear_strobe(kb_state);
    // AKD is "any key down". it is set instantly whenever a key is pressed.
    // the apple ii keyboard can't report multiple keys. So, just check to see if SDL
        // sees any key as down.
        // alternately we can keep track of all key-down and key-up events and if there are more key down than key up, set AKD.
    int numkeys;
    const bool *keyarr = SDL_GetKeyboardState(&numkeys);
    bool akd = false;
    for (int i = 0; i < numkeys; i++) {
        if (keyarr[i]) { akd = true; break; }
    }
   
    return kb_state->kb_key_strobe | (akd ? 0x80 : 0x00);
}

void kb_write_C01X(void *context, uint32_t address, uint8_t value) {
    keyboard_state_t *kb_state = (keyboard_state_t *)context;

    // Clear the keyboard latch
    kb_clear_strobe(kb_state);
}

void decode_key_mod(SDL_Keycode key, SDL_Keymod mod) {
    fprintf(stdout, "key: %08X mod: ", key);
    if (mod & SDL_KMOD_LSHIFT) {
        fprintf(stdout, "LSHIFT ");
    }
    if (mod & SDL_KMOD_RSHIFT) {
        fprintf(stdout, "RSHIFT ");
    }
    if (mod & SDL_KMOD_LCTRL) {
        fprintf(stdout, "LCTRL ");
    }
    if (mod & SDL_KMOD_RCTRL) {
        fprintf(stdout, "RCTRL ");
    }
    if (mod & SDL_KMOD_LALT) {
        fprintf(stdout, "LALT ");
    }
    if (mod & SDL_KMOD_RALT) {
        fprintf(stdout, "RALT ");
    }
    fprintf(stdout, "\n");
}

void handle_paste(keyboard_state_t *kb_state, const SDL_Event &event) {
    fprintf(stdout, "handle_paste\n");
    char *clipboardText = SDL_GetClipboardText();
    if (clipboardText) {
        fprintf(stdout, "clipboardText: %s\n", clipboardText);
        kb_state->paste_buffer = std::string(clipboardText);
        SDL_free(clipboardText);
    }
}

//void handle_keydown_iiplus(cpu_state *cpu, const SDL_Event &event) {
void handle_keydown_iiplus(const SDL_Event &event, keyboard_state_t *kb_state) {

    // Ignore if only shift is pressed
    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;

    if (DEBUG(DEBUG_KEYBOARD))  decode_key_mod(key, mod);

    if (mod & SDL_KMOD_CTRL) { // still have to handle control this way..
        // Convert lowercase to control code (0x01-0x1A)
        if (mod & SDL_KMOD_SHIFT && key == 'p') { // handle control-shift-P as a special case.
            kb_key_pressed(kb_state, 0x00);
        } else if (mod & SDL_KMOD_SHIFT && key == 'n') { // handle control-shift-N as a special case.
            kb_key_pressed(kb_state, 0x1E);
        } else {
            if (key >= 'a' && key <= 'z') {
                key = key - 'a' + 1;
                kb_key_pressed(kb_state, key);
                /* if (DEBUG(DEBUG_KEYBOARD)) fprintf(stdout, "control key pressed: %08X\n", key); */
            } 
        }
    }  else {
        // map the scancode + mods to a sensible keycode. Strip ALT/GUI to prevent weird mapping where hi bit gets set.
        SDL_Keycode mapped = SDL_GetKeyFromScancode(event.key.scancode, event.key.mod & KEYMOD_REMOVE, false);
        if (DEBUG(DEBUG_KEYBOARD)) printf("mapped key: %08X\n", mapped);

        if (mapped == SDLK_LEFT) { kb_key_pressed(kb_state, 0x08); return; }
        if (mapped == SDLK_RIGHT) { kb_key_pressed(kb_state, 0x15); return; }
        if (mapped >= 'a' && mapped <= 'z') mapped = mapped - 'a' + 'A';
        if (mapped < 128) {
            kb_key_pressed(kb_state, mapped);
        }
    }
}

void init_mb_iiplus_keyboard(computer_t *computer, SlotType_t slot) {
    if (DEBUG(DEBUG_KEYBOARD)) fprintf(stdout, "init_keyboard\n");
    keyboard_state_t *kb_state = new keyboard_state_t;
    computer->set_module_state(MODULE_KEYBOARD, kb_state);

    kb_state->mmu = computer->mmu;

    /** Sather P31: 'The keyboard read addres sis $C00X and the strobe flip-flop reset address is $C01X. */
    for (int i = 0; i < 16; i++) {
        computer->mmu->set_C0XX_read_handler(0xC000+i, { kb_read_C00X, kb_state });

        /* on II's through the II+, the keyboard strobe reset is at $C01X read or write. This changes on the IIe and later. */
        computer->mmu->set_C0XX_read_handler(0xC010+i, { kb_read_C01X, kb_state });
        computer->mmu->set_C0XX_write_handler(0xC010+i, { kb_write_C01X, kb_state });
    }

    computer->dispatch->registerHandler(SDL_EVENT_KEY_DOWN, [kb_state](const SDL_Event &event) {
        if (event.key.key == SDLK_INSERT && event.key.mod & SDL_KMOD_SHIFT) {
            handle_paste(kb_state,event);
            return true;
        }
        handle_keydown_iiplus(event, kb_state);
        return false;
    });
}

void handle_keydown_iie(const SDL_Event &event, keyboard_state_t *kb_state) {

    // Ignore if only shift is pressed
    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;

    if (DEBUG(DEBUG_KEYBOARD))  decode_key_mod(key, mod);

    if (mod & SDL_KMOD_CTRL) { // still have to handle control this way..
        // Convert lowercase to control code (0x01-0x1A)
        if (key >= 'a' && key <= 'z') {
            key = key - 'a' + 1;
            kb_key_pressed(kb_state, key);
            /* if (DEBUG(DEBUG_KEYBOARD)) fprintf(stdout, "control key pressed: %08X\n", key); */
        } else if (mod & SDL_KMOD_SHIFT && key == '2') { // handle control-@ as a special case.
            kb_key_pressed(kb_state, 0x00);
        } else if (mod & SDL_KMOD_SHIFT && key == '6') { // handle control-shift-N as a special case.
            kb_key_pressed(kb_state, 0x1E);
        }

    }  else {
        // we need to map backspace to 0x7F because SDL maps backspace/delete to 0x08 (not what we want)
        if (event.key.scancode == SDL_SCANCODE_BACKSPACE) {
            kb_key_pressed(kb_state, 0x7F);
            return;
        }
        // map the scancode + mods to a sensible keycode
        SDL_Keycode mapped = SDL_GetKeyFromScancode(event.key.scancode, event.key.mod & KEYMOD_REMOVE, false);
        if (DEBUG(DEBUG_KEYBOARD)) printf("mapped key: %08X\n", mapped);

        if (mapped == SDLK_LEFT) { kb_key_pressed(kb_state, 0x08); return; }
        if (mapped == SDLK_RIGHT) { kb_key_pressed(kb_state, 0x15); return; }
        if (mapped == SDLK_UP) { kb_key_pressed(kb_state, 0x0B); return; }
        if (mapped == SDLK_DOWN) { kb_key_pressed(kb_state, 0x0A); return; }
        //if (mapped >= 'a' && mapped <= 'z') mapped = mapped - 'a' + 'A'; IIe allows lowercase!
        if (mapped < 128) {
            kb_key_pressed(kb_state, mapped);
        }
    }
}

void init_mb_iie_keyboard(computer_t *computer, SlotType_t slot) {
    if (DEBUG(DEBUG_KEYBOARD)) fprintf(stdout, "init_keyboard\n");
    keyboard_state_t *kb_state = new keyboard_state_t;
    computer->set_module_state(MODULE_KEYBOARD, kb_state);

    kb_state->mmu = computer->mmu;

    /** Sather P31: 'The keyboard read addres sis $C00X and the strobe flip-flop reset address is $C01X. */
    // IIe Read C000-C00F - same as II+.
    // Write C010 - C01F: same as II+
    // Write C000-C00F: new softswitches for memory management.
    // Read C010 - C01F: new softswitch status flags.

    for (int i = 0; i < 16; i++) {
        computer->mmu->set_C0XX_read_handler(0xC000+i, { kb_read_C00X, kb_state });

        /* on //e, writes to C010 - C01F still clear KBDSTRB but reads are different. */
        //computer->mmu->set_C0XX_read_handler(0xC010+i, { kb_read_C01X, kb_state });
        computer->mmu->set_C0XX_write_handler(0xC010+i, { kb_write_C01X, kb_state });
    }
    computer->mmu->set_C0XX_read_handler(0xC010, { kb_read_C010, kb_state });

    computer->dispatch->registerHandler(SDL_EVENT_KEY_DOWN, [kb_state](const SDL_Event &event) {
        if (event.key.key == SDLK_INSERT && event.key.mod & SDL_KMOD_SHIFT) {
            handle_paste(kb_state,event);
            return true;
        }
        handle_keydown_iie(event, kb_state);
        return false;
    });

    // set up the keyboard message.
    kb_state->mk = new message_keyboard_t;
    Message *msg = new KeyboardMessage(kb_state->mk);
    kb_state->mk->last_key_val = kb_state->kb_key_strobe;
    computer->mbus->send(msg);
}

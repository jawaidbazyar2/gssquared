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

/**
 * "Game Controller" or "Game Input"
 * 
 */
#include <cmath>
#include <algorithm>

#include <SDL3/SDL.h>
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_oldnames.h"
#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/game/mousewheel.hpp"
#include "devices/annunciator/annunciator.hpp"
#include "util/applekeys.hpp"

/**
 * this is a relatively naive implementation of game controller,
 * translating mouse position inside the emulator window, to the paddle/joystick
 * inputs. This may not be the easiest thing to control.
 */

/**
 * Each input (start with two) will decay to 0 over time,
 * at which point we flip the high bit of the register on.
 * At each read of -any- inputs, we decrement -all- the input
 * counters.
 * 
 * The counters are measured in cycles.
 * 
 * Read the Mouse X,Y location.
 * 3ms is what the //e reference says the decay time is.
 * 3ms = 2800 cycles.
 */

#define GAME_INPUT_DECAY_TIME 2805

/**
 * @struct JoystickValues
 * @brief Structure to hold joystick coordinate values
 */
struct JoystickValues {
    int x;
    int y;
};

/**
 * @brief Converts modern circular joystick values to Apple II square joystick range
 * @param x Raw X input from modern joystick (-32767 to 32767)
 * @param y Raw Y input from modern joystick (-32767 to 32767)
 * @return JoystickValues Apple II compatible values {x: 0-255, y: 0-255}
 */
JoystickValues convertJoystickValues(int32_t x, int32_t y) {
    // was told to ignore very small values as on some joysticks center can be off and there can be noise.
    if (std::abs(x) < 1000) {
        x = 0;
    }
    if (std::abs(y) < 1000) {
        y = 0;
    }
    // Normalize input values to range [-1, 1]
    double xNorm = static_cast<double>(x) / 32767.0;
    double yNorm = static_cast<double>(y) / 32767.0;
    
    // Calculate the length of the vector (distance from center)
    double length = std::sqrt(xNorm * xNorm + yNorm * yNorm);
    
    // If length is 0 or very small, return center position
    if (length < 0.0001) {
        return {128, 128}; // Center of Apple II range
    }
    
    // Normalize to unit vector
    double xUnit = xNorm / length;
    double yUnit = yNorm / length;
    
    double xSquare, ySquare;
    
    // Find the maximum scale factor to map to a square
    // The maximum scale is determined by which component (x or y) will hit the boundary first
    double scale = 1.0 / std::max(std::abs(xUnit), std::abs(yUnit));
    
    // Scale the unit vector by our scale factor and by the original length
    // This maintains the proportional distance from center
    xSquare = xUnit * scale * std::min(length, 1.0);
    ySquare = yUnit * scale * std::min(length, 1.0);
    
    // Scale and shift to Apple II range (0-255)
    int xApple = static_cast<int>(std::round((xSquare + 1.0) * 128));
    int yApple = static_cast<int>(std::round((ySquare + 1.0) * 128));
    
    // Clamp values to ensure they're within 0-255 range
    return {
        std::clamp(xApple, 0, 255),
        std::clamp(yApple, 0, 255)
    };
}

uint8_t strobe_game_inputs(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);

    if (ds->gps[0].game_type == GAME_INPUT_TYPE_MOUSE) {
        float mouse_x, mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        if (ds->paddle_flip_01) {
            uint64_t x_trigger =  cpu->cycles + (GAME_INPUT_DECAY_TIME * (1.0f - (float(mouse_x) / WINDOW_WIDTH)));
            uint64_t y_trigger = cpu->cycles + (GAME_INPUT_DECAY_TIME * (1.0f - (float(mouse_y) / WINDOW_HEIGHT)));

            ds->game_input_trigger_0 = y_trigger;
            ds->game_input_trigger_1 =x_trigger;   
        } else {
            uint64_t x_trigger =  cpu->cycles + (GAME_INPUT_DECAY_TIME * (float(mouse_x) / WINDOW_WIDTH));
            uint64_t y_trigger = cpu->cycles + (GAME_INPUT_DECAY_TIME * (float(mouse_y) / WINDOW_HEIGHT));

            ds->game_input_trigger_0 = x_trigger;
            ds->game_input_trigger_1 = y_trigger;
        }
        if (DEBUG(DEBUG_GAME)) fprintf(stdout, "Strobe game inputs: %f, %f: %llu, %llu\n", mouse_x, mouse_y, ds->game_input_trigger_0, ds->game_input_trigger_1);
    } else if (ds->gps[0].game_type == GAME_INPUT_TYPE_MOUSEWHEEL) {
        ds->game_input_trigger_0 = cpu->cycles + (GAME_INPUT_DECAY_TIME / 255) * ds->mouse_wheel_pos_0;
    } else if (ds->gps[0].game_type == GAME_INPUT_TYPE_GAMEPAD) {
        // Scale the axes larger, to get the corners to full extent
        
        int32_t axis0 = SDL_GetGamepadAxis(ds->gps[0].gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        int32_t axis1 = SDL_GetGamepadAxis(ds->gps[0].gamepad, SDL_GAMEPAD_AXIS_LEFTY);

        JoystickValues jv = convertJoystickValues(axis0, axis1);
        uint64_t x_trigger =  cpu->cycles + ((GAME_INPUT_DECAY_TIME * jv.x) / 255);
        uint64_t y_trigger = cpu->cycles + ((GAME_INPUT_DECAY_TIME * jv.y) / 255);

        ds->game_input_trigger_0 = x_trigger;
        ds->game_input_trigger_1 = y_trigger;
    }
    return ds->mmu->floating_bus_read();
}

void strobe_game_inputs_w(void *context, uint16_t address, uint8_t value) {
    strobe_game_inputs(context, address);
}

uint8_t read_game_input_0(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);

    if (ds->game_input_trigger_0 > cpu->cycles) {
        return 0x80;
    }
    return 0x00;
}

uint8_t read_game_input_1(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    if (ds->game_input_trigger_1 > cpu->cycles) {   
        return 0x80;
    }
    return 0x00;
}

uint8_t read_game_input_2(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);

    if (ds->game_input_trigger_2 > cpu->cycles) {
        return 0x80;
    }
    return 0x00;
}

uint8_t read_game_input_3(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    if (ds->game_input_trigger_3 > cpu->cycles) {
        return 0x80;
    }
    return 0x00;
}

uint8_t read_game_switch_0(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    
    if ((ds->joystick_mode == JOYSTICK_MODE_JOYPORT_ATARI) && (cpu->cycles > ds->joyport_activate)) { // reverse polarity for atari
        bool val = SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_EAST);
        return val ? 0x00 : 0x80;    
    } else {
        if (ds->gps[0].game_type == GAME_INPUT_TYPE_GAMEPAD) {
            if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_EAST)) {
                ds->game_switch_0 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_NORTH)) {
                ds->game_switch_0 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                ds->game_switch_0 = 1;
            } else {
                ds->game_switch_0 = 0;
            }
        } else {
            ds->game_switch_0 = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
        }
        SDL_Keymod mod = SDL_GetModState();
        if (SDL_GetModState() & KEYMOD_OPENAPPLE) { // TODO: restrict to Apple IIe
            ds->game_switch_0 = 1;
        }
        return ds->game_switch_0 ? 0x80 : 0x00;
    }
}

uint8_t read_game_switch_1(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);

    if ((ds->joystick_mode == JOYSTICK_MODE_JOYPORT_ATARI) && (cpu->cycles > ds->joyport_activate)) {
        bool val = false;

        annunciator_state_t *anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
        bool anc_1 = anc_d->annunciators[1];
        if (anc_1) { // up-1 
            val = SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        } else { // left-1
            val = SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        }
        if (SDL_GetModState() & KEYMOD_CLOSEDAPPLE) { // TODO: restrict to Apple IIe
            val = true;
        }
        return val ? 0x00 : 0x80;
    } else {
        if (ds->gps[0].game_type == GAME_INPUT_TYPE_GAMEPAD) {
            if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
                ds->game_switch_1 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_WEST)) {
                ds->game_switch_1 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
                ds->game_switch_1 = 1;
            } else {
                ds->game_switch_1 = 0;
            }
        } else {
            ds->game_switch_1 = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
        }
        if (SDL_GetModState() & KEYMOD_CLOSEDAPPLE) { // TODO: restrict to Apple IIe
            ds->game_switch_1 = 1;
        }
        return ds->game_switch_1 ? 0x80 : 0x00;
    }
}

uint8_t read_game_switch_2(void *context, uint16_t address) {
    cpu_state *cpu = (cpu_state *)context;
    gamec_state_t *ds = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);

    if ((ds->joystick_mode == JOYSTICK_MODE_JOYPORT_ATARI) && (cpu->cycles > ds->joyport_activate)) {
        bool val = false;

        annunciator_state_t *anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
        bool anc_1 = anc_d->annunciators[1];
        if (anc_1) { // down-1 
            val = SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        } else { // right-1
            val = SDL_GetGamepadButton(ds->gps[0].gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        }
        return val ? 0x00 : 0x80;
    } else {
        if (ds->gps[1].game_type == GAME_INPUT_TYPE_GAMEPAD) {
            if (SDL_GetGamepadButton(ds->gps[1].gamepad, SDL_GAMEPAD_BUTTON_EAST)) {
                ds->game_switch_2 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[1].gamepad, SDL_GAMEPAD_BUTTON_NORTH)) {
                ds->game_switch_2 = 1;
            } else if (SDL_GetGamepadButton(ds->gps[1].gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                ds->game_switch_2 = 1;
            } else {
                ds->game_switch_2 = 0;
            }
        } else {
            ds->game_switch_2 = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
        }
        return ds->game_switch_2 ? 0x80 : 0x00;
    }
}

/**
 * SDL3 handles joysticks / other controllers, by generating "add" events. I.e., 
 * once the controller comes online and connects into the engine, there will be
 * a "add" event.
 * 
 * We can use this to detect when a joystick is connected, and open it.
 * Same for Joystick removal in reverse.
 * 
 * So when a JS comes online, we automatically switch logic to use the joystick;
 * and when it goes offline, we switch back to mouse.
 * TODO: detect a 2nd joystick and wire that into game inputs 2/3 also.
 * Support up to 2 gamepads. ensure we load gamepad1 first, and then gamepad2.
 */

bool recompute_gamepads(gamec_state_t *gp_d) {
    int gpcount;
    SDL_JoystickID *gpid = SDL_GetGamepads(&gpcount);
    //printf("Gamepad count: %d\n", gpcount);

    for (int i = 0; i < gpcount; i++) {
        const char *nm = SDL_GetGamepadNameForID(gpid[i]);
        printf("Gamepad ID: %d, name: %s\n", gpid[i], nm);
    }
    if (gpcount > 0) {
        gp_d->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Gamepad connected"));
    } else {
        gp_d->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "No gamepads connected, joystick emulation via mouse"));
    }

    // if there is only one, connect to it.
    if (gpcount == 0) {
        gp_d->gps[0].game_type = GAME_INPUT_TYPE_MOUSE;
        gp_d->gps[1].game_type = GAME_INPUT_TYPE_MOUSE;
        gp_d->gps[0].gamepad = nullptr;
        gp_d->gps[1].gamepad = nullptr;
        gp_d->gps[0].id = -1;
        gp_d->gps[1].id = -1;
    }
    if (gpcount == 1) {
        gp_d->gps[0].gamepad = SDL_OpenGamepad(gpid[0]);
        if (gp_d->gps[0].gamepad== NULL) {
            printf("Error opening gamepad: %s\n", SDL_GetError());
            return false;
        }
        gp_d->gps[0].id = gpid[0];
        gp_d->gps[0].game_type = GAME_INPUT_TYPE_GAMEPAD;

        // zero out second gamepad info, because there is only one.
        gp_d->gps[1].game_type = GAME_INPUT_TYPE_MOUSE;
        gp_d->gps[1].gamepad = nullptr;
        gp_d->gps[1].id = -1;
    }
    if (gpcount >= 2) {
        gp_d->gps[1].gamepad = SDL_OpenGamepad(gpid[1]);
        if (gp_d->gps[1].gamepad== NULL) {
            printf("Error opening gamepad: %s\n", SDL_GetError());
            SDL_Quit();
            return false;
        }
        gp_d->gps[1].id = gpid[1];
        gp_d->gps[1].game_type = GAME_INPUT_TYPE_GAMEPAD;
    }
    return true;
}

#if 0
bool add_gamepad(cpu_state *cpu, const SDL_Event &event) {
    gamec_state_t *gp_d = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    printf("add_gamepad: %d\n", event.type);

    return recompute_gamepads(gp_d);
}

bool remove_gamepad(cpu_state *cpu, const SDL_Event &event) {
    gamec_state_t *gp_d = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    printf("remove_gamepad: %d\n", event.type);

    return recompute_gamepads(gp_d);
}
#endif

bool add_gamepad(gamec_state_t *gp_d, const SDL_Event &event) {
    //gamec_state_t *gp_d = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    printf("add_gamepad: %d\n", event.type);

    return recompute_gamepads(gp_d);
}

bool remove_gamepad(gamec_state_t *gp_d, const SDL_Event &event) {
    //gamec_state_t *gp_d = (gamec_state_t *)get_module_state(cpu, MODULE_GAMECONTROLLER);
    printf("remove_gamepad: %d\n", event.type);

    return recompute_gamepads(gp_d);
}

void set_joystick_mode(gamec_state_t *gp_d, joystick_mode_t mode) {
    gp_d->joystick_mode = mode;
}

void toggle_joystick_mode(gamec_state_t *gp_d) {
    const char *mode_names[] = {
        "Apple Joystick",
        "Atari Joysticks (Joyport)",
        "Apple Joysticks (Joyport)"
    };
    
    if (gp_d->joystick_mode == JOYSTICK_MODE_JOYPORT_ATARI) {
        gp_d->joystick_mode = JOYSTICK_MODE_APPLE;
    } else {
        gp_d->joystick_mode = JOYSTICK_MODE_JOYPORT_ATARI;
    }
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Joystick mode set to %s", mode_names[gp_d->joystick_mode]);
    gp_d->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, buffer));
}

void init_mb_game_controller(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    // alloc and init display state
    gamec_state_t *ds = new gamec_state_t;
    ds->event_queue = computer->event_queue;
    ds->mmu = computer->mmu;
    
    ds->game_switch_0 = 0;
    ds->game_switch_1 = 0;
    ds->game_switch_2 = 0;
    ds->game_input_trigger_0 = 0;
    ds->game_input_trigger_1 = 0;
    ds->game_input_trigger_2 = 0;
    ds->game_input_trigger_3 = 0;
    ds->mouse_wheel_pos_0 = 0;
    ds->paddle_flip_01 = 0; // to swap the mouse axes so Y is paddle 0
    ds->gps[0].game_type = GAME_INPUT_TYPE_MOUSE;
    ds->gps[1].game_type = GAME_INPUT_TYPE_MOUSE;
    ds->gps[0].gamepad = nullptr;
    ds->gps[1].gamepad = nullptr;
    ds->gps[0].id = -1;
    ds->gps[1].id = -1;

    // set in CPU so we can reference later
    set_module_state(cpu, MODULE_GAMECONTROLLER, ds);

    if (DEBUG(DEBUG_GAME)) fprintf(stdout, "Initializing game controller\n");

    // register the I/O ports
    computer->mmu->set_C0XX_read_handler(GAME_ANALOG_0, { read_game_input_0, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_ANALOG_1, { read_game_input_1, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_ANALOG_2, { read_game_input_2, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_ANALOG_3, { read_game_input_3, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_SWITCH_0, { read_game_switch_0, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_SWITCH_1, { read_game_switch_1, cpu });
    computer->mmu->set_C0XX_read_handler(GAME_SWITCH_2, { read_game_switch_2, cpu }); 

    // the reset strobe apparently responds at 0xC070-7F.
    for (int i = 0x00; i < 0x10; i++) {
        computer->mmu->set_C0XX_read_handler(GAME_ANALOG_RESET + i, { strobe_game_inputs, cpu });
        computer->mmu->set_C0XX_write_handler(GAME_ANALOG_RESET + i, { strobe_game_inputs_w, cpu });
    }

    // we need to compute on init! Otherwise we will only catch changes after boot.
    recompute_gamepads(ds);

    computer->dispatch->registerHandler(SDL_EVENT_GAMEPAD_REMOVED, [ds](const SDL_Event &event) {
        remove_gamepad(ds, event);
        return true;
    });
    computer->dispatch->registerHandler(SDL_EVENT_GAMEPAD_ADDED, [ds](const SDL_Event &event) {
        add_gamepad(ds, event);
        return true;
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_WHEEL, [ds](const SDL_Event &event) {
        handle_mouse_wheel(ds, event);
        return true;
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [ds, computer](const SDL_Event &event) {
        int key = event.key.key;
        if (key == SDLK_F6) {
            toggle_joystick_mode(ds);
            return true;
        }
        return false;
    });

    computer->register_reset_handler(
        [ds,cpu]() {
            ds->joyport_activate = cpu->cycles + 100000; // 100ms
            return true;
        });
}

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

#pragma once

#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"

// TODO: get these from the display subsystem. Maybe even query the window.
#define WINDOW_WIDTH 1120
#define WINDOW_HEIGHT 768

#define GAME_STROBE  0xC040
#define GAME_SWITCH_0 0xC061
#define GAME_SWITCH_1 0xC062
#define GAME_SWITCH_2 0xC063
#define GAME_ANALOG_0 0xC064
#define GAME_ANALOG_1 0xC065
#define GAME_ANALOG_2 0xC066
#define GAME_ANALOG_3 0xC067
#define GAME_ANALOG_RESET 0xC070

#define GAME_AN0_OFF 0xC058
#define GAME_AN0_ON  0xC059
#define GAME_AN1_OFF 0xC05A
#define GAME_AN1_ON  0xC05B
#define GAME_AN2_OFF 0xC05C
#define GAME_AN2_ON  0xC05D
#define GAME_AN3_OFF 0xC05E
#define GAME_AN3_ON  0xC05F

#define MAX_GAMEPAD_COUNT 2

typedef enum game_input_type {
    GAME_INPUT_TYPE_NONE = 0,
    GAME_INPUT_TYPE_MOUSE = 1,
    GAME_INPUT_TYPE_MOUSEWHEEL,
    GAME_INPUT_TYPE_GAMEPAD,
    GAME_INPUT_TYPE_JOYSTICK,
    NUM_GAME_INPUT_TYPES
} game_input_t;

typedef struct gamepad_state {
    game_input_t game_type;
    SDL_JoystickID id;
    SDL_Gamepad *gamepad;    
} gamepad_state_t;

typedef enum joystick_mode {
    JOYSTICK_MODE_APPLE = 0,
    JOYSTICK_MODE_JOYPORT_ATARI,
    JOYSTICK_MODE_JOYPORT_APPLE,
    NUM_JOYSTICK_MODES
} joystick_mode_t;

typedef struct gamec_state_t {
    joystick_mode_t joystick_mode = JOYSTICK_MODE_APPLE;
    uint64_t joyport_activate = 0;

    int game_switch_0;
    int game_switch_1;
    int game_switch_2;

    uint64_t game_input_trigger_0;
    uint64_t game_input_trigger_1;
    uint64_t game_input_trigger_2;
    uint64_t game_input_trigger_3;

    int mouse_wheel_pos_0; // only one wheel per mouse.
    int paddle_flip_01;

    gamepad_state gps[MAX_GAMEPAD_COUNT];   

    EventQueue *event_queue;
    MMU_II *mmu;
} gamec_state_t;

void init_mb_game_controller(computer_t *computer, SlotType_t slot);
bool add_gamepad(cpu_state *cpu, const SDL_Event &event);
bool remove_gamepad(cpu_state *cpu, const SDL_Event &event);

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

//#include "cpu.hpp"

struct cpu_state; // forward declaration

enum clock_set_t {
    CLOCK_SET_US = 0,
    CLOCK_SET_PAL,
    NUM_CLOCK_SETS
};

typedef enum {
    CLOCK_FREE_RUN = 0,
    CLOCK_1_024MHZ,
    CLOCK_2_8MHZ,
    CLOCK_7_159MHZ,
    CLOCK_14_3MHZ,
    NUM_CLOCK_MODES,
    INVALID_CLOCK_MODE = -1
} clock_mode_t;

typedef struct {
    uint64_t hz_rate;
    //uint64_t cycle_duration_ns;
    uint64_t c14M_per_second;
    uint64_t cycles_per_frame;
    uint64_t c_14M_per_cpu_cycle;
    uint64_t extra_per_scanline;
    uint64_t cycles_per_scanline;

    uint64_t c14M_per_frame;       // either 238944 (US) or 284544 (PAL)
    uint64_t us_per_frame_even;
    uint64_t us_per_frame_odd;
    uint64_t eff_cpu_rate;

} clock_mode_info_t;

//extern clock_mode_info_t clock_mode_info[NUM_CLOCK_MODES];
extern clock_mode_info_t *system_clock_mode_info;
clock_mode_info_t *get_clock_line(cpu_state *cpu);
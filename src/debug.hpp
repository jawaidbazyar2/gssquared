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

#include <cstdint>

// Values can be 0 - no debug, 1 - instruction decode, 2- detailed decode.
extern uint64_t debug_level;

//#define DEBUG_OPCODE 0x0001
//#define DEBUG_OPERAND 0x0002
//#define DEBUG_REGISTERS 0x0004
#define DEBUG_KEYBOARD          0x0010
#define DEBUG_DISPLAY           0x0020
#define DEBUG_CLOCK             0x0040
#define DEBUG_TIMEDRUN          0x0080
#define DEBUG_GUI               0x0100
#define DEBUG_SPEAKER           0x0200
#define DEBUG_HGR               0x0400
#define DEBUG_DISKII            0x0800
#define DEBUG_LANGCARD          0x1000
#define DEBUG_GAME              0x2000
#define DEBUG_MEMEXP            0x4000
#define DEBUG_MOUNTS            0x8000
#define DEBUG_DISKII_FORMAT    0x10000
#define DEBUG_THUNDERCLOCK     0x20000
#define DEBUG_PD_BLOCK         0x40000
#define DEBUG_PARALLEL         0x80000
#define DEBUG_VIDEX           0x100000
#define DEBUG_EVENT_TIMER     0x200000
#define DEBUG_MOCKINGBOARD    0x400000
#define DEBUG_MOUSE           0x800000
#define DEBUG_ANY           0xFFFFFFFF
#define DEBUG_BOOT_FLAG     0

#define DEBUG(flag) (debug_level & flag)

#include <cstdint>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "cpu.hpp"

/* void debug_print_cpu(cpu_t *cpu);
 */
void debug_dump_memory(cpu_state *cpu, uint32_t start, uint32_t end);
/* void debug_print_registers(cpu_t *cpu);
void debug_print_flags(cpu_t *cpu);
 */

void debug_set_level(uint64_t level);

std::string int_to_hex(uint16_t value) ;
std::string int_to_hex(uint8_t value) ;
inline void print_hex(uint8_t value) ;
void debug_dump_pointer(uint8_t *start, uint16_t length);
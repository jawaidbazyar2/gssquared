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

#include <iostream>
#include <sstream>
#include <iomanip>

#include "gs2.hpp"
#include "debug.hpp"

uint64_t debug_level = DEBUG_BOOT_FLAG ;

inline void print_hex(uint8_t value) {
    std::cout << std::hex << std::uppercase << static_cast<int>(value) << std::dec;
}

std::string int_to_hex(uint8_t value) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return ss.str();
}

std::string int_to_hex(uint16_t value) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(value);
    return ss.str();
}

void debug_dump_memory(cpu_state *cpu, uint32_t start, uint32_t end) {
    for (uint32_t i = start; i <= end; i++) {
        std::cout << int_to_hex((uint16_t)i) << ": " << int_to_hex(cpu->mmu->read(i)) << " ";
        if ((i % 16) == 0xF) std::cout << std::endl;
    }
    std::cout << std::endl;
}

void debug_set_level(uint64_t level) {
    debug_level = level;
};

void debug_dump_pointer(uint8_t *start, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        if (i % 32 == 0) std::cout << int_to_hex((uint16_t)i) << ": ";
        std::cout << int_to_hex(*(start+i)) << " ";
        if ((i % 32) == 0x1F) std::cout << std::endl;
    }
    std::cout << std::endl;
}

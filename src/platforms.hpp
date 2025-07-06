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
#include <stddef.h>
#include "clock.hpp"
#include "util/ResourceFile.hpp"
#include "cpus/processor_type.hpp"

typedef enum PlatformId_t {
    PLATFORM_APPLE_II = 0,
    PLATFORM_APPLE_II_PLUS,
    PLATFORM_APPLE_IIE,
    PLATFORM_APPLE_IIE_ENHANCED,
    PLATFORM_END
} PlatformId_t;

typedef enum MMU_Type_t {
    MMU_MMU_II,
    MMU_MMU_IIE,
} MMU_Type_t;

struct platform_info {
    const PlatformId_t id;       
    const char* name;           // Human readable name
    const char* rom_dir;        // Directory under roms/
    uint16_t rom_base_addr;    // Base address for the roms
    const processor_type processor_type;   // processor type
    const clock_mode_t default_clock_mode; // default clock mode for this platform at startup.
    const MMU_Type_t mmu_type;
};

typedef uint8_t char_rom_t[256 * 8];
typedef uint8_t main_rom_t[0x3000];

struct rom_data {
    uint16_t main_base_addr;
    ResourceFile *main_rom_file;
    ResourceFile *char_rom_file;
    main_rom_t *main_rom_data;
    char_rom_t *char_rom_data;

/*     uint8_t* main_rom;
    size_t main_size;
    uint8_t* char_rom;
    size_t char_size; */
};

extern  int num_platforms;
platform_info* get_platform(int index);
platform_info* find_platform_by_dir(const char* dir);
rom_data* load_platform_roms(platform_info *platform);
void free_platform_roms(rom_data* roms); 
void print_platform_info(platform_info *platform);
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

#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "util/ResourceFile.hpp"

// Parameters

#define VIDEX_SCREEN_MEMORY 2048
#define VIDEX_CHARSET_SIZE 2048
#define VIDEX_SCREEN_WIDTH 640
#define VIDEX_SCREEN_HEIGHT 216

// Writes

#define VIDEX_REG_ADDR 0x0000
#define VIDEX_REG_VAL 0x0001

// Reads

enum videx_page_t {
    VIDEX_PAGE0 = 0x0000,
    VIDEX_PAGE1 = 0x0001,
    VIDEX_PAGE2 = 0x0002,
    VIDEX_PAGE3 = 0x0003
};

#define VIDEX_PAGE_SELECT_0 0x0
#define VIDEX_PAGE_SELECT_1 0x4
#define VIDEX_PAGE_SELECT_2 0x8
#define VIDEX_PAGE_SELECT_3 0xC

// Videx Registers

enum videx_register_t {
    R0_HORZ_TOTAL = 0x00,
    R1_HORZ_DISPLAYED = 0x01,
    R2_HORZ_SYNC_POS = 0x02,
    R3_HORZ_SYNC_WIDTH = 0x03,
    R4_VERT_TOTAL = 0x04,
    R5_VERT_ADJUST = 0x05,
    R6_VERT_DISPLAYED = 0x06,
    R7_VERT_SYNC_POS = 0x07,
    R8_INTERLACE_MODE = 0x08,
    R9_MAX_SCAN_LINE = 0x09,
    R10_CURSOR_START = 0x0A,
    R11_CURSOR_END = 0x0B,
    R12_START_ADDR_HI = 0x0C,
    R13_START_ADDR_LO = 0x0D,
    R14_CURSOR_HI = 0x0E,
    R15_CURSOR_LO = 0x0F,
    R16_LIGHT_PEN_HI = 0x10,
    R17_LIGHT_PEN_LO = 0x11,
    VIDEX_REG_COUNT
};

enum videx_char_set_t {
    VIDEX_CHAR_SET_NORMAL = 0,
    VIDEX_CHAR_SET_INVERSE = 1,
    VIDEX_CHAR_SET_COUNT
};

struct videx_char_set_file_t {
    const char *name;
    const char *filename;
};

const videx_char_set_file_t videx_char_roms[VIDEX_CHAR_SET_COUNT] = {
    { "Normal", "roms/cards/videx/videx-normal.bin" },
    { "Inverse", "roms/cards/videx/videx-inverse.bin" }
};

typedef struct videx_data: public SlotData {
    video_system_t *video_system = nullptr;
    MMU_II *mmu = nullptr;
    cpu_state *cpu = nullptr;
    
    SDL_Texture *videx_texture = nullptr;
    uint8_t *buffer = nullptr; // 640x216x4

    uint8_t video_enabled = 0;

    bool cursor_blink_status = false;
    uint8_t cursor_blink_count = 0;

    uint8_t reg[VIDEX_REG_COUNT];
    uint8_t *screen_memory;

    uint8_t selected_register;
    videx_page_t selected_page; // 0 - 3

    bool line_dirty[24] = {false};

    ResourceFile *rom;
    
    ResourceFile *char_sets[VIDEX_CHAR_SET_COUNT];
    uint8_t *char_memory = nullptr;
    uint8_t *char_set = nullptr;
    uint8_t *alt_char_set = nullptr;
} videx_data;

void init_slot_videx(computer_t *computer, SlotType_t slot);
void deinit_slot_videx(videx_data *videx_d);
void videx_set_line_dirty_by_addr(videx_data * videx_d, uint16_t addr);
void videx_set_line_dirty(videx_data * videx_d, int line);
void update_videx_screen_memory(cpu_state *cpu, videx_data * videx_d);
void map_rom_videx(cpu_state *cpu, SlotType_t slot);
void update_display_videx(cpu_state *cpu);
void videx_render_line(cpu_state *cpu, videx_data * videx_d, int y);

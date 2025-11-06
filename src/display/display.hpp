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
#include "platforms.hpp"
#include "videosystem.hpp"

#define SCALE_X 2
#define SCALE_Y 4
#define BASE_WIDTH 560
#define BASE_HEIGHT 192
//#define BORDER_WIDTH 30
//#define BORDER_HEIGHT 20

#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
#include "devices/displaypp/render/GSRGB560.hpp"
#include "devices/displaypp/generate/AppleII.cpp"

class VideoScanGenerator ;
class CharRom;

// Graphics vs Text, C050 / C051
typedef enum {
    TEXT_MODE = 0,
    GRAPHICS_MODE = 1,
} display_mode_t;

// Full screen vs split screen, C052 / C053
typedef enum {
    FULL_SCREEN = 0,
    SPLIT_SCREEN = 1,
} display_split_mode_t;

// Lo-res vs Hi-res, C056 / C057
typedef enum {
    LORES_MODE = 0,
    HIRES_MODE = 1,
} display_graphics_mode_t;

typedef enum {
    LM_TEXT_MODE    = 0,
    LM_LORES_MODE   = 1,
    LM_HIRES_MODE   = 2,
    LM_TEXT80_MODE  = 3,
    LM_LORES80_MODE = 4,
    LM_HIRES80_MODE = 5,
    LM_HIRES_MODE_NOSHIFT = 6,
} line_mode_t;

typedef uint16_t display_page_table_t[24] ;

typedef struct display_page_t {
    uint16_t text_page_start;
    uint16_t text_page_end;
    display_page_table_t text_page_table;
    uint16_t hgr_page_start;
    uint16_t hgr_page_end;
    display_page_table_t hgr_page_table;
} display_page_t;

typedef enum {
    DISPLAY_PAGE_1 = 0,
    DISPLAY_PAGE_2,
    NUM_DISPLAY_PAGES
} display_page_number_t;

typedef enum {
    VS_II = 0,
    VS_IIE,
    VS_IIGS,
    NUM_VIDEO_SCANNERS

} video_scanner_t;

#define B_TOP 0
#define B_CEN 1
#define B_BOT 2
#define B_LT 0
#define B_RT 2

struct border_rect_t {
    SDL_FRect src;
    SDL_FRect dst;
};

typedef class display_state_t {

public:
    display_state_t();
    ~display_state_t();

    SDL_Texture* screenTexture;

    display_mode_t display_mode;
    display_split_mode_t display_split_mode;
    display_graphics_mode_t display_graphics_mode;
    display_page_number_t display_page_num;
    display_page_t *display_page_table;
    bool f_altcharset = false;
    bool f_80col = false;

    bool flash_state;
    int flash_counter;
    bool f_double_graphics = true;

    uint32_t dirty_line[24];
    line_mode_t line_mode[24] = {LM_TEXT_MODE}; // 0 = TEXT, 1 = LO RES GRAPHICS, 2 = HI RES GRAPHICS

    uint8_t *buffer = nullptr;
    EventQueue *event_queue;
    video_system_t *video_system;
    MMU_II *mmu;
    computer_t *computer;
    video_scanner_t video_scanner_type = VS_II;
    VideoScannerII *video_scanner = nullptr; // if set, use this instead of default video generation.
    VideoScanGenerator *vsg = nullptr;
    bool framebased = true;
    CharRom *char_rom = nullptr;

    AppleII_Display *a2_display;
    Frame560     *frame_bits;
    Frame560RGBA *frame_rgba;
    Monochrome560 mon_mono;
    NTSC560 mon_ntsc;
    GSRGB560 mon_rgb;
    MessageBus *mbus;

    // IIGS specific
    FrameBorder *fr_border = nullptr;
    Frame640 *fr_shr = nullptr;
    uint8_t new_video = 0x01;
    uint8_t text_color = 0x0F0;
    uint8_t border_color = 0x00;
    border_rect_t ii_borders[3][3]; // [y][x]
    border_rect_t shr_borders[3][3]; // [y][x]
    
} display_state_t;

void txt_memory_write(uint16_t , uint8_t );
void update_flash_state(cpu_state *cpu);
void init_mb_device_display(computer_t *computer, SlotType_t slot);

void display_dump_hires_page(MMU_II *mmu, int page);
void display_dump_text_page(MMU_II *mmu, int page);

void display_engine_get_buffer(computer_t *computer, uint8_t *buffer, uint32_t *width, uint32_t *height);
void display_update_video_scanner(display_state_t *ds, cpu_state *cpu);

uint8_t txt_bus_read_C050(void *context, uint16_t address);
uint8_t txt_bus_read_C051(void *context, uint16_t address);
uint8_t txt_bus_read_C052(void *context, uint16_t address);
uint8_t txt_bus_read_C053(void *context, uint16_t address);
uint8_t txt_bus_read_C054(void *context, uint16_t address);
uint8_t txt_bus_read_C055(void *context, uint16_t address);
uint8_t txt_bus_read_C056(void *context, uint16_t address);
uint8_t txt_bus_read_C057(void *context, uint16_t address);
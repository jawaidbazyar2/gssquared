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
#include "videosystem.hpp"

#define SCALE_X 2
#define SCALE_Y 4
#define BASE_WIDTH 560
#define BASE_HEIGHT 192

#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
#include "devices/displaypp/render/GSRGB560.hpp"
#include "devices/displaypp/generate/AppleII.cpp"
#include "devices/displaypp/VideoScanner.hpp"

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

#define B_TOP 0
#define B_CEN 1
#define B_BOT 2
#define B_LT 0
#define B_RT 2

struct border_rect_t {
    SDL_FRect src;
    SDL_FRect dst;
};
typedef border_rect_t border_rect_array_t[3][3];

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

    union {
        struct {
            uint8_t f_enable_mouse : 1;
            uint8_t f_enable_move : 1;
            uint8_t f_enable_switch : 1;
            uint8_t f_vbl_enable : 1;
            uint8_t f_quartersec_enable : 1;
            uint8_t f_inten_reserved : 3;
        };
        uint8_t f_INTEN = 0x00; // interrupt enable flag
    };

    union {
        struct {
            uint8_t f_extint_enable : 1;
            uint8_t f_scanline_enable : 1;
            uint8_t f_onesec_enable : 1;
            uint8_t f_reserved : 1;
            uint8_t f_extint_asserted : 1;
            uint8_t f_scanline_asserted : 1;
            uint8_t f_onesec_asserted : 1;
            uint8_t f_vgcint_asserted : 1;
        };
        uint8_t f_VGCINT = 0x00; // VGC interrupt status flag
    };

    /* MegaII interrupt status flags */
    union {
        struct {
            uint8_t f_system_irq_asserted : 1;
            uint8_t f_megaii_move_asserted : 1;    // unused 
            uint8_t f_megaii_switch_asserted : 1;  // unused
            uint8_t f_vblint_asserted : 1;
            uint8_t f_quartersec_asserted : 1;
            uint8_t f_an3_status : 1;
            uint8_t f_btn_last_status : 1;   // unused
            uint8_t f_btn_down : 1;          // unused
        };
        uint8_t f_INTFLAG = 0x00;
    };

    uint16_t onesec_counter = 0;
    uint16_t quartersec_counter = 0;
    
    uint8_t f_langsel = 0x00;

    uint32_t dirty_line[24];
    line_mode_t line_mode[24] = {LM_TEXT_MODE}; // 0 = TEXT, 1 = LO RES GRAPHICS, 2 = HI RES GRAPHICS

    uint8_t *buffer = nullptr;
    EventQueue *event_queue;
    video_system_t *video_system;
    MMU_II *mmu;
    computer_t *computer;
    NClockII *clock = nullptr;
    InterruptController *irq_control = nullptr;

    video_scanner_t video_scanner_type = Scanner_AppleII;
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
    SDL_Texture *stage2 = nullptr;

    // IIGS specific
    FrameBorder *fr_border = nullptr;
    Frame640 *fr_shr = nullptr;
    uint8_t new_video = 0x01;
    uint8_t text_color = 0x0F0;
    uint8_t border_color = 0x00;

    border_rect_array_t ii_borders; // [y][x]
    border_rect_array_t shr_borders; // [y][x]

    // these are pretty much static.
    SDL_FRect ii_frame_src = { 0.0, 0.0, 560.0f+42+49, 232.0 };
    SDL_FRect gs_ii_frame_src = { 0.0, 0.0, 651.0, 232.0}; // dst is null - "scale to whatever" 651 is weird but that's the number.. 
    SDL_FRect gs_shr_frame_src = { 0.0, 0.0, 744.0, 232.0}; // dst is null - "scale to whatever"
    SDL_FRect frame_dst = { 0.0, 0.0, 651.0, 232.0};
} display_state_t;

//void txt_memory_write(uint16_t , uint8_t );
//void update_flash_state(cpu_state *cpu);
void init_mb_device_display(computer_t *computer, SlotType_t slot);

void display_dump_hires_page(MMU_II *mmu, int page);
void display_dump_text_page(MMU_II *mmu, int page);

void display_engine_get_buffer(computer_t *computer, uint8_t *buffer, uint32_t *width, uint32_t *height);
void display_update_video_scanner(display_state_t *ds, cpu_state *cpu);

uint8_t txt_bus_read_C050(void *context, uint32_t address);
uint8_t txt_bus_read_C051(void *context, uint32_t address);
uint8_t txt_bus_read_C052(void *context, uint32_t address);
uint8_t txt_bus_read_C053(void *context, uint32_t address);
uint8_t txt_bus_read_C054(void *context, uint32_t address);
uint8_t txt_bus_read_C055(void *context, uint32_t address);
uint8_t txt_bus_read_C056(void *context, uint32_t address);
uint8_t txt_bus_read_C057(void *context, uint32_t address);

void update_vgc_interrupt(display_state_t *ds, bool assert_now);
void update_megaii_interrupt(display_state_t *ds, bool assert_now);
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
#include <unistd.h>
#include <cstdlib>

#include "debug.hpp"
#include "display.hpp"
#include "platforms.hpp"
//#include "displayng.hpp"


void update_flash_state(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    display_page_t *display_page = ds->display_page_table;
    uint16_t *TEXT_PAGE_TABLE = display_page->text_page_table;
    
       // 2 times per second (every 30 frames), the state of flashing characters (those matching 0b01xxxxxx) must be reversed.
    // according to a web site it's every 27.5 frames. 
    if (++(ds->flash_counter) < 14) {
        return;
    }
    ds->flash_counter = 0;
    ds->flash_state = !ds->flash_state;
    ds->a2_display->set_flash_state(ds->flash_state);
 return; // well we still want to update flash state.
    uint8_t *ram = ds->mmu->get_memory_base();

    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 40; x++) {
            uint16_t addr = TEXT_PAGE_TABLE[y] + x;

            uint8_t character = ram[addr];
            if ((character & 0b11000000) == 0x40) {
                ds->dirty_line[y] = 1;
                break;                           // stop after we find any flash char on a line.
            }
            if (ds->f_80col) { // if iie and 80 col enabled, check for flash in the other page.
                character = ram[addr + 0x1'0000];
                if ((character & 0b11000000) == 0x40) {
                    ds->dirty_line[y] = 1;
                    break;                           // stop after we find any flash char on a line.
                }
            }
        }
    }
}

/**
 * having written a character to the display memory, we need to update the display.
 * this function identifies the line of the update and marks that line as dirty.
 * Later, update_display() will be called and it will render the dirty lines.
 */
void txt_memory_write(void *context, uint16_t address, uint8_t value) {
    cpu_state *cpu = (cpu_state *)context;
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    uint16_t TEXT_PAGE_START = ds->display_page_table->text_page_start;
    uint16_t TEXT_PAGE_END = ds->display_page_table->text_page_end;

    // Strict bounds checking for text page 1
    if (address < TEXT_PAGE_START || address > TEXT_PAGE_END) {
        return;
    }

    // Convert text memory address to screen coordinates
    uint16_t addr_rel = address - TEXT_PAGE_START;
    
    // Each superrow is 128 bytes apart
    uint8_t superrow = addr_rel >> 7;      // Divide by 128 to get 0 or 1
    uint8_t superoffset = addr_rel & 0x7F; // Get offset within the 128-byte block
    
    uint8_t subrow = superoffset / 40;     // Each row is 40 characters
    uint8_t charoffset = superoffset % 40;
    
    // Calculate final screen position
    uint8_t y_loc = (subrow * 8) + superrow; // Each superrow contains 8 rows
    uint8_t x_loc = charoffset;

    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Address: $%04X -> dirty line y:%d (value: $%02X)\n", 
           address, y_loc, value); // Debug output

    // Extra bounds verification
    if (x_loc >= 40 || y_loc >= 24) {
        if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Invalid coordinates calculated: x=%d, y=%d from addr=$%04X\n", 
               x_loc, y_loc, address);
        return;
    }

    if (ds->display_mode == GRAPHICS_MODE && ds->display_graphics_mode == HIRES_MODE && ds->display_split_mode == SPLIT_SCREEN) {
        // only update lines 21 - 24 if we're in split screen hires mode.
        if (y_loc < 20) return;
    }

    // update any line. Dirty logic is same for text and lores.
    ds->dirty_line[y_loc] = 1;
}

#if 0
uint8_t txt_bus_read(cpu_state *cpu, uint16_t address) {
    return 0;
}
#endif
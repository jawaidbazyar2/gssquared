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


#include "gs2.hpp"
#include "hgr_280x192.hpp"
#include "debug.hpp"
#include "display.hpp"

void hgr_memory_write(void *context, uint16_t address, uint8_t value) {
    cpu_state *cpu = (cpu_state *)context;
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    display_page_t *display_page = ds->display_page_table;
    uint16_t *HGR_PAGE_TABLE = display_page->hgr_page_table;
    uint16_t HGR_PAGE_START = display_page->hgr_page_start;
    uint16_t HGR_PAGE_END = display_page->hgr_page_end;

    // Skip unless we're in graphics mode.
    if (! (ds->display_mode == GRAPHICS_MODE && ds->display_graphics_mode == HIRES_MODE)) {
        return;
    }
    if (DEBUG(DEBUG_HGR)) fprintf(stdout, "hgr_memory_write address: %04X value: %02X\n", address, value);
    // Strict bounds checking for text page 1
    if (address < HGR_PAGE_START || address > HGR_PAGE_END) {
        return;
    }

    // Convert text memory address to screen coordinates
    uint16_t addr_rel = (address - HGR_PAGE_START) & 0x03FF;
    
    // Each superrow is 128 bytes apart
    uint8_t superrow = addr_rel >> 7;      // Divide by 128 to get 0 or 1
    uint8_t superoffset = addr_rel & 0x7F; // Get offset within the 128-byte block
    
    uint8_t subrow = superoffset / 40;     // Each row is 40 characters
    uint8_t charoffset = superoffset % 40;
    
    // Calculate final screen position
    uint8_t y_loc = (subrow * 8) + superrow; // Each superrow contains 8 rows
    uint8_t x_loc = charoffset;

    if (DEBUG(DEBUG_HGR)) fprintf(stdout, "Address: $%04X -> dirty line y:%d (value: $%02X)\n", 
           address, y_loc, value); // Debug output

    // Extra bounds verification
    if (x_loc >= 40 || y_loc >= 24) {
        if (DEBUG(DEBUG_HGR)) fprintf(stdout, "Invalid coordinates calculated: x=%d, y=%d from addr=$%04X\n", 
               x_loc, y_loc, address);
        return;
    }

    ds->dirty_line[y_loc] = 1;
}

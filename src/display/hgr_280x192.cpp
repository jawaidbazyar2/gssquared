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


#include "gs2.hpp"
#include "hgr_280x192.hpp"
#include "debug.hpp"
#include "display.hpp"

RGBA_t hgr_color_table[4] = { //    Cur   Col   D7
    RGBA_t::make(0xDC, 0x43, 0xE1, 0xFF), // purple              1     0     0
    RGBA_t::make(0x40, 0xDE, 0x00, 0xFF), // green               1     1     0
    RGBA_t::make(0x00, 0xAF, 0xFF, 0xFF), // blue,               1     0     1 
    RGBA_t::make(0xFF, 0x50, 0x00, 0xFF), // orange              1     1     1
};

void render_hgr_scanline(cpu_state *cpu, int y, void *pixels, int pitch) {
    
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);

    display_page_t *display_page = ds->display_page_table;
    uint16_t *HGR_PAGE_TABLE = display_page->hgr_page_table;
    uint8_t composite = true; // set to true if we are in composite mode.
    uint8_t *ram = ds->mmu->get_memory_base();

    int pitchoff = pitch / 4;

    uint8_t lastBitOn = 0; // set to 0 at start of scanline.
    uint16_t pixel_column = 0;

    RGBA_t* texturePixels = (RGBA_t *)pixels;
    for (int row = 0; row < 8; row++) {
        uint32_t base = row * pitchoff;

        // clear the entire line first.
        for (int x = 0; x < 560; x++) {
            texturePixels[base + x] = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
        }

        for (int x = 0; x < 40; x++) {
            int charoff = x * 14;

            uint16_t address = HGR_PAGE_TABLE[y] + (row * 0x0400) + x;
            //uint8_t character = raw_memory_read(cpu, address);
            //uint8_t character = cpu->mmu->read_raw(address);
            uint8_t character = ram[address];
            uint8_t ch_D7 = (character & 0x80) >> 7;

            // color choice is two variables: odd or even pixel column. And D7 bit.

            for (int bit = 0; bit < 14; bit+=2) {

                uint8_t thisBitOn = (character & 0x01);
                RGBA_t color_value = hgr_color_table[(ch_D7 << 1) | (pixel_column&1) ];
                RGBA_t pixel = thisBitOn ? color_value : RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
                RGBA_t black = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
                RGBA_t white = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF);
                
                if (ch_D7 == 0) { // no delay
                    texturePixels[base + charoff + bit ] = pixel;
                    texturePixels[base + charoff + bit + 1] = pixel;
                    if (composite && pixel_column >=2 && (pixel != black) && (texturePixels[base + charoff + bit - 4] == pixel)  ) {
                        texturePixels[base + charoff + bit - 2] = pixel;
                        texturePixels[base + charoff + bit - 1] = pixel;
                    }
                } else {
                    texturePixels[base + charoff + bit + 1 ] = pixel;
                    if ((charoff + bit + 2) < 560) texturePixels[base + charoff + bit + 2] = pixel; // add bounds check
                    if (composite && pixel_column >= 2 &&  (pixel != black) && (texturePixels[base + charoff + bit - 3] == pixel)) {
                        texturePixels[base + charoff + bit] = pixel;
                        texturePixels[base + charoff + bit - 1] = pixel;
                    }
                }

                if (thisBitOn && lastBitOn) { // if last bit was also on, then this is a "double wide white" pixel.
                    // two pixels on apple ii is four pixels here. First, remake prior to white.
                    if (pixel_column >= 1) texturePixels[base + charoff + bit - 2] = white;
                    if (pixel_column >= 1) texturePixels[base + charoff + bit - 1] = white;
                    // now this one as white.
                    texturePixels[base + charoff + bit] = white;
                    texturePixels[base + charoff + bit + 1] = white;
                }

                lastBitOn = (character & 0x01);
                pixel_column++;

                character >>= 1;
            }
        }
    }
}

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

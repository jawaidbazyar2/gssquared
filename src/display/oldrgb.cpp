
#if 0
void render_line_rgb(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lores_scanline(cpu, y, pixels, pitch);
    else if (mode == LM_HIRES_MODE) render_hgr_scanline(cpu, y, pixels, pitch);
    else render_text_scanline(cpu, y, pixels, pitch);

}
#endif

#if 0
void render_line_mono(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    RGBA_t mono_color_value ;

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lgrng_scanline(cpu, y);
    else if (mode == LM_HIRES_MODE) render_hgrng_scanline(cpu, y, (uint8_t *)pixels);
    else render_text_scanline_ng(cpu, y);

    mono_color_value = vs->get_mono_color();

    processAppleIIFrame_Mono(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8, mono_color_value);
}
#endif


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



#if 0
RGBA_t lores_color_table[16] = {
    {0x00, 0x00, 0x00, 0xFF},
    {0x8A, 0x21, 0x40, 0xFF},
    {0x3C, 0x22, 0xA5, 0xFF},
    {0xC8, 0x47, 0xE4, 0xFF},
    {0x07, 0x65, 0x3E, 0xFF},
    {0x7B, 0x7E, 0x80, 0xFF},
    {0x30, 0x8E, 0xF3, 0xFF},
    {0xB9, 0xA9, 0xFD, 0xFF},
    {0x3B, 0x51, 0x07, 0xFF},
    {0xC7, 0x70, 0x28, 0xFF},
    {0x7B, 0x7E, 0x80, 0xFF},
    {0xF3, 0x9A, 0xC2, 0xFF},
    {0x2F, 0xB8, 0x1F, 0xFF},
    {0xB9, 0xD0, 0x60, 0xFF},
    {0x6E, 0xE1, 0xC0, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}
    };

void render_lores_scanline(cpu_state *cpu, int y, void *pixels, int pitch) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    display_page_t *display_page = ds->display_page_table;
    uint16_t *TEXT_PAGE_TABLE = display_page->text_page_table;
    uint8_t *ram = ds->mmu->get_memory_base();

    for (int x = 0; x < 40; x++) {
        //uint8_t character = raw_memory_read(cpu, TEXT_PAGE_TABLE[y] + x);
        uint8_t character = ram[TEXT_PAGE_TABLE[y] + x];

        // look up color key for top and bottom block
        RGBA_t color_top = lores_color_table[character & 0x0F];
        RGBA_t color_bottom = lores_color_table[(character & 0xF0) >> 4];

        int pitchoff = pitch / 4;
        int charoff = x * 14;
        // Draw the character bitmap into the texture
        RGBA_t * texturePixels = (RGBA_t *)pixels;
        for (int row = 0; row < 4; row++) {
            uint32_t base = row * pitchoff;
            texturePixels[base + charoff ] = color_top;
            texturePixels[base + charoff + 1] = color_top;
            texturePixels[base + charoff + 2] = color_top;
            texturePixels[base + charoff + 3] = color_top;
            texturePixels[base + charoff + 4] = color_top;
            texturePixels[base + charoff + 5] = color_top;
            texturePixels[base + charoff + 6] = color_top;
            texturePixels[base + charoff + 7] = color_top;
            texturePixels[base + charoff + 8] = color_top;
            texturePixels[base + charoff + 9] = color_top;
            texturePixels[base + charoff + 10] = color_top;
            texturePixels[base + charoff + 11] = color_top;
            texturePixels[base + charoff + 12] = color_top;
            texturePixels[base + charoff + 13] = color_top;
        }

        for (int row = 4; row < 8; row++) {
            uint32_t base = row * pitchoff;
            texturePixels[base + charoff ] = color_bottom;
            texturePixels[base + charoff + 1] = color_bottom;
            texturePixels[base + charoff + 2] = color_bottom;
            texturePixels[base + charoff + 3] = color_bottom;
            texturePixels[base + charoff + 4] = color_bottom;
            texturePixels[base + charoff + 5] = color_bottom;
            texturePixels[base + charoff + 6] = color_bottom;
            texturePixels[base + charoff + 7] = color_bottom;
            texturePixels[base + charoff + 8] = color_bottom;
            texturePixels[base + charoff + 9] = color_bottom;
            texturePixels[base + charoff + 10] = color_bottom;
            texturePixels[base + charoff + 11] = color_bottom;
            texturePixels[base + charoff + 12] = color_bottom;
            texturePixels[base + charoff + 13] = color_bottom;
        }    
    }
}
#endif


/* old ntsc hooks code */


// anything we lock we have to completely replace.
#if 0
void render_line_ntsc(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;
    // this writes into texture - do not put border stuff here.

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lgrng_scanline(cpu, y);
    else if (mode == LM_HIRES_MODE) render_hgrng_scanline(cpu, y, (uint8_t *)pixels);
    else render_text_scanline_ng(cpu, y);

    RGBA_t mono_color_value = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF); // override mono color to white when we're in color mode

    if (ds->display_mode == TEXT_MODE) {
        processAppleIIFrame_Mono(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8, mono_color_value);
    } else {
        processAppleIIFrame_LUT(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8);
    }

}
#endif


#if 0

int textMapIndex[24] =
  {   // text page 1 line addresses
            0x0000,
            0x0080,
            0x0100,
            0x0180,
            0x0200,
            0x0280,
            0x0300,
            0x0380,

            0x0028,
            0x00A8,
            0x0128,
            0x01A8,
            0x0228,
            0x02A8,
            0x0328,
            0x03A8,

            0x0050,
            0x00D0,
            0x0150,
            0x01D0,
            0x0250,
            0x02D0,
            0x0350,
            0x03D0,
        };
#endif

#if 0

// Apple II+ Character Set (7x8 pixels)
// Characters 0x20 through 0x7F
#define CHAR_GLYPHS_COUNT 256
#define CHAR_GLYPHS_SIZE 8

// The Character ROM contains 256 entries (total of 2048 bytes).
// Each character is 8 bytes, each byte is 1 row of pixels.

uint32_t APPLE2_FONT_32[CHAR_GLYPHS_COUNT * CHAR_GLYPHS_SIZE * 8 ]; // 8 pixels per row.

uint8_t APPLE2_FONT_8[CHAR_GLYPHS_COUNT * CHAR_GLYPHS_SIZE * 8]; // 8 pixels per row.

/** pre-render the text mode font into a 32-bit bitmap - directly ready to shove into texture*/
/**
 * input: APPLE2_FONT 
 * output: APPLE2_FONT_32
*/

void pre_calculate_font (rom_data *rd) {
    // Draw the character bitmap into a memory array. (32 bit pixels, 7 sequential pixels per row, 8 rows per character)
    // each char will be 56 pixels or 224 bytes

    uint32_t fg_color = 0xFFFFFFFF;
    uint32_t bg_color = 0x00000000;

    uint32_t pos_index = 0;

    // TODO: temporary hack to handle IIe font. dpp handles it better.
    if (rd->char_rom_file->size() == 4096) {
        for (int row = 0; row < 256 * 8; row++) {
            uint8_t rowBits = (*rd->char_rom_data)[row];
            if (row < 0x40*8) rowBits ^= 0xFF; // invert pixels for 0-3F. 
            for (int col = 0; col < 7; col++) {
                bool pixel = rowBits & (1 << col);
                pixel = !pixel; // leave inversion for 0-3F. 
                uint32_t color = pixel ? fg_color : bg_color;
                APPLE2_FONT_32[pos_index] = color;
                APPLE2_FONT_8[pos_index] = pixel ? 0xFF : 0x00; // calculate both fonts
                pos_index++;
                if (pos_index > CHAR_GLYPHS_COUNT * CHAR_GLYPHS_SIZE * 8 ) {
                    fprintf(stderr, "pos_index out of bounds: %d\n", pos_index);
                    exit(1);
                }
            }
        }
    } else {

        for (int row = 0; row < 256 * 8; row++) {
            uint8_t rowBits = (*rd->char_rom_data)[row];
            for (int col = 0; col < 7; col++) {
                bool pixel = rowBits & (1 << (6 - col));
                uint32_t color = pixel ? fg_color : bg_color;
                APPLE2_FONT_32[pos_index] = color;
                APPLE2_FONT_8[pos_index] = pixel ? 0xFF : 0x00; // calculate both fonts
                pos_index++;
                if (pos_index > CHAR_GLYPHS_COUNT * CHAR_GLYPHS_SIZE * 8 ) {
                    fprintf(stderr, "pos_index out of bounds: %d\n", pos_index);
                    exit(1);
                }
            }
        }
    }
}
#endif



#if 0
/**
 * render group of 8 scanlines represented by one row of bytes of memory.
 */

void render_text_scanline(cpu_state *cpu, int y, void *pixels, int pitch) {

    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    uint32_t color_value;  
    uint16_t *TEXT_PAGE_TABLE = ds->display_page_table->text_page_table;
    uint8_t *ram = ds->mmu->get_memory_base();

    color_value = 0xFFFFFFFF;
    
    for (int x = 0; x < 40; x++) {

        uint8_t character = ram[TEXT_PAGE_TABLE[y] + x];

        // Calculate font offset (8 bytes per character, starting at 0x20)
        const uint32_t* charPixels = &APPLE2_FONT_32[character * 56];

        bool inverse = false;
        // Check if top two bits are 0 (0x00-0x3F range)
        if ((character & 0xC0) == 0) {
            inverse = true;
        } else if (((character & 0xC0) == 0x40)) {
            inverse = ds->flash_state;
        }
        
        // for inverse, xor the pixels with 0xFFFFFFFF to invert them.
        uint32_t xor_mask = 0x00000000;
        if (inverse) {
            xor_mask = 0xFFFFFFFF;
        }

        int pitchoff = pitch / 4;
        int charoff = x * 7 * 2; // draw characters "double wide" for new screen geometry
        // Draw the character bitmap into the texture
        uint32_t* texturePixels = (uint32_t*)pixels;
        for (int row = 0; row < 8; row++) {
            uint32_t base = row * pitchoff;
            texturePixels[base + charoff ] = (charPixels[0] ^ xor_mask) & color_value;
            texturePixels[base + charoff +1 ] = (charPixels[0] ^ xor_mask) & color_value;
            
            texturePixels[base + charoff + 2] = (charPixels[1] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 3] = (charPixels[1] ^ xor_mask) & color_value;

            texturePixels[base + charoff + 4] = (charPixels[2] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 5] = (charPixels[2] ^ xor_mask) & color_value;
            
            texturePixels[base + charoff + 6] = (charPixels[3] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 7] = (charPixels[3] ^ xor_mask) & color_value;

            texturePixels[base + charoff + 8] = (charPixels[4] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 9] = (charPixels[4] ^ xor_mask) & color_value;
            
            texturePixels[base + charoff + 10] = (charPixels[5] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 11] = (charPixels[5] ^ xor_mask) & color_value;
            
            texturePixels[base + charoff + 12] = (charPixels[6] ^ xor_mask) & color_value;
            texturePixels[base + charoff + 13] = (charPixels[6] ^ xor_mask) & color_value;
            
            charPixels += 7;
        }
    }
}
#endif

#if 0
void render_text_scanline_ng(cpu_state *cpu, int y) {

    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    uint8_t *textdata = NULL;

    uint8_t *output = frameBuffer + (y * 8 * 560);
    uint8_t *ram = ds->mmu->get_memory_base();

    if (ds->display_page_num == DISPLAY_PAGE_1) {
        //textdata = cpu->memory->pages_read[0x04];
        //textdata = cpu->mmu->get_page_base_address(0x04);
        textdata = ram + 0x0400;
    } else if (ds->display_page_num == DISPLAY_PAGE_2) {
        //textdata = cpu->memory->pages_read[0x08];
        //textdata = cpu->mmu->get_page_base_address(0x08);
        textdata = ram + 0x0800;
    } else {
        return;
    }

    int offset = textMapIndex[y];

    for (int x = 0; x < 40; x++) {

        uint8_t character = textdata[offset + x];

        // Calculate font offset (8 bytes per character, starting at 0x20)
        const uint8_t* charPixels = &APPLE2_FONT_8[character * 56];

        bool inverse = false;
        // Check if top two bits are 0 (0x00-0x3F range)
        if ((character & 0xC0) == 0) {
            inverse = true;
        } else if (((character & 0xC0) == 0x40)) {
            inverse = ds->flash_state;
        }
        
        // for inverse, xor the pixels with 0xFFFFFFFF to invert them.
        uint8_t xor_mask = (inverse) ? 0xFF : 0x00;

        //int pitchoff = pitch / 4;
        int charoff = x * 7 * 2; // draw characters "double wide" for new screen geometry
        // Draw the character bitmap into the texture
        //uint32_t* texturePixels = (uint32_t*)pixels;
        uint32_t base = 0;
        for (int row = 0; row < 8; row++) {
            
            output[base + charoff ] = (charPixels[0] ^ xor_mask);
            output[base + charoff +1 ] = (charPixels[0] ^ xor_mask);
            
            output[base + charoff + 2] = (charPixels[1] ^ xor_mask);
            output[base + charoff + 3] = (charPixels[1] ^ xor_mask);

            output[base + charoff + 4] = (charPixels[2] ^ xor_mask);
            output[base + charoff + 5] = (charPixels[2] ^ xor_mask);
            
            output[base + charoff + 6] = (charPixels[3] ^ xor_mask);
            output[base + charoff + 7] = (charPixels[3] ^ xor_mask);

            output[base + charoff + 8] = (charPixels[4] ^ xor_mask);
            output[base + charoff + 9] = (charPixels[4] ^ xor_mask);
            
            output[base + charoff + 10] = (charPixels[5] ^ xor_mask);
            output[base + charoff + 11] = (charPixels[5] ^ xor_mask);
            
            output[base + charoff + 12] = (charPixels[6] ^ xor_mask);
            output[base + charoff + 13] = (charPixels[6] ^ xor_mask);
            
            charPixels += 7;
            base += 560;
        }
    }
}
#endif

#if 0
uint8_t txt_bus_read(cpu_state *cpu, uint16_t address) {
    return 0;
}
#endif

/* ntsc */


#if 0

#define DEBUG 0

int hiresMap[24] = {
            0x0000,
            0x0080,
            0x0100,
            0x0180,
            0x0200,
            0x0280,
            0x0300,
            0x0380,

            0x0028,
            0x00A8,
            0x0128,
            0x01A8,
            0x0228,
            0x02A8,
            0x0328,
            0x03A8,

            0x0050,
            0x00D0,
            0x0150,
            0x01D0,
            0x0250,
            0x02D0,
            0x0350,
            0x03D0,
        };

inline int hiresMapIndex(int y) {
    int row1 = y / 8;
    int row2 = y % 8;
    return hiresMap[row1] + row2 * 0x400;
}

/**
 * @brief Processes a single scanline (40 bytes) of Apple II hi-res data
 * 
 * @param hiresData The complete hi-res image data
 * @param startOffset The starting offset in the image data for this scanline
 * @param output Vector to store the processed scanline data
 * @param outputOffset Offset in the output vector to store the data
 * @return The number of bytes written to the output
 */
size_t generateHiresScanline(uint8_t *hiresData, int startOffset, 
                           uint8_t *output, size_t outputOffset) {
    int lastByte = 0x00;
    size_t index = outputOffset;
    
    //printf("hiresData: %p startOffset: %04X\n", hiresData, startOffset);

    // Process 40 bytes (one scanline)
    for (int x = 0; x < 40; x++) {
        uint8_t byte = hiresData[startOffset + x];

        size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font

        for (int i = 0; i < CELL_WIDTH; i++) {
            output[index++] = hires40Font[fontIndex + i];
            //printf("%02X ", hires40Font[fontIndex + i]);
        }
        
        lastByte = byte;
    }
    
    return index - outputOffset; // Return number of bytes written
}

/**
 * Called with:
 * hiresData: pointer to 8K block in memory of the hi-res image. We will "raw" read this from the memory mapped memory buffer.
 * bitSignalOut: pointer to 560x192 array of uint8_t. This will be filled with the bit signal data.
 * outputOffset: offset in the output array to store the data.
 * y: the y coordinate of the scanline we are processing.
 */
void emitBitSignalHGR(uint8_t *hiresData, uint8_t *bitSignalOut, size_t outputOffset, int y) {
   
    // Get the starting memory offset for this scanline
    int row1 = y / 8;
    int row2 = y % 8;
    int offset = hiresMap[row1] + row2 * 0x400;

    // Process this scanline directly
    //size_t outputOffset = y * pixelsPerScanline;
    size_t bytesWritten = generateHiresScanline(hiresData, offset, bitSignalOut, outputOffset);

}

/**
 * pixels here is the texture update buffer.
 */
void render_hgrng_scanline(cpu_state *cpu, int y, uint8_t *pixels)
{
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    uint8_t *hgrdata = NULL;
    uint8_t *ram = ds->mmu->get_memory_base();

    if (ds->display_page_num == DISPLAY_PAGE_1) {
        //hgrdata = cpu->memory->pages_read[0x20];
        hgrdata = ram + 0x2000; //cpu->mmu->get_page_base_address(0x20);
    } else if (ds->display_page_num == DISPLAY_PAGE_2) {
        //hgrdata = cpu->memory->pages_read[0x40];
        hgrdata = ram + 0x4000; //cpu->mmu->get_page_base_address(0x40);
    } else {
        return;
    }

    for (int yy = y * 8; yy < (y + 1) * 8; yy++) {
        //printf("yy: %d\n", yy);
        emitBitSignalHGR(hgrdata, frameBuffer, yy * 560, yy);
    }
    processAppleIIFrame_LUT(frameBuffer + (y * 8 * 560), (RGBA_t *)pixels, y * 8, (y + 1) * 8);
}
#endif


#if 0
int loresMapIndex[24] =
  {   // text page 1 line addresses
            0x0000,
            0x0080,
            0x0100,
            0x0180,
            0x0200,
            0x0280,
            0x0300,
            0x0380,

            0x0028,
            0x00A8,
            0x0128,
            0x01A8,
            0x0228,
            0x02A8,
            0x0328,
            0x03A8,

            0x0050,
            0x00D0,
            0x0150,
            0x01D0,
            0x0250,
            0x02D0,
            0x0350,
            0x03D0,
        };

#define DEBUG 1

size_t generateLoresScanline(uint8_t *loresData, int startOffset, 
                           uint8_t *output, size_t outputOffset) {
    int lastByte = 0x00;
    size_t index = outputOffset;
    
    // Process 40 bytes (one scanline)
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 40; x++) {
            uint8_t byte = loresData[startOffset + x];

            if (y & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                byte = byte >> 4;
            }
            int pixeloff = (x * 14) % 4;

            for (int bits = 0; bits < 14; bits++) {
                uint8_t bit = ((byte >> pixeloff) & 0x01);
                uint8_t gray = (bit ? 0xFF : 0x00);
                output[index++] = gray;
                pixeloff = (pixeloff + 1) % 4;
            }
        }
    }
    
    return index - outputOffset; // Return number of bytes written
}

void render_lgrng_scanline(cpu_state *cpu, int y)
{
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    const int pixelsPerScanline = 40 * CELL_WIDTH;
    uint8_t *lgrdata = NULL;
    uint16_t offset = loresMapIndex[y];
    uint8_t *ram = ds->mmu->get_memory_base();

    if (ds->display_page_num == DISPLAY_PAGE_1) {
        //lgrdata = cpu->memory->pages_read[0x04];
        lgrdata = ram + 0x0400; //cpu->mmu->get_page_base_address(0x04);
    } else if (ds->display_page_num == DISPLAY_PAGE_2) {
        //lgrdata = cpu->memory->pages_read[0x08];
        lgrdata = ram + 0x0800; //cpu->mmu->get_page_base_address(0x08);
    } else {
        return;
    }
    size_t outputOffset = y * pixelsPerScanline * 8;

    // Generate the bitstream for the lores line
    generateLoresScanline(lgrdata, offset, frameBuffer, outputOffset);

    // this processes scanlines in a range of y_start to y_end
    //processAppleIIFrame_LUT(frameBuffer + (y * 8 * 560), (RGBA *)pixels, y * 8, (y + 1) * 8); // convert to color
}
#endif

/* uint8_t *loresToGray(uint8_t *loresData) {
    // Calculate output size and pre-allocate
    const int pixelsPerScanline = 40 * CELL_WIDTH;
    const size_t totalPixels = 192 * pixelsPerScanline;
    uint8_t *bitSignalOut = new uint8_t[totalPixels];
    
    // Process each scanline (0-191)
    for (int y = 0; y < 24; y++) {
        // Get the starting memory offset for this scanline
        int offset = loresMapIndex[y];
        
        // Process this scanline directly
        size_t outputOffset = y * pixelsPerScanline * 8;
        size_t bytesWritten = generateLoresScanline(loresData, offset, bitSignalOut, outputOffset);
        
        // Print progress every 20 scanlines
        if (DEBUG ) {
            printf("Processed scanline %d at offset %04X\n", y, offset);
        }
    }
    
    printf("Processed entire image: %zu pixels\n", totalPixels);
    return bitSignalOut;
} */
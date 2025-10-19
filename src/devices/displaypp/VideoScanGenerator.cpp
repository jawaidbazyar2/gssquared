
#include "Device_ID.hpp"

//#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator.hpp"

VideoScanGenerator::VideoScanGenerator(CharRom *charrom)
{
    build_hires40Font(true);
    this->char_rom = charrom;
}

void VideoScanGenerator::build_hires40Font(bool delayEnabled) 
{
    for (int i = 0; i < 2 * CHAR_NUM; i++)
    {
        uint8_t value = (i & 0x7f) << 1 | (i >> 8);
        bool delay = delayEnabled && (i & 0x80);
        
        for (int x = 0; x < CHAR_WIDTH; x++)
        {
            bool bit = (value >> ((x + 2 - delay) >> 1)) & 0x1;
            
            hires40Font[i * CHAR_WIDTH + x] = bit ? 1 : 0;
        }
    }
}

void VideoScanGenerator::generate_frame(ScanBuffer *frame_scan, Frame560 *frame_byte)
{
    uint64_t fcnt = frame_scan->get_count();
    /* if (fcnt < 192*40) { // if there is less than a full frame, don't generate anything, ignore, and print a warning
        printf("Warning: less than a full frame in ScanBuffer: %lld\n", fcnt);
        return;
    } */
    /* if (fcnt > 192*40+8) {
        printf("Warning: more than a full frame in ScanBuffer\n");
    } */

    flash_counter++;
    if (flash_counter > 14) {
        flash_state = !flash_state;
        flash_counter = 0;
    }

    for (uint16_t vcount = 0; vcount < 192; vcount++)
    {
        //frame_scan->set_line(vcount);
        frame_byte->set_line(vcount);
        uint8_t lastByte = 0x00; // for hires
        Scan_t peek_scan = frame_scan->peek();
        color_mode_t color_mode;
        color_mode.colorburst = (peek_scan.mode == VM_TEXT40 || peek_scan.mode == VM_TEXT80) ? 0 : 1;
        color_mode.mixed_mode = peek_scan.flags & VS_FL_MIXED ? 1 : 0;
        uint8_t color_delay_mask = 0xFF;

        for (int hcount = 0; hcount < 53; hcount++) { 

            Scan_t scan = frame_scan->pull();
            uint8_t eff_mode = scan.mode;
            if ((scan.flags & VS_FL_MIXED) && (vcount >= 160)) {
                eff_mode = (scan.flags & VS_FL_80COL) ? VM_TEXT80 : VM_TEXT40; // or TEXT80 depending on mode..
            }

            switch (eff_mode) {
                case VM_BORDER_COLOR: {
                        for (int i = 0; i < 7; i++) {
                            frame_byte->push(0);
                        }
                    }
                    break;
                case VM_TEXT40: {
                        /* if (hcount == 0) {
                            for (int i = 0; i < 7; i++) {
                                frame_byte->push(0);
                            }
                            frame_byte->set_color_mode(vcount, color_mode);
                        } */
                        bool invert;
                        char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);

                        uint8_t tchar = scan.mainbyte;
        
                        if (char_rom->is_flash(tchar)) {
                            invert = flash_state;
                        } else {
                            invert = false;
                        }
        
                        uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
        
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert); 
                        cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); 
                        frame_byte->push((cdata & 1) ^ invert);     
                    }
                    break;
                case VM_TEXT80: {
                        if (hcount == 0) {
                            //frame_byte->set_color_mode(vcount, COLORBURST_OFF);
                            frame_byte->set_color_mode(vcount, color_mode);
                        }
                        bool invert;

                        uint8_t tchar = scan.auxbyte;
                        char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);
                        uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
        
                        if (char_rom->is_flash(tchar)) {
                            invert = flash_state;
                        } else {
                            invert = false;
                        }
        
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
        
                        tchar = scan.mainbyte;
                        cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                        if (char_rom->is_flash(tchar)) {
                            invert = flash_state;
                        } else {
                            invert = false;
                        }
        
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
                        frame_byte->push((cdata & 1) ^ invert); cdata>>=1;
            
                        if (hcount == 39) { // but they do have a trailing 7-pixel thing.. or do they?
                            for (uint16_t pp = 0; pp < 7; pp++) frame_byte->push(0);
                        }
                    }
                    break;
                case VM_LORES: {
                        if (hcount == 0) {
                            for (int i = 0; i < 7; i++) {
                                frame_byte->push(0);
                            }
                            frame_byte->set_color_mode(vcount, {1,0}); // COLORBURST_ON);
                        }
                        uint8_t tchar = scan.mainbyte;
                        
                        if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                            tchar = tchar >> 4;
                        }
                        uint16_t pixeloff = (hcount * 14) % 4;
        
                        for (int bits = 0; bits < CELL_WIDTH; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push(bit);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                    }
                    break;
                case VM_DLORES: {
                    if (hcount == 0) {
                        frame_byte->set_color_mode(vcount, {1,0}); // COLORBURST_ON);
                    }
                
                    uint8_t tchar = scan.auxbyte;

                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    uint16_t pixeloff = (hcount * 14) % 4;
    
                    for (uint16_t bits = 0; bits < 7; bits++) {
                        uint8_t bit = ((tchar >> pixeloff) & 0x01);
                        frame_byte->push(bit);
                        pixeloff = (pixeloff + 1) % 4;
                    }

                    tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    // this is correct.
                    pixeloff = (hcount * 14) % 4;
    
                    for (uint16_t bits = 0; bits < 7; bits++) {
                        uint8_t bit = ((tchar >> pixeloff) & 0x01);
                        frame_byte->push(bit);
                        pixeloff = (pixeloff + 1) % 4;
                    }        
                    
                    if (hcount == 39) { // but they do have a trailing 7-pixel thing.. or do they?
                        for (uint16_t pp = 0; pp < 7; pp++) frame_byte->push(0);
                    }
                }
                break;
                case VM_HIRES_NOSHIFT: // TODO: this needs to set a flag to disable color delay. But this should make it display something now.
                    color_delay_mask = 0x7F;

                case VM_HIRES: {
                        if (hcount == 0) {
                            for (int i = 0; i < 7; i++) {
                                frame_byte->push(0);
                            }
                            frame_byte->set_color_mode(vcount, {1,0}); // COLORBURST_ON);
                        }
                        uint8_t byte = scan.mainbyte & color_delay_mask;
                        size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font
                
                        for (int i = 0; i < 14; i++) {
                            frame_byte->push(hires40Font[fontIndex + i]);
                        }
                        lastByte = byte;
                    }
                    break;
                    case VM_DHIRES: {
                        if (hcount == 0) {
                            // dhgr starts at horz offset 0
                            frame_byte->set_color_mode(vcount, {1,0}); // COLORBURST_ON);
                        }
                         
                        uint8_t byteM = scan.mainbyte;
                        uint8_t byteA = scan.auxbyte;
                        for (int i = 0; i < 7; i++ ) {
                            frame_byte->push((byteA & 0x01) ? 1 : 0);
                            byteA >>= 1;
                        }
                        for (int i = 0; i < 7; i++ ) {
                            frame_byte->push((byteM & 0x01) ? 1 : 0);
                            byteM >>= 1;
                        }
                        
                        if (hcount == 39) { // but they do have a trailing 7-pixel thing.. or do they?
                            for (uint16_t pp = 0; pp < 7; pp++) frame_byte->push(0);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}


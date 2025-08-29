#include <cstdint>
#include <cstring>

#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/CharRom.hpp"


#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class AppleII_Display {

private: 
    CharRom &char_rom;
    bool flash_state = false;
    bool alt_char_set = false;
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];

    alignas(64) uint16_t A2_textMap[24] =
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

    alignas(64) uint16_t A2_hiresMap[24] = {
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

public:
    AppleII_Display(CharRom &char_rom) : char_rom(char_rom) { 
        buildHires40Font(true);
     }

    void set_char_set(bool alt_char_set) {
        char_rom.set_char_set(alt_char_set);
    }

    void set_flash_state(bool flash_state) {
        this->flash_state = flash_state;
    }

    /** delayEnabled is true for any Apple II model except Apple II Rev 0. */
    void buildHires40Font(bool delayEnabled)
    {
        for (int i = 0; i < 2 * CHAR_NUM; i++)
        {
            uint8_t value = (i & 0x7f) << 1 | (i >> 8);
            bool delay = delayEnabled && (i & 0x80);
            
            for (int x = 0; x < CHAR_WIDTH; x++)
            {
                bool bit = (value >> ((x + 2 - delay) >> 1)) & 0x1;
                
                hires40Font[i * CHAR_WIDTH + x] = bit ? 0xff : 0x00;
            }
        }
    }

/** Call with: pointer to text memory; pointer to frame; linegroup number */
    void generate_text40(uint8_t *textpage, Frame560 *f, uint16_t linegroup) {
        uint16_t scanline = linegroup * 8;
        uint16_t x = 0;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_OFF);
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);

            for (x = 0; x < 40; x++) {
                bool invert;

                uint8_t tchar = textpage[char_addr];

                if (char_rom.is_flash(tchar)) {
                    invert = flash_state;
                } else {
                    invert = false;
                }

                uint8_t cdata = char_rom.get_char_scanline(tchar, y);

                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 
                cdata>>=1;
                f->push((cdata & 1) ^ invert); 
                f->push((cdata & 1) ^ invert); 

                char_addr++;
            }
            scanline++;
        }
    }


    void generate_text80(uint8_t *textpage, uint8_t *alttextpage, Frame560 *f, uint16_t linegroup) {
        uint16_t scanline = linegroup * 8;
        uint16_t x = 0;
        bool pixel_on = 1;
        bool pixel_off = 0;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_OFF);

            for (x = 0; x < 40; x++) {
                bool invert;

                uint8_t tchar = alttextpage[char_addr];

                uint8_t cdata = char_rom.get_char_scanline(tchar, y);

                if (char_rom.is_flash(tchar)) {
                    invert = flash_state;
                } else {
                    invert = false;
                }

                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;

                tchar = textpage[char_addr];
                cdata = char_rom.get_char_scanline(tchar, y);
                if (char_rom.is_flash(tchar)) {
                    invert = flash_state;
                } else {
                    invert = false;
                }

                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;
                f->push((cdata & 1) ^ invert); cdata>>=1;

                char_addr++;
            }
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);
            scanline++;
        }
    }

    void generate_hires40(uint8_t *hgrpage, Frame560 *f, uint16_t linegroup) {
        uint8_t *d = hgrpage + A2_hiresMap[linegroup];
        uint16_t scanline = linegroup * 8;

        int lastByte = 0x00;
        for (uint16_t line = 0; line < 8; line++) {
            // Process 40 bytes (one scanline)
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_ON);
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);

            for (int x = 0; x < 40; x++) {
                uint8_t byte = d[x];

                size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font

                for (int i = 0; i < CELL_WIDTH; i++) {
                    f->push(hires40Font[fontIndex + i]);
                    //output[index++] = hires40Font[fontIndex + i];
                }
                lastByte = byte;
            }
            d += 0x400; // go to next line
            scanline++;
        }
    }

    void generate_hires40_noshift(uint8_t *hgrpage, Frame560 *f, uint16_t linegroup) {
        uint8_t *d = hgrpage + A2_hiresMap[linegroup];
        uint16_t scanline = linegroup * 8;

        int lastByte = 0x00;
        for (uint16_t line = 0; line < 8; line++) {
            // Process 40 bytes (one scanline)
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_ON);
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);

            for (int x = 0; x < 40; x++) {
                uint8_t byte = d[x] & 0x7F;

                size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font

                for (int i = 0; i < CELL_WIDTH; i++) {
                    f->push(hires40Font[fontIndex + i]);
                    //output[index++] = hires40Font[fontIndex + i];
                }
                lastByte = byte;
            }
            d += 0x400; // go to next line
            scanline++;
        }
    }

    void generate_hires80(uint8_t *hgrpage, uint8_t *althgrpage, Frame560 *f, uint16_t linegroup) {
        uint8_t *m = hgrpage + A2_hiresMap[linegroup];
        uint8_t *a = althgrpage + A2_hiresMap[linegroup];
        uint16_t scanline = linegroup * 8;

        for (uint16_t line = 0; line < 8; line++) {
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_ON);

            for (int x = 0; x < 40; x++) {
                uint8_t byteM = m[x];
                uint8_t byteA = a[x];
                for (int i = 0; i < 7; i++ ) {
                    f->push((byteA & 0x01) ? 1 : 0);
                    byteA >>= 1;
                }
                for (int i = 0; i < 7; i++ ) {
                    f->push((byteM & 0x01) ? 1 : 0);
                    byteM >>= 1;
                }
            }
            m += 0x400; // go to next line
            a += 0x400; // go to next line
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);
            scanline++;
        }
    }

    void generate_lores40(uint8_t *textpage, Frame560 *f, uint16_t linegroup) {
        uint16_t scanline = linegroup * 8;
        uint16_t x = 0;
        bool pixel_on = 1;
        bool pixel_off = 0;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_ON);

            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);
            
            for (x = 0; x < 40; x++) {
                uint8_t tchar = textpage[char_addr];
                
                if (y & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                    tchar = tchar >> 4;
                }
                uint16_t pixeloff = (x * 14) % 4;

                for (int bits = 0; bits < 14; bits++) {
                    uint8_t bit = ((tchar >> pixeloff) & 0x01);
                    f->push(bit);
                    pixeloff = (pixeloff + 1) % 4;
                }
                char_addr++;
            }
            scanline++;
        }
    }
    
    // TODO: this is just a guess right now.
    // Will need to start the phase 180 degrees off since we start 'early' in alt?
    void generate_lores80(uint8_t *textpage, uint8_t *alttextpage, Frame560 *f, uint16_t linegroup) {
        uint16_t scanline = linegroup * 8;
        //uint16_t x = 0;
        bool pixel_on = 1;
        bool pixel_off = 0;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            f->set_color_mode(scanline, COLORBURST_ON);

            for (uint16_t x = 0; x < 40; x++) {
                uint8_t tchar = alttextpage[char_addr];
                
                if (y & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                    tchar = tchar >> 4;
                }
                uint16_t pixeloff = (x * 14) % 4;

                for (uint16_t bits = 0; bits < 7; bits++) {
                    uint8_t bit = ((tchar >> pixeloff) & 0x01);
                    f->push(bit);
                    pixeloff = (pixeloff + 1) % 4;
                }
                
                tchar = textpage[char_addr];
                
                if (y & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                    tchar = tchar >> 4;
                }
                // this is correct.
                pixeloff = (x * 14) % 4;

                for (uint16_t bits = 0; bits < 7; bits++) {
                    uint8_t bit = ((tchar >> pixeloff) & 0x01);
                    f->push(bit);
                    pixeloff = (pixeloff + 1) % 4;
                }

                char_addr++;
            }
            for (uint16_t pp = 0; pp < 7; pp++) f->push(0);
            scanline++;
        }
    }

};

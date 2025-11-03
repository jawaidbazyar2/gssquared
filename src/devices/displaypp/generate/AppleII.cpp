#include <cstdint>
#include <cstring>

#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "AppleIIgs.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14


class AppleII_Display {

private: 
    CharRom *char_rom;
    bool flash_state = false;
    bool alt_char_set = false;
    bool f_80store = false;
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];
    uint8_t text_fg = 0x0F;
    uint8_t text_bg = 0x00;
    uint8_t text_color = 0xF0;
    uint8_t border_color = 0x00;

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
    AppleII_Display(CharRom *char_rom) : char_rom(char_rom) { 
        buildHires40Font(true);
     }

    void set_char_set(bool alt_char_set) {
        char_rom->set_char_set(alt_char_set);
    }

    void set_flash_state(bool flash_state) {
        this->flash_state = flash_state;
    }

    inline void set_80store(bool f_80store) {
        this->f_80store = f_80store;
    }

    inline bool is_80store() { return f_80store; }

    inline void set_text_fg(uint8_t fg) {
        this->text_fg = fg & 0x0F;
    }

    inline void set_text_bg(uint8_t bg) {
        this->text_bg = bg& 0x0F;
    }

    inline void set_border_color(uint8_t color) {
        this->border_color = color;
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

        uint8_t tc = (text_fg << 4) | 1;
        uint8_t td = text_bg << 4;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            color_mode_t cmode = {0,0,0};  // COLORBURST_OFF
            f->set_color_mode(scanline, cmode);

            for (x = 0; x < 40; x++) {

                uint8_t tchar = textpage[char_addr];

                uint8_t invert;
                if (char_rom->is_flash(tchar)) {
                    invert = flash_state ? 0xFF : 0x00;
                } else {
                    invert = 0x00;
                }

                uint8_t cdata = char_rom->get_char_scanline(tchar, y);
                cdata ^= invert;
                for (int n = 0; n < 7; n++) { // it's ok compiler will unroll this
                    f->push((cdata & 1) ? tc : td); 
                    f->push((cdata & 1) ? tc : td); 
                    cdata>>=1;
                }

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

        uint8_t tc = (text_fg << 4) | 1;
        uint8_t td = text_bg << 4;

        for (uint16_t y = 0; y < 8; y++) {
            uint16_t char_addr = A2_textMap[linegroup];
            f->set_line(scanline);
            color_mode_t cmode = {0,0,1};  // COLORBURST_OFF
            f->set_color_mode(scanline, cmode);

            for (x = 0; x < 40; x++) {
                uint8_t tchar = alttextpage[char_addr];
                uint8_t cdata = char_rom->get_char_scanline(tchar, y);

                uint8_t invert;
                if (char_rom->is_flash(tchar)) {
                    invert = flash_state ? 0xFF : 0x00;
                } else {
                    invert = 0x00;
                }
                cdata ^= invert;
                for (int n = 0; n < 7; n++) {
                    f->push((cdata & 1) ? tc : td); cdata>>=1;
                }

                tchar = textpage[char_addr];
                cdata = char_rom->get_char_scanline(tchar, y);

                if (char_rom->is_flash(tchar)) {
                    invert = flash_state ? 0xFF : 0x00;
                } else {
                    invert = 0x00;
                }
                cdata ^= invert;
                for (int n = 0; n < 7; n++) {
                    f->push((cdata & 1) ? tc : td); cdata>>=1;
                }

                char_addr++;
            }
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
            color_mode_t cmode = {1,0,0};  // COLORBURST_ON
            f->set_color_mode(scanline, cmode);

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
            color_mode_t cmode = {1,0,0};  // COLORBURST_ON
            f->set_color_mode(scanline, cmode);

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
            color_mode_t cmode = {1,0,1};  // COLORBURST_ON
            f->set_color_mode(scanline, cmode);

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
            color_mode_t cmode = {1,0,0};  // COLORBURST_ON
            f->set_color_mode(scanline, cmode);
            
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
            color_mode_t cmode = {1,0,1};  // COLORBURST_ON
            f->set_color_mode(scanline, cmode);

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
            scanline++;
        }
    }

    // TODO: need to implement 320 fill mode.
    void generate_shr(SHR *shrPage, Frame640 *f) {
        for (uint16_t line = 0; line < 200; line++) {
            SHRMode mode = shrPage->modes[line];
            uint8_t p_num = mode.p;

            f->set_line(line);

            if (mode.mode640) { // each byte is 4 pixels
                for (int x = 0; x < 160; x++) {
                    uint8_t pval = shrPage->pixels[line * 160 + x];
    
                    SHRColor pind = shrPage->palettes[p_num].colors[pixel640<3>(pval) + 0x8];
                    f->push(convert12bitTo24bit(pind));
                    
                    pind = shrPage->palettes[p_num].colors[pixel640<2>(pval) + 0x0C];
                    f->push(convert12bitTo24bit(pind));
    
                    pind = shrPage->palettes[p_num].colors[pixel640<1>(pval) + 0x00];
                    f->push(convert12bitTo24bit(pind));
    
                    pind = shrPage->palettes[p_num].colors[pixel640<0>(pval) + 0x04];
                    f->push(convert12bitTo24bit(pind));
                }
            } else {
                for (int x = 0; x < 160; x++) { // each byte is 2 pixels
                    uint8_t pval = shrPage->pixels[line * 160 + x];
    
                    SHRColor pind = shrPage->palettes[p_num].colors[pixel320<1>(pval)];
                    RGBA_t xx = convert12bitTo24bit(pind);
                    f->push(xx);
                    f->push(xx);
    
                    pind = shrPage->palettes[p_num].colors[pixel320<0>(pval)];
                    xx = convert12bitTo24bit(pind);
                    f->push(xx);
                    f->push(xx);
                }
            }
        }
    }
};

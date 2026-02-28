
#include "Device_ID.hpp"
#include "AppleIIgsColors.hpp"

//#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator.hpp"
#include "generate/AppleIIgs.hpp"
#include "util/printf_helper.hpp"

// Alias to the shared Apple IIgs color table for text rendering
static const RGBA_t (&gs_text_palette)[16] = AppleIIgs::TEXT_COLORS;

VideoScanGenerator::VideoScanGenerator(CharRom *charrom, bool border_enabled)
{
    build_hires40Font(true);
    this->char_rom = charrom;
    //this->border_enabled = border_enabled;
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

void VideoScanGenerator::generate_frame(ScanBuffer *frame_scan, Frame560 *frame_byte, FrameBorder *border, Frame640 *frame_shr)
{
    uint64_t fcnt = frame_scan->get_count();
    if (fcnt == 0) {
        printf("Warning: no data in ScanBuffer\n");
        return;
    }

    if (border != nullptr) {
        assert(1);
    }

    flash_counter++;
    if (flash_counter > 14) {
        flash_state = !flash_state;
        flash_counter = 0;
    }

    SHRMode mode = { .p = 0 }; 
    Palette palette = { .colors = {0} };
    RGBA_t lastpixel = {0};

    uint32_t hcount = 0;
    uint32_t vcount = 0;
    frame_byte->set_line(vcount);
    if (border != nullptr) {
        border->set_line(vcount);
    }
    if (frame_shr != nullptr) {
        frame_shr->set_line(vcount);
    }

    uint8_t lastByte = 0x00; // for hires
    color_mode_t color_mode;
    
    uint8_t color_delay_mask = 0xFF;

    int palette_index = 0; // reset to 0 each scanline.
    bool modeChecks = true;

    while (1) {
        Scan_t scan = frame_scan->pull();
        if (modeChecks && scan.mode <= VM_DHIRES) {
            color_mode.colorburst = (scan.mode == VM_TEXT40 || scan.mode == VM_TEXT80) ? 0 : 1;
            color_mode.mixed_mode = scan.flags & VS_FL_MIXED ? 1 : 0;
            modeChecks = false;
        }

        uint8_t eff_mode = scan.mode;
        if ((eff_mode <= VM_DHIRES) && (scan.flags & VS_FL_MIXED) && (vcount >= 160)) {
            eff_mode = (scan.flags & VS_FL_80COL) ? VM_TEXT80 : VM_TEXT40; // or TEXT80 depending on mode..
        }

        switch (eff_mode) {
            case VM_VSYNC:  // end of frame
                    return; 

            case VM_HSYNC: {
                    lastByte = 0x00; // for hires
                    hcount = 0;
                    vcount++;
                    if (vcount >= 200) {
                        assert(1);
                    }
                    modeChecks = true;
                    frame_byte->set_line(vcount);
                    if (border != nullptr) {
                        border->set_line(vcount);
                    }
                    if (frame_shr != nullptr) {
                        frame_shr->set_line(vcount);
                    }

                    // from MAME
                    // the low 5 bits of the SCB determine the initial fillmode color
                    // for the scanline (hardware testing by John Brooks)
                    static const uint32_t fillmode_init[32] =
                    {
                        2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
                        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
                    };
                    /* Right now it seems to be consistently palette index #2 unless I use the magic undefined bit, and then it is consistently palette index #3. - Ian Brumby 4/17/2020 */
                    lastpixel = convert12bitTo24bit(palette.colors[fillmode_init[vcount & 0x1F]]);
                }
                break;
            case VM_BORDER_COLOR: {
                    if (border != nullptr) {
                        border->push(gs_text_palette[scan.mainbyte & 0x0F]);
                    }
                }
                break; // we don't increment hcount here.
            case VM_SHR: {
                    
                    uint8_t p_num = mode.p;
                    uint32_t shr_bytes = scan.shr_bytes; // 4 video bytes in 32 bits.
                    for (int x = 0; x < 4; x++) {
                        uint8_t pval = shr_bytes & 0xFF;
                        shr_bytes >>= 8;

                        if (mode.mode640) { // each byte is 4 pixels
                
                            SHRColor pind = palette.colors[pixel640<3>(pval) + 0x8];
                            frame_shr->push(convert12bitTo24bit(pind));
                            
                            pind = palette.colors[pixel640<2>(pval) + 0x0C];
                            frame_shr->push(convert12bitTo24bit(pind));
            
                            pind = palette.colors[pixel640<1>(pval) + 0x00];
                            frame_shr->push(convert12bitTo24bit(pind));
            
                            pind = palette.colors[pixel640<0>(pval) + 0x04];
                            frame_shr->push(convert12bitTo24bit(pind));
                        } else {
                            uint16_t pixel = pixel320<1>(pval);
                            SHRColor pind;
                           
                            if (!mode.fill || (pixel != 0)) {
                                pind = palette.colors[pixel];
                                lastpixel = convert12bitTo24bit(pind);
                            }
                            frame_shr->push(lastpixel);
                            frame_shr->push(lastpixel);

                            pixel = pixel320<0>(pval);
                           
                            if (!mode.fill || (pixel != 0)) {
                                pind = palette.colors[pixel];
                                lastpixel = convert12bitTo24bit(pind);
                            }
                            frame_shr->push(lastpixel);
                            frame_shr->push(lastpixel);
                        }
                    }
                }
                break;
            case VM_SHR_MODE: {
                    mode.v = scan.mainbyte;
                }
                break;
            case VM_SHR_PALETTE: { // load the palette values into palette based on index.
                    palette.colors[palette_index].v = scan.shr_bytes & 0xFFFF;
                    palette.colors[palette_index+1].v = (scan.shr_bytes >> 16) & 0xFFFF;
                    palette_index = (palette_index + 2) % 16;
                }
                break;
            case VM_TEXT40: {
                    if (hcount == 0) {
                        color_mode_t cmode = color_mode;
                        cmode.phase_offset = 0;
                        frame_byte->set_color_mode(vcount, cmode);
                    }
                    
                    //char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);
                    char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);

                    uint8_t tchar = scan.mainbyte;
                    uint8_t invert;
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    uint8_t tc = (scan.shr_bytes & 0xF0) | 1;
                    uint8_t td = (scan.shr_bytes & 0x0F) << 4;
                    uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    cdata ^= invert;

                    for (int n = 0; n < 7; n++) { // it's ok compiler will unroll this
                        frame_byte->push((cdata & 1) ? tc : td); 
                        frame_byte->push((cdata & 1) ? tc : td); 
                        cdata>>=1;
                    }
                }
                hcount++;
                break;
            case VM_TEXT80: {
                    if (hcount == 0) {
                        //frame_byte->set_color_mode(vcount, COLORBURST_OFF);
                        color_mode_t cmode = color_mode;
                        cmode.phase_offset = 1;
                        frame_byte->set_color_mode(vcount, cmode);
                    }
                    uint8_t invert;

                    uint8_t tchar = scan.auxbyte;
                    //char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);
                    char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);
                    uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
    
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    cdata ^= invert;
                    uint8_t tc = (scan.shr_bytes & 0xF0) | 1;
                    uint8_t td = (scan.shr_bytes & 0x0F) << 4;

                    for (int n = 0; n < 7; n++) {
                        frame_byte->push((cdata & 1) ? tc : td); cdata>>=1;
                    }

                    tchar = scan.mainbyte;
                    cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    cdata ^= invert;
                    tc = (scan.shr_bytes & 0xF0) | 1;
                    td = (scan.shr_bytes & 0x0F) << 4;
                    for (int n = 0; n < 7; n++) {
                        frame_byte->push((cdata & 1) ? tc : td); cdata>>=1;
                    }
                }
                hcount++;
                break;
            case VM_LORES: {
                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 0};
                        frame_byte->set_color_mode(vcount, cmode); // COLORBURST_ON);
                    }
                    uint8_t tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    uint32_t pixeloff = (hcount * 14) % 4;
    
                    for (size_t bits = 0; bits < CELL_WIDTH; bits++) {
                        uint8_t bit = ((tchar >> pixeloff) & 0x01);
                        frame_byte->push(bit);
                        pixeloff = (pixeloff + 1) % 4;
                    }
                }
                hcount++;
                break;
            case VM_DLORES: {
                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 1};
                        frame_byte->set_color_mode(vcount, cmode); // COLORBURST_ON);
                    }
                
                    uint8_t tchar = scan.auxbyte;

                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    uint32_t pixeloff = (hcount * 14) % 4;
    
                    for (size_t bits = 0; bits < 7; bits++) {
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
    
                    for (size_t bits = 0; bits < 7; bits++) {
                        uint8_t bit = ((tchar >> pixeloff) & 0x01);
                        frame_byte->push(bit);
                        pixeloff = (pixeloff + 1) % 4;
                    }        
                }
                hcount++;
                break;
            
            case VM_HIRES_NOSHIFT: // TODO: this needs to set a flag to disable color delay. But this should make it display something now.
                color_delay_mask = 0x7F;

            case VM_HIRES: {
                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 0};
                        frame_byte->set_color_mode(vcount, cmode); // COLORBURST_ON);
                    }
                    uint8_t byte = scan.mainbyte & color_delay_mask;
                    size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font
            
                    for (size_t i = 0; i < 14; i++) {
                        frame_byte->push(hires40Font[fontIndex + i]);
                    }
                    lastByte = byte;
                }
                hcount++;
                break;
            case VM_DHIRES: {
                    if (hcount == 0) {
                        // dhgr starts at horz offset 0
                        color_mode_t cmode = {1,0, 1};
                        // TODO: this is where we insert colorburst off to implement the IIgs monochrome modes.
                        if (mono_mode) cmode.colorburst = 0; // this is sufficient for NTSC. but rgb.. 
                        frame_byte->set_color_mode(vcount, cmode); // COLORBURST_ON);
                    }
                    uint8_t oncolor = (mono_mode) ? 0xF1 : 1; // 0xF0 is mono mode - with no colorburst we have to provide the color...
                        
                    uint8_t byteM = scan.mainbyte;
                    uint8_t byteA = scan.auxbyte;
                    for (size_t i = 0; i < 7; i++ ) {
                        frame_byte->push((byteA & 0x01) ? oncolor : 0);
                        byteA >>= 1;
                    }
                    for (size_t i = 0; i < 7; i++ ) {
                        frame_byte->push((byteM & 0x01) ? oncolor : 0);
                        byteM >>= 1;
                    }
                }
                hcount++;
                break;
            default:
                break;
        }
    }

    fcnt = frame_scan->get_count();
    if (fcnt > 100) {
        printf("Warning: %llu elements left in ScanBuffer at end of generate_frame\n", u64_t(fcnt));
    }
}


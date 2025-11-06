
#include "Device_ID.hpp"

//#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator.hpp"
#include "generate/AppleIIgs.hpp"

static const RGBA_t gs_text_palette[16] = {
    RGBA_t::make(0x00, 0x00, 0x00, 0xFF), // 0x0000 - Black        0b0000 - ok
    RGBA_t::make(0xDD, 0x00, 0x33, 0xFF), // 0x0D03 - Deep Red     0b0001 - ok
    RGBA_t::make(0x88, 0x55, 0x00, 0xFF), // 0x0850 - Brown        0b1000 - dk blue
    RGBA_t::make(0xFF, 0x66, 0x00, 0xFF), // 0x0F60 - Orange       0b1001 - purple
    RGBA_t::make(0x00, 0x77, 0x22, 0xFF), // 0x0072 - Dark Green   0b0100 - ok
    RGBA_t::make(0x55, 0x55, 0x55, 0xFF), // 0x0555 - Dark Gray    0b0101 - ok
    RGBA_t::make(0x11, 0xDD, 0x00, 0xFF), // 0x01D0 - Light Green 0b1100 - med blu
    RGBA_t::make(0xFF, 0xFF, 0x00, 0xFF), // 0x0FF0 - Yellow      0b1101 - lt blue

    RGBA_t::make(0x00, 0x00, 0x99, 0xFF), // 0x0009 - Dark Blue    0b0010 - brown
    RGBA_t::make(0xDD, 0x22, 0xDD, 0xFF), // 0x0D2D - Purple       0b0011 - orange
    RGBA_t::make(0xAA, 0xAA, 0xAA, 0xFF), // 0x0AAA - Light Gray  0b1010 - ok
    RGBA_t::make(0xFF, 0x99, 0x88, 0xFF), // 0x0F98 - Pink        0b1011 - ok
    RGBA_t::make(0x22, 0x22, 0xFF, 0xFF), // 0x022F - Medium Blue  0b0110 - light green
    RGBA_t::make(0x66, 0xAA, 0xFF, 0xFF), // 0x06AF - Light Blue   0b0111 - yellow
    RGBA_t::make(0x44, 0xFF, 0x99, 0xFF), // 0x04F9 - Aquamarine  0b1110 - ok
    RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)  // 0x0FFF - White       0b1111 - ok
};

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

    SHRMode mode = { .p = 0 };    // TODO: shrPage->modes[line]; (hard code 320 palette 0 for now, need to process Palette entries from ScanBuffer
    Palette palette = { .colors = {0} };

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
                
                            SHRColor pind = palette.colors[pixel320<1>(pval)];
                            RGBA_t xx = convert12bitTo24bit(pind);
                            frame_shr->push(xx);
                            frame_shr->push(xx);
            
                            pind = palette.colors[pixel320<0>(pval)];
                            xx = convert12bitTo24bit(pind);
                            frame_shr->push(xx);
                            frame_shr->push(xx);
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
                    
                    char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);

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
                    char_rom->set_char_set(scan.flags & VS_FL_ALTCHARSET ? 1 : 0);
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
                        frame_byte->set_color_mode(vcount, cmode); // COLORBURST_ON);
                    }
                        
                    uint8_t byteM = scan.mainbyte;
                    uint8_t byteA = scan.auxbyte;
                    for (size_t i = 0; i < 7; i++ ) {
                        frame_byte->push((byteA & 0x01) ? 1 : 0);
                        byteA >>= 1;
                    }
                    for (size_t i = 0; i < 7; i++ ) {
                        frame_byte->push((byteM & 0x01) ? 1 : 0);
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
        printf("Warning: %lld elements left in ScanBuffer at end of generate_frame\n", fcnt);
    }
}


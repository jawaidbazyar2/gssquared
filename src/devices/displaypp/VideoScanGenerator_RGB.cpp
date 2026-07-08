
#include "Device_ID.hpp"
#include "AppleIIgsColors.hpp"

//#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator_RGB.hpp"
#include "generate/AppleIIgs.hpp"
#include "util/printf_helper.hpp"
#include "render/GSRGB_LUT.hpp"
#include "render/GSRGB560.hpp"

// Alias to the shared Apple IIgs color table for text rendering
//static const RGBA_t (&gs_text_palette)[16] = AppleIIgs::TEXT_COLORS;

VideoScanGenerator_RGB::VideoScanGenerator_RGB(CharRom *charrom, bool border_enabled, FrameVSG *frame_vsg)
{
    build_hires40Font(true);
    this->char_rom = charrom;
    //this->border_enabled = border_enabled;

    mode = { .p = 0 }; 
    palette = { .colors = {0} };
    lastpixel = {0};

    hcount = 0;
    vcount = 0;

    beam_h = 0;
    beam_v = 0;
    sawdata = false;

    lastByte = 0x00; // for hires
    
    color_delay_mask = 0xFF;

    palette_index = 0; // reset to 0 each scanline.
    modeChecks = true;

    //this->frame_scan = frame_scan;
    this->frame_vsg = frame_vsg;

    lut = (uint16_t *)HiresColorTable;
    for (int i = 0; i < 16; i++) {
        hgr_color_lut[i] = RGBA_t::make(GSHGRColors[i].r>>8, GSHGRColors[i].g>>8, GSHGRColors[i].b>>8, 0xFF);
        //hgr_color_lut[i] = AppleIIgs::RGB_COLORS[i];
        txt_color_lut[i] = gs_txt_colors[i];
    }
    build_mono_lut(txt_color_lut, txt_mono_lut);
    build_mono_lut(hgr_color_lut, hgr_mono_lut);
    mono_mode = false;
    update_mono_lut();
/*     txt_lut = txt_color_lut;
    hgr_lut = hgr_color_lut; */

    frame_vsg->set_line(0);
}

void VideoScanGenerator_RGB::build_mono_lut(RGBA_t *ct, RGBA_t *mt) {
    for (int i = 0; i < 16; i++) {
        RGBA_t c = ct[i];
        uint16_t luma = (c.r * 0.299 + c.g * 0.587 + c.b * 0.114);
        mt[i] = RGBA_t::make(luma, luma, luma, 0xFF);
    }
}

void VideoScanGenerator_RGB::build_hires40Font(bool delayEnabled) 
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

// shiftreg - new bits (as we can left to right on scanline) are shifted left into shiftreg.
inline void VideoScanGenerator_RGB::add_hires_bits(uint8_t hires_byte) {
    size_t fontIndex = (hires_byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font

    for (size_t i = 0; i < 14; i++) {
        bit_stream.push(hires40Font[fontIndex + i]);
    }
    lastByte = hires_byte;
}

inline void VideoScanGenerator_RGB::add_dhires_bits(uint8_t main_byte, uint8_t aux_byte) {
    for (size_t i = 0; i < 7; i++ ) {
        bit_stream.push(aux_byte & 0x01);
        aux_byte >>= 1;
    }
    for (size_t i = 0; i < 7; i++ ) {
        bit_stream.push(main_byte & 0x01);
        main_byte >>= 1;
    }
}

inline void VideoScanGenerator_RGB::hires_start() {
    bit_stream.clear();
}

// on new scanline, shiftreg is initialized to 0.
// in one cycle, we're going to emit 14 pixels.
// Perform hgr lookup. This gets us next 14 dots of info.
// Each 11-bit lookup gets us 4 pixels.
// cycle -1: preload 3.
// cycle 0: add 14 bits; have 17 bits.
// execute pixel emission loop until we have less than 7 or fewer bits left in shift reg.

inline void VideoScanGenerator_RGB::emit_hires_pixels(uint16_t shift) {
    uint16_t pixels = lut[shiftreg];

    RGBA_t c = hgr_lut[(pixels >> 12) & 0xF];
    frame_vsg->push(c);
    c = hgr_lut[(pixels >> 8) & 0xF];
    frame_vsg->push(c);
    c = hgr_lut[(pixels >> 4) & 0xF];
    frame_vsg->push(c);
    c = hgr_lut[pixels & 0xF];
    frame_vsg->push(c);

}

// instead of calculating and passing bools here, just pass hcount and compare hcount.
//inline void VideoScanGenerator_RGB::render_hires(bool start_of_line, bool end_of_line) {
inline void VideoScanGenerator_RGB::render_hires() {
    // rgb, we are always in color mode, and text is always rendered artifact-free.

    bool bit;

    if (hcount == 0) { // preload shift reg
        for (int i = 0; i < 3-phase_offset; i++) {
            bit = bit_stream.front(); bit_stream.pop();
            shiftreg = ((shiftreg << 1) | bit);
        }
    }
    phase_offset = 0;

    while (bit_stream.size() >= 4) {
        // fetch 4 bits at a time
        for (int i = 0; i < 4; i++) {
            bit = bit_stream.front(); bit_stream.pop();
            shiftreg = ((shiftreg << 1) | bit);
        }
        shiftreg = shiftreg & 0x7FF; // 11 bits
        emit_hires_pixels(shiftreg);
    }
    // if we're at end of scanline and have less than 4 bits left do something about it..
    if (hcount == 39) {
        int cnt = bit_stream.size();
        assert(true);
        // we need to add extra bits to make the remainder four bits.
        for (int i = 0; i < cnt; i++) {
            bit = bit_stream.front(); bit_stream.pop();
            shiftreg = ((shiftreg << 1) | bit);
        }
        for (int i = 0; i < 4-cnt; i++) {
            shiftreg = ((shiftreg << 1) | 0);
        }
        shiftreg = shiftreg & 0x7FF; // 11 bits
        emit_hires_pixels(shiftreg);
    }

}

inline void VideoScanGenerator_RGB::render_hires_mono() {
    // rgb, we are always in color mode, and text is always rendered artifact-free.
    constexpr RGBA_t white = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF);
    constexpr RGBA_t black = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);

    bool bit;

    while (bit_stream.size()) {
        frame_vsg->push(bit_stream.front() ? white : black);
        bit_stream.pop();
    }
}

/**
 * Processes ScanBuffer (which is all the )
 */
void VideoScanGenerator_RGB::generate_frame(ScanBuffer *frame_scan)
{
    uint64_t fcnt = frame_scan->get_count();
    if (fcnt == 0) {
        //printf("Warning: no data in ScanBuffer\n");
        return;
    }

    // Start each frame from a clean, top-aligned slate. This generator paints
    // directly into the persistent frame_vsg texture and only resets its row
    // (beam_v) on VM_VSYNC, so if it is ever handed a partial / phase-shifted
    // buffer (e.g. after an MCP tool clears the scan buffer mid-frame), the
    // rows it doesn't reach would retain the previous frame's pixels and stack
    // into ghost copies. Clearing + resetting the row here makes every render
    // self-contained. frame_vsg is open()'d (locked) by the caller.
    frame_vsg->clear_stream(RGBA_t::make(0x00, 0x00, 0x00, 0xFF));
    beam_v = 0;
    frame_vsg->set_line(0);

    if (dump_next_frame) {
        frame_scan->saveToFile("scan_buffer.txt");
        dump_next_frame = false;
    }

    flash_counter++;
    if (flash_counter > 14) {
        flash_state = !flash_state;
        flash_counter = 0;
    }

    while (fcnt--) {
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
            case VM_BLANK:
                frame_vsg->advance(scanner_freq); // advance by scanner_freq pixels
                //frame_vsg->push_n(RGBA_t::make(0x00, 0x00, 0x00, 0xFF), scanner_freq);
                break;
            case VM_VSYNC:
                vcount = 0;
                beam_v = 0;
                beam_h = 0;
                frame_vsg->set_line_v(beam_v); // move V but not H
                sawdata = false;
                return;
                //break;

            case VM_HSYNC: {
                    lastByte = 0x00; // for hires
                    hcount = 0; if (sawdata) vcount++;
                    beam_h = 0; beam_v++;
                    frame_vsg->set_line(beam_v); // and set H to 0
                    
                    modeChecks = true;

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
            case VM_BORDER_COLOR:
                // emit dots based on scanner_freq pixel clock.
                frame_vsg->push_n(txt_lut[scan.mainbyte & 0x0F], scanner_freq);
                break;
            case VM_SHR: {
                    sawdata = true;
                    scanner_freq = 16;
                    uint8_t p_num = mode.p;
                    uint32_t shr_bytes = scan.shr_bytes; // 4 video bytes in 32 bits.
                    for (int x = 0; x < 4; x++) {
                        uint8_t pval = shr_bytes & 0xFF;
                        shr_bytes >>= 8;

                        if (mode.mode640) { // each byte is 4 pixels
                            frame_vsg->push(palette.active[pixel640<3>(pval) + 0x8]);                            
                            frame_vsg->push(palette.active[pixel640<2>(pval) + 0x0C]);            
                            frame_vsg->push(palette.active[pixel640<1>(pval) + 0x00]);            
                            frame_vsg->push(palette.active[pixel640<0>(pval) + 0x04]);
                        } else {
                            uint16_t pixel = pixel320<1>(pval);
                            if (!mode.fill || (pixel != 0)) {
                                lastpixel = palette.active[pixel];
                            }
                            frame_vsg->push_n(lastpixel, 2);

                            pixel = pixel320<0>(pval);
                            if (!mode.fill || (pixel != 0)) {
                                lastpixel = palette.active[pixel];
                            }
                            frame_vsg->push_n(lastpixel, 2);
                        }
                    }
                }
                break;
            case VM_SHR_MODE: {
                    mode.v = scan.mainbyte;
                    palette_index = 0;
                }
                break;
            case VM_SHR_PALETTE: { // load the palette values into palette based on index.
                    palette.colors[palette_index].v = scan.shr_bytes & 0xFFFF;
                    palette.colors[palette_index+1].v = (scan.shr_bytes >> 16) & 0xFFFF;
                    if (mono_mode) {
                        palette.active[palette_index] = convert24bitColorToMono(convert12bitTo24bit(palette.colors[palette_index]));
                        palette.active[palette_index+1] = convert24bitColorToMono(convert12bitTo24bit(palette.colors[palette_index+1]));
                    } else {
                        palette.active[palette_index] = convert12bitTo24bit(palette.colors[palette_index]);
                        palette.active[palette_index+1] = convert12bitTo24bit(palette.colors[palette_index+1]);
                    }
                    palette_index = (palette_index + 2) % 16;
                }
                break;
            case VM_TEXT40: {
                    sawdata = true;
                    scanner_freq = 14;
                    
                    char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);

                    uint8_t tchar = scan.mainbyte;
                    uint8_t invert;
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    uint8_t tc = (scan.shr_bytes & 0xF0) >> 4; // fg color
                    uint8_t td = (scan.shr_bytes & 0x0F);      // bg color
                    uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    cdata ^= invert;

                    for (int n = 0; n < 7; n++) { // it's ok compiler will unroll this
                        /* const RGBA_t px = txt_lut[(cdata & 1) ? tc : td];
                        
                        frame_vsg->push(px);
                        frame_vsg->push(px); */
                        frame_vsg->push_n(txt_lut[(cdata & 1) ? tc : td], 2);
                        /* frame_vsg->push(txt_lut[(cdata & 1) ? tc : td]); 
                        frame_vsg->push(txt_lut[(cdata & 1) ? tc : td]);  */
                        cdata>>=1;
                        
                    }
                }
                hcount++;
                break;
            case VM_TEXT80: {
                    sawdata = true;
                    scanner_freq = 14;

                    uint8_t invert;

                    uint8_t tchar = scan.auxbyte;

                    char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);
                    uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
    
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    cdata ^= invert;
                    uint8_t tc = (scan.shr_bytes & 0xF0) >> 4;
                    uint8_t td = (scan.shr_bytes & 0x0F);

                    for (int n = 0; n < 7; n++) {
                        frame_vsg->push(txt_lut[(cdata & 1) ? tc : td]); cdata>>=1;
                    }

                    tchar = scan.mainbyte;
                    cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    cdata ^= invert;
                    tc = (scan.shr_bytes & 0xF0) >> 4;
                    td = (scan.shr_bytes & 0x0F);
                    for (int n = 0; n < 7; n++) {
                        frame_vsg->push(txt_lut[(cdata & 1) ? tc : td]); cdata>>=1;
                    }
                }
                hcount++;
                break;
               
            case VM_LORES_7M: {
                    constexpr char pixtable[2][4] = {
                        { 0, 3, 12, 15 },
                        { 0, 12, 3, 15 }
                    };
                    sawdata = true;
                    scanner_freq = 14;

                    uint8_t tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    int ph = (hcount & 1) << 1;
                    int pix = (tchar >> ph) & 0b11;
                    tchar = pixtable[ph/2][pix]; // remap 
                    
                    RGBA_t color = txt_lut[tchar];
    
                    frame_vsg->push_n(color, CELL_WIDTH);
                    /* for (size_t bits = 0; bits < CELL_WIDTH; bits++) {
                        frame_vsg->push(color);
                    } */
                }
                hcount++;
                break;

            case VM_LORES: {
                    sawdata = true;
                    scanner_freq = 14;

                    uint8_t tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    
                    const RGBA_t color = txt_lut[tchar & 0x0F];
    
                    frame_vsg->push_n(color, CELL_WIDTH);
                    /* for (uint32_t bits = 0; bits < CELL_WIDTH; bits++) {
                        frame_vsg->push(color);
                    } */
                }
                hcount++;
                break;
            case VM_DLORES: {
                    sawdata = true;
                    scanner_freq = 14;
                    
                    // To reproduce a motherboard color, store the motherboard color, rotated right one bit, in aux card ram. - Sather IIe 8-29
                    // So we want to rotate LEFT one bit to convert this to the mobo color.
                    uint8_t tchar = scan.auxbyte;

                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    tchar = (tchar << 1) | ((tchar & 0x08) >> 3);                    

                    RGBA_t color = txt_lut[tchar & 0x0F];
                    frame_vsg->push_n(color, 7);
                    /* for (size_t bits = 0; bits < 7; bits++) {
                        frame_vsg->push(color);
                    }*/

                    tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    
                    color = txt_lut[tchar & 0x0F];
                    frame_vsg->push_n(color, 7);
                    /* for (size_t bits = 0; bits < 7; bits++) {
                        frame_vsg->push(color);
                    } */       
                }
                hcount++;
                break;

            case VM_HIRES_NOSHIFT:
                color_delay_mask = 0x7F;

            case VM_HIRES: {
                    sawdata = true;
                    scanner_freq = 14;
                    if (hcount == 0) {
                        phase_offset = 0; // 0 or 1
                        hires_start();
                    }
                    uint8_t byte = scan.mainbyte & color_delay_mask;
                    add_hires_bits(byte);
                    //render_hires((hcount == 0), (hcount == 39));
                    render_hires();
                }
                color_delay_mask = 0xFF;
                hcount++;
                break;

            case VM_DHIRES: {
                    sawdata = true;
                    scanner_freq = 14;
                    if (hcount == 0) {
                        phase_offset = 1; // 0 or 1
                        hires_start();
                    }
                        
                    add_dhires_bits(scan.mainbyte, scan.auxbyte);
                    if (dhgr_mono_mode) render_hires_mono();
                    else //render_hires((hcount == 0), (hcount == 39));
                        render_hires();
                }
                hcount++;
                break;

            default:
                assert(false); // should not get here.
                break;
        }
        beam_h++;
    }
}


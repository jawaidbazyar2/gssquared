
#include "Device_ID.hpp"
#include "AppleIIgsColors.hpp"

//#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator_Comp.hpp"
#include "generate/AppleIIgs.hpp"
#include "util/printf_helper.hpp"

// Alias to the shared Apple IIgs color table for text rendering
//static const RGBA_t (&gs_text_palette)[16] = AppleIIgs::TEXT_COLORS;

VideoScanGenerator_Comp::VideoScanGenerator_Comp(CharRom *charrom, bool border_enabled, FrameVSG *frame_vsg)
{
    build_patterns(true);
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

    mono_mode = false;

    frame_bits = new FrameBits();
    frame_bits->clear();
    select_line(0);
}

void VideoScanGenerator_Comp::build_patterns(bool delayEnabled)
{
    for (uint32_t c = 0; c < 128; c++) {
        uint16_t p = 0;
        for (uint32_t n = 0; n < 7; n++) {
            if ((c >> n) & 1) p |= 0b11 << (n * 2);
        }
        dbl7[c] = p;
    }

    // lores dot rotation starts at (hcount * 14) % 4, which is 0 or 2.
    for (uint32_t nib = 0; nib < 16; nib++) {
        for (uint32_t odd = 0; odd < 2; odd++) {
            uint32_t off = odd ? 2 : 0;
            uint16_t p14 = 0, p7m = 0, p7 = 0;
            for (uint32_t i = 0; i < 14; i++) {
                if ((nib >> ((off + i) & 3)) & 1) p14 |= 1 << i;
            }
            for (uint32_t i = 0; i < 7; i++) {
                if ((nib >> ((off + i) & 3)) & 1) {
                    p7m |= 0b11 << (i * 2);
                    p7 |= 1 << i;
                }
            }
            lores14[nib][odd] = p14;
            lores7m[nib][odd] = p7m;
            lo7[nib][odd] = p7;
        }
    }

    for (int i = 0; i < 2 * CHAR_NUM; i++)
    {
        uint8_t value = (i & 0x7f) << 1 | (i >> 8);
        bool delay = delayEnabled && (i & 0x80);
        uint16_t p = 0;
        for (int x = 0; x < CELL_WIDTH; x++)
        {
            if ((value >> ((x + 2 - delay) >> 1)) & 0x1) p |= 1 << x;
        }
        hires14[i] = p;
    }
}



void VideoScanGenerator_Comp::generate_frame(ScanBuffer *frame_scan)
{
    /* mode = { .p = 0 }; 
    palette = { .colors = {0} };
    lastpixel = {0};

    hcount = 0;
    vcount = 0;
    
    lastByte = 0x00; // for hires
    
    color_delay_mask = 0xFF;

    palette_index = 0; // reset to 0 each scanline.
    modeChecks = true; */

    uint64_t fcnt = frame_scan->get_count();
/*     if (fcnt == 0) {
        printf("Warning: no data in ScanBuffer\n");
        return;
    } */
    
    /* if (dump_next_frame) {
        saveScanBufferToFile(frame_scan, "scan_buffer.txt");
        dump_next_frame = false;
    } */

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
                //frame_vsg->advance(scanner_freq); // advance by scanner_freq pixels
                break;
            case VM_VSYNC:  // end of frame
                    flush_line();
                    vcount = 0;
                    beam_v = 0;
                    beam_h = 0;
                    select_line(vcount);
                    sawdata = false;
                    fcnt=0;
                    break;

            case VM_HSYNC: {
                    flush_line();
                    lastByte = 0x00; // for hires
                    hcount = 0; if (sawdata) vcount++;
                    beam_h = 0; beam_v++;
                    select_line(vcount);
                    if (cur) frame_bits->clear_line(vcount);
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
                //frame_vsg->push_n(txt_lut[scan.mainbyte & 0x0F], scanner_freq);
                break; // we don't increment hcount here.
#if 0                
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
#endif                
            case VM_TEXT40: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        color_mode_t cmode = color_mode;
                        cmode.phase_offset = 0;
                        start_line(cmode);
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
                    /* uint8_t tc = (scan.shr_bytes & 0xF0) | 1;
                    uint8_t td = (scan.shr_bytes & 0x0F) << 4; */
                    uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    cdata ^= invert;

                    emit14(dbl7[cdata & 0x7F]);
                }
                hcount++;
                break;
            case VM_TEXT80: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        color_mode_t cmode = color_mode;
                        cmode.phase_offset = 1;
                        start_line(cmode);
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
                    uint32_t aux7 = cdata & 0x7F;

                    tchar = scan.mainbyte;
                    cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                    if (char_rom->is_flash(tchar)) {
                        invert = flash_state ? 0xFF : 0x00;
                    } else {
                        invert = 0x00;
                    }
                    cdata ^= invert;
                    emit14(aux7 | (uint32_t)(cdata & 0x7F) << 7);
                }
                hcount++;
                break;
            case VM_LORES_7M: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 0};
                        start_line(cmode);
                    }
                    uint8_t tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    emit14(lores7m[tchar & 0x0F][hcount & 1]);
                }
                hcount++;
                break;
            case VM_LORES: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 0};
                        start_line(cmode);
                    }
                    uint8_t tchar = scan.mainbyte;
                    
                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        tchar = tchar >> 4;
                    }
                    emit14(lores14[tchar & 0x0F][hcount & 1]);
                }
                hcount++;
                break;
            case VM_DLORES: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 1};
                        start_line(cmode);
                    }
                
                    uint8_t auxc = scan.auxbyte;
                    uint8_t mainc = scan.mainbyte;

                    if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                        auxc >>= 4;
                        mainc >>= 4;
                    }
                    // both halves start at the same rotation.
                    uint32_t odd = hcount & 1;
                    emit14(lo7[auxc & 0x0F][odd] | (uint32_t)lo7[mainc & 0x0F][odd] << 7);
                }
                hcount++;
                break;
            
            case VM_HIRES_NOSHIFT: // TODO: this needs to set a flag to disable color delay. But this should make it display something now.
                color_delay_mask = 0x7F;

            case VM_HIRES: {
                    sawdata = true;
                    scanner_freq = 14;
                    if (hcount == 0) {
                        color_mode_t cmode = {1,0, 0};
                        start_line(cmode);
                    }
                    uint8_t byte = scan.mainbyte & color_delay_mask;
                    emit14(hires14[byte | ((lastByte & 0x40) << 2)]); // bit 6 from last byte selects 2nd half of table
                    lastByte = byte;
                }
                color_delay_mask = 0xFF;
                hcount++;
                break;
            case VM_DHIRES: {
                    sawdata = true;
                    scanner_freq = 14;

                    if (hcount == 0) {
                        // dhgr starts at horz offset 0
                        color_mode_t cmode = {1,0, 1};
                        // TODO: this is where we insert colorburst off to implement the IIgs monochrome modes.
                        if (dhgr_mono_mode) cmode.colorburst = 0; // this is sufficient for NTSC. but rgb.. 
                        start_line(cmode);
                    }
                    emit14((scan.auxbyte & 0x7F) | (uint32_t)(scan.mainbyte & 0x7F) << 7);
                }
                hcount++;
                break;
            default:
                break;
        }
    }

    // TODO: insert here call to the selected renderer.
    // save our hloc/scanline.
    // call renderer
    render->render(*frame_bits, frame_vsg);
    // restore hloc/scanline.
}


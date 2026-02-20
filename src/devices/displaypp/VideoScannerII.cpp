
#include "VideoScannerII.hpp"
#include "cpu.hpp"
#include "frame/Frames.hpp"
#include "ScanBuffer.hpp"

void VideoScannerII::init_video_addresses()
{
    allocate();
    init_mode_table();
    
    printf("II+ init_video_addresses()\n"); fflush(stdout);

    uint32_t hcount = 0;     // beginning of right border
    uint32_t vcount = 0x100; // first scanline at top of screen

    for (int idx = 0; idx < SCANNER_LUT_SIZE; ++idx)
    {
        // hcount and vcount here are as they are defined inside the Apple II. They do not directly correspond to
        // the hcount and vcount values used in the video_cycle() function.

        // A2-A0 = H2-H0
        uint32_t A2toA0 = hcount & 7;

        // A6-A3
        uint32_t V3V4V3V4 = ((vcount & 0xC0) >> 1) | ((vcount & 0xC0) >> 3);
        uint32_t A6toA3 = (0x68 + (hcount & 0x38) + V3V4V3V4) & 0x78;

        // A9-A7 = V2-V0
        uint32_t A9toA7 = (vcount & 0x38) << 4;

        // A15-A10
        uint32_t HBL = (hcount < 0x58);
        uint32_t LoresA15toA10 = 0x400 | (HBL << 12);
        uint32_t HiresA15toA10 = (0x2000 | ((vcount & 7) << 10));

        uint32_t lores_address = A2toA0 | A6toA3 | A9toA7 | LoresA15toA10;
        uint32_t hires_address = A2toA0 | A6toA3 | A9toA7 | HiresA15toA10;

        bool mixed_mode_text = (vcount >= 0x1A0 && vcount < 0x1C0) || (vcount >= 0x1E0) || (vcount < 0x100);

        lores_p1[idx].addr = lores_address;
        lores_p2[idx].addr = lores_address + 0x400;

        hires_p1[idx].addr = hires_address;
        hires_p2[idx].addr = hires_address + 0x2000;

        if (mixed_mode_text) {
            mixed_p1[idx].addr = lores_address;
            mixed_p2[idx].addr = lores_address + 0x400;
        }
        else {
            mixed_p1[idx].addr = hires_address;
            mixed_p2[idx].addr = hires_address + 0x2000;
        }

        if (hcount) {
            hcount = (hcount + 1) & 0x7F;
            if (hcount == 0) {
                vcount = (vcount + 1) & 0x1FF;
                if (vcount == 0)
                    vcount = 0xFA;
            }
        }
        else {
            hcount = 0x40;
        }

        // set the flags for the current scan address
        uint16_t fl = 0;
        uint32_t hc = idx % 65;
        uint32_t vc = idx / 65;
        if (hc < 25) fl |= SA_FLAG_HBL;
        if (vc >= 192) fl |= SA_FLAG_VBL;
        // hc=0 vc=0 here is upper left pixel of data display.
        if ((hc == 64) && (vc < 261)) fl |= SA_FLAG_HSYNC;
        if ((hc == 64) && (vc == 261)) fl |= SA_FLAG_VSYNC;

        lores_p1[idx].flags = fl;
        lores_p2[idx].flags = fl;
        hires_p1[idx].flags = fl;
        hires_p2[idx].flags = fl;
        mixed_p1[idx].flags = fl;
        mixed_p2[idx].flags = fl;
    }
}

void VideoScannerII::init_mode_table() {
    for (int i = 0; i < 256; i++) {
        mode_table[i] = calc_video_mode_x(i);
    }
}

// calculate video mode and table based on the vmode flags
mode_table_t VideoScannerII::calc_video_mode_x(uint8_t xvmode)
{
    bool xgraf = xvmode & F_GRAF;
    bool xhires = xvmode & F_HIRES;
    bool xpage2 = xvmode & F_PAGE2;
    bool xsw80col = xvmode & F_80COL;
    bool xdblres = xvmode & F_DBLRES;
    bool xmixed = xvmode & F_MIXED;
    bool xf80store = xvmode & F_80STORE;
    bool xshr = xvmode & F_SHR;
    
    mode_table_t mt;
    mt.vaddr = nullptr;
    // set a reasonable default for invalid vmodes
    mt.mode = VM_TEXT40;
   /*  if (xpage2) {
        mt.vaddr = lores_p2;
    } else {
        mt.vaddr = lores_p1;
    } */

    if (xshr) {
        mt.mode = VM_SHR;
    } else if (xgraf) {
        if (xhires) {
            if (xdblres) {
                if (xsw80col)
                    mt.mode = VM_DHIRES;
                else
                    mt.mode = VM_HIRES_NOSHIFT;
            }
            else
                mt.mode = VM_HIRES;
        } else {
            if (xdblres) {
                if (xsw80col)
                    mt.mode = VM_DLORES;
                else
                    mt.mode = VM_LORES; // is there a "noshift" lores?
            }
            else {
                mt.mode = VM_LORES;
            }
        }
    } else if (xsw80col) {
        mt.mode = VM_TEXT80;
    } else {
        mt.mode = VM_TEXT40;
    }

    if (mt.mode == VM_SHR) {
        mt.vaddr = shr_p1;
    } else if (mt.mode == VM_TEXT40 || mt.mode == VM_TEXT80 || mt.mode == VM_LORES || mt.mode == VM_DLORES) { // text modes, page 1 and 2
        if (xpage2 && !xf80store) {
            mt.vaddr = lores_p2;
        } else {
            mt.vaddr = lores_p1;
        }
    } else { // a hires graphics mode.
        // Experiment shows that when **80STORE** is on, the video scanner is forced to page1.
        // this should be checking 80STORE, not 80COL.
        if (xmixed) { // if mixed.. 
            if (xpage2 && !xf80store) { // page 2 / 1
                mt.vaddr = mixed_p2;
            } else {
                mt.vaddr = mixed_p1;
            }
        } else { // not mixed..
            if (xpage2 && !xf80store) {
                mt.vaddr = hires_p2;
            } else {
                mt.vaddr = hires_p1;
            }    
        }
    }
    if (mt.vaddr == nullptr) {
        printf("Warning: invalid video mode: %d\n", xvmode); // ensure we set a vaddr etc for every vmode.
    }
    return mt;
}

void VideoScannerII::set_video_mode()
{
    vmode = 0;
    vmode |= f_80store ? F_80STORE : 0;
    vmode |= graf ? F_GRAF : 0;
    vmode |= hires ? F_HIRES : 0;
    vmode |= page2 ? F_PAGE2 : 0;
    vmode |= sw80col ? F_80COL : 0;
    vmode |= dblres ? F_DBLRES : 0;
    vmode |= mixed ? F_MIXED : 0;
    vmode |= shr ? F_SHR : 0;

    mode_table_t &mode = mode_table[vmode];

    video_addresses = mode.vaddr;
    
    // Validate mode.mode is within valid enum range (0-13) before assigning to enum
    // This prevents UBSan "INVALID ENUM LOAD" warnings
    // video_mode = (video_mode_t)mode.mode;
    uint8_t mode_val = mode.mode;
    if (mode_val > static_cast<uint8_t>(VM_LAST_HBL)) {
        mode_val = static_cast<uint8_t>(VM_TEXT40); // fallback to safe default
    }
    video_mode = static_cast<video_mode_t>(mode_val);

    uint8_t flags = 0;
    if (altchrset) flags |= VS_FL_ALTCHARSET;
    if (mixed) flags |= VS_FL_MIXED;
    if (sw80col) flags |= VS_FL_80COL;
    mode_flags = flags;
}

void VideoScannerII::video_cycle()
{
    scan_address_t &sa = video_addresses[scan_index];

    uint16_t address = sa.addr;

    video_byte = ram[address];
    mmu->set_floating_bus(video_byte);

    Scan_t scan;
    if (!(sa.flags & SA_FLAG_BLANK)) {
        scan.mode = (uint8_t)video_mode;
        scan.auxbyte = 0x00;
        scan.mainbyte = video_byte;
        scan.flags = mode_flags;
        scan.shr_bytes = text_color;
        frame_scan->push(scan);
    }
    if (sa.flags & SA_FLAG_VSYNC) {
        scan.mode = (uint8_t)VM_VSYNC;
        scan.mainbyte = 0;
        scan.flags = mode_flags;
        frame_scan->push(scan);
    }
    if (sa.flags & SA_FLAG_HSYNC) {
        scan.mode = (uint8_t)VM_HSYNC;
        scan.mainbyte = 0;
        scan.flags = mode_flags;
        frame_scan->push(scan);
    }
    if (++scan_index == 17030) {
        scan_index = 0;
    }
}

ScanBuffer *VideoScannerII::get_frame_scan()
{
    return frame_scan;
}

VideoScannerII::VideoScannerII(MMU_II *mmu)
{

    this->mmu = mmu;
    this->ram = mmu->get_memory_base();

    // Note: init_video_addresses() is not called here because it's virtual
    // and needs to be called from derived class constructors to ensure
    // the correct derived implementation is used
    //init_mode_table();

    frame_scan = new ScanBuffer;

    text_bg = 0x00;
    text_fg = 0x0F;
    text_color = 0xF0;
    border_color = 0x00;
    
    // set initial video mode: text, lores, not mixed, page 1
    graf  = false;
    hires = false;
    mixed = false;
    page2 = false;
    sw80col   = false;
    altchrset = false;
    dblres    = false;
    f_80store = false;
    shr = false;
    set_video_mode();

    video_byte = 0;
    //video_data_size = 0;

    reset();
    //hcount = 64;   // will increment to zero on first video scan
    //vcount = 261;  // will increment to 262 on first video scan and "reset"

    /*
    ** Explanation of hcount and vcount initialization **
    The lines of the video display that display video data are conceptually lines
    0-191 (legacy modes) or 0-199 (SHR modes). The lines after that, up to and
    including line 220 are the bottom colored border on the IIgs. Lines 221-242
    are undisplayed lines including the vertical sync. Lines 243-261 are the top
    colored border on the IIgs. Each line is considered to begin (with hcount == 0)
    at the beginning of the right border of the screen. hcount values 0-6 are the
    right colored border of the IIgs. hcount values 7-18 are undisplayed states
    including the horizontal sync. hcount values 19-24 are the left colored border
    of the IIgs. hcount values 25-64 are used to display video data.
    The hcount and vcount initialization values above are chosen so that the video
    scanner (which does not produce data for undisplayed hcount/vcount values)
    will produce video data for the current frame starting at the beginning of the
    top border.
    */
}


void VideoScannerII::allocate()
{
    lores_p1 = new scan_address_t[cycles_per_frame];
    lores_p2 = new scan_address_t[cycles_per_frame];
    hires_p1 = new scan_address_t[cycles_per_frame];
    hires_p2 = new scan_address_t[cycles_per_frame];
    mixed_p1 = new scan_address_t[cycles_per_frame];
    mixed_p2 = new scan_address_t[cycles_per_frame];
    shr_p1 = new scan_address_t[cycles_per_frame];
    video_addresses = lores_p1;
}

/*
 * *** NEEDS UPDATE ***
 *
 * Here's what's going on in the function init_video_addresses()
 *
 *     In the Apple II, the display is generated by the video subsystem.
 * Central to the video subsytem are two counters, the horizontal counter
 * and the vertical counter. The horizontal counter is 7 bits and can be
 * in one of 65 states, with values 0x00, then 0x40-0x7F. Of these 65
 * possible states, values 0x58-0x7F represent the 40 1-Mhz clock periods
 * in which video data is displayed on a scan line. Horizontal blanking and
 * horizontal sync occur during the other 25 states (0x00,0x40-0x57).
 *     The vertical counter is 9 bits and can be in one of 262 states, with
 * values 0xFA-0x1FF. Of these 262 possible states, values 0x100-0x1BF
 * represent the 192 scan lines in which video data is displayed on screen.
 * Vertical blanking and vertical sync occur during the other 70 states
 * (0x1C0-0x1FF,0xFA-0xFF).
 *     The horizontal counter is updated to the next state every 1-Mhz cycle.
 * If it is in states 0x40-0x7F it is incremented. Incrementing the horizontal
 * counter from 0x7F wraps it around to 0x00. If the horizontal counter is in
 * state 0x00, it is updated by being set to 0x40 instead of being incremented.
 *     The vertical counter is updated to the next state every time the
 * horizontal counter wraps around to 0x00. If it is in states 0xFA-0x1FF,
 * it is incremented. If the vertical counter is in state 0x1FF, it is updated
 * by being set to 0xFA instead of being incremented.
 *     The bits of the counters can be labaled as follows:
 * 
 * horizontal counter:        H6 H5 H4 H3 H2 H1 H0
 *   vertical counter:  V5 V4 V3 V2 V1 V0 VC VB VA
 *      most significant ^                       ^ least significant
 * 
 *     During each 1-Mhz cycle, an address is formed as a logical combination
 * of these bits and terms constructed from the values of the soft switches
 * TEXT/GRAPHICS, LORES/HIRES, PAGE1/PAGE2, MIXED/NOTMIXED. The video subsytem
 * reads the byte at this address from RAM and, if the counters are in states
 * that correspond to video display times, use the byte to display video on
 * screen. How this byte affects the video display depends on the current video
 * mode as set by the soft switches listed above. If the counters are in states
 * that correspond to horizontal or vertical blanking times, a byte is still
 * read from the address formed by the video subsystem but it has no effect
 * on the display. However, the byte most recently read by the video subsystem,
 * whether it affects the video display or not, can be obtained by a program
 * by reading an address that does not result in data being driven onto the
 * data bus. That is, by reading an address that does not correspond to any
 * RAM/ROM or peripheral register.
 *     The address read by the video subsystem in each cycle consists of 16
 * bits labeled A15 (most significant) down to A0 (least significant) and is
 * formed as described below:
 * 
 * The least signficant 3 bits are just the least signifcant 3 bits of the
 * horizontal counter:
 * 
 *     A0 = H0, A1 = H1, A2 = H2
 * 
 * The next 4 bits are formed by an arithmetic sum involving bits from the
 * horizontal and vertical counters:
 *
 *       1  1  0  1
 *         H5 H4 H3
 *   +  V4 V3 V4 V3
 *   --------------
 *      A6 A5 A4 A3
 * 
 * The next 3 bits are just bits V0-V2 of the vertical counter (nb: these
 * are NOT the least signficant bits of the vertical counter).
 * 
 *     A7 = V0, A8 = V1, A9 = V2
 * 
 * The remaining bits differ depending on whether hires graphics has been
 * selected or not and whether, if hires graphics has been selected, mixed
 * mode is on or off, and whether, if mixed mode is on, the vertical counter
 * currently corresponds to a scanline in which text is to be displayed.
 * 
 * If hires graphics IS NOT currently to be displayed, then
 * 
 *     A10 = PAGE1  : 1 if page one is selected, 0 if page two is selected
 *     A11 = PAGE2  : 0 if page one is selected, 1 if page two is selected
 *     A12 = HBL    : 1 if in horizontal blanking time (horizontal counter
 *                    states 0x00,0x40-0x57), 0 if not in horizontal blanking
 *                    time (horizontal counter states 0x58-0x7F)
 *     A13 = 0
 *     A14 = 0
 *     A15 = 0
 * 
 *  If hires graphics IS currently to be displayed, then
 *
 *     A10 = VA     : the least significant bit of the vertical counter
 *     A11 = VB
 *     A12 = VC
 *     A13 = PAGE1  : 1 if page one is selected, 0 if page two is selected
 *     A14 = PAGE2  : 0 if page one is selected, 1 if page two is selected
 *     A15 = 0
 * 
 * The code in mega_ii_cycle implements the horizontal and vertical counters
 * and the formation of the video scan address as described above in a fairly
 * straightforward way. The main complication is how mixed mode is handled.
 * When hires mixed mode is set on the Apple II, the product V2 & V4 deselects
 * hires mode. V2 & V4 is true (1) during vertical counter states 0x1A0-0x1BF
 * and 0x1E0-0x1FF, corresponding to scanlines 160-191 (the text window at
 * the bottom of the screen in mixed mode) and scanlines 224-261 (undisplayed
 * lines in vertical blanking time). This affects how bits A14-A10 of the
 * address is formed.
 * 
 * These bits are formed in the following way: Three variables (data members)
 * called page_bit, lores_mode_mask and hires_mode_mask are established which
 * store the current video settings. If page one is selected, page_bit = 0x2000.
 * If page two is selected, page_bit = 0x4000, which in both cases places a set
 * bit at the correct location for forming the address when displaying hires
 * graphics. If hires graphics is selected, hires_mode_mask = 0x7C00, with the
 * set bits corresponding to A14-A10, and lores_mode_mask = 0x0000. If hires
 * graphics is not selected, hires_mode_mask = 0x0000, and lores_mode_mask =
 * 0x1C00, with the set bits corresponding to A12-A10. Thus, these two variables
 * are masks for the high bits of the address in each mode.
 * 
 * For each cycle, a variable HBL is given the value 0x1000 if the horizontal
 * count < 0x58, and the value 0x0000 if the horizontal count >= 0x58, then
 * values for bits A14-A10 are formed for both the hires and not-hires cases.
 * If there were no mixed mode, the values would be formed like this:
 * 
 *     Hires A15-A10 = (page_bit | (vertical-count << 10)) & hires_mode_mask
 *     Lores A15-A10 = ((page_bit >> 3) | HBL) & lores_mode_mask
 * 
 * Since the variables hires_mode_bits and lores_mode_bits are masks, they
 * ensure that at all times, one of these values is zero, and the other is the
 * correct high bits of the address scanned by the video system.
 * 
 * To implement mixed mode, two more variables are introduced: mixed_lores_mask
 * and mixed_hires_mask. If mixed mode is not selected, or if mixed mode is
 * selected but V2 & V4 == 0, then mixed_lores_mask = 0x0000 and mixed_hires_mask
 * = 0x7C00. If mixed mode is selected and (V2 & V4) == 1, then mixed_lores_mask
 * = 0x1C00 and mixed_hires_mask = 0x0000.
 * 
 * Combined masks for the high bits of both text/lores and hires are then formed
 * 
 *     combined_lores_mask = mixed_lores_mask | lores_mode_mask
 *     combined_hires_mask = mixed_hires_mask & hires_mode_mask
 * 
 * This indicates that a text/lores address will be generated if either
 * text/lores mode is selected (lores_mode_mask != 0) OR if hires mixed mode
 * is selected and the vertical counter specifies a text mode scanline
 * (mixed_lores_mask != 0). A hires address will be generated if hires mode is
 * selected (hires_mode_mask != 0) AND the vertical counter does not specify a
 * text mode scanline (mixed_hires_mask != 0).
 * 
 * The high bits of the address generated by the video scanner for both
 * text/lores and hires mode are then formed
 * 
 *     Hires A15-A10 = (page_bit | (vertical-count << 10)) & combined_hires_mask
 *     Lores A15-A10 = ((page_bit >> 3) | HBL) & combined_lores_mask
 * 
 * The result is that, for each cycle, either Hires A15-A10 is nonzero or
 * Lores A15-A10 is nonzero, depending on the selected graphics mode and the
 * state of the vertical counter.
 *
 * Finally the address to be scanned is formed by summing the individual parts
 * A2-A0, A6-A3, A9-A7, A15-A10.
 */

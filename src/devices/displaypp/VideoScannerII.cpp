
#include "VideoScannerII.hpp"
#include "cpu.hpp"
#include "frame/Frames.hpp"
#include "CharRom.hpp"

void VideoScannerII::init_video_addresses()
{
    printf("II+ init_video_addresses()\n"); fflush(stdout);

    uint32_t hcount = 0;     // beginning of right border
    uint32_t vcount = 0x100; // first scanline at top of screen

    for (int idx = 0; idx < 65*262; ++idx)
    {
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

        bool mixed_mode_text = (vcount >= 0x1A0 && vcount < 0x1C0) || (vcount >= 0x1E0);

        lores_p1_addresses[idx] = lores_address;
        lores_p2_addresses[idx] = lores_address + 0x400;

        hires_p1_addresses[idx] = hires_address;
        hires_p2_addresses[idx] = hires_address + 0x2000;

        if (mixed_mode_text) {
            mixed_p1_addresses[idx] = lores_address;
            mixed_p2_addresses[idx] = lores_address + 0x400;
        }
        else {
            mixed_p1_addresses[idx] = hires_address;
            mixed_p2_addresses[idx] = hires_address + 0x2000;
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
    }
}

void VideoScannerII::set_video_mode()
{
    // Set combined mode and video address LUT
    // assume text/lores addresses until proven otherwise
    if (page2) {
        video_addresses = &(lores_p2_addresses);
    } else {
        video_addresses = &(lores_p1_addresses);
    }

    if (graf) {
        if (hires) {
            if (mixed) {
                video_mode = VM_HIRES_MIXED;
                if (page2) {
                    video_addresses = &(mixed_p2_addresses);
                } else {
                    video_addresses = &(mixed_p1_addresses);
                }
            } else {
                video_mode = VM_HIRES;
                if (page2) {
                    video_addresses = &(hires_p2_addresses);
                } else {
                    video_addresses = &(hires_p1_addresses);
                }
            }
        } else if (mixed) {
            video_mode = VM_LORES_MIXED;
        } else {
            video_mode = VM_LORES;
        }
    } else {
        video_mode = VM_TEXT40;
    }
}

void VideoScannerII::video_cycle()
{
    uint32_t prev_vcount = vcount;

    hcount += 1;
    if (hcount == 65) {
        hcount = 0;
        vcount += 1;
        if (vcount == 262) {
            vcount = 0;
        }
    }

    if (vcount != prev_vcount) {
        if (vcount < 192) frame_scan->set_line(vcount);
    }

    uint16_t address = (*(video_addresses))[65*vcount+hcount];

    //uint8_t * ram = mmu->get_memory_base();
    video_byte = ram[address];

    if (is_vbl()) return; // vcount>=192

    if (hcount < 25) return; // this was off by 1 compared to is_hbl().

    Scan_t scan;
    scan.mode = (uint8_t)video_mode;
    scan.auxbyte = 0;
    scan.mainbyte = video_byte;
    if (hcount < 65) frame_scan->push(scan);
/* 
    if (hcount == 24) {
        video_data[video_data_size++] = (uint8_t)VM_LAST_HBL;
        video_data[video_data_size++] = video_byte;
    }
    else {
        video_data[video_data_size++] = (uint8_t)video_mode;
        video_data[video_data_size++] = video_byte;
    } */
}

FrameScan560 *VideoScannerII::get_frame_scan()
{
    return frame_scan;
}

VideoScannerII::VideoScannerII(uint8_t *ram)
{
    //this->mmu = dynamic_cast<MMU_II *>(mmu);
    this->ram = ram;

    init_video_addresses();

    frame_scan = new FrameScan560(580, 192);

    // set initial video mode: text, lores, not mixed, page 1
    graf  = false;
    hires = false;
    mixed = false;
    page2 = false;
    set_video_mode();

    video_byte = 0;
    video_data_size = 0;

    hcount = 64;   // will increment to zero on first video scan
    vcount = 242;  // will increment to 243 on first video scan

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

uint8_t vs_bus_read_C050(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_graf();
    return vs->get_video_byte();
}

void vs_bus_write_C050(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C050(context, address);
}

uint8_t vs_bus_read_C051(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_text();
    return vs->get_video_byte();
}

void vs_bus_write_C051(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C051(context, address);
}

uint8_t vs_bus_read_C052(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_full();
    return vs->get_video_byte();
}

void vs_bus_write_C052(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C052(context, address);
}


uint8_t vs_bus_read_C053(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_mixed();
    return vs->get_video_byte();
}

void vs_bus_write_C053(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C053(context, address);
}


uint8_t vs_bus_read_C054(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_page_1();
    return vs->get_video_byte();
}
void vs_bus_write_C054(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C054(context, address);
}


uint8_t vs_bus_read_C055(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_page_2();
    return vs->get_video_byte();
}

void vs_bus_write_C055(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C055(context, address);
}

uint8_t vs_bus_read_C056(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_lores();
    return vs->get_video_byte();
}

void vs_bus_write_C056(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C056(context, address);
}

uint8_t vs_bus_read_C057(void *context, uint16_t address)
{
    VideoScannerII *vs = (VideoScannerII *)context;
    vs->set_hires();
    return vs->get_video_byte();
}

void vs_bus_write_C057(void *context, uint16_t address, uint8_t value) {
    vs_bus_read_C057(context, address);
}

/* void init_mb_video_scanner(computer_t *computer, SlotType_t slot)
{
    cpu_state *cpu = computer->cpu;
    
    // alloc and init video scanner
    VideoScannerII * vs = new VideoScannerII(computer->mmu);
    computer->video_scanner = vs;
    printf("Allocated video scanner: %p\n", vs);
    
    computer->mmu->set_C0XX_read_handler(0xC050, { vs_bus_read_C050, vs });
    computer->mmu->set_C0XX_write_handler(0xC050, { vs_bus_write_C050, vs });
    computer->mmu->set_C0XX_read_handler(0xC051, { vs_bus_read_C051, vs });
    computer->mmu->set_C0XX_write_handler(0xC051, { vs_bus_write_C051, vs });
    computer->mmu->set_C0XX_read_handler(0xC052, { vs_bus_read_C052, vs });
    computer->mmu->set_C0XX_write_handler(0xC052, { vs_bus_write_C052, vs });
    computer->mmu->set_C0XX_read_handler(0xC053, { vs_bus_read_C053, vs });
    computer->mmu->set_C0XX_write_handler(0xC053, { vs_bus_write_C053, vs });
    computer->mmu->set_C0XX_read_handler(0xC054, { vs_bus_read_C054, vs });
    computer->mmu->set_C0XX_write_handler(0xC054, { vs_bus_write_C054, vs });
    computer->mmu->set_C0XX_read_handler(0xC055, { vs_bus_read_C055, vs });
    computer->mmu->set_C0XX_write_handler(0xC055, { vs_bus_write_C055, vs });
    computer->mmu->set_C0XX_read_handler(0xC056, { vs_bus_read_C056, vs });
    computer->mmu->set_C0XX_write_handler(0xC056, { vs_bus_write_C056, vs });
    computer->mmu->set_C0XX_read_handler(0xC057, { vs_bus_read_C057, vs });
    computer->mmu->set_C0XX_write_handler(0xC057, { vs_bus_write_C057, vs });
} */

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

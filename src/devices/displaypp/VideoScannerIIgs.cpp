
#include "VideoScannerIIgs.hpp"
#include "VideoScannerII.hpp"
#include "ScanBuffer.hpp"
#include "mmus/mmu_ii.hpp"

void VideoScannerIIgs::init_video_addresses()
{
    printf("IIgs init_video_addresses()\n"); fflush(stdout);

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
        // Big difference IIe vs II is no HBL shifted up to bit 12
        uint32_t LoresA15toA10 = 0x400;
        uint32_t HiresA15toA10 = (0x2000 | ((vcount & 7) << 10));

        uint32_t lores_address = A2toA0 | A6toA3 | A9toA7 | LoresA15toA10;
        uint32_t hires_address = A2toA0 | A6toA3 | A9toA7 | HiresA15toA10;

        bool mixed_mode_text = (vcount >= 0x1A0 && vcount < 0x1C0) || (vcount >= 0x1E0);

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
        if ((hc == 64) && (vc < 261)) fl |= SA_FLAG_HSYNC;
        if ((hc == 64) && (vc == 261)) fl |= SA_FLAG_VSYNC;

        if ( (vc >= 0 and vc < 192) &&  
            (((hc >= 0) && (hc <= 6)) || ((hc >= 19) && (hc <= 24)) )) fl |= SA_FLAG_BORDER;
        if (
            ((vc >= 243 && vc <= 261) || (vc >= 192 && vc <= 220)) && 
            (((hc <= 6) || (hc>= 19)))
        ) fl |= SA_FLAG_BORDER;
        /* if (((vc >= 243 && vc <= 261) || (vc >= 192 && vc <= 220)) && 
            ((hc <= 6 || hc >= 25))) {
            fl |= SA_FLAG_BORDER;
        } */

        // SHR Mode
        // shr buffer is linear. But there are also some cycles where we need to read the palette data from RAM.
        // each cycle in SHR mode grabs 4 bytes from RAM.
        uint16_t shr_addr = 0x2000; // dummy default
        uint8_t shr_fl = fl;
        if (vc < 200) { // shr is 200 scanlines..
            if (hc>=25) { shr_addr = 0x2000 + ((hc-25)*4) + (vc * 160); shr_fl |= SA_FLAG_SHR; } // shr data
            else if (hc == 6) { shr_addr = 0x9D00 + vc; shr_fl |= SA_FLAG_SCB; } // SCB
            else if (hc >= 7 && hc <= 14) { shr_addr = 0x9E00 + ((hc - 7) * 4); shr_fl |= SA_FLAG_PALETTE; } // Palette
        }
        shr_p1[idx].addr = shr_addr;
        shr_p1[idx].flags = shr_fl;

        lores_p1[idx].flags = fl;
        lores_p2[idx].flags = fl;
        hires_p1[idx].flags = fl;
        hires_p2[idx].flags = fl;
        mixed_p1[idx].flags = fl;
        mixed_p2[idx].flags = fl;
    }
    /* int borderpixels = 0;
    for (int idx = 0; idx < SCANNER_LUT_SIZE; ++idx) {
        if (lores_p1[idx].flags & SA_FLAG_BORDER) {
            borderpixels++;
        }
    } */
}

void VideoScannerIIgs::video_cycle()
{
    scan_address_t &sa = video_addresses[scan_index];
    uint16_t address = sa.addr;

    video_byte = ram[address];
    mmu->set_floating_bus(video_byte);

    Scan_t scan;
    if (sa.flags & SA_FLAG_BORDER) {
        scan.mode = (uint8_t)VM_BORDER_COLOR;
        scan.mainbyte = border_color;
        scan.flags = mode_flags;
        frame_scan->push(scan);
    }
    if (sa.flags & SA_FLAG_SHR) {
        scan.mode = (uint8_t)video_mode; // SHR_PIXEL, SHR_PALETTE, SHR_MODE
        scan.shr_bytes = *((uint32_t *)(ram + 0x1'0000 + address));
        frame_scan->push(scan);
    } else if (sa.flags & SA_FLAG_SCB) {
        scan.mode = (uint8_t)VM_SHR_MODE;
        scan.mainbyte = ram[address + 0x10000];
        scan.flags = mode_flags;
        frame_scan->push(scan);
        palette_index = (scan.mainbyte & 0x0F); // store palette index to control next palette read
    } else if (sa.flags & SA_FLAG_PALETTE) {
        scan.mode = (uint8_t)VM_SHR_PALETTE;
        uint32_t eaddr = address + (palette_index * 32) + 0x1'0000;
        scan.shr_bytes = *((uint32_t *)(ram + eaddr));
        scan.flags = mode_flags;
        frame_scan->push(scan);
    } else if (!(sa.flags & SA_FLAG_BLANK)) {
        scan.mode = (uint8_t)video_mode; 
        scan.auxbyte = ram[address + 0x10000];
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


VideoScannerIIgs::VideoScannerIIgs(MMU_II *mmu) : VideoScannerII(mmu)
{
}

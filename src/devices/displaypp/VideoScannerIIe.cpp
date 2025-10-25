
#include "VideoScannerIIe.hpp"
#include "VideoScannerII.hpp"
#include "ScanBuffer.hpp"
#include "mmus/mmu_ii.hpp"

void VideoScannerIIe::init_video_addresses()
{
    printf("IIe init_video_addresses()\n"); fflush(stdout);

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
        uint16_t hc = idx % 65;
        uint16_t vc = idx / 65;
        if (hc < 25) fl |= SA_FLAG_HBL;
        if (vc >= 192) fl |= SA_FLAG_VBL;
       /*  if ( (vc >= 0 and vc <= 191) &&  
            (((hc >= 40) && (hc <= 46)) || ((hc >= 59) && (hc <= 64)) )) fl |= SA_FLAG_BORDER; */

        lores_p1[idx].flags = fl;
        lores_p2[idx].flags = fl;
        hires_p1[idx].flags = fl;
        hires_p2[idx].flags = fl;
        mixed_p1[idx].flags = fl;
        mixed_p2[idx].flags = fl;
    }
}

#if 0
void VideoScannerIIe::video_cycle()
{
    hcount += 1;
    if (hcount == 65) {
        hcount = 0;
        vcount += 1;
        if (vcount == 262) {
            vcount = 0;
            scan_index = 0;
        }
    }

    //uint16_t address = (*(video_addresses))[65*vcount+hcount];
    uint16_t address = video_addresses[scan_index++];

    uint8_t aux_byte = ram[address + 0x10000];
    video_byte = ram[address];
    /* if (mmu) */ mmu->set_floating_bus(video_byte);

    if (is_vbl()) return;
    if (is_hbl()) return;

    Scan_t scan;
    scan.mode = (uint8_t)video_mode;
    scan.auxbyte = aux_byte;
    scan.mainbyte = video_byte;
    scan.flags = mode_flags /* | (colorburst ? VS_FL_COLORBURST : 0) */;
    frame_scan->push(scan);
}
#endif

void VideoScannerIIe::video_cycle()
{
    scan_address_t &sa = video_addresses[scan_index];
    uint16_t address = sa.addr;

    video_byte = ram[address];
    mmu->set_floating_bus(video_byte);

    Scan_t scan;
    /* if (sa.flags & (SA_FLAG_BORDER)) {
        scan.mode = VM_BORDER_COLOR;
        scan.mainbyte = 0;
        scan.flags = mode_flags;
        frame_scan->push(scan);

    } else  */if (!(sa.flags & SA_FLAG_BLANK)) {
        scan.mode = (uint8_t)video_mode;
        scan.auxbyte = ram[address + 0x10000];
        scan.mainbyte = video_byte;
        scan.flags = mode_flags;
        frame_scan->push(scan);
    }

    if (++scan_index == 17030) {
        scan_index = 0;
    }
}


VideoScannerIIe::VideoScannerIIe(MMU_II *mmu) : VideoScannerII(mmu)
{
    //init_video_addresses();

}


#include "VideoScannerIIe.hpp"
#include "VideoScannerII.hpp"

#include "mmus/mmu_ii.hpp"

void VideoScannerIIe::init_video_addresses()
{
    printf("IIe init_video_addresses()\n"); fflush(stdout);

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
        // Big difference IIe vs II is no HBL shifted up to bit 12
        uint32_t LoresA15toA10 = 0x400;
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

void VideoScannerIIe::video_cycle()
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
        if (vcount < 192) {
            frame_scan->set_line(vcount);
            if (video_mode == VM_TEXT40 || video_mode == VM_TEXT80) {
                frame_scan->set_color_mode(vcount, COLORBURST_OFF);
            } else {
                frame_scan->set_color_mode(vcount, COLORBURST_ON);
            }
        }
    }

    uint16_t address = (*(video_addresses))[65*vcount+hcount];

    uint8_t aux_byte = ram[address + 0x10000];
    video_byte = ram[address];
    if (mmu) mmu->set_floating_bus(video_byte);

    if (is_vbl()) return;
    if (hcount < 25) return;

    Scan_t scan;
    scan.mode = (uint8_t)video_mode;
    scan.auxbyte = aux_byte;
    scan.mainbyte = video_byte;
    scan.flags = mode_flags;
    if (hcount < 65) frame_scan->push(scan);
}

VideoScannerIIe::VideoScannerIIe(MMU_II *mmu) : VideoScannerII(mmu)
{
    //init_video_addresses();

}


#include "VideoScannerIIe.hpp"
#include "VideoScannerII.hpp"

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

void VideoScannerIIe::set_video_mode()
{
    // Set combined mode and video address LUT
    // assume text/lores addresses until proven otherwise
    if (page2 && !sw80store) {
        video_addresses = &(lores_p2_addresses);
    } else {
        video_addresses = &(lores_p1_addresses);
    }

    if (graf) {
        if (hires) {
            if (mixed) {
                if (sw80col) {
                    if (dblres) {
                        if (altchrset)
                            video_mode = VM_DHIRES_ALT_MIXED80;
                        else
                            video_mode = VM_DHIRES_MIXED80;
                    }
                    else {
                        if (altchrset)
                            video_mode = VM_HIRES_ALT_MIXED80;
                        else
                            video_mode = VM_HIRES_MIXED80;
                    }
                } else {
                    if (dblres)
                        video_mode = VM_HIRES_NOSHIFT_MIXED;
                    else
                        video_mode = VM_HIRES_MIXED;
                }
                if (page2 && !sw80store) {
                    video_addresses = &(mixed_p2_addresses);
                } else {
                    video_addresses = &(mixed_p1_addresses);
                }
            } else {
                if (dblres) {
                    if (sw80col)
                        video_mode = VM_DHIRES;
                    else
                        video_mode = VM_HIRES_NOSHIFT;
                }
                else
                    video_mode = VM_HIRES;
                if (page2 && !sw80store) {
                    video_addresses = &(hires_p2_addresses);
                } else {
                    video_addresses = &(hires_p1_addresses);
                }
            }
        } else if (mixed) {
            if (sw80col) {
                if (dblres) {
                    if (altchrset)
                        video_mode = VM_DLORES_ALT_MIXED80;
                    else
                        video_mode = VM_DLORES_MIXED80;
                }
                else {
                    if (altchrset)
                        video_mode = VM_LORES_ALT_MIXED80;
                    else
                        video_mode = VM_LORES_MIXED80;
                }
            } else {
                if (altchrset)
                    video_mode = VM_LORES_ALT_MIXED;
                else
                    video_mode = VM_LORES_MIXED;
            }
        } else {
            if (dblres) {
                if (sw80col)
                    video_mode = VM_DLORES;
                else
                    video_mode = VM_LORES; // is there a "noshift" lores?
            }
            else {
                video_mode = VM_LORES;
            }
        }
    } else if (sw80col) {
        if (altchrset)
            video_mode = VM_ALT_TEXT80;
        else
            video_mode = VM_TEXT80;
    } else {
        if (altchrset)
            video_mode = VM_ALT_TEXT40;
        else
            video_mode = VM_TEXT40;
    }

    //printf("Video Mode: %d\n", video_mode);
}

void VideoScannerIIe::video_cycle()
{
    hcount += 1;
    if (hcount == 65) {
        hcount = 0;
        vcount += 1;
        if (vcount == 262) {
            vcount = 0;
        }
    }

    uint32_t address = (*(video_addresses))[65*vcount+hcount];

    //video_byte = mmu->read_raw(address);
    uint8_t * ram = mmu->get_memory_base();
    uint8_t aux_byte = ram[address + 0x10000];
    video_byte = ram[address];

    if (is_vbl()) return;
    if (hcount < 24) return;

    if (hcount == 24) {
        video_data[video_data_size++] = (uint8_t)VM_LAST_HBL;
        video_data[video_data_size++] = video_byte;
        return;
    }

    // I don't really like this.
    bool aux_text = video_mode >= VM_TEXT80 && video_mode <= VM_DHIRES_ALT_MIXED80 && address < 0x2000;
    bool aux_graf = video_mode >= VM_DHIRES && video_mode <= VM_DHIRES_ALT_MIXED80 && address >= 0x2000;
    if ((video_mode == VM_LORES_MIXED80 || video_mode == VM_LORES_ALT_MIXED80) && vcount < 160)
        aux_text = false;

    video_data[video_data_size++] = (uint8_t)video_mode;

    if (aux_text || aux_graf)
        video_data[video_data_size++] = aux_byte;

    video_data[video_data_size++] = video_byte;
}

VideoScannerIIe::VideoScannerIIe(MMU * mmu) : VideoScannerII(mmu)
{
    init_video_addresses();

    sw80col   = false;
    sw80store = false;
    altchrset = false;
    dblres    = false;
}

void vs_bus_write_C00C(void *context, uint16_t address, uint8_t data) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    //printf("--80COL\n"); 
    vs->reset_80col();
}

void vs_bus_write_C00D(void *context, uint16_t address, uint8_t data) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    //printf("++80COL\n");
    vs->set_80col();
}

void vs_bus_write_C00E(void *context, uint16_t address, uint8_t data) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    vs->reset_altchrset();
}

void vs_bus_write_C00F(void *context, uint16_t address, uint8_t data) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    vs->set_altchrset();
}

uint8_t vs_bus_read_C019(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t hibit = vs->is_vbl() ? 0 : 0x80; // This is IIe. IIgs is opposite
    return hibit | (vs->get_video_byte() & 0x7F);
}

uint8_t vs_bus_read_C01A(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t hibit = vs->is_text() ? 0x80 : 0;
    return hibit | (vs->get_video_byte() & 0x7F);
}

uint8_t vs_bus_read_C01B(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t hibit = vs->is_mixed() ? 0x80 : 0;
    return hibit | (vs->get_video_byte() & 0x7F);
}

uint8_t vs_bus_read_C01D(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t vblbit = vs->is_hires() ? 0x80 : 0;
    return vblbit | (vs->get_video_byte() & 0x7F);
}

uint8_t vs_bus_read_C01E(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t hibit = vs->is_altchrset() ? 0x80 : 0;
    return hibit | (vs->get_video_byte() & 0x7F);
}

uint8_t vs_bus_read_C01F(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    uint8_t retval = vs->is_80col() ? 0x80 : 0;
    retval |= (vs->get_video_byte() & 0x7F);
    //printf("??80COL: %2.2x\n", retval); 
    return retval;
}

uint8_t vs_bus_read_C05E(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    vs->set_dblres();
    return vs->get_video_byte();
}

uint8_t vs_bus_read_C05F(void *context, uint16_t address) {
    VideoScannerIIe *vs = (VideoScannerIIe *)context;
    vs->reset_dblres();
    return vs->get_video_byte();
}

void vs_bus_write_C05E(void *context, uint16_t address, uint8_t data) {
    vs_bus_read_C05E(context, address);
}

void vs_bus_write_C05F(void *context, uint16_t address, uint8_t data) {
    vs_bus_read_C05F(context, address);
}

void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot)
{
    cpu_state *cpu = computer->cpu;
    
    // alloc and init video scanner
    VideoScannerIIe * vs = new VideoScannerIIe(computer->mmu);
    computer->video_scanner = vs;
    printf("Allocated video scanner IIe: %p\n", vs);

    computer->mmu->set_C0XX_write_handler(0xC00C, { vs_bus_write_C00C, vs });
    computer->mmu->set_C0XX_write_handler(0xC00D, { vs_bus_write_C00D, vs });
    computer->mmu->set_C0XX_write_handler(0xC00E, { vs_bus_write_C00E, vs });
    computer->mmu->set_C0XX_write_handler(0xC00F, { vs_bus_write_C00F, vs });

    computer->mmu->set_C0XX_read_handler(0xC019, { vs_bus_read_C019, vs });
    computer->mmu->set_C0XX_read_handler(0xC01A, { vs_bus_read_C01A, vs });
    computer->mmu->set_C0XX_read_handler(0xC01B, { vs_bus_read_C01B, vs });
    // C01C is the status of the page2 switch, and is handled by iiememory
    computer->mmu->set_C0XX_read_handler(0xC01D, { vs_bus_read_C01D, vs });
    computer->mmu->set_C0XX_read_handler(0xC01E, { vs_bus_read_C01E, vs });
    computer->mmu->set_C0XX_read_handler(0xC01F, { vs_bus_read_C01F, vs });
    
    // these functions are implemented in VideoScannerII.cpp
    computer->mmu->set_C0XX_read_handler(0xC050, { vs_bus_read_C050, vs });
    computer->mmu->set_C0XX_read_handler(0xC051, { vs_bus_read_C051, vs });
    computer->mmu->set_C0XX_read_handler(0xC052, { vs_bus_read_C052, vs });
    computer->mmu->set_C0XX_read_handler(0xC053, { vs_bus_read_C053, vs });
    // C054, C055 (PAGE2) alter the page2 switch, and are handled by iiememory
    computer->mmu->set_C0XX_read_handler(0xC056, { vs_bus_read_C056, vs });
    computer->mmu->set_C0XX_read_handler(0xC057, { vs_bus_read_C057, vs });

    computer->mmu->set_C0XX_write_handler(0xC050, { vs_bus_write_C050, vs });
    computer->mmu->set_C0XX_write_handler(0xC051, { vs_bus_write_C051, vs });
    computer->mmu->set_C0XX_write_handler(0xC052, { vs_bus_write_C052, vs });
    computer->mmu->set_C0XX_write_handler(0xC053, { vs_bus_write_C053, vs });
    // C054, C055 (PAGE2) alter the page2 switch, and are handled by iiememory
    computer->mmu->set_C0XX_write_handler(0xC056, { vs_bus_write_C056, vs });
    computer->mmu->set_C0XX_write_handler(0xC057, { vs_bus_write_C057, vs });

    // AN3 (double res graphics)
    computer->mmu->set_C0XX_read_handler(0xC05E, { vs_bus_read_C05E, vs });
    computer->mmu->set_C0XX_read_handler(0xC05F, { vs_bus_read_C05F, vs });
    computer->mmu->set_C0XX_write_handler(0xC05E, { vs_bus_write_C05E, vs });
    computer->mmu->set_C0XX_write_handler(0xC05F, { vs_bus_write_C05F, vs });
}

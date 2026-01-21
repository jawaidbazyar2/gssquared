/*
 *   Copyright (c) 2025 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "debug.hpp"

#include "iiememory.hpp"
#include "Module_ID.hpp"
//#include "display/display.hpp"
#include "devices/languagecard/languagecard.hpp" // to get bit flag names

#include "mmus/mmu_iie.hpp"

#include "mbus/KeyboardMessage.hpp"
#include "mbus/MessageBus.hpp"

#include "util/DebugHandlerIDs.hpp"

/**
 * First, handling the "language card" portion or what the IIe manual calls the "Bank Switch RAM".
 * 
 * ROM: C1-CF     ROM + 0x0100 
 * ROM: D0-DF     ROM + 0x1000
 * ROM: E0-FF     ROM + 0x2000 (- 0x3FFF)
 * RAM: D0-DF     Bank 1:  RAM + 0xC000
 * RAM: D0-DF     Bank 2:  RAM + 0xD000
 * RAM: E0-FF              RAM + 0xE000
 * ALT RAM:       add 0x10000 to address 
 */

void bsr_map_memory(iiememory_state_t *lc) {

    uint32_t bankd0offset = (lc->FF_BANK_1 == 1) ? 0xC000 : 0xD000;
    uint32_t banke0offset = 0xE000;
    if (lc->f_altzp) {
        bankd0offset += 0x1'0000; // alternate bank!
        banke0offset += 0x1'0000; // alternate bank!
    }

    //uint8_t *bank = (lc->FF_BANK_1 == 1) ? lc->ram : lc->ram + 0x1000;
    uint8_t *bankd0 = lc->ram + bankd0offset;
    uint8_t *banke0 = lc->ram + banke0offset;
    uint8_t *rom = lc->mmu->get_rom_base();

    const char *bank_d = (lc->FF_BANK_1 == 1) ? "LC_BANK1" : "LC_BANK2";

    /* Map D0 - DF */
    for (int i = 0; i < 16; i++) {
        if (lc->FF_READ_ENABLE) {
            lc->mmu->map_page_read(i + 0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);
        } else { // reads == READ_ROM
        // TODO: this is wrong - needs to somehow know to return to ROM D0/etc wherever that may be.
        // right now hard-code to what we know about the iie rom.
            lc->mmu->map_page_read(i + 0xD0, rom + 0x1000 + (i*GS2_PAGE_SIZE), "SYS_ROM");
        }

        if (!lc->_FF_WRITE_ENABLE) {
            lc->mmu->map_page_write(i+0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);

        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            lc->mmu->map_page_write(i+0xD0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    /* Map E0 - FF */
    for (int i = 0; i < 32; i++) {
        if (lc->FF_READ_ENABLE) {
            lc->mmu->map_page_read(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");

        } else { // reads == READ_ROM
            // TODO: this is wrong - needs to somehow know to return to ROM D0/etc wherever that may be.
            lc->mmu->map_page_read(i+0xE0, rom + 0x2000 + (i * GS2_PAGE_SIZE), "SYS_ROM");
        }

        if (!lc->_FF_WRITE_ENABLE) {
            lc->mmu->map_page_write(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");
        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            lc->mmu->map_page_write(i+0xE0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    if (DEBUG(DEBUG_LANGCARD)) {
        lc->mmu->dump_page_table(0xD0, 0xD0);
        lc->mmu->dump_page_table(0xE0, 0xE0);
    }
}

uint8_t bsr_read_C0xx(void *context, uint32_t address) {
    iiememory_state_t *lc = (iiememory_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard read %04X ", address);

    /** Bank Select */    
    
    if (address & LANG_A3 ) {
        // 1 = any access sets Bank_1
        lc->FF_BANK_1 = 1;
    } else {
        // 0 = any access resets Bank_1
        lc->FF_BANK_1 = 0;
    }

    /** Read Enable */
    
    if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) {
        // 00, 11 - set READ_ENABLE
        lc->FF_READ_ENABLE = 1;
    } else {
        // 01, 10 - reset READ_ENABLE
        lc->FF_READ_ENABLE = 0;
    }

    int old_pre_write = lc->FF_PRE_WRITE;

    /* PRE_WRITE */
    if ((address & 0b00000001) == 1) {  // read 1 or 3
        // 00000001 - set PRE_WRITE
        lc->FF_PRE_WRITE = 1;
    } else {                            // read 0 or 2
        // 00000000 - reset PRE_WRITE
        lc->FF_PRE_WRITE = 0;
    }

    /** Write Enable */
    if ((old_pre_write == 1) && ((address & 0b00000001) == 1)) { // PRE_WRITE set, read 1 or 3
        // 00000000 - reset WRITE_ENABLE'
        lc->_FF_WRITE_ENABLE = 0;
    }
    if ((address & 0b00000001) == 0) { // read 0 or 2, set _WRITE_ENABLE
        // 00000001 - set WRITE_ENABLE'
        lc->_FF_WRITE_ENABLE = 1;
    }

    if (DEBUG(DEBUG_LANGCARD)) printf("FF_BANK_1: %d, FF_READ_ENABLE: %d, FF_PRE_WRITE: %d, _FF_WRITE_ENABLE: %d\n", 
        lc->FF_BANK_1, lc->FF_READ_ENABLE, lc->FF_PRE_WRITE, lc->_FF_WRITE_ENABLE);
   
    /**
     * compose the memory map.
     * in main_ram:
     * 0x0000 - 0xBFFF - straight map.
     * 0xC000 - 0xCFFF - bank 1 extra memory
     * 0xD000 - 0xDFFF - bank 2 extra memory
     * 0xE000 - 0xFFFF - rest of extra memory
     * */

    bsr_map_memory(lc);
    return 0;
}


void bsr_write_C0xx(void *context, uint32_t address, uint8_t value) {
    iiememory_state_t *lc = (iiememory_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard write %04X value: %02X\n", address, value);
    

    /** Bank Select */

    if (address & LANG_A3 ) {
        // 1 = any access sets Bank_1 - read or write.
        lc->FF_BANK_1 = 1;
    } else {
        // 0 = any access resets Bank_1
        lc->FF_BANK_1 = 0;
    }

    /** READ_ENABLE  */
    
    if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) { // write 0 or 3
        // 00, 11 - set READ_ENABLE
        lc->FF_READ_ENABLE = 1;
    } else {
        // 01, 10 - reset READ_ENABLE
        lc->FF_READ_ENABLE = 0;
    }
    
    /** PRE_WRITE */ // any write, reests PRE_WRITE
    lc->FF_PRE_WRITE = 0;

    /** WRITE_ENABLE */
    if ((address & 0b00000001) == 0) { // write 0 or 2
        lc->_FF_WRITE_ENABLE = 1;
    }

    /* This means there is a feature of RAM card control
    not documented by Apple: write access to odd address in C08X
    range controls the READ ENABLE flip-flip without affecting the WRITE enable' flip-flop.
    STA $C081: no effect on write enable, disable read, bank 2 */

    if (DEBUG(DEBUG_LANGCARD)) printf("FF_BANK_1: %d, FF_READ_ENABLE: %d, FF_PRE_WRITE: %d, _FF_WRITE_ENABLE: %d\n", 
        lc->FF_BANK_1, lc->FF_READ_ENABLE, lc->FF_PRE_WRITE, lc->_FF_WRITE_ENABLE);   

    bsr_map_memory(lc);
}

uint8_t bsr_read_C011(void *context, uint32_t address) {
    iiememory_state_t *lc = (iiememory_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard_read_C011 %04X FF_BANK_1: %d\n", address, lc->FF_BANK_1);
    uint8_t fl = (lc->FF_BANK_1 == 0) ? 0x80 : 0x00;
    
    KeyboardMessage *keymsg = (KeyboardMessage *)lc->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;
}

uint8_t bsr_read_C012(void *context, uint32_t address) {
    iiememory_state_t *lc = (iiememory_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard_read_C012 %04X FF_READ_ENABLE: %d\n", address, lc->FF_READ_ENABLE);

    uint8_t fl = (lc->FF_READ_ENABLE != 0) ? 0x80 : 0x00; /* << 7; */

    KeyboardMessage *keymsg = (KeyboardMessage *)lc->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;
}


/**
 * Now the IIe-specific stuff.
 * 
 * 
 * 
 * 
 * 
 */


void iiememory_debug(iiememory_state_t *iiememory_d) {
    // CX debug
    iiememory_d->mmu->dump_page_table(0xC2, 0xC3);
    /* printf("IIe Memory: m_zp: %d, m_text1_r: %d, m_text1_w: %d, m_hires1_r: %d, m_hires1_w: %d, m_all_r: %d, m_all_w: %d\n", 
        iiememory_d->m_zp, iiememory_d->m_text1_r, iiememory_d->m_text1_w, iiememory_d->m_hires1_r, iiememory_d->m_hires1_w, iiememory_d->m_all_r, iiememory_d->m_all_w); */
}

void iiememory_compose_map(iiememory_state_t *iiememory_d) {
    const char *TAG_MAIN = "MAIN";
    const char *TAG_ALT = "AUX";
        
    bool n_zp; 
    bool n_text1_r;
    bool n_text1_w;
    bool n_hires1_r;
    bool n_hires1_w;
    bool n_all_r;
    bool n_all_w;

    n_zp = iiememory_d->f_altzp;
    n_all_r = iiememory_d->f_ramrd;
    n_all_w = iiememory_d->f_ramwrt;
    if (iiememory_d->f_80store) {
        if (iiememory_d->s_hires) {
            n_text1_r = iiememory_d->s_page2;
            n_text1_w = iiememory_d->s_page2;
            n_hires1_r = iiememory_d->s_page2;
            n_hires1_w = iiememory_d->s_page2;
        } else {
            n_text1_r = iiememory_d->s_page2;
            n_text1_w = iiememory_d->s_page2;
            n_hires1_r = iiememory_d->f_ramrd;
            n_hires1_w = iiememory_d->f_ramwrt;
        }
    } else { // 80STORE OFF
        n_text1_r = iiememory_d->f_ramrd;
        n_text1_w = iiememory_d->f_ramwrt;
        n_hires1_r = iiememory_d->f_ramrd;
        n_hires1_w = iiememory_d->f_ramwrt;
    }

    uint8_t *memory_base = iiememory_d->ram;

    if (n_zp != iiememory_d->m_zp) { // this is both read and write.
        // change $00, $01, $D0 - $FF
        uint32_t altoffset = n_zp ? 0x1'0000 : 0x0'0000;

        iiememory_d->mmu->map_page_read(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        iiememory_d->mmu->map_page_write(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        iiememory_d->mmu->map_page_read(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);
        iiememory_d->mmu->map_page_write(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);
        
        // handle mapping the "language card" portion.
        bsr_map_memory(iiememory_d); // handle the 'language card' portion.
    }
    if (n_text1_r != iiememory_d->m_text1_r) {
        // change $04 - $07
        uint32_t altoffset = n_text1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            iiememory_d->mmu->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_text1_w != iiememory_d->m_text1_w) {
        // change $04 - $07
        uint32_t altoffset = n_text1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            iiememory_d->mmu->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_r != iiememory_d->m_hires1_r) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            iiememory_d->mmu->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_w != iiememory_d->m_hires1_w) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            iiememory_d->mmu->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_r != iiememory_d->m_all_r) {  
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            iiememory_d->mmu->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            iiememory_d->mmu->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            iiememory_d->mmu->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_w != iiememory_d->m_all_w) {
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            iiememory_d->mmu->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            iiememory_d->mmu->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            iiememory_d->mmu->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
    }

    // update the "current memory map state" flags.
    iiememory_d->m_zp = n_zp;
    iiememory_d->m_text1_r = n_text1_r;
    iiememory_d->m_text1_w = n_text1_w;
    iiememory_d->m_hires1_r = n_hires1_r;
    iiememory_d->m_hires1_w = n_hires1_w;
    iiememory_d->m_all_r = n_all_r;
    iiememory_d->m_all_w = n_all_w;
}

void iiememory_write_C00X(void *context, uint32_t address, uint8_t data) {
    iiememory_state_t *iiememory_d = (iiememory_state_t *)context;

    switch (address) {

        case 0xC000: // 80STOREOFF
            iiememory_d->f_80store = false;
            break;
        case 0xC001: // 80STOREON
            iiememory_d->f_80store = true;
            break;
        case 0xC002: // RAMRDOFF
            iiememory_d->f_ramrd = false;
            break;
        case 0xC003: // RAMRDON
            iiememory_d->f_ramrd = true;
            break;
        case 0xC004: // RAMWRTOFF
            iiememory_d->f_ramwrt = false;
            break;
        case 0xC005: // RAMWRTON
            iiememory_d->f_ramwrt = true;
            break;
        case 0xC008: // ALTZPOFF
            iiememory_d->f_altzp = false;
            break;
        case 0xC009: // ALTZPON
            iiememory_d->f_altzp = true;
            break;
    }
    iiememory_compose_map(iiememory_d);

}

uint8_t iiememory_read_C01X(void *context, uint32_t address) {
    iiememory_state_t *iiememory_d = (iiememory_state_t *)context;

    uint8_t fl = 0x00;
    MMU_IIe *mmu = (MMU_IIe *)iiememory_d->mmu;

    switch (address) {
        case 0xC011: // BSRBANK2
            fl = (!iiememory_d->FF_BANK_1) ? 0x80 : 0x00;
            break;
        case 0xC012: // BSRREADRAM
            fl = (iiememory_d->FF_READ_ENABLE) ? 0x80 : 0x00;
            break;
        case 0xC013: // RAMRD
            fl = (iiememory_d->f_ramrd) ? 0x80 : 0x00;
            break;
        case 0xC014: // RAMWRT
            fl = (iiememory_d->f_ramwrt) ? 0x80 : 0x00;
            break;
        case 0xC015: // INTCXROM
            fl = (mmu->f_intcxrom) ? 0x80 : 0x00;
            break;
        case 0xC016: // ALTZP
            fl = (iiememory_d->f_altzp) ? 0x80 : 0x00;
            break;
        case 0xC017: // SLOTC3ROM
            fl =  (mmu->f_slotc3rom) ? 0x80 : 0x00;
            break;
        case 0xC018: // 80STORE
            fl =  (iiememory_d->f_80store) ? 0x80 : 0x00;
            break;
        default:
            assert(false && "iiememory: Unhandled C01X read");
            //fl =  0x00;
    }
    KeyboardMessage *keymsg = (KeyboardMessage *)iiememory_d->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;
    //return iie_kb_read_strobe(iiememory_d->computer) | fl;
}

uint8_t iiememory_read_display(void *context, uint32_t address) {
    iiememory_state_t *iiememory_d = (iiememory_state_t *)context;

    uint8_t retval = 0x00;
    switch (address) {
        case 0xC050: // TEXTOFF
            iiememory_d->s_text = false;
            break;
        case 0xC051: // TEXTON
            iiememory_d->s_text = true;
            break;
        case 0xC052: // MIXEDOFF
            iiememory_d->s_mixed = false;
            break;
        case 0xC053: // MIXEDON
            iiememory_d->s_mixed = true;
            break;
        case 0xC054: // PAGE2OFF
            iiememory_d->s_page2 = false;
            break;
        case 0xC055: // PAGE2ON
            iiememory_d->s_page2 = true;
            break;
        case 0xC056: // HIRESOFF
            iiememory_d->s_hires = false;
            break;
        case 0xC057: // HIRESON
            iiememory_d->s_hires = true;
            break;
        default:
            assert(false && "iiememory: Unhandled display read");
    }
    // recompose the memory map
    iiememory_compose_map(iiememory_d);
    return retval;
}

// write - do same as read but disregard return value.
void iiememory_write_display(void *context, uint32_t address, uint8_t data) {
    iiememory_state_t *iiememory_d = (iiememory_state_t *)context;
    iiememory_read_display(context, address);
}

DebugFormatter *debug_iiememory(iiememory_state_t *iiememory_d) {
    DebugFormatter *f = new DebugFormatter();
    MMU_IIe *mmu = (MMU_IIe *)iiememory_d->mmu; // we are in fact a iie

    // dump the page table
    f->addLine("80ST: %1d RAMR: %1d RAMW: %1d ALTZP: %1d SLOTC3: %1d",
        iiememory_d->f_80store, iiememory_d->f_ramrd, iiememory_d->f_ramwrt, iiememory_d->f_altzp, mmu->f_slotc3rom);
    f->addLine("INTCX: %1d LC [ BNK1: %1d READ: %1d WRT: %1d ]",
        mmu->f_intcxrom, iiememory_d->FF_BANK_1, iiememory_d->FF_READ_ENABLE, !iiememory_d->_FF_WRITE_ENABLE);
    f->addLine("TEXT: %1d MIXED: %1d PAGE2: %1d HIRES: %1d",
        iiememory_d->s_text, iiememory_d->s_mixed, iiememory_d->s_page2, iiememory_d->s_hires);

    iiememory_d->mmu->debug_output_page(f, 0x00, true);
    iiememory_d->mmu->debug_output_page(f, 0x02);
    iiememory_d->mmu->debug_output_page(f, 0x04);
    iiememory_d->mmu->debug_output_page(f, 0x08);
    iiememory_d->mmu->debug_output_page(f, 0x20);
    iiememory_d->mmu->debug_output_page(f, 0x40);
    iiememory_d->mmu->debug_output_page(f, 0xC1);
    iiememory_d->mmu->debug_output_page(f, 0xC3);
    iiememory_d->mmu->debug_output_page(f, 0xC6);
    f->addLine("C8xx_slot: %d", iiememory_d->mmu->get_C8xx_slot());
    iiememory_d->mmu->debug_output_page(f, 0xC8);
    iiememory_d->mmu->debug_output_page(f, 0xD0);
    iiememory_d->mmu->debug_output_page(f, 0xE0);
    return f;
}

/*
 * When you initiate a reset, hardware in the Apple IIe sets the memory-controlling soft switches to normal: 
 * main board RAM and ROM are enabled; if there is an 80 column card in the aux slot, expansion slot 3 is allocated 
 * to the built-in 80 column firmware. 
 * auxiliary ram is disabled and the BSR is set up to read ROM and write RAM, bank 2. (hardware)
*/

void reset_iiememory(iiememory_state_t *lc) {

    // in a IIe RESET does affect the memory map.
    lc->FF_BANK_1 = 0;
    lc->FF_PRE_WRITE = 0;
    lc->FF_READ_ENABLE = 0;
    lc->_FF_WRITE_ENABLE = 0;

    lc->f_80store = false;
    lc->f_ramrd = false;
    lc->f_ramwrt = false;
    lc->f_altzp = false;

    // TODO: what about the display flags? I don't think these are reset on RESET, but give some consideration.

    bsr_map_memory(lc);
    iiememory_compose_map(lc);
}

void init_iiememory(computer_t *computer, SlotType_t slot) {
    
    iiememory_state_t *iiememory_d = new iiememory_state_t;
    iiememory_d->computer = computer;
    iiememory_d->mmu = computer->mmu;
    iiememory_d->ram = computer->mmu->get_memory_base();
    iiememory_d->mbus = computer->mbus;

    computer->set_module_state(MODULE_IIEMEMORY, iiememory_d);
    
    for (int i = 0xC000; i <= 0xC009; i++) {
        if (i == 0xC006 || i == 0xC007) continue; // INTCXROM is handled by the MMU.
        // C00A-C00b: handled by mmu
        //if (i == 0xC00C || i == 0xC00D) continue; // 80COL is handled by the display device.
        computer->mmu->set_C0XX_write_handler(i, { iiememory_write_C00X, iiememory_d });
    }

    /* C019 - C01F are handled by display */
    for (uint16_t i = 0xC011; i <= 0xC018; i++) {
        //if (i == 0xC015) continue; // INTCXROM is handled by the MMU.
        computer->mmu->set_C0XX_read_handler(i, { iiememory_read_C01X, iiememory_d });
    }

    // We also need these for tracking memory mapping.
    for (uint16_t i = 0xC050; i <= 0xC057; i++) {
        computer->mmu->set_C0XX_read_handler(i, { iiememory_read_display, iiememory_d });
        computer->mmu->set_C0XX_write_handler(i, { iiememory_write_display, iiememory_d });
    }

    /**
     * set up the "BSR" state.0
     */
    /** At power up, the RAM card is disabled for reading and enabled for writing.
    * the pre-write flip-flop is reset, and bank 2 is selected. 
    * the RESET .. has no effect on the RAM card configuration.
    */
    iiememory_d->FF_BANK_1 = 0;
    iiememory_d->FF_PRE_WRITE = 0;
    iiememory_d->FF_READ_ENABLE = 0;
    iiememory_d->_FF_WRITE_ENABLE = 0;

    iiememory_d->mmu->set_C0XX_read_handler(0xC011, { bsr_read_C011, iiememory_d });
    iiememory_d->mmu->set_C0XX_read_handler(0xC012, { bsr_read_C012, iiememory_d });

    for (uint16_t i = 0xC080; i <= 0xC08F; i++) {
        iiememory_d->mmu->set_C0XX_read_handler(i, { bsr_read_C0xx, iiememory_d });
        iiememory_d->mmu->set_C0XX_write_handler(i, { bsr_write_C0xx, iiememory_d });
    }

    bsr_map_memory(iiememory_d);

    // need to handle reset here.
    computer->register_reset_handler(
        [iiememory_d]() {
            reset_iiememory(iiememory_d);
            return true;
        });

    computer->register_debug_display_handler(
        "iiememory",
        DH_IIEMEMORY, // unique ID for this, need to have in a header.
        [iiememory_d]() -> DebugFormatter * {
            return debug_iiememory(iiememory_d);
        }
    );
}


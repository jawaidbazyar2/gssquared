/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

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

#include "devices/languagecard/languagecard.hpp"

void set_memory_pages_based_on_flags(languagecard_state_t *lc) {

    uint8_t *bank = (lc->FF_BANK_1 == 1) ? lc->ram_bank : lc->ram_bank + 0x1000;
    const char *bank_d = (lc->FF_BANK_1 == 1) ? "LC_BANK1" : "LC_BANK2";

    for (int i = 0; i < 16; i++) {
        if (lc->FF_READ_ENABLE) {
            lc->mmu->map_page_read(i + 0xD0, bank + (i*GS2_PAGE_SIZE), bank_d);
        } else { // reads == READ_ROM
            lc->mmu->map_page_read(i + 0xD0, lc->mmu->get_rom_base() + (i*GS2_PAGE_SIZE), "SYS_ROM");
        }

        if (!lc->_FF_WRITE_ENABLE) {
            lc->mmu->map_page_write(i+0xD0, bank + (i*GS2_PAGE_SIZE), bank_d);

        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            lc->mmu->map_page_write(i+0xD0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    for (int i = 0; i < 32; i++) {
        if (lc->FF_READ_ENABLE) {
            lc->mmu->map_page_read(i+0xE0, lc->ram_bank + 0x2000 + (i * GS2_PAGE_SIZE), "LC RAM");

        } else { // reads == READ_ROM
            lc->mmu->map_page_read(i+0xE0, lc->mmu->get_rom_base() + 0x1000 + (i * GS2_PAGE_SIZE), "LC RAM");
        }

        if (!lc->_FF_WRITE_ENABLE) {
            lc->mmu->map_page_write(i+0xE0, lc->ram_bank + 0x2000 + (i * GS2_PAGE_SIZE), "LC RAM");
        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            lc->mmu->map_page_write(i+0xE0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    if (DEBUG(DEBUG_LANGCARD)) {
        lc->mmu->dump_page_table(0xD0, 0xD0);
        lc->mmu->dump_page_table(0xE0, 0xE0);
    }
}

uint8_t languagecard_read_C0xx(void *context, uint32_t address) {
    languagecard_state_t *lc = (languagecard_state_t *)context;

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

    set_memory_pages_based_on_flags(lc);
    return 0;
}


void languagecard_write_C0xx(void *context, uint32_t address, uint8_t value) {
    languagecard_state_t *lc = (languagecard_state_t *)context;

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

    set_memory_pages_based_on_flags(lc);
}

uint8_t languagecard_read_C011(void *context, uint32_t address) {
    languagecard_state_t *lc = (languagecard_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard_read_C011 %04X FF_BANK_1: %d\n", address, lc->FF_BANK_1);
   return (lc->FF_BANK_1 == 0) ? 0x80 : 0x00;
}

uint8_t languagecard_read_C012(void *context, uint32_t address) {
    languagecard_state_t *lc = (languagecard_state_t *)context;

    if (DEBUG(DEBUG_LANGCARD)) printf("languagecard_read_C012 %04X FF_READ_ENABLE: %d\n", address, lc->FF_READ_ENABLE);
    return (lc->FF_READ_ENABLE != 0) ? 0x80 : 0x00; /* << 7; */
}


void reset_languagecard(languagecard_state_t *lc) {
    // in a II Plus the language card does NOT reset memory map on a RESET.
    /* lc->FF_BANK_1 = 0;
    lc->FF_PRE_WRITE = 0;
    lc->FF_READ_ENABLE = 0;
    lc->_FF_WRITE_ENABLE = 0;
    lc->ram_bank = new uint8_t[0x4000];

    set_memory_pages_based_on_flags(lc); */
}

void init_slot_languagecard(computer_t *computer, SlotType_t slot) {

    fprintf(stdout, "languagecard_register_slot %d\n", slot);
    if (slot != 0) {
        fprintf(stdout, "languagecard_register_slot %d - not supported\n", slot);
        return;
    }

    languagecard_state_t *lc = new languagecard_state_t();
    lc->mmu = computer->mmu;

/** At power up, the RAM card is disabled for reading and enabled for writing.
 * the pre-write flip-flop is reset, and bank 2 is selected. 
 * the RESET .. has no effect on the RAM card configuration.
 */
    lc->FF_BANK_1 = 0;
    lc->FF_PRE_WRITE = 0;
    lc->FF_READ_ENABLE = 0;
    lc->_FF_WRITE_ENABLE = 0;
    lc->ram_bank = new uint8_t[0x4000];

    // TODO: This should only be for Apple IIe and IIgs.
    lc->mmu->set_C0XX_read_handler(0xC011, { languagecard_read_C011, lc });
    lc->mmu->set_C0XX_read_handler(0xC012, { languagecard_read_C012, lc });

    for (uint16_t i = 0xC080; i <= 0xC08F; i++) {
        lc->mmu->set_C0XX_read_handler(i, { languagecard_read_C0xx, lc });
        lc->mmu->set_C0XX_write_handler(i, { languagecard_write_C0xx, lc });
    }

    set_memory_pages_based_on_flags(lc);

    computer->register_reset_handler(
        [lc]() {
            reset_languagecard(lc);
            return true;
        });
}

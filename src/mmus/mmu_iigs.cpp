#include "mmu_iigs.hpp"
#include "clock.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "mmus/mmu.hpp"

uint8_t float_area_read(void *context, uint32_t address) {
    if (DEBUG(DEBUG_MMUGS)) printf("Float area read at address %06X\n", address);
    return address >> 16 ;
}

inline uint8_t bank_e0_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    //if ((address & 0xFF00) == 0xC000) return mmu_iigs->read_c0xx(address); // MegaII will call back to us
    return mmu_iigs->megaii->read(address & 0xFFFF);
}

inline void bank_e0_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    //if ((address & 0xFF00) == 0xC000) {mmu_iigs->write_c0xx(address, value); return;} // MegaII will call back to us
    mmu_iigs->megaii->write(address & 0xFFFF, value);
}

/* Bank E1 read/write */

inline uint8_t bank_e1_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    if ((address & 0xFF00) == 0xC000) return mmu_iigs->megaii->read(address & 0xFFFF);  // return mmu_iigs->read_c0xx(address);
    
    if (!mmu_iigs->is_bank_latch()) {
        return mmu_iigs->megaii->read(address & 0xFFFF);
    } else 
    {
        uint8_t *ram = mmu_iigs->megaii->get_memory_base();
        return ram[address & 0x1FFFF];   
    }
}

inline void bank_e1_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    if ((address & 0xFF00) == 0xC000) mmu_iigs->megaii->write(address & 0xFFFF, value); // mmu_iigs->write_c0xx(address, value);

    if (!mmu_iigs->is_bank_latch()) {
        mmu_iigs->megaii->write(address & 0xFFFF, value);
    } else 
    {
        uint8_t *ram = mmu_iigs->megaii->get_memory_base();
        ram[address & 0x1FFFF] = value;   
    }
}

inline void MMU_IIgs::set_intcxrom(bool value) {
    megaii->f_intcxrom = value;
    g_intcxrom = value;
    megaii->compose_c1cf();

/*     if (value) {
        // enable main ROM in pages $C1 - $CF
        megaii->f_intcxrom = true;
        g_intcxrom = true;
        megaii->compose_c1cf();
    } else {
        // TODO: this isn't taking into account that C8xx might be active for slot 3 (i.e., what sather calls INTC8ROM). (but test on real GS seems to behave differently?)
        // enable slot ROM in pages $C1 - $CF
        megaii->f_intcxrom = false;
        g_intcxrom = false;
        megaii->compose_c1cf();
        //megaii->set_default_C8xx_map(); // TODO: is this right?  https://zellyn.com/a2audit/v0/#e000b
    }     */
}

inline void MMU_IIgs::write_c0xx(uint16_t address, uint8_t value) {

    switch (address) {
        case 0xC000: g_80store = false; break;
        case 0xC001: g_80store = true; break;
        case 0xC002: g_ramrd = false; break;
        case 0xC003: g_ramrd = true; break;
        case 0xC004: g_ramwrt = false; break;
        case 0xC005: g_ramwrt = true; break;
        case 0xC008: g_altzp = false; break;
        case 0xC009: g_altzp = true; break;
        case 0xC029: reg_new_video = value; break;
        case 0xC006: // INTCXROMOFF
            set_intcxrom(false);
            /* // enable slot ROM in pages $C1 - $CF
            megaii->f_intcxrom = false;
            megaii->compose_c1cf();
            megaii->set_default_C8xx_map(); // TODO: is this right?  https://zellyn.com/a2audit/v0/#e000b
            //printf("IIe Memory: INTCXROMOFF\n"); */
            break;
        case 0xC007: // INTCXROMON
            set_intcxrom(true);
/*             // enable main ROM in pages $C1 - $CF
            megaii->f_intcxrom = true;
            megaii->compose_c1cf();
            //printf("IIe Memory: INTCXROMON\n"); */
            break;
        case 0xC00A: // SLOTC3ROM
            megaii->f_slotc3rom = false;
            megaii->compose_c1cf();
            break;
        case 0xC00B: // SLOTC3ROM
            megaii->f_slotc3rom = true;
            megaii->compose_c1cf();
            break;
        case 0xC054: g_page2 = false;  break;
        case 0xC055: g_page2 = true;  break;
        case 0xC056: g_hires = false;  break;
        case 0xC057: g_hires = true;  break;
        default:
            assert(false && "MMU_IIgs: Unhandled C0XX write");
    }
    megaii_compose_map();
    if (DEBUG(DEBUG_MMUGS)) printf("Reg Write: %04X: %02X\n", address, value);
}

inline uint8_t MMU_IIgs::read_c0xx(uint16_t address) {
    uint8_t retval = 0xEE;
    switch (address) {
        case 0xC054: g_page2 = false; /* Call Display; */; break;
        case 0xC055: g_page2 = true; /* Call Display; */; break;
        case 0xC056: g_hires = false; /* Call Display; */; break;
        case 0xC057: g_hires = true; /* Call Display; */; break;
        case 0xC029: retval = reg_new_video; break;
        default:
            assert(false && "MMU_IIgs: Unhandled C0XX read");
    }
    megaii_compose_map();
    return retval;
}

uint8_t read_c068(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->state_register();
}

/* 
   TODO: GuS optimized this by checking bits old vs new and only calling remapper functions if necessary.
*/
void write_c068(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_state_register(value);
    mmu_iigs->set_intcxrom((value & 0x01) ? true : false);
    mmu_iigs->set_lc_bank1(((value & 0x04) != 0) ? false : true); // "if this bit is 1, lc ram bank 2 is selected" inverting sense (GS hw Ref is backwards)
    mmu_iigs->set_lc_read_enable((value & 0x08) ? false : true); // If bit 3 is set, we need to update the LC read enable flag also.
    mmu_iigs->megaii_compose_map();
    mmu_iigs->bsr_map_memory();
    
    // also set PAGE2 display in video scanner
    if (value & 0x40) {
        mmu_iigs->megaii->write(0xC055, 0x00);
        mmu_iigs->set_page2(true);
    } else {
        mmu_iigs->megaii->write(0xC054, 0x00);
        mmu_iigs->set_page2(false);
    }
   /*  if (value & 0x80) { // ALTZP - not needed, megaii_compose_map() does this for us.
        mmu_iigs->megaii->write(0xC009,0x01);
    } else {
        mmu_iigs->megaii->write(0xC008,0x00);
    } */

}

void MMU_IIgs::set_slot_register(uint8_t value) {
    // store here
    reg_slot = value;
    if (DEBUG(DEBUG_MMUGS)) printf("setting slot register: %02X\n", value);
    // also update the megaii slot register.
    megaii->set_slot_register(value);
    //megaii->compose_c1cf(); // redundant since megaii->set_slot_register() does this.
}

void write_c02d(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_slot_register(value);
}

uint8_t read_c02d(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->get_slot_register();
}

// read/write handlers for C0XX registers as handed to the MegaII.
inline uint8_t megaii_c0xx_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->read_c0xx(address);
}

inline void megaii_c0xx_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->write_c0xx(address, value);
}


void MMU_IIgs::megaii_compose_map() {
    const char *TAG_MAIN = "MAIN";
    const char *TAG_ALT = "AUX";
    
    //update_display_flags(iiememory_d);
    
    bool n_zp; 
    bool n_text1_r;
    bool n_text1_w;
    bool n_hires1_r;
    bool n_hires1_w;
    bool n_all_r;
    bool n_all_w;

    n_zp = g_altzp;
    n_all_r = g_ramrd;
    n_all_w = g_ramwrt;
    if (g_80store) {
        if (g_hires) {
            n_text1_r = g_page2;
            n_text1_w = g_page2;
            n_hires1_r = g_page2;
            n_hires1_w = g_page2;
        } else {
            n_text1_r = g_page2;
            n_text1_w = g_page2;
            n_hires1_r = g_ramrd;
            n_hires1_w = g_ramwrt;
        }
    } else { // 80STORE OFF
        n_text1_r = g_ramrd;
        n_text1_w = g_ramwrt;
        n_hires1_r = g_ramrd;
        n_hires1_w = g_ramwrt;
    }

    uint8_t *memory_base = megaii->get_memory_base();

    if (n_zp != m_zp) { // this is both read and write.
        // change $00, $01, $D0 - $FF
        uint32_t altoffset = n_zp ? 0x1'0000 : 0x0'0000;

        megaii->map_page_read(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_write(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_read(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_write(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);
        
        // handle mapping the "language card" portion.
        //bsr_map_memory(iiememory_d); // handle the 'language card' portion.
    }
    if (n_text1_r != m_text1_r) {
        // change $04 - $07
        uint32_t altoffset = n_text1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_text1_w != m_text1_w) {
        // change $04 - $07
        uint32_t altoffset = n_text1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_r != m_hires1_r) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_w != m_hires1_w) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_r != m_all_r) {  
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_w != m_all_w) {
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
    }

    // update the "current memory map state" flags.
    m_zp = n_zp;
    m_text1_r = n_text1_r;
    m_text1_w = n_text1_w;
    m_hires1_r = n_hires1_r;
    m_hires1_w = n_hires1_w;
    m_all_r = n_all_r;
    m_all_w = n_all_w;
}

void MMU_IIgs::bsr_map_memory() {

    uint32_t bankd0offset = (FF_BANK_1 == 1) ? 0xC000 : 0xD000;
    uint32_t banke0offset = 0xE000;
    if (g_altzp) {
        bankd0offset += 0x1'0000; // alternate bank!
        banke0offset += 0x1'0000; // alternate bank!
    }

    //uint8_t *bank = (lc->FF_BANK_1 == 1) ? lc->ram : lc->ram + 0x1000;
    uint8_t *bankd0 = megaii->get_memory_base() + bankd0offset;
    uint8_t *banke0 = megaii->get_memory_base() + banke0offset;
    uint8_t *rom = megaii->get_rom_base();

    const char *bank_d = (FF_BANK_1 == 1) ? "LC_BANK1" : "LC_BANK2";

    /* Map D0 - DF */
    for (int i = 0; i < 16; i++) {
        if (FF_READ_ENABLE) {
            megaii->map_page_read(i + 0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);
        } else { // reads == READ_ROM
        // TODO: this is wrong - needs to somehow know to return to ROM D0/etc wherever that may be.
        // right now hard-code to what we know about the iie rom.
            megaii->map_page_read(i + 0xD0, rom + 0x1000 + (i*GS2_PAGE_SIZE), "SYS_ROM");
        }

        if (!_FF_WRITE_ENABLE) {
            megaii->map_page_write(i+0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);

        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            megaii->map_page_write(i+0xD0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    /* Map E0 - FF */
    for (int i = 0; i < 32; i++) {
        if (FF_READ_ENABLE) {
            megaii->map_page_read(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");

        } else { // reads == READ_ROM
            // TODO: this is wrong - needs to somehow know to return to ROM D0/etc wherever that may be.
            megaii->map_page_read(i+0xE0, rom + 0x2000 + (i * GS2_PAGE_SIZE), "SYS_ROM");
        }

        if (!_FF_WRITE_ENABLE) {
            megaii->map_page_write(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");
        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            megaii->map_page_write(i+0xE0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    /* if (DEBUG(DEBUG_LANGCARD)) {
        lc->mmu->dump_page_table(0xD0, 0xD0);
        lc->mmu->dump_page_table(0xE0, 0xE0);
    } */
}

uint8_t g_bsr_read_C0xx(void *context, uint32_t address) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    //if (DEBUG(DEBUG_LANGCARD)) printf("languagecard read %04X ", address);

    /** Bank Select */    
    
    if (address & LANG_A3 ) {
        // 1 = any access sets Bank_1
        lc->set_lc_bank1(1);
    } else {
        // 0 = any access resets Bank_1
        lc->set_lc_bank1(0);
    }

    /** Read Enable */
    
    if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) {
        // 00, 11 - set READ_ENABLE
        lc->set_lc_read_enable(1);
    } else {
        // 01, 10 - reset READ_ENABLE
        lc->set_lc_read_enable(0);
    }

    int old_pre_write = lc->is_lc_pre_write();

    /* PRE_WRITE */
    if ((address & 0b00000001) == 1) {  // read 1 or 3
        // 00000001 - set PRE_WRITE
        lc->set_lc_pre_write(1);
    } else {                            // read 0 or 2
        // 00000000 - reset PRE_WRITE
        lc->set_lc_pre_write(0);
    }

    /** Write Enable */
    if ((old_pre_write == 1) && ((address & 0b00000001) == 1)) { // PRE_WRITE set, read 1 or 3
        // 00000000 - reset WRITE_ENABLE'
        lc->set_lc_write_enable(0);
    }
    if ((address & 0b00000001) == 0) { // read 0 or 2, set _WRITE_ENABLE
        // 00000001 - set WRITE_ENABLE'
        lc->set_lc_write_enable(1);
    }

    if (DEBUG(DEBUG_MMUGS)) printf("FF_BANK_1: %d, FF_READ_ENABLE: %d, FF_PRE_WRITE: %d, _FF_WRITE_ENABLE: %d\n", 
        lc->is_lc_bank1(), lc->is_lc_read_enable(), lc->is_lc_pre_write(), lc->is_lc_write_enable());
   
    /**
     * compose the memory map.
     * in main_ram:
     * 0x0000 - 0xBFFF - straight map.
     * 0xC000 - 0xCFFF - bank 1 extra memory
     * 0xD000 - 0xDFFF - bank 2 extra memory
     * 0xE000 - 0xFFFF - rest of extra memory
     * */

    lc->bsr_map_memory();
    return 0;
}


void gs_bsr_write_C0xx(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    //if (DEBUG(DEBUG_LANGCARD)) printf("languagecard write %04X value: %02X\n", address, value);
    

    /** Bank Select */

    if (address & LANG_A3 ) {
        // 1 = any access sets Bank_1 - read or write.
        lc->set_lc_bank1(1);
    } else {
        // 0 = any access resets Bank_1
        lc->set_lc_bank1(0);
    }

    /** READ_ENABLE  */
    
    if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) { // write 0 or 3
        // 00, 11 - set READ_ENABLE
        lc->set_lc_read_enable(1);
    } else {
        // 01, 10 - reset READ_ENABLE
        lc->set_lc_read_enable(0);
    }
    
    /** PRE_WRITE */ // any write, reests PRE_WRITE
    lc->set_lc_pre_write(0);

    /** WRITE_ENABLE */
    if ((address & 0b00000001) == 0) { // write 0 or 2
        lc->set_lc_write_enable(1);
    }

    /* This means there is a feature of RAM card control
    not documented by Apple: write access to odd address in C08X
    range controls the READ ENABLE flip-flip without affecting the WRITE enable' flip-flop.
    STA $C081: no effect on write enable, disable read, bank 2 */

    if (DEBUG(DEBUG_LANGCARD)) printf("FF_BANK_1: %d, FF_READ_ENABLE: %d, FF_PRE_WRITE: %d, _FF_WRITE_ENABLE: %d\n", 
        lc->is_lc_bank1(), lc->is_lc_read_enable(), lc->is_lc_pre_write(), lc->is_lc_write_enable());   

    lc->bsr_map_memory();

}

uint8_t gs_bsr_read_C01x(void *context, uint32_t address) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    if (DEBUG(DEBUG_MMUGS)) printf("gs_bsr_read_C01x %04X \n", address);
    uint8_t fl = 0x00;
    switch (address & 0xF) {
        case 0x1: /* C011 */ fl = (lc->is_lc_bank1() == 0) ? 0x80 : 0x00; break;
        case 0x2: /* C012 */ fl = (lc->is_lc_read_enable() != 0) ? 0x80 : 0x00; break;
        case 0x3: /* C013 */ fl = (lc->state_register() & 0x20) ? 0x80 : 0x00; break;
        case 0x4: /* C014 */ fl = (lc->state_register() & 0x10) ? 0x80 : 0x00; break;
        case 0x5: /* C015 */ fl = (lc->state_register() & 0x01) ? 0x80 : 0x00; break;
        case 0x6: /* C016 */ fl = (lc->state_register() & 0x80) ? 0x80 : 0x00; break;
        case 0x7: /* C017 */ fl = lc->is_slotc3rom() ? 0x80 : 0x00; break;
        case 0x8: /* C018 */ fl = lc->is_80store() ? 0x80 : 0x00; break;

        // TODO: this is a weird place to have these. They don't affect memory map.
        case 0xA: /* C01A */ fl = lc->is_text() ? 0x80 : 0x00; break;
        case 0xB: /* C01B */ fl = lc->is_mixed() ? 0x80 : 0x00; break;

        /* These do affect memory map. */
        case 0xC: /* C01C */ fl = lc->is_page2() ? 0x80 : 0x00; break;
        case 0xD: /* C01D */ fl = lc->is_hires() ? 0x80 : 0x00; break;
    }
    
    /* KeyboardMessage *keymsg = (KeyboardMessage *)lc->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl; */
    return fl;
}

/*
TODO: need to add: c015, c016, c017, c018, c01a, c01b, c01c, c01d 
*/


// set shadow register
void c035_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_shadow_register(value);
}

uint8_t c035_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->shadow_register();
}

uint8_t c036_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->speed_register();
}

// set speed register
void c036_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    uint8_t current_speed_reg = mmu_iigs->speed_register();
    if ((current_speed_reg ^ value) & 0x80) {
        // speed change.
        if (value & 0x80) set_clock_mode(mmu_iigs->get_cpu(), CLOCK_4MHZ);
        else set_clock_mode(mmu_iigs->get_cpu(), CLOCK_1_024MHZ);
    }
    mmu_iigs->set_speed_register(value);
    mmu_iigs->set_ram_shadow_banks();
}

/*
IIe Memory Mapping for Main/AUX:
    | 7 | ALTZP | 1 = ZP, Stack, LC are in Main; 0 = in Aux |
    | 6 | PAGE2 | 1 = Text Page 2 Selected |
    | 5 | RAMRD | 1 = Aux RAM is read-enabled |
    | 4 | RAMWRT | 1 = aux RAM is write-enabled |
    |   | 80STORE | 1 = 80-store is active (PAGE2 controls Main vs Aux) |
*/

/* uint32_t MMU_IIgs::calc_aux_write(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramwrt))) return 0x1'0000;
    if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramwrt))) return 0x1'0000;
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramwrt)) return 0x1'0000;
    return 0x0'0000;
}

uint32_t MMU_IIgs::calc_aux_read(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramrd))) return 0x1'0000;
    if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramrd ))) return 0x1'0000;
    //if (((page >= 0x00 && page <= 0x01) || (page >= 0xD0 && page <= 0xFF)) && (g_altzp || g_lcbnk2)) return 0x1'0000;
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    //if (g_ramrd) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramrd)) return 0x1'0000;
    return 0x0'0000;
} */

uint32_t MMU_IIgs::calc_aux_write(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    //if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramwrt))) return 0x1'0000;
    //if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramwrt))) return 0x1'0000;
    if (g_80store && (page >= 0x04 && page <= 0x07) ) {
        if (g_page2) return 0x1'0000;
        else return 0x0'0000;
    }
    if (g_80store && g_hires && (page >= 0x20 && page <= 0x3F) ) {
        if (g_page2) return 0x1'0000;
        else return 0x0'0000;
    }
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramwrt)) return 0x1'0000;
    return 0x0'0000;
}

uint32_t MMU_IIgs::calc_aux_read(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    //if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramrd))) return 0x1'0000;
    //if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramrd ))) return 0x1'0000;
    if (g_80store && (page >= 0x04 && page <= 0x07) ) {
        if (g_page2) return 0x1'0000;
        else return 0x0'0000;
    }
    if (g_80store && g_hires && (page >= 0x20 && page <= 0x3F) ) {
        if (g_page2) return 0x1'0000;
        else return 0x0'0000;
    }
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramrd)) return 0x1'0000;
    return 0x0'0000;
}

/* Any ram bank that has shadowing enabled of any kind comes through here. */
uint8_t bank_shadow_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;

    uint32_t page = (address & 0xFF00) >> 8;

    // if IOLC is "shadowed" and it's an I/O location, send write down to Megaii.
    if ( mmu_iigs->is_iolc_shadowed() && (page >= 0xC0 && page <= 0xCF)) {
        return mmu_iigs->megaiiRead(address & 0x1'FFFF);
    }
    
    // if IOLC is "shadowed" (enabled) and it's a language card location, map the address.
    if (mmu_iigs->is_iolc_shadowed() && (page >= 0xD0 && page <= 0xFF)) {
        
        if (mmu_iigs->is_lc_read_enable()) { // ram
            if (mmu_iigs->is_lc_bank1()) {
                if (page >= 0xD0 && page <= 0xDF) address -= 0x1000; // only this area.
            }
        } else {
            //uint8_t *addr;
            // rom
            if (DEBUG(DEBUG_MMUGS)) printf("Read: ROM Effective address: %06X\n", address);
            return mmu_iigs->get_rom_base()[0x1'0000 + (address & 0xFFFF)]; // TODO: this is only for ROM01. ROM03 has more ROM needs different offset. Have a routine to calculate.
        }
    }
    // we know at this point we were not originally C0 IO space. so.. 

    address += mmu_iigs->calc_aux_read(address);    // handle RAMRD, 80STORE, ALTZP, PAGE2, HIRES (cuz we can have this AND LC at same time)
    if (DEBUG(DEBUG_MMUGS)) printf("Read: Effective address: %06X\n", address);
    return mmu_iigs->get_memory_base()[address];
}

// use different routine for direct aux bank shadow
void bank_shadow_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    
    uint32_t page = (address & 0xFF00) >> 8;

    // if IOLC is "shadowed" and it's an I/O location, send write down to Megaii.
    if ( (mmu_iigs->is_iolc_shadowed()) && (page >= 0xC0 && page <= 0xCF) ) {
        mmu_iigs->megaiiWrite(address & 0x1'FFFF, value);
        return; // we delegated this to the MegaII
    }
    
    // if IOLC is "shadowed" (enabled) and it's a language card location, map the address.
    if (mmu_iigs->is_iolc_shadowed() && (page >= 0xD0 && page <= 0xFF)) {
        
        if (mmu_iigs->is_lc_write_enable()) { // is LC RAM writable?
            if (mmu_iigs->is_lc_bank1()) {
                if (page >= 0xD0 && page <= 0xDF) address -= 0x1000; // only this area.
            }
        } else {
            // ROM - do nothing, just fall through to here.
            return; // do not write to either LC RAM since _WRITE_ENABLE=1
        }
    }

    address += mmu_iigs->calc_aux_write(address);   // handle RAMWRT, 80STORE, ALTZP, PAGE2, HIRES

    if ( mmu_iigs->shadow_is_enabled(address)) {
        // Shadowed
        mmu_iigs->megaiiWrite(address & 0x1'FFFF, value);
    }
    if (DEBUG(DEBUG_MMUGS)) printf("Write: Effective address: %06X\n", address);
    // goes to RAM
    mmu_iigs->get_memory_base()[address] = value;
}

uint8_t iolc_rom_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->get_rom_base()[0x1'0000 + (address & 0xFFFF)];
}

read_handler_t float_read_handler = { (memory_read_func)float_area_read, nullptr };

void MMU_IIgs::init_c0xx_handlers() {
    for (uint32_t i = 0xC000; i <= 0xC009; i++) {
        megaii->set_C0XX_write_handler(i, {megaii_c0xx_write, this});
    }

    for (uint32_t i = 0xC011; i <= 0xC018; i++) {
        megaii->set_C0XX_read_handler(i, {gs_bsr_read_C01x, this});
    }

    //megaii->set_C0XX_read_handler(0xC01A, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01B, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01C, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01D, {gs_bsr_read_C01x, this});

    megaii->set_C0XX_write_handler(0xC02D, {write_c02d, this});
    megaii->set_C0XX_read_handler(0xC02D, {read_c02d, this});

    for (uint32_t i = 0xC054; i <= 0xC057; i++) {
        megaii->set_C0XX_write_handler(i, {megaii_c0xx_write, this});
        megaii->set_C0XX_read_handler(i, {megaii_c0xx_read, this});
    }

    for (uint32_t i = 0xC080; i <= 0xC08F; i++) {
        megaii->set_C0XX_write_handler(i, {gs_bsr_write_C0xx, this});
        megaii->set_C0XX_read_handler(i, {g_bsr_read_C0xx, this});
    }

    for (uint32_t i = 0xC071; i <= 0xC07F; i++) {
        megaii->set_C0XX_read_handler(i, {iolc_rom_read, this});
    }

    megaii->set_C0XX_write_handler(0xC035, {c035_write, this});
    megaii->set_C0XX_write_handler(0xC036, {c036_write, this});
    megaii->set_C0XX_read_handler(0xC035, {c035_read, this});
    megaii->set_C0XX_read_handler(0xC036, {c036_read, this});
    
    megaii->set_C0XX_write_handler(0xC029, {megaii_c0xx_write, this});
    megaii->set_C0XX_read_handler(0xC029, {megaii_c0xx_read, this});
    
    megaii->set_C0XX_read_handler(0xC068, {read_c068, this});
    megaii->set_C0XX_write_handler(0xC068, {write_c068, this});
}

void MMU_IIgs::set_ram_shadow_banks() {
    uint32_t max_shadow_bank = (reg_speed & SPEED_SHADOW_ALL) ? ram_banks : 0x02;
    if (DEBUG(DEBUG_MMUGS)) printf("setting ram shadow banks: 0x00 - 0x%02X\n", max_shadow_bank - 1);

    for (int i = 0x00; i < max_shadow_bank; i++) {
        map_page_both(i, nullptr, "MAIN_RAM");
        set_page_read_h(i, {bank_shadow_read, this}, "SHADRD");
        set_page_write_h(i, {bank_shadow_write, this}, "SHADWR");       
    }
    for (int i = max_shadow_bank; i < ram_banks; i++) {
        map_page_both(i, main_ram + i * BANK_SIZE, "MAIN_RAM");
        set_page_read_h(i, {nullptr, nullptr}, "MAIN_RAM");
        set_page_write_h(i, {nullptr, nullptr}, "MAIN_RAM");
    }
    if (DEBUG(DEBUG_MMUGS)) {
        dump_page_table(0x00, 0x03);
        dump_page_table(0xE0, 0xE1);
        dump_page_table(0xFE, 0xFF);
    }
}

void MMU_IIgs::init_map() {

    for (int i = 0; i < rom_banks; i++) {
        map_page_read_only((0x100 - rom_banks) + i, main_rom + i * BANK_SIZE, "SYS_ROM");
    }
    for (int i = 0x80; i < 0xE0; i++) {
        set_page_read_h(i, float_read_handler, "Float");
    }

    set_page_read_h(0xE0, {bank_e0_read, this}, "MEGAII");
    set_page_write_h(0xE0, {bank_e0_write, this}, "MEGAII");

    set_page_read_h(0xE1, {bank_e1_read, this}, "MEGAII");
    set_page_write_h(0xE1, {bank_e1_write, this}, "MEGAII");

    for (int i = 0xE2; i < (0x100 - rom_banks); i++) {
        set_page_read_h(i, float_read_handler, "Float");
    }

    set_ram_shadow_banks();

    // use new routine.
    for (int i = 1; i < 16; i++) {
        megaii->map_c1cf_internal_rom(0xC0 + i, main_rom + 0x1'C000 + i * GS2_PAGE_SIZE, "GS INT");
    }

    /* megaii->set_slot_rom(SLOT_1, main_rom + 0x1'C100, "GS INT");
    megaii->set_slot_rom(SLOT_2, main_rom + 0x1'C200, "GS INT");
    megaii->set_slot_rom(SLOT_3, main_rom + 0x1'C300, "GS INT");
    megaii->set_slot_rom(SLOT_4, main_rom + 0x1'C400, "GS INT");
    megaii->set_slot_rom(SLOT_5, main_rom + 0x1'C500, "GS INT");
    megaii->set_slot_rom(SLOT_6, main_rom + 0x1'C600, "GS INT"); */

    init_c0xx_handlers();
    map_initialized = true;
}

void MMU_IIgs::reset() {
    //reg_new_video = 0x01; reg_shadow = 0x08; reg_state = 0; g_80store = false; g_hires = false; g_rdrom = true; 

    reg_new_video = 0x01;
    g_hires = false;
    g_80store = false;
    g_text = true;
    g_mixed = false;

    // on RESET, set:
    set_slot_register(0x00);
    reg_speed = 0x80;
    reg_shadow = 0x08;
    set_state_register(0x0C); // KEGS does 0x0D - enable ROM RD, 
    set_intcxrom(g_intcxrom); // this is needed to set the C1-CF map correctly.

    if (map_initialized) {
        set_ram_shadow_banks();
        // Stolen from c068 handler - consolidate somewhere
        megaii_compose_map();
        bsr_map_memory();
        // also set PAGE2 display in video scanner
        megaii->write(0xC054, 0x00);    
    }
}

void MMU_IIgs::debug_dump(DebugFormatter *df) {
    df->addLine("LC: BANK_1: %d, READ_ENABLE: %d, PRE_WRITE: %d, /WRITE_ENABLE: %d", FF_BANK_1, FF_READ_ENABLE, FF_PRE_WRITE, _FF_WRITE_ENABLE);
    df->addLine("Shadow: %02X: ![IOLC: %d T2: %d AUXH: %d SHR: %d H2: %d H1: %d T1: %d]",
        reg_shadow,
        (reg_shadow & SHADOW_INH_IOLC) != 0,
        (reg_shadow & SHADOW_INH_TEXT2) != 0,
        (reg_shadow & SHADOW_INH_AUXHGR) != 0,
        (reg_shadow & SHADOW_INH_SHR) != 0,
        (reg_shadow & SHADOW_INH_HGR2) != 0,
        (reg_shadow & SHADOW_INH_HGR1) != 0,
        (reg_shadow & SHADOW_INH_TEXT1) != 0);
    df->addLine("SlotReg: %02X", reg_slot);
    df->addLine("State: %02X: INTCXROM: %d ROMBANK: %d LCBNK2: %d RDROM: %d",         
        reg_state,
        g_intcxrom,
        g_rombank,
        g_lcbnk2,
        g_rdrom
    );
    df->addLine("           RAMWRT:   %d RAMRD:   %d PAGE2:  %d ALTZP: %d",
        g_ramwrt,
        g_ramrd,
        g_page2,
        g_altzp);
    df->addLine("           80STORE:  %d HIRES:   %d", g_80store, g_hires);
    
    df->addLine("Speed: %02X", reg_speed);
    debug_output_page(df, 0x00);
    debug_output_page(df, 0x02);
    df->addLine("Mega II");
    df->addLine("C8xx_slot: %d", megaii->get_C8xx_slot());
    megaii->debug_output_page(df, 0x00);
    megaii->debug_output_page(df, 0x20);
    megaii->debug_output_page(df, 0x40);
    megaii->debug_output_page(df, 0x60);
    megaii->debug_output_page(df, 0xC3);
    megaii->debug_output_page(df, 0xC7);
    megaii->debug_output_page(df, 0xC8);
    megaii->debug_output_page(df, 0xD0);
    megaii->debug_output_page(df, 0xE0);
    megaii->debug_output_page(df, 0xFF);
}

uint8_t MMU_IIgs::vp_read(uint32_t address) {
    if (is_iolc_shadowed()) { // if IOLC is shadowed, read from ROM.
        return get_rom_base()[0x1'0000 + (address & 0xFFFF)];
    } else {
        return read(address);
    }
}
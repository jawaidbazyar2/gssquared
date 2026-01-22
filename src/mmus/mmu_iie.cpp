#include "mmu_iie.hpp"

/**
 * Sets base memory map without any specificity for various devices.
 */
void MMU_IIe::init_map() {
    // Apple IIe - first 48K is RAM, 0000 - BFFF
     for (int i = 0; i < ram_pages; i++) {
        map_page_both(i, main_ram + i * GS2_PAGE_SIZE, "MAIN_RAM");
    }
    // Next, 4K of I/O area, C000 - CFFF
    /* for (int i = 0; i < (IO_KB / GS2_PAGE_SIZE); i++) {
        map_page_both(i + 0xC0, main_io_4 + i * GS2_PAGE_SIZE, M_IO, 1, 1);
       
    } */
    // Then 12K of ROM. D000 - FFFF.
    // the iie ROM file is from $C000 - $FFFF, so skip the first 0x1000 bytes here.
    if (main_rom_D0 != nullptr) {
        for (int i = 0; i < (ROM_KB / GS2_PAGE_SIZE); i++) {
            map_page_read_only(i + 0xD0, main_rom_D0 + 0x1000 + i * GS2_PAGE_SIZE, "SYS_ROM");
        }   
    }
}

void MMU_IIe::power_on_randomize(uint8_t *ram, int ram_size) {

    // put something different in aux memory
    for (int i = 0x1'0000; i < 0x2'0000; i+=4) {
        ram[i] = 0xCC;
        ram[i+1] = 0xCC;
        ram[i+2] = 0x00;
        ram[i+3] = 0x00;
    }
    
}

void MMU_IIe::set_default_C8xx_map() {
    if (!f_intcxrom) { // only do this if slot rom is switched in.
        MMU_II::set_default_C8xx_map();
    }
}

void MMU_IIe::compose_c1cf() {
    if (!f_intcxrom) { // INTCXROM off - C1-CF is for cards, with possible exception for C3 if SLOTC3ROM is on.
        // the C8 routines put this here. this is correct.
        for (int i = 8; i < 16; i++) {
            page_table[0xC0 + i] = slot_rom_ptable[i-1];
        }
        page_table[0xC1] = reg_slot & 0x01 ? slot_rom_ptable[0] : int_rom_ptable[0];
        page_table[0xC2] = reg_slot & 0x02 ? slot_rom_ptable[1] : int_rom_ptable[1];
        page_table[0xC3] = (f_slotc3rom) ? slot_rom_ptable[2] : int_rom_ptable[2]; // different flag here.
        page_table[0xC4] = reg_slot & 0x08 ? slot_rom_ptable[3] : int_rom_ptable[3];
        page_table[0xC5] = reg_slot & 0x10 ? slot_rom_ptable[4] : int_rom_ptable[4];
        page_table[0xC6] = reg_slot & 0x20 ? slot_rom_ptable[5] : int_rom_ptable[5];
        page_table[0xC7] = reg_slot & 0x40 ? slot_rom_ptable[6] : int_rom_ptable[6];
        
        /* if (!f_slotc3rom) { // this has effect in A2Ts only if intcxrom is off.
            map_page_read_only(0xC3, main_rom_D0 + 0x0300, "SYS_ROM");
        } */
    } else { // INTCXROM is on - C1-CF is all ROM.
        for (int i = 1; i < 16; i++) {
            page_table[0xC0 + i] = int_rom_ptable[i-1];
            //map_page_read_only(0xC0 + i, main_rom_D0 + i * GS2_PAGE_SIZE, "SYS_ROM");
        }
    }
}

/*
 * When you initiate a reset, hardware in the Apple IIe sets the memory-controlling soft switches to normal: 
 * main board RAM and ROM are enabled; if there is an 80 column card in the aux slot, expansion slot 3 is allocated 
 * to the built-in 80 column firmware. auxiliary ram is disabled and the BSR is set up to read ROM and write RAM, bank 2. (hardware)
*/
void MMU_IIe::reset() {
    MMU_II::reset();
    f_intcxrom = false;
    f_slotc3rom = false;
    compose_c1cf();
}

void iie_mmu_handle_C00X_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIe *mmu = (MMU_IIe *)context;

    switch (address) {
        case 0xC006: // INTCXROMOFF
            // enable slot ROM in pages $C1 - $CF
            mmu->f_intcxrom = false;
            mmu->compose_c1cf();
            //mmu->set_default_C8xx_map(); // TODO: is this right?  https://zellyn.com/a2audit/v0/#e000b
            //printf("IIe Memory: INTCXROMOFF\n");
            break;
        case 0xC007: // INTCXROMON
            // enable main ROM in pages $C1 - $CF
            mmu->f_intcxrom = true;
            mmu->compose_c1cf();
            //printf("IIe Memory: INTCXROMON\n");
            break;
        case 0xC00A: // SLOTC3ROM
            mmu->f_slotc3rom = false;
            mmu->compose_c1cf();
            break;
        case 0xC00B: // SLOTC3ROM
            mmu->f_slotc3rom = true;
            mmu->compose_c1cf();
            break;
        default:
            break;
    }
}

/**
 * sets default C800 - CFFF map.
 * Called with location CFFF hit, or, on reset.
 */
#if 0
void MMU_IIe::set_default_C8xx_map() {
    C8xx_slot = 0xFF;
    for (uint8_t page = 0; page < 8; page++) {
        //map_page_read_only(page + 0xC8, main_io_4 + (page + 0x08) * 0x100, M_IO);
        map_page_both(page + 0xC8, nullptr, "NONE"); // 
    }
}
#endif

/**
 * read
 * Applies some of the Apple II built-in memory map logic. 
 * C000 - C0FF. 
 * C800 - CFFF.
 * CFFF to reset the C8xx map.
 */
 #if 0
uint8_t MMU_IIe::read(uint32_t address) {
    uint8_t bank = address >> 12;
    page_t page = address >> 8;
    if (bank == 0xC) {
        if (page == 0xC0) {
            read_handler_t funcptr =  C0xx_memory_read_handlers[address & 0xFF];
            if (funcptr.read != nullptr) {
                return (*funcptr.read)(funcptr.context, address);
            }
            return floating_bus_read();
        }

        /**
         * For any access in C100 - C7FF, we:
         * call any registered C8xx handler so the device can map in its own scheme from C800 - CFFF.
         */

        if (page < 0xC8) { // it's not C0, but it's less than C8 - Slot-card firmware.
            uint8_t slot = page & 0x7; // slot number is just the lower digit of page
            if (C8xx_slot != slot) call_C8xx_handler((SlotType_t)slot);
        } else 
            if (address == 0xCFFF) set_default_C8xx_map(); // When CFFF is read, reset the C8xx map to default, then execute the underlying read of CFFF.
    }
    page_table_entry_t *pte = &page_table[page];
    if (pte->read_p != nullptr) return pte->read_p[address & 0xFF];
    else if (pte->read_h.read != nullptr) return pte->read_h.read(pte->read_h.context, address);
    else return floating_bus_read();
    /* return MMU::read(address); */
}


void MMU_IIe::write(uint32_t address, uint8_t value) {
    uint8_t bank = address >> 12;
    page_t page = address >> 8;

    if (bank == 0xC) {
        if (page == 0xC0) {
            write_handler_t funcptr =  C0xx_memory_write_handlers[address & 0xFF];
            if (funcptr.write != nullptr) {
                (*funcptr.write)(funcptr.context, address, value);
            }
            return;
        }

        /** Handle the C800-CFFF mapping  */
        if (page < 0xC8) { // it's not C0, and less than C8 - Slot-card firmware area.
            uint8_t slot = (address / 0x100) & 0x7;
            if (C8xx_slot != slot) call_C8xx_handler((SlotType_t)slot);            
        } else if (address == 0xCFFF) set_default_C8xx_map();
    }

    // if there is a write handler, call it instead of writing directly.
    page_table_entry_t *pte = &page_table[page];
    if (pte->write_h.write != nullptr) pte->write_h.write(pte->write_h.context, address, value);
    else if (pte->write_p) pte->write_p[address & 0xFF] = value;
    if (pte->shadow_h.write != nullptr) pte->shadow_h.write(pte->shadow_h.context, address, value);

    /* MMU::write(address, value); */
}
#endif

#if 0
void MMU_IIe::set_C8xx_handler(SlotType_t slot, void (*handler)(void *context, SlotType_t slot), void *context) {
    C8xx_handlers[slot].handler = handler;
    C8xx_handlers[slot].context = context;
}

void MMU_IIe::call_C8xx_handler(SlotType_t slot) {
    if (C8xx_handlers[slot].handler != nullptr) {
        C8xx_handlers[slot].handler(C8xx_handlers[slot].context, slot);
    }
    C8xx_slot = slot;
}

void MMU_IIe::set_C0XX_read_handler(uint16_t address, read_handler_t handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    C0xx_memory_read_handlers[address - C0X0_BASE] = handler;
}

void MMU_IIe::set_C0XX_write_handler(uint16_t address, write_handler_t handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    C0xx_memory_write_handlers[address - C0X0_BASE] = handler;
}

// TODO: This is only used by the IIPlus LanguageCard device.
uint8_t *MMU_IIe::get_rom_base() {
    return main_rom_D0;
}

 // TODO: determine if we should use this, and if so take a read_d here.
void MMU_IIe::set_slot_rom(SlotType_t slot, uint8_t *rom, const char *name) {
    if (slot < 1 || slot >= 8) {
        return;
    }
    map_page_read_only(0xC0 + slot, rom, name);
}

void MMU_IIe::reset() {
    set_default_C8xx_map();
}

void MMU_IIe::dump_C0XX_handlers() {
    printf("C0XX handlers:\n");
    for (int i = 0; i < C0X0_SIZE; i++) {
        if (C0xx_memory_read_handlers[i].read != nullptr) {
            printf("C0%02X: %p\n", i, C0xx_memory_read_handlers[i].read);
        }
    }
}
#endif

void MMU_IIe::map_c1cf_internal_rom(page_t page, uint8_t *data, const char *read_d) {
    assert(page >= 0xC1 && page <= 0xCF);
/*     if (page < 0xC1 || page > 0xCF) {
        return;
    }
 */    page_table_entry_t *pte = &int_rom_ptable[page - 0xC1];

    pte->read_p = data;
    pte->write_p = nullptr;
    pte->read_d = read_d;
    pte->write_d = nullptr;
}

void iie_map_rom_slot3(void *context, SlotType_t slot) {
    MMU_IIe *mmu = (MMU_IIe *)context;

    // Sather UtA2E 5-28: "The C800-CFFF range is assigned to motherboard ROM any time SlotC3 ROM is reset and an 
    // access is made to $C3XX.
    if (mmu->f_slotc3rom) return; // SLOTC3ROM is -set- so this does not apply.

    uint8_t *rom = mmu->get_rom_base();

    // our virtual slot 3 also gets to map in C8-CF to internal ROM regardless of intcxrom if C3XX is accessed.
    for (uint8_t page = 0; page < 8; page++) {
        mmu->map_c1cf_page_read_only(page + 0xC8, rom + 0x800 + (page * 0x100), "SYS_ROM");
    }
}

MMU_IIe::MMU_IIe(int page_table_size, int ram_amount, uint8_t *rom_pointer) : MMU_II(page_table_size, ram_amount, rom_pointer) {
    //main_rom_D0 = rom_pointer + 0x1000;
    MMU_IIe::init_map();
    power_on_randomize(main_ram, ram_amount);

    set_C0XX_write_handler(0xC006, {iie_mmu_handle_C00X_write, this});
    set_C0XX_write_handler(0xC007, {iie_mmu_handle_C00X_write, this});
    set_C0XX_write_handler(0xC00A, {iie_mmu_handle_C00X_write, this});
    set_C0XX_write_handler(0xC00B, {iie_mmu_handle_C00X_write, this});

    set_C8xx_handler(SLOT_3, iie_map_rom_slot3, this );

    // set up map table for the internal C1-CF ROM.
    for (int i = 1; i < 16; i++) {
        map_c1cf_internal_rom(0xC0 + i, main_rom_D0 + i * GS2_PAGE_SIZE, "SYS_ROM");
    }
}

MMU_IIe::~MMU_IIe() {
    // free up memory areas.
    //delete[] main_ram;
    //delete[] main_io_4;
    //delete[] main_rom_D0;
}

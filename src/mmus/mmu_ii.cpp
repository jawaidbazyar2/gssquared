#include "mmu_ii.hpp"

/**
 * Sets base memory map without any specificity for various devices.
 */
void MMU_II::init_map() {
    // Apple II Plus - first 48K is RAM, 0000 - BFFF
     for (int i = 0; i < ram_pages; i++) {
        map_page_both(i, main_ram + i * GS2_PAGE_SIZE, "MAIN_RAM");

    }
    // Next, 4K of I/O area, C000 - CFFF
    /* for (int i = 0; i < (IO_KB / GS2_PAGE_SIZE); i++) {
        map_page_both(i + 0xC0, main_io_4 + i * GS2_PAGE_SIZE, M_IO, 1, 1);
       
    } */
    // Then 12K of ROM. D000 - FFFF
    if (main_rom_D0 != nullptr) {
        for (int i = 0; i < (ROM_KB / GS2_PAGE_SIZE); i++) {
            map_page_read_only(i + 0xD0, main_rom_D0 + i * GS2_PAGE_SIZE, "SYS_ROM");
        }   
    }
}

void MMU_II::power_on_randomize(uint8_t *ram, int ram_size) {
    for (int i = 0; i < ram_size; i+=4) {
        ram[i] = 0xFF;
        ram[i+1] = 0xFF;
        ram[i+2] = 0x00;
        ram[i+3] = 0x00;
    }
}

MMU_II::MMU_II(int page_table_size, int ram_amount, uint8_t *rom_pointer) : MMU(256) {
    //ram_pages = ram_amount / GS2_PAGE_SIZE;
    ram_pages = (48 * 1024) / GS2_PAGE_SIZE; // should be 48k worth of pages or 192 pages.
    main_ram = new uint8_t[ram_amount];
    power_on_randomize(main_ram, ram_amount);
    
    //main_io_4 = new uint8_t[IO_KB]; // TODO: we're not using this..
    main_rom_D0 = rom_pointer;

    // initialize memory map
    init_map();
    set_default_C8xx_map();
    // initialize the slot rom page table.
    for (int i = 0; i < 15; i++) {
        slot_rom_ptable[i].read_p = nullptr;
        slot_rom_ptable[i].write_p = nullptr;
        slot_rom_ptable[i].read_h = {nullptr, nullptr};
        slot_rom_ptable[i].write_h = {nullptr, nullptr};
        slot_rom_ptable[i].shadow_h = {nullptr, nullptr};
    }
}

MMU_II::~MMU_II() {
    // free up memory areas.
    delete[] main_ram;
    //delete[] main_io_4;
    delete[] main_rom_D0;
}

/**
  * Routines to provide mapping of pages in 0xC1.
  * For II,II+, semantics are the same as for calling
  * MMU::map_page_both. But, this only sets the special c1-cf page table.
  */

void MMU_II::map_c1cf_page_both(uint8_t page, uint8_t *data, const char *read_d) {
    if (page < 0xC1 || page > 0xCF) {
        return;
    }
    page_table_entry_t *pte = &slot_rom_ptable[page - 0xC1];

    pte->read_p = data;
    pte->write_p = data;
    pte->read_h = {nullptr, nullptr};
    pte->write_h = {nullptr, nullptr};
    pte->read_d = read_d;
    pte->write_d = read_d;
}

void MMU_II::map_c1cf_page_read_only(page_t page, uint8_t *data, const char *read_d) {
    if (page < 0xC1 || page > 0xCF) {
        return;
    }
    page_table_entry_t *pte = &slot_rom_ptable[page - 0xC1];

    pte->read_p = data;
    pte->write_p = nullptr;
    pte->read_d = read_d;
    pte->write_d = nullptr;
}

// TODO: don't do compose every time. But should we rely on caller to do it? OTOH it's very fast.
void MMU_II::map_c1cf_page_read_h(page_t page, read_handler_t handler, const char *read_d) {
    if (page < 0xC1 || page > 0xCF) {
        return;
    }
    page_table_entry_t *pte = &slot_rom_ptable[page - 0xC1];

    pte->read_h = handler;
    pte->read_d = read_d;
    compose_c1cf();
}

void MMU_II::map_c1cf_page_write_h(page_t page, write_handler_t handler, const char *write_d) {
    if (page < 0xC1 || page > 0xCF) {
        return;
    }
    page_table_entry_t *pte = &slot_rom_ptable[page - 0xC1];

    pte->write_h = handler;
    pte->write_d = write_d;
    compose_c1cf();
}

/**
 * sets default C800 - CFFF map.
 * Called with location CFFF hit, or, on reset.
 */
void MMU_II::set_default_C8xx_map() {
    C8xx_slot = 0xFF;
    for (uint8_t page = 0; page < 8; page++) {
        //map_page_both(page + 0xC8, nullptr, "NONE");
        map_c1cf_page_both(page + 0xC8, nullptr, "NONE");
    }
    compose_c1cf();
}

 // TODO: determine if we should use this, and if so take a read_d here.
void MMU_II::set_slot_rom(SlotType_t slot, uint8_t *rom, const char *name) {
    if (slot < 1 || slot >= 8) {
        return;
    }
    map_c1cf_page_read_only(0xC0 + slot, rom, name);
    //map_page_read_only(0xC0 + slot, rom, name);
    compose_c1cf();
}

// the handlers must only use functions in this area, to set slot-parameters.
void MMU_II::set_C8xx_handler(SlotType_t slot, void (*handler)(void *context, SlotType_t slot), void *context) {
    C8xx_handlers[slot].handler = handler;
    C8xx_handlers[slot].context = context;
}

void MMU_II::call_C8xx_handler(SlotType_t slot) {
    if (C8xx_handlers[slot].handler != nullptr) {
        C8xx_handlers[slot].handler(C8xx_handlers[slot].context, slot);
    }
    C8xx_slot = slot;
    compose_c1cf();
}

void MMU_II::compose_c1cf() {
    // only thing we do is set based on this. IIe expands on this.
    for (int i = 0; i < 15; i++) {
        page_table[0xC1 + i] = slot_rom_ptable[i];
    }
}

/**
 * read
 * Applies some of the Apple II built-in memory map logic. 
 * C000 - C0FF. 
 * C800 - CFFF.
 * CFFF to reset the C8xx map.
 */
uint8_t MMU_II::read(uint32_t address) {
    uint16_t eaddress = address & 0xFFFF;
    uint8_t bank = eaddress >> 12;
    page_t page = eaddress >> 8;
    
    if (eaddress != address) {
        printf("MMU_II::read: address %06X is out of bounds\n", address);
    }

    if (bank == 0xC) {
        if (page == 0xC0) {
            read_handler_t funcptr =  C0xx_memory_read_handlers[eaddress & 0xFF];
            if (funcptr.read != nullptr) {
                return (*funcptr.read)(funcptr.context, eaddress);
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
            if (eaddress == 0xCFFF) set_default_C8xx_map(); // When CFFF is read, reset the C8xx map to default, then execute the underlying read of CFFF.
    }
    page_table_entry_t *pte = &page_table[page];
    if (pte->read_p != nullptr) return pte->read_p[eaddress & 0xFF];
    else if (pte->read_h.read != nullptr) return pte->read_h.read(pte->read_h.context, eaddress);
    else return floating_bus_read();
    /* return MMU::read(address); */
}


void MMU_II::write(uint32_t address, uint8_t value) {
    uint16_t eaddress = address & 0xFFFF;
    uint8_t bank = eaddress >> 12;
    page_t page = eaddress >> 8;
    if (eaddress != address) {
        printf("MMU_II::write: address %06X is out of bounds\n", address);
    }

    if (bank == 0xC) {
        if (page == 0xC0) {
            write_handler_t funcptr =  C0xx_memory_write_handlers[eaddress & 0xFF];
            if (funcptr.write != nullptr) {
                (*funcptr.write)(funcptr.context, eaddress, value);
            }
            return;
        }

        /** Handle the C800-CFFF mapping  */
        if (page < 0xC8) { // it's not C0, and less than C8 - Slot-card firmware area.
            uint8_t slot = (eaddress / 0x100) & 0x7;
            if (C8xx_slot != slot) call_C8xx_handler((SlotType_t)slot);            
        } else if (eaddress == 0xCFFF) set_default_C8xx_map();
    }

    // if there is a write handler, call it instead of writing directly.
    page_table_entry_t *pte = &page_table[page];
    if (pte->write_h.write != nullptr) pte->write_h.write(pte->write_h.context, eaddress, value);
    else if (pte->write_p) pte->write_p[eaddress & 0xFF] = value;
    if (pte->shadow_h.write != nullptr) pte->shadow_h.write(pte->shadow_h.context, eaddress, value);

    /* MMU::write(address, value); */
}

void MMU_II::set_C0XX_read_handler(uint16_t address, read_handler_t handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    C0xx_memory_read_handlers[address - C0X0_BASE] = handler;
}

void MMU_II::set_C0XX_write_handler(uint16_t address, write_handler_t handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    C0xx_memory_write_handlers[address - C0X0_BASE] = handler;
}

uint8_t *MMU_II::get_rom_base() {
    return main_rom_D0;
}

void MMU_II::reset() {
    set_default_C8xx_map(); // TODO: verify if RESET resets the C8xx map.
    init_map();
}

void MMU_II::dump_C0XX_handlers() {
    printf("C0XX handlers:\n");
    for (int i = 0; i < C0X0_SIZE; i++) {
        if (C0xx_memory_read_handlers[i].read != nullptr) {
            printf("C0%02X: %p\n", i, C0xx_memory_read_handlers[i].read);
        }
    }
}
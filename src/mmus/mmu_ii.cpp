#include "mmu_ii.hpp"
#include "NClock.hpp"

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

MMU_II::MMU_II(int page_table_size, int ram_amount, uint8_t *rom_pointer) : MMU(256, GS2_PAGE_SIZE) {
    //ram_pages = ram_amount / GS2_PAGE_SIZE;
    ram_pages = (48 * 1024) / GS2_PAGE_SIZE; // should be 48k worth of pages or 192 pages.
    ram_size_ = static_cast<uint32_t>(ram_amount);
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
    // TODO: we did not allocate, so we should not deallocate this. 
    //delete[] main_rom_D0;
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
    C8xx_slot = -1;
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
        set_page_table_entry(0xC1 + i, &slot_rom_ptable[i]);
    }
}

// A slot card can watch the bus of an Apple II or IIe, where each CPU access
// is one bus cycle. This reports one access to the observer before the access
// completes: the card may supply a read byte or hold RDY (Ready) low to make
// the CPU wait. Return true only when the card supplies read data. These
// machines have no M2SEL or M2B0: slot pin 39 carries SYNC, which is not
// modelled, so each cycle is selected.
bool MMU_II::observe_bus_access(uint16_t address, uint8_t &value, bool read) {
    slot_bus_transfer_t transfer;
    transfer.address = address;
    transfer.data = value;
    if (debug_access) {
        // The debugger's access is not a bus cycle. A read may still show
        // what the card would drive.
        if (!read) return false;
        transfer.inspect = true;
        bus_observer(transfer);
        value = transfer.data;
        return transfer.driven;
    }
    // The CPU's own clock step, which follows the access, ends this cycle.
    transfer.ticks = clock ? slot_bus_cycle_end(clock->get_c14m()) : 0;
    transfer.read = read;
    const uint64_t held_ticks = bus_observer(transfer);
    if (held_ticks) advance_bus_hold(transfer.ticks + held_ticks);
    value = transfer.data;
    return transfer.driven;
}

// While RDY is low the CPU repeats its bus cycle, so each held period is one
// more cycle of the clock. ticks is the absolute 14.31818 MHz time of the PH0
// fall that ends the access; the CPU's own clock step is its last period. A
// held write stays on the bus: memory and write strobes get it again in each
// period.
void MMU_II::advance_bus_hold(uint64_t ticks) {
    if (!clock) return;
    while (slot_bus_cycle_end(clock->get_c14m()) < ticks) {
        clock->incr_cycles();
        clock->process_due();
        if (held_write_active) write(held_write_address, held_write_data);
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

    if (bus_observer) {
        uint8_t value = floating_bus_read();
        if (observe_bus_access(eaddress, value, true)) return value;
    }

    // TODO: we may have to check all the handlers to make sure they're returning correct values (i.e., just the bits they're responsible for)
    if (bank == 0xC) {
        if (page == 0xC0) {
            uint16_t subaddr = eaddress & 0xFF;
            read_handler_t funcptr =  C0xx_memory_read_handlers[subaddr].hs[0];
            uint8_t retval = 0x00;
            if (funcptr.read != nullptr) {
                retval |= (*funcptr.read)(funcptr.context, eaddress);
            }
            read_handler_t funcptr2 = C0xx_memory_read_handlers[subaddr].hs[1];
            if (funcptr2.read != nullptr) {
                uint8_t second = (*funcptr2.read)(funcptr2.context, eaddress);
                if (C0xx_read_slot1_replaces[subaddr]) {
                    retval = second;
                } else {
                    retval |= second;
                }
            }
            // if neither was called then get floating bus here.
            //retval |= floating_bus_read();
            if (funcptr.read == nullptr && funcptr2.read == nullptr) {
                retval |= floating_bus_read();
            }
            return retval;
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
    uint8_t *p = read_p[page];
    if (p != nullptr) return p[eaddress & 0xFF];
    else if (read_h[page].read != nullptr) return read_h[page].read(read_h[page].context, eaddress);
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

    // The observer sees the write, and may hold it, before it completes.
    // advance_bus_hold() calls back here for each held period.
    if (bus_observer && !held_write_active) {
        held_write_active = true;
        held_write_address = eaddress;
        held_write_data = value;
        observe_bus_access(eaddress, value, false);
        held_write_active = false;
    }

    if (bank == 0xC) {
        if (page == 0xC0) {
            uint16_t subaddr = eaddress & 0xFF;
            write_handler_t funcptr =  C0xx_memory_write_handlers[subaddr].hs[0];
            if (funcptr.write != nullptr) {
                (*funcptr.write)(funcptr.context, eaddress, value);
            }
            funcptr = C0xx_memory_write_handlers[subaddr].hs[1];
            if (funcptr.write != nullptr) {
                (*funcptr.write)(funcptr.context, eaddress, value);
            }
            return;
        }

        /** Handle the C800-CFFF mapping  */
        if (page < 0xC8) { // it's not C0, and less than C8 - Slot-card firmware area.
            uint8_t slot = (eaddress / 0x100) & 0x7; // TODO: use a bit shift instead
            if (C8xx_slot != slot) call_C8xx_handler((SlotType_t)slot);            
        } else if (eaddress == 0xCFFF) set_default_C8xx_map();
    }

    // if there is a write handler, call it instead of writing directly.
    if (write_h[page].write != nullptr) write_h[page].write(write_h[page].context, eaddress, value);
    else if (write_p[page]) write_p[page][eaddress & 0xFF] = value;
    if (shadow_h[page].write != nullptr) shadow_h[page].write(shadow_h[page].context, eaddress, value);

    /* MMU::write(address, value); */
}

void MMU_II::set_C0XX_read_handler(uint16_t address, read_handler_t handler) {
    assert(address >= C0X0_BASE && address < C0X0_BASE + C0X0_SIZE);
/*     if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        assert(false);
    } */
    uint16_t base = address & 0xFF;
    if (C0xx_memory_read_handlers[base].hs[0].read == nullptr) {
        C0xx_memory_read_handlers[base].hs[0] = handler;
    } else if (C0xx_memory_read_handlers[base].hs[1].read == nullptr) {
        C0xx_memory_read_handlers[base].hs[1] = handler;
    } else {
        assert(false); // Bug out if out of handler slots
    }
    //C0xx_memory_read_handlers[address - C0X0_BASE] = handler;
}

void MMU_II::set_C0XX_read_replaces(uint16_t address) {
    assert(address >= C0X0_BASE && address < C0X0_BASE + C0X0_SIZE);
    C0xx_read_slot1_replaces[address & 0xFF] = true;
}

void MMU_II::set_C0XX_write_handler(uint16_t address, write_handler_t handler) {
    assert(address >= C0X0_BASE && address < C0X0_BASE + C0X0_SIZE);
/*     if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        assert(false);
    } */
    uint16_t base = address & 0xFF;
    if (C0xx_memory_write_handlers[base].hs[0].write == nullptr) {
        C0xx_memory_write_handlers[base].hs[0] = handler;
    } else if (C0xx_memory_write_handlers[base].hs[1].write == nullptr) {
        C0xx_memory_write_handlers[base].hs[1] = handler;
    } else {
        assert(false); // Bug out if out of handler slots
    }
    //C0xx_memory_write_handlers[address - C0X0_BASE] = handler;
}

// TODO: these will go away once we're done migrating to double-handlers
/* void MMU_II::get_C0XX_read_handler(uint16_t address, read_handler_t &handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    handler = C0xx_memory_read_handlers[address - C0X0_BASE].hs[0];
}

void MMU_II::get_C0XX_write_handler(uint16_t address, write_handler_t &handler) {
    if (address < C0X0_BASE || address >= C0X0_BASE + C0X0_SIZE) {
        return;
    }
    handler = C0xx_memory_write_handlers[address - C0X0_BASE].hs[0];
} */

uint8_t *MMU_II::get_rom_base() {
    return main_rom_D0;
}

void MMU_II::reset(bool cold_start) {
    (void)cold_start;
    set_default_C8xx_map(); // TODO: verify if RESET resets the C8xx map.
    init_map();
}

void MMU_II::dump_C0XX_handlers() {
    printf("C0XX handlers:\n");
    for (int i = 0; i < C0X0_SIZE; i++) {
        if (C0xx_memory_read_handlers[i].hs[0].read != nullptr) {
            printf("C0%02X: %p\n", i, C0xx_memory_read_handlers[i].hs[0].read);
        }
        // TODO: display the 2nd also? instead of the address maybe put in a string name?
        /* if (C0xx_memory_read_handlers[i].hs[1].read != nullptr) {
            printf("C0%02X: %p\n", i, C0xx_memory_read_handlers[i].hs[1].read);
        } */
    }
}
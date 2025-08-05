#include <assert.h>
#include <stdio.h>

#include "mmu.hpp"

/**
 * MMU provides the memory management interface for the CPU.
 * 
 * Any memory space access here is "raw". It does not trigger cycles in the CPU.
 * 
 * The base implementation in MMU supports pages of type RAM, ROM, and IO. IO calls the memory_bus_read and memory_bus_write methods.
 * This is relatively generic. 
 */

MMU::MMU(page_t num_pages) {
    this->num_pages = num_pages;
    page_table = new page_table_entry_t[num_pages];
    for (int i = 0 ; i < num_pages ; i++) {
        //page_table[i].readable = 0;
        //page_table[i].writeable = 0;
        //page_table[i].type_r = M_NON;
        //page_table[i].type_w = M_NON;
        page_table[i].read_p = nullptr;
        page_table[i].write_p = nullptr;
        page_table[i].read_h = {nullptr, nullptr};
        page_table[i].write_h = {nullptr, nullptr};
        page_table[i].shadow_h = {nullptr, nullptr};
    }
}

MMU::~MMU() {
    delete[] page_table;
}

/* void MMU::set_cpu(cpu_state *cpu) {
    this->cpu = cpu;
} */

// Raw. Do not trigger cycles or do the IO bus stuff
uint8_t MMU::read_raw(uint32_t address) {
    uint16_t page = address / GS2_PAGE_SIZE;
    uint16_t offset = address % GS2_PAGE_SIZE;
    if (page > num_pages) return floating_bus_read();
    page_table_entry_t *pte = &page_table[page];
    if (pte->read_p == nullptr) return floating_bus_read();
    return pte->read_p[offset];
}

// no writable check here, do it higher up - this needs to be able to write to 
// memory block no matter what.
void MMU::write_raw(uint32_t address, uint8_t value) {
    uint16_t page = address / GS2_PAGE_SIZE;
    uint16_t offset = address % GS2_PAGE_SIZE;
    if (page > num_pages) return;
    page_table_entry_t *pte = &page_table[page];
    if (pte->read_p == nullptr) return;
    pte->write_p[offset] = value;
}

void MMU::write_raw_word(uint32_t address, uint16_t value) {
    write_raw(address, value & 0xFF);
    write_raw(address + 1, value >> 8);
}

#if 0
/**
 * read 
 * Perform bus read which includes potential I/O and slot-card handlers etc.
 * This is the only interface to the CPU.
 *  */
uint8_t MMU::read(uint32_t address) {
    // TODO: incr_cycles(cpu); make sure there is a CPU method that incr_cycles whenever it calls this

    uint16_t page = address / GS2_PAGE_SIZE;
    uint16_t offset = address % GS2_PAGE_SIZE;
    assert(page < num_pages);

    page_table_entry_t *pte = &page_table[page];

    if (pte->read_p != nullptr) return pte->read_p[offset];
    else if (pte->read_h.read != nullptr) return pte->read_h.read(pte->read_h.context, address);
    else return floating_bus_read();
}

/**
 * write 
 * Perform bus write which includes potential I/O and slot-card handlers etc.
 * This is the only interface to the CPU.
 * 
 * If a page has a write_h, it's "IO" and we call that handler.
 * If a page has a write_p, it is "RAM" and can be written to.
 * If a page has no write_p, it is "ROM" and cannot be written to.
 * If a page has a shadow_h, it is "shadowed memory" and we further call the shadow handler.
 *  */
void MMU::write(uint32_t address, uint8_t value) {
    uint16_t page = address / GS2_PAGE_SIZE;
    uint16_t offset = address % GS2_PAGE_SIZE;

    assert(page < num_pages);
    page_table_entry_t *pte = &page_table[page];

    // TODO: incr_cycles(cpu); make sure there is a CPU method that incr_cycles whenever it calls this
    //incr_cycles(cpu);
    
    // if there is a write handler, call it instead of writing directly.
    if (pte->write_h.write != nullptr) pte->write_h.write(pte->write_h.context, address, value);
    else if (pte->write_p) pte->write_p[offset] = value;

    if (pte->shadow_h.write != nullptr) pte->shadow_h.write(pte->shadow_h.context, address, value);

    // if none of those things were set, silently do nothing.
}
#endif

uint8_t *MMU::get_page_base_address(page_t page) {
    return page_table[page].read_p;
}

void MMU::set_floating_bus(uint8_t val) {
    floating_bus_val = val;
}

uint8_t MMU::floating_bus_read() {
    return floating_bus_val;
}

void MMU::map_page_both(page_t page, uint8_t *data, const char *read_d) {
    if (page > num_pages) {
        return;
    }
    page_table_entry_t *pte = &page_table[page];

    pte->read_p = data;
    pte->write_p = data;
    pte->read_h = {nullptr, nullptr};
    pte->write_h = {nullptr, nullptr};
    pte->read_d = read_d;
    pte->write_d = read_d;
}

// map page to read only
void MMU::map_page_read_only(page_t page, uint8_t *data, const char *read_d) {
    if (page > num_pages) {
        return;
    }
    page_table_entry_t *pte = &page_table[page];

    pte->read_p = data;
    pte->write_p = nullptr;
    pte->read_d = read_d;
    pte->write_d = nullptr;
}

void MMU::map_page_read(page_t page, uint8_t *data, const char *read_d) {
    if (page > num_pages) {
        return;
    }
    page_table_entry_t *pte = &page_table[page];
    pte->read_p = data;
    pte->read_d = read_d;
}

void MMU::map_page_write(page_t page, uint8_t *data, const char *write_d) {
    if (page > num_pages) {
        return;
    }
    page_table_entry_t *pte = &page_table[page];
    
    pte->write_p = data;
    pte->write_d = write_d;
}

/* void MMU::map_page_read_write(page_t page, uint8_t *read_data, uint8_t *write_data, ) {
    if (page > num_pages) {
        return;
    }
    page_table_entry_t *pte = &page_table[page];
    //pte->type_r = type;
    //pte->type_w = type;
    pte->read_p = read_data;
    pte->write_p = write_data;
} */

void MMU::set_page_shadow(page_t page, write_handler_t handler) {
    page_table[page].shadow_h = handler;
}

void MMU::set_page_read_h(page_t page, read_handler_t handler, const char *read_d) {
    page_table[page].read_h = handler;
    page_table[page].read_d = read_d;
}

void MMU::set_page_write_h(page_t page, write_handler_t handler, const char *write_d) {
    page_table[page].write_h = handler;
    page_table[page].write_d = write_d;
}

void MMU::dump_page_table(page_t start_page, page_t end_page) {
    /* const char *type_str[] = {
        "NON",
        "RAM",
        "ROM",
        "IO"
    }; */

    printf("Page                        R-Ptr            W-Ptr              read_h   (    context     )        write_h  (     context    )        S-Handler(     context    )\n");
    printf("-------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    for (int i = start_page ; i <= end_page ; i++) {
        printf("%02X (%8s %8s): %16p %16p %16p(%16p) %16p(%16p) %16p(%16p)\n", 
            i, 
            page_table[i].read_d, page_table[i].write_d, //page_table[i].readable, page_table[i].writeable,
            page_table[i].read_p,
            page_table[i].write_p, 
            page_table[i].read_h.read, page_table[i].read_h.context,
            page_table[i].write_h.write, page_table[i].write_h.context,
            page_table[i].shadow_h.write, page_table[i].shadow_h.context
        );
    }
}

void MMU::dump_page_table() {
    dump_page_table(0, num_pages - 1);
}

void MMU::dump_page(page_t page) {
    printf("Page %02X:\n", page);
    for (int i = 0; i < GS2_PAGE_SIZE; i++) {
        printf("%02X ", read((page << 8) | i) );
        if (i % 16 == 15) printf("\n"); // 16 bytes per line
    }
    printf("\n");
}

void MMU::reset() {
    // do nothing.
}

const char *MMU::get_read_d(page_t page) {
    return page_table[page].read_d;
}

const char *MMU::get_write_d(page_t page) {
    return page_table[page].write_d;
}

void MMU::get_page_table_entry(page_t page, page_table_entry_t *pte) {
    *pte = page_table[page];
}

void MMU::set_page_table_entry(page_t page, page_table_entry_t *pte) {
    page_table[page] = *pte;
}
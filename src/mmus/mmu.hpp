#pragma once

#include <cstdint>
#include <assert.h>

#include "memoryspecs.hpp"

#define C0X0_BASE 0xC000
#define C0X0_SIZE 0x100

enum memory_type_t {
    M_NON,
    M_RAM,
    M_ROM,
    M_IO,
};

typedef uint8_t *page_ref;
typedef uint32_t page_t;

// Function pointer type for memory bus handlers
typedef uint8_t (*memory_read_func)(void *context, uint16_t address);
typedef void (*memory_write_func)(void *context, uint16_t address, uint8_t value);

struct read_handler_t {
    memory_read_func read;
    void *context;
};

struct write_handler_t {
    memory_write_func write;
    void *context;
};

struct page_table_entry_t {
    page_ref read_p; // pointer to uint8_t pointers
    page_ref write_p;
    read_handler_t read_h;
    write_handler_t write_h;
    write_handler_t shadow_h;
    const char *read_d;
    const char *write_d;
};

class MMU {
    public:
        MMU(page_t num_pages);
        virtual ~MMU();
        //void set_cpu(cpu_state *cpu);

        void reset();
        uint8_t read_raw(uint32_t address);
        void write_raw(uint32_t address, uint8_t value);
        void write_raw_word(uint32_t address, uint16_t value);

        inline virtual uint8_t read(uint32_t address) {
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
        inline virtual void write(uint32_t address, uint8_t value) {
            uint16_t page = address / GS2_PAGE_SIZE;
            uint16_t offset = address % GS2_PAGE_SIZE;

            assert(page < num_pages);
            page_table_entry_t *pte = &page_table[page];
            
            // if there is a write handler, call it instead of writing directly.
            if (pte->write_h.write != nullptr) pte->write_h.write(pte->write_h.context, address, value);
            else if (pte->write_p) pte->write_p[offset] = value;

            if (pte->shadow_h.write != nullptr) pte->shadow_h.write(pte->shadow_h.context, address, value);

            // if none of those things were set, silently do nothing.
        }
        virtual uint8_t floating_bus_read();
        void set_floating_bus(uint8_t val);

        void map_page_both(page_t page, uint8_t *data, const char *read_d); // map page to same memory with no read or write handler.
        void map_page_read_only(page_t page, uint8_t *data, const char *read_d);
        //void map_page_read_write(page_t page, uint8_t *read_data, uint8_t *write_data/* , memory_type_t type */);
        void map_page_read(page_t page, uint8_t *data, const char *read_d);
        void map_page_write(page_t page, uint8_t *data, const char *write_d);
        void set_page_shadow(page_t page, write_handler_t handler );
        void set_page_read_h(page_t page, read_handler_t handler, const char *read_d); // set just the read handler routine
        void set_page_write_h(page_t page, write_handler_t handler, const char *write_d); // set just a write handler routine
        uint8_t *get_page_base_address(page_t page);
        const char *get_read_d(page_t page);
        const char *get_write_d(page_t page);
        void dump_page_table();
        void dump_page_table(page_t start_page, page_t end_page);
        void dump_page(page_t page);
        void get_page_table_entry(page_t page, page_table_entry_t *pte);
        void set_page_table_entry(page_t page, page_table_entry_t *pte);
    protected:
        //cpu_state *cpu;
        int num_pages = 0;
        // this is an array of info about each page.
        page_table_entry_t *page_table;
        uint8_t floating_bus_val = 0xEE;
};

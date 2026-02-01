#pragma once

#include <cstdint>
#include <assert.h>

#include "util/DebugFormatter.hpp"
#include "memoryspecs.hpp"      // not used here but used by lots of stuff that includes this.

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
typedef uint8_t (*memory_read_func)(void *context, uint32_t address);
typedef void (*memory_write_func)(void *context, uint32_t address, uint8_t value);

struct read_handler_t {
    memory_read_func read;
    void *context;
};

struct read_handler_pair_t {
    read_handler_t hs[2];
};

struct write_handler_t {
    memory_write_func write;
    void *context;
};

struct write_handler_pair_t {
    write_handler_t hs[2];
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
    protected:
        //cpu_state *cpu;
        int num_pages = 0;
        // this is an array of info about each page.
        page_table_entry_t *page_table;
        uint8_t floating_bus_val = 0xEE;
        uint32_t page_size = 0;
        uint32_t page_size_bits = 0;
        uint32_t page_size_mask = 0;

        /* static constexpr uint32_t PAGE_SIZE_BITS = __builtin_ctz(PAGE_SIZE);
        static constexpr uint32_t PAGE_MASK = PAGE_SIZE - 1; */
            
/**
 * MMU provides the memory management interface for the CPU.
 * 
 * Any memory space access here is "raw". It does not trigger cycles in the CPU.
 * 
 * The base implementation in MMU supports pages of type RAM, ROM, and IO. IO calls the memory_bus_read and memory_bus_write methods.
 * This is relatively generic. 
 */
    public:

        MMU(page_t num_pages, uint32_t page_size) {
            this->num_pages = num_pages;
            this->page_size = page_size;
            this->page_size_bits = __builtin_ctz(page_size);
            this->page_size_mask = page_size - 1;
            
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

        virtual ~MMU() {
            delete[] page_table;
        }

        // Raw. Do not trigger cycles or do the IO bus stuff
        uint8_t read_raw(uint32_t address) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
            if (page > num_pages) return floating_bus_read();
            page_table_entry_t *pte = &page_table[page];
            if (pte->read_p == nullptr) return floating_bus_read();
            return pte->read_p[offset];
        }

        // no writable check here, do it higher up - this needs to be able to write to 
        // memory block no matter what.
        void write_raw(uint32_t address, uint8_t value) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
            if (page > num_pages) return;
            page_table_entry_t *pte = &page_table[page];
            if (pte->read_p == nullptr) return;
            pte->write_p[offset] = value;
        }

        /* void write_raw_word(uint32_t address, uint16_t value) {
            write_raw(address, value & 0xFF);
            write_raw(address + 1, value >> 8);
        } */


        inline virtual uint8_t read(uint32_t address) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
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
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;

            assert(page < num_pages);
            page_table_entry_t *pte = &page_table[page];
            
            // if there is a write handler, call it instead of writing directly.
            if (pte->write_h.write != nullptr) pte->write_h.write(pte->write_h.context, address, value);
            else if (pte->write_p) pte->write_p[offset] = value;

            if (pte->shadow_h.write != nullptr) pte->shadow_h.write(pte->shadow_h.context, address, value);

            // if none of those things were set, silently do nothing.
        }

        // By default, this is the same as read.
        inline virtual uint8_t vp_read(uint32_t address) {
            return read(address);
        }

        void set_floating_bus(uint8_t val) { floating_bus_val = val; }
    
        uint8_t floating_bus_read() { return floating_bus_val; }
    

        uint8_t *get_page_base_address(page_t page) {
            return page_table[page].read_p;
        }

        void map_page_both(page_t page, uint8_t *data, const char *read_d) {
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
        void map_page_read_only(page_t page, uint8_t *data, const char *read_d) {
            if (page > num_pages) {
                return;
            }
            page_table_entry_t *pte = &page_table[page];

            pte->read_p = data;
            pte->write_p = nullptr;
            pte->read_d = read_d;
            pte->write_d = nullptr;
        }

        void map_page_read(page_t page, uint8_t *data, const char *read_d) {
            if (page > num_pages) {
                return;
            }
            page_table_entry_t *pte = &page_table[page];
            pte->read_p = data;
            pte->read_d = read_d;
        }

        void map_page_write(page_t page, uint8_t *data, const char *write_d) {
            if (page > num_pages) {
                return;
            }
            page_table_entry_t *pte = &page_table[page];
            
            pte->write_p = data;
            pte->write_d = write_d;
        }

        void set_page_shadow(page_t page, write_handler_t handler) {
            page_table[page].shadow_h = handler;
        }

        void set_page_read_h(page_t page, read_handler_t handler, const char *read_d) {
            page_table[page].read_h = handler;
            page_table[page].read_d = read_d;
        }

        void set_page_write_h(page_t page, write_handler_t handler, const char *write_d) {
            page_table[page].write_h = handler;
            page_table[page].write_d = write_d;
        }

        void dump_page_table(page_t start_page, page_t end_page) {

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

        void debug_output_page(DebugFormatter *f, page_t page, bool header = false) {
            if (header) {
                f->addLine("Page                        R-Ptr            W-Ptr              read_h   (    context     )        write_h  (     context    )        S-Handler(     context    )\n");
                f->addLine("-------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            }
            f->addLine("%02X (%8s %8s): %16p %16p %16p(%16p) %16p(%16p) %16p(%16p)\n", 
                page, 
                page_table[page].read_d, page_table[page].write_d, //page_table[i].readable, page_table[i].writeable,
                page_table[page].read_p,
                page_table[page].write_p, 
                page_table[page].read_h.read, page_table[page].read_h.context,
                page_table[page].write_h.write, page_table[page].write_h.context,
                page_table[page].shadow_h.write, page_table[page].shadow_h.context
            );
        }

        void dump_page_table() {
            dump_page_table(0, num_pages - 1);
        }

        void dump_page(page_t page) {
            printf("Page %02X:\n", page);
            for (int i = 0; i < page_size; i++) {
                printf("%02X ", read((page << 8) | i) );
                if (i % 16 == 15) printf("\n"); // 16 bytes per line
            }
            printf("\n");
        }

        virtual void reset() {
            // do nothing.
        }

        const char *get_read_d(page_t page) {
            return page_table[page].read_d;
        }

        const char *get_write_d(page_t page) {
            return page_table[page].write_d;
        }

        void get_page_table_entry(page_t page, page_table_entry_t *pte) {
            *pte = page_table[page];
        }

        void set_page_table_entry(page_t page, page_table_entry_t *pte) {
            page_table[page] = *pte;
        }

};

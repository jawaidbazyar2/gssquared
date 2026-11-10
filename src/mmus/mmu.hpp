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
        // One array per field. A read loads read_p[page] (stride 8).
        uint8_t **read_p = nullptr;
        uint8_t **write_p = nullptr;
        read_handler_t *read_h = nullptr;
        write_handler_t *write_h = nullptr;
        write_handler_t *shadow_h = nullptr;
        const char **read_d = nullptr;
        const char **write_d = nullptr;
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
        // True while the debugger reads or writes memory. Those accesses are
        // not Apple bus cycles; a device watching the bus ignores them.
        bool debug_access = false;

        MMU(page_t num_pages, uint32_t page_size) {
            this->num_pages = num_pages;
            this->page_size = page_size;
            this->page_size_bits = __builtin_ctz(page_size);
            this->page_size_mask = page_size - 1;
            
            read_p = new uint8_t *[num_pages]();
            write_p = new uint8_t *[num_pages]();
            read_h = new read_handler_t[num_pages]();
            write_h = new write_handler_t[num_pages]();
            shadow_h = new write_handler_t[num_pages]();
            read_d = new const char *[num_pages]();
            write_d = new const char *[num_pages]();
        }

        virtual ~MMU() {
            delete[] read_p;
            delete[] write_p;
            delete[] read_h;
            delete[] write_h;
            delete[] shadow_h;
            delete[] read_d;
            delete[] write_d;
        }

        /** Contiguous RAM allocation, if any. Linear offsets for MAIN_RAW / MEGAII_RAW. */
        virtual uint8_t *get_memory_base() { return nullptr; }
        virtual uint32_t get_memory_size() { return 0; }

        // Raw. Do not trigger cycles or do the IO bus stuff
        uint8_t read_raw(uint32_t address) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
            if (page > num_pages) return floating_bus_read();
            uint8_t *p = read_p[page];
            if (p == nullptr) return floating_bus_read();
            return p[offset];
        }

        // no writable check here, do it higher up - this needs to be able to write to 
        // memory block no matter what.
        void write_raw(uint32_t address, uint8_t value) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
            if (page > num_pages) return;
            if (read_p[page] == nullptr) return;
            write_p[page][offset] = value;
        }

        /* void write_raw_word(uint32_t address, uint16_t value) {
            write_raw(address, value & 0xFF);
            write_raw(address + 1, value >> 8);
        } */


        inline virtual uint8_t read(uint32_t address) {
            uint16_t page = address >> page_size_bits; // / GS2_PAGE_SIZE;
            uint16_t offset = address & page_size_mask; // % GS2_PAGE_SIZE;
            assert(page < num_pages);

            uint8_t *p = read_p[page];
            if (p != nullptr) return p[offset];
            else if (read_h[page].read != nullptr) return read_h[page].read(read_h[page].context, address);
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

            // if there is a write handler, call it instead of writing directly.
            if (write_h[page].write != nullptr) write_h[page].write(write_h[page].context, address, value);
            else if (write_p[page]) write_p[page][offset] = value;

            if (shadow_h[page].write != nullptr) shadow_h[page].write(shadow_h[page].context, address, value);

            // if none of those things were set, silently do nothing.
        }

        // By default, this is the same as read.
        inline virtual uint8_t vp_read(uint32_t address) {
            return read(address);
        }

        void set_floating_bus(uint8_t val) { floating_bus_val = val; }
    
        uint8_t floating_bus_read() { return floating_bus_val; }
    

        uint8_t *get_page_base_address(page_t page) {
            return read_p[page];
        }

        void map_page_both(page_t page, uint8_t *data, const char *read_d) {
            if (page > num_pages) {
                return;
            }
            read_p[page] = data;
            write_p[page] = data;
            read_h[page] = {nullptr, nullptr};
            write_h[page] = {nullptr, nullptr};
            this->read_d[page] = read_d;
            write_d[page] = read_d;
        }

        // map page to read only
        void map_page_read_only(page_t page, uint8_t *data, const char *read_d) {
            if (page > num_pages) {
                return;
            }
            read_p[page] = data;
            write_p[page] = nullptr;
            this->read_d[page] = read_d;
            write_d[page] = nullptr;
        }

        void map_page_read(page_t page, uint8_t *data, const char *read_d) {
            if (page > num_pages) {
                return;
            }
            read_p[page] = data;
            this->read_d[page] = read_d;
        }

        void map_page_write(page_t page, uint8_t *data, const char *write_d) {
            if (page > num_pages) {
                return;
            }
            write_p[page] = data;
            this->write_d[page] = write_d;
        }

        // Pages [lo, hi] map to consecutive page_size blocks. base is the pointer
        // for page lo (not for page 0). A null base is stored as-is on every page.
        void map_range_read(page_t lo, page_t hi, uint8_t *base, const char *read_d) {
            if (lo > hi) {
                return;
            }
            for (page_t page = lo; page <= hi; page++) {
                uint8_t *ptr = base == nullptr ? nullptr : base + (page - lo) * page_size;
                map_page_read(page, ptr, read_d);
            }
        }

        void map_range_write(page_t lo, page_t hi, uint8_t *base, const char *write_d) {
            if (lo > hi) {
                return;
            }
            for (page_t page = lo; page <= hi; page++) {
                uint8_t *ptr = base == nullptr ? nullptr : base + (page - lo) * page_size;
                map_page_write(page, ptr, write_d);
            }
        }

        void set_page_shadow(page_t page, write_handler_t handler) {
            shadow_h[page] = handler;
        }

        void set_page_read_h(page_t page, read_handler_t handler, const char *read_d) {
            read_h[page] = handler;
            this->read_d[page] = read_d;
        }

        void set_page_write_h(page_t page, write_handler_t handler, const char *write_d) {
            write_h[page] = handler;
            this->write_d[page] = write_d;
        }

        void dump_page_table(page_t start_page, page_t end_page) {

            printf("Page                        R-Ptr            W-Ptr              read_h   (    context     )        write_h  (     context    )        S-Handler(     context    )\n");
            printf("-------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            for (int i = start_page ; i <= end_page ; i++) {
                printf("%02X (%8s %8s): %16p %16p %16p(%16p) %16p(%16p) %16p(%16p)\n", 
                    i, 
                    read_d[i], write_d[i],
                    read_p[i],
                    write_p[i], 
                    read_h[i].read, read_h[i].context,
                    write_h[i].write, write_h[i].context,
                    shadow_h[i].write, shadow_h[i].context
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
                read_d[page], write_d[page],
                read_p[page],
                write_p[page], 
                read_h[page].read, read_h[page].context,
                write_h[page].write, write_h[page].context,
                shadow_h[page].write, shadow_h[page].context
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

        virtual void reset(bool cold_start = false) {
            // do nothing.
        }

        // IIgs C029: aux $2000–$9FFF is linearized (bit 6) or SHR is on (bit 7).
        // Default false so IIe/II+ and test apps need no MMU_IIgs RTTI.
        virtual bool is_aux_linear() { return false; }

        // IIgs Mega II (display RAM for banks $E0/$E1). Null on II/IIe so
        // debugger/tests need no dynamic_cast / MMU_IIgs typeinfo.
        virtual MMU *get_megaii_mmu() { return nullptr; }

        const char *get_read_d(page_t page) {
            return read_d[page];
        }

        const char *get_write_d(page_t page) {
            return write_d[page];
        }

        void get_page_table_entry(page_t page, page_table_entry_t *pte) {
            pte->read_p = read_p[page];
            pte->write_p = write_p[page];
            pte->read_h = read_h[page];
            pte->write_h = write_h[page];
            pte->shadow_h = shadow_h[page];
            pte->read_d = read_d[page];
            pte->write_d = write_d[page];
        }

        void set_page_table_entry(page_t page, page_table_entry_t *pte) {
            read_p[page] = pte->read_p;
            write_p[page] = pte->write_p;
            read_h[page] = pte->read_h;
            write_h[page] = pte->write_h;
            shadow_h[page] = pte->shadow_h;
            read_d[page] = pte->read_d;
            write_d[page] = pte->write_d;
        }

};

// Mark the debugger's memory accesses for one scope. The previous value
// returns at scope exit, so nested debugger calls keep the mark.
struct MMUDebugAccess {
    MMU *mmu;
    bool previous;
    explicit MMUDebugAccess(MMU *mmu) : mmu(mmu), previous(mmu && mmu->debug_access) {
        if (mmu) mmu->debug_access = true;
    }
    ~MMUDebugAccess() {
        if (mmu) mmu->debug_access = previous;
    }
    MMUDebugAccess(const MMUDebugAccess &) = delete;
    MMUDebugAccess &operator=(const MMUDebugAccess &) = delete;
};

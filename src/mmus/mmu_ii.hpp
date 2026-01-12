#pragma once

#include "gs2.hpp"
#include "mmu.hpp"
#include "mmu_ii.hpp"


struct C8XX_handler_t {
    void (*handler)(void *context, SlotType_t slot);
    void *context;
};

class MMU_II : public MMU {
    protected:
        int ram_pages;
        uint8_t *main_ram = nullptr;
        //uint8_t *main_io_4 = nullptr;
        uint8_t *main_rom_D0 = nullptr;

        read_handler_t C0xx_memory_read_handlers[C0X0_SIZE] = { nullptr };          // Table of memory read handlers
        write_handler_t C0xx_memory_write_handlers[C0X0_SIZE] = { nullptr };        // Table of memory write handlers

        int8_t C8xx_slot;
        C8XX_handler_t C8xx_handlers[8] = {nullptr};
        page_table_entry_t slot_rom_ptable[15]; // handle C1-CF

        virtual void power_on_randomize(uint8_t *ram, int ram_size);
        
    public:
        bool f_intcxrom = false;

        MMU_II(int page_table_size, int ram_amount, uint8_t *rom_pointer);
        MMU_II();
        virtual ~MMU_II();
        uint8_t read(uint32_t address) override;
        void write(uint32_t address, uint8_t value) override;
        
        virtual void set_C8xx_handler(SlotType_t slot, void (*handler)(void *context, SlotType_t slot), void *context);
        virtual void set_C0XX_read_handler(uint16_t address, read_handler_t handler);
        virtual void set_C0XX_write_handler(uint16_t address, write_handler_t handler);
        virtual void get_C0XX_read_handler(uint16_t address, read_handler_t &handler);
        virtual void get_C0XX_write_handler(uint16_t address, write_handler_t &handler);
        
        virtual void call_C8xx_handler(SlotType_t slot);
        virtual uint8_t *get_rom_base();
        virtual uint8_t *get_memory_base() { return main_ram; };
        virtual void init_map();
        virtual void set_default_C8xx_map();
        virtual void set_slot_rom(SlotType_t slot, uint8_t *rom, const char *name);
        virtual void reset() override;
        virtual void dump_C0XX_handlers();
        /* Handlers for "Slot ROM" area C1 - CF */
        virtual void compose_c1cf();
        virtual void map_c1cf_page_both(uint8_t page, uint8_t *data, const char *read_d);
        virtual void map_c1cf_page_read_only(page_t page, uint8_t *data, const char *read_d);
        virtual void map_c1cf_page_read_h(page_t page, read_handler_t handler, const char *read_d);
        virtual void map_c1cf_page_write_h(page_t page, write_handler_t handler, const char *write_d);
};


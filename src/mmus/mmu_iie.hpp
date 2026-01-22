#pragma once

#include "mmu_ii.hpp"

#include "mbus/MessageBus.hpp"

class MMU_IIe : public MMU_II {
    private:
        MessageBus *mbus;

        virtual void power_on_randomize(uint8_t *ram, int ram_size) override;
    protected:
        uint8_t reg_slot = 0b11110110; // slots 7-4,2-1 set to 1 here.
        page_table_entry_t int_rom_ptable[15];

    public:
        bool f_intcxrom = false;
        bool f_slotc3rom = false;

        MMU_IIe(int page_table_size, int ram_amount, uint8_t *rom_pointer);
        virtual ~MMU_IIe();
        void set_default_C8xx_map() override;
        void compose_c1cf() override;
        void set_slot_register(uint8_t value) { reg_slot = value; compose_c1cf(); }
        uint8_t get_slot_register() { return reg_slot; }
        void map_c1cf_internal_rom(page_t page, uint8_t *data, const char *read_d);

        void init_map() override;
        void reset() override;
};

void iie_mmu_handle_C00X_write(void *context, uint16_t address, uint8_t value);

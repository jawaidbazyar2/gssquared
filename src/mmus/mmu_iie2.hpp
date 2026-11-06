#pragma once

#include "mmu_ii.hpp"
#include "devices/languagecard/LanguageCardLogic.hpp"

class MessageBus;

/**
 * IIe motherboard memory map: slot ROM ($C1-$CF) plus auxiliary RAM and the
 * language-card window. Independent of MMU_IIe. AUX 0 is
 * get_memory_base() + 0x10000. Composers publish auxiliary pages from cpu_aux.
 * With no RamWorks card, cpu_aux stays on AUX 0.
 *
 * install_memory_switches selects the aux / language-card handlers. The slot-ROM
 * writes ($C006, $C007, $C00A, $C00B) are always installed. The flag exists so a
 * later Mega II can construct this class without claiming those soft switches.
 */
class MMU_IIe2 : public MMU_II {
    protected:
        uint8_t reg_slot = 0b11110110;
        page_table_entry_t int_rom_ptable[15];

        bool f_80store = false;
        bool f_ramrd = false;
        bool f_ramwrt = false;
        bool f_altzp = false;
        bool s_page2 = false;
        bool s_hires = false;

        bool m_zp = false;
        bool m_text1_r = false;
        bool m_text1_w = false;
        bool m_hires1_r = false;
        bool m_hires1_w = false;
        bool m_all_r = false;
        bool m_all_w = false;

        bool install_memory_switches_ = true;
        MessageBus *mbus = nullptr;

        void power_on_randomize(uint8_t *ram, int ram_size) override;
        void map_range(int lo, int hi, bool aux, bool is_write);
        void install_slot_rom();
        void install_memory_handlers();
        uint8_t key_low() const;

        static void c00x_write(void *context, uint32_t address, uint8_t value);
        static void aux_c00x_write(void *context, uint32_t address, uint8_t value);
        static uint8_t c01x_read(void *context, uint32_t address);
        static uint8_t display_read(void *context, uint32_t address);
        static void display_write(void *context, uint32_t address, uint8_t value);
        static uint8_t lc_read(void *context, uint32_t address);
        static void lc_write(void *context, uint32_t address, uint8_t value);
        static void map_rom_slot3(void *context, SlotType_t slot);

    public:
        bool f_slotc3rom = false;
        LanguageCardLogic ll;

        MMU_IIe2(int page_table_size, int ram_amount, uint8_t *rom_pointer,
                 bool install_memory_switches = true);
        ~MMU_IIe2() override;

        void set_message_bus(MessageBus *bus) { mbus = bus; }

        void set_intcxrom(bool on);
        void set_slotc3rom(bool on);
        void set_rom_base(uint8_t *rom) { main_rom_D0 = rom; }
        void set_80store(bool on) { f_80store = on; }
        void set_ramrd(bool on) { f_ramrd = on; }
        void set_ramwrt(bool on) { f_ramwrt = on; }
        void set_altzp(bool on) { f_altzp = on; }
        void set_page2(bool on) { s_page2 = on; }
        void set_hires(bool on) { s_hires = on; }

        /** CPU's current auxiliary 64K. AUX 0 is main_ram + 0x10000. */
        uint8_t *cpu_aux = nullptr;
        /** AUX 1..127. Null until a RamWorks card calls install_ramworks(). */
        uint8_t *aux_extra = nullptr;
        uint8_t aux_bank = 0;

        static constexpr int kRamWorksBankCount = 128;
        static constexpr uint32_t kAuxBankBytes = 0x10000;

        uint8_t *cpu_aux_base() const { return cpu_aux; }
        uint8_t get_aux_bank() const { return aux_bank; }
        bool ramworks_installed() const { return aux_extra != nullptr; }
        /** Allocate AUX 1..127 beside the existing 128K block. AUX 0 stays put. */
        void install_ramworks();
        /** Mask to 0x7F. Bank 0 is AUX 0; bank k>0 is aux_extra + (k-1)*64K.
         *  Recomposes only when memory switches are installed. */
        void set_aux_bank(uint8_t bank);

        void set_default_C8xx_map() override;
        void compose_c1cf() override;
        void set_slot_register(uint8_t value) { reg_slot = value; compose_c1cf(); }
        uint8_t get_slot_register() const { return reg_slot; }
        void map_c1cf_internal_rom(page_t page, uint8_t *data, const char *read_d);

        void init_map() override;
        void reset(bool cold_start = false) override;

        void bsr_map_memory();
        void compose_map(bool force);
        DebugFormatter *debug_formatter();
};

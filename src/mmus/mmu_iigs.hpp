#pragma once

#include "mmu.hpp"
#include "mmu_iie.hpp"
#include "iigs_shadow_flags.hpp"
#include "debug.hpp"
#include "NClock.hpp"
#include "devices/languagecard/LanguageCardLogic.hpp"

class MMU_IIgs : public MMU {
    protected:
        uint32_t ram_banks;
        uint8_t *main_ram = nullptr;
        uint32_t rom_banks;
        uint8_t *main_rom = nullptr;
        constexpr static uint32_t BANK_SIZE = 65536;

        uint8_t reg_slot = 0;
        uint8_t reg_shadow = 0;
        uint8_t reg_speed = 0x08;
        union {
            uint8_t reg_state = 0;
            struct {
                uint8_t g_intcxrom : 1;
                uint8_t g_rombank : 1;
                uint8_t g_lcbnk2 : 1; // 1 = LC Bank 2 Selected; 0 = LC Bank 1 Selected
                uint8_t g_rdrom : 1;
                uint8_t g_ramwrt : 1;
                uint8_t g_ramrd : 1;
                uint8_t g_page2 : 1;
                uint8_t g_altzp : 1; // 1 = ZP, Stack, LC are in Aux; 0 = in Main
            };
        };
        uint8_t g_80store;
        uint8_t g_hires;
        bool g_text;
        bool g_mixed;
            
        union {
            uint8_t reg_new_video=0;
            struct {
                uint8_t g_bank_latch: 1;
                uint8_t g_reserved1 : 4;
                uint8_t g_dhr_mono : 1;
                uint8_t g_aux_linear : 1;
                uint8_t g_shr_enabled : 1;                
            };
        };
        /* bool f_intcxrom = false;
        bool f_slotc3rom = false; */

        // "current memory map state" flags.
        // does not imply we need to track text s/s
        bool m_zp = false; // this is both read and write.
        bool m_text1_r = false; // 
        bool m_text1_w = false; // 
        bool m_hires1_r = false; // 
        bool m_hires1_w = false; // 
        bool m_all_r = false; // 
        bool m_all_w = false; //

        bool map_initialized = false;

        bool is_rom03 = false;

        //cpu_state *cpu = nullptr;
        NClock *clock = nullptr;

    public:
        MMU_IIe *megaii = nullptr;
        LanguageCardLogic ll;

        MMU_IIgs(size_t num_banks, int ram_size, uint32_t rom_size, uint8_t *rom, MMU_IIe *mmu_iie) : MMU(num_banks, BANK_SIZE), megaii(mmu_iie) {
            ram_banks = ram_size / BANK_SIZE;
            main_ram = new uint8_t[ram_banks * BANK_SIZE];
            rom_banks = rom_size / BANK_SIZE;
            main_rom = rom;
            map_initialized = false;
            reset();
        };
        virtual ~MMU_IIgs() { delete main_ram; delete main_rom; };

        virtual uint8_t read(uint32_t address) override {
            if (address >= 0xFC0000) set_next_cycle_type(CYCLE_TYPE_FAST_ROM); // rom access is fast.

            return MMU::read(address);
        }

        virtual void write(uint32_t address, uint8_t value) override {
            if (address >= 0xFC0000) set_next_cycle_type(CYCLE_TYPE_FAST_ROM); // rom access is fast.

            MMU::write(address, value);
        }

        inline bool shadow_is_enabled(uint32_t address) {
            uint32_t address_16 = address & 0xFFFF;
            uint32_t address_17 = address & 0x1FFFF;
            
            if ((address_16 >= 0x0400 && address_16 <= 0x07FF) && !(reg_shadow & SHADOW_INH_TEXT1)) {
                return true;
            }
            if (is_rom03 && ((address_16 >= 0x0800 && address_16 <= 0x0BFF) && !(reg_shadow & SHADOW_INH_TEXT2))) {
                return true;
            }
            if ((address_16 >= 0x2000 && address_16 <= 0x3FFF) && !(reg_shadow & SHADOW_INH_HGR1)) {
                return true;
            }
            if ((address_16 >= 0x4000 && address_16 <= 0x5FFF) && !(reg_shadow & SHADOW_INH_HGR2)) {
                return true;
            }
            if ((address_17 >= 0x12000 && address_17 <= 0x19FFF) && !(reg_shadow & SHADOW_INH_SHR)) {
                return true;
            }
            if ((address_17 >= 0x12000 && address_17 <= 0x15FFF) && !(reg_shadow & SHADOW_INH_AUXHGR)) {
                return true;
            }
            
            
            return false;
        }

        inline uint8_t megaiiRead(uint32_t address) {
            if ((address & 0x1'0000) && g_bank_latch) {
                return megaii->get_memory_base()[address & 0x1'FFFF]; // does not currently have an interface for this
            }
            else return megaii->read(address & 0xFFFF);
        }

        inline void megaiiWrite(uint32_t address, uint8_t value) { 
            if ((address & 0x1'0000) && g_bank_latch)
                megaii->get_memory_base()[address & 0x1'FFFF] = value; 
            else megaii->write(address & 0xFFFF, value);
        }

        void megaii_compose_map();

        inline bool is_iolc_shadowed() { return !(reg_shadow & SHADOW_INH_IOLC); }
        inline bool is_bank_latch() { return g_bank_latch; }

        inline void set_shadow_register(uint8_t value) { if (DEBUG(DEBUG_MMUGS)) printf("setting shadow register: %02X\n", value); reg_shadow = value; }
        inline void set_speed_register(uint8_t value) { 
            if (DEBUG(DEBUG_MMUGS)) printf("setting speed register: %02X\n", value); 
            reg_speed = value;
            if (clock) ((NClockIIgs *)clock)->set_slow_mode(value & 0x80 ? false : true);
        }
        inline void set_state_register(uint8_t value) { 
            if (DEBUG(DEBUG_MMUGS)) printf("setting state register: %02X\n", value); 
            reg_state = value; 
            ll.FF_READ_ENABLE = !g_rdrom; // sync LC state with state reg 
            ll.FF_BANK_1 = !g_lcbnk2; // this was missing.. 
        }
        inline uint8_t shadow_register() { return reg_shadow; }
        inline uint8_t speed_register() { return reg_speed; }
        inline uint8_t state_register() { return reg_state; }

        void set_ram_shadow_banks();
        //void shadow_register(uint16_t address, bool rw); // track accesses to softswitches the FPI also tracks.
        inline bool is_lc_bank1() { return ll.FF_BANK_1 == 1; }
        inline void set_lc_bank1(bool value) { ll.FF_BANK_1 = value; g_lcbnk2 = !value; }
        inline bool is_lc_read_enable() { return ll.FF_READ_ENABLE == 1; }
        inline void set_lc_read_enable(bool value) { ll.FF_READ_ENABLE = value; g_rdrom = !value; }
        inline bool is_lc_pre_write() { return ll.FF_PRE_WRITE == 1; }
        inline void set_lc_pre_write(bool value) { ll.FF_PRE_WRITE = value; }
        inline bool is_lc_write_enable() { return ll._FF_WRITE_ENABLE == 0; } // reverse sense since this is active low
        inline void set_lc_write_enable(bool value) { ll._FF_WRITE_ENABLE = value; }
        inline bool is_page2() { return g_page2; }
        inline void set_page2(bool value) { g_page2 = value; }
        inline bool is_hires() { return g_hires; }
        inline void set_hires(bool value) { g_hires = value; }
        inline bool is_text() { return g_text; }
        inline void set_text(bool value) { g_text = value; }
        inline bool is_mixed() { return g_mixed; }
        inline void set_mixed(bool value) { g_mixed = value; }
        inline bool is_altzp() { return g_altzp; }
        inline void set_altzp(bool value) { g_altzp = value; }
        
        inline bool is_80store() { return g_80store ? true : false; }
        inline bool is_slotc3rom() { return megaii->f_slotc3rom ? true : false; }
        inline void set_intcxrom(bool value);
        
        inline void set_slot_register(uint8_t value);
        
        inline uint8_t get_slot_register() { return reg_slot; }

        void bsr_map_memory();
        virtual uint8_t vp_read(uint32_t address) override;

        uint32_t calc_aux_write(uint32_t address);
        uint32_t calc_aux_read(uint32_t address);
        void init_c0xx_handlers();
        void write_c0xx(uint16_t address, uint8_t value);
        uint8_t read_c0xx(uint16_t address);
        
        virtual uint8_t *get_rom_base() { return main_rom; };
        virtual uint8_t *get_memory_base() { return main_ram; };
        virtual void init_map();
        virtual void reset() override;
        void debug_dump(DebugFormatter *df);

        inline void set_clock(NClockII *clock) { this->clock = clock; }
        inline void set_clock_mode(clock_mode_t mode) { clock->set_clock_mode(mode); }
        inline void set_next_cycle_type(cycle_type_t type) { if (clock) ((NClockIIgs *)clock)->set_next_cycle_type(type); }
        inline void set_slow_mode(bool value) { ((NClockIIgs *)clock)->set_slow_mode(value); }
};

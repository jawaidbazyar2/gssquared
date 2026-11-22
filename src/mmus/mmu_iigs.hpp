#pragma once

#include "mmu.hpp"
#include "mmu_iie2.hpp"
#include "iigs_shadow_flags.hpp"
#include "iigs_aux_linear.hpp"
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
        /* The FPI decodes banks $F0-$FF as ROM space and runs those cycles at full
           speed with no RAM-refresh stretch, whether or not a ROM answers. Motherboard
           ROM sits at the top of the window (ROM01: $FE-$FF, ROM03: $FC-$FF); the rest
           is where expansion-card ROM lands. */
        constexpr static uint32_t ROM_SPACE_BASE = 0xF00000;

        uint8_t reg_slot = 0;
        uint8_t reg_shadow = 0;
        /* 1K slots in the low 17 address bits. Filled once in the constructor.
           reg_shadow stays live.
           or_mask: shadow when any listed inhibit bit is clear.
           and_mask: shadow when the mask is nonzero and every listed bit is clear.
           An OR of inhibit bits cannot express odd $2000-$5FFF: that page copies
           when its page inhibit and AUXHGR are both clear, and also when SHR is clear. */
        struct shadow_slot {
            uint8_t or_mask;
            uint8_t and_mask;
        };
        shadow_slot shadow_mask[128]{};
        void fill_shadow_mask();
        /* One region bit per 256-byte page. Filled once in the constructor.
           aux_read_flags / aux_write_flags stay live and are packed by megaii_compose_map. */
        uint8_t aux_region[256]{};
        uint8_t aux_read_flags{};
        uint8_t aux_write_flags{};
        void fill_aux_region();
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

        uint8_t dma_bank_register = 0;

        //cpu_state *cpu = nullptr;
        NClock *clock = nullptr;
        // Only the outstanding CPU write owns these strobes. A null RAM
        // destination means use the ordinary selected Mega II bus decoder.
        uint8_t *held_write_destination = nullptr;
        uint16_t held_write_address = 0;
        uint8_t held_write_data = 0;
        bool held_write_active = false;
        // M2B0 is on slots 1-6 of a ROM 03 board and only on slot 3 of a
        // ROM 01 board (Apple IIgs Technical Note 68). Slot 7 has the color
        // reference on that pin.
        bool slot_has_m2b0(SlotType_t slot) const {
            return is_rom03 ? slot >= SLOT_1 && slot <= SLOT_6 : slot == SLOT_3;
        }
        // Called only with an installed observer; the check for one stays
        // inline so ordinary Apple accesses need no callback or transfer record.
        bool observe_megaii_access(uint32_t address, uint8_t &value, bool read);
        void write_megaii_observed_access(uint32_t address, uint8_t value, uint8_t *destination);

    public:
        MMU_IIe2 *megaii = nullptr;
        LanguageCardLogic ll;

        MMU_IIgs(size_t num_banks, int ram_size, uint32_t rom_size, uint8_t *rom, MMU_IIe2 *mmu_iie2) : MMU(num_banks, BANK_SIZE), megaii(mmu_iie2) {
            ram_banks = ram_size / BANK_SIZE;
            main_ram = new uint8_t[ram_banks * BANK_SIZE];
            rom_banks = rom_size / BANK_SIZE;
            is_rom03 = rom_banks >= 4;
            fill_shadow_mask();
            fill_aux_region();
            main_rom = rom;
            map_initialized = false;
            dma_bank_register = 0;
            reset(true); // power-on: ROM03 CYAREG bit 6 set when cold_start
        };

        virtual ~MMU_IIgs() { delete[] main_ram; /* main_rom is owned by caller */ }

        virtual uint8_t read(uint32_t address) override {
            if (address >= ROM_SPACE_BASE) set_next_cycle_type(CYCLE_TYPE_FAST_ROM); // rom access is fast.

            return MMU::read(address);
        }

        virtual void write(uint32_t address, uint8_t value) override {
            if (address >= ROM_SPACE_BASE) set_next_cycle_type(CYCLE_TYPE_FAST_ROM); // rom access is fast.

            MMU::write(address, value);
        }

        /* Odd $2000-$3FFF: (HGR1 clear AND AUXHGR clear) OR SHR clear.
           Odd $4000-$5FFF: (HGR2 clear AND AUXHGR clear) OR SHR clear. */
        inline bool shadow_is_enabled(uint32_t address) {
            const uint32_t slot = (address >> 10) & 0x7F;
            const shadow_slot &entry = shadow_mask[slot];
            const uint8_t reg = reg_shadow;
            const bool or_enabled = (entry.or_mask & static_cast<uint8_t>(~reg)) != 0;
            const bool and_enabled = entry.and_mask != 0 && (entry.and_mask & reg) == 0;
            return or_enabled || and_enabled;
        }

        const uint8_t *shadow_mask_bytes() const {
            return reinterpret_cast<const uint8_t *>(shadow_mask);
        }
        size_t shadow_mask_nbytes() const { return sizeof(shadow_mask); }

        /* Odd bank returns 0. Otherwise one region bit selects main (0) or aux (0x10000). */
        inline uint32_t calc_aux_read(uint32_t address) {
            if (address & 0x10000) return 0;
            const uint32_t page = (address >> 8) & 0xFF;
            const bool use_aux = (aux_read_flags & aux_region[page]) != 0;
            return static_cast<uint32_t>(use_aux) << 16;
        }
        inline uint32_t calc_aux_write(uint32_t address) {
            if (address & 0x10000) return 0;
            const uint32_t page = (address >> 8) & 0xFF;
            const bool use_aux = (aux_write_flags & aux_region[page]) != 0;
            return static_cast<uint32_t>(use_aux) << 16;
        }

        const uint8_t *aux_region_bytes() const { return aux_region; }

        // TODO: remove, not used anywhere any more. bank_e1_read handles bank latch directly.
        /* inline uint8_t megaiiRead(uint32_t address) {
            if ((address & 0x1'0000) && g_bank_latch) {
                return megaii->get_memory_base()[address & 0x1'FFFF]; // does not currently have an interface for this
            }
            else return megaii->read(address & 0xFFFF);
        } */

        // Called on the thread that runs the CPU, never on a card's own
        // thread. Only the current retained access may advance motherboard
        // time while RDY holds it.
        void advance_bus_hold(uint64_t ticks) override;
        // Perform a write with a retained physical destination across RDY.
        inline void write_megaii_observed(uint32_t address, uint8_t value, uint8_t *destination = nullptr) {
            // Keep the ordinary store inline when no card observes the bus.
            if (bus_observer) write_megaii_observed_access(address, value, destination);
            else if (destination) *destination = value;
            else megaii->write(address & 0xffff, value);
        }

        // Report an original CPU address before completion; true means the card
        // supplied value for a read. Any card delay advances the host clock.
        bool observe_megaii(uint32_t address, uint8_t &value, bool read) {
            return bus_observer && observe_megaii_access(address, value, read);
        }
        // Fetch the retained bus byte only when a card can answer. Without
        // a card, use ordinary Mega II decode directly.
        uint8_t megaii_bus_read(uint32_t address) {
            if (bus_observer) {
                uint8_t value = megaii->floating_bus_read();
                if (observe_megaii_access(address, value, true)) return value;
            }
            return megaii->read(address & 0xffff);
        }
        // Capture and finish an RDY hold before the ordinary write completes.
        void megaii_bus_write(uint32_t address, uint8_t value) {
            write_megaii_observed(address, value);
        }

        // A shadow copy also reaches the observer. m2b0 is the original CPU
        // bank bit, saved before soft switches redirected the RAM destination
        // (Apple IIgs Technical Note 68, "The Mega II Bank 0 Signal").
        inline void megaiiWrite(uint32_t address, uint8_t value, bool m2b0) {
            // Select the bus cycle before the observer runs: the periods of
            // an RDY hold are slow cycles too.
            set_next_cycle_type(CYCLE_TYPE_SYNC);
            const uint32_t original = (address & 0xffff) | (uint32_t(m2b0) << 16);
            if ((address & 0x1'0000) && g_bank_latch) {
                uint32_t idx = address & 0x1'FFFF;
                if (is_aux_linear()) {
                    idx = 0x1'0000 | iigs_aux_linear_to_phys((uint16_t)idx);
                }
                write_megaii_observed(original, value, megaii->get_memory_base() + idx);
            } else {
                write_megaii_observed(original, value);
            }
        }

        void megaii_compose_map();

        inline bool is_iolc_shadowed() { return !(reg_shadow & SHADOW_INH_IOLC); }
        inline bool is_bank_latch() { return g_bank_latch; }
        /* Enabling SHR ($C029 bit 7) linearizes aux $2000-$9FFF too, not just the
           explicit linearize bit 6. GS/OS draws its boot screen with $41 (linear,
           display off) then switches to $81, and expects the mapping to persist. */
        inline bool is_aux_linear() override { return g_aux_linear || g_shr_enabled; }
        MMU *get_megaii_mmu() override { return megaii; }

        /* Physical aux index for a direct bank $E1 access (bank latch on). Aux is
           forced, so ALTZP / RAMRD / RAMWRT / 80STORE / PAGE2 do not apply here -
           those select main vs aux for bank $E0. The LC bank 2 $D000 image (parked in
           the $C000 I/O hole, as in bsr_map_memory) and the linear/SHR aux mapping do
           still apply; the two ranges are disjoint. */
        inline uint32_t e1_aux_index(uint16_t a16) {
            if (a16 >= 0xD000 && a16 <= 0xDFFF && !is_lc_bank1()) return 0x1'0000 | (a16 - 0x1000);
            if (is_aux_linear()) return 0x1'0000 | iigs_aux_linear_to_phys(a16);
            return 0x1'0000 | a16;
        }

        inline void set_shadow_register(uint8_t value) { if (DEBUG(DEBUG_MMUGS)) printf("setting shadow register: %02X\n", value); reg_shadow = value; }
        inline void set_speed_register(uint8_t value) { 
            if (DEBUG(DEBUG_MMUGS)) printf("setting speed register: %02X\n", value); 
            reg_speed = value;
            if (clock) ((NClockIIgs *)clock)->set_slow_mode(value & 0x80 ? false : true);
        }
        inline void set_state_register(uint8_t value) { 
            if (DEBUG(DEBUG_MMUGS)) printf("setting state register: %02X\n", value); 
            const uint8_t changed = static_cast<uint8_t>(reg_state ^ value);
            reg_state = value; 
            ll.FF_READ_ENABLE = !g_rdrom; // sync LC state with state reg 
            ll.FF_BANK_1 = !g_lcbnk2; // this was missing.. 
            if (!map_initialized) return;
            // ROM 03 keeps ROMBANK in this register and does not rebuild ROM for it.
            if (is_rom03 && (changed & ~0x02) == 0) return;
            sync_rom_windows();
        }
        /* $C028 read or write toggles C068 bit 1. The data byte is ignored.
           MAME's do_io($28) does the same: it never receives the data byte, so
           bit 7 does not choose ROM versus RAM. RAM versus ROM stays the
           language-card RDROM bit. */
        void toggle_rombank();
        void sync_rom_windows();
        void map_internal_cx_rom();
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
        inline bool is_ramrd() { return g_ramrd; }
        inline bool is_ramwrt() { return g_ramwrt; }

        inline bool is_80store() { return g_80store ? true : false; }
        inline bool is_slotc3rom() { return megaii->f_slotc3rom ? true : false; }
        inline void set_intcxrom(bool value);
        
        inline void set_slot_register(uint8_t value);
        inline uint8_t get_slot_register() { return reg_slot; }

        inline void set_dma_bank_register(uint8_t value) { dma_bank_register = value; }
        inline uint8_t get_dma_bank_register() { return dma_bank_register; }

        void bsr_map_memory();
        virtual uint8_t vp_read(uint32_t address) override;

        void init_c0xx_handlers();
        void write_c0xx(uint16_t address, uint8_t value);
        uint8_t read_c0xx(uint16_t address);
        
        virtual uint8_t *get_rom_base() { return main_rom; };

        // Byte offset within main_rom of the start of virtual bank $FF (the
        // top bank, always the *last* physical bank in the ROM image
        // regardless of total ROM size). ROM01 is 2 banks (128KB) -> 0x10000;
        // ROM03 is 4 banks (256KB) -> 0x30000. Fixed-offset lookups (LC-area
        // ROM overlay reads, the $C100-$CFFF slot-firmware mapping, the
        // embedded IIe-compat ROM pointer) key off bank $FF and used to
        // hardcode 0x10000, silently only correct for a 128KB ROM.
        inline uint32_t rom_bank_ff_offset() const { return (rom_banks - 1) * BANK_SIZE; }

        /* ROM 01 ROMBANK forces A14 low on the Mega II windows that alias bank $FF:
           $C071–$C07F, internal $C1xx–$CFxx, and language-card ROM $D000–$FFFF.
           $D000 becomes $9000. Direct $FE/$FF reads do not come through here.
           ROM 03 stores the bit (CYA pin 60 is grounded) and does not move A14. */
        inline uint16_t mega_rom_offset(uint16_t a16) const {
            if (!is_rom03 && g_rombank) a16 = static_cast<uint16_t>(a16 & ~0x4000);
            return a16;
        }
        inline uint8_t read_mega_rom(uint16_t a16) const {
            return main_rom[rom_bank_ff_offset() + mega_rom_offset(a16)];
        }
        uint8_t *get_memory_base() override { return main_ram; }
        uint32_t get_memory_size() override { return ram_banks * BANK_SIZE; }
        inline bool is_rom03_machine() const { return is_rom03; }

        virtual void init_map();
        virtual void reset(bool cold_start = false) override;
        void debug_dump(DebugFormatter *df);

        inline void set_clock(NClockII *clock) { this->clock = clock; }
        inline void set_clock_mode(clock_mode_t mode) { clock->set_clock_mode(mode); }
        inline void set_next_cycle_type(cycle_type_t type) { if (clock) ((NClockIIgs *)clock)->set_next_cycle_type(type); }
        inline void set_slow_mode(bool value) { ((NClockIIgs *)clock)->set_slow_mode(value); }
};

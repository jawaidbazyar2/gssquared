#include "mmu_iigs.hpp"
#include "NClock.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "mmus/mmu.hpp"
#include "memoryspecs.hpp"
#include <cstring>

// Second Sight watches the slow Apple II bus supplied by the Mega II chip.
// Call it before completing the access: it may supply a read byte or hold
// RDY (Ready) low to make the Apple wait for its firmware. address is the
// original CPU address, value carries the byte, and read selects direction.
// Return true only when the card supplies read data. The delay is counted
// in ticks of the host's 14.31818 MHz clock, about 70 ns each.
// Source: Apple IIgs Technical Note 68, Mega II Select and Ready sections.
bool MMU_IIgs::observe_megaii_access(uint32_t address, uint8_t &value, bool read) {
    // The FPI (Fast Processor Interface) serves these accesses without the
    // slow bus. /M2SEL is the active-low signal that tells a slot card the
    // slow-bus address is valid. Reporting these accesses to Second Sight
    // would invent card events. STATEREG ($C068) writes do reach the bus;
    // reads do not. See Apple IIgs Technical Note 68, "The Mega II Select
    // Signal"; the $C071-$C07F read-only ROM is likewise served on the fast side.
    const uint16_t a = address;
    if (a == 0xc035 || a == 0xc036 || a == 0xc037) return false;
    if (read && (a == 0xc02d || a == 0xc068 || (a >= 0xc071 && a <= 0xc07f))) return false;
    megaii_transfer_t transfer;
    // The bus record names the first physical data-phase fall, rather than
    // the instruction's request time. The following CPU increment retires
    // this same cycle; it must not charge another period after an RDY hold.
    transfer.ticks = clock ? ((NClockIIgs *)clock)->begin_slot_cycle() : 0;
    if (held_write_active) held_write_fall = transfer.ticks;
    transfer.address = address;
    transfer.data = value;
    transfer.read = read;
    // Second Sight needs the CPU bank bit (M2B0), before soft switches choose
    // main or auxiliary RAM. A soft switch is an address that controls a
    // hardware setting. Technical Note 68 explicitly distinguishes bank and RAM selection.
    transfer.m2b0 = (address >> 16) & 1;
    uint64_t held_ticks = megaii_observer(transfer);
    // A card RDY hold postpones completion of this Apple access, so advance
    // the host clock as well as the card before returning its data.
    if (clock && held_ticks) advance_megaii_hold(transfer.ticks + held_ticks);
    value = transfer.data;
    return transfer.driven;
}

// Second Sight reports resolved RDY time in absolute 14.31818 MHz ticks.
// Repeat the retained write at each Apple bus fall while the CPU waits.
void MMU_IIgs::advance_megaii_hold(uint64_t ticks) {
    if (!clock) return;
    auto *gs_clock = (NClockIIgs *)clock;
    // The retained request is physically presented on every occupied PH0
    // period. RAM and selected write strobes remain live while the 65816
    // waits; subsequent CPU writes cannot occur (Apple IIgs Technical Note
    // 68, pp. 3-4; Cortland Custom ICs, 14 February 1986, pp. 22-24).
    while (held_write_active && held_write_fall && held_write_fall <= ticks) {
        gs_clock->advance_slot_hold(held_write_fall);
        if (held_write_destination) *held_write_destination = held_write_data;
        else megaii->write(held_write_address, held_write_data);
        held_write_strobed = true;
        held_write_fall += NClockIIgs::slot_period(held_write_fall % 912 == 896);
    }
    gs_clock->advance_slot_hold(ticks);
}

// Keep the original write byte and physical RAM destination across a card
// wait. Null destination selects ordinary Mega II decoding. A write already
// strobed during the hold must not be repeated again on function return.
void MMU_IIgs::write_megaii_observed_access(uint32_t address, uint8_t value, uint8_t *destination) {
    held_write_active = true;
    held_write_strobed = false;
    held_write_fall = 0;
    held_write_destination = destination;
    held_write_address = address;
    held_write_data = value;
    observe_megaii_access(address, value, false);
    held_write_active = false;
    if (held_write_strobed) return;
    if (destination) *destination = value;
    else megaii->write(address & 0xffff, value);
}

// These two routines create static lookup tables for shadow memory, and aux-mapping.

// shadow_mask is one entry per 1K of the low 17 address bits. or_mask enables
// the slot when any listed inhibit is clear. and_mask enables it only when
// every listed inhibit is clear. Odd hi-res needs both: the page bit and
// AUXHGR are an AND, and that term is OR'd with SHR.
void MMU_IIgs::fill_shadow_mask() {
    std::memset(shadow_mask, 0, sizeof(shadow_mask));
    shadow_mask[1].or_mask = SHADOW_INH_TEXT1;
    shadow_mask[65].or_mask = SHADOW_INH_TEXT1;
    if (is_rom03) {
        shadow_mask[2].or_mask = SHADOW_INH_TEXT2;
        shadow_mask[66].or_mask = SHADOW_INH_TEXT2;
    }
    for (int slot = 8; slot <= 15; slot++) {
        shadow_mask[slot].or_mask = SHADOW_INH_HGR1;
    }
    for (int slot = 16; slot <= 23; slot++) {
        shadow_mask[slot].or_mask = SHADOW_INH_HGR2;
    }
    for (int slot = 72; slot <= 79; slot++) {
        shadow_mask[slot].or_mask = SHADOW_INH_SHR;
        shadow_mask[slot].and_mask = static_cast<uint8_t>(SHADOW_INH_HGR1 | SHADOW_INH_AUXHGR);
    }
    for (int slot = 80; slot <= 87; slot++) {
        shadow_mask[slot].or_mask = SHADOW_INH_SHR;
        shadow_mask[slot].and_mask = static_cast<uint8_t>(SHADOW_INH_HGR2 | SHADOW_INH_AUXHGR);
    }
    for (int slot = 88; slot <= 103; slot++) {
        shadow_mask[slot].or_mask = SHADOW_INH_SHR;
    }
}

// aux_region is used to quickly determine which memory pages (256-byte units) are
// currently mapped to aux bank. The index is packed read or write flag bits.
void MMU_IIgs::fill_aux_region() {
    constexpr uint8_t kZp = 0x01;
    constexpr uint8_t kText = 0x02;
    constexpr uint8_t kHires = 0x04;
    constexpr uint8_t kAll = 0x08;

    std::memset(aux_region, 0, sizeof(aux_region));
    aux_region[0x00] = kZp;
    aux_region[0x01] = kZp;
    for (int page = 0xC0; page <= 0xFF; page++) aux_region[page] = kZp;
    for (int page = 0x04; page <= 0x07; page++) aux_region[page] = kText;
    for (int page = 0x20; page <= 0x3F; page++) aux_region[page] = kHires;
    aux_region[0x02] = kAll;
    aux_region[0x03] = kAll;
    for (int page = 0x08; page <= 0x1F; page++) aux_region[page] = kAll;
    for (int page = 0x40; page <= 0xBF; page++) aux_region[page] = kAll;
}

uint8_t float_area_read(void *context, uint32_t address) {
    if (DEBUG(DEBUG_MMUGS)) printf("Float area read at address %06X\n", address);
    return address >> 16 ;
}

inline uint8_t bank_e0_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);

    //if ((address & 0xFF00) == 0xC000) return mmu_iigs->read_c0xx(address); // MegaII will call back to us
    // Second Sight observes this slow-bus read with the original bank address.
    return mmu_iigs->megaii_bus_read(address);
}

inline void bank_e0_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);

    //if ((address & 0xFF00) == 0xC000) {mmu_iigs->write_c0xx(address, value); return;} // MegaII will call back to us
    // Second Sight must see this write before normal slow-bus storage changes.
    mmu_iigs->megaii_bus_write(address, value);
}

/* Bank E1 read/write */

inline uint8_t bank_e1_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);

    // Second Sight observes this slow-bus read with the original bank address.
    if ((address & 0xF000) == 0xC000) return mmu_iigs->megaii_bus_read(address);
    
    if (!mmu_iigs->is_bank_latch()) {
        // Second Sight observes this slow-bus read with the original bank address.
        return mmu_iigs->megaii_bus_read(address);
    } else 
    {
        uint8_t value = 0;
        // This auxiliary-RAM path bypasses Mega II decode, but Second Sight
        // still sees the physical bus access and may hold RDY before it ends.
        mmu_iigs->observe_megaii(address, value, true);
        uint16_t a16 = (uint16_t)address;
        /* $FF ROM overlays the LC window here just as it does in bank $E0; verified
           against a real ROM 01. Cycle type stays SYNC - whether the FPI serves this
           as a fast ROM cycle instead is unverified. */
        if (a16 >= 0xD000 && !mmu_iigs->is_lc_read_enable()) {
            return mmu_iigs->read_mega_rom(a16);
        }
        uint8_t *ram = mmu_iigs->megaii->get_memory_base();
        return ram[mmu_iigs->e1_aux_index(a16)];
    }
}

inline void bank_e1_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);

    if ((address & 0xF000) == 0xC000) { 
        // Second Sight must see this write before normal slow-bus storage changes.
        mmu_iigs->megaii_bus_write(address, value);
        return;
    }

    if (!mmu_iigs->is_bank_latch()) {
        // Second Sight must see this write before normal slow-bus storage changes.
        mmu_iigs->megaii_bus_write(address, value);
    } else 
    {
        // This auxiliary-RAM path bypasses Mega II decode, but Second Sight
        // still sees the physical bus access and may hold RDY before it ends.
        uint16_t a16 = (uint16_t)address;
        if (a16 >= 0xD000 && !mmu_iigs->is_lc_write_enable()) {
            mmu_iigs->observe_megaii(address, value, false);
            return; // LC write protected
        }
        uint8_t *ram = mmu_iigs->megaii->get_memory_base();
        mmu_iigs->write_megaii_observed(address, value, ram + mmu_iigs->e1_aux_index(a16));
    }
}

inline void MMU_IIgs::set_intcxrom(bool value) {
    megaii->set_intcxrom(value);
    g_intcxrom = value;
}

inline void MMU_IIgs::write_c0xx(uint16_t address, uint8_t value) {

    switch (address) {
        case 0xC000: g_80store = false; break;
        case 0xC001: g_80store = true; break;
        case 0xC002: g_ramrd = false; break;
        case 0xC003: g_ramrd = true; break;
        case 0xC004: g_ramwrt = false; break;
        case 0xC005: g_ramwrt = true; break;
        case 0xC008: g_altzp = false; break;
        case 0xC009: g_altzp = true; break;
        case 0xC029: reg_new_video = static_cast<uint8_t>(value & 0xE1); break;
        case 0xC006: // INTCXROMOFF
            set_intcxrom(false);
            /* // enable slot ROM in pages $C1 - $CF
            megaii->f_intcxrom = false;
            megaii->compose_c1cf();
            megaii->set_default_C8xx_map(); // TODO: is this right?  https://zellyn.com/a2audit/v0/#e000b
            //printf("IIe Memory: INTCXROMOFF\n"); */
            break;
        case 0xC007: // INTCXROMON
            set_intcxrom(true);
/*             // enable main ROM in pages $C1 - $CF
            megaii->f_intcxrom = true;
            megaii->compose_c1cf();
            //printf("IIe Memory: INTCXROMON\n"); */
            break;
        case 0xC00A: // SLOTC3ROM
            megaii->set_slotc3rom(false);
            break;
        case 0xC00B: // SLOTC3ROM
            megaii->set_slotc3rom(true);
            break;
        case 0xC054: g_page2 = false;  break;
        case 0xC055: g_page2 = true;  break;
        case 0xC056: g_hires = false;  break;
        case 0xC057: g_hires = true;  break;
        default:
            assert(false && "MMU_IIgs: Unhandled C0XX write");
    }
    megaii_compose_map();
    if (DEBUG(DEBUG_MMUGS)) printf("Reg Write: %04X: %02X\n", address, value);
}

inline uint8_t MMU_IIgs::read_c0xx(uint16_t address) {
    uint8_t retval = megaii->floating_bus_read();
    switch (address) {
        case 0xC054: g_page2 = false; /* Call Display; */; break;
        case 0xC055: g_page2 = true; /* Call Display; */; break;
        case 0xC056: g_hires = false; /* Call Display; */; break;
        case 0xC057: g_hires = true; /* Call Display; */; break;
        case 0xC029: retval = static_cast<uint8_t>(reg_new_video & 0xE1); break;
        default:
            assert(false && "MMU_IIgs: Unhandled C0XX read");
    }
    megaii_compose_map();
    return retval;
}

// $C028 toggles ROMBANK. Read and write both count; the data byte does not.
uint8_t read_c028(void *context, uint32_t address) {
    (void)address;
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->toggle_rombank();
    return mmu_iigs->megaii->floating_bus_read();
}

void write_c028(void *context, uint32_t address, uint8_t value) {
    (void)address;
    (void)value;
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->toggle_rombank();
}

uint8_t read_c068(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return mmu_iigs->state_register();
}

/* 
   TODO: GuS optimized this by checking bits old vs new and only calling remapper functions if necessary.
*/
void write_c068(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_state_register(value);
    mmu_iigs->set_intcxrom((value & 0x01) ? true : false);
    //mmu_iigs->set_lc_bank1(((value & 0x04) != 0) ? false : true); // "if this bit is 1, lc ram bank 2 is selected" inverting sense (GS hw Ref is backwards)
    //mmu_iigs->set_lc_read_enable((value & 0x08) ? false : true); // If bit 3 is set, we need to update the LC read enable flag also.
    mmu_iigs->megaii_compose_map();
    mmu_iigs->bsr_map_memory();
    
    // also set PAGE2 display in video scanner
    if (value & 0x40) {
        mmu_iigs->megaii->write(0xC055, 0x00);
        mmu_iigs->set_page2(true);
    } else {
        mmu_iigs->megaii->write(0xC054, 0x00);
        mmu_iigs->set_page2(false);
    }
   /*  if (value & 0x80) { // ALTZP - not needed, megaii_compose_map() does this for us.
        mmu_iigs->megaii->write(0xC009,0x01);
    } else {
        mmu_iigs->megaii->write(0xC008,0x00);
    } */

}

void MMU_IIgs::set_slot_register(uint8_t value) {
    // store here
    reg_slot = value;
    if (DEBUG(DEBUG_MMUGS)) printf("setting slot register: %02X\n", value);
    // also update the megaii slot register.
    megaii->set_slot_register(value);
    //megaii->compose_c1cf(); // redundant since megaii->set_slot_register() does this.
}

// "Bit 3,0 must 0"
void write_c02d(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_slot_register(static_cast<uint8_t>(value & 0xF6));
}

uint8_t read_c02d(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return (static_cast<uint8_t>(mmu_iigs->get_slot_register() & 0xF6));
}

// read/write handlers for C0XX registers as handed to the MegaII.
inline uint8_t megaii_c0xx_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    return mmu_iigs->read_c0xx(address);
}

inline void megaii_c0xx_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->write_c0xx(address, value);
}


void MMU_IIgs::megaii_compose_map() {
    const char *TAG_MAIN = "MAIN";
    const char *TAG_ALT = "AUX";
    
    //update_display_flags(iiememory_d);
    
    bool n_zp; 
    bool n_text1_r;
    bool n_text1_w;
    bool n_hires1_r;
    bool n_hires1_w;
    bool n_all_r;
    bool n_all_w;

    n_zp = g_altzp;
    n_all_r = g_ramrd;
    n_all_w = g_ramwrt;
    if (g_80store) {
        if (g_hires) {
            n_text1_r = g_page2;
            n_text1_w = g_page2;
            n_hires1_r = g_page2;
            n_hires1_w = g_page2;
        } else {
            n_text1_r = g_page2;
            n_text1_w = g_page2;
            n_hires1_r = g_ramrd;
            n_hires1_w = g_ramwrt;
        }
    } else { // 80STORE OFF
        n_text1_r = g_ramrd;
        n_text1_w = g_ramwrt;
        n_hires1_r = g_ramrd;
        n_hires1_w = g_ramwrt;
    }

    uint8_t *memory_base = megaii->get_memory_base();

    if (n_zp != m_zp) { // this is both read and write.
        // change $00, $01, $D0 - $FF
        uint32_t altoffset = n_zp ? 0x1'0000 : 0x0'0000;

        megaii->map_page_read(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_write(0x00, memory_base + altoffset + 0x0000, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_read(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);
        megaii->map_page_write(0x01, memory_base + altoffset + 0x0100, n_zp ? TAG_ALT : TAG_MAIN);

        /* ALTZP moves the LC window too, so $D0-$FF has to be recomposed. Without
           this, $C008/$C009 left those pages on whichever bank the last $C08x or
           $C068 access selected, and a stale aux mapping made $E0's LC alias $E1's. */
        bsr_map_memory();
    }
    if (n_text1_r != m_text1_r) {
        // change $04 - $07
        uint32_t altoffset = n_text1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_text1_w != m_text1_w) {
        // change $04 - $07
        uint32_t altoffset = n_text1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x04; i <= 0x07; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_text1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_r != m_hires1_r) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_hires1_w != m_hires1_w) {
        // change $20 - $3F
        uint32_t altoffset = n_hires1_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x20; i <= 0x3F; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_hires1_w ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_r != m_all_r) {  
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_r ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            megaii->map_page_read(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_r ? TAG_ALT : TAG_MAIN);
        }
    }
    if (n_all_w != m_all_w) {
        // change $02 - $03, $08 - $1F, $40 - $BF
        uint32_t altoffset = n_all_w ? 0x1'0000 : 0x0'0000;
        for (int i = 0x02; i <= 0x03; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x08; i <= 0x1F; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
        for (int i = 0x40; i <= 0xBF; i++) {
            megaii->map_page_write(i, memory_base + altoffset + (i * GS2_PAGE_SIZE), n_all_w ? TAG_ALT : TAG_MAIN);
        }
    }

    // update the "current memory map state" flags.
    m_zp = n_zp;
    m_text1_r = n_text1_r;
    m_text1_w = n_text1_w;
    m_hires1_r = n_hires1_r;
    m_hires1_w = n_hires1_w;
    m_all_r = n_all_r;
    m_all_w = n_all_w;

    // Absolute, not a delta. n_* already resolved 80STORE / PAGE2 / HIRES.
    constexpr uint8_t kZp = 0x01;
    constexpr uint8_t kText = 0x02;
    constexpr uint8_t kHires = 0x04;
    constexpr uint8_t kAll = 0x08;
    auto pack = [](bool zp, bool text, bool hires, bool all) -> uint8_t {
        return static_cast<uint8_t>((zp ? kZp : 0) | (text ? kText : 0) | (hires ? kHires : 0) | (all ? kAll : 0));
    };
    aux_read_flags = pack(n_zp, n_text1_r, n_hires1_r, n_all_r);
    aux_write_flags = pack(n_zp, n_text1_w, n_hires1_w, n_all_w);
}

void MMU_IIgs::bsr_map_memory() {

    uint32_t bankd0offset = (ll.FF_BANK_1 == 1) ? 0xD000 : 0xC000;
    uint32_t banke0offset = 0xE000;
    if (g_altzp) {
        bankd0offset += 0x1'0000; // alternate bank!
        banke0offset += 0x1'0000; // alternate bank!
    }

    //uint8_t *bank = (lc->FF_BANK_1 == 1) ? lc->ram : lc->ram + 0x1000;
    uint8_t *bankd0 = megaii->get_memory_base() + bankd0offset;
    uint8_t *banke0 = megaii->get_memory_base() + banke0offset;
    uint8_t *ff_rom = main_rom + rom_bank_ff_offset();

    const char *bank_d = (ll.FF_BANK_1 == 1) ? "LC_BANK1" : "LC_BANK2";

    /* Map D0 - DF */
    for (int i = 0; i < 16; i++) {
        if (ll.FF_READ_ENABLE) {
            megaii->map_page_read(i + 0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);
        } else { // reads == READ_ROM
            const uint16_t rom_addr = mega_rom_offset(static_cast<uint16_t>(0xD000 + i * GS2_PAGE_SIZE));
            megaii->map_page_read(i + 0xD0, ff_rom + rom_addr, "SYS_ROM");
        }

        if (!ll._FF_WRITE_ENABLE) {
            megaii->map_page_write(i+0xD0, bankd0 + (i*GS2_PAGE_SIZE), bank_d);

        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            megaii->map_page_write(i+0xD0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    /* Map E0 - FF */
    for (int i = 0; i < 32; i++) {
        if (ll.FF_READ_ENABLE) {
            megaii->map_page_read(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");

        } else { // reads == READ_ROM
            const uint16_t rom_addr = mega_rom_offset(static_cast<uint16_t>(0xE000 + i * GS2_PAGE_SIZE));
            megaii->map_page_read(i+0xE0, ff_rom + rom_addr, "SYS_ROM");
        }

        if (!ll._FF_WRITE_ENABLE) {
            megaii->map_page_write(i+0xE0, banke0 + (i * GS2_PAGE_SIZE), "LC RAM");
        } else { // writes == WRITE_NONE - set it to the ROM and can_write = 0
            megaii->map_page_write(i+0xE0, nullptr, "NONE"); // much simpler actually.. no write enable means null write pointer.
        }
    }

    /* if (DEBUG(DEBUG_LANGCARD)) {
        lc->mmu->dump_page_table(0xD0, 0xD0);
        lc->mmu->dump_page_table(0xE0, 0xE0);
    } */
}

uint8_t g_bsr_read_C0xx(void *context, uint32_t address) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    //if (DEBUG(DEBUG_LANGCARD)) printf("languagecard read %04X ", address);

    lc->ll.read(address);
    lc->set_lc_bank1(lc->ll.FF_BANK_1); // sync back to state reg
    lc->set_lc_read_enable(lc->ll.FF_READ_ENABLE);
    lc->bsr_map_memory();
    return lc->megaii->floating_bus_read();
}


void gs_bsr_write_C0xx(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    //if (DEBUG(DEBUG_LANGCARD)) printf("languagecard write %04X value: %02X\n", address, value);
    
    lc->ll.write(address);
    lc->set_lc_bank1(lc->ll.FF_BANK_1); // sync back to state reg
    lc->set_lc_read_enable(lc->ll.FF_READ_ENABLE);
    lc->bsr_map_memory();

}

uint8_t gs_bsr_read_C01x(void *context, uint32_t address) {
    MMU_IIgs *lc = (MMU_IIgs *)context;

    if (DEBUG(DEBUG_MMUGS)) printf("gs_bsr_read_C01x %04X \n", address);
    uint8_t fl = 0x00;
    switch (address & 0xF) {
        case 0x1: /* C011 */ fl = (lc->is_lc_bank1() == 0) ? 0x80 : 0x00; break;
        case 0x2: /* C012 */ fl = (lc->is_lc_read_enable() != 0) ? 0x80 : 0x00; break;
        case 0x3: /* C013 */ fl = (lc->state_register() & 0x20) ? 0x80 : 0x00; break;
        case 0x4: /* C014 */ fl = (lc->state_register() & 0x10) ? 0x80 : 0x00; break;
        case 0x5: /* C015 */ fl = (lc->state_register() & 0x01) ? 0x80 : 0x00; break;
        case 0x6: /* C016 */ fl = (lc->state_register() & 0x80) ? 0x80 : 0x00; break;
        case 0x7: /* C017 */ fl = lc->is_slotc3rom() ? 0x80 : 0x00; break;
        case 0x8: /* C018 */ fl = lc->is_80store() ? 0x80 : 0x00; break;

        // TODO: this is a weird place to have these. They don't affect memory map.
        case 0xA: /* C01A */ fl = lc->is_text() ? 0x80 : 0x00; break;
        case 0xB: /* C01B */ fl = lc->is_mixed() ? 0x80 : 0x00; break;

        /* These do affect memory map. */
        case 0xC: /* C01C */ fl = lc->is_page2() ? 0x80 : 0x00; break;
        case 0xD: /* C01D */ fl = lc->is_hires() ? 0x80 : 0x00; break;
    }
    
    /* KeyboardMessage *keymsg = (KeyboardMessage *)lc->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl; */
    return fl;
}

/*
TODO: need to add: c015, c016, c017, c018, c01a, c01b, c01c, c01d 
*/


// set shadow register
void c035_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    mmu_iigs->set_shadow_register(value);
}

uint8_t c035_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return mmu_iigs->shadow_register();
}

uint8_t c036_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return mmu_iigs->speed_register();
}

// set speed register
void c036_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    mmu_iigs->set_speed_register(value);
    mmu_iigs->set_ram_shadow_banks();
}

uint8_t c037_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return mmu_iigs->get_dma_bank_register();
}

void c037_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    mmu_iigs->set_dma_bank_register(value);
}

/*
IIe Memory Mapping for Main/AUX:
    | 7 | ALTZP | 1 = ZP, Stack, LC are in Main; 0 = in Aux |
    | 6 | PAGE2 | 1 = Text Page 2 Selected |
    | 5 | RAMRD | 1 = Aux RAM is read-enabled |
    | 4 | RAMWRT | 1 = aux RAM is write-enabled |
    |   | 80STORE | 1 = 80-store is active (PAGE2 controls Main vs Aux) |
*/

/* uint32_t MMU_IIgs::calc_aux_write(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramwrt))) return 0x1'0000;
    if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramwrt))) return 0x1'0000;
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramwrt)) return 0x1'0000;
    return 0x0'0000;
}

uint32_t MMU_IIgs::calc_aux_read(uint32_t address) {
    uint32_t page = (address & 0xFF00) >> 8;
    if ((page >= 0x04 && page <= 0x07) && ((g_80store && g_page2) || (!g_80store && g_ramrd))) return 0x1'0000;
    if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramrd ))) return 0x1'0000;
    //if (((page >= 0x00 && page <= 0x01) || (page >= 0xD0 && page <= 0xFF)) && (g_altzp || g_lcbnk2)) return 0x1'0000;
    // we know at this point we were not originally C0 IO space. so.. 
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && (g_altzp)) return 0x1'0000;
    //if (g_ramrd) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramrd)) return 0x1'0000;
    return 0x0'0000;
} */

/* Any ram bank that has shadowing enabled of any kind comes through here. */
uint8_t bank_shadow_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;

    uint32_t page = (address & 0xFF00) >> 8;

    // if IOLC is "shadowed" and it's an I/O location, send write down to Megaii.
    if ( mmu_iigs->is_iolc_shadowed() && (page >= 0xC0 && page <= 0xCF)) {
        mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);
        //return mmu_iigs->megaiiRead(address & 0x1'FFFF);
        // Second Sight observes this slow-bus read with the original bank address.
        return mmu_iigs->megaii_bus_read(address);
    }
    
    // if IOLC is "shadowed" (enabled) and it's a language card location, map the address.
    if (mmu_iigs->is_iolc_shadowed() && (page >= 0xD0 && page <= 0xFF)) {
        
        if (mmu_iigs->is_lc_read_enable()) { // ram
            if (!mmu_iigs->is_lc_bank1()) {
                if (page >= 0xD0 && page <= 0xDF) address -= 0x1000; // bank 2 lives at $C000
            }
        } else {
            //uint8_t *addr;
            // rom
            mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM); // even though it's in "LC" area it's ROM. 
            if (DEBUG(DEBUG_MMUGS)) printf("Read: ROM Effective address: %06X\n", address);
            return mmu_iigs->read_mega_rom(static_cast<uint16_t>(address));
        }
    }
    // we know at this point we were not originally C0 IO space. so.. 

    address += mmu_iigs->calc_aux_read(address);    // handle RAMRD, 80STORE, ALTZP, PAGE2, HIRES (cuz we can have this AND LC at same time)
    if (DEBUG(DEBUG_MMUGS)) printf("Read: Effective address: %06X\n", address);
    // Unpopulated banks (SPEED_SHADOW_ALL past ram_banks): float = bank number.
    if (address >= mmu_iigs->get_memory_size()) {
        return (uint8_t)(address >> 16);
    }
    return mmu_iigs->get_memory_base()[address];
}

// use different routine for direct aux bank shadow
void bank_shadow_write(void *context, uint32_t address, uint8_t value) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    
    uint32_t page = (address & 0xFF00) >> 8;

    // if IOLC is "shadowed" and it's an I/O location, send write down to Megaii.
    if ( (mmu_iigs->is_iolc_shadowed()) && (page >= 0xC0 && page <= 0xCF) ) {
        //mmu_iigs->megaiiWrite(address & 0x1'FFFF, value);
        mmu_iigs->set_next_cycle_type(CYCLE_TYPE_SYNC);
        // Second Sight must see this write before normal slow-bus storage changes.
        mmu_iigs->megaii_bus_write(address, value);
        return; // we delegated this to the MegaII
    }
    
    // if IOLC is "shadowed" (enabled) and it's a language card location, map the address.
    if (mmu_iigs->is_iolc_shadowed() && (page >= 0xD0 && page <= 0xFF)) {
        
        if (mmu_iigs->is_lc_write_enable()) { // is LC RAM writable?
            if (!mmu_iigs->is_lc_bank1()) {
                if (page >= 0xD0 && page <= 0xDF) address -= 0x1000; // bank 2 lives at $C000
            }
        } else {
            // ROM - do nothing, just fall through to here.
            return; // do not write to either LC RAM since _WRITE_ENABLE=1
        }
    }

    // Save the original CPU bank for Second Sight before calc_aux_write()
    // applies soft switches. M2B0 does not encode that auxiliary selection
    // (Apple IIgs Technical Note 68, "The Mega II Bank 0 Signal").
    const bool m2b0 = (address >> 16) & 1;
    address += mmu_iigs->calc_aux_write(address);   // handle RAMWRT, 80STORE, ALTZP, PAGE2, HIRES

    if ( mmu_iigs->shadow_is_enabled(address)) {
        // Shadowed
        // Shadowing copies fast RAM writes onto the slow bus. Second Sight
        // needs that bus copy, including writes made through bank $01.
        mmu_iigs->megaiiWrite(address & 0x1'FFFF, value, m2b0);
    }
    if (DEBUG(DEBUG_MMUGS)) printf("Write: Effective address: %06X\n", address);
    // Unpopulated banks: Mega II shadow already applied; discard RAM store.
    if (address >= mmu_iigs->get_memory_size()) {
        return;
    }
    mmu_iigs->get_memory_base()[address] = value;
}

uint8_t iolc_rom_read(void *context, uint32_t address) {
    MMU_IIgs *mmu_iigs = (MMU_IIgs *)context;
    mmu_iigs->set_next_cycle_type(CYCLE_TYPE_FAST_ROM);
    return mmu_iigs->read_mega_rom(static_cast<uint16_t>(address));
}

void MMU_IIgs::toggle_rombank() {
    g_rombank = !g_rombank;
    // ROM 03 tracks the latch. CYA pin 60 is grounded, so the windows stay put.
    if (map_initialized && !is_rom03) sync_rom_windows();
}

void MMU_IIgs::map_internal_cx_rom() {
    // The base is already A14-adjusted by sync_rom_windows. Clearing A14 again
    // would map $C100 to $4100.
    uint8_t *base = megaii->get_rom_base();
    for (int i = 1; i < 16; i++) {
        megaii->map_c1cf_internal_rom(0xC0 + i, base + i * GS2_PAGE_SIZE, "GS INT");
    }
}

void MMU_IIgs::sync_rom_windows() {
    // ROMBANK clear keeps $FF/C000. ROMBANK set on a ROM 01 selects $FF/8000.
    // mega_rom_offset is a no-op on ROM 03, so this writes the constructor pointer again.
    megaii->set_rom_base(main_rom + rom_bank_ff_offset() + mega_rom_offset(0xC000));
    map_internal_cx_rom();
    if (megaii->get_C8xx_slot() == SLOT_3) {
        megaii->call_C8xx_handler(SLOT_3);
    }
    megaii->compose_c1cf();
    bsr_map_memory();
}

read_handler_t float_read_handler = { (memory_read_func)float_area_read, nullptr };

void MMU_IIgs::init_c0xx_handlers() {
    for (uint32_t i = 0xC000; i <= 0xC009; i++) {
        megaii->set_C0XX_write_handler(i, {megaii_c0xx_write, this});
    }

    for (uint32_t i = 0xC011; i <= 0xC018; i++) {
        megaii->set_C0XX_read_handler(i, {gs_bsr_read_C01x, this});
    }

    //megaii->set_C0XX_read_handler(0xC01A, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01B, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01C, {gs_bsr_read_C01x, this});
    //megaii->set_C0XX_read_handler(0xC01D, {gs_bsr_read_C01x, this});

    megaii->set_C0XX_write_handler(0xC02D, {write_c02d, this});
    megaii->set_C0XX_read_handler(0xC02D, {read_c02d, this});

    for (uint32_t i = 0xC054; i <= 0xC057; i++) {
        megaii->set_C0XX_write_handler(i, {megaii_c0xx_write, this});
        megaii->set_C0XX_read_handler(i, {megaii_c0xx_read, this});
    }

    for (uint32_t i = 0xC080; i <= 0xC08F; i++) {
        megaii->set_C0XX_write_handler(i, {gs_bsr_write_C0xx, this});
        megaii->set_C0XX_read_handler(i, {g_bsr_read_C0xx, this});
    }

    for (uint32_t i = 0xC071; i <= 0xC07F; i++) {
        megaii->set_C0XX_read_handler(i, {iolc_rom_read, this});
    }

    megaii->set_C0XX_write_handler(0xC035, {c035_write, this});
    megaii->set_C0XX_write_handler(0xC036, {c036_write, this});
    megaii->set_C0XX_read_handler(0xC035, {c035_read, this});
    megaii->set_C0XX_read_handler(0xC036, {c036_read, this});
    
    megaii->set_C0XX_write_handler(0xC029, {megaii_c0xx_write, this});
    megaii->set_C0XX_read_handler(0xC029, {megaii_c0xx_read, this});
    
    megaii->set_C0XX_read_handler(0xC028, {read_c028, this});
    megaii->set_C0XX_write_handler(0xC028, {write_c028, this});
    megaii->set_C0XX_read_handler(0xC068, {read_c068, this});
    megaii->set_C0XX_write_handler(0xC068, {write_c068, this});
}

void MMU_IIgs::set_ram_shadow_banks() {
    const bool all_banks = (reg_speed & SPEED_SHADOW_ALL) != 0;
    uint32_t max_shadow_bank = all_banks ? ram_banks : 0x02;
    if (DEBUG(DEBUG_MMUGS)) printf("setting ram shadow banks: 0x00 - 0x%02X (all=%d)\n", max_shadow_bank - 1, all_banks);

    for (int i = 0x00; i < (int)max_shadow_bank; i++) {
        map_page_both(i, nullptr, "MAIN_RAM");
        set_page_read_h(i, {bank_shadow_read, this}, "SHADRD");
        set_page_write_h(i, {bank_shadow_write, this}, "SHADWR");       
    }
    for (int i = max_shadow_bank; i < (int)ram_banks; i++) {
        map_page_both(i, main_ram + i * BANK_SIZE, "MAIN_RAM");
        set_page_read_h(i, {nullptr, nullptr}, "MAIN_RAM");
        set_page_write_h(i, {nullptr, nullptr}, "MAIN_RAM");
    }

    // SPEED_SHADOW_ALL shadows through $EF (not only installed RAM). Skip $E0/$E1 (Mega II).
    auto map_float_shadow = [this](int bank) {
        map_page_both(bank, nullptr, "Float");
        set_page_read_h(bank, {bank_shadow_read, this}, "SHADRD");
        set_page_write_h(bank, {bank_shadow_write, this}, "SHADWR");
    };
    auto map_float_plain = [this](int bank) {
        map_page_both(bank, nullptr, "Float");
        set_page_read_h(bank, float_read_handler, "Float");
        set_page_write_h(bank, {nullptr, nullptr}, "Float");
    };

    if (all_banks) {
        for (int i = (int)ram_banks; i <= 0xDF; i++) {
            map_float_shadow(i);
        }
        for (int i = 0xE2; i <= 0xEF; i++) {
            map_float_shadow(i);
        }
    } else {
        // Restore unpopulated banks to float (same as init_map for $80-$DF / $E2+).
        for (int i = (int)ram_banks; i <= 0xDF; i++) {
            map_float_plain(i);
        }
        for (int i = 0xE2; i <= 0xEF; i++) {
            map_float_plain(i);
        }
    }

    if (DEBUG(DEBUG_MMUGS)) {
        dump_page_table(0x00, 0x03);
        dump_page_table(0xE0, 0xE1);
        dump_page_table(0xFE, 0xFF);
    }
}

void MMU_IIgs::init_map() {

    for (int i = 0; i < rom_banks; i++) {
        map_page_read_only((0x100 - rom_banks) + i, main_rom + i * BANK_SIZE, "SYS_ROM");
    }
    for (int i = 0x80; i < 0xE0; i++) {
        set_page_read_h(i, float_read_handler, "Float");
    }

    set_page_read_h(0xE0, {bank_e0_read, this}, "MEGAII");
    set_page_write_h(0xE0, {bank_e0_write, this}, "MEGAII");

    set_page_read_h(0xE1, {bank_e1_read, this}, "MEGAII");
    set_page_write_h(0xE1, {bank_e1_write, this}, "MEGAII");

    for (int i = 0xE2; i < (0x100 - rom_banks); i++) {
        set_page_read_h(i, float_read_handler, "Float");
    }

    set_ram_shadow_banks();

    map_internal_cx_rom();

    /* megaii->set_slot_rom(SLOT_1, main_rom + 0x1'C100, "GS INT");
    megaii->set_slot_rom(SLOT_2, main_rom + 0x1'C200, "GS INT");
    megaii->set_slot_rom(SLOT_3, main_rom + 0x1'C300, "GS INT");
    megaii->set_slot_rom(SLOT_4, main_rom + 0x1'C400, "GS INT");
    megaii->set_slot_rom(SLOT_5, main_rom + 0x1'C500, "GS INT");
    megaii->set_slot_rom(SLOT_6, main_rom + 0x1'C600, "GS INT"); */

    init_c0xx_handlers();
    map_initialized = true;
}

void MMU_IIgs::reset(bool cold_start) {
    //reg_new_video = 0x01; reg_shadow = 0x08; reg_state = 0; g_80store = false; g_hires = false; g_rdrom = true; 

    reg_new_video = 0x01;
    g_hires = false;
    g_80store = false;
    g_text = true;
    g_mixed = false;

    // on RESET, set:
    set_slot_register(0x00);
    set_speed_register(cold_start && is_rom03 ? 0xC0u : 0x80u); // bit 7 slow; ROM03 bit 6 = power-on on cold start
    reg_shadow = 0x08;
    set_state_register(0x0C); // KEGS does 0x0D - enable ROM RD, 
    set_intcxrom(g_intcxrom); // this is needed to set the C1-CF map correctly.
    // TODO: uh should I reset the LC state here? (or am I doing it via the state register above?)
    ll.reset();
    if (map_initialized) {
        set_ram_shadow_banks();
        // Stolen from c068 handler - consolidate somewhere
        megaii_compose_map();
        bsr_map_memory();
        // also set PAGE2 display in video scanner
        // don't do this here.
        //megaii->write(0xC054, 0x00);    
    }
}

void MMU_IIgs::debug_dump(DebugFormatter *df) {
    df->addLine("LC: BANK_1: %d, READ_ENABLE: %d, PRE_WRITE: %d, /WRITE_ENABLE: %d", ll.FF_BANK_1, ll.FF_READ_ENABLE, ll.FF_PRE_WRITE, ll._FF_WRITE_ENABLE);
    df->addLine("Shadow: %02X: ![IOLC: %d T2: %d AUXH: %d SHR: %d H2: %d H1: %d T1: %d]",
        reg_shadow,
        (reg_shadow & SHADOW_INH_IOLC) != 0,
        (reg_shadow & SHADOW_INH_TEXT2) != 0,
        (reg_shadow & SHADOW_INH_AUXHGR) != 0,
        (reg_shadow & SHADOW_INH_SHR) != 0,
        (reg_shadow & SHADOW_INH_HGR2) != 0,
        (reg_shadow & SHADOW_INH_HGR1) != 0,
        (reg_shadow & SHADOW_INH_TEXT1) != 0);
    df->addLine("SlotReg: %02X", reg_slot);
    df->addLine("State: %02X: INTCXROM: %d ROMBANK: %d LCBNK2: %d RDROM: %d",         
        reg_state,
        g_intcxrom,
        g_rombank,
        g_lcbnk2,
        g_rdrom
    );
    df->addLine("           RAMWRT:   %d RAMRD:   %d PAGE2:  %d ALTZP: %d",
        g_ramwrt,
        g_ramrd,
        g_page2,
        g_altzp);
    df->addLine("           80STORE:  %d HIRES:   %d", g_80store, g_hires);
    
    df->addLine("Speed: %02X", reg_speed);
    debug_output_page(df, 0x00);
    debug_output_page(df, 0x01);
    debug_output_page(df, 0x02);
    df->addLine("Mega II");
    df->addLine("C8xx_slot: %d", megaii->get_C8xx_slot());
    megaii->debug_output_page(df, 0x00);
    megaii->debug_output_page(df, 0x20);
    megaii->debug_output_page(df, 0x40);
    megaii->debug_output_page(df, 0x60);
    megaii->debug_output_page(df, 0xC3);
    megaii->debug_output_page(df, 0xC7);
    megaii->debug_output_page(df, 0xC8);
    megaii->debug_output_page(df, 0xD0);
    megaii->debug_output_page(df, 0xE0);
    megaii->debug_output_page(df, 0xFF);
}

uint8_t MMU_IIgs::vp_read(uint32_t address) {
    if (is_iolc_shadowed()) { // if IOLC is shadowed, read from ROM.
        return read_mega_rom(static_cast<uint16_t>(address));
    } else {
        return read(address);
    }
}
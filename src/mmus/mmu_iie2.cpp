#include "mmu_iie2.hpp"

#include "mbus/KeyboardMessage.hpp"
#include "mbus/MessageBus.hpp"
#include "memoryspecs.hpp"
#include "util/DebugFormatter.hpp"

#include <cassert>

void MMU_IIe2::init_map() {
    for (int i = 0; i < ram_pages; i++) {
        map_page_both(i, main_ram + i * GS2_PAGE_SIZE, "MAIN_RAM");
    }
    // IIe ROM image is $C000-$FFFF, so $D000 starts 0x1000 into the file.
    if (main_rom_D0 != nullptr) {
        for (int i = 0; i < (ROM_KB / GS2_PAGE_SIZE); i++) {
            map_page_read_only(i + 0xD0, main_rom_D0 + 0x1000 + i * GS2_PAGE_SIZE, "SYS_ROM");
        }
    }
}

void MMU_IIe2::power_on_randomize(uint8_t *ram, int ram_size) {
    (void)ram_size;
    for (int i = 0x1'0000; i < 0x2'0000; i += 4) {
        ram[i] = 0xCC;
        ram[i + 1] = 0xCC;
        ram[i + 2] = 0x00;
        ram[i + 3] = 0x00;
    }
}

void MMU_IIe2::set_default_C8xx_map() {
    if (!f_intcxrom) {
        MMU_II::set_default_C8xx_map();
    }
}

void MMU_IIe2::compose_c1cf() {
    if (!f_intcxrom) {
        for (int i = 8; i < 16; i++) {
            set_page_table_entry(0xC0 + i, &slot_rom_ptable[i - 1]);
        }
        set_page_table_entry(0xC1, reg_slot & 0x02 ? &slot_rom_ptable[0] : &int_rom_ptable[0]);
        set_page_table_entry(0xC2, reg_slot & 0x04 ? &slot_rom_ptable[1] : &int_rom_ptable[1]);
        set_page_table_entry(0xC3, f_slotc3rom ? &slot_rom_ptable[2] : &int_rom_ptable[2]);
        set_page_table_entry(0xC4, reg_slot & 0x10 ? &slot_rom_ptable[3] : &int_rom_ptable[3]);
        set_page_table_entry(0xC5, reg_slot & 0x20 ? &slot_rom_ptable[4] : &int_rom_ptable[4]);
        set_page_table_entry(0xC6, reg_slot & 0x40 ? &slot_rom_ptable[5] : &int_rom_ptable[5]);
        set_page_table_entry(0xC7, reg_slot & 0x80 ? &slot_rom_ptable[6] : &int_rom_ptable[6]);
    } else {
        for (int i = 1; i < 16; i++) {
            set_page_table_entry(0xC0 + i, &int_rom_ptable[i - 1]);
        }
    }
}

void MMU_IIe2::map_c1cf_internal_rom(page_t page, uint8_t *data, const char *read_d) {
    assert(page >= 0xC1 && page <= 0xCF);

    page_table_entry_t *pte = &int_rom_ptable[page - 0xC1];
    pte->read_p = data;
    pte->write_p = nullptr;
    pte->read_h = {nullptr, nullptr};
    pte->write_h = {nullptr, nullptr};
    pte->shadow_h = {nullptr, nullptr};
    pte->read_d = read_d;
    pte->write_d = nullptr;
}

void MMU_IIe2::set_intcxrom(bool on) {
    f_intcxrom = on;
    compose_c1cf();
}

void MMU_IIe2::set_slotc3rom(bool on) {
    f_slotc3rom = on;
    compose_c1cf();
}

void MMU_IIe2::c00x_write(void *context, uint32_t address, uint8_t value) {
    (void)value;
    auto *self = static_cast<MMU_IIe2 *>(context);
    switch (address) {
        case 0xC006: self->set_intcxrom(false); break;
        case 0xC007: self->set_intcxrom(true); break;
        case 0xC00A: self->set_slotc3rom(false); break;
        case 0xC00B: self->set_slotc3rom(true); break;
        default: break;
    }
}

void MMU_IIe2::map_rom_slot3(void *context, SlotType_t slot) {
    (void)slot;
    auto *self = static_cast<MMU_IIe2 *>(context);
    if (self->f_slotc3rom) {
        return;
    }
    uint8_t *rom = self->get_rom_base();
    for (uint8_t page = 0; page < 8; page++) {
        self->map_c1cf_page_read_only(page + 0xC8, rom + 0x800 + (page * 0x100), "SYS_ROM");
    }
}

void MMU_IIe2::map_range(int lo, int hi, bool aux, bool is_write) {
    const uint32_t altoffset = aux ? 0x1'0000u : 0;
    const char *tag = aux ? "AUX" : "MAIN";
    uint8_t *base = main_ram + altoffset + static_cast<uint32_t>(lo) * GS2_PAGE_SIZE;
    if (is_write) {
        map_range_write(static_cast<page_t>(lo), static_cast<page_t>(hi), base, tag);
    } else {
        map_range_read(static_cast<page_t>(lo), static_cast<page_t>(hi), base, tag);
    }
}

void MMU_IIe2::bsr_map_memory() {
    uint32_t bankd0offset = (ll.FF_BANK_1 == 1) ? 0xC000 : 0xD000;
    uint32_t banke0offset = 0xE000;
    if (f_altzp) {
        bankd0offset += 0x1'0000;
        banke0offset += 0x1'0000;
    }

    uint8_t *bankd0 = main_ram + bankd0offset;
    uint8_t *banke0 = main_ram + banke0offset;
    uint8_t *rom = get_rom_base();
    const char *bank_d = (ll.FF_BANK_1 == 1) ? "LC_BANK1" : "LC_BANK2";
    const bool ram_read = ll.FF_READ_ENABLE != 0;
    const bool ram_write = ll._FF_WRITE_ENABLE == 0;

    map_range_read(0xD0, 0xDF, ram_read ? bankd0 : rom + 0x1000, ram_read ? bank_d : "SYS_ROM");
    map_range_write(0xD0, 0xDF, ram_write ? bankd0 : nullptr, ram_write ? bank_d : "NONE");
    map_range_read(0xE0, 0xFF, ram_read ? banke0 : rom + 0x2000, ram_read ? "LC RAM" : "SYS_ROM");
    map_range_write(0xE0, 0xFF, ram_write ? banke0 : nullptr, ram_write ? "LC RAM" : "NONE");
}

void MMU_IIe2::compose_map(bool force) {
    const bool n_zp = f_altzp;
    const bool n_all_r = f_ramrd;
    const bool n_all_w = f_ramwrt;
    bool n_text1_r;
    bool n_text1_w;
    bool n_hires1_r;
    bool n_hires1_w;

    if (f_80store) {
        if (s_hires) {
            n_text1_r = s_page2;
            n_text1_w = s_page2;
            n_hires1_r = s_page2;
            n_hires1_w = s_page2;
        } else {
            n_text1_r = s_page2;
            n_text1_w = s_page2;
            n_hires1_r = f_ramrd;
            n_hires1_w = f_ramwrt;
        }
    } else {
        n_text1_r = f_ramrd;
        n_text1_w = f_ramwrt;
        n_hires1_r = f_ramrd;
        n_hires1_w = f_ramwrt;
    }

    if (force || n_zp != m_zp) {
        map_range(0x00, 0x01, n_zp, false);
        map_range(0x00, 0x01, n_zp, true);
        bsr_map_memory();
    }
    if (force || n_text1_r != m_text1_r) {
        map_range(0x04, 0x07, n_text1_r, false);
    }
    if (force || n_text1_w != m_text1_w) {
        map_range(0x04, 0x07, n_text1_w, true);
    }
    if (force || n_hires1_r != m_hires1_r) {
        map_range(0x20, 0x3F, n_hires1_r, false);
    }
    if (force || n_hires1_w != m_hires1_w) {
        map_range(0x20, 0x3F, n_hires1_w, true);
    }
    if (force || n_all_r != m_all_r) {
        map_range(0x02, 0x03, n_all_r, false);
        map_range(0x08, 0x1F, n_all_r, false);
        map_range(0x40, 0xBF, n_all_r, false);
    }
    if (force || n_all_w != m_all_w) {
        map_range(0x02, 0x03, n_all_w, true);
        map_range(0x08, 0x1F, n_all_w, true);
        map_range(0x40, 0xBF, n_all_w, true);
    }

    m_zp = n_zp;
    m_text1_r = n_text1_r;
    m_text1_w = n_text1_w;
    m_hires1_r = n_hires1_r;
    m_hires1_w = n_hires1_w;
    m_all_r = n_all_r;
    m_all_w = n_all_w;
}

void MMU_IIe2::aux_c00x_write(void *context, uint32_t address, uint8_t value) {
    (void)value;
    auto *self = static_cast<MMU_IIe2 *>(context);
    switch (address) {
        case 0xC000: self->set_80store(false); break;
        case 0xC001: self->set_80store(true); break;
        case 0xC002: self->set_ramrd(false); break;
        case 0xC003: self->set_ramrd(true); break;
        case 0xC004: self->set_ramwrt(false); break;
        case 0xC005: self->set_ramwrt(true); break;
        case 0xC008: self->set_altzp(false); break;
        case 0xC009: self->set_altzp(true); break;
        default: break;
    }
    self->compose_map(false);
}

uint8_t MMU_IIe2::key_low() const {
    if (mbus == nullptr) {
        return 0xEE & 0x7F;
    }
    KeyboardMessage *keymsg = static_cast<KeyboardMessage *>(mbus->read(MESSAGE_TYPE_KEYBOARD));
    return (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
}

uint8_t MMU_IIe2::c01x_read(void *context, uint32_t address) {
    auto *self = static_cast<MMU_IIe2 *>(context);
    uint8_t fl = 0x00;
    switch (address) {
        case 0xC011: fl = (!self->ll.FF_BANK_1) ? 0x80 : 0x00; break;
        case 0xC012: fl = (self->ll.FF_READ_ENABLE) ? 0x80 : 0x00; break;
        case 0xC013: fl = self->f_ramrd ? 0x80 : 0x00; break;
        case 0xC014: fl = self->f_ramwrt ? 0x80 : 0x00; break;
        case 0xC015: fl = self->f_intcxrom ? 0x80 : 0x00; break;
        case 0xC016: fl = self->f_altzp ? 0x80 : 0x00; break;
        case 0xC017: fl = self->f_slotc3rom ? 0x80 : 0x00; break;
        case 0xC018: fl = self->f_80store ? 0x80 : 0x00; break;
        default: break;
    }
    return self->key_low() | fl;
}

uint8_t MMU_IIe2::display_read(void *context, uint32_t address) {
    auto *self = static_cast<MMU_IIe2 *>(context);
    switch (address) {
        case 0xC054: self->set_page2(false); self->compose_map(false); break;
        case 0xC055: self->set_page2(true); self->compose_map(false); break;
        case 0xC056: self->set_hires(false); self->compose_map(false); break;
        case 0xC057: self->set_hires(true); self->compose_map(false); break;
        default: break;
    }
    return 0;
}

void MMU_IIe2::display_write(void *context, uint32_t address, uint8_t value) {
    (void)value;
    display_read(context, address);
}

uint8_t MMU_IIe2::lc_read(void *context, uint32_t address) {
    auto *self = static_cast<MMU_IIe2 *>(context);
    self->ll.read(address);
    self->bsr_map_memory();
    return self->floating_bus_read();
}

void MMU_IIe2::lc_write(void *context, uint32_t address, uint8_t value) {
    (void)value;
    auto *self = static_cast<MMU_IIe2 *>(context);
    self->ll.write(address);
    self->bsr_map_memory();
}

void MMU_IIe2::install_slot_rom() {
    set_C0XX_write_handler(0xC006, {c00x_write, this});
    set_C0XX_write_handler(0xC007, {c00x_write, this});
    set_C0XX_write_handler(0xC00A, {c00x_write, this});
    set_C0XX_write_handler(0xC00B, {c00x_write, this});
    set_C8xx_handler(SLOT_3, map_rom_slot3, this);
    for (int i = 1; i < 16; i++) {
        map_c1cf_internal_rom(0xC0 + i, main_rom_D0 + i * GS2_PAGE_SIZE, "SYS_ROM");
    }
}

void MMU_IIe2::install_memory_handlers() {
    for (int i = 0xC000; i <= 0xC009; i++) {
        if (i == 0xC006 || i == 0xC007) {
            continue;
        }
        set_C0XX_write_handler(i, {aux_c00x_write, this});
    }
    for (uint16_t i = 0xC011; i <= 0xC018; i++) {
        set_C0XX_read_handler(i, {c01x_read, this});
    }
    for (uint16_t i = 0xC050; i <= 0xC057; i++) {
        set_C0XX_read_handler(i, {display_read, this});
        set_C0XX_write_handler(i, {display_write, this});
    }
    for (uint16_t i = 0xC080; i <= 0xC08F; i++) {
        set_C0XX_read_handler(i, {lc_read, this});
        set_C0XX_write_handler(i, {lc_write, this});
    }
}

MMU_IIe2::MMU_IIe2(int page_table_size, int ram_amount, uint8_t *rom_pointer,
                   bool install_memory_switches)
    : MMU_II(page_table_size, ram_amount, rom_pointer),
      install_memory_switches_(install_memory_switches) {
    for (int i = 0; i < 15; i++) {
        int_rom_ptable[i].read_p = nullptr;
        int_rom_ptable[i].write_p = nullptr;
        int_rom_ptable[i].read_h = {nullptr, nullptr};
        int_rom_ptable[i].write_h = {nullptr, nullptr};
        int_rom_ptable[i].shadow_h = {nullptr, nullptr};
        int_rom_ptable[i].read_d = nullptr;
        int_rom_ptable[i].write_d = nullptr;
    }
    init_map();
    power_on_randomize(main_ram, static_cast<int>(ram_size_));
    install_slot_rom();
    if (install_memory_switches_) {
        install_memory_handlers();
        bsr_map_memory();
    }
}

MMU_IIe2::~MMU_IIe2() = default;

DebugFormatter *MMU_IIe2::debug_formatter() {
    auto *f = new DebugFormatter();
    f->addLine("80ST: %1d RAMR: %1d RAMW: %1d ALTZP: %1d SLOTC3: %1d",
               f_80store, f_ramrd, f_ramwrt, f_altzp, f_slotc3rom);
    f->addLine("INTCX: %1d LC [ BNK1: %1d READ: %1d WRT: %1d ]",
               f_intcxrom, ll.FF_BANK_1, ll.FF_READ_ENABLE, !ll._FF_WRITE_ENABLE);
    f->addLine("PAGE2: %1d HIRES: %1d", s_page2, s_hires);
    f->addLine("SlotReg: %02X", get_slot_register());
    debug_output_page(f, 0x00, true);
    debug_output_page(f, 0x02);
    debug_output_page(f, 0x04);
    debug_output_page(f, 0x08);
    debug_output_page(f, 0x20);
    debug_output_page(f, 0x40);
    debug_output_page(f, 0xC1);
    debug_output_page(f, 0xC3);
    debug_output_page(f, 0xC6);
    f->addLine("C8xx_slot: %d", get_C8xx_slot());
    debug_output_page(f, 0xC8);
    debug_output_page(f, 0xD0);
    debug_output_page(f, 0xE0);
    return f;
}

void MMU_IIe2::reset(bool cold_start) {
    MMU_II::reset(cold_start);
    f_intcxrom = false;
    f_slotc3rom = false;
    compose_c1cf();
    if (!install_memory_switches_) {
        return;
    }
    ll.reset();
    f_80store = false;
    f_ramrd = false;
    f_ramwrt = false;
    f_altzp = false;
    s_page2 = false;
    bsr_map_memory();
    compose_map(true);
}

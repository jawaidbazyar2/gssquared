/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Apple Super Serial Card (MOS 6551 ACIA + firmware ROM).
 */

#include "ssc.hpp"

#include "Device_ID.hpp"
#include "MOS6551.hpp"
#include "SlotData.hpp"
#include "computer.hpp"
#include "device_irq_id.hpp"
#include "serial_devices/SerialDevice.hpp"
#include "serial_devices/file/FileDevice.hpp"
#if !defined(__EMSCRIPTEN__)
#include "serial_devices/modem/ModemDevice.hpp"
#endif
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/ResourceFile.hpp"

#include <cstdio>
#include <cstdint>

namespace {

/* SSC offsets within $C0n0–$C0nF */
constexpr uint8_t SSC_DIPSW1 = 0x01;
constexpr uint8_t SSC_DIPSW2 = 0x02;
constexpr uint8_t SSC_DATA = 0x08;
constexpr uint8_t SSC_STATUS = 0x09;
constexpr uint8_t SSC_COMMAND = 0x0A;
constexpr uint8_t SSC_CONTROL = 0x0B;

/*
 * Modem-friendly factory defaults (values as read by firmware).
 * Closed switch (ON) reads as 0; open (OFF) reads as 1.
 *
 * DIPSW1 packing: S1 S2 S3 S4 Z Z S5 S6
 *   9600 baud = OFF OFF OFF ON → 1 1 1 0
 *   Communications mode = OFF OFF → S5=1 S6=1
 *   Unused Z bits open → 1
 *   → 0b11101111 = 0xEF
 *
 * DIPSW2 packing: S1 Z S2 Z S3 S4 S5 CTS
 *   Modem 8N1: SW2-1 ON, SW2-2 ON, SW2-3 OFF, SW2-4 ON → S1=0 S2=0 S3=1 S4=0
 *   Auto-LF off → S5=1; CTS asserted/ready → 0
 *   → 0b01011010 = 0x5A
 *
 * SW2-6 (IRQ enable) is a hardware gate, not in the DIPSW2 read byte — default on.
 */
constexpr uint8_t SSC_DEFAULT_DIPSW1 = 0xEF;
constexpr uint8_t SSC_DEFAULT_DIPSW2 = 0x5A;

struct ssc_state_t : public SlotData {
    ResourceFile *rom_file = nullptr;
    uint8_t *rom = nullptr;
    MMU_II *mmu = nullptr;
    MOS6551 *acia = nullptr;
    SerialDevice *serial_device = nullptr;
    InterruptController *irq_control = nullptr;
    char port_id[16] = {};
    uint8_t dipsw1 = SSC_DEFAULT_DIPSW1;
    uint8_t dipsw2 = SSC_DEFAULT_DIPSW2;
    bool cts_ready = true; /* live CTS into DIPSW2 bit 0; ready = 0 */
    bool dip_irq_enabled = true;
};

void map_rom_ssc(void *context, SlotType_t /*slot*/) {
    auto *st = static_cast<ssc_state_t *>(context);
    if (!st || !st->rom || !st->mmu) {
        return;
    }
    /* Full 2K image at $C800–$CFFF (Cn page lives at file offset $700). */
    for (uint8_t page = 0; page < 8; page++) {
        st->mmu->map_c1cf_page_read_only(page + 0xC8, st->rom + (page * 0x100), "SSC_ROM");
    }
}

uint8_t ssc_read_c0xx(void *context, uint32_t address) {
    auto *st = static_cast<ssc_state_t *>(context);
    const uint8_t off = static_cast<uint8_t>(address & 0x0F);
    switch (off) {
        case SSC_DIPSW1:
            return st->dipsw1;
        case SSC_DIPSW2: {
            uint8_t v = st->dipsw2 & 0xFE;
            if (!st->cts_ready) {
                v |= 0x01;
            }
            return v;
        }
        case SSC_DATA:
            return st->acia->read_data();
        case SSC_STATUS:
            return st->acia->read_status();
        case SSC_COMMAND:
            return st->acia->read_command();
        case SSC_CONTROL:
            return st->acia->read_control();
        default:
            return 0x00;
    }
}

void ssc_write_c0xx(void *context, uint32_t address, uint8_t data) {
    auto *st = static_cast<ssc_state_t *>(context);
    const uint8_t off = static_cast<uint8_t>(address & 0x0F);
    switch (off) {
        case SSC_DATA:
            st->acia->write_data(data);
            break;
        case SSC_STATUS:
            st->acia->write_reset(data);
            break;
        case SSC_COMMAND:
            st->acia->write_command(data);
            break;
        case SSC_CONTROL:
            st->acia->write_control(data);
            break;
        default:
            break;
    }
}

} // namespace

void init_slot_ssc(computer_t *computer, SlotType_t slot) {
    auto *st = new ssc_state_t();
    st->id = DEVICE_ID_SUPER_SERIAL;
    st->_slot = slot;
    st->mmu = computer->mmu;
    st->irq_control = computer->irq_control;
    st->dipsw1 = SSC_DEFAULT_DIPSW1;
    st->dipsw2 = SSC_DEFAULT_DIPSW2;
    st->cts_ready = true;
    st->dip_irq_enabled = true;

    ResourceFile *rom = new ResourceFile("roms/cards/ssc/341-0065-A.bin", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "SSC: failed to open ROM\n");
        delete st;
        return;
    }
    rom->load();
    if (rom->size() < 0x800) {
        fprintf(stderr, "SSC: ROM too small (%ju bytes), need 2048\n",
                static_cast<uintmax_t>(rom->size()));
        delete rom;
        delete st;
        return;
    }
    st->rom_file = rom;
    st->rom = rom->get_data();

    const device_irq_id irq_id = static_cast<device_irq_id>(slot);
    const uint64_t timer_base = 0x65510000ull | (static_cast<uint64_t>(slot) << 8);
    st->acia = new MOS6551(st->irq_control, computer->event_timer, computer->clock, irq_id,
                           timer_base);
    st->acia->set_dip_irq_enabled(st->dip_irq_enabled);

    /* $Cn00 is the last page of the 2K image. */
    computer->mmu->set_slot_rom(slot, st->rom + 0x700, "SSC_ROM");
    computer->mmu->set_C8xx_handler(slot, map_rom_ssc, st);

    const uint16_t slot_base = static_cast<uint16_t>(0xC080 + (slot * 0x10));
    for (uint16_t off = 0; off < 0x10; ++off) {
        computer->mmu->set_C0XX_read_handler(slot_base + off, {ssc_read_c0xx, st});
        computer->mmu->set_C0XX_write_handler(slot_base + off, {ssc_write_c0xx, st});
    }

    snprintf(st->port_id, sizeof(st->port_id), "SSC%d", static_cast<int>(slot));
#if defined(__EMSCRIPTEN__)
    st->serial_device = new FileDevice(computer->event_queue, computer->device_frame_dispatcher, st->port_id);
#else
    st->serial_device = new ModemDevice(nullptr, st->port_id);
#endif
    st->acia->set_device(st->serial_device);

    computer->register_debug_display_handler(
        "ssc", DH_SSC,
        [st]() -> DebugFormatter * {
            DebugFormatter *df = new DebugFormatter();
            df->addLine("SSC slot %d  DIPSW1=%02X DIPSW2=%02X cts=%d irq_dip=%d",
                        static_cast<int>(st->_slot), st->dipsw1, st->dipsw2, st->cts_ready,
                        st->dip_irq_enabled);
            if (st->acia) {
                st->acia->debug_output(df);
            }
            return df;
        });

    computer->register_reset_handler([st](bool /*cold_start*/) {
        if (st->acia) {
            st->acia->reset();
        }
        return true;
    });

    computer->register_shutdown_handler([st]() {
        delete st->serial_device;
        st->serial_device = nullptr;
        delete st->acia;
        st->acia = nullptr;
        delete st->rom_file;
        st->rom_file = nullptr;
        st->rom = nullptr;
        delete st;
        return true;
    });

    fprintf(stdout, "SSC: init slot %d (6551 @ $C0%X8)\n", static_cast<int>(slot),
            8 + static_cast<int>(slot));
}

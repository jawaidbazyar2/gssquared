/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Uthernet II slot card (W5100). Register model adapted from AppleWin
 *   Uthernet2 (Andrea Odetti / AppleWin, GPL).
 */

#include "uthernet2.hpp"

#include "SlotData.hpp"
#include "Device_ID.hpp"
#include "mmus/mmu.hpp"

#include <cstdio>
#include <memory>

#if defined(GS2_UTHERNET2_STUB) || defined(__EMSCRIPTEN__)

void init_slot_uthernet2(computer_t * /*computer*/, SlotType_t /*slot*/) {
    fprintf(stderr, "Uthernet II: not available on this platform\n");
}

#else

#include "w5100_chip.hpp"
#include "w5100.hpp"

namespace {

struct uthernet2_data : public SlotData {
    std::unique_ptr<u2::W5100Chip> chip;
};

uint8_t u2_read_c0xx(void *context, uint32_t addr) {
    auto *d = static_cast<uthernet2_data *>(context);
    if (!d || !d->chip) {
        return 0xFF;
    }
    return d->chip->io_read(static_cast<uint8_t>(addr & U2_C0X_MASK));
}

void u2_write_c0xx(void *context, uint32_t addr, uint8_t data) {
    auto *d = static_cast<uthernet2_data *>(context);
    if (!d || !d->chip) {
        return;
    }
    d->chip->io_write(static_cast<uint8_t>(addr & U2_C0X_MASK), data);
}

}  // namespace

void init_slot_uthernet2(computer_t *computer, SlotType_t slot) {
    auto *d = new uthernet2_data();
    d->id = DEVICE_ID_UTHERNET2;
    d->_slot = slot;
    d->chip = std::make_unique<u2::W5100Chip>();
    if (!d->chip->start()) {
        fprintf(stderr, "Uthernet II: failed to start network worker\n");
        delete d;
        return;
    }

    const uint16_t slot_base = static_cast<uint16_t>(0xC080 + (slot * 0x10));
    // A0/A1 only — alias all 16 bytes onto the four registers.
    for (uint16_t off = 0; off < 0x10; ++off) {
        computer->mmu->set_C0XX_read_handler(slot_base + off, {u2_read_c0xx, d});
        computer->mmu->set_C0XX_write_handler(slot_base + off, {u2_write_c0xx, d});
    }

    computer->register_reset_handler([d](bool cold) {
        if (d->chip) {
            d->chip->reset(cold);
        }
        return true;
    });

    computer->register_shutdown_handler([d]() {
        if (d->chip) {
            d->chip->stop();
            d->chip.reset();
        }
        delete d;
        return true;
    });
}

#endif

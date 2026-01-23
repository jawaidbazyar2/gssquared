#include <cstdint>

#include "iwm_device.hpp"
#include "computer.hpp"
#include "IWM.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"

uint8_t iwm_read_C0xx(void *context, uint32_t address) {
    static uint8_t random = 0;
    iwm_state_t *st = (iwm_state_t *)context;
    
    return st->iwm->read(address & 0x0F);
}

void iwm_write_C0xx(void *context, uint32_t address, uint8_t data) {
    iwm_state_t *st = (iwm_state_t *)context;

    st->iwm->write(address & 0x0F, data);
}

/*
 * Disk Register read/write 
 */
uint8_t iwm_read_C031(void *context, uint32_t address) {
    iwm_state_t *st = (iwm_state_t *)context;
    return st->iwm->read_disk_register();
}

void iwm_write_C031(void *context, uint32_t address, uint8_t data) {
    iwm_state_t *st = (iwm_state_t *)context;
    st->iwm->write_disk_register(data);
}

DebugFormatter *debug_iwm(iwm_state_t *st) {
    DebugFormatter *df = new DebugFormatter();
    st->iwm->debug_output(df);
    return df;
}

void init_iwm_slot(computer_t *computer, SlotType_t slot) {

    iwm_state_t *st = new iwm_state_t();
    st->iwm = new IWM();

    for (uint32_t i = 0xC0E0; i <= 0xC0EF; i++) {
        computer->mmu->set_C0XX_write_handler(i, { iwm_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { iwm_read_C0xx, st });
    }

    computer->mmu->set_C0XX_read_handler(0xC031, { iwm_read_C031, st });
    computer->mmu->set_C0XX_write_handler(0xC031, { iwm_write_C031, st });

    computer->register_debug_display_handler(
        "iwm",
        DH_IWM, // unique ID for this, need to have in a header.
        [st]() -> DebugFormatter * {
            return debug_iwm(st);
        }
    );

}

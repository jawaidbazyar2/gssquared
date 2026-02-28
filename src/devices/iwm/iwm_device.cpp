#include <cstdint>

#include "iwm_device.hpp"
#include "computer.hpp"
#include "IWM.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/media.hpp"
#include "util/mount.hpp"

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
    st->computer = computer;
    st->iwm = new IWM(computer->sound_effect, computer->clock);

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

    computer->register_reset_handler(
        [st]() {
            st->iwm->reset();
            return true;
        });

    // TODO: this might need to go into the Floppy525 constructor.
    storage_key_t key;
    key.slot = 6;
    key.drive = 0;
    computer->mounts->register_storage_device(key, st->iwm->get_drive(0), DRIVE_TYPE_APPLEDISK_525);
    key.drive = 1;
    computer->mounts->register_storage_device(key + 1, st->iwm->get_drive(1), DRIVE_TYPE_APPLEDISK_525);
    
    // TODO: register the 3.5s here later

    // register frame handler for soundeffects etc.
    computer->device_frame_dispatcher->registerHandler(
        [st]() {
            // motor off timer check. WAY easier to do here than in the drive.
            st->iwm->check_motor_off_timer();

            if (st->computer->execution_mode == EXEC_NORMAL) {
                if (st->iwm->get_motor()) {
                    st->iwm->soundeffects_update();
                }
            }
            return true;
        });

}

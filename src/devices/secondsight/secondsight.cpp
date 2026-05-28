#include "secondsight.hpp"
#include "computer.hpp"
#include "videosystem.hpp"
#include "cpu.hpp"
#include "util/DebugHandlerIDs.hpp"

/*
ssCOMMAND =    $E0C0B0
ssWRITEDATA =  $E0C0B1
ssREADDATA =   $E0C0B2
ssHANDSHAKE =  $E0C0B8
*/

struct secondsight_state_t {
    SecondSight *secondsight;
};

void secondsight_write_00(void *context, uint32_t address, uint8_t value) {
    secondsight_state_t *st = (secondsight_state_t *)context;
    st->secondsight->write_cmd(value);
}

void secondsight_write_01(void *context, uint32_t address, uint8_t value) {
    secondsight_state_t *st = (secondsight_state_t *)context;
    st->secondsight->write_data(address, value);
}

uint8_t secondsight_read_02(void *context, uint32_t address) {
    secondsight_state_t *st = (secondsight_state_t *)context;
    return st->secondsight->read_data();
}

uint8_t secondsight_read_08(void *context, uint32_t address) {
    secondsight_state_t *st = (secondsight_state_t *)context;
    return st->secondsight->read_handshake();
}

void init_secondsight(computer_t *computer, SlotType_t slot) {
    secondsight_state_t *st = new secondsight_state_t();
    // we pass in the "CPU" MMU, i.e., the one that has access to full IIgs memory.
    st->secondsight = new SecondSight(computer->video_system, computer->cpu->mmu, computer->clock);

    uint16_t slot_base = 0xC080 + (slot * 0x10);
    
    computer->mmu->set_C0XX_write_handler(slot_base+0x00, { secondsight_write_00, st });
    computer->mmu->set_C0XX_write_handler(slot_base+0x01, { secondsight_write_01, st });
    // C0B2 can ALSO be a write.
    computer->mmu->set_C0XX_write_handler(slot_base+0x02, { secondsight_write_01, st });
    computer->mmu->set_C0XX_read_handler(slot_base+0x02, { secondsight_read_02, st });
    computer->mmu->set_C0XX_read_handler(slot_base+0x08, { secondsight_read_08, st });

    computer->video_system->register_frame_processor(1, [st,computer](bool force_full_frame) -> bool {
        bool ret = st->secondsight->frame();
        if (ret) {
            // TODO: clear the main video scan buffer. this is sort of ugly to have here.. big dependency.
            computer->clock->get_video_scanner()->get_frame_scan()->clear();
        }
        return ret;
    });

    computer->register_reset_handler([st](bool cold_start) {
        st->secondsight->reset();
        return true;
    });

    computer->register_shutdown_handler([st]() {
        delete st->secondsight;
        delete st;
        return true;
    });

    computer->register_debug_display_handler("ss", DH_SECOND_SIGHT, [st]() -> DebugFormatter * {
        DebugFormatter *df = new DebugFormatter();
        st->secondsight->debug(df);
        return df;
    });

}

#include "applemouseiii.hpp"

#include <iostream>
#include <string>
#include <vector>

#include "SDL3/SDL_events.h"

#include "debug.hpp"
#include "Device_ID.hpp"
#include "gs2.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/printf_helper.hpp"

namespace {

void applemouseiii_apply_rom_bank(applemouseiii_state_t *ds, uint8_t bank) {
    if (!ds || !ds->rom || !ds->computer || !ds->computer->mmu) {
        return;
    }
    if (bank > 7) {
        bank = 7;
    }
    ds->computer->mmu->set_slot_rom(ds->_slot, ds->rom + (static_cast<size_t>(bank) << 8),
                                    "APPLEMOUSEIII_ROM");
}

void applemouseiii_set_irq(applemouseiii_state_t *ds, bool asserted) {
    if (!ds || !ds->irq_control) {
        return;
    }
    ds->irq_control->set_irq(static_cast<device_irq_id>(ds->_slot), asserted);
}

void applemouseiii_schedule_vbl(applemouseiii_state_t *ds);

void applemouseiii_vbl_interrupt(uint64_t instanceID, void *user_data) {
    auto *ds = static_cast<applemouseiii_state_t *>(user_data);
    ds->vbl_cycle = ds->computer->get_frame_start_cycle() + ds->clock->get_c14m_per_frame()
                    + ds->clock->get_c14m_per_scanline() * 192;
    ds->controller.on_vbl();
    if (ds->vbl_cycle <= ds->clock->get_c14m()) {
        fprintf(stdout, "AppleMouse III vbl cycle is before current cycle: %llu < %llu\n",
                u64_t(ds->vbl_cycle), u64_t(ds->clock->get_c14m()));
        ds->vbl_timer_armed = false;
        return;
    }
    ds->event_timer->scheduleEvent(ds->vbl_cycle, applemouseiii_vbl_interrupt, instanceID, ds);
    ds->vbl_timer_armed = true;
}

void applemouseiii_schedule_vbl(applemouseiii_state_t *ds) {
    if (!ds || !ds->event_timer || !ds->computer || !ds->clock) {
        return;
    }
    if (ds->vbl_timer_armed) {
        return;
    }
    const uint64_t when = ds->computer->get_frame_start_cycle() + ds->clock->get_c14m_per_frame()
                          + ds->clock->get_c14m_per_scanline() * 192;
    const uint64_t instance = 0x11000000ull | (static_cast<uint64_t>(ds->_slot) << 8);
    ds->event_timer->scheduleEvent(when, applemouseiii_vbl_interrupt, instance, ds);
    ds->vbl_timer_armed = true;
}

void applemouseiii_vbl_mode(applemouseiii_state_t *ds, bool enabled) {
    if (enabled) {
        applemouseiii_schedule_vbl(ds);
    }
    // Timer keeps running once armed; on_vbl() is mode-gated in the controller.
}

uint8_t applemouseiii_read_c0xx(void *context, uint32_t address) {
    auto *ds = static_cast<applemouseiii_state_t *>(context);
    return ds->controller.pia_read(static_cast<uint16_t>(address & 0x0F));
}

void applemouseiii_write_c0xx(void *context, uint32_t address, uint8_t value) {
    auto *ds = static_cast<applemouseiii_state_t *>(context);
    ds->controller.pia_write(address & 0x0F, value);
}

bool applemouseiii_motion(applemouseiii_state_t *ds, const SDL_Event &event) {
    if (event.type != SDL_EVENT_MOUSE_MOTION) {
        return false;
    }
    int motion_x = event.motion.xrel / 2;
    int motion_y = event.motion.yrel / 2;
    if (motion_x == 0 && motion_y == 0) {
        return false;
    }
    if (motion_x > 127) {
        motion_x = 127;
    }
    if (motion_x < -128) {
        motion_x = -128;
    }
    if (motion_y > 127) {
        motion_y = 127;
    }
    if (motion_y < -128) {
        motion_y = -128;
    }
    ds->controller.move_xy(static_cast<int8_t>(motion_x), static_cast<int8_t>(motion_y));
    return true;
}

bool applemouseiii_updown(applemouseiii_state_t *ds, const SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            ds->controller.update_button(1, true);
        } else {
            ds->controller.update_button(0, true);
        }
        return true;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            ds->controller.update_button(1, false);
        } else {
            ds->controller.update_button(0, false);
        }
        return true;
    }
    return false;
}

DebugFormatter *debug_applemouseiii(applemouseiii_state_t *ds) {
    auto *df = new DebugFormatter();
    df->addLine("   AppleMouse III ");
    df->addLine("  X: %6d Y: %6d  bank: %u", ds->controller.x(), ds->controller.y(),
                ds->controller.rom_bank());
    df->addLine("  Clamp X: %d..%d  Y: %d..%d", ds->controller.clamp_min_x(),
                ds->controller.clamp_max_x(), ds->controller.clamp_min_y(),
                ds->controller.clamp_max_y());
    df->addLine("  Mode: %02X  IntState: %02X  IRQ: %d", ds->controller.operating_mode(),
                ds->controller.int_state(), ds->controller.irq_asserted() ? 1 : 0);
    df->addLine("  Buttons: %d %d", ds->controller.button0() ? 1 : 0,
                ds->controller.button1() ? 1 : 0);
    const PIA6520 &pia = ds->controller.pia();
    df->addLine("  PIA OR=%02X/%02X DDR=%02X/%02X CR=%02X/%02X I=%02X/%02X", pia.ORA, pia.ORB,
                pia.DDRA, pia.DDRB, pia.CRA, pia.CRB, pia.IA, pia.IB);
    return df;
}

} // namespace

void init_applemouseiii(computer_t *computer, SlotType_t slot) {
    auto *ds = new applemouseiii_state_t;
    ds->id = DEVICE_ID_MOUSE;
    ds->computer = computer;
    ds->irq_control = computer->irq_control;
    ds->clock = computer->clock;
    ds->event_timer = computer->event_timer;
    ds->_slot = slot;

    ResourceFile *rom = new ResourceFile("roms/cards/applemouseiii/342-0270-C.bin", READ_ONLY);
    if (rom == nullptr) {
        std::cerr << "Failed to load applemouseiii ROM\n";
        delete ds;
        return;
    }
    rom->load();
    ds->rom_file = rom;
    ds->rom = rom->get_data();
    if (!ds->rom) {
        std::cerr << "Failed to get applemouseiii ROM data\n";
        delete ds;
        return;
    }

    ds->controller.init(
        [ds](bool asserted) { applemouseiii_set_irq(ds, asserted); },
        [ds](uint8_t bank) { applemouseiii_apply_rom_bank(ds, bank); },
        [ds](bool enabled) { applemouseiii_vbl_mode(ds, enabled); });

    applemouseiii_apply_rom_bank(ds, 0);

    const uint16_t slot_address = static_cast<uint16_t>(0xC080 + (slot * 0x10));
    for (int i = 0; i < 0x10; i++) {
        computer->mmu->set_C0XX_read_handler(slot_address + i, {applemouseiii_read_c0xx, ds});
        computer->mmu->set_C0XX_write_handler(slot_address + i, {applemouseiii_write_c0xx, ds});
    }

    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "1");

    computer->register_reset_handler([ds](bool /*cold_start*/) {
        ds->controller.reset();
        applemouseiii_apply_rom_bank(ds, 0);
        return true;
    });

    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_MOTION, [ds](const SDL_Event &event) {
        return applemouseiii_motion(ds, event);
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, [ds](const SDL_Event &event) {
        applemouseiii_updown(ds, event);
        return true;
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_UP, [ds](const SDL_Event &event) {
        applemouseiii_updown(ds, event);
        return true;
    });

    computer->register_debug_display_handler(
        "mouse", DH_APPLEMOUSEIII, [ds]() -> DebugFormatter * {
            return debug_applemouseiii(ds);
        });

    computer->register_device_debug(
        DEVICE_ID_MOUSE,
        [ds](uint32_t op, const std::vector<uint8_t> &req, std::vector<uint8_t> &reply,
             std::string &err) -> bool {
            if (op == DEVOP_STATE_GET) {
                reply.resize(APPLEMOUSEIII_STATE_GET_V1_SIZE);
                if (!ds->controller.pack_state_get(reply.data(), reply.size(),
                                                   static_cast<uint8_t>(ds->_slot))) {
                    err = "STATE_GET pack failed";
                    return false;
                }
                return true;
            }
            if (op == DEVOP_STATE_SET) {
                if (!ds->controller.apply_state_set(req.data(), req.size())) {
                    err = "STATE_SET rejected";
                    return false;
                }
                reply.clear();
                return true;
            }
            err = "unsupported device op";
            return false;
        });

    // Keep VBL timer armed so SETMOUSE VBL mode can latch without re-arm races.
    applemouseiii_schedule_vbl(ds);

    if (DEBUG(DEBUG_MOUSE)) {
        fprintf(stdout, "AppleMouse III initialized in slot %d\n", slot);
    }
}

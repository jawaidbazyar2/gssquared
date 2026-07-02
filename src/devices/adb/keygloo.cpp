#include <SDL3/SDL.h>

#include <cstdio>

#include "computer.hpp"
#include "videosystem.hpp"

#include "agent/Protocol.hpp"        // AGENT_MOUSEID, AGENT_USER_SET_MOUSE_MODE
#include "devices/adb/keygloo.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/Event.hpp"
#include "debug.hpp"

namespace {

// Returns true if this SDL event was synthesized by the agent (i.e.
// stamped with AGENT_MOUSEID in its `which` field). We only need to
// inspect mouse events; key events flow through unchanged in all modes.
bool is_agent_mouse_event(const SDL_Event &event) {
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            return event.motion.which == agent::protocol::AGENT_MOUSEID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return event.button.which == agent::protocol::AGENT_MOUSEID;
        default:
            return false;
    }
}

bool is_mouse_event(const SDL_Event &event) {
    return event.type == SDL_EVENT_MOUSE_MOTION ||
           event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
           event.type == SDL_EVENT_MOUSE_BUTTON_UP;
}

}  // namespace


void keygloo_update_interrupt_status(keygloo_state_t *kb_state, KeyGloo *kg ) {
    // TODO: check if mouse interrupt is enabled, and if so, assert it.
    if (kg->interrupt_status()) {
        kb_state->irq_control->set_irq(IRQ_ID_KEYGLOO, true);
    } else {
        kb_state->irq_control->set_irq(IRQ_ID_KEYGLOO, false);
    }
    if (kg->data_interrupt_status()) {
        kb_state->irq_control->set_irq(IRQ_ID_ADB_DATAREG, true);
    } else {
        kb_state->irq_control->set_irq(IRQ_ID_ADB_DATAREG, false);
    }
}

uint8_t keygloo_read_C000(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    return kg->read_key_latch();
}

uint8_t keygloo_read_C010(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    return kg->read_key_strobe();
}

void keygloo_write_C010(void *context, uint32_t address, uint8_t value) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    kg->write_key_strobe(value);
}

uint8_t keygloo_read_C025(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    return kg->read_mod_latch();
}

uint8_t keygloo_read_C024(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    uint8_t data = kg->read_mouse_data();
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
    return data;
}

uint8_t keygloo_read_C026(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    uint8_t data = kg->read_data_register();
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
    return data;
}

void keygloo_write_C026(void *context, uint32_t address, uint8_t value) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    kg->write_cmd_register(value);
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
}

uint8_t keygloo_read_C027(void *context, uint32_t address) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    return kg->read_status_register();
}

void keygloo_write_C027(void *context, uint32_t address, uint8_t value) {
    keygloo_state_t *kb_state = (keygloo_state_t *)context;
    KeyGloo *kg = kb_state->kg;
    kg->write_status_register(value);
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
}

bool keygloo_process_event(keygloo_state_t *kb_state, const SDL_Event &event) {
    KeyGloo *kg = kb_state->kg;

    // Mouse-mode filtering. In DISABLED mode the IIgs only sees events
    // the agent injected (AGENT_MOUSEID-tagged). In FOLLOW_HOST/CAPTURE
    // both real and agent events flow through. Key events are never
    // filtered here. We still return true so the dispatcher considers
    // the event consumed — letting it fall through would just hand
    // it to the next handler, which is rarely what we want.
    if (is_mouse_event(event) &&
        kb_state->mouse_mode == MOUSE_MODE_DISABLED &&
        !is_agent_mouse_event(event)) {
        return true;
    }

    SDL_Event event_copy = event;
    kg->process_event(event_copy);
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
    return true;
}

// ---- Mode plumbing -----------------------------------------------------
//
// All functions below run on the main (SDL event) thread. Lookups via
// computer->get_module_state(MODULE_KEYGLOO) are non-locking; the agent's
// reader thread doesn't touch keygloo_state_t directly — it pushes a
// AGENT_USER_SET_MOUSE_MODE custom event whose handler (in gs2.cpp's
// SDL_AppEvent) calls keygloo_set_mouse_mode on the main thread.

const char *keygloo_mouse_mode_name(MouseMode mode) {
    switch (mode) {
        case MOUSE_MODE_FOLLOW_HOST: return "follow host";
        case MOUSE_MODE_CAPTURE:     return "capture";
        case MOUSE_MODE_DISABLED:    return "disabled (agent only)";
        default:                     return "?";
    }
}

// EVENT_SHOW_MESSAGE's Event ctor stashes the char* as a uint64_t
// without copying (see util/Event.cpp), so we MUST hand it a string
// with static lifetime. Building one with snprintf into a stack buffer
// corrupts the OSD as soon as the calling frame returns. These literals
// are the per-mode toasts; use the function rather than building strings
// at the call site.
static const char *toast_for_mode(MouseMode mode) {
    switch (mode) {
        case MOUSE_MODE_FOLLOW_HOST: return "Mouse mode: follow host";
        case MOUSE_MODE_CAPTURE:     return "Mouse mode: capture";
        case MOUSE_MODE_DISABLED:    return "Mouse mode: disabled (agent only)";
        default:                     return "Mouse mode: ?";
    }
}

static keygloo_state_t *keygloo_state(computer_t *computer) {
    if (computer == nullptr) return nullptr;
    return (keygloo_state_t *)computer->get_module_state(MODULE_KEYGLOO);
}

void keygloo_set_mouse_mode(computer_t *computer, MouseMode mode) {
    keygloo_state_t *kb_state = keygloo_state(computer);
    if (kb_state == nullptr) return;
    if (mode < 0 || mode >= MOUSE_MODE_COUNT) return;

    const MouseMode previous = kb_state->mouse_mode;
    kb_state->mouse_mode = mode;

    // SDL relative mouse mode is the only side-effect that touches
    // host state — only flip it when entering or leaving CAPTURE.
    const bool was_capture = (previous == MOUSE_MODE_CAPTURE);
    const bool now_capture = (mode == MOUSE_MODE_CAPTURE);
    if (was_capture != now_capture &&
        computer != nullptr && computer->video_system != nullptr) {
        computer->video_system->display_capture_mouse(now_capture);
    }

    // Surface the change to the user via the on-screen message overlay
    // (the same path F1's existing "Mouse Captured" toast uses) and to
    // stderr for the agent log.
    if (computer != nullptr && computer->video_system != nullptr &&
        computer->video_system->event_queue != nullptr) {
        computer->video_system->event_queue->addEvent(
            new Event(EVENT_SHOW_MESSAGE, 0, toast_for_mode(mode)));
    }
    std::fprintf(stderr, "[keygloo] mouse mode: %s -> %s\n",
                 keygloo_mouse_mode_name(previous),
                 keygloo_mouse_mode_name(mode));
}

void keygloo_cycle_mouse_mode(computer_t *computer) {
    keygloo_state_t *kb_state = keygloo_state(computer);
    if (kb_state == nullptr) return;
    const MouseMode next = static_cast<MouseMode>(
        (static_cast<int>(kb_state->mouse_mode) + 1) % MOUSE_MODE_COUNT);
    keygloo_set_mouse_mode(computer, next);
}

MouseMode keygloo_get_mouse_mode(computer_t *computer) {
    keygloo_state_t *kb_state = keygloo_state(computer);
    if (kb_state == nullptr) return MOUSE_MODE_FOLLOW_HOST;
    return kb_state->mouse_mode;
}

DebugFormatter *debug_keygloo(keygloo_state_t *kb_state) {
    DebugFormatter *df = new DebugFormatter();
    kb_state->kg->debug_display(df);
    return df;
}

void init_slot_keygloo(computer_t *computer, SlotType_t slot) {

    keygloo_state_t *kb_state = new keygloo_state_t;
    computer->set_module_state(MODULE_KEYGLOO, kb_state);

    kb_state->computer = computer;
    kb_state->irq_control = computer->irq_control;
    kb_state->mmu = computer->mmu;
    kb_state->reset_control = computer->reset_control;

    KeyGloo *kg = new KeyGloo(kb_state->reset_control);
    kb_state->kg = kg;

    computer->dispatch->registerHandler(SDL_EVENT_KEY_DOWN, [kb_state](const SDL_Event &event) {
        return keygloo_process_event(kb_state, event);
    });
    computer->dispatch->registerHandler(SDL_EVENT_KEY_UP, [kb_state](const SDL_Event &event) {
        return keygloo_process_event(kb_state, event);
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_MOTION, [kb_state](const SDL_Event &event) {
        return keygloo_process_event(kb_state, event);
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, [kb_state](const SDL_Event &event) {
        return keygloo_process_event(kb_state, event);
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_UP, [kb_state](const SDL_Event &event) {
        return keygloo_process_event(kb_state, event);
    });

    for (int i = 0xC000; i <= 0xC00F; i++) { // should mirror C000 like //e
        computer->mmu->set_C0XX_read_handler(i, { keygloo_read_C000, kb_state });
    }
    //computer->mmu->set_C0XX_read_handler(0xC000, { keygloo_read_C000, kb_state });
    computer->mmu->set_C0XX_read_handler(0xC010, { keygloo_read_C010, kb_state });
    computer->mmu->set_C0XX_write_handler(0xC010, { keygloo_write_C010, kb_state });
    computer->mmu->set_C0XX_read_handler(0xC025, { keygloo_read_C025, kb_state });
    computer->mmu->set_C0XX_read_handler(0xC024, { keygloo_read_C024, kb_state });
    computer->mmu->set_C0XX_read_handler(0xC026, { keygloo_read_C026, kb_state });
    computer->mmu->set_C0XX_write_handler(0xC026, { keygloo_write_C026, kb_state });
    computer->mmu->set_C0XX_read_handler(0xC027, { keygloo_read_C027, kb_state });
    computer->mmu->set_C0XX_write_handler(0xC027, { keygloo_write_C027, kb_state });

    computer->device_frame_dispatcher->registerHandler([kb_state]() {
        kb_state->kg->frame_handler();
        return true;
    });
    computer->register_debug_display_handler(
        "adb",
        DH_ADB, // unique ID for this, need to have in a header.
        [kb_state]() -> DebugFormatter * {
            return debug_keygloo(kb_state);
        }
    );

    computer->register_reset_handler([kb_state](bool cold_start) {
        if (cold_start) {
            kb_state->kg->zero_0x51();
        }
        kb_state->kg->reset();
        return true;
    });

}
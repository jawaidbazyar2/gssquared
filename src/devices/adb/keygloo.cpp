#include <SDL3/SDL.h>

#include "computer.hpp"

#include "devices/adb/keygloo.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "debug.hpp"


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
    SDL_Event event_copy = event;
    kg->process_event(event_copy);
    keygloo_update_interrupt_status(kb_state, kg); // could have IRQ after event..
    return true;
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
    
    KeyGloo *kg = new KeyGloo();
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

    computer->register_debug_display_handler(
        "adb",
        DH_ADB, // unique ID for this, need to have in a header.
        [kb_state]() -> DebugFormatter * {
            return debug_keygloo(kb_state);
        }
    );

    computer->register_reset_handler([kb_state]() {
        kb_state->kg->reset();
        return true;
    });

}
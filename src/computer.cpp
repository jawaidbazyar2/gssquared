#include <iostream>

#include "gs2.hpp"
#include "cpu.hpp"

#include "computer.hpp"
#include "debugger/debugwindow.hpp"
#include "util/EventDispatcher.hpp"
#include "util/EventTimer.hpp"
#include "videosystem.hpp"
#include "util/mount.hpp"
#include "platforms.hpp"
#include "mbus/MessageBus.hpp"
#include "util/DebugFormatter.hpp"

computer_t::computer_t() {
    // lots of stuff is going to need this.
    event_queue = new EventQueue();
    if (!event_queue) {
        std::cerr << "Failed to allocate event queue for CPU " << std::endl;
        exit(1);
    }

    event_timer = new EventTimer();

    mbus = new MessageBus();

    sys_event = new EventDispatcher(); // different queue for "system" events that get processed first.
    dispatch = new EventDispatcher(); // has to be very first thing, devices etc are going to immediately register handlers.
    device_frame_dispatcher = new DeviceFrameDispatcher();

    cpu = new cpu_state();
    //cpu->init();

    mounts = new Mounts(cpu);

    video_system = new video_system_t(this);
    debug_window = new debug_window_t(this);

    sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [this](const SDL_Event &event) {
        int key = event.key.key;
        SDL_Keymod mod = event.key.mod;
        if ((mod & SDL_KMOD_CTRL) && (key == SDLK_F10)) {
            if (mod & SDL_KMOD_ALT) {
                reset(true); 
            } else {
                reset(false); 
            }
            return true;
        }
        if (key == SDLK_F12) { 
            cpu->halt = HLT_USER; 
            return true;
        }
        if (key == SDLK_F9) { 
            toggle_clock_mode(cpu);
            send_clock_mode_message();
            return true; 
        }
        return false;
    });
    sys_event->registerHandler(SDL_EVENT_QUIT, [this](const SDL_Event &event) {
        cpu->halt = HLT_USER;
        return true;
    });

}

computer_t::~computer_t() {
    // TODO: call shutdown() handlers on all devices that registered one.
    for (auto& handler : shutdown_handlers) {
        handler();
    }
    delete cpu;
    delete video_system;
    delete debug_window;
    delete event_timer;
    delete sys_event;
    delete dispatch;
    delete device_frame_dispatcher;
}

void computer_t::register_reset_handler(ResetHandler handler) {
    reset_handlers.push_back(handler);
}

void computer_t::register_shutdown_handler(ShutdownHandler handler) {
    shutdown_handlers.push_back(handler);
}

void computer_t::register_debug_display_handler(std::string name, uint64_t id, DebugDisplayHandler handler) {
    debug_display_handlers.push_back({name, id, handler});
}

DebugFormatter *computer_t::call_debug_display_handler(std::string name) {
    for (auto& handler : debug_display_handlers) {
        if (handler.name == name) {
            return handler.handler();
        }
    }
    return 0;
}

void computer_t::reset(bool cold_start) {

    if (cold_start) {
        // force a cold start reset
        mmu->write(0x3f2, 0x00);
        mmu->write(0x3f3, 0x00);
        mmu->write(0x3f4, 0x00);
    }

    mmu->reset(); // this first, so when CPU fetches PC from RESET it will be based on main memory/rom.
    cpu->reset();
    
//    mmu->init_map(); // changed to reset() above.

    /* for (reset_handler_rec rec : reset_handlers) {
        rec.handler(rec.context);
    } */
    for (auto& handler : reset_handlers) {
        handler();
    }
}


/* void computer_t::registerHandler(EventHandler handler) {
    handlers.push_back(handler);
}

void DeviceFrameDispatcher::dispatch() {
    for (auto& handler : handlers) {
        handler();
    }
} */

/** State storage for non-slot devices. */
void *computer_t::get_module_state(module_id_t module_id) {
    void *state = cpu->module_store[module_id];
    if (state == nullptr) {
        fprintf(stderr, "Module %d not initialized\n", module_id);
    }
    return state;
}

void computer_t::set_module_state(module_id_t module_id, void *state) {
    cpu->module_store[module_id] = state;
}

/** State storage for slot devices. */
SlotData *computer_t::get_slot_state(SlotType_t slot) {
    SlotData *state = cpu->slot_store[slot];
    /* if (state == nullptr) {
        fprintf(stderr, "Slot Data for slot %d not initialized\n", slot);
    } */
    return state;
}

SlotData *computer_t::get_slot_state_by_id(device_id id) {
    for (int i = 0; i < 8; i++) {
        if (cpu->slot_store[i] && cpu->slot_store[i]->id == id) {
            return cpu->slot_store[i];
        }
    }
    return nullptr;
}

void computer_t::set_slot_state( SlotType_t slot, /* void */ SlotData *state) {
    state->_slot = slot;
    cpu->slot_store[slot] = state;
}

void computer_t::set_slot_irq(uint8_t slot, bool irq) {
    if (irq) {
        cpu->irq_asserted |= (1 << slot);
    } else {
        cpu->irq_asserted &= ~(1 << slot);
    }
}

// TODO: should live inside a reconstituted clock class.
void computer_t::send_clock_mode_message() {
    static char buffer[256];
    const char *clock_mode_names[] = {
        "Ludicrous Speed",
        "1.0205MHz",
        "2.8 MHz",
        "4.0 MHz"
    };

    snprintf(buffer, sizeof(buffer), "Clock Mode Set to %s", clock_mode_names[cpu->clock_mode]);
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, buffer));
}

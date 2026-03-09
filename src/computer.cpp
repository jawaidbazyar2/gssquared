#include <iostream>
#include <cstdint>

#include "PlatformIDs.hpp"
#include "SDL3/SDL_keycode.h"
#include "cpu.hpp"
#include "NClock.hpp"
#include "computer.hpp"
#include "debugger/debugwindow.hpp"
#include "util/EventDispatcher.hpp"
#include "util/EventTimer.hpp"
#include "videosystem.hpp"
#include "util/mount.hpp"
#include "platforms.hpp"
#include "mbus/MessageBus.hpp"
#include "util/DebugFormatter.hpp"
#include "util/applekeys.hpp"
#include "gs2.hpp"
#include "platform-specific/menu.h"
#include "devices/keyboard/keyboard.hpp"
#include "util/InterruptController.hpp"
#include "util/ResetController.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/AudioSystem.hpp"
#include "util/SoundEffect.hpp"
#include "util/printf_helper.hpp"

computer_t::computer_t(NClockII *clock) {
    this->clock = clock;

    // initialize module store to nullptr.
    for (int i = 0; i < MODULE_NUM_MODULES; i++) {
        module_store[i] = nullptr;
    }

    // lots of stuff is going to need this.
    event_queue = new EventQueue();
    if (!event_queue) {
        std::cerr << "Failed to allocate event queue for CPU " << std::endl;
        exit(1);
    }

    mbus = new MessageBus();

    // allocate IRQ controller
    irq_control = new InterruptController();
    // On update, force change in CPU.
    irq_control->register_irq_receiver([this](uint64_t irq_asserted) {
        cpu->irq_asserted = irq_asserted;
    });
    register_debug_display_handler(
        "irq",
        DH_IRQ, // unique ID for this, need to have in a header.
        [this]() -> DebugFormatter * {
            return irq_control->debug_irq();
        }
    );

    reset_control = new ResetController(this);
    reset_control->register_reset_receiver([this](uint64_t reset_asserted) {
        cpu->reset_asserted = reset_asserted;
    });
    /* register_debug_display_handler(
        "reset",
        DH_RESET, // unique ID for this, need to have in a header.
        [this]() -> DebugFormatter * {
            return reset_control->debug_reset();
        }
    ); */


    audio_system = new AudioSystem();
    sound_effect = new SoundEffect(audio_system);

    sys_event = new EventDispatcher(); // different queue for "system" events that get processed first.
    dispatch = new EventDispatcher(); // has to be very first thing, devices etc are going to immediately register handlers.
    device_frame_dispatcher = new DeviceFrameDispatcher();

    cpu = new cpu_state(PROCESSOR_6502); // default to 6502, then we will override later.

    event_timer = new EventTimer(clock); // TODO: clock needs to be set before here?
    vid_event_timer = new EventTimer(clock); 

    slot_manager = new SlotManager_t();
    mounts = new Mounts(slot_manager);

    video_system = new video_system_t(this);
    debug_window = new debug_window_t(this);

    sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [this](const SDL_Event &event) {
        int key = event.key.key;
        SDL_Keymod mod = event.key.mod;
        if (this->platform->id == PLATFORM_APPLE_IIGS) { // try out new reset thing.
            if ((mod & SDL_KMOD_CTRL) && (key == KEY_RESET)) {
                if ((mod & KEYMOD_OPENAPPLE) && (this->platform->id <= PLATFORM_APPLE_II_PLUS)) { // only II+ and earlier.
                    reset(true);
                } else {
                    reset(false); 
                }
                return true;
            }
        }
        if (key == SDLK_F9) { 
            this->speed_shift = true;
            if (mod & SDL_KMOD_SHIFT) {
                this->speed_new = this->clock->toggle(-1);
            } else {
                this->speed_new = this->clock->toggle(1);
            }
            send_clock_mode_message(speed_new);
            return true; 
        }
        return false;
    });
    sys_event->registerHandler(SDL_EVENT_QUIT, [this](const SDL_Event &event) {
        cpu->halt = HLT_USER;
        return true;
    });
    sys_event->registerHandler(gs2_app_values.menu_event_type, [this](const SDL_Event &event) {
        switch (event.user.code) {
            case MENU_MACHINE_RESET:
                reset(false);
                return true;
            case MENU_MACHINE_RESTART:
                return true;
            case MENU_MACHINE_PAUSE_RESUME:
                return true;
            case MENU_MACHINE_CAPTURE_MOUSE:
                video_system->display_capture_mouse_message(true);
                return true;
            case MENU_SPEED_1_0:
                speed_new = CLOCK_1_024MHZ; speed_shift = true;
                send_clock_mode_message(speed_new);
                return true;
            case MENU_SPEED_2_8:
                speed_new = CLOCK_2_8MHZ; speed_shift = true;
                send_clock_mode_message(speed_new);
                return true;
            case MENU_SPEED_7_1:
                speed_new = CLOCK_7_159MHZ; speed_shift = true;
                send_clock_mode_message(speed_new);
                return true;
            case MENU_SPEED_14_3:
                speed_new = CLOCK_14_3MHZ; speed_shift = true;
                send_clock_mode_message(speed_new);
                return true;
            case MENU_MONITOR_COMPOSITE:
                video_system->set_display_engine(DM_ENGINE_NTSC);
                return true;
            case MENU_MONITOR_GS_RGB:
                video_system->set_display_engine(DM_ENGINE_RGB);
                return true;
            case MENU_MONITOR_MONO_GREEN:
                video_system->set_display_engine(DM_ENGINE_MONO);
                video_system->set_display_mono_color(DM_MONO_GREEN);
                return true;
            case MENU_MONITOR_MONO_AMBER:
                video_system->set_display_engine(DM_ENGINE_MONO);
                video_system->set_display_mono_color(DM_MONO_AMBER);
                return true;
            case MENU_MONITOR_MONO_WHITE:
                video_system->set_display_engine(DM_ENGINE_MONO);
                video_system->set_display_mono_color(DM_MONO_WHITE);
                return true;
            case MENU_DISPLAY_FULLSCREEN:
                video_system->toggle_fullscreen();
                send_full_screen_message();
                return true;
            case MENU_EDIT_COPY_SCREEN:
                video_system->copy_screen();
                return true;
            case MENU_EDIT_PASTE_TEXT: {
                keyboard_state_t *kb = (keyboard_state_t *)get_module_state(MODULE_KEYBOARD);
                if (kb) {
                    char *text = SDL_GetClipboardText();
                    if (text) {
                        kb->paste_buffer = std::string(text);
                        SDL_free(text);
                    }
                }
                return true;
            }
            case MENU_OPEN_DEBUG_WINDOW:
                debug_window->set_open();
                return true;
        }
        return false;
    });

    // TODO: it might be nice to, after this is done, remove ourselves from the device_frame_dispatcher.
    device_frame_dispatcher->registerHandler(
        [this]() {
            if (powerup_reset_cycles > 0) {
                powerup_reset_cycles--;
                if (powerup_reset_cycles == 0) {
                    reset_control->assert_reset((device_reset_id)RST_ID_POWERUP, false);
                }
            }
            return true;
        }
    );

}

computer_t::~computer_t() {
    // TODO: call shutdown() handlers on all devices that registered one.
    for (auto& handler : shutdown_handlers) {
        handler();
    }
    delete cpu;
    delete sound_effect;
    delete audio_system;
    delete irq_control;
    delete reset_control;
    delete video_system;
    delete debug_window;
    delete event_timer;
    delete sys_event;
    delete dispatch;
    delete device_frame_dispatcher;
}

void computer_t::set_frame_start_cycle() {
    frame_start_cycle = clock->get_c14m();
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
    
    for (auto& handler : reset_handlers) {
        handler();
    }
}

void computer_t::set_clock(NClockII *clock) {
    this->clock = clock;
    event_timer->set_clock(clock);
}

/** State storage for non-slot devices. */
void *computer_t::get_module_state(module_id_t module_id) {
    void *state = module_store[module_id];
    if (state == nullptr) {
        fprintf(stderr, "Module %d not initialized\n", module_id);
    }
    return state;
}

void computer_t::set_module_state(module_id_t module_id, void *state) {
    module_store[module_id] = state;
}

// TODO: should live inside a reconstituted clock class.
void computer_t::send_clock_mode_message(clock_mode_t clock_mode) {
    static char buffer[256];

    snprintf(buffer, sizeof(buffer), "Clock Mode Set to %s", clock->get_clock_mode_name(clock_mode)); // 
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, buffer));
}

void computer_t::send_full_screen_message() {
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Entering Full Screen Mode. Press F3 to exit."));
}

/**
 * call at end of every frame to update status and statistics.
 */
void computer_t::frame_status_update() {
    if (last_frame_end_time == 0) {
        last_frame_end_time = SDL_GetTicksNS()-1;
        last_5sec_update = last_frame_end_time;
    }
    frame_count++;
    if (frame_count % 60 == 0) {
        uint64_t this_frame_end_time = SDL_GetTicksNS();
        uint64_t frame_counter_delta = this_frame_end_time - last_frame_end_time;

        fps = ((float)frame_count * 1000000000) / frame_counter_delta;
        last_frame_end_time = this_frame_end_time;
        frame_count = 0;

        // TODO: maybe should update this every second instead of every 5 seconds.
        uint64_t delta = clock->get_cycles() - last_5sec_cycles;
        e_mhz = 1000 * (double)delta / ((double)(this_frame_end_time - last_5sec_update));

        status_count++;
        if (status_count == 2) {
            last_5sec_cycles = clock->get_cycles();
            last_5sec_update = this_frame_end_time;
    
            fprintf(stdout, "%llu delta %llu cycles clock-mode: %d CPS: %12.8f MHz [ slips: %llu]\n", 
                u64_t(delta), u64_t(clock->get_cycles()), clock->get_clock_mode(), e_mhz, u64_t(clock_slip));
            uint64_t et = event_times.getAverage();
            uint64_t at = audio_times.getAverage();
            uint64_t dt = display_times.getAverage();
            uint64_t aet = app_event_times.getAverage();
            uint64_t total = et + at + dt + aet;
            fprintf(stdout, "event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", 
                u64_t(et), u64_t(at), u64_t(dt), u64_t(aet), u64_t(total));
            fprintf(stdout, "PC: %04X, A: %02X, X: %02X, Y: %02X, P: %02X\n", 
                cpu->pc, cpu->a, cpu->x, cpu->y, cpu->p);        
            status_count = 0;
        }
    }
}
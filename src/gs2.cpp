/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <regex>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "gs2.hpp"
#include "platform-specific/menu.h"
#include "Module_ID.hpp"
#include "paths.hpp"
#include "cpu.hpp"
#include "display/display.hpp"
#include "devices/speaker/speaker.hpp"
#include "platforms.hpp"
#include "util/dialog.hpp"
#include "util/mount.hpp"
#include "ui/OSD.hpp"
#include "systemconfig.hpp"
#include "slots.hpp"
#include "videosystem.hpp"
#include "debugger/debugwindow.hpp"
#include "computer.hpp"
#include "mmus/mmu_ii.hpp"
#include "mmus/mmu_iie.hpp"
#include "mmus/mmu_iigs.hpp"
#include "util/EventTimer.hpp"
#include "ui/SelectSystem.hpp"
#include "ui/MainAtlas.hpp"
#include "cpus/cpu_implementations.hpp"
#include "version.h"
#include "util/Metrics.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/printf_helper.hpp"

/**
 * References: 
 * Apple Machine Language: Don Inman, Kurt Inman
 * https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
 * https://www.masswerk.at/6502/6502_instruction_set.html#USBC
 * 
 */

/**
 * gssquared
 * 
 * Apple II/+/e/c/GS Emulator Extraordinaire
 * 
 * Main component Goals:
 *  - 6502 CPU (nmos) emulation
 *  - 65c02 CPU (cmos) emulation
 *  - 65sc816 CPU (8 and 16-bit) emulation
 *  - Display Emulation
 *     - Text - 40 / 80 col
 *     - Lores - 40 x 40, 40 x 48, 80 x 40, 80 x 48
 *     - Hires - 280x192 etc.
 *  - Disk I/O
 *   - 5.25 Emulation
 *   - 3.5 Emulation (SmartPort)
 *   - SCSI Card Emulation
 *  - Memory management emulate - a proposed MMU to allow multiple virtual CPUs to each have their own 16M address space
 * User should be able to select the Apple variant: 
 *  - Apple II
 *  - Apple II+
 *  - Apple IIe
 *  - Apple IIe Enhanced
 *  - Apple IIc
 *  - Apple IIc+
 *  - Apple IIGS
 * and edition of ROM.
 */

/** Globals we haven't dealt properly with yet. */
OSD *osd = nullptr;

void handle_single_event(computer_t *computer, cpu_state *cpu, SDL_Event &event) {
    // check for system "pre" events
    if (computer->sys_event->dispatch(event)) {
        return;
    }
    if (computer->debug_window->handle_event(event)) { // ignores event if not for debug window
        return;
    }
    if (!osd->event(event)) { // if osd doesn't handle it..
        computer->dispatch->dispatch(event); // they say call "once per frame"
    }
}

/*
 * In the SDL3 App Callbacks model, SDL delivers events via SDL_AppEvent()
 * which calls handle_single_event() directly. frame_event() is still called
 * from run_one_frame() to maintain the MEASURE timing, but no longer polls
 * events itself. osd->update() is called from SDL_AppIterate().
 */
void frame_event(computer_t *computer, cpu_state *cpu) {
    // Events are now dispatched by SDL_AppEvent; nothing to poll here.
    // osd->update() is called from SDL_AppIterate before run_one_frame().
}

void frame_appevent(computer_t *computer, cpu_state *cpu) {
    Event *event = computer->event_queue->getNextEvent();
    if (event) {
        switch (event->getEventType()) {
            case EVENT_PLAY_SOUNDEFFECT:
                computer->sound_effect->play(event->getEventData());
                break;
            case EVENT_REFOCUS:
                computer->video_system->raise();
                break;
            case EVENT_MODAL_SHOW:
                osd->show_diskii_modal(event->getEventKey(), event->getEventData());
                break;
            case EVENT_MODAL_CLICK:
                {
                    storage_key_t key;
                    key.key = event->getEventKey();

                    uint64_t data = event->getEventData();
                    printf("EVENT_MODAL_CLICK: %llu %llu\n", u64_t(key), u64_t(data));
                    if (data == 1) {
                        // save and unmount.
                        computer->mounts->unmount_media(key, SAVE_AND_UNMOUNT);
                    } else if (data == 2) {
                        // save as - need to open file dialog, get new filename, change media filename, then unmount.
                    } else if (data == 3) {
                        // discard
                        computer->mounts->unmount_media(key, DISCARD);
                    } else if (data == 4) {
                        // cancel
                        // Do nothing!
                    }
                    osd->close_diskii_modal(key, data);
                }
                break;
            case EVENT_SHOW_MESSAGE:
                osd->set_heads_up_message((const char *)event->getEventData(), 512);
                break;
         
        }
        delete event; // processed, we can now delete it.
    }
}

/*
 * Update window
 */
void frame_video_update(computer_t *computer, cpu_state *cpu, bool force_full_frame = false) {

    computer->video_system->update_display(force_full_frame);    
    osd->render();
    computer->debug_window->render();
    computer->video_system->present();
}

void frame_sleep(computer_t *computer, uint64_t last_cycle_time, uint64_t ns_per_frame)
    /* uint64_t frame_count) */ {
    uint64_t wakeup_time = last_cycle_time + ns_per_frame; /*  + (frame_count & 1); */ // even frames have 16688154, odd frames have 16688154 + 1

    // sleep out the rest of this frame.
    uint64_t sleep_loops = 0;
    uint64_t current_time = SDL_GetTicksNS();
    if (current_time > wakeup_time) {
        computer->clock_slip++;
        // TODO: log clock slip for later display.
        //printf("Clock slip: event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
    } else {
        if (gs2_app_values.sleep_mode) { // sleep most of it, but more aggressively sneak up on target than SDL_DelayPrecise does itself
            SDL_DelayPrecise((wakeup_time - SDL_GetTicksNS())*0.95);
        }
        // busy wait sync cycle time
        do {
            sleep_loops++;
        } while (SDL_GetTicksNS() < wakeup_time);

    }
}


DebugFormatter *debug_clock(computer_t *computer) {
    DebugFormatter *f = new DebugFormatter();
    f->addLine("CPU Expected Rate: %d", computer->clock->get_hz_rate());
    f->addLine("CPU eMHZ: %12.8f, FPS: %12.8f", computer->e_mhz, computer->fps);
    f->addLine("CPU Cycle: %12llu", computer->clock->get_cycles());
    f->addLine("Vid Cycle: %12llu", computer->clock->get_vid_cycles());
    f->addLine("14M Cycle: %12llu", computer->clock->get_c14m());

    return f;
}

void register_clock_debug(computer_t *computer) {

    computer->register_debug_display_handler(
        "clock",
        DH_CLOCK, // unique ID for this, need to have in a header.
        [computer]() -> DebugFormatter * {
            return debug_clock(computer);
        }
    );

}


DebugFormatter *debug_mmu_iigs(MMU_IIgs *mmu_iigs) {
    DebugFormatter *f = new DebugFormatter();
    mmu_iigs->debug_dump(f);
    return f;
}

/*
Initialize emulation state before the first frame.
Called from transition_to_emulation() when a system is selected.
*/
void run_cpus_init(computer_t *computer) {
    computer->last_cycle_time = SDL_GetTicksNS();
    computer->last_start_frame_c14m = 0;
    computer->cached_speaker_state = computer->get_module_state(MODULE_SPEAKER);
    computer->cached_display_state = computer->get_module_state(MODULE_DISPLAY);
}

/*
Execute one frame of emulation. Returns true if emulation should continue,
false if the user requested a halt.
*/
bool run_one_frame(computer_t *computer) {
    cpu_state *cpu = computer->cpu;
    NClock *clock = computer->clock;
    speaker_state_t *speaker_state = (speaker_state_t *)computer->cached_speaker_state;
    display_state_t *ds = (display_state_t *)computer->cached_display_state;

    if (cpu->halt == HLT_USER) { // top of frame.
        return false;
    }

    uint64_t c14M_per_frame = clock->get_c14m_per_frame();

    if (computer->speed_shift) {
        computer->speed_shift = false;

        if (clock->get_clock_mode() == CLOCK_FREE_RUN) {
            speaker_state->sp->reset(clock->get_frame_start_c14M());
        }

        clock->set_clock_mode(computer->speed_new);
        
        display_update_video_scanner(ds, cpu);
        // cpu->video_scanner might be null here.
        int x = ds->video_scanner->get_frame_scan()->get_count();
        if (x > 0) {
            printf("Video scanner has %d samples @ speed shift [%d,%d]\n", x, ds->video_scanner->hcount, ds->video_scanner->get_vcount());
        }
    }

    if (computer->execution_mode == EXEC_STEP_INTO) {

        /* This will run about 60fps, primarily waiting on user input in the debugger window. */
        while (computer->instructions_left) {
            if (computer->event_timer->isEventPassed(clock->get_c14m())) {
                computer->event_timer->processEvents(clock->get_c14m());
            }
            if (computer->vid_event_timer->isEventPassed(clock->get_vid_cycles())) {
                computer->vid_event_timer->processEvents(clock->get_vid_cycles());
            }
            (cpu->cpun->execute_next)(cpu);
            computer->instructions_left--;
        }

        MEASURE(computer->event_times, frame_event(computer, cpu));

        /* Emit Audio Frame */
        // disable audio in step mode.
        
        /* Process Internal Event Queue */
        MEASURE(computer->app_event_times, frame_appevent(computer, cpu));

        /* Execute Device Frames - 60 fps */
        MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());

        /* Emit Video Frame */
        // set flag to force full frame draw instead of cycle based draw.
        MEASURE(computer->display_times, frame_video_update(computer, cpu, true));

        // if we're in stepwise mode, we should increment these only if we got to end of frame.
        if (clock->get_c14m() >= clock->get_frame_end_c14M()) {
            if (clock->get_video_scanner() != nullptr) {
                computer->video_system->update_display(false); // set flag to false to draw with cycle based, and, gobble up frame data.
            }

            // update frame counters.
            clock->next_frame();
            // set next frame cycle time (used for mouse) is at top of frame.
            computer->set_frame_start_cycle();
        }

        // sleep for 1/60th second ish, without updating frame counts etc.
        uint64_t wakeup_time = computer->last_cycle_time + 16667000;
        SDL_DelayPrecise(wakeup_time - SDL_GetTicksNS());
        
    } else if ((computer->execution_mode == EXEC_NORMAL) && (clock->get_clock_mode() != CLOCK_FREE_RUN)) {

        computer->set_frame_start_cycle();

        if (computer->debug_window->window_open) {
            while (clock->get_c14m() < clock->get_frame_end_c14M()) { // 1/60th second.
                if (computer->event_timer->isEventPassed(clock->get_c14m())) {
                    computer->event_timer->processEvents(clock->get_c14m());
                }
                if (computer->vid_event_timer->isEventPassed(clock->get_vid_cycles())) {
                    computer->vid_event_timer->processEvents(clock->get_vid_cycles());
                }
                // do the pre check.
                if (computer->debug_window->check_pre_breakpoint(cpu)) {
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }

                (cpu->cpun->execute_next)(cpu);
                
                // Do the post check.
                if (computer->debug_window->check_post_breakpoint(&cpu->trace_entry)) {
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }
                if (cpu->trace_entry.opcode == 0x00) { // catch a BRK and stop execution.
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }
            
            }
        } else { // skip all debug checks if debug window is not open - this may seem repetitious but it saves all kinds of cycles where every cycle counts 
            while (clock->get_c14m() < clock->get_frame_end_c14M()) {
                if (computer->event_timer->isEventPassed(clock->get_c14m())) {
                    computer->event_timer->processEvents(clock->get_c14m());
                }
                if (computer->vid_event_timer->isEventPassed(clock->get_vid_cycles())) {
                    computer->vid_event_timer->processEvents(clock->get_vid_cycles());
                }
                (cpu->cpun->execute_next)(cpu);
            }
        }

        /* Process Events */
        MEASURE(computer->event_times, frame_event(computer, cpu));

        /* Process Internal Event Queue */
        MEASURE(computer->app_event_times, frame_appevent(computer, cpu));

        /* Execute Device Frames - 60 fps */
        MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());

        /* Emit Video Frame */
        if (computer->execution_mode != EXEC_STEP_INTO) {
            MEASURE(computer->display_times, frame_video_update(computer, cpu));
        }
        
        // calculate what sleep-until time should be.
        uint64_t frame_length_ns = (computer->frame_count & 1) ? clock->get_us_per_frame_odd() : clock->get_us_per_frame_even();
        
        // update frame status; calculate stats; move these variables into computer;
        computer->frame_status_update();

        // if we completed a full frame, update the frame counters. otherwise we were interrupted by breakpoint etc 
        if (clock->get_c14m() >= clock->get_frame_end_c14M()) {
            clock->next_frame();

            computer->last_start_frame_c14m = clock->get_frame_start_c14M();
        }

        uint64_t time_to_sleep = frame_length_ns - (SDL_GetTicksNS() - computer->last_cycle_time);
        computer->set_idle_percent(((float)time_to_sleep / (float)frame_length_ns) * 100.0f);

        frame_sleep(computer, computer->last_cycle_time, frame_length_ns);
        computer->last_cycle_time = SDL_GetTicksNS(); 

    } else { // Ludicrous Speed!
        // TODO: how to handle VBL timing here. estimate it based on realtime?
        computer->set_frame_start_cycle(); // todo: unsure if this is right..
        uint64_t frame_length_ns = (computer->frame_count & 1) ? clock->get_us_per_frame_odd() : clock->get_us_per_frame_even();
        uint64_t next_frame_time = computer->last_cycle_time + frame_length_ns;

        computer->last_start_frame_c14m = clock->get_frame_start_c14M();
        
        if (computer->debug_window->window_open) {
            while (SDL_GetTicksNS() < next_frame_time) { // run emulated frame, but of course we don't sleep in this loop so we'll Go Fast.
                if (computer->event_timer->isEventPassed(clock->get_c14m())) {
                    computer->event_timer->processEvents(clock->get_c14m());
                }
                if (computer->vid_event_timer->isEventPassed(clock->get_vid_cycles())) {
                    computer->vid_event_timer->processEvents(clock->get_vid_cycles());
                }
                if (computer->debug_window->check_pre_breakpoint(cpu)) {
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }

                (cpu->cpun->execute_next)(cpu);
                
                if (computer->debug_window->check_post_breakpoint(&cpu->trace_entry)) {
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }
                if (cpu->trace_entry.opcode == 0x00) { // catch a BRK and stop execution.
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    break;
                }
            
            }
        } else { // skip all debug checks if debug window is not open - this may seem repetitious but it saves all kinds of cycles where every cycle counts (GO FAST MODE)
            while (SDL_GetTicksNS() < next_frame_time) { // run emulated frame, but of course we don't sleep in this loop so we'll Go Fast.
                if (computer->event_timer->isEventPassed(clock->get_c14m())) {
                    computer->event_timer->processEvents(clock->get_c14m());
                }
                if (computer->vid_event_timer->isEventPassed(clock->get_vid_cycles())) {
                    computer->vid_event_timer->processEvents(clock->get_vid_cycles());
                }
                (cpu->cpun->execute_next)(cpu);
            }
        }

        // this was roughly one video frame so let's pretend we went that many.
        clock->adjust_c14m(c14M_per_frame);

            /* Process Events */
            MEASURE(computer->event_times, frame_event(computer, cpu));
    
            /* Emit Audio Frame */
            // TODO: reevaluate disable audio output in ludicrous speed.

            /* Process Internal Event Queue */
            MEASURE(computer->app_event_times, frame_appevent(computer, cpu));
    
            /* Execute Device Frames - 60 fps */
            MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());
    
            /* Emit Video Frame */
    
            MEASURE(computer->display_times, frame_video_update(computer, cpu));
    

        // update frame window counters.
        // this gets wildly out of sync because we're not actually executing this many cycles in the loop,
        // because we are basing loop on time. So, maybe loop should be based on cycles per below after all,
        // while just periodically doing the frame update stuff here.
        computer->last_cycle_time = SDL_GetTicksNS(); 
        
        // update frame status; calculate stats; move these variables into computer;
        computer->frame_status_update();

        clock->next_frame(); // TODO: now redundant to above.
        computer->last_start_frame_c14m = clock->get_frame_start_c14M();
    }

    return true;
}

/* ========================================================================
   App State and Phase Machine for SDL3 App Callbacks
   ======================================================================== */

gs2_app_t gs2_app_values;

enum AppPhase {
    PHASE_SYSTEM_SELECT,
    PHASE_EMULATION,
    PHASE_SHUTTING_DOWN,
};

/* This is "application state" as passed by SDL into the various AppCallbacks routines */
struct GS2AppState {
    AppPhase phase = PHASE_SYSTEM_SELECT;

    // Parsed from command line (persistent across system-select cycles)
    int platform_id = PLATFORM_APPLE_II_PLUS;
    std::vector<disk_mount_t> disks_to_mount;

    // System selection
    SelectSystem *select_system = nullptr;
    AssetAtlas_t *aa = nullptr;

    // Emulation state
    computer_t *computer = nullptr;

    // MMU pointers tracked for cleanup
    MMU_II *mmu_ii = nullptr;
    MMU_IIe *mmu_iie = nullptr;
    MMU_IIgs *mmu_iigs = nullptr;
};

/*
 * Configure the selected system and transition from system-select to emulation.
 * This is the code that was between select_system->select() and run_cpus() in old main().
 */
void transition_to_emulation(GS2AppState *state, int system_id) {
    computer_t *computer = state->computer;
    video_system_t *vs = computer->video_system;

    // Emulation manages its own timing, so turn off vsync.
    SDL_SetRenderVSync(vs->renderer, 0);

    SystemConfig_t *system_config = get_system_config(system_id);
    state->platform_id = system_config->platform_id;

    platform_info* platform = get_platform(state->platform_id);
    print_platform_info(platform);

    // TODO: This is a little disjointed. the clock abstraction should probably program all the things that need the clock.
    // the initial setting here is 1MHz, except for platform which has the right starting clock?
    //select_system_clock(system_config->clock_set);
    //computer->set_clock(&system_clock_mode_info[computer->speed_new]);
    //set_clock_mode(computer->cpu, platform->default_clock_mode);

    computer->cpu->set_processor(platform->cpu_type);
    // important to do this before setting up the rest of the computer.
    NClockII *nclock = NClockFactory::create_clock(platform->id, system_config->clock_set);
    computer->set_clock(nclock);
    gs2_app_values.clock = nclock;

    //computer->set_cpu(new cpu_state(platform->cpu_type));

    computer->set_platform(platform);
    computer->set_video_scanner(system_config->scanner_type);
    computer->set_system_id(system_id);
    
    // TODO: load platform roms - this info should get stored in the 'computer'
    rom_data *rd = load_platform_roms(platform);
    if (!rd) {
        system_failure("Failed to load platform roms, exiting.");
        return;
    }

    // we will ALWAYS have a 256 page map. because it's a 6502 and all is addressible in a II.
    // II can have 4k, 8k, 12k; or 16k, 32k, 48k.
    // II Plus can have 16k, 32K, or 48k RAM. 16K more BUT IN THE LANGUAGE CARD MODULE.
    // always 12k rom, but not necessarily always the same ROM.
    state->mmu_ii = nullptr;
    state->mmu_iie = nullptr;
    state->mmu_iigs = nullptr;

    switch (platform->mmu_type) {
        case MMU_MMU_II:
            state->mmu_ii = new MMU_II(256, 48*1024, (uint8_t *) rd->main_rom_data);
            computer->cpu->set_mmu(state->mmu_ii);
            computer->set_mmu(state->mmu_ii);
            computer->debug_window->set_mmu(state->mmu_ii);
            break;
        case MMU_MMU_IIE:
            state->mmu_iie = new MMU_IIe(256, 128*1024, (uint8_t *) rd->main_rom_data);
            computer->cpu->set_mmu(state->mmu_iie);
            computer->set_mmu(state->mmu_iie);
            computer->debug_window->set_mmu(state->mmu_iie);
            break;
        case MMU_MMU_IIGS:
            state->mmu_iie = new MMU_IIe(256, 128*1024, /* (uint8_t *) */rd->main_rom_data + 0x1'C000);
            state->mmu_iigs = new MMU_IIgs(256, 8*1024*1024, 128*1024, /* (uint8_t *) */rd->main_rom_data, state->mmu_iie);
            state->mmu_iigs->init_map();
            computer->cpu->set_mmu(state->mmu_iigs); // cpu gets FPI
            computer->set_mmu(state->mmu_iie); // everything else gets the Mega II
            computer->debug_window->set_mmu(state->mmu_iigs);
            state->mmu_iigs->set_clock((NClockII *)nclock);

            break;
        default:
            printf("Unknown MMU type: %d\n", platform->mmu_type);
            break;
    }
    // need to tell the MMU about our ROM somehow.
    // need a function in MMU to "reset page to default".
    computer->cpu->cpun = createCPU(platform->cpu_type, (NClock *)nclock);

    computer->cpu->core = computer->cpu->cpun.get(); // set the core. Probably need a better set cpu for cpu_state.
    //computer->cpu->core->set_clock(nclock);

    // Initialize the slot manager.
    //SlotManager_t *slot_manager = new SlotManager_t();

    // Create Mounts object
    //computer->mounts = new Mounts(computer->cpu, computer->slot_manager);

    //init_display_font(rd);

    //SystemConfig_t *system_config = get_system_config(platform_id);

    for (int i = 0; system_config->device_map[i].id != DEVICE_ID_END; i++) {
        DeviceMap_t dm = system_config->device_map[i];

        Device_t *device = get_device(dm.id);
        if (device->power_on == nullptr) {
            printf("Device has no poweron, not found: %d", dm.id);
            continue;
        } 
        device->power_on(computer, dm.slot);
        if (dm.slot != SLOT_NONE) {
            computer->slot_manager->register_slot(device, dm.slot);
        }
    }

    register_clock_debug(computer);

    computer->cpu->reset();

    // mount disks - AFTER device init.
    for (const auto& disk_mount : state->disks_to_mount) {
        computer->mounts->mount_media(disk_mount);
    }

    osd = new OSD(computer, vs->renderer, vs->window, computer->slot_manager, 1120, 768, state->aa);
    osd->set_clock(computer->clock);

    // TODO: this should be handled differently. have osd save/restore?
    int error = SDL_SetRenderTarget(vs->renderer, nullptr);
    /* if (!error) {
        fprintf(stderr, "Error setting render target: %s\n", SDL_GetError());
        return(1);
    } */
    computer->video_system->set_window_title(system_config->name);
    
    computer->video_system->update_display(); // check for events 60 times per second.

    if (platform->mmu_type == MMU_MMU_IIGS) {
        //mmu_iigs->set_cpu(computer->cpu); // not needed any more, clock handles it.
        
        //computer->debug_window->set_open();
        //computer->cpu->execution_mode = EXEC_STEP_INTO;
        
        computer->register_debug_display_handler(
            "mmugs",
            DH_MMUGS, // unique ID for this, need to have in a header.
            [state]() -> DebugFormatter * {
                return debug_mmu_iigs(state->mmu_iigs);
            }
        );


        computer->cpu->trace_buffer->set_cpu_type(PROCESSOR_65816);
        computer->video_system->set_display_engine(DM_ENGINE_RGB);

        computer->register_reset_handler([state]() {
            state->mmu_iigs->reset();
            return true;
        });
    }

    run_cpus_init(computer);
    state->phase = PHASE_EMULATION;
}

/*
 * Clean up emulation state and transition to system select or exit.
 */
void transition_to_shutdown(GS2AppState *state) {
    computer_t *computer = state->computer;

    // save cpu trace buffer, then exit.
    // TODO: move this to the trace buffer destructor.
    std::string tracepath;
    Paths::calc_docs(tracepath, "trace.bin");
    computer->cpu->trace_buffer->save_to_file(tracepath);

    // deallocate stuff.
    delete osd;
    osd = nullptr;

    platform_info *platform = computer->platform;
    delete computer;
    state->computer = nullptr;

    switch (platform->mmu_type) {
        case MMU_MMU_II:
            delete state->mmu_ii;
            break;
        case MMU_MMU_IIE:
            delete state->mmu_iie;
            break;
        case MMU_MMU_IIGS:
            delete state->mmu_iigs;
            delete state->mmu_iie;
            break;
    }
    state->mmu_ii = nullptr;
    state->mmu_iie = nullptr;
    state->mmu_iigs = nullptr;

    delete state->select_system;
    state->select_system = nullptr;

    // AssetAtlas holds textures tied to the old renderer — must delete before creating new computer
    delete state->aa;
    state->aa = nullptr;

    // Create fresh computer and select system for next cycle
    state->computer = new computer_t(nullptr);
    video_system_t *vs = state->computer->video_system;
    gs2_app_values.video_system = vs;

    initMenu(vs->window);

    // Recreate AssetAtlas with the new renderer
    state->aa = new AssetAtlas_t(vs->renderer, "img/atlas.png");
    state->aa->set_elements(MainAtlas_count, asset_rects);

    state->select_system = new SelectSystem(vs, state->aa);

    // Let vsync throttle the selection UI instead of spinning.
    SDL_SetRenderVSync(vs->renderer, 1);
    state->phase = PHASE_SYSTEM_SELECT;
}

/* ========================================================================
   SDL3 App Callback Entry Points
   ======================================================================== */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    std::cout << "Booting GSSquared!" << std::endl;

    SDL_SetAppMetadata("GSSquared", VERSION_STRING, "Copyright 2025-2026 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Copyright 2025-2026 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/jawaidbazyar/gssquared");

    GS2AppState *state = new GS2AppState();
    
    int platform_id = PLATFORM_APPLE_II_PLUS;  // default to Apple II Plus
    int opt;
    int slot, drive;

    if (isatty(fileno(stdin))) {
        gs2_app_values.console_mode = true;
    }
    Paths::initialize(gs2_app_values.console_mode);

    gs2_app_values.base_path = get_base_path(gs2_app_values.console_mode);
    printf("base_path: %s\n", gs2_app_values.base_path.c_str());
    gs2_app_values.pref_path = get_pref_path();
    printf("pref_path: %s\n", gs2_app_values.pref_path.c_str());

    if (gs2_app_values.console_mode) {
        // parse command line optionss
        while ((opt = getopt(argc, argv, "sxp:d:")) != -1) {
            switch (opt) {
                case 'p':
                    platform_id = std::stoi(optarg);
                    break;
                case 'd':
                    {
                        std::string filename;
                        std::string arg_str(optarg);
                        // Using regex for better parsing
                        std::regex disk_pattern("s([0-9]+)d([0-9]+)=(.+)");
                        std::smatch matches;
                    
                        if (std::regex_match(arg_str, matches, disk_pattern) && matches.size() == 4) {
                            slot = std::stoi(matches[1]);
                            drive = std::stoi(matches[2]) - 1;
                            filename = matches[3];
                            //std::cout << std::format("Mounting disk {} in slot {} drive {}\n", filename, slot, drive) << std::endl;
                            std::cout << "Mounting disk " << filename << " in slot " << slot << " drive " << drive << std::endl;
                            state->disks_to_mount.push_back({ (uint16_t)slot, (uint16_t)drive, filename});
                        }
                    }
                    break;
                /* case 'x':
                    gs2_app_values.disk_accelerator = true;
                    break; */
                case 's':
                    gs2_app_values.sleep_mode = true;
                    break;
                default:
                    std::cerr << "Usage: " << argv[0] << " [-p platform] [-dsXdX=filename] [-x] [-s] \n";
                    std::cerr << "  -s: sleep mode (don't busy-wait, sleep)\n";
                    std::cerr << "  -x: disk accelerator (speed up CPU when disk II drive is active)\n";
                    return SDL_APP_FAILURE;
            }
        }
    }

    gs2_app_values.menu_event_type = SDL_RegisterEvents(1);

    state->platform_id = platform_id;

    // Debug print mounted media
    std::cout << "Mounted Media (" << state->disks_to_mount.size() << " disks):" << std::endl;
    for (const auto& disk_mount : state->disks_to_mount) {
        std::cout << " Slot " << disk_mount.slot << " Drive " << disk_mount.drive << " - " << disk_mount.filename << std::endl;
    }

    state->computer = new computer_t(nullptr); // We'll set the clock later.

    video_system_t *vs = state->computer->video_system;
    gs2_app_values.video_system = vs;

    initMenu(vs->window);

    state->aa = new AssetAtlas_t(vs->renderer, "img/atlas.png");
    state->aa->set_elements(MainAtlas_count, asset_rects);

    state->select_system = new SelectSystem(vs, state->aa);

    // Let vsync throttle the selection UI instead of spinning.
    SDL_SetRenderVSync(vs->renderer, 1);
    state->phase = PHASE_SYSTEM_SELECT;

    *appstate = state;

    // Register callback so emulation continues during macOS menu tracking and window resize
    setMenuTrackingCallback(SDL_AppIterate, state);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    GS2AppState *state = (GS2AppState *)appstate;

    if (state->phase == PHASE_SYSTEM_SELECT) {
        state->select_system->event(*event);
        if (event->type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS; // clean exit
        }
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EMULATION) {
        computer_t *computer = state->computer;
        cpu_state *cpu = computer->cpu;

        handle_single_event(computer, cpu, *event);

        if (event->type == SDL_EVENT_QUIT) {
            cpu->halt = HLT_USER;
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    GS2AppState *state = (GS2AppState *)appstate;

    if (state->phase == PHASE_SYSTEM_SELECT) {
        /* Render the selection UI (one frame). Events already dispatched by SDL_AppEvent. */
        video_system_t *vs = state->computer->video_system;
        SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
        vs->clear();
        if (state->select_system->update()) {
            state->select_system->render();
            vs->present();
        }

        int system_id = state->select_system->get_selected_system();
        if (system_id == SELECT_QUIT) {
            return SDL_APP_SUCCESS; // user closed window during selection
        }
        if (system_id >= 0) {
            transition_to_emulation(state, system_id);
        }
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EMULATION) {
        computer_t *computer = state->computer;

        osd->update();

        if (!run_one_frame(computer)) {
            // User requested halt, transition back to system select
            transition_to_shutdown(state);
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    GS2AppState *state = (GS2AppState *)appstate;
    if (!state) return;

    if (osd) {
        delete osd;
        osd = nullptr;
    }
    if (state->computer) {
        delete state->computer;
        state->computer = nullptr;
    }

    // Clean up MMUs if they exist (e.g., quit during emulation)
    delete state->mmu_ii;
    delete state->mmu_iie;
    delete state->mmu_iigs;

    delete state->select_system;
    delete state->aa;
    delete state;
    // SDL_Quit() is called automatically by SDL after this returns.
}

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
#include <memory>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "gs2.hpp"
#include "platform-specific/menu.h"
#include "Module_ID.hpp"
#include "paths.hpp"
#include "FileAssociations.hpp"
#include "cpu.hpp"
#include "display/display.hpp"
#include "devices/speaker/speaker.hpp"
#include "platforms.hpp"
#include "util/dialog.hpp"
#include "util/BlankDisk.hpp"
#include "util/mount.hpp"
#include "util/Connections.hpp"
#include "util/SystemConfig.hpp"
#include "util/SystemSettings.hpp"
#include "ui/OSD.hpp"
#if defined(__EMSCRIPTEN__)
#include "platform-specific/emscripten/web_file_dialog.hpp"
#include <emscripten.h>
#elif defined(__APPLE__)
#include "platform-specific/macos/gs2_save_dialog.hpp"
#endif
#include "systemconfig.hpp"
#include "slots.hpp"
#include "videosystem.hpp"
#include "debugger/debugwindow.hpp"
#include "debugger/DebugProtocolServer.hpp"
#include "debugger/BreakpointTable.hpp"
#include "computer.hpp"
#include "mmus/mmu_ii.hpp"
#include "mmus/mmu_iie.hpp"
#include "mmus/mmu_iigs.hpp"
#include "mmus/iigs_memory.hpp"
#include "ui/SelectSystem.hpp"
#include "ui/EditSystem.hpp"
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

// Defined in OSD.cpp — used here where osd is accessible for menu-triggered disk toggle
void handle_disk_toggle(computer_t *computer, OSD *osd, storage_key_t key);

void handle_single_event(computer_t *computer, cpu_state *cpu, SDL_Event &event) {
    // Handle disk toggle from menu directly here where osd is accessible
    if (event.type == gs2_app_values.menu_event_type && event.user.code == MENU_DISK_TOGGLE) {
        storage_key_t key((uint64_t)(uintptr_t)event.user.data1);
        handle_disk_toggle(computer, osd, key);
        return;
    }
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
            case EVENT_QUIT:
                computer->cpu->halt = HLT_USER;
                break;
            /* case EVENT_MODAL_SHOW:
                osd->show_diskii_modal(event->getEventKey(), event->getEventData());
                break; */
            /* case EVENT_MODAL_CLICK:
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
                break; */
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
void frame_video_update(computer_t *computer, bool force_full_frame = false) {
    video_system_t *vs = computer->video_system;
    display_state_t *ds = (display_state_t *)computer->cached_display_state;

    // Step-into does not run a full frame of scanner ticks during CPU execution.
    // Advance one video frame here so VBL / QTR / scanline IRQs still fire even if a
    // higher-weight processor (Second Sight, Videx) skips Apple II rendering.
    // Ludicrous (N×14M) keeps the scanner hooked and ticks it during execute.
    if (force_full_frame && ds && ds->video_scanner) {
        const int n = (int)computer->clock->get_vid_cycles_per_frame();
        for (int i = 0; i < n; i++) {
            ds->video_scanner->video_cycle();
        }
    }

    vs->update_display(force_full_frame);

    // If the CRT post-process shader is active, composite the offscreen scene
    // onto the swapchain (through the shader) before the OSD is drawn on top.
    vs->present_scene();

    // The OSD and its modals/HUD are authored in window-point coordinates (the
    // OSD lays out against SDL_GetWindowSize, which returns points). On a high-DPI
    // backbuffer the renderer output is in real pixels, so render the UI through a
    // points-sized logical presentation to keep its layout correct, then restore
    // the emulator's "draw in real output pixels" convention (DISABLED).
    //
    // The logical size MUST match the point coordinate space the OSD uses; scaling
    // it by the display scale here would desync the layout and make the UI render
    // oversized (the STRETCH already maps points onto the full pixel backbuffer).
    int points_w = 0, points_h = 0;
    SDL_GetWindowSize(vs->window, &points_w, &points_h);
    SDL_SetRenderLogicalPresentation(vs->renderer, points_w, points_h,
        SDL_LOGICAL_PRESENTATION_STRETCH);
    osd->render();
    SDL_SetRenderLogicalPresentation(vs->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    computer->debug_window->render();
    vs->present();
}

#if defined(__EMSCRIPTEN__)
// Keep SDL_AppIterate on requestAnimationFrame.
//
// SDL_SetRenderVSync(1) before the emscripten main loop exists fails the
// swap-interval check and SDL arms a software vsync at a hardcoded 60 Hz
// (the emscripten video driver never reports a refresh rate). After a
// close/reopen the main loop is already RAF, that SetVSync succeeds, the
// software cap stays off, and the guest runs at the panel rate — 75 Hz on
// a 48–75 FreeSync display. Do not use SDL_SetRenderVSync on the web.
//
// PumpEvents can also apply a deferred swap interval of 0, which switches
// the loop to setTimeout(0). Re-assert RAF every iterate, after that pump.
static void gs2_web_lock_raf() {
    emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
}

// The next guest frame cannot start until the browser calls us back, which is
// after we return. Spinning all the way to the deadline stacks that delay on
// top of the frame, and the slower wasm work then slips. Return this much
// early so the callback lands on the deadline. Adapted from how late the
// previous wake actually was.
static uint64_t gs2_web_wake_lead_ns = 2'000'000;
static uint64_t gs2_web_target_start_ns = 0;

static void gs2_web_begin_frame(computer_t *computer) {
    (void)computer;
    uint64_t now = SDL_GetTicksNS();
    if (gs2_web_target_start_ns == 0) {
        return;
    }
    int64_t late = (int64_t)now - (int64_t)gs2_web_target_start_ns;
    int64_t lead = (int64_t)gs2_web_wake_lead_ns + late / 2;
    if (lead < 0) lead = 0;
    if (lead > 8'000'000) lead = 8'000'000;
    gs2_web_wake_lead_ns = (uint64_t)lead;
    // The browser often calls back before the guest deadline. Starting the
    // frame then, and rebasing last_cycle_time to that early wake, shortens
    // every frame (measured 60.5 fps). Hold until the stamped boundary.
    if (now < gs2_web_target_start_ns) {
        while (SDL_GetTicksNS() < gs2_web_target_start_ns) {
        }
    }
}
#endif

void frame_sleep(computer_t *computer, uint64_t last_cycle_time, uint64_t ns_per_frame)
    /* uint64_t frame_count) */ {
    computer->frame_slipped = false;
    if (gs2_app_values.modal_tracking) return;

    uint64_t wakeup_time = last_cycle_time + ns_per_frame; /*  + (frame_count & 1); */ // even frames have 16688154, odd frames have 16688154 + 1

    // sleep out the rest of this frame.
    uint64_t sleep_loops = 0;
    uint64_t current_time = SDL_GetTicksNS();
    if (current_time > wakeup_time) {
        computer->clock_slip++;
        computer->frame_slipped = true;
#if defined(__EMSCRIPTEN__)
        gs2_web_target_start_ns = 0;
#endif
        // TODO: log clock slip for later display.
        //printf("Clock slip: event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
    } else {
#if defined(__EMSCRIPTEN__)
        // No Asyncify, so this cannot yield. Return early by the lead so the
        // browser can deliver the next callback at the guest deadline.
        // gs2_web_begin_frame() holds if that callback arrives early; that
        // hold is what keeps the rate at 59.9227 Hz instead of creeping up.
        uint64_t lead = gs2_web_wake_lead_ns;
        if (lead > ns_per_frame / 2) lead = ns_per_frame / 2;
        uint64_t return_at = wakeup_time - lead;
        if (current_time < return_at) {
            while (SDL_GetTicksNS() < return_at) {
                sleep_loops++;
            }
        }
        gs2_web_target_start_ns = wakeup_time;
#else
        if (gs2_app_values.sleep_mode) { // sleep most of it, but more aggressively sneak up on target than SDL_DelayPrecise does itself
            SDL_DelayPrecise((wakeup_time - SDL_GetTicksNS())*0.95);
        }
        // busy wait sync cycle time
        do {
            sleep_loops++;
        } while (SDL_GetTicksNS() < wakeup_time);
#endif
    }
}

#if 0
DebugFormatter *debug_clock(computer_t *computer) {
    DebugFormatter *f = new DebugFormatter();
    f->addLine("Clock Mode: %s", computer->clock->get_clock_mode_name(computer->clock->get_clock_mode()));
    f->addLine("CPU Slow Mode: %d", computer->clock->get_slow_mode());
    f->addLine("CPU Expected Rate: %d", computer->clock->get_hz_rate());
    f->addLine("CPU eMHZ: %12.8f, FPS: %12.8f", computer->e_mhz, computer->fps);
    f->addLine("CPU Cycle: %12llu", computer->clock->get_cycles());
    f->addLine("Vid Cycle: %12llu", computer->clock->get_vid_cycles());
    f->addLine("14M Cycle: %12llu", computer->clock->get_c14m());

    return f;
}
#endif

void register_clock_debug(computer_t *computer) {

    computer->register_debug_display_handler(
        "clock",
        DH_CLOCK, // unique ID for this, need to have in a header.
        [computer]() -> DebugFormatter * {
            DebugFormatter *f = computer->clock->debug();
            if (computer->clock->get_clock_mode() == CLOCK_FREE_RUN) {
                const char *cal = "idle";
                switch (computer->ludicrous_cal_state) {
                    case LS_CAL_PROBE: cal = "probe"; break;
                    case LS_CAL_DROPPING: cal = "calibrating"; break;
                    case LS_CAL_LOCKED: cal = "locked"; break;
                    default: break;
                }
                f->addLine("Ludicrous Cal: %s", cal);
            }
            return f;
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
    display_state_t *ds = (display_state_t *)computer->cached_display_state;

    if (cpu->halt == HLT_USER) { // top of frame.
        return false;
    }

    if (computer->execution_mode == EXEC_PAUSED) {
        return true;
    }

    if (computer->speed_shift) {
        computer->speed_shift = false;

        int x = ds->video_scanner->get_frame_scan()->get_count();
        if (x > 100) {
            printf("Video scanner has %d samples @ speed shift [%d,%d]\n", x, ds->video_scanner->get_hcount(), ds->video_scanner->get_vcount());
        }

        clock->set_clock_mode(computer->speed_new);
        if (computer->speed_new == CLOCK_FREE_RUN) {
            computer->begin_ludicrous_calibration();
        }
        display_update_video_scanner(ds);
    }

    if (computer->execution_mode == EXEC_STEP_INTO) {

        /* This will run about 60fps, primarily waiting on user input in the debugger window. */
        const bool had_work = computer->instructions_left > 0;
        while (computer->instructions_left) {
            clock->process_due();
            (cpu->cpun->execute_next)(cpu);
            computer->instructions_left--;
        }
        if (had_work && computer->debug_protocol) {
            computer->debug_protocol->emit_stopped_step(computer);
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
        MEASURE(computer->display_times, frame_video_update(computer, true));

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
        // On the web, RAF paces the loop; blocking here would hang the tab.
#ifndef __EMSCRIPTEN__
        uint64_t wakeup_time = computer->last_cycle_time + 16667000;
        SDL_DelayPrecise(wakeup_time - SDL_GetTicksNS());
#endif
        
    } else if (computer->execution_mode == EXEC_NORMAL) {

#if defined(__EMSCRIPTEN__)
        gs2_web_begin_frame(computer);
#endif
        computer->set_frame_start_cycle();
        uint64_t frame_cpu_start = clock->get_cycles();

#ifndef LUDICROUS_BINARY_SEARCH_PROBE
        // One wall-timed probe frame: count CPU cycles with scanner on (N=1),
        // then pick N0 and enter drop/lock. Keeps 14M advancing so IRQs fire.
        if (clock->get_clock_mode() == CLOCK_FREE_RUN &&
            computer->ludicrous_cal_state == LS_CAL_PROBE) {
            uint64_t frame_length_ns = (computer->frame_count & 1)
                ? clock->get_us_per_frame_odd() : clock->get_us_per_frame_even();
            uint64_t deadline = computer->last_cycle_time + frame_length_ns;
            uint64_t cycles_start = clock->get_cycles();

            if (computer->debug_window->needs_breakpoint_checks()) {
                while (SDL_GetTicksNS() < deadline) {
                    clock->process_due();
                    StopHit hit{};
                    if (computer->debug_window->check_pre_breakpoint(cpu, &hit)) {
                        uint32_t prev = computer->execution_mode;
                        computer->execution_mode = EXEC_STEP_INTO;
                        computer->instructions_left = 0;
                        if (computer->debug_protocol) {
                            computer->debug_protocol->emit_stopped(computer, hit);
                            computer->debug_protocol->emit_run_state(EXEC_STEP_INTO, prev);
                        }
                        break;
                    }
                    uint32_t pc_before = cpu->full_pc;
                    (cpu->cpun->execute_next)(cpu);
                    if (computer->breakpoints) {
                        computer->breakpoints->on_instruction_retired(pc_before);
                    }
                    if (computer->debug_window->check_post_breakpoint(cpu, &cpu->trace_entry, &hit)) {
                        uint32_t prev = computer->execution_mode;
                        computer->execution_mode = EXEC_STEP_INTO;
                        computer->instructions_left = 0;
                        if (computer->debug_protocol) {
                            computer->debug_protocol->emit_stopped(computer, hit);
                            computer->debug_protocol->emit_run_state(EXEC_STEP_INTO, prev);
                        }
                        break;
                    }
                    if (cpu->trace_entry.opcode == 0x00) {
                        computer->execution_mode = EXEC_STEP_INTO;
                        computer->instructions_left = 0;
                        break;
                    }
                    // Keep frame counters aligned if we run past end during probe.
                    if (clock->get_c14m() >= clock->get_frame_end_c14M()) {
                        clock->next_frame();
                    }
                }
            } else {
                while (SDL_GetTicksNS() < deadline) {
                    clock->process_due();
                    (cpu->cpun->execute_next)(cpu);
                    if (clock->get_c14m() >= clock->get_frame_end_c14M()) {
                        clock->next_frame();
                    }
                }
            }

            uint64_t dc = clock->get_cycles() - cycles_start;
            uint64_t cpf = clock->get_cycles_per_frame();
            uint32_t n0 = (cpf > 0) ? (uint32_t)(dc / cpf) : LUDICROUS_CPU_PER_14M_MIN;
            if (n0 < LUDICROUS_CPU_PER_14M_MIN) n0 = LUDICROUS_CPU_PER_14M_MIN;
            if (n0 > LUDICROUS_CPU_PER_14M_MAX) n0 = LUDICROUS_CPU_PER_14M_MAX;
            clock->set_cpu_per_14m(n0);
            computer->ludicrous_cal_state = LS_CAL_DROPPING;
            computer->ludicrous_slip_streak = 0;
            computer->ludicrous_stable_frames = 0;
            fprintf(stdout, "Ludicrous probe: %llu cycles -> N=%u (%.1f MHz)\n",
                (unsigned long long)dc, n0, (n0 * 14.31818));

            /* Process Events */
            MEASURE(computer->event_times, frame_event(computer, cpu));

            /* Process Internal Event Queue */
            MEASURE(computer->app_event_times, frame_appevent(computer, cpu));

            /* Execute Device Frames - 60 fps */
            MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());

            /* Emit Video Frame */
            if (computer->execution_mode != EXEC_STEP_INTO) {
                MEASURE(computer->display_times, frame_video_update(computer));
            }
            computer->frame_status_update();
            computer->last_start_frame_c14m = clock->get_frame_start_c14M();
            computer->last_cycle_time = SDL_GetTicksNS();
            return true;
        }
#endif

        if (computer->debug_window->needs_breakpoint_checks()) {
            while (clock->get_c14m() < clock->get_frame_end_c14M()) { // 1/60th second.
                clock->process_due();
                StopHit hit{};
                if (computer->debug_window->check_pre_breakpoint(cpu, &hit)) {
                    uint32_t prev = computer->execution_mode;
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    if (computer->debug_protocol) {
                        computer->debug_protocol->emit_stopped(computer, hit);
                        computer->debug_protocol->emit_run_state(EXEC_STEP_INTO, prev);
                    }
                    break;
                }

                uint32_t pc_before = cpu->full_pc;
                (cpu->cpun->execute_next)(cpu);
                if (computer->breakpoints) {
                    computer->breakpoints->on_instruction_retired(pc_before);
                }

                if (computer->debug_window->check_post_breakpoint(cpu, &cpu->trace_entry, &hit)) {
                    uint32_t prev = computer->execution_mode;
                    computer->execution_mode = EXEC_STEP_INTO;
                    computer->instructions_left = 0;
                    if (computer->debug_protocol) {
                        computer->debug_protocol->emit_stopped(computer, hit);
                        computer->debug_protocol->emit_run_state(EXEC_STEP_INTO, prev);
                    }
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
                clock->process_due();
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
            MEASURE(computer->display_times, frame_video_update(computer));
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

        // Measure against the same baseline frame_sleep uses for its deadline, so
        // idle% and slip detection agree even when pre-loop work runs long.
        uint64_t frame_work_ns = SDL_GetTicksNS() - computer->last_cycle_time;
        uint64_t time_to_sleep =
            (frame_work_ns < frame_length_ns)
                ? frame_length_ns - frame_work_ns
                : 0;
        computer->set_idle_percent(
            ((float)time_to_sleep / (float)frame_length_ns) * 100.0f);

        frame_sleep(computer, computer->last_cycle_time, frame_length_ns);
        computer->update_ludicrous_calibration(
            gs2_app_values.modal_tracking,
            clock->get_cycles() - frame_cpu_start);
#if defined(__EMSCRIPTEN__)
        // Stamp the guest boundary, not the early return. The next callback
        // is the rest of the wait; stamping "now" would shrink every frame
        // by the lead and run fast.
        if (!computer->frame_slipped && gs2_web_target_start_ns != 0) {
            computer->last_cycle_time = gs2_web_target_start_ns;
        } else {
            computer->last_cycle_time = SDL_GetTicksNS();
        }
#else
        computer->last_cycle_time = SDL_GetTicksNS();
#endif 

    }

    return true;
}

/* ========================================================================
   App State and Phase Machine for SDL3 App Callbacks
   ======================================================================== */

gs2_app_t gs2_app_values;

enum AppPhase {
    PHASE_SYSTEM_SELECT,
    PHASE_EDIT_SYSTEM,
    PHASE_EMULATION,
    PHASE_SHUTTING_DOWN,
};

/* This is "application state" as passed by SDL into the various AppCallbacks routines */
struct GS2AppState {
    AppPhase phase = PHASE_SYSTEM_SELECT;

    // Parsed from command line (persistent across system-select cycles)
    int platform_id = PLATFORM_APPLE_II_PLUS;
    std::vector<disk_mount_t> disks_to_mount;
    std::unique_ptr<SystemConfig> loaded_config;
    /** Persist target for builtin IIgs launches (BRAM written back into a .gs2). */
    std::unique_ptr<SystemConfig> persist_config;

    // True when the user gave -p PLATFORM or a config file path and we skipped
    // the system selector at startup. In that mode, closing the emulator window
    // quits the app (rather than bouncing back to the selector) — the
    // user expressly asked for one specific machine and there's no
    // "back" to return to.
    bool auto_launched = false;

    // Finder / Open With of a .gs2 while emulating: confirm, then halt and
    // boot this path without exiting the process or flashing System Select.
    std::string pending_config_path;
    bool switch_after_halt = false;

    // System selection / config editor
    SelectSystem *select_system = nullptr;
    EditSystem *edit_system = nullptr;
    AssetAtlas_t *aa = nullptr;

    // Emulation state
    computer_t *computer = nullptr;

    // External debug protocol (optional; started via --debug / -D)
    std::unique_ptr<DebugProtocolServer> debug_protocol;

    // MMU pointers tracked for cleanup
    MMU_II *mmu_ii = nullptr;
    MMU_IIe *mmu_iie = nullptr;
    MMU_IIgs *mmu_iigs = nullptr;
};

void transition_to_emulation(GS2AppState *state, const SystemConfig_t *system_config, int builtin_system_id);

static bool apply_system_config_file(GS2AppState *state, const std::string& path, std::string& error_out) {
    state->loaded_config = std::make_unique<SystemConfig>();
    if (!state->loaded_config->load(path, error_out)) {
        state->loaded_config.reset();
        return false;
    }
    state->disks_to_mount = state->loaded_config->mounts();
    state->platform_id = state->loaded_config->config().platform_id;
    SystemSettings::instance().record_use(path);
    return true;
}

static bool is_launchable_config_path(const char *path) {
    if (path == nullptr) {
        return false;
    }
    const ConfigFileKind kind = detect_config_file_kind(path);
    return kind == ConfigFileKind::Gs2 || kind == ConfigFileKind::Settings;
}

static bool launch_config_and_emulate(GS2AppState *state, const std::string& path,
                                      bool document_session, std::string& error_out) {
    if (!apply_system_config_file(state, path, error_out)) {
        return false;
    }
    std::cout << "Auto-launching system config: " << path << std::endl;
    transition_to_emulation(state, &state->loaded_config->config(), -1);
    state->auto_launched = document_session;
    if (state->computer && state->computer->video_system) {
        state->computer->video_system->raise();
    }
    return true;
}

static void request_open_config(GS2AppState *state, const char *path) {
    if (!is_launchable_config_path(path)) {
        return;
    }
    if (state->phase == PHASE_SYSTEM_SELECT) {
        std::string error;
        if (!launch_config_and_emulate(state, path, true, error)) {
            std::string diag = "Failed to load system config '" + std::string(path) + "':\n" + error;
            std::cerr << diag << "\n";
            system_diag(diag.data());
        }
        return;
    }
    if (state->phase == PHASE_EDIT_SYSTEM) {
        // Do not discard an unsaved editor draft.
        return;
    }
    if (state->phase == PHASE_EMULATION) {
        if (osd == nullptr) {
            return;
        }
        std::string error;
        SystemConfig probe;
        if (!probe.load(path, error)) {
            std::string diag = "Failed to load system config '" + std::string(path) + "':\n" + error;
            std::cerr << diag << "\n";
            osd->set_heads_up_message("Failed to load config", 180);
            return;
        }
        const std::string path_copy(path);
        osd->prompt_launch_config(path_copy, [state, path_copy]() {
            state->pending_config_path = path_copy;
            state->switch_after_halt = true;
            if (state->computer && state->computer->cpu) {
                state->computer->cpu->halt = HLT_USER;
            }
        });
    }
}

struct open_config_dialog_data_t {
    GS2AppState *state;
    bool edit_mode = false;
};

static void transition_to_edit_system(GS2AppState *state) {
    delete state->edit_system;
    state->edit_system = new EditSystem(state->computer->video_system, state->aa);
    state->phase = PHASE_EDIT_SYSTEM;
}

static void leave_edit_system(GS2AppState *state) {
    delete state->edit_system;
    state->edit_system = nullptr;
    // Restore SelectSystem letterbox presentation.
    video_system_t *vs = state->computer->video_system;
    delete state->select_system;
    state->select_system = new SelectSystem(vs, state->aa);
    state->phase = PHASE_SYSTEM_SELECT;
}

static void system_config_dialog_callback(void *userdata, const char *const *filelist, int /*filter*/) {
    auto *data = static_cast<open_config_dialog_data_t *>(userdata);
    GS2AppState *state = data->state;
    bool edit_mode = data->edit_mode;
    delete data;

    if (!filelist || !filelist[0]) {
        return;
    }

    SystemSettings::instance().remember_file_dialog_selection(FileDialogKind::Config, filelist[0]);

    std::string error;
    if (!apply_system_config_file(state, filelist[0], error)) {
        std::string diag = "Failed to load system config '" + std::string(filelist[0]) + "':\n" + error;
        std::cerr << diag << "\n";
        system_diag(diag.data());
        return;
    }

    if (edit_mode) {
        transition_to_edit_system(state);
        state->edit_system->start_from_config(*state->loaded_config);
        return;
    }

    transition_to_emulation(state, &state->loaded_config->config(), -1);
}

static void open_system_config_dialog(GS2AppState *state, bool edit_mode = false) {
    static const SDL_DialogFileFilter filters[] = {
        { "GS2 System Config (.gs2)", "gs2" },
        { "Profiles Settings (.txt)", "txt" },
        { "All files", "*" }
    };

    auto *data = new open_config_dialog_data_t{ state, edit_mode };

#if defined(__EMSCRIPTEN__)
    web_open_file_dialog(system_config_dialog_callback, data, ".gs2");
#else
    const std::string last_path =
        SystemSettings::instance().get_file_dialog_default_location(FileDialogKind::Config);
    SDL_ShowOpenFileDialog(
        system_config_dialog_callback,
        data,
        state->computer->video_system->window,
        filters,
        sizeof(filters) / sizeof(SDL_DialogFileFilter),
        last_path.empty() ? nullptr : last_path.c_str(),
        false);
#endif
}

static bool blank_disk_type_from_menu(Sint32 code, BlankDiskType *out) {
    switch (code) {
        case MENU_FILE_NEW_DISK_525_UNFMT:
            *out = BlankDiskType::Floppy525Unformatted;
            return true;
        case MENU_FILE_NEW_DISK_525_DOS33:
            *out = BlankDiskType::Floppy525Dos33;
            return true;
        case MENU_FILE_NEW_DISK_525_PRODOS:
            *out = BlankDiskType::Floppy525Prodos;
            return true;
        case MENU_FILE_NEW_DISK_35_PRODOS:
            *out = BlankDiskType::Floppy35Prodos;
            return true;
        case MENU_FILE_NEW_DISK_32M_HD:
            *out = BlankDiskType::Hd32M;
            return true;
        case MENU_FILE_NEW_DISK_32M_PRODOS:
            *out = BlankDiskType::Hd32MProdos;
            return true;
        default:
            return false;
    }
}

static void show_blank_disk_error(const std::string& message) {
    std::cerr << message << "\n";
    std::string copy = message;
    system_diag(copy.data());
}

struct new_disk_dialog_data_t {
    BlankDiskType type;
    std::string default_path;
};

static void new_disk_dialog_callback(void *userdata, const char *const *filelist, int /*filter*/) {
    auto *data = static_cast<new_disk_dialog_data_t *>(userdata);
    BlankDiskType type = data->type;
    std::string path;
    if (filelist && filelist[0]) {
        path = filelist[0];
    }
    delete data;

    if (path.empty()) {
        return;
    }

    std::string err;
    if (!write_blank_disk(type, path, err)) {
        show_blank_disk_error(err);
        return;
    }
    SystemSettings::instance().remember_file_dialog_selection(
        FileDialogKind::Disk, blank_disk_ensure_extension(type, path));
}

static void begin_new_disk_image(GS2AppState *state, BlankDiskType type) {
    if (!state->computer || !state->computer->video_system) {
        show_blank_disk_error("Cannot create a disk image (no window).");
        return;
    }

    auto *cb_data = new new_disk_dialog_data_t{
        type,
        SystemSettings::instance().get_file_dialog_save_default_location(
            FileDialogKind::Disk, blank_disk_suggested_filename(type)),
    };

    const bool floppy = blank_disk_is_floppy(type);
    static const SDL_DialogFileFilter woz_filters[] = {
        {"WOZ images", "woz"},
    };
    static const SDL_DialogFileFilter hdv_filters[] = {
        {"Hard disk images", "hdv"},
    };
    const SDL_DialogFileFilter *filters = floppy ? woz_filters : hdv_filters;
    const int nfilters = 1;

#if defined(__EMSCRIPTEN__)
    const char *files[2] = { cb_data->default_path.c_str(), nullptr };
    new_disk_dialog_callback(cb_data, files, -1);
#elif defined(__APPLE__)
    (void)filters;
    (void)nfilters;
    const char *message = floppy
        ? "Choose a .woz file to overwrite, or enter a new name."
        : "Choose a .hdv file to overwrite, or enter a new name.";
    gs2_show_save_file_dialog(new_disk_dialog_callback, cb_data,
                              state->computer->video_system->window,
                              cb_data->default_path.c_str(),
                              "New Disk Image", message,
                              blank_disk_suggested_filename(type));
#else
    SDL_ShowSaveFileDialog(new_disk_dialog_callback, cb_data,
                           state->computer->video_system->window, filters, nfilters,
                           cb_data->default_path.c_str());
#endif
}

/*
 * Configure the selected system and transition from system-select to emulation.
 * This is the code that was between select_system->select() and run_cpus() in old main().
 */
void transition_to_emulation(GS2AppState *state, const SystemConfig_t *system_config, int builtin_system_id = -1) {
    computer_t *computer = state->computer;
    video_system_t *vs = computer->video_system;

    // The system selector renders through a LETTERBOX logical presentation; the
    // emulator draws in real output pixels, so clear it before emulation starts.
    SDL_SetRenderLogicalPresentation(vs->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    // Emulation manages its own timing, so turn off vsync.
    // On the web, frame_sleep holds the guest frame and gs2_web_lock_raf()
    // keeps requestAnimationFrame. SDL_SetRenderVSync(1) must not be used
    // there: it arms a one-shot 60 Hz software vsync that does not survive
    // close/reopen (the second renderer then runs at the panel refresh).
#ifndef __EMSCRIPTEN__
    SDL_SetRenderVSync(vs->renderer, 0);
#endif

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
    clock_mode_t boot_speed = system_config->clock_mode;
    if (boot_speed == INVALID_CLOCK_MODE) {
        boot_speed = platform->default_clock_mode;
    }
    NClockII *nclock = NClockFactory::create_clock(platform->id, system_config->clock_set, boot_speed);
    computer->set_clock(nclock);
    if (boot_speed == CLOCK_FREE_RUN) {
        computer->begin_ludicrous_calibration();
    }
    getMenuInterface()->setComputer(computer);

    //computer->set_cpu(new cpu_state(platform->cpu_type));

    computer->set_platform(platform);
    computer->set_video_scanner(system_config->scanner_type);
    state->persist_config.reset();

    auto wire_bram_persist = [computer](SystemConfig *cfg) {
        if (!cfg || cfg->is_settings_source() || cfg->path().empty()) {
            return;
        }
        if (cfg->import_legacy_bram()) {
            std::string err;
            if (!cfg->save(cfg->path(), err)) {
                std::cerr << "Failed to embed legacy BRAM in '" << cfg->path()
                          << "': " << err << std::endl;
            } else {
                std::cout << "Embedded legacy BRAM in " << cfg->path() << std::endl;
            }
        }
        if (cfg->has_bram()) {
            computer->set_initial_bram(cfg->bram_data(), 256);
            std::cout << "Using BRAM from " << cfg->path() << std::endl;
        }
        computer->set_config_path(cfg->path());
        computer->set_bram_persist_handler([cfg](const uint8_t *data, size_t len) {
            cfg->set_bram(data, len);
            std::string err;
            if (!cfg->save(cfg->path(), err)) {
                std::cerr << "Failed to persist config/BRAM to '" << cfg->path()
                          << "': " << err << std::endl;
            } else {
                std::cout << "Saved " << cfg->path() << std::endl;
            }
        });
    };

    if (state->loaded_config) {
        computer->set_system_id(-1);
        computer->set_system_config(&state->loaded_config->config());
        computer->set_machine_id(state->loaded_config->id());
        wire_bram_persist(state->loaded_config.get());
    } else {
        computer->set_system_id(builtin_system_id);
        computer->set_system_config(nullptr);
        computer->set_machine_id(system_config->id ? system_config->id : "");
        computer->set_config_path({});

        // The web build has no persistent user config directory (MEMFS starts
        // empty every load), so there is no saved BRAM to read back and nowhere
        // to save it. Skip the persist config entirely and let the machine boot
        // with default BRAM.
#if !defined(__EMSCRIPTEN__)
        if (system_config->id && system_config->id[0]) {
            state->persist_config = std::make_unique<SystemConfig>();
            const std::string persist_path =
                SystemConfig::user_config_path_for_id(system_config->id);
            std::string err;
            if (!state->persist_config->load(persist_path, err)) {
                const std::string found =
                    SystemConfig::find_user_config_path_for_id(system_config->id);
                if (found.empty() || !state->persist_config->load(found, err)) {
                    std::cerr << "Builtin config '" << persist_path
                              << "' not loaded (" << err << "); using tile defaults"
                              << std::endl;
                    state->persist_config->set_from_parts(*system_config,
                                                          state->disks_to_mount, {});
                    state->persist_config->set_path(persist_path);
                    state->persist_config->try_import_bram_from_gs2(persist_path);
                    if (!found.empty()) {
                        state->persist_config->try_import_bram_from_gs2(found);
                    }
                }
            }
            wire_bram_persist(state->persist_config.get());
        }
#endif
    }
    
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
        {
            // Bank $FF (the IIe-compatibility ROM the emulation-mode 6502
            // core runs, and the source for the LC-area/IOLC ROM overlay
            // reads inside MMU_IIgs) is always the *last* 64K bank of the
            // ROM image, wherever that falls for the actual ROM size --
            // NOT a hardcoded 128KB-ROM (ROM01) assumption. ROM01 is 128KB
            // (bank $FF at file offset 0x10000); ROM03 is 256KB (0x30000).
            // Previously hardcoded to 0x1'C000 / 128*1024, which silently
            // only worked for a 128KB ROM01 image and scrambled the bank
            // layout (and the emulation-mode reset vector) for ROM03.
            const size_t main_rom_size = rd->main_rom_file->size();
            const size_t rom_bank_ff_offset = main_rom_size - 65536;
            // Contiguous FPI RAM = motherboard base (ROM-dependent) + 8MB expansion
            // card, hard-capped at bank $7F (FPI decodes 23 RAM address bits).
            const size_t mobo_ram = iigs_memory::mobo_ram_bytes(main_rom_size);
            const size_t exp_ram = iigs_memory::kDefaultExpBytes;
            const size_t fast_ram = iigs_memory::fast_ram_bytes(main_rom_size, exp_ram);
            const uint32_t last_bank = iigs_memory::last_ram_bank(fast_ram);
            printf("IIgs RAM: %s mobo %zuKB + exp %zuMB = %zu bytes (banks $00–$%02X)\n",
                   iigs_memory::is_rom03(main_rom_size) ? "ROM03" : "ROM01",
                   mobo_ram / 1024,
                   exp_ram / (1024 * 1024),
                   fast_ram,
                   last_bank);
            state->mmu_iie = new MMU_IIe(256, 128*1024, /* (uint8_t *) */rd->main_rom_data + rom_bank_ff_offset + 0xC000);
            state->mmu_iigs = new MMU_IIgs(256, (int)fast_ram, (uint32_t) main_rom_size, /* (uint8_t *) */rd->main_rom_data, state->mmu_iie);
        }
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


    //init_display_font(rd);


    // Iterate through Platform Devices and create/register/initialize the devices.
    for (int i = 0; platform->mb_devices[i] != DEVICE_ID_END; i++) {
        Device_t *device = get_device(platform->mb_devices[i]);
        if (device->power_on == nullptr) {
            printf("Device has no poweron, not found: %d", platform->mb_devices[i]);
            continue;
        }
        device->power_on(computer, SLOT_NONE);
    }

    std::string slot_error;
    if (!validate_slot_devices(*system_config, slot_error)) {
        printf("Invalid slot configuration: %s\n", slot_error.c_str());
        system_failure(slot_error.c_str());
        return;
    }

    // Iterate through SystemConfig Slot Devices and create/register/initialize the devices.
    for (int i = 0; i < NUM_SLOTS; i++) {
        device_id id = system_config->slot_devices[i];
        if (id == DEVICE_ID_NONE) continue;

        Device_t *device = get_device(id);
        if (device->power_on == nullptr) {
            printf("Slot Device has no poweron handler: %d", id);
            continue;
        } 
        device->power_on(computer, (SlotType_t)i);

        computer->slot_manager->register_slot(device, (SlotType_t)i);
    }

    register_clock_debug(computer);

    computer->cpu->reset();

    // mount disks - AFTER device init.
    for (const auto& disk_mount : state->disks_to_mount) {
        computer->mounts->mount_media(disk_mount);
    }

    // Apply serial/parallel [[connections]] after ports register during device init.
    if (state->loaded_config) {
        for (const auto& conn : state->loaded_config->connections()) {
            const connection_key_t key = normalize_connection_key(conn.slot, conn.port);
            const connection_device_type_t dtype = connection_device_type_from_string(conn.device);
            if (!computer->connections->attach(key, dtype, conn.path)) {
                printf("Warning: connection slot %d device=%s not applied (port not registered?)\n",
                       key.slot, conn.device.c_str());
            }
        }
    }
    computer->connections->apply_defaults();

    osd = new OSD(computer, vs->renderer, vs->window, computer->slot_manager, 1120, 768, state->aa);

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

        computer->register_reset_handler([state](bool cold_start) {
            state->mmu_iigs->reset(cold_start);
            return true;
        });
    }

    if (system_config->display_monitor != DISPLAY_MONITOR_UNSET) {
        switch (system_config->display_monitor) {
            case DISPLAY_MONITOR_COMPOSITE:
                vs->set_display_engine(DM_ENGINE_NTSC);
                break;
            case DISPLAY_MONITOR_RGB:
                vs->set_display_engine(DM_ENGINE_RGB);
                break;
            case DISPLAY_MONITOR_GREEN:
                vs->set_display_engine(DM_ENGINE_MONO);
                vs->set_display_mono_color(DM_MONO_GREEN);
                break;
            case DISPLAY_MONITOR_AMBER:
                vs->set_display_engine(DM_ENGINE_MONO);
                vs->set_display_mono_color(DM_MONO_AMBER);
                break;
            case DISPLAY_MONITOR_WHITE:
                vs->set_display_engine(DM_ENGINE_MONO);
                vs->set_display_mono_color(DM_MONO_WHITE);
                break;
            default:
                break;
        }
    } else if (platform_is_iigs(system_config->platform_id)) {
        vs->set_display_engine(DM_ENGINE_RGB);
    }

    run_cpus_init(computer);
    vs->set_crt_shader_enabled(false, false);
    if (gs2_app_values.crt_shader_at_boot) {
        vs->set_crt_shader_enabled(true, true);
    }
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
    Paths::calc_docs(tracepath, "gssquared-trace.bin");
    computer->cpu->trace_buffer->save_to_file(tracepath);

    // deallocate stuff.
    delete osd;
    osd = nullptr;

    platform_info *platform = computer->platform;
    getMenuInterface()->setComputer(nullptr);
    computer->set_system_config(nullptr);
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

    initMenu(vs->window);

    // Recreate AssetAtlas with the new renderer
    state->aa = new AssetAtlas_t(vs->renderer, "img/atlas.png");
    state->aa->set_elements(MainAtlas_count, asset_rects);

    state->select_system = new SelectSystem(vs, state->aa);

    // Let vsync throttle the selection UI instead of spinning.
    // On the web, RAF does that (gs2_web_lock_raf in SDL_AppIterate).
    // SDL_SetRenderVSync(1) here would arm SDL's 60 Hz software fallback.
#ifndef __EMSCRIPTEN__
    SDL_SetRenderVSync(vs->renderer, 1);
#endif
    state->phase = PHASE_SYSTEM_SELECT;

    state->loaded_config.reset();
    state->persist_config.reset();
    state->disks_to_mount.clear();
    state->auto_launched = false;
}

/* ========================================================================
   SDL3 App Callback Entry Points
   ======================================================================== */

#if defined(__EMSCRIPTEN__)
static GS2AppState *g_web_gesture_state = nullptr;
static SDL_AppResult g_web_gesture_result = SDL_APP_CONTINUE;
static int g_web_gesture_depth = 0;
#endif

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    std::cout << "Booting GSSquared!" << std::endl;

    SDL_SetAppMetadata("GSSquared", VERSION_STRING, "Copyright 2025-2026 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Copyright 2025-2026 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/jawaidbazyar/gssquared");
    // for the scrollback in the debugger.
    SDL_SetHint(SDL_HINT_MAC_SCROLL_MOMENTUM, "1");

    GS2AppState *state = new GS2AppState();
    
    int platform_id = PLATFORM_APPLE_II_PLUS;  // default to Apple II Plus
    bool platform_explicit = false;            // true when -p was given on CLI
    std::string config_path;
    std::string debug_socket_path;
    std::vector<disk_mount_t> cli_mounts;
    int opt;
    int slot, drive;

    auto upsert_mount = [](std::vector<disk_mount_t>& mounts, const disk_mount_t& mount) {
        for (auto& existing : mounts) {
            if (existing.slot == mount.slot && existing.drive == mount.drive) {
                existing = mount;
                return;
            }
        }
        mounts.push_back(mount);
    };

    if (isatty(fileno(stdin)) || argc > 1) {
        // argc>1: scripted/CLI launch (often no TTY) still needs program-files
        // resource paths (…/resources/) so fonts and ROMs resolve.
        gs2_app_values.console_mode = true;
    }
    Paths::initialize(gs2_app_values.console_mode);

    gs2_app_values.base_path = get_base_path(gs2_app_values.console_mode);
    printf("base_path: %s\n", gs2_app_values.base_path.c_str());
    gs2_app_values.pref_path = get_pref_path();
    printf("pref_path: %s\n", gs2_app_values.pref_path.c_str());

    // Load app settings before seeding defaults so dialog-path seed cannot overwrite
    // an existing system_settings.toml with empty in-memory state.
    SystemSettings::instance().load();
    SystemConfig::ensure_default_system_configs();
    for (int i = 0; i < NUM_SYSTEM_CONFIGS; ++i) {
        SystemConfig::migrate_builtin_bram(BuiltinSystemConfigs[i]);
    }

    // Parse CLI whenever there are arguments. console_mode (isatty) is still used
    // elsewhere; without this, scripted launches (no TTY) would ignore --debug / -p / etc.
    if (gs2_app_values.console_mode || argc > 1) {
        // parse command line options
        enum { OPT_NO_QUIT_CONFIRM = 1000 };
        static struct option long_options[] = {
            {"debug", required_argument, nullptr, 'D'},
            {"no-quit-confirm", no_argument, nullptr, OPT_NO_QUIT_CONFIRM},
            {nullptr, 0, nullptr, 0}
        };
        while ((opt = getopt_long(argc, argv, "sxgp:d:D:", long_options, nullptr)) != -1) {
            switch (opt) {
                case 'p':
                    platform_id = std::stoi(optarg);
                    platform_explicit = true;
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
                            std::cout << "Mounting disk " << filename << " in slot " << slot << " drive " << drive << std::endl;
                            upsert_mount(cli_mounts, { (uint16_t)slot, (uint16_t)drive, filename});
                        }
                    }
                    break;
                /* case 'x':
                    gs2_app_values.disk_accelerator = true;
                    break; */
                case 's':
                    gs2_app_values.sleep_mode = true;
                    break;
                case 'g':
                    gs2_app_values.crt_shader_at_boot = true;
                    break;
                case 'D':
                    debug_socket_path = optarg;
                    std::cerr << "Debug protocol socket: " << debug_socket_path << "\n";
                    break;
                case OPT_NO_QUIT_CONFIRM:
                    gs2_app_values.no_quit_confirm = true;
                    break;
                default:
                    std::cerr << "Usage: " << argv[0] << " [file.gs2|*Settings.txt] [-p platform] [-dsXdY=filename] [-s] [-g] [--debug PATH] [--no-quit-confirm]\n";
                    std::cerr << "  file.gs2|*Settings.txt: load system configuration from a .gs2 TOML file\n";
                    std::cerr << "        or Neil Profiles Settings.txt file, skip the system-selector UI,\n";
                    std::cerr << "        and auto-launch that system.\n";
                    std::cerr << "        Closing the emulator window then quits the app rather\n";
                    std::cerr << "        than returning to the selector.\n";
                    std::cerr << "  -p N: skip the system-selector UI and auto-launch the\n";
                    std::cerr << "        first builtin system that matches the given platform.\n";
                    std::cerr << "        Closing the emulator window then quits the app rather\n";
                    std::cerr << "        than returning to the selector. Valid N:\n";
                    std::cerr << "          0 = Apple II         3 = Apple IIe Enhanced\n";
                    std::cerr << "          1 = Apple II Plus    4 = Apple IIe 65816\n";
                    std::cerr << "          2 = Apple IIe        5 = Apple IIgs\n";
                    std::cerr << "  -dsXdY=filename: mount disk image `filename` in slot X drive Y.\n";
                    std::cerr << "        Drives are 1-indexed; e.g. -ds6d1=foo.dsk for the\n";
                    std::cerr << "        first drive of the controller in slot 6.\n";
                    std::cerr << "        Overrides a [[storage]] entry from the config file for the same slot/drive.\n";
                    std::cerr << "  -s: sleep mode (don't busy-wait, sleep)\n";
                    std::cerr << "  -g: enable CRT post-process shader when guest emulation\n";
                    std::cerr << "        starts (same as pressing F7 with shader off).\n";
                    std::cerr << "  -D PATH, --debug PATH: listen for external debug protocol on\n";
                    std::cerr << "        Unix-domain socket PATH (see Docs/DebugProtocol.md).\n";
                    std::cerr << "  --no-quit-confirm: skip QuitModal / dirty-disk prompts on\n";
                    std::cerr << "        SDL_EVENT_QUIT (useful for tests that SIGTERM/kill the process).\n";
                    return SDL_APP_FAILURE;
            }
        }
        if (optind < argc) {
            config_path = argv[optind];
        }
    }

    if (debug_socket_path.empty()) {
        register_gs2_file_association();
    }

    if (!config_path.empty()) {
        std::string error;
        if (!apply_system_config_file(state, config_path, error)) {
            std::string diag = "Failed to load system config '" + config_path + "':\n" + error;
            std::cerr << diag << "\n";
            system_diag(diag.data());
            delete state;
            return SDL_APP_FAILURE;
        }
        for (const auto& mount : cli_mounts) {
            upsert_mount(state->disks_to_mount, mount);
        }
    } else {
        state->disks_to_mount = std::move(cli_mounts);
        state->platform_id = platform_id;
    }

    gs2_app_values.menu_event_type = SDL_RegisterEvents(1);

    // Debug print mounted media
    std::cout << "Mounted Media (" << state->disks_to_mount.size() << " disks):" << std::endl;
    for (const auto& disk_mount : state->disks_to_mount) {
        std::cout << " Slot " << disk_mount.slot << " Drive " << disk_mount.drive << " - " << disk_mount.filename << std::endl;
    }

    state->computer = new computer_t(nullptr); // We'll set the clock later.

    // Start debug protocol after computer exists so GET_STATUS can read execution_mode.
    if (!debug_socket_path.empty()) {
        state->debug_protocol = std::make_unique<DebugProtocolServer>(debug_socket_path);
        if (!state->debug_protocol->start()) {
            std::cerr << "Failed to start debug protocol server on " << debug_socket_path << "\n";
            delete state->computer;
            state->computer = nullptr;
            delete state;
            return SDL_APP_FAILURE;
        }
        state->computer->debug_protocol = state->debug_protocol.get();
    }

    video_system_t *vs = state->computer->video_system;

    initMenu(vs->window);

    state->aa = new AssetAtlas_t(vs->renderer, "img/atlas.png");
    state->aa->set_elements(MainAtlas_count, asset_rects);

    state->select_system = new SelectSystem(vs, state->aa);

    // Let vsync throttle the selection UI instead of spinning.
    // On the web, RAF does that (gs2_web_lock_raf in SDL_AppIterate).
    // SDL_SetRenderVSync(1) here would arm SDL's 60 Hz software fallback.
#ifndef __EMSCRIPTEN__
    SDL_SetRenderVSync(vs->renderer, 1);
#endif
    state->phase = PHASE_SYSTEM_SELECT;

    // If the caller passed a config file path, skip the system-selector UI and
    // jump straight into emulation using the loaded configuration.
    if (state->loaded_config) {
        std::cout << "Auto-launching system config: " << config_path << std::endl;
        transition_to_emulation(state, &state->loaded_config->config(), -1);
        state->auto_launched = true;
        vs->raise();
    } else if (platform_explicit) {
        // If the caller passed `-p PLATFORM`, skip the system-selector UI
        // and jump straight into emulation using the first builtin system
        // whose platform_id matches.
        const int system_id = find_first_system_for_platform(platform_id);
        if (system_id >= 0) {
            std::cout << "Auto-launching system_id=" << system_id
                      << " for platform_id=" << platform_id << std::endl;
            transition_to_emulation(state, get_system_config(system_id), system_id);
            state->auto_launched = true;
        } else {
            std::cerr << "No system config matches platform_id=" << platform_id
                      << ", staying in selector\n";
        }
    }

    *appstate = state;

#if defined(__EMSCRIPTEN__)
    g_web_gesture_state = state;
    // Safari opens a file picker only from the DOM click that started the
    // gesture. Install the listener now that app state exists.
    web_install_file_dialog_hook();
#endif

    // Register callback so emulation continues during macOS menu tracking and window resize
    setMenuTrackingCallback(SDL_AppIterate, state);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    GS2AppState *state = (GS2AppState *)appstate;

    // Let the platform menu consume the event first (Linux hamburger/right-click)
    if (handleMenuEvent(event)) return SDL_APP_CONTINUE;

    if (event->type == gs2_app_values.menu_event_type) {
        BlankDiskType blank_type;
        if (blank_disk_type_from_menu(event->user.code, &blank_type)) {
            begin_new_disk_image(state, blank_type);
            return SDL_APP_CONTINUE;
        }
    }

    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data
        && is_launchable_config_path(event->drop.data)) {
        request_open_config(state, event->drop.data);
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_SYSTEM_SELECT) {
        if (event->type == gs2_app_values.menu_event_type
            && event->user.code == MENU_OPEN_CONFIG) {
            open_system_config_dialog(state, false);
            return SDL_APP_CONTINUE;
        }
        state->select_system->event(*event);
        if (event->type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS; // clean exit
        }
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EDIT_SYSTEM) {
        state->edit_system->event(*event);
        if (event->type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS;
        }
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EMULATION) {
        computer_t *computer = state->computer;
        cpu_state *cpu = computer->cpu;

        handle_single_event(computer, cpu, *event);

        // handled in computer now
        /* if (event->type == SDL_EVENT_QUIT) {
            cpu->halt = HLT_USER;
        } */
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

#if defined(__EMSCRIPTEN__)
// Pull SDL events off the queue and run them through SDL_AppEvent. The main
// loop does this on the next animation frame, which is too late for Safari's
// file-picker check (it requires the DOM click call stack, not just transient
// activation). Returns false when the queue was empty.
static bool gs2_web_dispatch_queued(GS2AppState *state)
{
    SDL_PumpEvents();
    bool any = false;
    SDL_Event ev;
    for (int n = 0; n < 64; ++n) {
        const int got = SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (got <= 0)
            break;
        any = true;
        const SDL_AppResult rc = SDL_AppEvent(state, &ev);
        if (rc != SDL_APP_CONTINUE) {
            // Direct SDL_AppEvent skips SDL's callback-result atomic. Remember
            // it so the next iterate actually quits.
            g_web_gesture_result = rc;
            break;
        }
    }
    return any;
}

// DOM click listener (see web_install_file_dialog_hook). ImGui trickles a
// button down and the matching up onto separate frames, and File > Drives
// then pushes another SDL event. Two frames plus a trailing drain turn that
// click into web_open_file_dialog before this function returns.
extern "C" EMSCRIPTEN_KEEPALIVE void gs2_web_user_gesture(void)
{
    if (g_web_gesture_depth)
        return;
    GS2AppState *state = g_web_gesture_state;
    if (!state || !state->computer || !state->computer->video_system)
        return;
    video_system_t *vs = state->computer->video_system;
    if (!vs->renderer)
        return;

    g_web_gesture_depth++;
    for (int pass = 0; pass < 3; ++pass) {
        const int generation = web_file_dialog_generation();
        gs2_web_dispatch_queued(state);
        if (g_web_gesture_result != SDL_APP_CONTINUE)
            break;
        if (web_file_dialog_generation() != generation)
            break; // picker is up; don't synthesize a second one
        if (pass == 2)
            break;
        const SDL_FRect content = vs->target_rect_in_window_points();
        advanceMenuFrameForGesture(vs->renderer, &content);
    }
    g_web_gesture_depth--;
}
#endif

/**
 * Draw the menu bar for the SelectSystem / EditSystem phases.
 *
 * ImGui works in window points (handleMenuEvent feeds it raw SDL events;
 * ImGui_ImplSDL3_NewFrame takes DisplaySize from SDL_GetWindowSize), so we
 * switch to a points-sized STRETCH presentation for the overlay. The bar sits
 * immediately above the letterboxed dest (or on its top edge when the canvas
 * is wide), so present-time letterbox fill cannot erase it or its menus, and
 * the selector / editor leave a strip so widgets are not covered. Callers
 * re-apply their presentation afterwards, since event() converts mouse
 * positions with it.
 */
static void render_menu_overlay_for_ui_phase(video_system_t *vs) {
#if defined(__linux__) || defined(__EMSCRIPTEN__)   // ImGui menu (see menu.h)
    SDL_FRect content = vs->target_rect_in_window_points();
    int points_w = 0, points_h = 0;
    SDL_GetWindowSize(vs->window, &points_w, &points_h);
    SDL_SetRenderLogicalPresentation(vs->renderer, points_w, points_h,
        SDL_LOGICAL_PRESENTATION_STRETCH);

    renderMenuOverlay(vs->renderer, &content);

    // Leave presentation DISABLED for present(): the opening SDL_RenderClear()
    // already blacked the whole target. Callers restore LETTERBOX after present
    // so event() conversion stays in design space.
    SDL_SetRenderLogicalPresentation(vs->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
#else
    // macOS / Windows have a native menu bar: this draws nothing into the
    // renderer, so leave the caller's presentation alone and let SDL fill the
    // letterbox bars at present() as before.
    renderMenuOverlay(vs->renderer, nullptr);
#endif
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    GS2AppState *state = (GS2AppState *)appstate;

#if defined(__EMSCRIPTEN__)
    if (g_web_gesture_result != SDL_APP_CONTINUE) {
        const SDL_AppResult rc = g_web_gesture_result;
        g_web_gesture_result = SDL_APP_CONTINUE;
        return rc;
    }
    // After SDL_PumpEvents, which may have applied a deferred swap interval.
    gs2_web_lock_raf();
#endif

    // Pump any pending GTK/GDK events (Linux menu). Called here rather than
    // in SDL_AppEvent to avoid blocking SDL's X11 connection (deadlock risk).
    pumpMenuEvents();

    // Drain main-thread debug commands every frame (including while paused / on selector).
    if (state->debug_protocol) {
        state->debug_protocol->process_main_thread(state->computer);
    }

    if (state->phase == PHASE_SYSTEM_SELECT) {
        /* Render the selection UI (one frame). Events already dispatched by SDL_AppEvent. */
        video_system_t *vs = state->computer->video_system;

        // ImGui menu events are consumed by handleMenuEvent (WantCaptureMouse) and
        // never dirty SelectSystem. Keep ticking the overlay while ImGui has capture,
        // or the menu and the tile UI both freeze.
        if (menuNeedsFrame()) {
            state->select_system->mark_dirty();
        }
        if (state->select_system->update()) {
            SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
            vs->clear();
            state->select_system->render();
            render_menu_overlay_for_ui_phase(vs);
            vs->present();
            state->select_system->apply_logical_presentation();
        }

        int system_id = state->select_system->get_selected_system();
        if (system_id == SELECT_QUIT) {
            return SDL_APP_SUCCESS; // user closed window during selection
        }
        if (system_id == SELECT_NEW) {
            state->select_system->clear_selection();
            transition_to_edit_system(state);
            state->edit_system->start_new();
            return SDL_APP_CONTINUE;
        }
        if (system_id == SELECT_OPEN_LAUNCH) {
            state->select_system->clear_selection();
            open_system_config_dialog(state, false);
            return SDL_APP_CONTINUE;
        }
        if (system_id == SELECT_OPEN_EDIT) {
            state->select_system->clear_selection();
            open_system_config_dialog(state, true);
            return SDL_APP_CONTINUE;
        }
        if (system_id >= SELECT_RECENT_BASE) {
            const std::string& path = state->select_system->get_recent_path(system_id);
            state->select_system->clear_selection();
            if (path.empty()) {
                return SDL_APP_CONTINUE;
            }
            std::string error;
            if (!apply_system_config_file(state, path, error)) {
                std::string diag = "Failed to load system config '" + path + "':\n" + error;
                std::cerr << diag << "\n";
                system_diag(diag.data());
                return SDL_APP_CONTINUE;
            }
            transition_to_emulation(state, &state->loaded_config->config(), -1);
            return SDL_APP_CONTINUE;
        }
        if (system_id >= 0) {
            transition_to_emulation(state, get_system_config(system_id), system_id);
        }
        // On the web, vsync paces the selection UI; blocking would hang the tab.
#ifndef __EMSCRIPTEN__
        SDL_Delay(16);
#endif
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EDIT_SYSTEM) {
        video_system_t *vs = state->computer->video_system;
        // Same ImGui capture / dirty-flag coupling as PHASE_SYSTEM_SELECT.
        if (menuNeedsFrame()) {
            state->edit_system->mark_dirty();
        }
        if (state->edit_system->update()) {
            SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
            vs->clear();
            state->edit_system->render();
            render_menu_overlay_for_ui_phase(vs);
            vs->present();
            state->edit_system->apply_logical_presentation();
        }

        int edit_result = state->edit_system->get_result();
        if (edit_result == EDIT_QUIT) {
            return SDL_APP_SUCCESS;
        }
        if (edit_result == EDIT_CANCEL || edit_result == EDIT_SAVED) {
            leave_edit_system(state);
        }
#ifndef __EMSCRIPTEN__
        SDL_Delay(16);
#endif
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EMULATION) {
        computer_t *computer = state->computer;

        osd->update();

        if (!run_one_frame(computer)) {
            // Finder Open of another .gs2 while emulating: tear down and
            // boot the pending config. Must not take the auto_launched exit
            // path — that would quit the process before the switch.
            if (state->switch_after_halt && !state->pending_config_path.empty()) {
                const std::string path = std::move(state->pending_config_path);
                state->pending_config_path.clear();
                state->switch_after_halt = false;
                transition_to_shutdown(state);
                std::string error;
                if (!launch_config_and_emulate(state, path, true, error)) {
                    std::string diag = "Failed to load system config '" + path + "':\n" + error;
                    std::cerr << diag << "\n";
                    system_diag(diag.data());
                }
                return SDL_APP_CONTINUE;
            }
            // User requested halt. Snapshot before transition_to_shutdown
            // clears auto_launched. When exiting the process, skip selector
            // recreation — SDL_AppQuit tears down; recreating first leaks
            // SDL objects and can hang on an assertion dialog.
            const bool exit_app = state->auto_launched || gs2_app_values.force_app_exit;
            if (exit_app) {
                std::string tracepath;
                Paths::calc_docs(tracepath, "gssquared-trace.bin");
                computer->cpu->trace_buffer->save_to_file(tracepath);
                return SDL_APP_SUCCESS;
            }
            transition_to_shutdown(state);
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    //(void)result;
    GS2AppState *state = (GS2AppState *)appstate;
#if defined(__EMSCRIPTEN__)
    g_web_gesture_state = nullptr;
#endif
    if (!state) return;

    // Stop the debug protocol thread before tearing down computer/SDL objects;
    // otherwise the bridge thread can race AppQuit and trip SDL object asserts.
    if (state->debug_protocol) {
        if (state->computer) {
            state->computer->debug_protocol = nullptr;
        }
        state->debug_protocol->stop();
        state->debug_protocol.reset();
    }

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

    delete state->edit_system;
    delete state->select_system;
    delete state->aa;
    delete state;
    // SDL_Quit() is called automatically by SDL after this returns.
}

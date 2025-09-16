/*
 *   Copyright (c) 2025 Jawaid Bazyar

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
#include <SDL3/SDL_main.h>

#include "gs2.hpp"
#include "paths.hpp"
#include "cpu.hpp"
#include "clock.hpp"
#include "display/display.hpp"
#include "display/text_40x24.hpp"
#include "event_poll.hpp"
#include "devices/speaker/speaker.hpp"
#include "platforms.hpp"
#include "util/dialog.hpp"
#include "util/mount.hpp"
#include "ui/OSD.hpp"
#include "systemconfig.hpp"
#include "slots.hpp"
#include "util/soundeffects.hpp"
#include "devices/diskii/diskii.hpp"
#include "videosystem.hpp"
#include "debugger/debugwindow.hpp"
#include "computer.hpp"
#include "mmus/mmu_ii.hpp"
#include "mmus/mmu_iie.hpp"
#include "util/EventTimer.hpp"
#include "ui/SelectSystem.hpp"
#include "ui/MainAtlas.hpp"
#include "cpus/cpu_implementations.hpp"
#include "version.h"
#include "util/Metrics.hpp"

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

void frame_event(computer_t *computer, cpu_state *cpu) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        // check for system "pre" events
        if (computer->sys_event->dispatch(event)) {
            continue;
        }
        if (computer->debug_window->handle_event(event)) { // ignores event if not for debug window
            continue;
        }
        if (!osd->event(event)) { // if osd doesn't handle it..
            computer->dispatch->dispatch(event); // they say call "once per frame"
        }
    }

    osd->update();
}

void frame_appevent(computer_t *computer, cpu_state *cpu) {
    Event *event = computer->event_queue->getNextEvent();
    if (event) {
        switch (event->getEventType()) {
            case EVENT_PLAY_SOUNDEFFECT:
                soundeffects_play(event->getEventData());
                break;
            case EVENT_REFOCUS:
                computer->video_system->raise();
                break;
            case EVENT_MODAL_SHOW:
                osd->show_diskii_modal(event->getEventKey(), event->getEventData());
                break;
            case EVENT_MODAL_CLICK:
                {
                    uint64_t key = event->getEventKey();
                    uint64_t data = event->getEventData();
                    printf("EVENT_MODAL_CLICK: %llu %llu\n", key, data);
                    if (data == 1) {
                        // save and unmount.
                        computer->mounts->unmount_media(key, SAVE_AND_UNMOUNT);
                        osd->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_OPEN));
                    } else if (data == 2) {
                        // save as - need to open file dialog, get new filename, change media filename, then unmount.
                    } else if (data == 3) {
                        // discard
                        computer->mounts->unmount_media(key, DISCARD);
                        osd->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_OPEN));
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
void frame_video_update(computer_t *computer, cpu_state *cpu) {

    computer->video_system->update_display();    
    osd->render();
    computer->debug_window->render();
    computer->video_system->present();
}

void frame_sleep(computer_t *computer, cpu_state *cpu, uint64_t last_cycle_time, uint64_t frame_count) {
    uint64_t wakeup_time = last_cycle_time + 16688154 + (frame_count & 1); // even frames have 16688154, odd frames have 16688154 + 1
        
    // sleep out the rest of this frame.
    uint64_t sleep_loops = 0;
    uint64_t current_time = SDL_GetTicksNS();
    if (current_time > wakeup_time) {
        cpu->clock_slip++;
        // TODO: log clock slip for later display.
        //printf("Clock slip: event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
    } else {
        if (gs2_app_values.sleep_mode) {
            SDL_DelayPrecise(wakeup_time - SDL_GetTicksNS());
        } else {
            // busy wait sync cycle time
            do {
                sleep_loops++;
            } while (SDL_GetTicksNS() < wakeup_time);
        }
    }
}

/*
This is the run_cpus func with:
speeds other than 1mhz ripped out,
*/
void run_cpus(computer_t *computer) {
    cpu_state *cpu = computer->cpu;
    
    uint64_t last_cycle_time = SDL_GetTicksNS();

    uint64_t frame_count = 0;     // used to add an extra bit of time to frame sleep

    uint64_t frame_14M_marker = 0;
    uint64_t last_frame_14M_marker = 0;

    while (cpu->halt != HLT_USER) { // top of frame.

        uint64_t frdiff = frame_14M_marker - last_frame_14M_marker; // this is just a check.
        last_frame_14M_marker = frame_14M_marker;
        uint64_t end_frame_c_14M = frame_14M_marker;

        if (cpu->execution_mode == EXEC_STEP_INTO) {
            
            /* This will run about 60fps, primarily waiting on user input in the debugger window. */
            while (cpu->instructions_left) {
                if (computer->event_timer->isEventPassed(cpu->cycles)) {
                    computer->event_timer->processEvents(cpu->cycles);
                }
                (cpu->cpun->execute_next)(cpu);
                cpu->instructions_left--;
            }
            uint64_t current_time = SDL_GetTicksNS();

            MEASURE(computer->event_times, frame_event(computer, cpu));
    
            /* Emit Audio Frame */
            // disable audio in step mode.
            //MEASURE(computer->audio_times, audio_generate_frame(cpu /* , last_cycle_window_start, cycle_window_start */));
    
            /* Process Internal Event Queue */
            MEASURE(computer->app_event_times, frame_appevent(computer, cpu));
    
            /* Execute Device Frames - 60 fps */
            MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());
    
            /* Emit Video Frame */
            /* if there is a video scanner and it has a full frame, briefly switch to normal mode and update display
            to consume the frame data.
            */
            //update_flash_state(cpu); // TODO: this goes into display.cpp frame handler.
            if (cpu->video_scanner != nullptr) {
                if (cpu->video_scanner->get_frame_scan()->get_count() >= 7680) { // consume a cycle's worth of frame data.
                    cpu->execution_mode = EXEC_NORMAL;
                    computer->video_system->update_display();    
                    cpu->execution_mode = EXEC_STEP_INTO;
                }
            }
            osd->render();
            computer->debug_window->render();
            computer->video_system->present();
            MEASURE(computer->display_times, frame_video_update(computer, cpu));
    
            // update frame window counters.
            // TODO:if we're in stepwise mode, we should not increment these as if they're a full next frame.
            if (cpu->c_14M >= end_frame_c_14M) {
                cpu->current_frame_start_14M = cpu->next_frame_start_14M;
                cpu->next_frame_start_14M += 238944;
                frame_14M_marker += 238944;
                cpu->frame_count++;

                // set next frame cycle time.
                computer->set_frame_start_cycle();
            }

            // sleep for 1/60th second ish, without updating frame counts etc.
            uint64_t wakeup_time = last_cycle_time + 16667000;
            SDL_DelayPrecise(wakeup_time - SDL_GetTicksNS());
            
        } else if (cpu->execution_mode == EXEC_STEP_OVER) {

        } else if ((cpu->execution_mode == EXEC_NORMAL) && (cpu->clock_mode != CLOCK_FREE_RUN)) {
            computer->set_frame_start_cycle();

            if (computer->debug_window->window_open) {
                while (cpu->c_14M < end_frame_c_14M) { // 1/60th second.
                    if (computer->event_timer->isEventPassed(cpu->cycles)) {
                        computer->event_timer->processEvents(cpu->cycles);
                    }
                    (cpu->cpun->execute_next)(cpu);
                    if (computer->debug_window->window_open) {
                        if (computer->debug_window->check_breakpoint(&cpu->trace_entry)) {
                            cpu->execution_mode = EXEC_STEP_INTO;
                            cpu->instructions_left = 0;
                            break;
                        }
                        if (cpu->trace_entry.opcode == 0x00) { // catch a BRK and stop execution.
                            cpu->execution_mode = EXEC_STEP_INTO;
                            cpu->instructions_left = 0;
                            break;
                        }
                    }
                }
            } else { // skip all debug checks if debug window is not open - this may seem repetitious but it saves all kinds of cycles where every cycle counts (GO FAST MODE)
                while (cpu->c_14M < end_frame_c_14M) { // 1/60th second.
                    if (computer->event_timer->isEventPassed(cpu->cycles)) {
                        computer->event_timer->processEvents(cpu->cycles);
                    }
                    (cpu->cpun->execute_next)(cpu);
                }
            }

            uint64_t current_time = SDL_GetTicksNS();

            /* Process Events */
            MEASURE(computer->event_times, frame_event(computer, cpu));
    
            /* Emit Audio Frame */
            MEASURE(computer->audio_times, audio_generate_frame(cpu /* , last_cycle_window_start, cycle_window_start */));
    
            /* Process Internal Event Queue */
            MEASURE(computer->app_event_times, frame_appevent(computer, cpu));
    
            /* Execute Device Frames - 60 fps */
            MEASURE(computer->device_times, computer->device_frame_dispatcher->dispatch());
    
            /* Emit Video Frame */
    
            MEASURE(computer->display_times, frame_video_update(computer, cpu));
    
            // update frame window counters.
            cpu->current_frame_start_14M = cpu->next_frame_start_14M;
            cpu->next_frame_start_14M += 238944;
            frame_14M_marker += 238944;
            cpu->frame_count++;
    
            // calculate what sleep-until time should be.
            uint64_t wakeup_time = last_cycle_time + 16688154 + (frame_count & 1); // even frames have 16688154, odd frames have 16688154 + 1
            
            // sleep out the rest of this frame.
            frame_sleep(computer, cpu, last_cycle_time, frame_count);

        } else { // Ludicrous Speed!
            // TODO: how to handle VBL timing here. estimate it based on realtime?
            computer->set_frame_start_cycle(); // todo: unsure if this is right..
            
            uint64_t next_frame_time = last_cycle_time + 16688154 + (frame_count & 1); // even frames have 16688154, odd frames have 16688154 + 1

            uint64_t frdiff = frame_14M_marker - last_frame_14M_marker; // this is just a check.
            last_frame_14M_marker = frame_14M_marker;
            uint64_t end_frame_c_14M = frame_14M_marker;
            
            if (computer->debug_window->window_open) {
                while (SDL_GetTicksNS() < next_frame_time) { // run emulated frame, but of course we don't sleep in this loop so we'll Go Fast.
                    if (computer->event_timer->isEventPassed(cpu->cycles)) {
                        computer->event_timer->processEvents(cpu->cycles);
                    }
                    (cpu->cpun->execute_next)(cpu);
                    if (computer->debug_window->window_open) {
                        if (computer->debug_window->check_breakpoint(&cpu->trace_entry)) {
                            cpu->execution_mode = EXEC_STEP_INTO;
                            cpu->instructions_left = 0;
                            break;
                        }
                        if (cpu->trace_entry.opcode == 0x00) { // catch a BRK and stop execution.
                            cpu->execution_mode = EXEC_STEP_INTO;
                            cpu->instructions_left = 0;
                            break;
                        }
                    }
                }
            } else { // skip all debug checks if debug window is not open - this may seem repetitious but it saves all kinds of cycles where every cycle counts (GO FAST MODE)
                while (SDL_GetTicksNS() < next_frame_time) { // run emulated frame, but of course we don't sleep in this loop so we'll Go Fast.
                    if (computer->event_timer->isEventPassed(cpu->cycles)) {
                        computer->event_timer->processEvents(cpu->cycles);
                    }
                    (cpu->cpun->execute_next)(cpu);
                }
            }
            // this was roughly one video frame so let's pretend we went that many.
            cpu->c_14M += 238944; // fake increment this so it doesn't get wildly out of sync.
            cpu->current_frame_start_14M = cpu->next_frame_start_14M;
            cpu->next_frame_start_14M += 238944;
            frame_14M_marker += 238944;
            cpu->frame_count++;
            // TODO: either push current cpu count into Speaker here or have Speaker sync up when we leave ludicrous speed.

            uint64_t current_time = SDL_GetTicksNS();

            // if it's been roughly 1/60th second then do a device/display/etc frame update.

                /* Process Events */
                MEASURE(computer->event_times, frame_event(computer, cpu));
        
                /* Emit Audio Frame */
                //MEASURE(computer->audio_times, audio_generate_frame(cpu /* , last_cycle_window_start, cycle_window_start */));
        
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

        }
        
        /*
         should this be last_cycle_time = wakeup_time? Then we don't lose some ns on the function return etc..
         discussion: in the event of a clock slip, we get all confused if we only stay synced to wakeup_time.
         as long as there are no slips, we're good.
         */
        last_cycle_time = SDL_GetTicksNS(); 
        
        // update frame status; calculate stats; move these variables into computer;
        computer->frame_status_update();

        frame_count++;
    }

    // save cpu trace buffer, then exit.
    computer->video_system->update_display(); // update one last time to show the last state.

    cpu->trace_buffer->save_to_file(gs2_app_values.pref_path + "trace.bin");
}

void run_cpus_old(computer_t *computer) {
    cpu_state *cpu = computer->cpu;
    uint64_t status_count = 0; // how many seconds since last status update basically (well, it's 60 * this frames)

    /* initialize time tracker vars */
    uint64_t ct = SDL_GetTicksNS();
    uint64_t last_event_update = ct;
    uint64_t last_display_update = ct;
    uint64_t last_audio_update = ct;
    uint64_t last_app_event_update = ct;
    uint64_t last_5sec_update = ct;
    uint64_t last_mockingboard_update = ct;
    uint64_t last_frame_update = ct;
    uint64_t last_5sec_cycles = cpu->cycles;

    uint64_t last_cycle_count =cpu->cycles;
    uint64_t last_cycle_time = SDL_GetTicksNS();

    //uint64_t last_time_window_start = 0;
    uint64_t last_cycle_window_start = 0;

    uint64_t frame_counter_start_time = SDL_GetTicksNS();
    uint64_t frame_count = 0;

    uint64_t last_frame_overrun = 0;

    while (1) {
        uint64_t cycle_window_start = cpu->cycles;
        uint64_t cycle_window_delta = cycle_window_start - last_cycle_window_start;

        //uint64_t last_cycle_count = cpu->cycles;
        //uint64_t last_cycle_time = SDL_GetTicksNS();

        uint64_t cycles_for_this_burst = clock_mode_info[cpu->clock_mode].cycles_per_frame;
        uint64_t end_frame_c_14M = cpu->next_frame_start_14M - last_frame_overrun;

        if (! cpu->halt) {
            //cpu->frame_end_ns_56_8 += ((uint64_t)1000000000*256) / (59.923f) - last_frame_overrun_56_8; // adjust for last frame's overrun
            //cpu->frame_end_ns_56_8 += ((uint64_t)16'688'000*256) - last_frame_overrun_56_8; // adjust for last frame's overrun
            switch (cpu->execution_mode) {
                    case EXEC_NORMAL:
                        {
                        if (computer->debug_window->window_open) {
                            while (cpu->c_14M < end_frame_c_14M) { // 1/60th second.
                                if (computer->event_timer->isEventPassed(cpu->cycles)) {
                                    computer->event_timer->processEvents(cpu->cycles);
                                }
                                (cpu->cpun->execute_next)(cpu);
                                if (computer->debug_window->window_open) {
                                    if (computer->debug_window->check_breakpoint(&cpu->trace_entry)) {
                                        cpu->execution_mode = EXEC_STEP_INTO;
                                        cpu->instructions_left = 0;
                                        break;
                                    }
                                    if (cpu->trace_entry.opcode == 0x00) { // catch a BRK and stop execution.
                                        cpu->execution_mode = EXEC_STEP_INTO;
                                        cpu->instructions_left = 0;
                                        break;
                                    }
                                }
                            }
                            last_frame_overrun = cpu->c_14M - cpu->next_frame_start_14M;
                        } else { // skip all debug checks if the window is not open - this may seem repetitioius but it saves all kinds of cycles where every cycle counts (GO FAST MODE)
                            while (cpu->c_14M < end_frame_c_14M) { // 1/60th second.
                                if (computer->event_timer->isEventPassed(cpu->cycles)) {
                                    computer->event_timer->processEvents(cpu->cycles);
                                }
                                (cpu->cpun->execute_next)(cpu);
                            }
                            last_frame_overrun = cpu->c_14M - cpu->next_frame_start_14M;
                        }
                        }
                        break;
                    case EXEC_STEP_INTO:
                        while (cpu->instructions_left) {
                            if (computer->event_timer->isEventPassed(cpu->cycles)) {
                                computer->event_timer->processEvents(cpu->cycles);
                            }
                            (cpu->cpun->execute_next)(cpu);
                            cpu->instructions_left--;
                        }
                        break;
                    case EXEC_STEP_OVER:
                        break;
                }
        } else {
            // fake-increment cycle counter to keep audio in sync.
            last_cycle_count = cpu->cycles;
            cpu->cycles += cycles_for_this_burst;
        }

        uint64_t current_time;
        uint64_t audio_time;
        uint64_t display_time;
        uint64_t event_time;
        uint64_t app_event_time;

        bool this_free_run = (cpu->clock_mode == CLOCK_FREE_RUN) || (cpu->execution_mode == EXEC_STEP_INTO );

        if ((this_free_run) && (current_time - last_event_update > 16667000)
            || (!this_free_run)) {
            current_time = SDL_GetTicksNS();

            //computer->dispatch->processEvents();

            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                // check for system "pre" events
                if (computer->sys_event->dispatch(event)) {
                    continue;
                }
                if (computer->debug_window->handle_event(event)) { // ignores event if not for debug window
                    continue;
                }
                if (!osd->event(event)) { // if osd doesn't handle it..
                    computer->dispatch->dispatch(event); // they say call "once per frame"
                }
            }

            osd->update();

            event_time = SDL_GetTicksNS() - current_time;
            last_event_update = current_time;
        }

        /* Emit Audio Frame */
        current_time = SDL_GetTicksNS();
        if ((this_free_run) && (current_time - last_audio_update > 16667000)
            || (!this_free_run)) {            
            audio_generate_frame(cpu  /* , last_cycle_window_start, cycle_window_start */);
            audio_time = SDL_GetTicksNS() - current_time;
            last_audio_update = current_time;
        }

        /* Process Internal Event Queue */
        current_time = SDL_GetTicksNS();
        if ((this_free_run) && (current_time - last_app_event_update > 16667000)
            || (!this_free_run)) {
            Event *event = computer->event_queue->getNextEvent();
            if (event) {
                switch (event->getEventType()) {
                    case EVENT_PLAY_SOUNDEFFECT:
                        soundeffects_play(event->getEventData());
                        break;
                    case EVENT_REFOCUS:
                        computer->video_system->raise();
                        break;
                    case EVENT_MODAL_SHOW:
                        osd->show_diskii_modal(event->getEventKey(), event->getEventData());
                        break;
                    case EVENT_MODAL_CLICK:
                        {
                            uint64_t key = event->getEventKey();
                            uint64_t data = event->getEventData();
                            printf("EVENT_MODAL_CLICK: %llu %llu\n", key, data);
                            if (data == 1) {
                                // save and unmount.
                                computer->mounts->unmount_media(key, SAVE_AND_UNMOUNT);
                                osd->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_OPEN));
                            } else if (data == 2) {
                                // save as - need to open file dialog, get new filename, change media filename, then unmount.
                            } else if (data == 3) {
                                // discard
                                computer->mounts->unmount_media(key, DISCARD);
                                osd->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_OPEN));
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
            app_event_time = SDL_GetTicksNS() - current_time;
            last_app_event_update = current_time;
        }

        /* Execute Device Frames - 60 fps */
        current_time = SDL_GetTicksNS();
        if ((this_free_run) && (current_time - last_frame_update > 16667000)
            || (!this_free_run)) {
            computer->device_frame_dispatcher->dispatch();
            last_frame_update = current_time;
        }

        /* Emit Video Frame */
        current_time = SDL_GetTicksNS();
        if ((this_free_run) && (current_time - last_display_update > 16667000)
            || (!this_free_run)) {
            //_state(cpu); // TODO: this goes into display.cpp frame handler.
            computer->video_system->update_display();    
            osd->render();
            computer->debug_window->render();
            computer->video_system->present();
            display_time = SDL_GetTicksNS() - current_time;
            last_display_update = current_time;
        }

        /* Emit 5-second Stats */
        /* current_time = SDL_GetTicksNS();
        if (current_time - last_5sec_update > 5'000'000'000) {
            uint64_t delta = cpu->cycles - last_5sec_cycles;
            cpu->e_mhz = (float)delta / float(5'000'000);

            fprintf(stdout, "%llu delta %llu cycles clock-mode: %d CPS: %f MHz [ slips: %llu ]\n", delta, cpu->cycles, cpu->clock_mode, cpu->e_mhz, cpu->clock_slip);
            fprintf(stdout, "event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
            fprintf(stdout, "PC: %04X, A: %02X, X: %02X, Y: %02X, P: %02X\n", cpu->pc, cpu->a, cpu->x, cpu->y, cpu->p);
            last_5sec_cycles = cpu->cycles;
            last_5sec_update = current_time;
        } */

        if (cpu->halt == HLT_USER) {
            computer->video_system->update_display(); // update one last time to show the last state.
            break;
        }

        // update frame window counters.
        // TODO:if we're in stepwise mode, we should not increment these as if they're a full next frame.
        if (cpu->c_14M >= end_frame_c_14M) {
            cpu->current_frame_start_14M = cpu->next_frame_start_14M;
            cpu->next_frame_start_14M += 238944;
            cpu->frame_count++;
        }

        // calculate what sleep-until time should be.
        //uint64_t wakeup_time = last_cycle_time + (cpu->cycles - last_cycle_count) * cpu->cycle_duration_ns;
        //uint64_t wakeup_time = last_cycle_time + ((uint64_t) 1000000000 / 59.923) ;
        uint64_t wakeup_time = last_cycle_time + 16688154 + (frame_count & 1); // even frames have 16688154, odd frames have 16688154 + 1

        if (!this_free_run)  {
            uint64_t sleep_loops = 0;
            uint64_t current_time = SDL_GetTicksNS();
            if (current_time > wakeup_time) {
                cpu->clock_slip++;
                printf("Clock slip: event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
            } else {
                if (gs2_app_values.sleep_mode) {
                    SDL_DelayPrecise(wakeup_time - SDL_GetTicksNS());
                } else {
                    // busy wait sync cycle time
                    do {
                        sleep_loops++;
                    } while (SDL_GetTicksNS() < wakeup_time);
                }
            }
        }

        last_cycle_time = SDL_GetTicksNS(); // TODO: I am pretty sure this is redundant.
        last_cycle_count = cpu->cycles;

        //last_time_window_start = time_window_start;
        last_cycle_window_start = cycle_window_start;
        
        if ((this_free_run) && (current_time - last_frame_update > 16667000) // we need this, free run is ludicrous OR step-into
            || (!this_free_run)) {
                frame_count++;
                if (frame_count % 60 == 0) {
                    uint64_t frame_counter_end_time = last_cycle_time;
                    uint64_t frame_counter_delta = frame_counter_end_time - frame_counter_start_time;

                    cpu->fps = ((float)frame_count * 1000000000) / frame_counter_delta;
                    frame_counter_start_time = frame_counter_end_time;
                    frame_count = 0;
    
                    uint64_t delta = cpu->cycles - last_5sec_cycles;
                    cpu->e_mhz = 1000 * (double)delta / ((double)(frame_counter_end_time - last_5sec_update));
                    last_5sec_cycles = cpu->cycles;
                    last_5sec_update = frame_counter_end_time;

                    // TODO: would like this to track like 1 sec or 5 sec rolling avg of these statistics.
                    status_count++;
                    if (status_count == 5) {
                        fprintf(stdout, "%llu delta %llu cycles clock-mode: %d CPS: %12.8f MHz [ slips: %llu ]\n", delta, cpu->cycles, cpu->clock_mode, cpu->e_mhz, cpu->clock_slip);
                        fprintf(stdout, "event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
                        fprintf(stdout, "PC: %04X, A: %02X, X: %02X, Y: %02X, P: %02X\n", cpu->pc, cpu->a, cpu->x, cpu->y, cpu->p);        
                        status_count = 0;
                    }
                }
        }
    }
    cpu->trace_buffer->save_to_file(gs2_app_values.pref_path + "trace.bin");
}

gs2_app_t gs2_app_values;

int main(int argc, char *argv[]) {
    std::cout << "Booting GSSquared!" << std::endl;

    SDL_SetAppMetadata("GSSquared", VERSION_STRING, "Copyright 2025 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Copyright 2025 by Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "Jawaid Bazyar");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/jawaidbazyar/gssquared");
    
    int platform_id = PLATFORM_APPLE_II_PLUS;  // default to Apple II Plus
    int opt;
    
    char slot_str[2], drive_str[2] /* , filename[256] */;
    int slot, drive;
    
    std::vector<disk_mount_t> disks_to_mount;

    if (isatty(fileno(stdin))) {
        gs2_app_values.console_mode = true;
    }

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
                            disks_to_mount.push_back({slot, drive, filename});
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
                    exit(1);
            }
        }
    }

    // Debug print mounted media
    std::cout << "Mounted Media (" << disks_to_mount.size() << " disks):" << std::endl;
    for (const auto& disk_mount : disks_to_mount) {
        std::cout << " Slot " << disk_mount.slot << " Drive " << disk_mount.drive << " - " << disk_mount.filename << std::endl;
    }

    while (1) {

    computer_t *computer = new computer_t();

    video_system_t *vs = computer->video_system;

    AssetAtlas_t *aa = new AssetAtlas_t(vs->renderer, "img/atlas.png");
    aa->set_elements(MainAtlas_count, asset_rects);

    SelectSystem *select_system = new SelectSystem(vs, aa);
    int system_id = select_system->select();
    if (system_id == -1) {
        delete select_system;
        delete aa;
        delete computer;
        break;
    }
    SystemConfig_t *system_config = get_system_config(system_id);
    platform_id = system_config->platform_id;

    platform_info* platform = get_platform(platform_id);
    print_platform_info(platform);
    computer->set_platform(platform);
    
    // TODO: load platform roms - this info should get stored in the 'computer'
    rom_data *rd = load_platform_roms(platform);
    if (!rd) {
        system_failure("Failed to load platform roms, exiting.");
        exit(1);
    }

    // we will ALWAYS have a 256 page map. because it's a 6502 and all is addressible in a II.
    // II can have 4k, 8k, 12k; or 16k, 32k, 48k.
    // II Plus can have 16k, 32K, or 48k RAM. 16K more BUT IN THE LANGUAGE CARD MODULE.
    // always 12k rom, but not necessarily always the same ROM.
    MMU_II *mmu_ii = nullptr;
    MMU_IIe *mmu_iie = nullptr;

    switch (platform->mmu_type) {
        case MMU_MMU_II:
            mmu_ii = new MMU_II(256, 48*1024, (uint8_t *) rd->main_rom_data);
            computer->cpu->set_mmu(mmu_ii);
            computer->set_mmu(mmu_ii);
            break;
        case MMU_MMU_IIE:
            mmu_iie = new MMU_IIe(256, 128*1024, (uint8_t *) rd->main_rom_data);
            computer->cpu->set_mmu(mmu_iie);
            computer->set_mmu(mmu_iie);
            break;
        default:
            printf("Unknown MMU type: %d\n", platform->mmu_type);
            break;
    }

    // need to tell the MMU about our ROM somehow.
    // need a function in MMU to "reset page to default".
#if 0
    computer->cpu->set_processor(platform->processor_type);
#endif
    computer->cpu->cpun = createCPU(platform->cpu_type);
    computer->mounts = new Mounts(computer->cpu);

    //init_display_font(rd);

    //SystemConfig_t *system_config = get_system_config(platform_id);

    SlotManager_t *slot_manager = new SlotManager_t();

    for (int i = 0; system_config->device_map[i].id != DEVICE_ID_END; i++) {
        DeviceMap_t dm = system_config->device_map[i];

        Device_t *device = get_device(dm.id);
        if (device->power_on == nullptr) {
            printf("Device has no poweron, not found: %d", dm.id);
            continue;
        } 
        device->power_on(computer, dm.slot);
        if (dm.slot != SLOT_NONE) {
            slot_manager->register_slot(device, dm.slot);
        }
    }

    bool result = soundeffects_init(computer);

    computer->cpu->reset();

    // mount disks - AFTER device init.
    while (!disks_to_mount.empty()) {
        disk_mount_t disk_mount = disks_to_mount.back();
        disks_to_mount.pop_back(); 

        computer->mounts->mount_media(disk_mount);
    }

    //video_system_t *vs = computer->video_system;
    osd = new OSD(computer, computer->cpu, vs->renderer, vs->window, slot_manager, 1120, 768, aa);
    // TODO: this should be handled differently. have osd save/restore?
    int error = SDL_SetRenderTarget(vs->renderer, nullptr);
    if (!error) {
        fprintf(stderr, "Error setting render target: %s\n", SDL_GetError());
        return 1;
    }
    computer->video_system->set_window_title(system_config->name);
    //computer->mmu->dump_page_table(0x00, 0x0f);
    computer->video_system->update_display(); // check for events 60 times per second.

    run_cpus(computer);

    // deallocate stuff.

    delete osd;
    delete computer;
    switch (platform->mmu_type) {
        case MMU_MMU_II:
            delete mmu_ii;
            break;
        case MMU_MMU_IIE:
            delete mmu_iie;
            break;
    }
    delete select_system;
    delete aa;
    }
    //SDL_Delay(1000); 
    SDL_Quit();
    return 0;
}

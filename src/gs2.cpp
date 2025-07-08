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

/**
 * TODO: Decide whether to implement the 'illegal' instructions. Maybe have that as a processor variant? the '816 does not implement them.
 * 
 */

/** Globals we haven't dealt properly with yet. */
OSD *osd = nullptr;

#if 0
cpu_state *CPUs[MAX_CPUS];
void init_cpus() { // this is the same as a power-on event.
    for (int i = 0; i < MAX_CPUS; i++) {
        CPUs[i] = new cpu_state();
        CPUs[i]->init();
    }
}
#endif

void run_cpus(computer_t *computer) {
    cpu_state *cpu = computer->cpu;

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

    uint64_t last_time_window_start = 0;
    uint64_t last_cycle_window_start = 0;

    while (1) {
        uint64_t cycle_window_start = cpu->cycles;
        uint64_t cycle_window_delta = cycle_window_start - last_cycle_window_start;

        uint64_t last_cycle_count = cpu->cycles;
        uint64_t last_cycle_time = SDL_GetTicksNS();

        uint64_t cycles_for_this_burst = clock_mode_info[cpu->clock_mode].cycles_per_burst;

        if (! cpu->halt) {
            switch (cpu->execution_mode) {
                    case EXEC_NORMAL:
                        {
                        if (computer->debug_window->window_open) {
                            uint64_t end_frame_cycles = cpu->cycles + cycles_for_this_burst;
                            while (cpu->cycles < end_frame_cycles) { // 1/60th second.
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
                        } else { // skip all debug checks if the window is not open - this may seem repetitioius but it saves all kinds of cycles where every cycle counts (GO FAST MODE)
                            uint64_t end_frame_cycles = cpu->cycles + cycles_for_this_burst;
                            while (cpu->cycles < end_frame_cycles) { // 1/60th second.
                                if (computer->event_timer->isEventPassed(cpu->cycles)) {
                                    computer->event_timer->processEvents(cpu->cycles);
                                }
                                (cpu->cpun->execute_next)(cpu);
                            }
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

        bool this_free_run = (cpu->clock_mode == CLOCK_FREE_RUN) || (cpu->execution_mode == EXEC_STEP_INTO || (gs2_app_values.disk_accelerator && (any_diskii_motor_on(cpu))));

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
            bool diskii_run = any_diskii_motor_on(cpu);
            soundeffects_update(diskii_run, diskii_tracknumber_on(cpu));

            event_time = SDL_GetTicksNS() - current_time;
            last_event_update = current_time;
        }

        /* Emit Audio Frame */
        current_time = SDL_GetTicksNS();
        if ((this_free_run) && (current_time - last_audio_update > 16667000)
            || (!this_free_run)) {            
            audio_generate_frame(cpu, last_cycle_window_start, cycle_window_start);
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
            update_flash_state(cpu); // TODO: this goes into display.cpp frame handler.
            computer->video_system->update_display();    
            osd->render();
            computer->debug_window->render();
            computer->video_system->present();
            display_time = SDL_GetTicksNS() - current_time;
            last_display_update = current_time;
        }

        /* Emit 5-second Stats */
        current_time = SDL_GetTicksNS();
        if (current_time - last_5sec_update > 5000000000) {
            uint64_t delta = cpu->cycles - last_5sec_cycles;
            cpu->e_mhz = (float)delta / float(5000000);

            fprintf(stdout, "%llu delta %llu cycles clock-mode: %d CPS: %f MHz [ slips: %llu, busy: %llu, sleep: %llu]\n", delta, cpu->cycles, cpu->clock_mode, cpu->e_mhz, cpu->clock_slip, cpu->clock_busy, cpu->clock_sleep);
            fprintf(stdout, "event_time: %10llu, audio_time: %10llu, display_time: %10llu, app_event_time: %10llu, total: %10llu\n", event_time, audio_time, display_time, app_event_time, event_time + audio_time + display_time + app_event_time);
            fprintf(stdout, "PC: %04X, A: %02X, X: %02X, Y: %02X, P: %02X\n", cpu->pc, cpu->a, cpu->x, cpu->y, cpu->p);
            last_5sec_cycles = cpu->cycles;
            last_5sec_update = current_time;
        }

        if (cpu->halt == HLT_USER) {
            computer->video_system->update_display(); // update one last time to show the last state.
            break;
        }

        // calculate what sleep-until time should be.
        uint64_t wakeup_time = last_cycle_time + (cpu->cycles - last_cycle_count) * cpu->cycle_duration_ns;

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

        //printf("event_time / audio time / display time / total time: %9llu %9llu %9llu %9llu\n", event_time, audio_time, display_time, event_time + audio_time + display_time);

        last_cycle_count = cpu->cycles;
        last_cycle_time = SDL_GetTicksNS();

        //last_time_window_start = time_window_start;
        last_cycle_window_start = cycle_window_start;
    }
    cpu->trace_buffer->save_to_file(gs2_app_values.pref_path + "trace.bin");
}

gs2_app_t gs2_app_values;

int main(int argc, char *argv[]) {
    std::cout << "Booting GSSquared!" << std::endl;

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
                case 'x':
                    gs2_app_values.disk_accelerator = true;
                    break;
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
    platform_id = select_system->select();
    if (platform_id == -1) {
        delete select_system;
        delete aa;
        delete computer;
        break;
    }
// load platform roms - this info should get stored in the 'computer'
    platform_info* platform = get_platform(platform_id);
    print_platform_info(platform);
    computer->set_platform(platform);
    
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

    init_display_font(rd);

    SystemConfig_t *system_config = get_system_config(platform_id);

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
    SDL_Delay(1000); 
    SDL_Quit();
    return 0;
}

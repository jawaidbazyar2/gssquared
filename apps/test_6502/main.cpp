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
#include <sstream>
#include <iomanip>
#include <time.h>
#include <mach/mach_time.h>
#include <getopt.h>

#include "gs2.hpp"
#include "cpu.hpp"
#include "clock.hpp"
#include "memory.hpp"
#include "display/display.hpp"
#include "opcodes.hpp"
#include "debug.hpp"
#include "test.hpp"
#include "display/text_40x24.hpp"
#include "event_poll.hpp"
#include "devices/keyboard/keyboard.hpp"
#include "devices/speaker/speaker.hpp"
#include "devices/loader.hpp"
#include "platforms.hpp"

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


/**
 * initialize memory
 */
void init_default_memory_map(cpu_state *cpu) {

    for (int i = 0; i < (RAM_KB / GS2_PAGE_SIZE); i++) {
        cpu->memory->page_info[i + 0x00].type = MEM_RAM;
        cpu->memory->page_info[i + 0x00].can_read = 1;
        cpu->memory->page_info[i + 0x00].can_write = 1;
        cpu->memory->pages_read[i + 0x00] = cpu->main_ram_64 + i * GS2_PAGE_SIZE;
        cpu->memory->pages_write[i + 0x00] = cpu->main_ram_64 + i * GS2_PAGE_SIZE;
    }
    for (int i = 0; i < (IO_KB / GS2_PAGE_SIZE); i++) {
        cpu->memory->page_info[i + 0xC0].type = MEM_IO;
        cpu->memory->page_info[i + 0xC0].can_read = 1;
        cpu->memory->page_info[i + 0xC0].can_write = 1;
        cpu->memory->pages_read[i + 0xC0] = cpu->main_io_4 + i * GS2_PAGE_SIZE;
        cpu->memory->pages_write[i + 0xC0] = cpu->main_io_4 + i * GS2_PAGE_SIZE;
    }
    for (int i = 0; i < (ROM_KB / GS2_PAGE_SIZE); i++) {
        cpu->memory->page_info[i + 0xD0].type = MEM_ROM;
        cpu->memory->page_info[i + 0xD0].can_read = 1;
        cpu->memory->page_info[i + 0xD0].can_write = 0;
        cpu->memory->pages_read[i + 0xD0] = cpu->main_rom_D0 + i * GS2_PAGE_SIZE;
        cpu->memory->pages_write[i + 0xD0] = cpu->main_rom_D0 + i * GS2_PAGE_SIZE;
    }
}
void init_memory(cpu_state *cpu) {
    cpu->memory = new memory_map();
    
    cpu->main_ram_64 = new uint8_t[RAM_KB];
    cpu->main_io_4 = new uint8_t[IO_KB];
    cpu->main_rom_D0 = new uint8_t[ROM_KB];

    #ifdef APPLEIIGS
    for (int i = 0; i < RAM_SIZE / GS2_PAGE_SIZE; i++) {
        cpu->memory->page_info[i].type = MEM_RAM;
        cpu->memory->pages[i] = new memory_page(); /* do we care if this is aligned */
        if (!cpu->memory->pages[i]) {
            std::cerr << "Failed to allocate memory page " << i << std::endl;
            exit(1);
        }
    }
    #else
    init_default_memory_map(cpu);
    #endif
/*     for (int i = 0; i < 256; i++) {
        printf("page %02X: %p\n", i, cpu->memory->pages[i]);
    } */
}

uint64_t get_current_time_in_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void init_cpus() { // this is the same as a power-on event.
    for (int i = 0; i < MAX_CPUS; i++) {
        init_memory(&CPUs[i]);

        CPUs[i].boot_time = get_current_time_in_microseconds();
        CPUs[i].pc = 0x0400;
        CPUs[i].sp = rand() & 0xFF; // simulate a random stack pointer
        CPUs[i].a = 0;
        CPUs[i].x = 0;
        CPUs[i].y = 0;
        CPUs[i].p = 0;
        CPUs[i].cycles = 0;
        //CPUs[i].last_tick = 0;

        //CPUs[i].execute_next = &execute_next_6502;

        set_clock_mode(&CPUs[i], CLOCK_1_024MHZ);
    }
}

void set_cpu_processor(cpu_state *cpu, int processor_type) {
    cpu->execute_next = processor_models[processor_type].execute_next;
}

void run_cpus(void) {
    cpu_state *cpu = &CPUs[0];

    uint64_t last_display_update = 0;
    uint64_t last_5sec_update = 0;
    uint64_t last_5sec_cycles;

    while (1) {
        if ((cpu->execute_next)(cpu) > 0) {
            break;
        }
        
        uint64_t current_time = get_current_time_in_microseconds();
#if 0
        if (current_time - last_display_update > 16667) {
            event_poll(cpu); // they say call "once per frame"
            update_flash_state(cpu);
            update_display(cpu); // check for events 60 times per second.
            last_display_update = current_time;
        }
#endif
        if (current_time - last_5sec_update > 5000000) {
            uint64_t delta = cpu->cycles - last_5sec_cycles;
            fprintf(stdout, "%llu delta %llu cycles clock-mode: %d CPS: %f MHz [ slips: %llu, busy: %llu]\n", delta, cpu->cycles, cpu->clock_mode, (float)delta / float(5000000) , cpu->clock_slip, cpu->clock_busy);
            last_5sec_cycles = cpu->cycles;
            last_5sec_update = current_time;
        }

        if (cpu->halt) {
            update_display(cpu); // update one last time to show the last state.
            break;
        }
    }
}


int main(int argc, char *argv[]) {
    std::cout << "Booting GSSquared!" << std::endl;

    int platform_id = 1;  // default to Apple II Plus
    int opt;
    
#if 0
    while ((opt = getopt(argc, argv, "p:a:b:")) != -1) {
        switch (opt) {
            case 'p':
                platform_id = atoi(optarg);
                break;
            case 'a':
                loader_set_file_info(optarg, 0x0801);
                break;
            case 'b':
                loader_set_file_info(optarg, 0x7000);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p platform] [-a program.bin] [-b loader.bin]\n", argv[0]);
                exit(1);
        }
    }
#endif

    init_cpus();

        // read file 6502_65C02_functional_tests/bin_files/6502_functional_test.bin into a 64k byte array

        uint8_t *memory_chunk = (uint8_t *)malloc(65536); // Allocate 64KB memory chunk
        if (memory_chunk == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            exit(1);
        }

        FILE* file = fopen("6502_65C02_functional_tests/bin_files/6502_functional_test.bin", "rb");
        if (file == NULL) {
            fprintf(stderr, "Failed to open file\n");
            exit(1);
        }
        fread(memory_chunk, 1, 65536, file);
        fclose(file);
        for (uint64_t i = 0; i < 65536; i++) {
            raw_memory_write(&CPUs[0], i, memory_chunk[i]);
        }


    set_cpu_processor(&CPUs[0], PROCESSOR_6502);

#if 0
    if (init_display_sdl(rd)) {
        fprintf(stderr, "Error initializing display\n");
        exit(1);
    }

    init_keyboard();
    init_device_display();
    init_speaker(&CPUs[0]);
    init_thunderclock(1);
    diskII_register_slot(&CPUs[0], 6); // put a disk II in slot 6
#endif

    cpu_reset(&CPUs[0]);

    run_cpus();

    printf("CPU halted: %d\n", CPUs[0].halt);
    if (CPUs[0].halt == HLT_INSTRUCTION) { // keep screen up and give user a chance to see the last state.
        printf("Press Enter to continue...");
        getchar();
    }

    //dump_full_speaker_event_log();

    delete CPUs[0].video_system;

    debug_dump_memory(&CPUs[0], 0x1230, 0x123F);
    return 0;
}

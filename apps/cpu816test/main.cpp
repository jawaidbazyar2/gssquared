/**
 * cputest
 * 
 * perform the 6502_65c02 test suite.
 */

/**
 * Combines: 
 * CPU module (65c816)
 * MMU: Base MMU with no Apple-II specific features.
 * 
 * To use: 
 * cd 6502_65c02_functional_tests/bin_files
 * /path/to/cputest [trace_on]
 * 
 * if trace_on is present and is 1, it will print a debug trace of the CPU operation.
 * Otherwise, it will execute the test and report the results. 
 * 
 * You may need to review the 6502_functional_test.lst file to understand the test suite, if it should fail.
 * on a test failure, the suite will execute an instruction that jumps to itself. Our main loop tests for this
 * condition and exits with the PC of the failed test.
 */
#include <cinttypes>
#include <SDL3/SDL.h>

#include "cpu.hpp"
#include "devices/videx/videx.hpp"
#include "gs2.hpp"
//#include "cpu.hpp"
#include "cpus/cpu_implementations.cpp"
#include "mmus/mmu.hpp"
#include "mmus/mmu_iigs.hpp"
#include "util/ResourceFile.hpp"
#include "NClock.hpp"

gs2_app_t gs2_app_values;

uint8_t memory[16*1024*1024];

uint64_t debug_level = 0;

/**
 * ------------------------------------------------------------------------------------
 * Fake old-style MMU functions. They are only sort-of MMU. They are a a mix of MMU and CPU functions (that alter the program counter)
 */
/* 
uint8_t read_memory(cpu_state *cpu, uint16_t address) {
    return memory[address];
};
void write_memory(cpu_state *cpu, uint16_t address, uint8_t value) {
    memory[address] = value;
}; */

void update_display(char *msgbuf, bool top = false) {
    // send this sequence to display: ESC[1;1H
    if (top) printf("\033[1;1H");

    printf("--------------------------------\n");
    for (int line = 0; line < 32; line++) {
        for (int col = 0; col < 32; col++) {
            uint8_t c = msgbuf[line*32 + col];
            if (c == 0) {
                c = ' ';
            }
            printf("%c", c);
        }
        printf("\n");
    }
    printf("--------------------------------\n");
    fflush(stdout);
}

/**
 * ------------------------------------------------------------------------------------
 * Main
 */

int main(int argc, char **argv) {
    bool failed = false;
    bool trace_on = false;
    bool display = false;
    processor_type cputype = PROCESSOR_65816;
    int testsuite = 0; // 0 = snes-test test, 1 = other test, 2 = even another test
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "trace") == 0) {
            trace_on = true;
        } else if (strcmp(argv[i], "65c02") == 0) {
            cputype = PROCESSOR_65C02;
        } else if (strcmp(argv[i], "65816e") == 0) {
            cputype = PROCESSOR_65816;
        } else if (strcmp(argv[i], "display") == 0) {
            display = true;
        } else {
            printf("usage: cpu816test [trace] [snes-test] [testsnes-test]\n");
        }
    }

    switch (testsuite) {
        case 0:
            printf("Running snes-test test suite\n");
            break;
        case 1:
            printf("Running 65c02 test suite\n");
            break;
        case 2:
            printf("Running decimal test suite\n");
            break;
        default:
            printf("Invalid test suite\n");
            exit(1);
    }
    switch (cputype) {
        case PROCESSOR_6502:
            printf("Running 6502\n");
            break;
        case PROCESSOR_65C02:
            printf("Running 65c02\n");
            break;
        case PROCESSOR_65816:
            printf("Running 65816\n");
            break;
        default:
            printf("Invalid CPU type\n");
            exit(1);
    }

    printf("Starting CPU test...\n");


    gs2_app_values.base_path = "./";
    gs2_app_values.pref_path = gs2_app_values.base_path;
    gs2_app_values.console_mode = false;

// create MMU, map all pages to our "ram"
    MMU *mmu = new MMU(16384*1024/256, 256);
    for (int i = 0; i < 16384*1024/256; i++) {
        mmu->map_page_both(i, &memory[i*256], "TEST RAM");
    }
    //mmu->dump_page_table();

    ResourceFile *rom;
    
    /* The 256K here is mapped using SNES LoROM. Each bank is:
       32K of I/O type stuff, then, 32K of ROM. 
       So the 256K file is 8 ROM segments, each 32K, each mapped into $8000 of the bank. */
    if (testsuite == 0) {
        rom = new ResourceFile("../snes-tests/cputest/cputest-full.sfc", READ_ONLY);
    } else {
        printf("Invalid CPU type\n");
        return 1;
    }
    rom->load();
    uint8_t *rom_data = rom->get_data();
    int rom_size = rom->size();
    printf("ROM size: %d\n", rom_size);
    uint32_t rom_offset = 0;
    for (int bank = 0; bank < 8; bank++) {
        for (int i = 0x8000; i <= 0xFFFF; i++) {
            mmu->write((bank * 0x10000) + i, rom_data[rom_offset]);
            rom_offset++;
        }
    }

    cpu_state *cpu = new cpu_state(cputype);

    //std::unique_ptr<BaseCPU> cpux = createCPU(cputype);
    NClock *clock = new NClock();
    cpu->cpun = createCPU(cputype, clock);
    if (!cpu->cpun) {
        printf("Failed to create CPU\n");
        return 1;
    }
    cpu->core = cpu->cpun.get();

    //cpu->set_processor(PROCESSOR_6502);
    cpu->trace = true; // trace_on; always trace.
    cpu->set_mmu(mmu);

    cpu->reset(); // reset the CPU - AFTER setting MMU - required to set additional registers etc on powerup/reset.

    uint64_t start_time = SDL_GetTicksNS();

    cpu->pc = 0x008000;

    char msgbuf[32*32] = { ' ' }; // 32 x 32 tiles ?
    int tile_index = 0;

    while (1) {
        uint32_t opc = cpu->full_pc;
        (cpu->core->execute_next)(cpu);

        if (trace_on) {
            char * trace_entry = cpu->trace_buffer->decode_trace_entry(&cpu->trace_entry);
            printf("%s\n", trace_entry);
        }

        if (display) {
            if (cpu->trace_entry.eaddr == 0x002116) {
                tile_index = cpu->trace_entry.data;
            }
    
            // the snes has a register at 0x002118 for outputting text
            if (cpu->trace_entry.eaddr == 0x002118) {
                msgbuf[tile_index++] = cpu->trace_entry.data;
                if (!trace_on) update_display(msgbuf, true);
            }
        }

        if (cpu->trace_entry.eaddr == 0x002116) {
            tile_index = cpu->trace_entry.data;
        }

        // the snes has a register at 0x002118 for outputting text
       /*  if (cpu->trace_entry.eaddr == 0x002118) {
            if (display) {
                msgbuf[tile_index++] = cpu->trace_entry.data;
                if (!trace_on) update_display(msgbuf, true);
            }
        } */
        if (cpu->full_pc == 0x0081A2) {
            failed = false;
            break;
        }

        if ( // PC doesn't change on mvn or mvp moves, don't break for those.
            (cpu->full_pc == opc) && 
            ((cpu->trace_entry.opcode != 0x54)  &&
            (cpu->trace_entry.opcode != 0x44))
        ) { 
            printf("Test failed at PC: %06X\n", cpu->full_pc);
            failed = true;
            break; 
        }
    }
    uint64_t end_time = SDL_GetTicksNS();
    if (display) update_display(msgbuf);

    uint64_t duration = end_time - start_time;
    printf("Test took %" PRIu64 " ns\n", duration);
    printf("Average 'cycle' time: %f ns\n", (double)duration / (double) clock->get_cycles());
    printf("Effective MHz: %f\n", 1'000'000'000 / ((double)duration / (double) clock->get_cycles()) / 1000000);

    if (!failed) {
        printf("Test passed!\n");
    } else {
        printf("Test failed!\n");
    }    
    return 0;
}

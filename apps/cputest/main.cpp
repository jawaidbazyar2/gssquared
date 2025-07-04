/**
 * cputest
 * 
 * perform the 6502_65c02 test suite.
 */

/**
 * Combines: 
 * CPU module (6502 or 65c02)
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
#include <SDL3/SDL.h>

#include "gs2.hpp"
//#include "cpu.hpp"
#include "cpus/cpu_implementations.cpp"
#include "mmus/mmu.hpp"
#include "util/ResourceFile.hpp"

gs2_app_t gs2_app_values;

uint8_t memory[65536];

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

/**
 * ------------------------------------------------------------------------------------
 * Main
 */

int main(int argc, char **argv) {
    bool trace_on = false;

    printf("Starting CPU test...\n");
    if (argc > 1) {
        trace_on = atoi(argv[1]);
    }

    gs2_app_values.base_path = "./";
    gs2_app_values.pref_path = gs2_app_values.base_path;
    gs2_app_values.console_mode = false;

// create MMU, map all pages to our "ram"
    MMU *mmu = new MMU(256);
    for (int i = 0; i < 256; i++) {
        mmu->map_page_both(i, &memory[i*256], "TEST RAM");
    }

    int cputype = 0;
    if (argc >= 2) {
        cputype = atoi(argv[1]);
    }

    ResourceFile *rom;
    
    if (cputype == 0) {
        rom = new ResourceFile("6502_functional_test.bin", READ_ONLY);
    } else if (cputype == 1) {
        rom = new ResourceFile("65C02_extended_opcodes_test.bin", READ_ONLY);
    } else {
        printf("Invalid CPU type\n");
        return 1;
    }
    rom->load();
    uint8_t *rom_data = rom->get_data();
    int rom_size = rom->size();
    printf("ROM size: %d\n", rom_size);
    for (int i = 0; i < rom_size; i++) {
        mmu->write(i, rom_data[i]);
    }

    std::unique_ptr<BaseCPU> cpux = createCPU(cputype == 1? "65C02" : "6502");
    if (!cpux) {
        printf("Failed to create CPU\n");
        return 1;
    }

    cpu_state *cpu = new cpu_state();
    //cpu->set_processor(PROCESSOR_6502);
    cpu->trace = trace_on;
    cpu->set_mmu(mmu);

    uint64_t start_time = SDL_GetTicksNS();
    
    while (1) {
        uint16_t opc = cpu->pc;
        (cpux->execute_next)(cpu);

        if (trace_on) {
                char * trace_entry = cpu->trace_buffer->decode_trace_entry(&cpu->trace_entry);
                printf("%s\n", trace_entry);
        }

        if (cpu->pc == opc) {
            break;
        }
    }
    uint64_t end_time = SDL_GetTicksNS();

    uint64_t duration = end_time - start_time;
    printf("Test took %llu ns\n", duration);
    printf("Average 'cycle' time: %f ns\n", (double)duration / (double) cpu->cycles);
    printf("Effective MHz: %f\n", 1'000'000'000 / ((double)duration / (double) cpu->cycles) / 1000000);

    if (cpu->pc == 0x3469 || cpu->pc == 0x23BC) {
        printf("Test passed!\n");
    } else {
        printf("Test failed!\n");
    }
    
    return 0;
}

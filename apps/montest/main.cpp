#include <SDL3/SDL.h>
#include "debugger/Monitor.hpp"
#include <iostream>


#include "gs2.hpp"
#include "mmus/mmu.hpp"

gs2_app_t gs2_app_values;

uint8_t memory[65536];

uint64_t debug_level = 0;


/**
 * ------------------------------------------------------------------------------------
 * Main
 */

int main(int argc, char **argv) {

    printf("Starting monitor test...\n");

    gs2_app_values.base_path = "./";
    gs2_app_values.pref_path = gs2_app_values.base_path;
    gs2_app_values.console_mode = false;

    // create MMU, map all pages to our "ram"
    MMU *mmu = new MMU(256, GS2_PAGE_SIZE);
    for (int i = 0; i < 256; i++) {
        mmu->map_page_both(i, &memory[i*256], "TEST RAM");
    }

    Disassembler *disasm = new Disassembler(mmu, PROCESSOR_6502);
    std::vector<std::string> debug_displays;
    MemoryWatch watches;
    BreakpointTable breakpoints;

    Monitor monitor;
    monitor.bind(mmu, &watches, &breakpoints, disasm, &debug_displays, nullptr);

    std::string command;
    while (1) {
        std::getline(std::cin, command);
        std::cout << "command: " << command << std::endl;

        const auto &output = monitor.execute(command);
        for (const auto &line : output) {
            std::cout << line << std::endl;
        }
    }
    return 0;
}

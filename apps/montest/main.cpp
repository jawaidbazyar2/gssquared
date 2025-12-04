#include <SDL3/SDL.h>
#include "debugger/MonitorCommand.hpp"
#include "debugger/ExecuteCommand.hpp"
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

    Disassembler *disasm = new Disassembler(mmu);
    std::vector<std::string> debug_displays;

    std::string command;
    while (1) {
        std::getline(std::cin, command);
        std::cout << "command: " << command << std::endl;

        MonitorCommand *cmd = new MonitorCommand(command);
        cmd->print();
        
        ExecuteCommand *exec = new ExecuteCommand(mmu, cmd, nullptr, nullptr, disasm, &debug_displays);
        exec->execute();
        
        // Print the output buffer to stdout (you can remove this or redirect as needed)
        const auto& output = exec->getOutput();
        for (const auto& line : output) {
            std::cout << line << std::endl;
        }
        
        delete exec;
        delete cmd;
    }
    return 0;
}

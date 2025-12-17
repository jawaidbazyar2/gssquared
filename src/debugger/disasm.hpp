#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "mmus/mmu.hpp"

class Disassembler {
    public:
        Disassembler(MMU *mmu);
        ~Disassembler();

        void setAddress(uint32_t address);
        std::vector<std::string> disassemble(int count);
        void disassemble_one();
        void setLinePrepend(int spaces) { line_prepend = spaces; }

    private:
        uint32_t address;
        uint16_t length;
        MMU *mmu;
        int line_prepend;
        std::vector<std::string> output_buffer;

        uint8_t read_mem(uint32_t address);
};
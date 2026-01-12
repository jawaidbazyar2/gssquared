#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "mmus/mmu.hpp"
#include "cpus/processor_type.hpp"


class Disassembler {
    public:
        Disassembler(MMU *mmu, processor_type cputype);
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
        processor_type cpu_type;
        
        uint8_t read_mem(uint32_t address);
};
#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "mmus/mmu.hpp"
#include "cpus/processor_type.hpp"
#include "debugger/trace.hpp"

class Disassembler {
    public:
        Disassembler(MMU *mmu, processor_type cputype);
        ~Disassembler();

        void setAddress(uint32_t address);
        std::vector<std::string> disassemble(int count);
        void disassemble_one();

        /** Seed assumed M/X widths (1 = 8-bit, 0 = 16-bit). Also used after SEP/REP while walking. */
        void set_mx(bool m_8bit, bool x_8bit);

        /** Match trace column layout and Bytes toggle for prospective / list output. */
        void set_format(const trace_column_layout &layout, bool show_opbytes);

    private:
        uint32_t address;
        MMU *mmu;
        std::vector<std::string> output_buffer;
        processor_type cpu_type;
        int16_t cpu_mask;
        bool m_8bit = true;
        bool x_8bit = true;
        trace_column_layout layout_{};
        bool show_opbytes_ = true;
        bool layout_set_ = false;

        uint8_t read_mem(uint32_t address);
        int resolve_insn_size(const disasm_entry *da, const address_mode_entry *am) const;
        void apply_sep_rep(const disasm_entry *da, uint8_t mask);
};

#include <cstring>

#include "mmus/mmu.hpp"
#include "debugger/disasm.hpp"
#include "debugger/trace_opcodes.hpp"
#include "debugger/trace_format.hpp"
#include "debugger/line_buffer.hpp"

Disassembler::Disassembler(MMU *mmu, processor_type cputype) : mmu(mmu), cpu_type(cputype) {
    address = 0;
    cpu_mask = 0;
    if (cputype == PROCESSOR_65816) {
        cpu_mask = CPU_65816;
    } else if (cputype == PROCESSOR_65C02) {
        cpu_mask = CPU_65C02;
    } else if (cputype == PROCESSOR_6502) {
        cpu_mask = CPU_6502;
    }
}

Disassembler::~Disassembler() {
}

void Disassembler::setAddress(uint32_t address) {
    this->address = address;
}

void Disassembler::set_mx(bool m_8bit, bool x_8bit) {
    this->m_8bit = m_8bit;
    this->x_8bit = x_8bit;
}

void Disassembler::set_format(const trace_column_layout &layout, bool show_opbytes) {
    layout_ = layout;
    show_opbytes_ = show_opbytes;
    layout_set_ = true;
}

uint8_t Disassembler::read_mem(uint32_t address) {
    if (address >= 0xC000 && address < 0xC0FF) {
        return 0xEE; // do not actually trigger I/O when we're trying to disassemble.
    }
    if ((address & 0xFF00) == 0xC400) { // TODO: this is a hack to prevent disassembler from changing mockingboard state. need better method.
        return 0xDD;
    }
    return mmu->read(address);
}

int Disassembler::resolve_insn_size(const disasm_entry *da, const address_mode_entry *am) const {
    if (cpu_type != PROCESSOR_65816) {
        return am->size;
    }
    if (da->flags & (DISASM_SEP | DISASM_REP)) {
        return 2; // always opcode + 8-bit mask
    }
    if (da->flags & DISASM_IMM_M) {
        return m_8bit ? 2 : 3;
    }
    if (da->flags & DISASM_IMM_X) {
        return x_8bit ? 2 : 3;
    }
    return am->size;
}

void Disassembler::apply_sep_rep(const disasm_entry *da, uint8_t mask) {
    if (da->flags & DISASM_SEP) {
        if (mask & 0x20) {
            m_8bit = true;
        }
        if (mask & 0x10) {
            x_8bit = true;
        }
    } else if (da->flags & DISASM_REP) {
        if (mask & 0x20) {
            m_8bit = false;
        }
        if (mask & 0x10) {
            x_8bit = false;
        }
    }
}

void Disassembler::disassemble_one() {
    line_buffer buffer;
    buffer.reset();

    // Fallback: PC at column 0 with Bytes/Op/Operand spacing (montest / no set_format).
    trace_column_layout layout = layout_;
    if (!layout_set_) {
        const bool is_65816 = (cpu_type == PROCESSOR_65816);
        // cycle_w,a,x,y,sp,p unused when PC starts at 0
        if (is_65816) {
            layout = {0, 0, 0, 0, 0, 0, /*pc*/ 0, /*opbytes*/ 9, /*opcode*/ 21, /*operand*/ 26,
                      0, 0, 0, 0, 12};
        } else {
            layout = {0, 0, 0, 0, 0, 0, /*pc*/ 0, /*opbytes*/ 6, /*opcode*/ 18, /*operand*/ 23,
                      0, 0, 0, 0, 12};
        }
    }

    const bool is_65816 = (cpu_type == PROCESSOR_65816);
    uint8_t opcode = read_mem(address);
    const disasm_entry *da = &disasm_table[opcode];
    const address_mode_entry *am = &address_mode_formats[da->mode];

    int insn_size = resolve_insn_size(da, am);
    uint8_t op_sz = 1;
    if (da->mode == IMM) {
        op_sz = (insn_size == 3) ? 2 : 1;
    }

    uint32_t operand = 0;
    for (int i = 1; i < insn_size; i++) {
        uint8_t b = read_mem(address + i);
        operand |= ((uint32_t)b) << ((i - 1) * 8);
    }

    uint8_t pb = (uint8_t)((address >> 16) & 0xFF);
    uint16_t pc = (uint16_t)(address & 0xFFFF);

    emit_insn_pc(buffer, layout, is_65816, pb, pc);
    emit_insn_opbytes(buffer, layout, show_opbytes_, opcode, operand, insn_size);
    emit_insn_mnemonic_operand(buffer, layout, da, am, cpu_mask, pb, pc, operand, op_sz, nullptr);

    if (da->flags & (DISASM_SEP | DISASM_REP)) {
        apply_sep_rep(da, (uint8_t)(operand & 0xFF));
    }

    address += insn_size;
    output_buffer.push_back(buffer.get());
}

std::vector<std::string> Disassembler::disassemble(int count) {
    for (int i = 0; i < count; i++) {
        disassemble_one();
    }
    std::vector<std::string> ret(std::move(output_buffer));
    output_buffer.clear();
    return ret;
}

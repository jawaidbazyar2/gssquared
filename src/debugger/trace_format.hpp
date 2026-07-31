#pragma once

#include <cstdint>

#include "debugger/line_buffer.hpp"
#include "debugger/trace.hpp"
#include "debugger/trace_opcodes.hpp"

/** Emit PB/PC or PC at layout.pc, followed by ": ". */
void emit_insn_pc(line_buffer &buffer, const trace_column_layout &layout, bool is_65816,
                  uint8_t pb, uint16_t pc);

/**
 * Emit opcode + operand bytes at layout.opbytes.
 * insn_size is total instruction length in bytes (including opcode).
 */
void emit_insn_opbytes(line_buffer &buffer, const trace_column_layout &layout, bool show_opbytes,
                       uint8_t opcode, uint32_t operand, int insn_size);

/**
 * Emit mnemonic + operand text at layout.opcode / layout.operand.
 * op_sz is IMM operand byte count (1 or 2); ignored for non-IMM.
 * If branch_target_out is non-null, REL/REL_L store a 24-bit (pb<<16)|target.
 */
void emit_insn_mnemonic_operand(line_buffer &buffer, const trace_column_layout &layout,
                                const disasm_entry *da, const address_mode_entry *am,
                                int16_t cpu_mask, uint8_t pb, uint16_t pc, uint32_t operand,
                                uint8_t op_sz, uint32_t *branch_target_out);

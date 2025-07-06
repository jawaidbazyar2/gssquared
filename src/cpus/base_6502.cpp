/*
 *   Copyright (c) 2025 Jawaid Bazyar

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


#include <cstdint>
#include <cstdio>

#include "cpu.hpp"

#include "opcodes.hpp"

#include "cpu_traits.hpp"

/**
 * This file is 6502 / 65c02 instruction processing.
 * 
 * This file is a template that is used in two places, to instantiate a 6502 or a 65c02, based on 
 * Traits passed into the template. 98% of the architecture is identical.
 * 
 * The end result is two modules, one with the complete 6502 code in it,
 * one with the complete 65c02 code in it. No runtime if-then is done at
 * the instruction level. Only at the CPU module level.
 * 
 * It will eventually be used one more time in this same manner, which is a
 * version for the 65c816 in 8-bit "emulation mode". It will have a few different
 * instructions yet from the 6502 / 65c02. And then there will need to be something
 * very different for the 16-bit mode stuff.
 */

/**
 * References: 
 * Apple Machine Language: Don Inman, Kurt Inman
 * https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
 * https://www.masswerk.at/6502/6502_instruction_set.html#USBC
 * 
 */

/**
 * TODO: Decide whether to implement the 'illegal' instructions. Maybe have that as a processor variant? the '816 does not implement them.
 * 
 */


template<typename CPUTraits>
class CPU6502Core : public BaseCPU {

private:

/**
 * This group of routines implement reused addressing mode calculations.
 *
 */

/**
 * Set the N and Z flags based on the value. Used by all arithmetic instructions, as well
 * as load instructions.
 */
inline void set_n_z_flags(cpu_state *cpu, uint8_t value) {
    cpu->Z = (value == 0);
    cpu->N = (value & 0x80) != 0;
}

inline void set_n_z_v_flags(cpu_state *cpu, uint8_t value, uint8_t N) {
    cpu->Z = (value == 0); // is A & M zero?
    cpu->N = (N & 0x80) != 0; // is M7 set?
    cpu->V = (N & 0x40) != 0; // is M6 set?
}

/* only set Z flag - used by some 65c02 instructions */
inline void set_z_flag(cpu_state *cpu, uint8_t value) {
    cpu->Z = (value == 0);
}

/**
 * Convert a binary-coded decimal byte to an integer.
 * Each nibble represents a decimal digit (0-9).
 */
inline uint8_t bcd_to_int(byte_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * Convert an integer (0-99) to binary-coded decimal format.
 * Returns a byte with each nibble representing a decimal digit.
 */
inline byte_t int_to_bcd(uint8_t n) {
    uint8_t n1 = n % 100;
    return ((n1 / 10) << 4) | (n1 % 10);
}


/**
 * perform accumulator addition. M is current value of accumulator. 
 * cpu: cpu flag
 * N: number being added to accumulator
 */
inline void add_and_set_flags(cpu_state *cpu, uint8_t N) {
    if (cpu->D == 0) {  // binary mode
        uint8_t M = cpu->a_lo;
        uint32_t S = M + N + (cpu->C);
        uint8_t S8 = (uint8_t) S;
        cpu->a_lo = S8;
        cpu->C = (S & 0x0100) >> 8;
        cpu->V =  !((M ^ N) & 0x80) && ((M ^ S8) & 0x80); // from 6502.org article https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
        set_n_z_flags(cpu, cpu->a_lo);
    } else {              // decimal mode
        uint8_t M = bcd_to_int(cpu->a_lo);
        uint8_t N1 = bcd_to_int(N);
        uint8_t S8 = M + N1 + cpu->C;
        cpu->a_lo = int_to_bcd(S8);
        cpu->C = (S8 > 99);
        if constexpr (CPUTraits::has_65c02_ops) {
            // TODO: handle V flag
            incr_cycles(cpu);
        }
        set_n_z_flags(cpu, cpu->a_lo);
    }
}

// see https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
// "Subtraction on the 6502" section.

/* Does subtraction based on 3 inputs: M, N, and C. 
 * Sets flags, returns the result of the subtraction. But does not store it in the accumulator.
 * Caller may store the result in the accumulator if desired.
 */
inline uint8_t subtract_core(cpu_state *cpu, uint8_t M, uint8_t N, uint8_t C) {
    uint8_t N1 = N ^ 0xFF;
    uint32_t S = M + N1 + C;
    uint8_t S8 = (uint8_t) S;
    cpu->C = (S & 0x0100) >> 8;
    cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
    set_n_z_flags(cpu, S8);
    return S8;
}

inline void subtract_and_set_flags(cpu_state *cpu, uint8_t N) {
    uint8_t C = cpu->C;

    if (cpu->D == 0) {
        uint8_t M = cpu->a_lo;

        uint8_t N1 = N ^ 0xFF;
        uint32_t S = M + N1 + C;
        uint8_t S8 = (uint8_t) S;
        cpu->C = (S & 0x0100) >> 8;
        cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
        cpu->a_lo = S8; // store the result in the accumulator. I accidentally deleted this before.
        set_n_z_flags(cpu, S8);
    } else {
        uint8_t M = bcd_to_int(cpu->a_lo);
        uint8_t N1 = bcd_to_int(N);
        int8_t S8 = M - N1 - !C;
        if (S8 < 0) {
            S8 += 100;
            cpu->C = 0; // c = 0 indicates a borrow in subtraction
        } else {
            cpu->C = 1;
        }
        cpu->a_lo = int_to_bcd(S8);
        if constexpr (CPUTraits::has_65c02_ops) {
            // TODO: handle V flag
            incr_cycles(cpu);
        }
        set_n_z_flags(cpu, cpu->a_lo);
    }
}

/** nearly Identical algorithm to subtraction mostly.
 * Compares are the same as SBC with the carry flag assumed to be 1.
 * does not store the result in the accumulator
 * ALSO DOES NOT SET the V flag
 */
inline void compare_and_set_flags(cpu_state *cpu, uint8_t M, uint8_t N) {
    uint8_t C = 1;
    uint8_t N1 = N ^ 0xFF;
    uint32_t S = M + N1 + C;
    uint8_t S8 = (uint8_t) S;
    cpu->C = (S & 0x0100) >> 8;
    //cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
    set_n_z_flags(cpu, S8);
}

/**
 * Good discussion of branch instructions:
 * https://www.masswerk.at/6502/6502_instruction_set.html#BCC
 */
inline void branch_if(cpu_state *cpu, uint8_t N, bool condition) {
    uint16_t oaddr = cpu->pc;
    uint16_t taddr = oaddr + (int8_t) N;

    if (condition) {
        if ((oaddr-2) == taddr) { // this test should back up 2 bytes.. so the infinite branch is actually bxx FE.
            fprintf(stdout, "JUMP TO SELF INFINITE LOOP Branch $%04X -> %01X\n", taddr, condition);
            cpu->halt = HLT_INSTRUCTION;
        }

        cpu->pc = cpu->pc + (int8_t) N;
        // branch taken uses another clock to update the PC
        incr_cycles(cpu); 
        /* If a branch is taken and the target is on a different page, this adds another CPU cycle (4 in total). */
        if ((oaddr & 0xFF00) != (taddr & 0xFF00)) {
            incr_cycles(cpu);
        }
    }
}


/**
 * Loading data for various addressing modes.
 * TODO: on ,X and ,Y we need to add cycle if a page boundary is crossed. (DONE)
 */

/** First, these methods (get_operand_address_*) read the type of operand from the PC,
 *  then read the operand from memory.
 * These are here because some routines read modify write, and we don't need to 
 * replicate that calculation.
 * */

/* there is no get_operand_address_immediate because functions below read the value itself from the PC */

/* These also generate the disassembly output - the operand address and mode */
inline zpaddr_t get_operand_address_zeropage(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = zpaddr; )
    return zpaddr;
}

inline zpaddr_t get_operand_address_zeropage_x(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    zpaddr_t taddr = zpaddr + cpu->x_lo; // make sure it wraps.
    incr_cycles(cpu); // ZP,X adds a cycle.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline zpaddr_t get_operand_address_zeropage_y(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    zpaddr_t taddr = zpaddr + cpu->y_lo; // make sure it wraps.
    // TODO: if it wraps add another cycle.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline absaddr_t get_operand_address_absolute(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = addr; )
    return addr;
}

inline absaddr_t get_operand_address_absolute_indirect(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    absaddr_t taddr = cpu->read_word(addr);
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline absaddr_t get_operand_address_absolute_indirect_x(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    absaddr_t taddr = cpu->read_word((uint16_t)(addr + cpu->x_lo));
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline absaddr_t get_operand_address_absolute_x(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    absaddr_t taddr = addr + cpu->x_lo;
    if ((addr & 0xFF00) != (taddr & 0xFF00)) {
        incr_cycles(cpu);
    }
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

// current working version
/* inline absaddr_t get_operand_address_absolute_x_rmw(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc(); // T1,T2 // TODO: is the order of reading the PC correct?
    absaddr_t taddr = addr + cpu->x_lo; // T3
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}
 */

inline absaddr_t get_operand_address_absolute_x_rmw(cpu_state *cpu) {
    addr_t ba;
    ba.al = cpu->read_byte_from_pc();      // T1

    ba.ah = cpu->read_byte_from_pc();      // T2

    addr_t ad;
    ad.al = (ba.al + cpu->x_lo);
    ad.ah = (ba.ah);
    cpu->read_byte( ad.a );                // T3 

    absaddr_t taddr = ba.a + cpu->x_lo;    // first part of T4, caller has to do the data fetch

    TRACE(cpu->trace_entry.operand = ba.a; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline absaddr_t get_operand_address_absolute_y(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    absaddr_t taddr = addr + cpu->y_lo;
    if ((addr & 0xFF00) != (taddr & 0xFF00)) {
        incr_cycles(cpu);
    }
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

// 65c02 only address mode
inline uint16_t get_operand_address_zeropage_indirect(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    absaddr_t taddr = cpu->read_word((uint8_t)zpaddr); // make sure it wraps.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
}

inline uint16_t get_operand_address_indirect_x(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    absaddr_t taddr = cpu->read_word((uint8_t)(zpaddr + cpu->x_lo)); // make sure it wraps.
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
}

inline absaddr_t get_operand_address_indirect_y(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    absaddr_t iaddr = cpu->read_word(zpaddr);
    absaddr_t taddr = iaddr + cpu->y_lo;

    if ((iaddr & 0xFF00) != (taddr & 0xFF00)) {
        incr_cycles(cpu);
    }
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
}

/** Second, these methods (get_operand_*) read the operand value from memory. */

inline byte_t get_operand_immediate(cpu_state *cpu) {
    byte_t N = cpu->read_byte_from_pc();
    TRACE(cpu->trace_entry.operand = N;)
    return N;
}

inline byte_t get_operand_zeropage(cpu_state *cpu) {
    zpaddr_t addr = get_operand_address_zeropage(cpu);
    byte_t N = cpu->read_byte(addr);
    TRACE(cpu->trace_entry.eaddr = addr; cpu->trace_entry.operand = addr; cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage(cpu_state *cpu, byte_t N) {
    zpaddr_t addr = get_operand_address_zeropage(cpu);
    cpu->write_byte(addr, N);
    TRACE(cpu->trace_entry.eaddr = addr; cpu->trace_entry.data = N;)
}

inline byte_t get_operand_zeropage_x(cpu_state *cpu) {
    zpaddr_t addr = get_operand_address_zeropage_x(cpu);
    byte_t N = cpu->read_byte(addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage_x(cpu_state *cpu, byte_t N) {
    zpaddr_t addr = get_operand_address_zeropage_x(cpu);
    cpu->write_byte(addr, N);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_zeropage_y(cpu_state *cpu) {
    zpaddr_t zpaddr = get_operand_address_zeropage_y(cpu);
    byte_t N = cpu->read_byte(zpaddr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage_y(cpu_state *cpu, byte_t N) {
    zpaddr_t zpaddr = get_operand_address_zeropage_y(cpu);
    cpu->write_byte(zpaddr, N);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_zeropage_indirect_x(cpu_state *cpu) {
    absaddr_t taddr = get_operand_address_indirect_x(cpu);
    byte_t N = cpu->read_byte(taddr);
    TRACE( cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage_indirect_x(cpu_state *cpu, byte_t N) {
    absaddr_t taddr = get_operand_address_indirect_x(cpu);
    cpu->write_byte(taddr, N);
    TRACE( cpu->trace_entry.data = N;)
}

inline byte_t get_operand_zeropage_indirect_y(cpu_state *cpu) {
    absaddr_t addr = get_operand_address_indirect_y(cpu);
    byte_t N = cpu->read_byte(addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage_indirect_y(cpu_state *cpu, byte_t N) {
    absaddr_t addr = get_operand_address_indirect_y(cpu);
    incr_cycles(cpu); // TODO: where should this extra cycle actually go?
    cpu->write_byte(addr, N);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_absolute(cpu_state *cpu) {
    absaddr_t addr = get_operand_address_absolute(cpu);
    byte_t N = cpu->read_byte(addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_absolute(cpu_state *cpu, byte_t N) {
    absaddr_t addr = get_operand_address_absolute(cpu);
    cpu->write_byte(addr, N);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_absolute_x(cpu_state *cpu) {
    absaddr_t addr = get_operand_address_absolute_x(cpu);
    byte_t N = cpu->read_byte(addr);
    //if (DEBUG(DEBUG_OPCODE)) fprintf(stdout, "   [#%02X] <- $%04X", N, addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_absolute_x(cpu_state *cpu, byte_t N) {
    absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
    cpu->write_byte(addr, N);
    //if (DEBUG(DEBUG_OPCODE)) fprintf(stdout, "   [#%02X] -> $%04X", N, addr);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_absolute_y(cpu_state *cpu) {
    absaddr_t addr = get_operand_address_absolute_y(cpu);
    byte_t N = cpu->read_byte(addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_absolute_y(cpu_state *cpu, byte_t N) {
    absaddr_t addr = get_operand_address_absolute_y(cpu);
    incr_cycles(cpu); // TODO: where should this extra cycle actually go?
    cpu->write_byte(addr, N);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t get_operand_relative(cpu_state *cpu) {
    byte_t N = cpu->read_byte_from_pc();
    TRACE(cpu->trace_entry.operand = N;)
    return N;
}


inline void op_transfer_to_x(cpu_state *cpu, byte_t N) {
    cpu->x_lo = N;
    set_n_z_flags(cpu, cpu->x_lo);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_y(cpu_state *cpu, byte_t N) {
    cpu->y_lo = N;
    set_n_z_flags(cpu, cpu->y_lo);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_a(cpu_state *cpu, byte_t N) {
    cpu->a_lo = N;
    set_n_z_flags(cpu, cpu->a_lo);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_s(cpu_state *cpu, byte_t N) {
    cpu->sp = N;
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

/**
 * Decrement an operand.
 */
inline void dec_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    N--;
    incr_cycles(cpu);
    cpu->write_byte(addr, N);
    set_n_z_flags(cpu, N);
    TRACE( cpu->trace_entry.data = N;)
}

/**
 * Increment an operand.
 */
/* inline void inc_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr); // in abs,x this is T4
    N++;
    incr_cycles(cpu);
    cpu->write_byte(addr, N);
    set_n_z_flags(cpu, N);
    //if (DEBUG(DEBUG_OPCODE)) fprintf(stdout, "   [#%02X]", N);
    TRACE(cpu->trace_entry.data = N;)
} */

inline void inc_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr); // in abs,x this is completion of T4
    
    // T5 write original value
    cpu->write_byte(addr,N); 
    N++;
    
    // T6 write new value
    set_n_z_flags(cpu, N);
    cpu->write_byte(addr, N);

    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t logical_shift_right(cpu_state *cpu, byte_t N) {
    uint8_t C = N & 0x01;
    N = N >> 1;
    cpu->C = C;
    set_n_z_flags(cpu, N);
    incr_cycles(cpu);
    return N;
}

inline byte_t logical_shift_right_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    byte_t result = logical_shift_right(cpu, N);
    cpu->write_byte(addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
}

inline byte_t arithmetic_shift_left(cpu_state *cpu, byte_t N) {
    uint8_t C = (N & 0x80) >> 7;
    N = N << 1;
    cpu->C = C;
    set_n_z_flags(cpu, N);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline byte_t arithmetic_shift_left_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    byte_t result = arithmetic_shift_left(cpu, N);
    cpu->write_byte(addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
}


inline byte_t rotate_right(cpu_state *cpu, byte_t N) {
    uint8_t C = N & 0x01;
    N = N >> 1;
    N |= (cpu->C << 7);
    cpu->C = C;
    set_n_z_flags(cpu, N);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline byte_t rotate_right_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    byte_t result = rotate_right(cpu, N);
    cpu->write_byte(addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
}

inline byte_t rotate_left(cpu_state *cpu, byte_t N) {
    uint8_t C = ((N & 0x80) != 0);
    N = N << 1;
    N |= cpu->C;
    cpu->C = C;
    set_n_z_flags(cpu, N);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline byte_t rotate_left_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    byte_t result = rotate_left(cpu, N);
    cpu->write_byte(addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
}

inline void push_byte(cpu_state *cpu, byte_t N) {
    cpu->write_byte(0x0100 + cpu->sp, N);
    cpu->sp = (uint8_t)(cpu->sp - 1);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t pop_byte(cpu_state *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1);
    incr_cycles(cpu);
    byte_t N = cpu->read_byte(0x0100 + cpu->sp);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void push_word(cpu_state *cpu, word_t N) {
    cpu->write_byte(0x0100 + cpu->sp, (N & 0xFF00) >> 8);
    cpu->write_byte(0x0100 + cpu->sp - 1, N & 0x00FF);
    cpu->sp = (uint8_t)(cpu->sp - 2);
    //incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
}

inline absaddr_t pop_word(cpu_state *cpu) {
    absaddr_t N = cpu->read_word(0x0100 + cpu->sp + 1);
    cpu->sp = (uint8_t)(cpu->sp + 2);
    incr_cycles(cpu);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void invalid_opcode(cpu_state *cpu, opcode_t opcode) {
    fprintf(stdout, "Unknown opcode: %04X: 0x%02X", cpu->pc-1, ((unsigned int)opcode) & 0xFF);
    //cpu->halt = HLT_INSTRUCTION;
}

inline void invalid_nop(cpu_state *cpu, int bytes, int cycles) {
    cpu->pc += (bytes-1); // we already fetched the opcode, so just count bytes excl. that.
    cpu->cycles += (cycles-1);
    TRACE(cpu->trace_entry.data = 0;)
}

public:

int execute_next(cpu_state *cpu) override {

    system_trace_entry_t *tb = &cpu->trace_entry;
    TRACE(
    if (cpu->trace) {
    tb->cycle = cpu->cycles;
    tb->pc = cpu->pc;
    tb->a = cpu->a_lo;
    tb->x = cpu->x_lo;
    tb->y = cpu->y_lo;
    tb->sp = cpu->sp;
    tb->d = cpu->d;
    tb->p = cpu->p;
    tb->db = cpu->db;
    tb->pb = cpu->pb;
    tb->eaddr = 0;
    }
    )

#if 0
    if (DEBUG(DEBUG_CLOCK)) {
        uint64_t current_time = get_current_time_in_microseconds();
        fprintf(stdout, "[ %llu ]", cpu->cycles);
        uint64_t elapsed_time = current_time - cpu->boot_time;
        fprintf(stdout, "[eTime: %llu] ", elapsed_time);
        float cycles_per_second = (cpu->cycles * 1000000000.0) / (elapsed_time * 1000.0);
        fprintf(stdout, "[eHz: %.0f] ", cycles_per_second);
    }
#endif

    if (!cpu->I && cpu->irq_asserted) { // if IRQ is not disabled, and IRQ is asserted, handle it.
        push_word(cpu, cpu->pc); // push current PC
        push_byte(cpu, cpu->p | FLAG_UNUSED); // break flag and Unused bit set to 1.
        cpu->I = 1; // interrupt disable flag set to 1.
        if constexpr (CPUTraits::has_65c02_ops) {
            cpu->D = 0; // turn off decimal mode on brk and interrupts
        }
        cpu->pc = cpu->read_word(IRQ_VECTOR);
        incr_cycles(cpu);
        incr_cycles(cpu);
        return 0;
    }

    opcode_t opcode = cpu->read_byte_from_pc();
    tb->opcode = opcode;

    switch (opcode) {

        /* ADC --------------------------------- */
        case OP_ADC_IMM: /* ADC Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                add_and_set_flags(cpu, N);                    
            }
            break;

        case OP_ADC_ZP: /* ADC ZP */
            {
                byte_t N = get_operand_zeropage(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ZP_X: /* ADC ZP, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND: /* ADC (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                add_and_set_flags(cpu, cpu->read_byte(addr));
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_ADC_ABS: /* ADC Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_X: /* ADC Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_Y: /* ADC Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND_X: /* ADC (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND_Y: /* ADC (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                add_and_set_flags(cpu, N);
            }
            break;

    /* AND --------------------------------- */

        case OP_AND_IMM: /* AND Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo &= N; // replace with an and_and_set_flags 
                set_n_z_flags(cpu, cpu->a_lo); 
            }
            break;

        case OP_AND_ZP: /* AND Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_IND: /* AND (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo &= cpu->read_byte(addr);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_AND_ZP_X: /* AND Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_ABS: /* AND Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_AND_ABS_X: /* AND Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_ABS_Y: /* AND Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_IND_X: /* AND (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_AND_IND_Y: /* AND (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

    /* ASL --------------------------------- */

    case OP_ASL_ACC: /* ASL Accumulator */
        {
            byte_t N = cpu->a_lo;
            cpu->a_lo = arithmetic_shift_left(cpu, N);
        }
        break;

    case OP_ASL_ZP: /* ASL Zero Page */
        {
            absaddr_t addr = get_operand_address_zeropage(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;

    case OP_ASL_ZP_X: /* ASL Zero Page, X */
        {
            zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
            arithmetic_shift_left_addr(cpu, zpaddr);
        }
        break;

    case OP_ASL_ABS: /* ASL Absolute */
        {
            absaddr_t addr = get_operand_address_absolute(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;
        
    case OP_ASL_ABS_X: /* ASL Absolute, X */
        {
            absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
            arithmetic_shift_left_addr(cpu, addr);
        }
        break;

    /* Branching --------------------------------- */
        /* BBR and BBS - branch if bit reset (0) or set (1) in accumulator */
        /* These implementations are wrong - we need a 3-byte instruction, opcode, then zp address, then relative address */
        case OP_BBR0_REL: /* BBR0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBR1_REL: /* BBR1 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBR2_REL: /* BBR2 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }   
            break;

        case OP_BBR3_REL: /* BBR3 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }   
            break;

        case OP_BBR4_REL: /* BBR4 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBR5_REL: /* BBR5 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBR6_REL: /* BBR6 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;
        case OP_BBR7_REL: /* BBR6 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;
        case OP_BBS0_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;
        case OP_BBS1_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS2_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS3_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS4_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS5_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS6_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_BBS7_REL: /* BBS0 Relative */
            if constexpr (CPUTraits::has_bbr_bbs) {
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        /* BCS / BCC */

        case OP_BCC_REL: /* BCC Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->C == 0);
            }
            break;

        case OP_BCS_REL: /* BCS Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->C == 1);
            }
            break;

        case OP_BEQ_REL: /* BEQ Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->Z == 1);
            }
            break;

        case OP_BNE_REL: /* BNE Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->Z == 0);
            }
            break;

        case OP_BMI_REL: /* BMI Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->N == 1);
            }
            break;

        case OP_BPL_REL: /* BPL Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->N == 0);
            }
            break;

        case OP_BRA_REL: /* BRA Relative */
            if constexpr (CPUTraits::has_65c02_ops) {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, true);
            }
            break;

        case OP_BVC_REL: /* BVC Relative */
            {
                uint8_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->V == 0);
            }
            break;

        case OP_BVS_REL: /* BVS Relative */
            {
                byte_t N = get_operand_relative(cpu);
                branch_if(cpu, N, cpu->V == 1);
            }
            break;

    /* CMP --------------------------------- */
        case OP_CMP_IMM: /* CMP Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_IND: /* CMP (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, cpu->read_byte(addr));
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_CMP_ZP: /* CMP Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ZP_X: /* CMP Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS: /* CMP Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS_X: /* CMP Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_ABS_Y: /* CMP Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_IND_X: /* CMP (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

        case OP_CMP_IND_Y: /* CMP (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);
            }
            break;

    /* CPX --------------------------------- */
        case OP_CPX_IMM: /* CPX Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

        case OP_CPX_ZP: /* CPX Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

        case OP_CPX_ABS: /* CPX Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->x_lo, N);
            }
            break;

    /* CPY --------------------------------- */
        case OP_CPY_IMM: /* CPY Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

        case OP_CPY_ZP: /* CPY Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

        case OP_CPY_ABS: /* CPY Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                compare_and_set_flags(cpu, cpu->y_lo, N);
            }
            break;

    /* DEC --------------------------------- */
        case OP_DEC_ZP: /* DEC Zero Page */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage(cpu);
                dec_operand(cpu, zpaddr);
            }
            break;

        case OP_DEC_ZP_X: /* DEC Zero Page, X */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                dec_operand(cpu, zpaddr);
            }
            break;

        case OP_DEC_ABS: /* DEC Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                dec_operand(cpu, addr);
            }
            break;

        case OP_DEC_ABS_X: /* DEC Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                dec_operand(cpu, addr);
            }
            break;

    /* DE(xy) --------------------------------- */
        case OP_DEX_IMP: /* DEX Implied */
            {
                cpu->x_lo --;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_DEY_IMP: /* DEY Implied */
            {
                cpu->y_lo --;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;


    /* EOR --------------------------------- */

        case OP_EOR_IMM: /* EOR Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_IND: /* EOR (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo ^= cpu->read_byte(addr);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_EOR_ZP: /* EOR Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ZP_X: /* EOR Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ABS: /* EOR Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_EOR_ABS_X: /* EOR Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_ABS_Y: /* EOR Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_IND_X: /* EOR (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_EOR_IND_Y: /* EOR (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;


        /* INC A / INA & DEC A / DEA --------------------------------- */
        case OP_INA_ACC: /* INA Accumulator */
            if constexpr (CPUTraits::has_65c02_ops) {
                cpu->a_lo++;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_DEA_ACC: /* DEA Accumulator */
            if constexpr (CPUTraits::has_65c02_ops) {
                cpu->a_lo--;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

    /* INC --------------------------------- */
        case OP_INC_ZP: /* INC Zero Page */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage(cpu);
                inc_operand(cpu, zpaddr);
            }
            break;

        case OP_INC_ZP_X: /* INC Zero Page, X */
            {
                zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                inc_operand(cpu, zpaddr);
            }
            break;

        case OP_INC_ABS: /* INC Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                inc_operand(cpu, addr);
            }
            break;

        case OP_INC_ABS_X: /* INC Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                inc_operand(cpu, addr);
            }
            break;

    /* IN(xy) --------------------------------- */

        case OP_INX_IMP: /* INX Implied */
            {
                cpu->x_lo ++;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_INY_IMP: /* INY Implied */
            {
                cpu->y_lo ++;
                incr_cycles(cpu);
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

    /* LDA --------------------------------- */

        case OP_LDA_IMM: /* LDA Immediate */
            {
                cpu->a_lo = get_operand_immediate(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ZP: /* LDA Zero Page */
            {
                cpu->a_lo =  get_operand_zeropage(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ZP_X: /* LDA Zero Page, X */
            {
                cpu->a_lo = get_operand_zeropage_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ABS: /* LDA Absolute */
            {
                cpu->a_lo = get_operand_absolute(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_LDA_ABS_X: /* LDA Absolute, X */
            {
                cpu->a_lo = get_operand_absolute_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_ABS_Y: /* LDA Absolute, Y */
            {
                cpu->a_lo = get_operand_absolute_y(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_IND: /* LDA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo = cpu->read_byte(addr);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_LDA_IND_X: /* LDA (Indirect, X) */
            {
                cpu->a_lo = get_operand_zeropage_indirect_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_LDA_IND_Y: /* LDA (Indirect), Y */
            {
                cpu->a_lo = get_operand_zeropage_indirect_y(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        /* LDX --------------------------------- */

        case OP_LDX_IMM: /* LDX Immediate */
            {
                cpu->x_lo = get_operand_immediate(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ZP: /* LDX Zero Page */
            {
                cpu->x_lo = get_operand_zeropage(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ZP_Y: /* LDX Zero Page, Y */
            {
                cpu->x_lo = get_operand_zeropage_y(cpu);
                incr_cycles(cpu); // ldx zp, y uses an extra cycle.
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ABS: /* LDX Absolute */
            {
                cpu->x_lo = get_operand_absolute(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        case OP_LDX_ABS_Y: /* LDX Absolute, Y */
            {
                cpu->x_lo = get_operand_absolute_y(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
            }
            break;

        /* LDY --------------------------------- */

        case OP_LDY_IMM: /* LDY Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;
        
        case OP_LDY_ZP: /* LDY Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ZP_X: /* LDY Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ABS: /* LDY Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

        case OP_LDY_ABS_X: /* LDY Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->y_lo = N;
                set_n_z_flags(cpu, cpu->y_lo);
            }
            break;

    /* LSR  --------------------------------- */

        case OP_LSR_ACC: /* LSR Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = logical_shift_right(cpu, N);
            }
            break;

        case OP_LSR_ZP: /* LSR Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ZP_X: /* LSR Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ABS: /* LSR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;

        case OP_LSR_ABS_X: /* LSR Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                logical_shift_right_addr(cpu, addr);
            }
            break;


    /* ORA --------------------------------- */

        case OP_ORA_IMM: /* AND Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_IND: /* ORA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo |= cpu->read_byte(addr);
                set_n_z_flags(cpu, cpu->a_lo);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_ORA_ZP: /* AND Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ZP_X: /* AND Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ABS: /* AND Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;
        
        case OP_ORA_ABS_X: /* AND Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_ABS_Y: /* AND Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_IND_X: /* AND (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

        case OP_ORA_IND_Y: /* AND (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);
            }
            break;

    /* Stack operations --------------------------------- */

        case OP_PHA_IMP: /* PHA Implied */
            {
                push_byte(cpu, cpu->a_lo);
            }
            break;

        case OP_PHP_IMP: /* PHP Implied */
            {
                push_byte(cpu, (cpu->p | (FLAG_B | FLAG_UNUSED))); // break flag and Unused bit set to 1.
            }
            break;

        case OP_PHX_IMP: /* PHX Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                push_byte(cpu, cpu->x_lo);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PHY_IMP: /* PHY Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                push_byte(cpu, cpu->y_lo);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PLP_IMP: /* PLP Implied */
            {
                cpu->p = pop_byte(cpu) & ~FLAG_B; // break flag is cleared.
                incr_cycles(cpu); // TODO: where should this extra cycle actually go?
            }
            break;

        case OP_PLA_IMP: /* PLA Implied */
            {
                cpu->a_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
                incr_cycles(cpu);
            }
            break;

        case OP_PLX_IMP: /* PLX Implied */  
            if constexpr (CPUTraits::has_65c02_ops) {
                cpu->x_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
                incr_cycles(cpu);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PLY_IMP: /* PLY Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                cpu->y_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->y_lo);
                incr_cycles(cpu);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

    /* ROL --------------------------------- */

        case OP_ROL_ACC: /* ROL Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = rotate_left(cpu, N);
            }
            break;

        case OP_ROL_ZP: /* ROL Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ZP_X: /* ROL Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ABS: /* ROL Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

        case OP_ROL_ABS_X: /* ROL Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                rotate_left_addr(cpu, addr);
            }
            break;

    /* ROR --------------------------------- */
        case OP_ROR_ACC: /* ROR Accumulator */
            {
                byte_t N = cpu->a_lo;
                cpu->a_lo = rotate_right(cpu, N);
            }
            break;

        case OP_ROR_ZP: /* ROR Zero Page */
            {
                absaddr_t addr = get_operand_address_zeropage(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ZP_X: /* ROR Zero Page, X */
            {
                absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ABS: /* ROR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        case OP_ROR_ABS_X: /* ROR Absolute, X */
            {
                absaddr_t addr = get_operand_address_absolute_x_rmw(cpu);
                rotate_right_addr(cpu, addr);
            }
            break;

        /* SBC --------------------------------- */
        case OP_SBC_IMM: /* SBC Immediate */
            {
                byte_t N = get_operand_immediate(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND: /* SBC (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                subtract_and_set_flags(cpu, cpu->read_byte(addr));
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_SBC_ZP: /* SBC Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ZP_X: /* SBC Zero Page, X */
            {
                byte_t N = get_operand_zeropage_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS: /* SBC Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_X: /* SBC Absolute, X */
            {
                byte_t N = get_operand_absolute_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_Y: /* SBC Absolute, Y */
            {
                byte_t N = get_operand_absolute_y(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND_X: /* SBC (Indirect, X) */
            {
                byte_t N = get_operand_zeropage_indirect_x(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND_Y: /* SBC (Indirect), Y */
            {
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                subtract_and_set_flags(cpu, N);
            }
            break;

        /* STA --------------------------------- */
        case OP_STA_IND: /* STA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->write_byte(addr, cpu->a_lo); // TODO: one clock too many (get_operand.. burns 4)
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STA_ZP: /* STA Zero Page */
            {
                store_operand_zeropage(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ZP_X: /* STA Zero Page, X */
            {
                store_operand_zeropage_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS: /* STA Absolute */
            {
                store_operand_absolute(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS_X: /* STA Absolute, X */
            {
                store_operand_absolute_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_ABS_Y: /* STA Absolute, Y */
            {
                store_operand_absolute_y(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_IND_X: /* STA (Indirect, X) */
            {
                store_operand_zeropage_indirect_x(cpu, cpu->a_lo);
            }
            break;

        case OP_STA_IND_Y: /* STA (Indirect), Y */
            {
                store_operand_zeropage_indirect_y(cpu, cpu->a_lo);
            }
            break;
        
        /* STX --------------------------------- */
        case OP_STX_ZP: /* STX Zero Page */
            {
                store_operand_zeropage(cpu, cpu->x_lo);
            }
            break;

        case OP_STX_ZP_Y: /* STX Zero Page, Y */
            {
                store_operand_zeropage_y(cpu, cpu->x_lo);
                incr_cycles(cpu); // ldx zp, y uses an extra cycle.
                // TODO: look into this and see where the extra cycle might need to actually go.
            }
            break;

        case OP_STX_ABS: /* STX Absolute */
            {
                store_operand_absolute(cpu, cpu->x_lo);
            }
            break;

    /* STY --------------------------------- */
        case OP_STY_ZP: /* STY Zero Page */
            {
                store_operand_zeropage(cpu, cpu->y_lo);
            }
            break;

        case OP_STY_ZP_X: /* STY Zero Page, X */
            {
                store_operand_zeropage_x(cpu, cpu->y_lo);
            }
            break;
        
        case OP_STY_ABS: /* STY Absolute */
            {   
                store_operand_absolute(cpu, cpu->y_lo);
            }
            break;

        /* STZ --------------------------------- */
        case OP_STZ_ZP: /* STZ Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                store_operand_zeropage(cpu, 0);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ZP_X: /* STZ Zero Page, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                store_operand_zeropage_x(cpu, 0);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ABS: /* STZ Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                store_operand_absolute(cpu, 0);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ABS_X: /* STZ Absolute, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                store_operand_absolute_x(cpu, 0);
            } else invalid_opcode(cpu, opcode);
            break;


        /* Transfer between registers --------------------------------- */



        case OP_TAX_IMP: /* TAX Implied */
            {
                op_transfer_to_x(cpu, cpu->a_lo);                
            }
            break;

        case OP_TAY_IMP: /* TAY Implied */
            {
                op_transfer_to_y(cpu, cpu->a_lo);
            }
            break;

        case OP_TYA_IMP: /* TYA Implied */
            {
                op_transfer_to_a(cpu, cpu->y_lo);
            }
            break;


        /* TSB and TRB - test and set or test and reset bits */
        case OP_TRB_ZP: /* TRB Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                uint16_t addr = get_operand_address_zeropage(cpu);
                byte_t N = cpu->read_byte(addr);
                byte_t temp = cpu->a_lo & N;
                set_z_flag(cpu, temp);
                incr_cycles(cpu);
                temp = N & ~(cpu->a_lo);
                cpu->write_byte(addr, temp);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TRB_ABS: /* TRB Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                uint16_t addr = get_operand_address_absolute(cpu);
                byte_t N = cpu->read_byte(addr);
                byte_t temp = cpu->a_lo & N;
                set_z_flag(cpu, temp);
                incr_cycles(cpu);
                temp = N & ~(cpu->a_lo);
                cpu->write_byte(addr, temp);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TSB_ZP: /* TSB Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                uint16_t addr = get_operand_address_zeropage(cpu);
                byte_t N = cpu->read_byte(addr);
                byte_t temp = cpu->a_lo & N;
                set_z_flag(cpu, (temp != 0)); // if any of the bits were previously set, set Z flag.
                incr_cycles(cpu);
                temp = N | (cpu->a_lo);
                cpu->write_byte(addr, temp);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TSB_ABS: /* TSB Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                uint16_t addr = get_operand_address_absolute(cpu);
                byte_t N = cpu->read_byte(addr);
                byte_t temp = cpu->a_lo & N;
                set_z_flag(cpu, (temp != 0));
                incr_cycles(cpu);
                temp = N | (cpu->a_lo);
                cpu->write_byte(addr, temp);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        /* TSX - transfer stack pointer to X */
        case OP_TSX_IMP: /* TSX Implied */
            {
                op_transfer_to_x(cpu, cpu->sp);
            }
            break;

        case OP_TXA_IMP: /* TXA Implied */
            {
                op_transfer_to_a(cpu, cpu->x_lo);
            }
            break;

        case OP_TXS_IMP: /* TXS Implied */
            {
                op_transfer_to_s(cpu, cpu->x_lo);
            }
            break;

        /* BRK --------------------------------- */
        case OP_BRK_IMP: /* BRK */
            {               
                push_word(cpu, cpu->pc+1); // pc of BRK plus 1 - leaves room for BRK 'mark'
                push_byte(cpu, cpu->p | FLAG_B | FLAG_UNUSED); // break flag and Unused bit set to 1.
                cpu->I = 1; // interrupt disable flag set to 1.
                if constexpr (CPUTraits::has_65c02_ops) {
                    cpu->D = 0; // turn off decimal mode on brk and interrupts
                }
                cpu->pc = cpu->read_word(BRK_VECTOR);
            }
            break;

        /* JMP --------------------------------- */
        case OP_JMP_ABS: /* JMP Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                cpu->pc = addr;
            }
            break;

        case OP_JMP_IND: /* JMP (Indirect) */
            { // TODO: need to implement the "JMP" bug for non-65c02. The below is correct for 65c02.
                absaddr_t addr = get_operand_address_absolute_indirect(cpu);
                cpu->pc = addr;
            }
            break;

        case OP_JMP_IND_X: /* JMP (Indirect), X */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_absolute_indirect_x(cpu);
                cpu->pc = addr;
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        /* JSR --------------------------------- */
        case OP_JSR_ABS: /* JSR Absolute */
            {
                absaddr_t addr = get_operand_address_absolute(cpu);
                push_word(cpu, cpu->pc -1); // return address pushed is last byte of JSR instruction
                cpu->pc = addr;
                // load address fetched into the PC
                incr_cycles(cpu);
            }
            break;

        /* RTI --------------------------------- */
        case OP_RTI_IMP: /* RTI */
            {
                // pop status register "ignore B | unused" which I think means don't change them.
                byte_t oldp = cpu->p & (FLAG_B | FLAG_UNUSED);
                byte_t p = pop_byte(cpu) & ~(FLAG_B | FLAG_UNUSED);
                cpu->p = p | oldp;

                cpu->pc = pop_word(cpu);
                TRACE(cpu->trace_entry.operand = cpu->pc;)
            }
            break;

        /* RTS --------------------------------- */
        case OP_RTS_IMP: /* RTS */
            {
                cpu->pc = pop_word(cpu);
                incr_cycles(cpu);
                cpu->pc++;
                incr_cycles(cpu);
                TRACE(cpu->trace_entry.operand = cpu->pc;)
            }
            break;

        /* NOP --------------------------------- */
        case OP_NOP_IMP: /* NOP */
            {
                incr_cycles(cpu);
            }
            break;

        /* Flags ---------------------------------  */

        case OP_CLD_IMP: /* CLD Implied */
            {
                cpu->D = 0;
                incr_cycles(cpu);
            }
            break;

        case OP_SED_IMP: /* SED Implied */
            {
                cpu->D = 1;
                incr_cycles(cpu);
            }
            break;

        case OP_CLC_IMP: /* CLC Implied */
            {
                cpu->C = 0;
                incr_cycles(cpu);
            }
            break;

        case OP_CLI_IMP: /* CLI Implied */
            {
                cpu->I = 0;
                incr_cycles(cpu);
            }
            break;

        case OP_CLV_IMP: /* CLV */
            {
                cpu->V = 0;
                incr_cycles(cpu);
            }
            break;

        case OP_SEC_IMP: /* SEC Implied */
            {
                cpu->C = 1;
                incr_cycles(cpu);
            }
            break;

        case OP_SEI_IMP: /* SEI Implied */
            {
                cpu->I = 1;
                incr_cycles(cpu);
            }
            break;

        /** Misc --------------------------------- */

        case OP_BIT_ZP: /* BIT Zero Page */
            {
                byte_t N = get_operand_zeropage(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            }
            break;

        case OP_BIT_ABS: /* BIT Absolute */
            {
                byte_t N = get_operand_absolute(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            }
            break;

        case OP_BIT_IMM: /* BIT Immediate */
            if constexpr (CPUTraits::has_65c02_ops) {
                byte_t N = get_operand_immediate(cpu);
                byte_t temp = cpu->a_lo & N;
                set_z_flag(cpu, temp);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_BIT_ZP_X: /* BIT Zero Page, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                byte_t N = get_operand_zeropage_x(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_BIT_ABS_X: /* BIT Absolute, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                byte_t N = get_operand_absolute_x(cpu);
                byte_t temp = cpu->a_lo & N;
                set_n_z_v_flags(cpu, temp, N);
            } else invalid_opcode(cpu, opcode);
            break;

        /* A bunch of stuff that is unimplemented in the 6502/65c02, but present in 65816 */

        case OP_INOP_02: /* INOP 02 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_22: /* INOP 22 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            }
            break;

        case OP_INOP_42: /* INOP 42 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_62: /* INOP 62 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_82: /* INOP 82 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C2: /* INOP C2 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E2: /* INOP E2 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_44: /* INOP 44 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 3);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_54: /* INOP 54 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D4: /* INOP D4 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_F4: /* INOP F4 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_5C: /* INOP 5C */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 8);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_DC: /* INOP DC */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_FC: /* INOP FC */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 4);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_03: /* INOP 03 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_13: /* INOP 13 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_23: /* INOP 23 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_33: /* INOP 33 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_43: /* INOP 43 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_53: /* INOP 53 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_63: /* INOP 63 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_73: /* INOP 73 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_83: /* INOP 83 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_93: /* INOP 93 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_A3: /* INOP A3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_B3: /* INOP B3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C3: /* INOP C3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D3: /* INOP D3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E3: /* INOP E3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_F3: /* INOP F3 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_07: /* INOP 07 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_17: /* INOP 17 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_27: /* INOP 27 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_37: /* INOP 37 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_47: /* INOP 47 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_57: /* INOP 57 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_67: /* INOP 67 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_77: /* INOP 77 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_87: /* INOP 87 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_97: /* INOP 97 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_A7: /* INOP A7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_B7: /* INOP B7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C7: /* INOP C7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D7: /* INOP D7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E7: /* INOP E7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_F7: /* INOP F7 */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_0B: /* INOP 0B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_1B: /* INOP 1B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_2B: /* INOP 2B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_3B: /* INOP 3B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_4B: /* INOP 4B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_5B: /* INOP 5B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_6B: /* INOP 6B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_7B: /* INOP 7B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_8B: /* INOP 8B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_9B: /* INOP 9B */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_AB: /* INOP AB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_BB: /* INOP BB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_CB: /* INOP CB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_DB: /* INOP DB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_EB: /* INOP EB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_FB: /* INOP FB */
            if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;


            
        /* End of Opcodes -------------------------- */

        default:
            invalid_opcode(cpu, opcode);
            break;
    }

    TRACE(if (cpu->trace) cpu->trace_buffer->add_entry(cpu->trace_entry);)

    return 0;
}

};

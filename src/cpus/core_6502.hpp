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

#pragma once

#include <cstdint>
#include <cstdio>

#include "cpu.hpp"


namespace cpu_6502 {

/**
 * not sure how I feel about this. I could just leave them back in the core_6502.cpp file..
 */

#if 0
uint64_t get_current_time_in_microseconds() {
    return SDL_GetTicksNS() / 1000;
}
#endif

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
        cpu->incr_cycles(); 
        /* If a branch is taken and the target is on a different page, this adds another CPU cycle (4 in total). */
        if ((oaddr & 0xFF00) != (taddr & 0xFF00)) {
            cpu->incr_cycles();
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
    cpu->incr_cycles(); // ZP,X adds a cycle.
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

inline absaddr_t get_operand_address_absolute_x(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc();
    absaddr_t taddr = addr + cpu->x_lo;
    if ((addr & 0xFF00) != (taddr & 0xFF00)) {
        cpu->incr_cycles();
    }
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

// current working version
/* inline absaddr_t get_operand_address_absolute_x_rmw(cpu_state *cpu) {
    absaddr_t addr = cpu->read_word_from_pc(); // T1,T2 // TODO: is the order of reading the PC correct?
    absaddr_t taddr = addr + cpu->x_lo; // T3
    cpu->incr_cycles();
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
        cpu->incr_cycles();
    }
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline uint16_t get_operand_address_indirect_x(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    absaddr_t taddr = cpu->read_word((uint8_t)(zpaddr + cpu->x_lo)); // make sure it wraps.
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
}

inline absaddr_t get_operand_address_indirect_y(cpu_state *cpu) {
    zpaddr_t zpaddr = cpu->read_byte_from_pc();
    absaddr_t iaddr = cpu->read_word(zpaddr);
    absaddr_t taddr = iaddr + cpu->y_lo;

    if ((iaddr & 0xFF00) != (taddr & 0xFF00)) {
        cpu->incr_cycles();
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
    cpu->incr_cycles(); // TODO: where should this extra cycle actually go?
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
    cpu->incr_cycles(); // TODO: where should this extra cycle actually go?
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
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_y(cpu_state *cpu, byte_t N) {
    cpu->y_lo = N;
    set_n_z_flags(cpu, cpu->y_lo);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_a(cpu_state *cpu, byte_t N) {
    cpu->a_lo = N;
    set_n_z_flags(cpu, cpu->a_lo);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline void op_transfer_to_s(cpu_state *cpu, byte_t N) {
    cpu->sp = N;
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

/**
 * Decrement an operand.
 */
inline void dec_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr);
    N--;
    cpu->incr_cycles();
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
    cpu->incr_cycles();
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
    cpu->incr_cycles();
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
    cpu->incr_cycles();
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
    cpu->incr_cycles();
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
    cpu->incr_cycles();
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
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t pop_byte(cpu_state *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1);
    cpu->incr_cycles();
    byte_t N = cpu->read_byte(0x0100 + cpu->sp);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void push_word(cpu_state *cpu, word_t N) {
    cpu->write_byte(0x0100 + cpu->sp, (N & 0xFF00) >> 8);
    cpu->write_byte(0x0100 + cpu->sp - 1, N & 0x00FF);
    cpu->sp = (uint8_t)(cpu->sp - 2);
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline absaddr_t pop_word(cpu_state *cpu) {
    absaddr_t N = cpu->read_word(0x0100 + cpu->sp + 1);
    cpu->sp = (uint8_t)(cpu->sp + 2);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

}
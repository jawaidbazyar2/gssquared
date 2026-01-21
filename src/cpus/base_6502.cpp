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
 * version for the 65c816 in 8-bit "emulation mode", the 8/8, 8/16, 16/8, and 16/16 modes.
 */

/**
 * References: 
 * Apple Machine Language: Don Inman, Kurt Inman
 * https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
 * https://www.masswerk.at/6502/6502_instruction_set.html#USBC
 * 
 */

//#define TRACE(SETTER) { SETTER }
#define TRACE(SETTER) \
    { \
        if constexpr (TraceTraits::trace_enabled) { \
            SETTER \
        } \
    }

template<typename CPUTraits, typename TraceTraits = TraceEnabled>
class CPU6502Core : public BaseCPU {

public:
    void reset(cpu_state* cpu) override {
        if constexpr (CPUTraits::has_65816_ops) {
            cpu->d = 0x0000;
            cpu->full_db = 0x00;
            cpu->pb = 0x00;
            cpu->sp_hi = 0x01; // sp forced to page 1
            cpu->x_hi = 0x00; // hi bytes cleared
            cpu->y_hi = 0x00;
            cpu->E = 1; // emul mode
            cpu->_M = 1; // 8 bit M and X
            cpu->_X = 1;
            cpu->D = 0; // disable decimal mode
            printf("stack init: %04X\n", cpu->sp);
        }

        // fill in with register reset logic.
        cpu->pc = read_word_bank0(cpu, RESET_VECTOR);
    }
    
    const char *get_name() override { return CPUTraits::name; }

private:
    // Type alias for ALU operations that can be either 8-bit or 16-bit based on A register width (m_16 trait)
    using alu_t = std::conditional_t<CPUTraits::m_16, word_t, byte_t>;
    using index_t = std::conditional_t<CPUTraits::x_16, word_t, byte_t>;

    const alu_t zero = 0;

inline uint8_t word_lo(word_t w) { return w & 0xFF; }
inline uint8_t word_hi(word_t w) { return (w >> 8) & 0xFF; }
inline word_t word(uint8_t lo, uint8_t hi) { return lo | (hi << 8); }

/* Memory Bus Routines */
/*
 We (almost) universally need to access bus followed by incr_cycles.
 So bake those two things here together.
*/
inline uint8_t bus_read(cpu_state *cpu, uint32_t addr) {
    uint8_t data = cpu->mmu->read(addr & 0xFFFFFF);
    cpu->incr_cycles();
    return data;
}
inline void bus_write(cpu_state *cpu, uint32_t addr, uint8_t data) {
    cpu->mmu->write(addr & 0xFFFFFF, data);
    cpu->incr_cycles();
}

inline uint8_t vp_read(cpu_state *cpu, uint32_t addr) {
    uint8_t data = cpu->mmu->vp_read(addr);
    cpu->incr_cycles();
    return data;
}

/** Phantom Read Routines */
/*
 these are the same kinds of routines, EXCEPT - they are named this way
 because we may optionally not actually perform the work of the extra bus
 access, if not needed in an Apple II. (typically, a phantom read of the PC).
 */
// Normal phantom read - always performs
inline void phantom_read(cpu_state *cpu, uint32_t address) {
    cpu->mmu->read(address);
    cpu->incr_cycles();
}

// Phantom read - only performs if full_phantom_reads is true. This is used for PRs that just re-read program counter etc and can't affect I/O in Apple II..
inline void phantom_read_ign(cpu_state *cpu, uint32_t address) {
    if constexpr (CPUTraits::full_phantom_reads) {
        cpu->mmu->read(address);
    }
    cpu->incr_cycles();
}

// Phantom write - always performs
inline void phantom_write(cpu_state *cpu, uint32_t address, uint8_t value) {
    cpu->mmu->write(address, value);
    cpu->incr_cycles();
}

// Ignorable phantom write. (Not sure this ever happens..)
inline void phantom_write_ign(cpu_state *cpu, uint32_t address, uint8_t value) {
    if constexpr (CPUTraits::full_phantom_reads) {
        cpu->mmu->write(address, value);
    }
    cpu->incr_cycles();
}

/*** Data Memory Accessors */

// read_byte, read_word, write_byte, write_word - are "absolute" mode memory accessors.
/**
 * byte accessor. 
 * word accessor: in emul mode (or just plain 6502), the address will wrap from FFFF -> 0.
 *   In native mode, does not wrap, FFFF + 1 = 1'0000.
 */

/** word accessor. in emul mode, wraps. In native mode, does not wrap. */

inline uint32_t make_direct_long(cpu_state *cpu, uint8_t address) {
    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) {
            if ((cpu->d & 0xFF) == 0) {
                return (cpu->d | address) & 0xFFFF;
            }
            return (cpu->d + address);
        } else return (cpu->d + address) & 0xFFFF; // wrap if access exceeds 
    } else return address;
}

/** Calculate Data Access address from a 16-bit address */
inline uint32_t make_address_long(cpu_state *cpu, uint16_t address) {
    if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode)
        return (cpu->full_db | address);
    else return address;
}

/* inline byte_t read_byte(cpu_state *cpu, uint16_t address) {
    uint32_t eaddr = make_address_long(cpu, address);
    uint8_t value = cpu->mmu->read(eaddr);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = value;)
    return value;
} */

/* inline void write_byte(cpu_state *cpu, uint16_t address, byte_t value) {
    uint32_t eaddr = make_address_long(cpu, address);
    cpu->mmu->write(eaddr, value);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = value;)
} */

/* Used to read a word from a 16-bit address plus data bank */
inline word_t read_word(cpu_state *cpu, uint16_t address) {
    uint32_t eaddr = make_address_long(cpu, address);
    byte_t vl = bus_read(cpu, eaddr);
    byte_t vh = bus_read(cpu, make_address_long(cpu, address + 1));
    word_t value = word(vl, vh);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = value;)
    return value;
}

/* Used to write a word to a 16-bit address plus data bank */
inline void write_word(cpu_state *cpu, uint16_t address, word_t value) {
    uint32_t eaddr = make_address_long(cpu, address);
    cpu->mmu->write(eaddr, word_lo(value));
    cpu->incr_cycles();
    cpu->mmu->write(eaddr + 1, word_hi(value));
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = value;)
}

/*** Program Counter Memory Accessors ***/

/** Calculate Program Fetch address from a 16-bit address */
inline uint32_t make_pc_long(cpu_state *cpu, uint16_t address) {
    if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode) {
        return (cpu->pb << 16) | address;
    } else return address;
}

/** consume a byte value from the program counter, and increment the PC accordingly. */
/* inline uint8_t read_byte_from_pc(cpu_state *cpu) {
    byte_t opcode = bus_read(cpu, make_pc_long(cpu, cpu->pc));
    cpu->pc++;
    return opcode;
} */

/** consume a word value from the program counter, and increment the PC accordingly. */
inline uint16_t read_word_from_pc(cpu_state *cpu) {
    // make sure this is read lo-byte first.
    byte_t al = bus_read(cpu, make_pc_long(cpu, cpu->pc));
    cpu->pc++;
    byte_t ah = bus_read(cpu, make_pc_long(cpu, cpu->pc));
    cpu->pc++;
    
    return word(al, ah);
}

// Vector pull - read word using vector pull
inline word_t read_word_bank0(cpu_state *cpu, uint16_t address) {
    byte_t vl = vp_read(cpu, (uint16_t)(address));
    byte_t vh = vp_read(cpu, (uint16_t)(address + 1));
    word_t value = word(vl, vh);
    TRACE(cpu->trace_entry.eaddr = address; cpu->trace_entry.data = value;)
    return value;
}

/**
ALU Operations

These are "function objects" that can be passed as parameters to the rmw_* functions.
The data operated upon has a width of alu_t (which is determined by the m_16 trait).
*/

struct IncrementOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        N++;
        set_n_z_flags(cpu, N);
    }
};

struct DecrementOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        N--;
        set_n_z_flags(cpu, N);
    }
};

struct ArithmeticShiftLeftOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        if constexpr (is_byte<alu_t>) {
            cpu->C = (N & 0x80) >> 7;
            N = N << 1;
        } else {
            cpu->C = (N & 0x8000) >> 15;
            N = N << 1;
        }
        set_n_z_flags(cpu, N);
        //TRACE(cpu->trace_entry.data = N;)
    }
};

struct LogicalShiftRightOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        cpu->C = N & 0x01;
        N = N >> 1;
        set_n_z_flags(cpu, N);
    }
};

struct RotateLeftOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        if constexpr (is_byte<alu_t>) {
            uint8_t C = ((N & 0x80) != 0);
            N = N << 1;
            N |= cpu->C;
            cpu->C = C;
            set_n_z_flags(cpu, N);
        } else {
            uint16_t C = ((N & 0x8000) != 0);
            N = N << 1;
            N |= cpu->C;
            cpu->C = C;
            set_n_z_flags(cpu, N);
        }
    }
};

struct RotateRightOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        uint8_t C = N & 0x01;
        N = N >> 1;

        if constexpr (is_byte<alu_t>) N |= (cpu->C << 7);
        else N |= (cpu->C << 15);

        cpu->C = C;
        set_n_z_flags(cpu, N);
    }
};

struct TestResetBitsOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        alu_t T = _A(cpu) & N;
        set_z_flag(cpu, T);
        N &= ~ _A(cpu);
    }   
};

struct TestSetBitsOp {
    void operator()(cpu_state* cpu, alu_t& N) {
        alu_t T = _A(cpu) & N;
        set_z_flag(cpu, T);
        N |= _A(cpu);
    }   
};

/**
This is the NEW address mode helper functions / templates / inlines
 */

// Type helper constants
template<typename T>
static constexpr bool is_byte = sizeof(T) == 1;

template<typename T>
static constexpr bool is_word = sizeof(T) == 2;

static auto& _A(cpu_state* cpu) {
    if constexpr (CPUTraits::m_16) return cpu->a;      // 16-bit A
    else return cpu->a_lo;                             // 8-bit A
}

// X register accessor - uses X flag (x_16 trait) 
static auto& _X(cpu_state* cpu) {
    if constexpr (CPUTraits::x_16) return cpu->x;      // 16-bit X
    else return cpu->x_lo;                             // 8-bit X
}

// Y register accessor - uses X flag (x_16 trait)
static auto& _Y(cpu_state* cpu) {
    if constexpr (CPUTraits::x_16) return cpu->y;      // 16-bit Y  
    else return cpu->y_lo;                             // 8-bit Y
}

inline uint32_t _PC(cpu_state *cpu) {
    if constexpr (CPUTraits::has_65816_ops) {
        //return (cpu->pb << 16) | cpu->pc;
        return cpu->full_pc;
    } else {
        return cpu->pc;
    }
}

inline uint8_t fetch_pc(cpu_state *cpu) {
    uint8_t b = cpu->mmu->read(_PC(cpu));
    cpu->incr_cycles();
    cpu->pc++;
    return b;
}

/** Memory Read/Write Helpers */

/* read_data - read either 1 or 2 bytes from memory depending on width of the type T. */
template<typename T>
inline void read_data(cpu_state *cpu, uint32_t eaddr, T &reg) {
    if constexpr (is_byte<T>) {
        reg = bus_read(cpu, eaddr);
    }
    if constexpr (is_word<T>) {
        
        reg = bus_read(cpu, eaddr);
        reg |= (bus_read(cpu, eaddr+1) << 8);
    }
}

/* write_data - write either 1 or 2 bytes to memory depending on width of the type T. */
template<typename T>
inline void write_data(cpu_state *cpu, uint32_t eaddr, T &reg) {

    if constexpr (is_byte<T>) {
        bus_write(cpu, eaddr, reg);
    }
    if constexpr (is_word<T>) 
    {
        bus_write(cpu, eaddr, word_lo(reg));
        bus_write(cpu, eaddr+1, word_hi(reg));
    }
}


/* Same as write_data but writes hi byte first then lo byte - used by rmw */
template<typename T>
inline void write_tada(cpu_state *cpu, uint32_t eaddr, T &reg) {

    if constexpr (is_byte<T>) {
        bus_write(cpu, eaddr, reg);
    }
    if constexpr (is_word<T>) {
        bus_write(cpu, eaddr+1, word_hi(reg));
        bus_write(cpu, eaddr, word_lo(reg));
    }
}

/** Direct Mode Helpers */
template<typename T>
inline void read_data_direct(cpu_state *cpu, uint16_t eaddr_16, T &reg) {
    if constexpr (is_byte<T>) {
        reg = bus_read(cpu, eaddr_16);  // cycle 4
    }
    // if we're in E mode, there will never be a second byte so don't have to deal with incrementing address here.
    if constexpr (is_word<T>) {        // TODO: might need special routine here in e-mode to wrap pointer inside the page
        reg = bus_read(cpu, eaddr_16); // cycle 4
        reg |= (bus_read(cpu, (uint16_t)(eaddr_16+1)) << 8); // cycle 4a
    }
}

template<typename T>
inline void write_data_direct(cpu_state *cpu, uint16_t eaddr_16, T &reg) {
    if constexpr (is_byte<T>) {
        bus_write(cpu, eaddr_16, reg); // cycle 4
    }
    // if we're in E mode, there will never be a second byte so don't have to deal with incrementing address here.
    if constexpr (is_word<T>) {        
        bus_write(cpu, (uint16_t)(eaddr_16), word_lo(reg)); // cycle 4
        bus_write(cpu, (uint16_t)(eaddr_16+1), word_hi(reg)); // cycle 4a
    }
}

template<typename T>
inline void write_tada_direct(cpu_state *cpu, uint16_t eaddr_16, T &reg) {
    if constexpr (is_byte<T>) {
        bus_write(cpu, eaddr_16, reg); // cycle 4
    }
    // if we're in E mode, there will never be a second byte so don't have to deal with incrementing address here.
    if constexpr (is_word<T>) {        
        bus_write(cpu, (uint16_t)(eaddr_16+1), word_hi(reg)); // cycle 4a
        bus_write(cpu, (uint16_t)(eaddr_16), word_lo(reg)); // cycle 4
    }
}

/** 8. Accumulator A mode.  */
template<typename T, typename Operation>
inline void rmw_acc(cpu_state *cpu, Operation operation) {
    // phantom
    phantom_read_ign(cpu, _PC(cpu));

    operation(cpu, _A(cpu));
}

/** 18. Immediate mode. */
template<typename T>
inline void read_imm(cpu_state *cpu, T &reg) {
    if constexpr (is_byte<T>) {
        reg = fetch_pc(cpu);
    } else if constexpr (is_word<T>) {
        uint16_t imm = fetch_pc(cpu);
        imm |= fetch_pc(cpu) << 8;
        reg = imm;
    }
    TRACE(cpu->trace_entry.operand = reg; cpu->trace_entry.f_op_sz = sizeof(T);)
}

/* 1. Absolute mode. */

inline uint16_t address_abs (cpu_state *cpu) {
    uint16_t eaddr = fetch_pc(cpu);
    eaddr |= fetch_pc(cpu) << 8;

    TRACE(cpu->trace_entry.operand = eaddr; )
    return eaddr;
}

template<typename T>
inline void read_abs(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_abs(cpu);

    if constexpr (CPUTraits::has_65816_ops)
        eaddr |= (cpu->full_db);

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T>
inline void write_abs(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_abs(cpu);

    if constexpr (CPUTraits::has_65816_ops)
        eaddr |= (cpu->full_db);

    write_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 1d. Absolute (R-M-W) a */
template<typename T, typename Operation>
inline void rmw_abs(cpu_state *cpu, Operation operation) {
    uint32_t eaddr = address_abs(cpu);
    alu_t reg;

    if constexpr (CPUTraits::has_65816_ops)
        eaddr |= (cpu->full_db);

    read_data(cpu, eaddr, reg);

    // Cycle 5. issue phantom cycle (during which we do the operation).
    // nmos 6502 does write during modify cycle.
    // 65c02 does read during modify cycle.
    // 65816 does read during modify cycle in native mode.
    // 65816 does write during modify cycle in emulation mode.

    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) // acts like nmos
            phantom_write(cpu, eaddr, reg);
        else { // addr based on reg size
            if constexpr (sizeof(reg) == 1) phantom_read(cpu, eaddr);
            else phantom_read(cpu, eaddr+1);
        }  
    } else if constexpr (CPUTraits::has_65c02_ops) { // 65c02 used a read
        phantom_read(cpu, eaddr);
    } else { // nmos used a write
        phantom_write(cpu, eaddr, reg);
    }
    
    operation(cpu, reg);     // perform operation here: tmp and flags are modified appropriately.

    // Cycles 6a, 6. write back the result.
    write_tada(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/* 4. Absolute Long Address Modes */

inline uint32_t address_long (cpu_state *cpu) {
    uint32_t eaddr = fetch_pc(cpu);
    eaddr |= fetch_pc(cpu) << 8;
    eaddr |= fetch_pc(cpu) << 16;

    TRACE(cpu->trace_entry.operand = eaddr; cpu->trace_entry.f_op_sz = 3;)
    return eaddr;
}

template<typename T>
inline void read_long(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_long(cpu);

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T>
inline void write_long(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_long(cpu);

    write_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/* 5 . Absolute Long, X al,x */

template<typename T>
inline uint32_t address_long_x (cpu_state *cpu, T &index) {
    uint32_t base = fetch_pc(cpu);
    base |= fetch_pc(cpu) << 8;
    base |= fetch_pc(cpu) << 16;
    uint32_t eaddr = (base + index); 

    TRACE(cpu->trace_entry.operand = base; cpu->trace_entry.f_op_sz = 3;)
    return eaddr;
}

template<typename T, typename U>
inline void read_long_x(cpu_state *cpu, T &reg, U &index) {
    uint32_t eaddr = address_long_x(cpu, index);

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T, typename U>
inline void write_long_x(cpu_state *cpu, T &reg, U &index) {
    uint32_t eaddr = address_long_x(cpu, index);

    write_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/* 6a. Absolute, X Mode */

/* 4. Add 1 cycle for indexing across page boundaries, or write, or X=0. 
When X=1 or in the emulation mode, this cycle contains invalid addresses:
 DBR, AAH, AAL+XL
 (i.e. it's a partial addition that effectively wraps on the same page)
*/

inline uint32_t address_abs_x(cpu_state *cpu, uint32_t index) {
    uint32_t base;
    uint32_t eaddr;

    if constexpr (CPUTraits::has_65816_ops) {
        base = address_abs(cpu);
        base = (cpu->full_db | base);
        eaddr = base + index;

        if constexpr ((CPUTraits::e_mode) || (!CPUTraits::x_16)) {
            if ((base & 0xFF00) != (eaddr & 0xFF00)) { // if we crossed page boundary
                phantom_read(cpu, (eaddr & 0xFFFF00) | ((base + index) & 0xFF) );
            }
        } else {
            phantom_read(cpu, eaddr); // TODO: not sure this is the correct phantom read.
        }
    } else {
        base = address_abs(cpu);
        eaddr = (uint16_t)(base + index);          // force to 16-bit address space.
        if ((base & 0xFF00) != (eaddr & 0xFF00)) { // if we crossed page boundary
            phantom_read(cpu, (eaddr & 0xFF00) | ((base + index) & 0xFF) );
        }
    }
    return eaddr;
}

/** Same as above, except on a write, we always have the phantom read cycle. */
inline uint32_t address_abs_x_write(cpu_state *cpu, uint32_t index) {
    uint32_t base;
    uint32_t eaddr;

    if constexpr (CPUTraits::has_65816_ops) {
        base = address_abs(cpu);
        base = (cpu->full_db | base);
        eaddr = base + index;

        if constexpr ((CPUTraits::e_mode) || (!CPUTraits::x_16)) {
            phantom_read(cpu, (eaddr & 0xFFFF00) | ((base + index) & 0xFF) );
        } else {
            phantom_read(cpu, eaddr); // TODO: not sure this is the correct phantom read.
        }
    } else {
        base = address_abs(cpu);
        eaddr = (uint16_t)(base + index);          // force to 16-bit address space.
        phantom_read(cpu, (eaddr & 0xFF00) | ((base + index) & 0xFF) );
    }
    return eaddr;
}

template<typename T, typename U>
inline void read_abs_x(cpu_state *cpu, T &reg, U &index) {
    uint32_t eaddr = address_abs_x(cpu, index);

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T, typename U>
inline void write_abs_x(cpu_state *cpu, T &reg, U &index) {
    uint32_t eaddr = address_abs_x_write(cpu, index);

    write_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

inline uint32_t address_rmw_abs_x(cpu_state *cpu, uint32_t index) {
    uint32_t base;
    uint32_t eaddr;

    if constexpr (CPUTraits::has_65816_ops) {
        base = address_abs(cpu);
        base = (cpu->full_db | base);
        eaddr = base + index;

        if constexpr ((CPUTraits::e_mode) || (!CPUTraits::x_16)) {
            phantom_read(cpu, (eaddr & 0xFFFF00) | ((base + index) & 0xFF) );
        } else {
            phantom_read(cpu, eaddr); // TODO: not sure this is the correct phantom read.
        }
    } else {
        base = address_abs(cpu);
        eaddr = (uint16_t)(base + index);          // force to 16-bit address space.
        phantom_read(cpu, (eaddr & 0xFF00) | ((base + index) & 0xFF) );
    }
    return eaddr;
}

template<typename T, typename Operation>
inline void rmw_abs_x(cpu_state *cpu, Operation operation) {
    alu_t reg;

    // I think it needs the phantom every time, not just on page cross
    uint32_t eaddr = address_rmw_abs_x(cpu, _X(cpu)); // cycles 2,3,4

    read_data(cpu, eaddr, reg);

    // TODO: keep eye on this in case it's still wrong...
    // Cycle 6

    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) // acts like nmos
            phantom_write(cpu, eaddr, reg);
        else { // addr based on reg size
            if constexpr (sizeof(reg) == 1) phantom_read(cpu, eaddr);
            else phantom_read(cpu, eaddr+1);
        }  
    } else if constexpr (CPUTraits::has_65c02_ops) { // 65c02 used a read
        phantom_read(cpu, eaddr);
    } else { // nmos used a write
        phantom_write(cpu, eaddr, reg);
    }
    operation(cpu, reg);

    write_tada(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/* 10a. Direct Mode - Zero Page Mode */

inline uint16_t address_direct (cpu_state *cpu) {
    uint8_t operand = fetch_pc(cpu); // cycle 2
    TRACE(cpu->trace_entry.operand = operand; )
    // conditions:
    // 6502
    // 816 in e-mode, dl is zero
    // 816 in e-mode, dl nonzero, or 816 in n-mode
    uint16_t eaddr_16;

    if constexpr (!CPUTraits::has_65816_ops) {
        eaddr_16 = operand;
    } else {
        if constexpr (CPUTraits::e_mode) {
            if ((cpu->d & 0xFF) == 0) {
                eaddr_16 = cpu->d | operand;
            } else {
                eaddr_16 = (cpu->d + operand); // with just direct there can be no page wrapping.
            }
        } else {
            eaddr_16 = (cpu->d + operand);
        }

        if (cpu->d & 0xFF) { // extra cycle taken if DP low is non-zero
            phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc)); // cycle 2a - PhantomIgn: PC+1 (current PC)
        }
    }
    return eaddr_16;
}

/** Address Mode: 10a. Direct d (Read) */
template<typename T>
inline void read_direct(cpu_state *cpu, T &reg) {

    uint16_t eaddr_16 = address_direct(cpu);

    read_data_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 10a. Direct d (Write) */
template<typename T>
inline void write_direct(cpu_state *cpu, T &reg) {

    uint16_t eaddr_16 = address_direct(cpu);

    write_data_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 10b. Direct d (RMW) */
template<typename T, typename Operation>
inline void rmw_direct(cpu_state *cpu, Operation operation) {
    uint16_t eaddr_16 = address_direct(cpu); // cycle 2 and perhaps 2a
    alu_t reg;

    read_data_direct(cpu, eaddr_16, reg);

    // do operation
    operation(cpu, reg);
    phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu))); // cycle 4

    write_tada_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 16a. Direct, X */
inline uint16_t address_direct_x (cpu_state *cpu, uint32_t index) {
    uint8_t operand = fetch_pc(cpu); // cycle 2
    TRACE(cpu->trace_entry.operand = operand; )
    // conditions:
    // 6502
    // 816 in e-mode, dl is zero
    // 816 in e-mode, dl nonzero, or 816 in n-mode
    uint16_t eaddr_16;

    if constexpr (!CPUTraits::has_65816_ops) {
        eaddr_16 = (uint8_t)(operand + index);
        phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu))); // cycle 3
    } else {
        if constexpr (CPUTraits::e_mode) {
            if ((cpu->d & 0xFF) == 0) {
                eaddr_16 = (cpu->d | (uint8_t) (operand + index));
            } else {
                eaddr_16 = (cpu->d + operand + index); // with just direct there can be no page wrapping.
            }
        } else {
            eaddr_16 = (cpu->d + operand + index);
        }

        if (cpu->d & 0xFF) { // extra cycle taken if DP low is non-zero
            phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc)); // cycle 2a - PhantomIgn: PC+1 (current PC)
        }
    }
    return eaddr_16;
}

/** Address Mode: 16a. Direct d, X (Read) */
template<typename T, typename U>
inline void read_direct_x(cpu_state *cpu, T &reg, U &index ) {

    uint16_t eaddr_16 = address_direct_x(cpu, index);

    read_data_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 16a. Direct d, X (Write) */
template<typename T, typename U>
inline void write_direct_x(cpu_state *cpu, T &reg, U &index ) {

    uint16_t eaddr_16 = address_direct_x(cpu, index); // cycle 2

    write_data_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 16b. Direct, X (RMW) */
template<typename T,typename Operation>
inline void rmw_direct_x(cpu_state *cpu, Operation operation) {
    uint16_t eaddr_16 = address_direct_x(cpu, _X(cpu)); // these are only X here..
    alu_t reg;

    read_data_direct(cpu, eaddr_16, reg);

    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) // acts like nmos
            phantom_write(cpu, eaddr_16, reg);
        else { // addr based on reg size
            if constexpr (sizeof(reg) == 1) phantom_read(cpu, eaddr_16);
            else phantom_read(cpu, (uint16_t)(eaddr_16+1));
        }  
    } else if constexpr (CPUTraits::has_65c02_ops) { // 65c02 used a read
        phantom_read(cpu, eaddr_16);
    } else { // nmos used a write
        phantom_write(cpu, eaddr_16, reg);
    }

    operation(cpu, reg);

    write_tada_direct(cpu, eaddr_16, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 11. Direct Indexed Indirect (d,x) */

template<typename T, typename U>
inline void read_direct_x_ind(cpu_state *cpu, T &reg, U &index ) {
    uint16_t eaddr_16 = address_direct_x(cpu, index);
    uint16_t temp;
    uint32_t eaddr;

    if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode) {
        read_data_direct(cpu, eaddr_16, temp);
        eaddr = temp;
    } else {
    //    if constexpr (CPUTraits::e_mode) {
        // in e mode the +1 needs to wrap inside its page.
        eaddr = bus_read(cpu, (uint16_t)(eaddr_16));
        eaddr |= bus_read(cpu, (uint16_t)(eaddr_16 & 0xFF00) | (uint8_t)((eaddr_16 & 0x00FF) + 1)) << 8;
    }
    if constexpr (CPUTraits::has_65816_ops) eaddr = (cpu->full_db | eaddr); // add data bank

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 11. Direct Indexed Indirect (d,x) (Write) */
template<typename T, typename U>
inline void write_direct_x_ind(cpu_state *cpu, T &reg, U &index ) {
    uint16_t eaddr_16 = address_direct_x(cpu, index); // cycle 2
    uint16_t temp;
    uint32_t eaddr;

    if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode) {
        read_data_direct(cpu, eaddr_16, temp);
        eaddr = temp;
    } else {
        // in e mode the +1 needs to wrap inside its page.
        eaddr = bus_read(cpu, (uint16_t)(eaddr_16));
        eaddr |= bus_read(cpu, (uint16_t)(eaddr_16 & 0xFF00) | (uint8_t)((eaddr_16 & 0x00FF) + 1)) << 8;
    }    
    if constexpr (CPUTraits::has_65816_ops) eaddr = (cpu->full_db | eaddr); // add data bank

    write_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr_16; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 12. Direct Indirect         (d) */

uint32_t address_direct_indirect(cpu_state *cpu) {
    uint16_t eaddr_16 = address_direct(cpu); // cycle 2

    uint32_t eaddr = bus_read(cpu, (uint16_t)(eaddr_16)); // cycle 3
    eaddr |= bus_read(cpu, (uint16_t)(eaddr_16+1)) << 8; // cycle 4
    if constexpr (CPUTraits::has_65816_ops) eaddr |= cpu->full_db; // cycle 5
    TRACE(cpu->trace_entry.eaddr = eaddr;)
    return eaddr;
}

template<typename T>
inline void read_direct_indirect(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_direct_indirect(cpu); // cycle 2

    read_data(cpu, eaddr, reg);

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T>
inline void write_direct_indirect(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_direct_indirect(cpu); // cycle 2

    write_data(cpu, eaddr, reg);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}


/** Address Mode: 13. Direct Indirect Indexed  (d),y  (Read) */
/* Add 1 cycle for indexing across page boundaries; (a) X = 1 or EM or 6502 , with page crossing
                   or write,
                   or X=0. (b)
   When X=1 or in the emulation mode, this cycle contains invalid addresses (c)*/

template<typename T, typename U>
inline void read_direct_ind_x(cpu_state *cpu, T &reg, U &index ) {

    // Get operand from PC and load data word from that DP address
    uint16_t eaddr_16 = address_direct_indirect(cpu);
    
    uint32_t base;
    uint32_t eaddr;

    //phantom_read needed here similar to address_abs_x
    if constexpr (CPUTraits::has_65816_ops) {
        base = (cpu->full_db | eaddr_16); // add data bank
        eaddr = /* eaddr_16 */ base + index; // calculate effective address

        if constexpr ((CPUTraits::e_mode) || (!CPUTraits::x_16)) { // (c)
            if ((eaddr & 0xFF00) != (base & 0xFF00)) {
                phantom_read(cpu, (eaddr & 0xFFFF00) | ((eaddr + index) & 0xFF));
            }
        } else {  // (b)
            phantom_read(cpu, eaddr);
        }
        read_data(cpu, eaddr, reg);
    } else { // 65c02 and NMOS 6502
        base = eaddr_16;
        eaddr = (uint16_t)(base + index); // calculate effective address

        if ((eaddr & 0xFF00) != (base & 0xFF00)) {
            phantom_read(cpu, (eaddr & 0x00FF00) | ((eaddr + index) & 0xFF));
        }
        read_data(cpu, (uint16_t)eaddr, reg);
    }

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T, typename U>
inline void write_direct_ind_x(cpu_state *cpu, T &reg, U &index ) {

    // Get operand from PC and load data word from that DP address
    uint16_t eaddr_16 = address_direct_indirect(cpu);
    
    uint32_t base;
    uint32_t eaddr;

    //phantom_read needed here similar to address_abs_x
    if constexpr (CPUTraits::has_65816_ops) {
        base = cpu->full_db | eaddr_16; // add data bank
        eaddr = base + index; // calculate effective address

        if constexpr ((CPUTraits::e_mode) || (!CPUTraits::x_16)) { // (c)
            phantom_read(cpu, (eaddr & 0xFFFF00) | ((eaddr + index) & 0xFF));
        } else {  // (b)
            phantom_read(cpu, eaddr);
        }
        write_data(cpu, eaddr, reg);
    } else { // 65c02 and NMOS 6502
        base = eaddr_16;
        eaddr = (uint16_t)(base + index); // calculate effective address

        phantom_read(cpu, (eaddr & 0x00FF00) | ((eaddr + index) & 0xFF));
        write_data(cpu, (uint16_t)eaddr, reg);
    }

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** Address Mode: 14. Direct Indirect Indexed Long  [d],y  (Read) */
template<typename T, typename U>
inline void read_direct_ind_x_long(cpu_state *cpu, T &reg, U &index ) {
    uint32_t eaddr = address_direct_indirect_long(cpu);
    eaddr += index;
    read_data(cpu, eaddr, reg);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T, typename U>
inline void write_direct_ind_x_long(cpu_state *cpu, T &reg, U &index ) {
    uint32_t eaddr = address_direct_indirect_long(cpu);
    eaddr += index;
    write_data(cpu, eaddr, reg);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 15. Direct Indirect Long    [d] */

uint32_t address_direct_indirect_long(cpu_state *cpu) {
    uint16_t eaddr_16 = address_direct(cpu); // cycle 2

    uint32_t eaddr = bus_read(cpu, (uint16_t)(eaddr_16)); // cycle 3
    eaddr |= bus_read(cpu, (uint16_t)(eaddr_16+1)) << 8; // cycle 4
    eaddr |= bus_read(cpu, (uint16_t)(eaddr_16+2)) << 16; // cycle 5
    //TRACE(cpu->trace_entry.eaddr = eaddr;)
    return eaddr;
}

template<typename T>
inline void read_direct_ind_long(cpu_state *cpu, T &reg) {
    uint32_t eaddr = address_direct_indirect_long(cpu);
    read_data(cpu, eaddr, reg);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

template<typename T>
inline void write_direct_ind_long(cpu_state *cpu, T &reg ) {
    uint32_t eaddr = address_direct_indirect_long(cpu);
    write_data(cpu, eaddr, reg);
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = sizeof(T)-1;)
}

/** 23. Stack Relative    d, s */

inline uint16_t address_stack_relative(cpu_state *cpu) {
    uint8_t offset = fetch_pc(cpu);
    uint16_t eaddr = cpu->sp + offset;
    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc)); // cycle 3
    TRACE(cpu->trace_entry.operand = offset;)
    return eaddr;
}

template<typename T>
inline void read_stack_relative(cpu_state *cpu, T &reg) {
    uint16_t eaddr = address_stack_relative(cpu);

    if constexpr (is_byte<T>) {
        reg = bus_read(cpu, eaddr);
        TRACE(cpu->trace_entry.f_data_sz = 0;)
    }
    if constexpr (is_word<T>) { // TODO: unsure about wrapping semantics here, but probably forced to stay on bank 0
        reg = bus_read(cpu, eaddr);
        reg |= (bus_read(cpu, (uint16_t)(eaddr+1)) << 8);
        TRACE(cpu->trace_entry.f_data_sz = 1;)
    }
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg;)
}

template<typename T>
inline void write_stack_relative(cpu_state *cpu, T &reg) {
    uint16_t eaddr = address_stack_relative(cpu);

    if constexpr (is_byte<T>) {
        write_data(cpu, eaddr, reg);
        TRACE(cpu->trace_entry.f_data_sz = 0;)
    }
    if constexpr (is_word<T>) { // TODO: unsure about wrapping semantics here, but probably forced to stay on bank 0
        write_data(cpu, eaddr, reg);
        //write_data(cpu, (uint16_t)(eaddr+1), word_hi(reg));
        TRACE(cpu->trace_entry.f_data_sz = 1;)
    }
    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg;)
}

template<typename T>
inline void read_stack_relative_y(cpu_state *cpu, T &reg) {
    uint16_t addr = address_stack_relative(cpu); // cycle 1-3

    uint32_t base = bus_read(cpu, (uint16_t)(addr)); // cycle 4
    base |= (bus_read(cpu, (uint16_t)(addr+1)) << 8); // cycle 5
    
    phantom_read(cpu, cpu->sp); // cycle 6

    base |= (cpu->full_db);
    uint32_t eaddr = base + _Y(cpu);

    if constexpr (is_byte<T>) {
        reg = bus_read(cpu, eaddr); // cycle 7
    }
    if constexpr (is_word<T>) { 
        reg = bus_read(cpu, eaddr); // cycle 7
        reg |= (bus_read(cpu, (eaddr+1)) << 8); // cycle 7a
    }

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = 1;)
}

template<typename T>
inline void write_stack_relative_y(cpu_state *cpu, T &reg) {
    uint16_t addr = address_stack_relative(cpu); // cycle 1-3

    uint32_t base = bus_read(cpu, (uint16_t)(addr)); // cycle 4
    base |= (bus_read(cpu, (uint16_t)(addr+1)) << 8); // cycle 5
    
    phantom_read(cpu, cpu->sp); // cycle 6

    base |= cpu->full_db;
    uint32_t eaddr = base + _Y(cpu);

    if constexpr (is_byte<T>) {
        bus_write(cpu, eaddr, reg); // cycle 7
    }
    if constexpr (is_word<T>) {
        bus_write(cpu, eaddr, reg); // cycle 7
        bus_write(cpu, (eaddr+1), reg >> 8); // cycle 7a
    }

    TRACE(cpu->trace_entry.eaddr = eaddr; cpu->trace_entry.data = reg; cpu->trace_entry.f_data_sz = 1;)
}

/** Immediate mode */

/** Stack Register Transfers */

template<typename T>
inline void transfer_x_s(cpu_state *cpu, T &reg) {
    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) {
            cpu->sp_lo = word_lo(reg);
            cpu->sp_hi = 0x01;
        } else {
            // if x is 8-bit:  top byte will be zero (8 bit extended to 16-bit)
            // if x is 16-bit, this will be a 16-bit transfer
            cpu->sp = reg; 
        }
    } else {
        cpu->sp_lo = reg;
        cpu->sp_hi = 0x01; // in 6502 this needs to stay 0 for now cuz push_byte and pop_byte relied on it..
    }
    phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
}

template<typename T>
inline void transfer_s_reg(cpu_state *cpu, T &reg) {
    reg = cpu->sp;
    set_n_z_flags(cpu, reg);

    phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
}

// TODO: transfer size is determined by destination register size.

template<typename T, typename U>
inline void transfer_reg_reg(cpu_state *cpu, T &S, U &D) {
    D = S;
    set_n_z_flags(cpu, D);
    phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
    TRACE(cpu->trace_entry.data = S;)
}


/**
 * This group of routines implement reused addressing mode calculations.
 *
 */
/**
 * Set the N and Z flags based on the value. Used by all arithmetic instructions, as well
 * as load instructions.
 */
template<typename T>
static inline void set_n_z_flags(cpu_state *cpu, T value) {
    cpu->Z = (value == 0);
    if constexpr (is_word<T>) {
        cpu->N = (value & 0x8000) != 0; // 16-bit: check bit 15
    } else {
        cpu->N = (value & 0x80) != 0;   // 8-bit: check bit 7
    }
}

template<typename T>
static inline void set_n_z_v_flags(cpu_state *cpu, T value, T N) {
    cpu->Z = (value == 0); // is A & M zero?
    if constexpr (is_word<T>) {
        cpu->N = (N & 0x8000) != 0; // 16-bit: check bit 15
        cpu->V = (N & 0x4000) != 0; // 16-bit: check bit 14
    } else {
        cpu->N = (N & 0x80) != 0;   // 8-bit: check bit 7
        cpu->V = (N & 0x40) != 0;   // 8-bit: check bit 6
    }
}

/* only set Z flag - used by some 65c02 instructions */
template<typename T>
static inline void set_z_flag(cpu_state *cpu, T value) {
    cpu->Z = (value == 0);
}

/**
 * Convert a binary-coded decimal byte to an integer.
 * Each nibble represents a decimal digit (0-9).
 */
inline uint8_t bcd_to_int(byte_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

inline uint16_t bcd_to_int(word_t bcd) {
    return ((bcd >> 12) * 1000) + 
           ((bcd >> 8 & 0x000F) * 100) +
           ((bcd >> 4 & 0x000F) * 10) + 
           (bcd & 0x0F);
}

/**
 * Convert an integer (0-99) to binary-coded decimal format.
 * Returns a byte with each nibble representing a decimal digit.
 */
inline byte_t int_to_bcd(uint8_t n) {
    uint8_t n1 = n % 100;
    return ((n1 / 10) << 4) | (n1 % 10);
}

inline word_t int_to_bcd16(word_t n) {
    uint16_t n1 = n % 10000;
    return ((n1 / 1000) << 12) | (((n1 / 100) % 10) << 8) | 
           (((n1 / 10) % 10) << 4) | (n1 % 10);
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
#if 0        
        uint8_t M = bcd_to_int(cpu->a_lo);
        uint8_t N1 = bcd_to_int(N);
        uint8_t S8 = M + N1 + cpu->C;
        cpu->a_lo = int_to_bcd(S8);
        cpu->C = (S8 > 99);
        if constexpr (CPUTraits::has_65c02_ops) {
            // TODO: handle V flag
            cpu->V =  !((M ^ N) & 0x80) && ((M ^ S8) & 0x80); // from 6502.org article https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
            cpu->incr_cycles();
        }
        set_n_z_flags(cpu, cpu->a_lo);
#else
        add_bcd_8_set_flags(cpu, cpu->a_lo, N);
#endif
    }
}

inline void add_and_set_flags(cpu_state *cpu, word_t N) {
    if (cpu->D == 0) {  // binary mode
        word_t M = _A(cpu);
        uint32_t S = M + N + (cpu->C);
        word_t S16 = (word_t) S;
        _A(cpu) = S16;
        cpu->C = (S & 0x1'0000) >> 16;
        cpu->V =  !((M ^ N) & 0x8000) && ((M ^ S16) & 0x8000); // from 6502.org article https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
        set_n_z_flags(cpu, S16);
    } else {              // TODO: need to 16 bit-ify this decimal mode
#if 0
        uint16_t bcd_a = _A(cpu);

        uint16_t M = bcd_to_int(_A(cpu));
        uint16_t N1 = bcd_to_int(N);
        // to detect V, we need this to be 32-bit int.
        uint32_t S16 = M + N1 + cpu->C;
        _A(cpu) = int_to_bcd16((uint16_t)S16);
        cpu->C = (S16 > 9999);
        uint16_t bcd_s = _A(cpu);
        // TODO: handle V flag - must be done on actual values not intermediate representation
        cpu->V = cpu->V =  !((bcd_a ^ N) & 0x8000) && ((bcd_a ^ bcd_s) & 0x8000);

        // 816 takes no additional cycles here.       
        set_n_z_flags(cpu, _A(cpu));
#else
        add_bcd_16_set_flags(cpu, N);
#endif
    }
}

inline void add_bcd_8_set_flags(cpu_state *cpu, uint8_t &a, uint8_t b) {

    uint16_t AL = (a & 0x0F) + (b & 0x0F) + cpu->C;

    if (AL >= 0x0A) AL = ((AL + 0x06) & 0x0F) + 0x10;

    uint16_t UA = (a & 0xF0) + (b & 0xF0) + AL;
    if (UA >= 0xA0) UA += 0x60;

    int16_t SA = (int8_t)(a & 0xF0) + (int8_t)(b & 0xF0) + (int16_t)AL;
    
    //printf("UA: %04X  SA: %04X\n", UA, SA);
    
    //printf("Result: %02X + %02X = %02X\n", a, b, UA);
    
    a = UA & 0xFF;
    cpu->C = (UA >= 0x100);
    cpu->Z = (a == 0);
    cpu->V = (SA < -128 || SA > 127);
    cpu->N = (UA & 0x80) != 0;
    //printf("P: %02X C: %d  Z: %d  V: %d N: %d\n", cpu.P, cpu.c, cpu.z, cpu.v, cpu.n);

}

inline void add_bcd_16_set_flags(cpu_state *cpu, word_t b) {
    uint8_t r_lo = word_lo(_A(cpu));
    uint8_t r_hi = word_hi(_A(cpu));
    add_bcd_8_set_flags(cpu, r_lo,  word_lo(b));
    add_bcd_8_set_flags(cpu, r_hi, word_hi(b));

    _A(cpu) = r_hi << 8 | r_lo;
    cpu->Z = (_A(cpu) == 0); // 16-bit zero?

}

inline void sub_bcd_8_set_flags( cpu_state *cpu, uint8_t &a, uint8_t b) {
    
    uint8_t N1 = b ^ 0xFF;

    int16_t AL = (a & 0x0F) - (b & 0x0F) + cpu->C - 1;

    if (AL < 0) AL = ((AL - 0x06) & 0x0F) - 0x10;

    int16_t UA = (a & 0xF0) - (b & 0xF0) + AL;
    
    // set based on intermediate result before final correction?
    cpu->V = !((a ^ N1) & 0x80) && ((a ^ UA) & 0x80);

    if (UA < 0 ) UA -= 0x60;

    //printf("UA: %04X  \n", UA);
    
    //printf("Result: %02X - %02X = %02X\n", a, b, UA);

    a = UA & 0xFF;
    
    cpu->C = !(UA < 0);
    cpu->Z = (a == 0);
    
    cpu->N = (UA & 0x80) != 0;
    //printf("P: %02X C: %d  Z: %d  V: %d N: %d\n", cpu.P, cpu.c, cpu.z, cpu.v, cpu.n);
}

inline void sub_bcd_16_set_flags( cpu_state *cpu, uint16_t b) {
    uint8_t r_lo = word_lo(_A(cpu));
    uint8_t r_hi = word_hi(_A(cpu));

    sub_bcd_8_set_flags(cpu, r_lo, word_lo(b));
    sub_bcd_8_set_flags(cpu, r_hi, word_hi(b));
    _A(cpu) = r_hi << 8 | r_lo;

    cpu->Z = (_A(cpu) == 0); // 16-bit zero?

    //cpu.v = (b > a);
    //cpu.v = !((a ^ N1) & 0x8000) && ((a ^ result) & 0x8000);
    //printf("Result: %04X - %04X = %04X\n", a, b, result);

    //printf("P: %02X C: %d  Z: %d  V: %d N: %d\n", cpu.P, cpu.c, cpu.z, cpu.v, cpu.n);
}

// see https://www.righto.com/2012/12/the-6502-overflow-flag-explained.html?m=1
// "Subtraction on the 6502" section.

/* Does subtraction based on 3 inputs: M, N, and C. 
 * Sets flags, returns the result of the subtraction. But does not store it in the accumulator.
 * Caller may store the result in the accumulator if desired.
   This is no longer used. Deprecated.
 */
/* inline uint8_t subtract_core(cpu_state *cpu, uint8_t M, uint8_t N, uint8_t C) {
    uint8_t N1 = N ^ 0xFF;
    uint32_t S = M + N1 + C;
    uint8_t S8 = (uint8_t) S;
    cpu->C = (S & 0x0100) >> 8;
    cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
    set_n_z_flags(cpu, S8);
    return S8;
} */

inline void subtract_and_set_flags(cpu_state *cpu, uint8_t N) {
    uint8_t C = cpu->C;

    if (cpu->D == 0) {
        uint8_t M = cpu->a_lo;

        uint8_t N1 = N ^ 0xFF;
        uint32_t S = M + N1 + C;
        uint8_t S8 = (uint8_t) S;
        // cpu->C = (S & 0x0100) >> 8; // awkward
        cpu->C = (S & 0x0100) != 0; // might be faster.
        cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
        cpu->a_lo = S8; // store the result in the accumulator. I accidentally deleted this before.
        set_n_z_flags(cpu, S8);
    } else {
#if 0
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
            cpu->incr_cycles();
        }
        set_n_z_flags(cpu, cpu->a_lo);
#else
        sub_bcd_8_set_flags(cpu, cpu->a_lo, N);
#endif
    }
}

// subtract from 16-bit accumulator
inline void subtract_and_set_flags(cpu_state *cpu, word_t N) {
    byte_t C = cpu->C;

    if (cpu->D == 0) {
        word_t M = _A(cpu);

        word_t N1 = N ^ 0xFFFF;
        uint32_t S = M + N1 + C;
        word_t S16 = (word_t) S;
        //cpu->C = (S & 0x1'0000) >> 16;
        cpu->C = (S & 0x1'0000) != 0;
        cpu->V =  !((M ^ N1) & 0x8000) && ((M ^ S16) & 0x8000);
        _A(cpu) = S16; 
        set_n_z_flags(cpu, S16);
    } else { // TODO: needs to be 16bit-ified
#if 0
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
            cpu->incr_cycles();
        }
        set_n_z_flags(cpu, cpu->a_lo);
#else
        sub_bcd_16_set_flags(cpu, N);
#endif
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

inline void compare_and_set_flags(cpu_state *cpu, word_t M, word_t N) {
    uint8_t C = 1;
    word_t N1 = N ^ 0xFFFF;
    uint32_t S = M + N1 + C;
    word_t S16 = (word_t) S;
    cpu->C = (S & 0x1'0000) >> 16;
    //cpu->V =  !((M ^ N1) & 0x80) && ((M ^ S8) & 0x80);
    set_n_z_flags(cpu, S16);
}

/**
 * Good discussion of branch instructions:
 * https://www.masswerk.at/6502/6502_instruction_set.html#BCC
 */
inline void branch_if(cpu_state *cpu, uint8_t N, bool condition) {
    uint16_t oaddr = cpu->pc;
    uint16_t taddr = oaddr + (int8_t) N;

    if (condition) {
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

/* inline zpaddr_t get_operand_address_zeropage_x(cpu_state *cpu) {
    zpaddr_t zpaddr = read_byte_from_pc(cpu);
    zpaddr_t taddr = zpaddr + cpu->x_lo; // make sure it wraps.
    cpu->incr_cycles(); // ZP,X adds a cycle.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
} */

/* inline zpaddr_t get_operand_address_zeropage_y(cpu_state *cpu) {
    zpaddr_t zpaddr = read_byte_from_pc(cpu);
    zpaddr_t taddr = zpaddr + cpu->y_lo; // make sure it wraps.
    // TODO: if it wraps add another cycle.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
} */

inline absaddr_t get_operand_address_absolute_indirect(cpu_state *cpu) {
    absaddr_t addr = read_word_from_pc(cpu);
    absaddr_t taddr = read_word(cpu,addr);
    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

uint32_t make_pb_addr(cpu_state *cpu, uint16_t addr) {
    if constexpr (CPUTraits::has_65816_ops) {
        return (cpu->pb << 16) | addr;
    } else {
        return addr;
    }
}

inline absaddr_t get_operand_address_absolute_indirect_x(cpu_state *cpu) {
    uint16_t addr = fetch_pc(cpu);
    addr |= fetch_pc(cpu) << 8;

    //absaddr_t addr = read_word_from_pc(cpu);
/*     printf("addr: %04X\n", addr);
    printf("7e/7000: %02X %02X %02X",
        cpu->mmu->read(0x7E7000),
        cpu->mmu->read(0x7E7001),
        cpu->mmu->read(0x7E7002)); */
    phantom_read_ign(cpu, _PC(cpu));

    absaddr_t taddr;
    if constexpr (CPUTraits::has_65816_ops) {
        // '816 pulls this from the Program Bank, not the Data Bank.
        uint16_t base = addr + _X(cpu);

        taddr = bus_read(cpu, make_pb_addr(cpu, (uint16_t) base));
        taddr |= bus_read(cpu, make_pb_addr(cpu, (uint16_t) base) + 1) << 8; 
    } else {
        taddr = read_word(cpu,(uint16_t)(addr + cpu->x_lo));  
    }

    TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}


// 65c02 only address mode
/* inline uint16_t get_operand_address_zeropage_indirect(cpu_state *cpu) {
    zpaddr_t zpaddr = read_byte_from_pc(cpu);
    absaddr_t taddr = read_word(cpu,(uint8_t)zpaddr); // make sure it wraps.
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
} */

/* inline uint16_t get_operand_address_indirect_x(cpu_state *cpu) {
    zpaddr_t zpaddr = read_byte_from_pc(cpu);
    absaddr_t taddr = read_word(cpu,(uint8_t)(zpaddr + cpu->x_lo)); // make sure it wraps.
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
} */

/* inline absaddr_t get_operand_address_indirect_y(cpu_state *cpu) {
    zpaddr_t zpaddr = read_byte_from_pc(cpu);
    absaddr_t iaddr = read_word(cpu,zpaddr);
    absaddr_t taddr = iaddr + cpu->y_lo;

    if ((iaddr & 0xFF00) != (taddr & 0xFF00)) {
        cpu->incr_cycles();
    }
    //printf("zpaddr: %02X iaddr: %04X taddr: %04X\n", zpaddr, iaddr, taddr);
    TRACE(cpu->trace_entry.operand = zpaddr; cpu->trace_entry.eaddr = taddr;)
    return taddr;
} */

/* inline byte_t get_operand_zeropage_x(cpu_state *cpu) {
    zpaddr_t addr = get_operand_address_zeropage_x(cpu);
    byte_t N = read_byte(cpu,addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
}
 */
/* inline void store_operand_zeropage_x(cpu_state *cpu, byte_t N) {
    zpaddr_t addr = get_operand_address_zeropage_x(cpu);
    write_byte(cpu,addr, N);
    TRACE(cpu->trace_entry.data = N;)
} */

/* inline byte_t get_operand_zeropage_y(cpu_state *cpu) {
    zpaddr_t zpaddr = get_operand_address_zeropage_y(cpu);
    byte_t N = read_byte(cpu,zpaddr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
} */

/* inline void store_operand_zeropage_y(cpu_state *cpu, byte_t N) {
    zpaddr_t zpaddr = get_operand_address_zeropage_y(cpu);
    write_byte(cpu,zpaddr, N);
    TRACE(cpu->trace_entry.data = N;)
} */

/* inline byte_t get_operand_zeropage_indirect_x(cpu_state *cpu) {
    absaddr_t taddr = get_operand_address_indirect_x(cpu);
    byte_t N = read_byte(cpu,taddr);
    TRACE( cpu->trace_entry.data = N;)
    return N;
}

inline void store_operand_zeropage_indirect_x(cpu_state *cpu, byte_t N) {
    absaddr_t taddr = get_operand_address_indirect_x(cpu);
    write_byte(cpu,taddr, N);
    TRACE( cpu->trace_entry.data = N;)
} */

/* inline byte_t get_operand_zeropage_indirect_y(cpu_state *cpu) {
    absaddr_t addr = get_operand_address_indirect_y(cpu);
    byte_t N = read_byte(cpu,addr);
    TRACE(cpu->trace_entry.data = N;)
    return N;
} */

/* inline void store_operand_zeropage_indirect_y(cpu_state *cpu, byte_t N) {
    absaddr_t addr = get_operand_address_indirect_y(cpu);
    cpu->incr_cycles(); // TODO: where should this extra cycle actually go?
    write_byte(cpu,addr, N);
    TRACE(cpu->trace_entry.data = N;)
} */

inline byte_t address_relative(cpu_state *cpu) {
    //byte_t N = read_byte_from_pc(cpu);
    byte_t N = fetch_pc(cpu);
    TRACE(cpu->trace_entry.operand = N;)
    return N;
}

inline uint16_t address_relative_long(cpu_state *cpu) {
    uint16_t N = fetch_pc(cpu);
    N |= fetch_pc(cpu) << 8;

    TRACE(cpu->trace_entry.operand = N; cpu->trace_entry.f_op_sz = 2;)
    return N;
}

/**
 * Decrement an operand.
 */
/* inline void dec_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr);
    N--;
    cpu->incr_cycles();
    write_byte(cpu,addr, N);
    set_n_z_flags(cpu, N);
    TRACE( cpu->trace_entry.data = N;)
} */

/**
 * Increment an operand.
 */


/* inline void inc_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr); // in abs,x this is completion of T4
    
    // T5 write original value
    write_byte(cpu,addr,N); 
    N++;
    
    // T6 write new value
    set_n_z_flags(cpu, N);
    write_byte(cpu,addr, N);

    TRACE(cpu->trace_entry.data = N;)
} */

inline byte_t logical_shift_right(cpu_state *cpu, byte_t N) {
    uint8_t C = N & 0x01;
    N = N >> 1;
    cpu->C = C;
    set_n_z_flags(cpu, N);
    cpu->incr_cycles();
    return N;
}

/* inline byte_t logical_shift_right_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr);
    byte_t result = logical_shift_right(cpu, N);
    write_byte(cpu,addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
} */

inline byte_t arithmetic_shift_left(cpu_state *cpu, byte_t N) {
    uint8_t C = (N & 0x80) >> 7;
    N = N << 1;
    cpu->C = C;
    set_n_z_flags(cpu, N);
    cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

/* inline byte_t arithmetic_shift_left_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr);
    byte_t result = arithmetic_shift_left(cpu, N);
    write_byte(cpu,addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
} */


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

/* inline byte_t rotate_right_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr);
    byte_t result = rotate_right(cpu, N);
    write_byte(cpu,addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
} */

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
/* 
inline byte_t rotate_left_addr(cpu_state *cpu, absaddr_t addr) {
    byte_t N = read_byte(cpu,addr);
    byte_t result = rotate_left(cpu, N);
    write_byte(cpu,addr, result);
    TRACE(cpu->trace_entry.data = result;)
    return result;
} */

/** Stack Helpers */
inline void push_byte(cpu_state *cpu, byte_t N) {
    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) { // in emulation mode sp_hi forced to 0x01
            bus_write(cpu, (uint16_t)(0x0100 | cpu->sp_lo), N);
            cpu->sp_lo = (uint8_t)(cpu->sp_lo - 1);
        } else { // in any native mode, sp is 16-bit.
            bus_write(cpu, cpu->sp, N);
            cpu->sp = cpu->sp - 1;
        }
    } else { // any 6502 mode, sp is just 8-bit.
        bus_write(cpu,(uint16_t)(0x0100 | cpu->sp_lo), N);
        cpu->sp = (uint8_t)(cpu->sp - 1);
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t pop_byte(cpu_state *cpu) {
    byte_t N;
    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) {
            cpu->sp_lo = (uint8_t)(cpu->sp_lo + 1);
            N = bus_read(cpu,(uint16_t)(0x0100 | cpu->sp_lo));
        } else {
            cpu->sp = (uint16_t)(cpu->sp + 1);
            N = bus_read(cpu, cpu->sp);
        }
    } else {
        cpu->sp = (uint8_t)(cpu->sp + 1);
        N = bus_read(cpu,(uint16_t)(0x0100 | cpu->sp_lo));
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline byte_t pop_byte_nocycle(cpu_state *cpu) {
    cpu->sp = (uint8_t)(cpu->sp + 1);
    //cpu->incr_cycles();
    byte_t N = bus_read(cpu,(uint16_t)(0x0100 | cpu->sp_lo));
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void push_word(cpu_state *cpu, word_t N) {
    // TODO: there are a couple cases (PEA?) where stack can go out of bounds even in emulation mode.
    if constexpr ((CPUTraits::has_65816_ops) && (!CPUTraits::e_mode)) {
        bus_write(cpu, (uint16_t) cpu->sp, (N & 0xFF00) >> 8);
        bus_write(cpu, (uint16_t) (cpu->sp - 1), N & 0x00FF);
        cpu->sp = cpu->sp - 2;
    } else {
        bus_write(cpu,0x0100 | (uint8_t) cpu->sp_lo, (N & 0xFF00) >> 8);
        bus_write(cpu,0x0100 | (uint8_t) (cpu->sp_lo - 1), N & 0x00FF);
        cpu->sp_lo = (uint8_t)(cpu->sp_lo - 2); // only change sp_lo
    }
    TRACE(cpu->trace_entry.data = N;)
}

inline void push_byte_new(cpu_state *cpu, byte_t N) {
    if constexpr (CPUTraits::has_65816_ops) {
        bus_write(cpu, cpu->sp, N);
        cpu->sp = cpu->sp - 1;
    } else { // any 6502 mode, sp is just 8-bit.
        // compile-time assert failure here.
        static_assert(!CPUTraits::has_65816_ops, "push_byte_new called in 6502 mode");
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline void push_word_new(cpu_state *cpu, word_t N) {
    if constexpr (CPUTraits::has_65816_ops) {
        bus_write(cpu, cpu->sp, word_hi(N) );
        cpu->sp = cpu->sp - 1;
        bus_write(cpu, cpu->sp, word_lo(N));
        cpu->sp = cpu->sp - 1;
    } else { // any 6502 mode, sp is just 8-bit.
        // compile-time assert failure here.
        static_assert(!CPUTraits::has_65816_ops, "push_word_new called in 6502 mode");
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline byte_t pop_byte_new(cpu_state *cpu) {
    byte_t N;
    if constexpr (CPUTraits::has_65816_ops) {
        cpu->sp = (uint16_t)(cpu->sp + 1);
        N = bus_read(cpu, cpu->sp);
    } else {
        static_assert(!CPUTraits::has_65816_ops, "pop_byte_new called in 6502 mode");
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
    return N;
}

inline void pop_word_new(cpu_state *cpu, word_t &N) {
    if constexpr (CPUTraits::has_65816_ops) {
        cpu->sp = (uint16_t) (cpu->sp + 1);
        N = bus_read(cpu, cpu->sp );
        cpu->sp = (uint16_t) (cpu->sp + 1);
        N |= bus_read(cpu, cpu->sp ) << 8;
    } else { // any 6502 mode, sp is just 8-bit.
        // compile-time assert failure here.
        static_assert(!CPUTraits::has_65816_ops, "pop_word_new called in 6502 mode");
    }
    //cpu->incr_cycles();
    TRACE(cpu->trace_entry.data = N;)
}

inline void stack_fix_new(cpu_state *cpu) {
    if constexpr (CPUTraits::has_65816_ops) {
        if constexpr (CPUTraits::e_mode) {
            cpu->sp_hi = 0x01;
        }
    } else {
        // compile-time assert failure here.
        static_assert(!CPUTraits::has_65816_ops, "stack_fix_new called in 6502 mode");
    }
}

/* 22b. Stack s - pld, plb, plp, pla, plx, ply */
template<typename T>
inline void stack_pull(cpu_state *cpu, T &reg) {
    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));

    if constexpr (sizeof(T) == 1) {
        reg = pop_byte(cpu);
    } else if constexpr (sizeof(T) == 2) {
        reg = pop_word(cpu);
    }
}


/* 22c. Stack s - pha, phb, php, pdh, phk, phx, phy */
template<typename T>
inline void stack_push(cpu_state *cpu, T &reg) {
    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
    if constexpr (sizeof(T) == 1) {
        push_byte(cpu, reg);
    } else if constexpr (sizeof(T) == 2) {
        push_word(cpu, reg);
    }
}

/* 22i. Stack s - rtl */

inline void stack_rtl(cpu_state *cpu) {
    pop_word_new(cpu, cpu->pc);
    cpu->pb = pop_byte_new(cpu);
    stack_fix_new(cpu);
/*     stack_pull(cpu, cpu->pc);
    stack_pull(cpu, cpu->pb); */
}

/* 22j. Stack s - brk, cop */
inline void brk_cop(cpu_state *cpu, uint16_t vector) {
    uint8_t sign = fetch_pc(cpu); // ignore this, we don't need it.

    if constexpr ((CPUTraits::has_65816_ops)) {
        if constexpr (!CPUTraits::e_mode) push_byte(cpu, cpu->pb);
        
        push_word(cpu, cpu->pc); // pc of BRK signature byte
        
        // break flag and Unused bit set to 1 only in emu
        if constexpr (CPUTraits::e_mode) push_byte(cpu, cpu->p | FLAG_B | FLAG_UNUSED);
        else push_byte(cpu, cpu->p); 
        
        cpu->I = 1; // interrupt disable flag set to 1.
        cpu->D = 0; // turn off decimal mode on brk and interrupts
        cpu->pc = read_word_bank0(cpu,vector);
        cpu->pb = 0x00;
    } else {
        push_word(cpu, cpu->pc); // pc of BRK signature byte
        push_byte(cpu, cpu->p | FLAG_B | FLAG_UNUSED); // break flag and Unused bit set to 1.
        cpu->I = 1; // interrupt disable flag set to 1.
        if constexpr (CPUTraits::has_65c02_ops) {
            cpu->D = 0; // turn off decimal mode on brk and interrupts
        }
        cpu->pc = read_word_bank0(cpu,vector);
    }

}

/* older stack routines */

inline absaddr_t pop_word(cpu_state *cpu) {
    uint8_t w_l = pop_byte(cpu);
    uint8_t w_h = pop_byte(cpu);
    absaddr_t N = (w_h << 8) | w_l;

    TRACE(cpu->trace_entry.data = N;)
    return N;
}

/** --------------------------------- */

inline void move_memory(cpu_state *cpu) {
    uint8_t DBA = fetch_pc(cpu); // cycle 2
    uint8_t SBA = fetch_pc(cpu); // cycle 3

    cpu->db = SBA;
    uint8_t N = bus_read(cpu, (cpu->full_db | _X(cpu))); // cycle 4
    cpu->db = DBA;
    bus_write(cpu, (cpu->full_db | _Y(cpu)), N); // cycle 5

    phantom_read_ign(cpu, (cpu->full_db | _Y(cpu))); // cycle 6
    phantom_read_ign(cpu, (cpu->full_db | _Y(cpu))); // cycle 7
    if (cpu->a--) { // uses full accumulator
        cpu->pc = cpu->pc - 3;
    };
    // TODO: put the address written into the trace eaddr. Eventually we need both.
    TRACE(cpu->trace_entry.operand = (DBA << 8) | SBA; cpu->trace_entry.f_op_sz = 2; cpu->trace_entry.eaddr = (cpu->full_db | _Y(cpu));)
}

inline void invalid_opcode(cpu_state *cpu, opcode_t opcode) {
    fprintf(stdout, "Unknown opcode: %04X: 0x%02X", cpu->pc-1, ((unsigned int)opcode) & 0xFF);
    //cpu->halt = HLT_INSTRUCTION;
}

inline void invalid_nop(cpu_state *cpu, int bytes, int cycles) {
    cpu->pc += (bytes-1); // we already fetched the opcode, so just count bytes excl. that.
    for (uint16_t i = 0; i < cycles-1; i++) {
        cpu->incr_cycles();
    }
    /* cpu->cycles += (cycles-1); */
    TRACE(cpu->trace_entry.data = 0;)
}

public:

int execute_next(cpu_state *cpu) override {

    system_trace_entry_t *tb = &cpu->trace_entry;
    TRACE(
    if (cpu->trace) {
    tb->cycle = cpu->cycles;
    tb->pc = cpu->pc;
    tb->a = cpu->a;
    tb->x = cpu->x;
    tb->y = cpu->y;
    tb->sp = cpu->sp;
    tb->d = cpu->d;
    tb->p = cpu->p;
    tb->db = cpu->db;
    tb->pb = cpu->pb;
    tb->eaddr = 0;
    tb->flags = 0; // tb->p & (TRACE_FLAG_M | TRACE_FLAG_X);
    tb->unused = 0;
    }
    )

    if (cpu->skip_next_irq_check == 0 && !cpu->I && cpu->irq_asserted) { // if IRQ is not disabled, and IRQ is asserted, handle it.
        if constexpr ((CPUTraits::has_65816_ops)) {
            if constexpr (!CPUTraits::e_mode) push_byte(cpu, cpu->pb);
            push_word(cpu, cpu->pc); // push current PC
            
            // only set UNUSED FLAG IN EMULATION MODE.
            //assert((cpu->p & FLAG_B) == 0);
            if constexpr (CPUTraits::e_mode) push_byte(cpu, (cpu->p & ~FLAG_B) | FLAG_UNUSED); // break flag and Unused bit set to 1.
            else push_byte(cpu, cpu->p);

            cpu->I = 1; // interrupt disable flag set to 1.
            cpu->D = 0; // turn off decimal mode on brk and interrupts
            if constexpr (!CPUTraits::e_mode) cpu->pc = read_word_bank0(cpu,N_IRQ_VECTOR);
            else cpu->pc = read_word_bank0(cpu,IRQ_VECTOR);
            cpu->pb = 0x00;
            cpu->incr_cycles();
        } else {
            push_word(cpu, cpu->pc); // push current PC
            push_byte(cpu, (cpu->p & ~FLAG_B) | FLAG_UNUSED); // break flag = 0, Unused bit set to 1.
            cpu->I = 1; // interrupt disable flag set to 1.
            if constexpr (CPUTraits::has_65c02_ops) {
                cpu->D = 0; // turn off decimal mode on brk and interrupts
            }
            cpu->pc = read_word_bank0(cpu,IRQ_VECTOR);
            cpu->incr_cycles();
            //cpu->incr_cycles(); // todo might be one too many, we're at 8, refs say it's 7. push_byte takes an extra cycle now?
        }
        TRACE ( tb->eaddr = cpu->pc; tb->f_irq = 1;);
        TRACE(if (cpu->trace) cpu->trace_buffer->add_entry(cpu->trace_entry);)
        return 0;
    } 
    if (cpu->skip_next_irq_check > 0) {
        cpu->skip_next_irq_check--;
    }

    //opcode_t opcode = read_byte_from_pc(cpu);
    opcode_t opcode = fetch_pc(cpu);
    tb->opcode = opcode;

    switch (opcode) {

        /* ADC --------------------------------- */
        case OP_ADC_IMM: /* ADC Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ZP: /* ADC ZP */
            {
                alu_t N;
                read_direct(cpu, N);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ZP_X: /* ADC ZP, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND: /* ADC (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                add_and_set_flags(cpu, N);
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                add_and_set_flags(cpu, read_byte(cpu,addr)); */
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_ADC_ABS: /* ADC Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_X: /* ADC Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_ABS_Y: /* ADC Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                add_and_set_flags(cpu, N);
            }
            break;

        case OP_ADC_IND_X: /* ADC (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                add_and_set_flags(cpu, N);
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                add_and_set_flags(cpu, N); */
            }
            break;

        case OP_ADC_IND_Y: /* ADC (Indirect), Y */
            {
#if 1
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                add_and_set_flags(cpu, N);
#else
                byte_t N = get_operand_zeropage_indirect_y(cpu);
                add_and_set_flags(cpu, N);
#endif
            }
            break;

    /* AND --------------------------------- */

        case OP_AND_IMM: /* AND Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_AND_ZP: /* AND Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_AND_IND: /* AND (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo &= read_byte(cpu,addr);
                set_n_z_flags(cpu, cpu->a_lo);*/
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_AND_ZP_X: /* AND Zero Page, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_AND_ABS: /* AND Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;
        
        case OP_AND_ABS_X: /* AND Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_AND_ABS_Y: /* AND Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_AND_IND_X: /* AND (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo);*/
            }
            break;

        case OP_AND_IND_Y: /* AND (Indirect), Y */
            {
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo &= N;
                set_n_z_flags(cpu, cpu->a_lo); */
            }
            break;

    /* ASL --------------------------------- */

        case OP_ASL_ACC: /* ASL Accumulator */
            {
                rmw_acc<alu_t>(cpu, ArithmeticShiftLeftOp());
            }
            break;

        case OP_ASL_ZP: /* ASL Zero Page */
            {
                rmw_direct<alu_t>(cpu, ArithmeticShiftLeftOp());
            }
            break;

        case OP_ASL_ZP_X: /* ASL Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, ArithmeticShiftLeftOp());
                /* zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                arithmetic_shift_left_addr(cpu, zpaddr); */
            }
            break;

        case OP_ASL_ABS: /* ASL Absolute */
            {
                rmw_abs<alu_t>(cpu, ArithmeticShiftLeftOp());
            }
            break;
            
        case OP_ASL_ABS_X: /* ASL Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, ArithmeticShiftLeftOp());
            }
            break;

        /* Long Mode Stuff --------------------------------- */
        case OP_ORA_ABSL: /* ORA Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_ORA_ABSL_X: /* ORA Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_AND_ABSL: /* AND Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_AND_ABSL_X: /* AND Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_EOR_ABSL: /* EOR Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_EOR_ABSL_X: /* EOR Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_ADC_ABSL: /* ADD Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                add_and_set_flags(cpu, N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;
            
        case OP_ADC_ABSL_X: /* ADC Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                add_and_set_flags(cpu, N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_STA_ABSL: /* STA Long */
            if constexpr (CPUTraits::has_65816_ops) {
                write_long(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_STA_ABSL_X: /* STA Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                write_long_x(cpu, _A(cpu), _X(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_LDA_ABSL: /* LDA Long */
            if constexpr (CPUTraits::has_65816_ops) {
                read_long(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_LDA_ABSL_X: /* LDA Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                read_long_x(cpu, _A(cpu), _X(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_CMP_ABSL: /* CMP Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_CMP_ABSL_X: /* CMP Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_SBC_ABSL: /* SBC Long */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long(cpu, N);
                subtract_and_set_flags(cpu, N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        case OP_SBC_ABSL_X: /* SBC Long, X */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_long_x(cpu, N, _X(cpu));
                subtract_and_set_flags(cpu, N);
            } else {
                invalid_nop(cpu, 1, 1);
            }
            break;

        /* BCS / BCC */

        case OP_BCC_REL: /* BCC Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->C == 0);
            }
            break;

        case OP_BCS_REL: /* BCS Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->C == 1);
            }
            break;

        case OP_BEQ_REL: /* BEQ Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->Z == 1);
            }
            break;

        case OP_BNE_REL: /* BNE Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->Z == 0);
            }
            break;

        case OP_BMI_REL: /* BMI Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->N == 1);
            }
            break;

        case OP_BPL_REL: /* BPL Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->N == 0);
            }
            break;

        case OP_BRA_REL: /* BRA Relative */
            if constexpr (CPUTraits::has_65c02_ops) {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, true);
            }
            break;

        case OP_BVC_REL: /* BVC Relative */
            {
                uint8_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->V == 0);
            }
            break;

        case OP_BVS_REL: /* BVS Relative */
            {
                byte_t N = address_relative(cpu);
                branch_if(cpu, N, cpu->V == 1);
            }
            break;

    /* CMP --------------------------------- */
        case OP_CMP_IMM: /* CMP Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_IND: /* CMP (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, read_byte(cpu,addr)); */
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_CMP_ZP: /* CMP Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_ZP_X: /* CMP Zero Page, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_ABS: /* CMP Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_ABS_X: /* CMP Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_ABS_Y: /* CMP Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
            }
            break;

        case OP_CMP_IND_X: /* CMP (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N);*/
            }
            break;

        case OP_CMP_IND_Y: /* CMP (Indirect), Y */
            {
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
                /* byte_t N = get_operand_zeropage_indirect_y(cpu);
                compare_and_set_flags(cpu, cpu->a_lo, N); */
            }
            break;

    /* CPX --------------------------------- */
        case OP_CPX_IMM: /* CPX Immediate */
            {
                index_t N;
                read_imm(cpu, N);
                compare_and_set_flags(cpu, _X(cpu), N);
            }
            break;

        case OP_CPX_ZP: /* CPX Zero Page */
            {
                index_t N;
                read_direct(cpu, N);
                compare_and_set_flags(cpu, _X(cpu), N);
            }
            break;

        case OP_CPX_ABS: /* CPX Absolute */
            {
                index_t N;
                read_abs(cpu, N);
                compare_and_set_flags(cpu, _X(cpu), N);
            }
            break;

    /* CPY --------------------------------- */
        case OP_CPY_IMM: /* CPY Immediate */
            {
                index_t N;
                read_imm(cpu, N);
                compare_and_set_flags(cpu, _Y(cpu), N);
            }
            break;

        case OP_CPY_ZP: /* CPY Zero Page */
            {
                index_t N;
                read_direct(cpu, N);
                compare_and_set_flags(cpu, _Y(cpu), N);
            }
            break;

        case OP_CPY_ABS: /* CPY Absolute */
            {
                index_t N;
                read_abs(cpu, N);
                compare_and_set_flags(cpu, _Y(cpu), N);
            }
            break;

    /* DEC --------------------------------- */
        case OP_DEC_ZP: /* DEC Zero Page */
            {
                rmw_direct<alu_t>(cpu, DecrementOp());
            }
            break;

        case OP_DEC_ZP_X: /* DEC Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, DecrementOp());
                /* zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                dec_operand(cpu, zpaddr); */
            }
            break;

        case OP_DEC_ABS: /* DEC Absolute */
            {
                rmw_abs<alu_t>(cpu, DecrementOp());
            }
            break;

        case OP_DEC_ABS_X: /* DEC Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, DecrementOp());
            }
            break;

    /* DE(xy) --------------------------------- */
        case OP_DEX_IMP: /* DEX Implied */
            {
                _X(cpu) --;
                set_n_z_flags(cpu, _X(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            }
            break;

        case OP_DEY_IMP: /* DEY Implied */
            {
                _Y(cpu) --;
                set_n_z_flags(cpu, _Y(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            }
            break;


    /* EOR --------------------------------- */

        case OP_EOR_IMM: /* EOR Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_EOR_IND: /* EOR (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo ^= read_byte(cpu,addr);
                set_n_z_flags(cpu, cpu->a_lo); */
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_EOR_ZP: /* EOR Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_EOR_ZP_X: /* EOR Zero Page, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_EOR_ABS: /* EOR Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;
        
        case OP_EOR_ABS_X: /* EOR Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_EOR_ABS_Y: /* EOR Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_EOR_IND_X: /* EOR (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo);*/
            }
            break;

        case OP_EOR_IND_Y: /* EOR (Indirect), Y */
            {
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo ^= N;
                set_n_z_flags(cpu, cpu->a_lo); */
            }
            break;


        /* INC A / INA & DEC A / DEA --------------------------------- */
        case OP_INA_ACC: /* INA Accumulator */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_acc<alu_t>(cpu, IncrementOp());
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_DEA_ACC: /* DEA Accumulator */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_acc<alu_t>(cpu, DecrementOp());
            } else invalid_opcode(cpu, opcode);
            break;

    /* INC --------------------------------- */
        case OP_INC_ZP: /* INC Zero Page */
            {
                rmw_direct<alu_t>(cpu, IncrementOp());
            }
            break;

        case OP_INC_ZP_X: /* INC Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, IncrementOp());
                /* zpaddr_t zpaddr = get_operand_address_zeropage_x(cpu);
                inc_operand(cpu, zpaddr); */
            }
            break;

        case OP_INC_ABS: /* INC Absolute */
            {
                rmw_abs<alu_t>(cpu, IncrementOp());
            }
            break;

        case OP_INC_ABS_X: /* INC Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, IncrementOp());
            }
            break;

    /* IN(xy) --------------------------------- */

        case OP_INX_IMP: /* INX Implied */
            {
                _X(cpu) ++;
                set_n_z_flags(cpu, _X(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            }
            break;

        case OP_INY_IMP: /* INY Implied */
            {
                _Y(cpu) ++;
                set_n_z_flags(cpu, _Y(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            }
            break;

    /* LDA --------------------------------- */

        case OP_LDA_IMM: /* LDA Immediate */
            {
                read_imm(cpu, _A(cpu));            
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_LDA_ZP: /* LDA Zero Page */
            {
                read_direct(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_LDA_ZP_X: /* LDA Zero Page, X */
            {
                read_direct_x(cpu, _A(cpu), _X(cpu));
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_LDA_ABS: /* LDA Absolute */
            {
                read_abs(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            }
            break;
        
        case OP_LDA_ABS_X: /* LDA Absolute, X */
            {
                read_abs_x(cpu, _A(cpu), _X(cpu));
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_LDA_ABS_Y: /* LDA Absolute, Y */
            {
                read_abs_x(cpu,_A(cpu), _Y(cpu));
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_LDA_IND: /* LDA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                read_direct_indirect(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo = read_byte(cpu,addr);
                set_n_z_flags(cpu, cpu->a_lo); */
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_LDA_IND_X: /* LDA (Indirect, X) */
            {
                read_direct_x_ind(cpu, _A(cpu), _X(cpu));
                set_n_z_flags(cpu, _A(cpu));
                /* cpu->a_lo = get_operand_zeropage_indirect_x(cpu);
                set_n_z_flags(cpu, cpu->a_lo);*/
            }
            break;

        case OP_LDA_IND_Y: /* LDA (Indirect), Y */
            {
                read_direct_ind_x(cpu, _A(cpu), _Y(cpu));
                set_n_z_flags(cpu, _A(cpu));
                /* cpu->a_lo = get_operand_zeropage_indirect_y(cpu);
                set_n_z_flags(cpu, cpu->a_lo); */
            }
            break;

        /* LDX --------------------------------- */

        case OP_LDX_IMM: /* LDX Immediate */
            {
                read_imm(cpu, _X(cpu));
                set_n_z_flags(cpu, _X(cpu));
            }
            break;

        case OP_LDX_ZP: /* LDX Zero Page */
            {
                read_direct(cpu, _X(cpu));
                set_n_z_flags(cpu, _X(cpu));
            }
            break;

        case OP_LDX_ZP_Y: /* LDX Zero Page, Y */
            {
                read_direct_x(cpu, _X(cpu), _Y(cpu));
                set_n_z_flags(cpu, _X(cpu));
            }
            break;

        case OP_LDX_ABS: /* LDX Absolute */
            {
                read_abs(cpu, _X(cpu));
                set_n_z_flags(cpu, _X(cpu));
            }
            break;

        case OP_LDX_ABS_Y: /* LDX Absolute, Y */
            {
                read_abs_x(cpu, _X(cpu), _Y(cpu));
                set_n_z_flags(cpu, _X(cpu));
            }
            break;

        /* LDY --------------------------------- */

        case OP_LDY_IMM: /* LDY Immediate */
            {
                read_imm(cpu, _Y(cpu));
                set_n_z_flags(cpu, _Y(cpu));
            }
            break;
        
        case OP_LDY_ZP: /* LDY Zero Page */
            {
                read_direct(cpu, _Y(cpu));
                set_n_z_flags(cpu, _Y(cpu));
            }
            break;

        case OP_LDY_ZP_X: /* LDY Zero Page, X */
            {
                read_direct_x(cpu, _Y(cpu), _X(cpu));
                set_n_z_flags(cpu, _Y(cpu));
            }
            break;

        case OP_LDY_ABS: /* LDY Absolute */
            {
                read_abs(cpu, _Y(cpu));
                set_n_z_flags(cpu, _Y(cpu));            }
            break;

        case OP_LDY_ABS_X: /* LDY Absolute, X */
            {
                read_abs_x(cpu, _Y(cpu), _X(cpu));
                set_n_z_flags(cpu, _Y(cpu));            }
            break;

    /* LSR  --------------------------------- */

        case OP_LSR_ACC: /* LSR Accumulator */
            {
                rmw_acc<alu_t>(cpu, LogicalShiftRightOp());
            }
            break;

        case OP_LSR_ZP: /* LSR Zero Page */
            {
                rmw_direct<alu_t>(cpu, LogicalShiftRightOp());
            }
            break;

        case OP_LSR_ZP_X: /* LSR Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, LogicalShiftRightOp());
                /* absaddr_t addr = get_operand_address_zeropage_x(cpu);
                logical_shift_right_addr(cpu, addr); */
            }
            break;

        case OP_LSR_ABS: /* LSR Absolute */
            {
                rmw_abs<alu_t>(cpu, LogicalShiftRightOp());
            }
            break;

        case OP_LSR_ABS_X: /* LSR Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, LogicalShiftRightOp());
            }
            break;


    /* ORA --------------------------------- */

        case OP_ORA_IMM: /* ORA Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_ORA_IND: /* ORA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                cpu->a_lo |= read_byte(cpu,addr);
                set_n_z_flags(cpu, cpu->a_lo);*/
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_ORA_ZP: /* AND Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_ORA_ZP_X: /* AND Zero Page, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_ORA_ABS: /* AND Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;
        
        case OP_ORA_ABS_X: /* AND Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_ORA_ABS_Y: /* AND Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            }
            break;

        case OP_ORA_IND_X: /* AND (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo);*/
            }
            break;

        case OP_ORA_IND_Y: /* AND (Indirect), Y */
            {
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
                /* byte_t N = get_operand_zeropage_indirect_y(cpu);
                cpu->a_lo |= N;
                set_n_z_flags(cpu, cpu->a_lo); */
            }
            break;

    /* Stack operations --------------------------------- */

        case OP_PHA_IMP: /* PHA Implied */
            {
                stack_push(cpu, _A(cpu));
                /* push_byte(cpu, cpu->a_lo); */
            }
            break;

        case OP_PHP_IMP: /* PHP Implied */
            {
                if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode) {
                    stack_push(cpu, cpu->p);
                } else {
                    uint8_t temp_p = (cpu->p | (FLAG_B | FLAG_UNUSED)); // break flag and Unused bit set to 1.
                    stack_push(cpu, temp_p);
                }
                /* uint8_t temp_p = (cpu->p | (FLAG_B | FLAG_UNUSED)); // break flag and Unused bit set to 1.
                stack_push(cpu, temp_p); */
                /* push_byte(cpu, (cpu->p | (FLAG_B | FLAG_UNUSED)));  */
            }
            break;

        case OP_PHX_IMP: /* PHX Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                stack_push(cpu, _X(cpu));
                //push_byte(cpu, cpu->x_lo);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PHY_IMP: /* PHY Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                stack_push(cpu, _Y(cpu));
                //push_byte(cpu, cpu->y_lo);
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PLP_IMP: /* PLP Implied */
            {
                if constexpr (CPUTraits::has_65816_ops && !CPUTraits::e_mode) {
                    stack_pull(cpu, cpu->p);
                    // TODO: perform x/m mode switch check here.
                } else if constexpr (CPUTraits::has_65816_ops && CPUTraits::e_mode) {
                    // when e flag=1, m/x are forced to 1, so after plp, both flags will still be 1 no matter what is pulled from stack.
                    stack_pull(cpu, cpu->p);
                    cpu->_M = 1;
                    cpu->_X = 1;
                } else {
                    stack_pull(cpu, cpu->p);
                    cpu->p &= ~FLAG_B; // break flag is cleared.
                }               
            }
            break;

        case OP_PLA_IMP: /* PLA Implied */
            {
                stack_pull(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
                /* cpu->a_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->a_lo);
                cpu->incr_cycles(); */
            }
            break;

        case OP_PLX_IMP: /* PLX Implied */  
            if constexpr (CPUTraits::has_65c02_ops) {
                stack_pull(cpu, _X(cpu));
                set_n_z_flags(cpu, _X(cpu));
                /* cpu->x_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->x_lo);
                cpu->incr_cycles(); */
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_PLY_IMP: /* PLY Implied */
            if constexpr (CPUTraits::has_65c02_ops) {
                stack_pull(cpu, _Y(cpu));
                set_n_z_flags(cpu, _Y(cpu));
                /* cpu->y_lo = pop_byte(cpu);
                set_n_z_flags(cpu, cpu->y_lo);
                cpu->incr_cycles(); */
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

    /* ROL --------------------------------- */

        case OP_ROL_ACC: /* ROL Accumulator */
            {
                rmw_acc<alu_t>(cpu, RotateLeftOp());
            }
            break;

        case OP_ROL_ZP: /* ROL Zero Page */
            {
                rmw_direct<alu_t>(cpu, RotateLeftOp());
            }
            break;

        case OP_ROL_ZP_X: /* ROL Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, RotateLeftOp());
                /* absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_left_addr(cpu, addr); */
            }
            break;

        case OP_ROL_ABS: /* ROL Absolute */
            {
                rmw_abs<alu_t>(cpu, RotateLeftOp());
            }
            break;

        case OP_ROL_ABS_X: /* ROL Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, RotateLeftOp());
            }
            break;

    /* ROR --------------------------------- */
        case OP_ROR_ACC: /* ROR Accumulator */
            {
                rmw_acc<alu_t>(cpu, RotateRightOp());
            }
            break;

        case OP_ROR_ZP: /* ROR Zero Page */
            {
                rmw_direct<alu_t>(cpu, RotateRightOp());
            }
            break;

        case OP_ROR_ZP_X: /* ROR Zero Page, X */
            {
                rmw_direct_x<alu_t>(cpu, RotateRightOp());
                /* absaddr_t addr = get_operand_address_zeropage_x(cpu);
                rotate_right_addr(cpu, addr); */
            }
            break;

        case OP_ROR_ABS: /* ROR Absolute */
            {
                rmw_abs<alu_t>(cpu, RotateRightOp());
            }
            break;

        case OP_ROR_ABS_X: /* ROR Absolute, X */
            {
                rmw_abs_x<alu_t>(cpu, RotateRightOp());
            }
            break;

        /* SBC --------------------------------- */
        case OP_SBC_IMM: /* SBC Immediate */
            {
                alu_t N;
                read_imm(cpu, N);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND: /* SBC (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_indirect(cpu, N);
                subtract_and_set_flags(cpu, N);
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                subtract_and_set_flags(cpu, read_byte(cpu,addr));*/
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_SBC_ZP: /* SBC Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ZP_X: /* SBC Zero Page, X */
            {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS: /* SBC Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_X: /* SBC Absolute, X */
            {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_ABS_Y: /* SBC Absolute, Y */
            {
                alu_t N;
                read_abs_x(cpu, N, _Y(cpu));
                subtract_and_set_flags(cpu, N);
            }
            break;

        case OP_SBC_IND_X: /* SBC (Indirect, X) */
            {
                alu_t N;
                read_direct_x_ind(cpu, N, _X(cpu));
                subtract_and_set_flags(cpu, N);
                /* byte_t N = get_operand_zeropage_indirect_x(cpu);
                subtract_and_set_flags(cpu, N);*/
            }
            break;

        case OP_SBC_IND_Y: /* SBC (Indirect), Y */
            {
                alu_t N;
                read_direct_ind_x(cpu, N, _Y(cpu));
                subtract_and_set_flags(cpu, N);
                /* byte_t N = get_operand_zeropage_indirect_y(cpu);
                subtract_and_set_flags(cpu, N); */
            }
            break;

        /* STA --------------------------------- */
        case OP_STA_IND: /* STA (Indirect) */
            if constexpr (CPUTraits::has_65c02_ops) {
                write_direct_indirect(cpu, _A(cpu));
                /* absaddr_t addr = get_operand_address_zeropage_indirect(cpu);
                write_byte(cpu,addr, cpu->a_lo); */
                // TODO: one clock too many (get_operand.. burns 4)
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STA_ZP: /* STA Zero Page */
            {
                write_direct(cpu, _A(cpu));
            }
            break;

        case OP_STA_ZP_X: /* STA Zero Page, X */
            {
                write_direct_x(cpu, _A(cpu), _X(cpu));
            }
            break;

        case OP_STA_ABS: /* STA Absolute */
            {
                write_abs(cpu, _A(cpu));
            }
            break;

        case OP_STA_ABS_X: /* STA Absolute, X */
            {
                write_abs_x(cpu, _A(cpu), _X(cpu));
            }
            break;

        case OP_STA_ABS_Y: /* STA Absolute, Y */
            {
                write_abs_x(cpu, _A(cpu), _Y(cpu));
            }
            break;

        case OP_STA_IND_X: /* STA (Indirect, X) */
            {
                write_direct_x_ind(cpu, _A(cpu), _X(cpu));
                /* store_operand_zeropage_indirect_x(cpu, cpu->a_lo); */
            }
            break;

        case OP_STA_IND_Y: /* STA (Indirect), Y */
            {
                write_direct_ind_x(cpu, _A(cpu), _Y(cpu));
                /* store_operand_zeropage_indirect_y(cpu, cpu->a_lo); */
            }
            break;
        
        /* STX --------------------------------- */
        case OP_STX_ZP: /* STX Zero Page */
            {
                write_direct(cpu, _X(cpu));
            }
            break;

        case OP_STX_ZP_Y: /* STX Zero Page, Y */
            {
                write_direct_x(cpu, _X(cpu), _Y(cpu));
            }
            break;

        case OP_STX_ABS: /* STX Absolute */
            {
                write_abs(cpu, _X(cpu));
            }
            break;

    /* STY --------------------------------- */
        case OP_STY_ZP: /* STY Zero Page */
            {
                write_direct(cpu, _Y(cpu));
            }
            break;

        case OP_STY_ZP_X: /* STY Zero Page, X */
            {
                write_direct_x(cpu, _Y(cpu), _X(cpu));
            }
            break;
        
        case OP_STY_ABS: /* STY Absolute */
            {
                write_abs(cpu, _Y(cpu));
            }
            break;

        /* STZ --------------------------------- */
        case OP_STZ_ZP: /* STZ Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                write_direct(cpu, zero);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ZP_X: /* STZ Zero Page, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                write_direct_x(cpu, zero, _X(cpu));
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ABS: /* STZ Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                write_abs(cpu, zero);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_STZ_ABS_X: /* STZ Absolute, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                write_abs_x(cpu, zero, _X(cpu));
            } else invalid_opcode(cpu, opcode);
            break;


        /* Transfer between registers --------------------------------- */

        case OP_TAX_IMP: /* TAX Implied */
            {
                if constexpr (sizeof(_X(cpu)) == 2) cpu->x = cpu->a;
                else cpu->x_lo = cpu->a_lo;
                set_n_z_flags(cpu, _X(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                TRACE(cpu->trace_entry.data = _X(cpu);)

                //transfer_reg_reg(cpu, _A(cpu), _X(cpu));
            }
            break;

        case OP_TAY_IMP: /* TAY Implied */
            {
                if constexpr (sizeof(_Y(cpu)) == 2) cpu->y = cpu->a;
                else cpu->y_lo = cpu->a_lo;
                set_n_z_flags(cpu, _Y(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                TRACE(cpu->trace_entry.data = _Y(cpu);)

                //transfer_reg_reg(cpu, _A(cpu), _Y(cpu));
            }
            break;

        case OP_TYA_IMP: /* TYA Implied */
            {
                if constexpr (sizeof(_A(cpu)) == 2) cpu->a = cpu->y;
                else cpu->a_lo = cpu->y_lo;
                set_n_z_flags(cpu, _A(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                TRACE(cpu->trace_entry.data = _A(cpu);)
            
                //transfer_reg_reg(cpu, _Y(cpu), _A(cpu));
            }
            break;


        /* TSB and TRB - test and set or test and reset bits */
        case OP_TRB_ZP: /* TRB Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_direct<alu_t>(cpu, TestResetBitsOp());
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TRB_ABS: /* TRB Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_abs<alu_t>(cpu, TestResetBitsOp());
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TSB_ZP: /* TSB Zero Page */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_direct<alu_t>(cpu, TestSetBitsOp());
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        case OP_TSB_ABS: /* TSB Absolute */
            if constexpr (CPUTraits::has_65c02_ops) {
                rmw_abs<alu_t>(cpu, TestSetBitsOp());
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        /* TSX - transfer stack pointer to X */
        case OP_TSX_IMP: /* TSX Implied */
            {
                transfer_s_reg(cpu, _X(cpu));
            }
            break;

        case OP_TXA_IMP: /* TXA Implied */
            {
                if constexpr (sizeof(_A(cpu)) == 2) cpu->a = cpu->x;
                else cpu->a_lo = cpu->x_lo;
                set_n_z_flags(cpu, _A(cpu));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                TRACE(cpu->trace_entry.data = _A(cpu);)
                //transfer_reg_reg(cpu, _X(cpu), _A(cpu));
            }
            break;

        case OP_TXS_IMP: /* TXS Implied */
            {
                transfer_x_s(cpu, _X(cpu));
            }
            break;

        /* BRK --------------------------------- */
        case OP_BRK_IMP: /* BRK */
            {     
                if constexpr ((CPUTraits::has_65816_ops) && (!CPUTraits::e_mode)) {
                    brk_cop(cpu, N_BRK_VECTOR);
                } else {
                    brk_cop(cpu, BRK_VECTOR);
                }

               /*  uint8_t sign = fetch_pc(cpu); // ignore this, we don't need it.

                push_word(cpu, cpu->pc); // pc of BRK signature byte
                push_byte(cpu, cpu->p | FLAG_B | FLAG_UNUSED); // break flag and Unused bit set to 1.
                cpu->I = 1; // interrupt disable flag set to 1.
                if constexpr (CPUTraits::has_65c02_ops) {
                    cpu->D = 0; // turn off decimal mode on brk and interrupts
                }
                cpu->pc = read_word(cpu,BRK_VECTOR); */
            }
            break;

        /* JMP --------------------------------- */
        case OP_JMP_ABS: /* JMP Absolute */
            { // 1b. Absolute a    JMP
                absaddr_t addr = address_abs(cpu);
                cpu->pc = addr;
            }
            break;

        case OP_JMP_IND: /* JMP (Indirect) */
            {   // TODO: need to implement the "JMP" bug for non-65c02. The below is correct for 65c02.
                // TODO: Note that JMP (absolute) is 5 cycles, same as the NMOS 6502, but different from the 65C02 (6 cycles).
                // 1. get AA from PC+1, PC+2
                // 2. get 0,AA and 0,AA+1 -> PC
                uint16_t aa = fetch_pc(cpu);
                aa |= fetch_pc(cpu) << 8;
                uint16_t eaddr = bus_read(cpu, aa);
                eaddr |= bus_read(cpu, (uint16_t)(aa + 1)) << 8 ;

                //absaddr_t addr = get_operand_address_absolute_indirect(cpu);
                cpu->pc = eaddr;
                TRACE(cpu->trace_entry.operand = aa; cpu->trace_entry.eaddr = eaddr; )
            }
            break;

        case OP_JMP_IND_X: /* 2a. Absolute indexed indirect (a,x)     JMP (Indirect, X) */
            if constexpr (CPUTraits::has_65c02_ops) {
                absaddr_t addr = get_operand_address_absolute_indirect_x(cpu);
                cpu->pc = addr;
            } else {
                invalid_opcode(cpu, opcode);
            }
            break;

        /* JSR --------------------------------- */
        case OP_JSR_ABS: /* JSR Absolute */ /* 1c. Absolute a     JSR */
            {
                absaddr_t addr = address_abs(cpu);
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc-1));

                push_word(cpu, cpu->pc -1); // return address pushed is last byte of JSR instruction
                cpu->pc = addr;
            }
            break;

        /* RTI --------------------------------- */
        case OP_RTI_IMP: /* RTI */
            {
                // TODO: make sure you finish this.
                if constexpr (CPUTraits::has_65816_ops) {
                    // "different order from N6502"
                    byte_t oldp = cpu->p & (FLAG_B | FLAG_UNUSED);

                    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
                    phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
                    byte_t p = pop_byte(cpu);
                    cpu->pc = pop_word(cpu);
                    if constexpr (!CPUTraits::e_mode) {
                        cpu->p = p;
                        cpu->pb = pop_byte(cpu);
                    } else {
                        p = p & ~(FLAG_B | FLAG_UNUSED);
                        cpu->p = p | oldp;
                    }
                    
                } else {
                    // pop status register "ignore B | unused" which I think means don't change them.
                    // can't find reference for order of RTI bus operations on 6502.
                    byte_t oldp = cpu->p & (FLAG_B | FLAG_UNUSED);
                    byte_t p = pop_byte_nocycle(cpu) & ~(FLAG_B | FLAG_UNUSED);
                    cpu->p = p | oldp;

                    cpu->pc = pop_word(cpu);
                    cpu->incr_cycles();
                    cpu->incr_cycles();
                    TRACE(cpu->trace_entry.operand = cpu->pc;)
                }
            }
            break;

        /* RTS --------------------------------- */
        case OP_RTS_IMP: /* RTS */
            {
                stack_pull(cpu, cpu->pc);
                phantom_read(cpu, cpu->sp);
                cpu->pc++;

                /* cpu->pc = pop_word(cpu);
                cpu->pc++;
                cpu->incr_cycles(); */
                TRACE(cpu->trace_entry.operand = cpu->pc;)
            }
            break;

        /* NOP --------------------------------- */
        case OP_NOP_IMP: /* NOP */
            {
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            }
            break;

        /* Flags ---------------------------------  */

        case OP_CLD_IMP: /* CLD Implied */
            {
                cpu->D = 0;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_SED_IMP: /* SED Implied */
            {
                cpu->D = 1;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_CLC_IMP: /* CLC Implied */
            {
                cpu->C = 0;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_CLI_IMP: /* CLI Implied */
            {
                if (cpu->I) cpu->skip_next_irq_check = 1; // TODO: this can be cpu->skip_next_irq_check = cpu->I; test after change.
                cpu->I = 0;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_CLV_IMP: /* CLV */
            {
                cpu->V = 0;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_SEC_IMP: /* SEC Implied */
            {
                cpu->C = 1;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        case OP_SEI_IMP: /* SEI Implied */
            {
                cpu->I = 1;
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
            }
            break;

        /** Misc --------------------------------- */

        case OP_BIT_ZP: /* BIT Zero Page */
            {
                alu_t N;
                read_direct(cpu, N);
                alu_t T = _A(cpu) & N;
                set_n_z_v_flags(cpu, T, N);
            }
            break;

        case OP_BIT_ABS: /* BIT Absolute */
            {
                alu_t N;
                read_abs(cpu, N);
                alu_t T = _A(cpu) & N;
                set_n_z_v_flags(cpu, T, N);
            }
            break;

        case OP_BIT_IMM: /* BIT Immediate */

            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_imm(cpu, N);
                set_z_flag(cpu, _A(cpu) & N);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_BIT_ZP_X: /* BIT Zero Page, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_direct_x(cpu, N, _X(cpu));
                alu_t T = _A(cpu) & N;
                set_n_z_v_flags(cpu, T, N);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_BIT_ABS_X: /* BIT Absolute, X */
            if constexpr (CPUTraits::has_65c02_ops) {
                alu_t N;
                read_abs_x(cpu, N, _X(cpu));
                alu_t T = _A(cpu) & N;
                set_n_z_v_flags(cpu, T, N);
            } else invalid_opcode(cpu, opcode);
            break;

        /* A bunch of stuff that is unimplemented in the 6502/65c02, but present in 65816 */

        case OP_INOP_02: /* INOP 02 */ /* OP_COP_S*/
            if constexpr (CPUTraits::has_65816_ops) {
                if constexpr (CPUTraits::e_mode) {
                    brk_cop(cpu, COP_VECTOR);
                } else {
                    brk_cop(cpu, N_COP_VECTOR);
                }
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_22: /* INOP 22 */ /* 4c. OP_JSL_ABSL Absolute Long */
            if constexpr (CPUTraits::has_65816_ops) {
                uint16_t eaddr = fetch_pc(cpu); // cycle 2
                eaddr |= fetch_pc(cpu) << 8; // cycle 3
                push_byte_new(cpu, cpu->pb); // cycle 4
                phantom_read(cpu, cpu->sp);
                //cpu->incr_cycles(); // TODO: replace with phantom_read (0,S) // cycle 5
                uint16_t o_pc = cpu->pc;
                cpu->pb = fetch_pc(cpu); // cycle 6
                push_word_new(cpu, o_pc); // cycle 7 and 8
                stack_fix_new(cpu);
                cpu->pc = eaddr;
                uint32_t eaddr_long = (cpu->pb << 16) | eaddr;
                TRACE(cpu->trace_entry.operand = eaddr_long; cpu->trace_entry.eaddr = eaddr_long; )
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            }
            break;

        case OP_INOP_42: /* INOP 42 */ /* OP_WDM_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                uint8_t N = fetch_pc(cpu); // cycle 2
                // otherwise do nothing
                TRACE(cpu->trace_entry.operand = N; cpu->trace_entry.f_data_sz = 1;)
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_62: /* INOP 62 */ /* OP_PER_S */
            if constexpr (CPUTraits::has_65816_ops) {
                uint16_t offset = address_relative_long(cpu);
                uint16_t target = cpu->pc + offset; 
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));
                push_word_new(cpu, target);
                stack_fix_new(cpu);
                TRACE(cpu->trace_entry.eaddr = target; )
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_82: /* INOP 82 */ /* OP_BRL_REL_L */
            if constexpr (CPUTraits::has_65816_ops) {
                uint16_t offset = address_relative_long(cpu);
                cpu->pc += offset; 
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));

                TRACE(cpu->trace_entry.eaddr = cpu->pc; )
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C2: /* INOP C2 */ /* OP_REP_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                byte_t N;
                read_imm(cpu, N);
                cpu->p &= ~N;
                if (cpu->E) {
                    cpu->p |= 0x30; // "if e flag is 1 m and x flags are forced to 1". this will not change register width.
                    cpu->sp_hi = 0x01;
                } else {
                    /*  */
                    // TODO: execute potential register width change.
                }
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E2: /* INOP E2 */ /* SEP */
            if constexpr (CPUTraits::has_65816_ops) {
                byte_t N;
                read_imm(cpu, N);

                cpu->p |= N;
                if (cpu->E) {
                    cpu->p |= 0x30; // "if e flag is 1 m and x flags are forced to 1". this will not change register width.
                } else {
                    // TODO: execute potential register width change.
                    if (cpu->_X == 1) { // when switch to 8-bit index registers, x/y hi are forced to 0.
                        cpu->x_hi = 0;
                        cpu->y_hi = 0;
                    }
                }
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 2);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_44: /* INOP 44 */
            if constexpr (CPUTraits::has_65816_ops) {
                move_memory(cpu);
                _X(cpu)--;
                _Y(cpu)--;
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 3);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_54: /* INOP 54 */ /* OP_MVN_MOVE */
            if constexpr (CPUTraits::has_65816_ops) {
                move_memory(cpu);
                _X(cpu)++;
                _Y(cpu)++;
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D4: /* INOP D4 */ /* OP_PEI_S */
            if constexpr (CPUTraits::has_65816_ops) {
                uint16_t addr;
                read_direct(cpu, addr); // read 16-bit immediate operand
                push_word_new(cpu, addr);
                stack_fix_new(cpu);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_F4: /* INOP F4 */ /* OP_PEA_S */
            if constexpr (CPUTraits::has_65816_ops) {
                uint16_t addr;
                read_imm(cpu, addr); // read 16-bit immediate operand
                push_word_new(cpu, addr);
                stack_fix_new(cpu);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 2, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_5C: /* INOP 5C */ /* 4b. OP_JMP_ABSL  OP_JML long */
            if constexpr (CPUTraits::has_65816_ops) {
                uint32_t addr = address_long(cpu);
                cpu->pc = (uint16_t)addr;
                cpu->pb = (uint8_t)(addr >> 16);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 8);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_DC: /* INOP DC */ /* 3a.Absolute Indirect (a)   JML [Absolute] */
            if constexpr (CPUTraits::has_65816_ops) {
                absaddr_t addr = fetch_pc(cpu); // cycle 2.
                addr |= fetch_pc(cpu) << 8; // cycle 3.

                // addr wraps around in bank 0, and bank number is forced to 0 for these reads. That's why 
                // it's 'absolute'.
                absaddr_t eaddr = bus_read(cpu, addr); // cycle 4
                eaddr |= bus_read(cpu, (uint16_t) (addr + 1)) << 8; // cycle 5
                cpu->pb = bus_read(cpu, (uint16_t) (addr + 2)); // cycle 6
                cpu->pc = eaddr;

                TRACE(cpu->trace_entry.operand = addr; cpu->trace_entry.eaddr = eaddr; )
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 4);
            } else invalid_opcode(cpu, opcode);
            break;
            
        case OP_INOP_FC: /* INOP FC */ /* OP_JSR_IND_X */ /* 2b. Absolute indexed indirect (a,x)     JSR (Indirect, X) */
            if constexpr (CPUTraits::has_65816_ops) {
                absaddr_t base = fetch_pc(cpu); // cycle 2.
                
                push_word_new(cpu, cpu->pc); // Cycles 3, 4: return address pushed is last byte of JSR instruction (PC after opcode and first byte of operand)

                base |= fetch_pc(cpu) << 8; // Cycle 5: 2nd byte of operand
                
                phantom_read_ign(cpu, make_pc_long(cpu, cpu->pc));

                absaddr_t indexed = base + _X(cpu);
                
                absaddr_t indaddr = bus_read(cpu, make_pb_addr(cpu, (uint16_t) indexed));
                indaddr |= bus_read(cpu, make_pb_addr(cpu, (uint16_t) indexed) + 1) << 8; 

                cpu->pc = indaddr;
                stack_fix_new(cpu);

                TRACE(cpu->trace_entry.operand = base; cpu->trace_entry.eaddr = indaddr; )
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 3, 4);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_03: /* INOP 03 */ /* OP_ORA_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_13: /* INOP 13 */ /* OP_ORA_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_23: /* INOP 23 */ /* OP_AND_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_33: /* INOP 33 */ /* OP_AND_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_43: /* INOP 43 */ /* OP_EOR_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_53: /* INOP 53 */ /* OP_EOR_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_63: /* INOP 63 */ /* OP_ADC_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                add_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_73: /* INOP 73 */ /* OP_ADC_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                add_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_83: /* INOP 83 */ /* OP_STA_S */
            if constexpr (CPUTraits::has_65816_ops) {
                write_stack_relative(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_93: /* INOP 93 */ /* OP_STA_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                write_stack_relative_y(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_A3: /* INOP A3 */ /* OP_LDA_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_B3: /* INOP B3 */ /* OP_LDA_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C3: /* INOP C3 */ /* OP_CMP_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D3: /* INOP D3 */ /* OP_CMP_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E3: /* INOP E3 */ /* OP_SBC_S */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative(cpu, N);
                subtract_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_F3: /* INOP F3 */ /* OP_SBC_S_Y */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_stack_relative_y(cpu, N);
                subtract_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_07: /* INOP 07 */ /* OP_ORA_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_17: /* INOP 17 */ /* OP_ORA_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                _A(cpu) |= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_27: /* INOP 27 */ /* OP_AND_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_37: /* INOP 37 */ /* OP_AND_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                _A(cpu) &= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_47: /* INOP 47 */ /* OP_EOR_IND_LONG */  
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_57: /* INOP 57 */ /* OP_EOR_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                _A(cpu) ^= N;
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_67: /* INOP 67 */ /* OP_ADC_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                add_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_77: /* INOP 77 */ /* OP_ADC_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                add_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_87: /* INOP 87 */ /* OP_STA_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                write_direct_ind_long(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_97: /* INOP 97 */ /* OP_STA_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                write_direct_ind_x_long(cpu, _A(cpu), _Y(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_A7: /* INOP A7 */ /* OP_LDA_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                read_direct_ind_long(cpu, _A(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_B7: /* INOP B7 */ /* OP_LDA_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                read_direct_ind_x_long(cpu, _A(cpu), _Y(cpu));
                set_n_z_flags(cpu, _A(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_C7: /* INOP C7 */ /* OP_CMP_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                compare_and_set_flags(cpu, _A(cpu), N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_D7: /* INOP D7 */ /* OP_CMP_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                compare_and_set_flags(cpu, _A(cpu), N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_E7: /* INOP E7 */ /* OP_SBC_IND_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_long(cpu, N);
                subtract_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_F7: /* INOP F7 */ /* OP_SBC_IND_Y_LONG */
            if constexpr (CPUTraits::has_65816_ops) {
                alu_t N;
                read_direct_ind_x_long(cpu, N, _Y(cpu));
                subtract_and_set_flags(cpu, N);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_0B: /* INOP 0B */ /* OP_PHD_S */
            if constexpr (CPUTraits::has_65816_ops) {
                push_word_new(cpu, cpu->d);
                stack_fix_new(cpu);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_1B: /* INOP 1B */ /* OP_TCS_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                cpu->sp = cpu->a; // always 16-bit transfer.. 
                if constexpr (CPUTraits::e_mode) { // but in e-mode hi byte of S is forced to 0x01.
                    cpu->sp_hi = 0x01;
                }
                // we do not set flags on TCS
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_2B: /* INOP 2B */ /* OP_PLD_S */
            if constexpr (CPUTraits::has_65816_ops) {
                pop_word_new(cpu, cpu->d);
                stack_fix_new(cpu);
                set_n_z_flags(cpu, cpu->d);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_3B: /* INOP 3B */ /* OP_TSC_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                transfer_s_reg(cpu, cpu->a); // always transfer 16 bits no matter value of m flag.
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_4B: /* INOP 4B */ /* OP_PHK_S */
            if constexpr (CPUTraits::has_65816_ops) {
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu))); // cycle 2
                push_byte_new(cpu, cpu->pb); // cycle 3
                stack_fix_new(cpu);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_5B: /* INOP 5B */ /* OP_TCD_IMP*/
            if constexpr (CPUTraits::has_65816_ops) {
                transfer_reg_reg(cpu, cpu->a, cpu->d);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_6B: /* INOP 6B */ /* OP_RTL_S */
            if constexpr (CPUTraits::has_65816_ops) {
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
                stack_rtl(cpu);
                cpu->pc++;
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_7B: /* INOP 7B */ /* OP_TDC_IMP*/
            if constexpr (CPUTraits::has_65816_ops) {
                transfer_reg_reg(cpu, cpu->d, cpu->a);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_8B: /* INOP 8B */ /* OP_PHB_S */
            if constexpr (CPUTraits::has_65816_ops) {
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu))); // cycle 2
                push_byte_new(cpu, cpu->db); // cycle 3
                stack_fix_new(cpu);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_9B: /* INOP 9B */ /* OP_TXY_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                transfer_reg_reg(cpu, _X(cpu), _Y(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_AB: /* INOP AB */ /* OP_PLB_S */
            if constexpr (CPUTraits::has_65816_ops) {
                cpu->db = pop_byte_new(cpu); // "unsafe" pop.
                stack_fix_new(cpu);
                set_n_z_flags(cpu, cpu->db);
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_BB: /* INOP BB */ /* OP_TYX_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                transfer_reg_reg(cpu, _Y(cpu), _X(cpu));
            } else if constexpr (CPUTraits::has_65c02_ops) {
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

        case OP_INOP_EB: /* INOP EB */ /* OP_XBA_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                uint8_t tmp = cpu->a_lo;
                cpu->a_lo = cpu->a_hi;
                cpu->a_hi = tmp;
                set_n_z_flags(cpu, cpu->a_lo);
                phantom_read_ign(cpu, make_pc_long(cpu, _PC(cpu)));
            } else if constexpr (CPUTraits::has_65c02_ops) {
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode);
            break;

        case OP_INOP_FB: /* OP_XCE_IMP */
            if constexpr (CPUTraits::has_65816_ops) {
                bool old_E = cpu->E;
                cpu->E = cpu->C;
                cpu->C = old_E;
                if (cpu->E) {
                    cpu->x_hi = 0;
                    cpu->y_hi = 0;
                    cpu->sp_hi = 0x01;
                    cpu->_M = 1;
                    cpu->_X = 1;
                }
            } else if constexpr (CPUTraits::has_65c02_ops) { // invalid opcode
                invalid_nop(cpu, 1, 1);
            } else invalid_opcode(cpu, opcode); // invalid opcode
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

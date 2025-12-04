/**
 * cputest
 * 
 * perform the 6502_65c02 test suite.
 */

/**
 * Combines: 
 * CPU module (6502 or 65c02)
 * MMU: Base MMU with no Apple-II specific features.
 * 
 * To use: 
 * cd 6502_65c02_functional_tests/bin_files
 * /path/to/cputest [trace_on]
 * 
 * if trace_on is present and is 1, it will print a debug trace of the CPU operation.
 * Otherwise, it will execute the test and report the results. 
 * 
 * You may need to review the 6502_functional_test.lst file to understand the test suite, if it should fail.
 * on a test failure, the suite will execute an instruction that jumps to itself. Our main loop tests for this
 * condition and exits with the PC of the failed test.
 */
#include <SDL3/SDL.h>

#include "gs2.hpp"
//#include "cpu.hpp"
#include "cpus/cpu_implementations.cpp"
#include "mmus/mmu.hpp"

gs2_app_t gs2_app_inues;

uint8_t memory[65536];

uint64_t debug_level = 0;

/**
 * ------------------------------------------------------------------------------------
 * Fake old-style MMU functions. They are only sort-of MMU. They are a a mix of MMU and CPU functions (that alter the program counter)
 */
/* 
uint8_t read_memory(cpu_state *cpu, uint16_t address) {
    return memory[address];
};
void write_memory(cpu_state *cpu, uint16_t address, uint8_t value) {
    memory[address] = value;
}; */

struct instr {
    uint8_t op[3];
};

struct test_record {
    std::string description;

    instr operation = {0, 0, 0};

    uint64_t expected_cycles;

    uint8_t a_in = 0;
    uint8_t x_in = 0;
    uint8_t y_in = 0;
    uint8_t p_in = 0;

};

test_record test_records[] = {

    // ADC

    {
        "ADC #", 
        {0x69, 0},
        2
    },
    {
        "ADC ZP", 
        {0x65, 0},
        3
    },
    {
        "ADC ZP,X", 
        {0x75, 0},
        4
    },
    {
        "ADC ABS", 
        {0x6D, 0, 0},
        4
    },
    {
        "ADC ABS,X", 
        {0x7D, 0, 0},
        4
    },
    {
        "ADC ABS,X w/Crossing", 
        {0x7D, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "ADC ABS,Y", 
        {0x79, 0, 0},
        4
    },
        {
        "ADC ABS,Y w/Crossing", 
        {0x79, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },   
    {
        "ADC (IND,X)", 
        {0x61, 0},
        6
    },
    {
        "ADC (IND),Y", 
        {0x71, 0},
        5
    },

    // AND

    {
        "AND #IMM", 
        {0x29, 0},
        2
    },
    {
        "AND ZP", 
        {0x25, 0},
        3
    },
    {
        "AND ZP,X", 
        {0x35, 0},
        4
    },
    {
        "AND ABS", 
        {0x2D, 0, 0},
        4
    },
    {
        "AND ABS,X", 
        {0x3D, 0, 0},
        4
    },
    {
        "AND ABS,X w/Crossing", 
        {0x3D, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "AND ABS,Y", 
        {0x39, 0, 0},
        4
    },
    {
        "AND ABS,Y w/Crossing", 
        {0x39, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "AND (IND,X)", 
        {0x21, 0},
        6
    },
    {
        "AND (IND),Y", 
        {0x31, 0},
        5
    },
    {
        "AND (IND),Y w/Crossing", 
        {0x31, 0x02},
        6,
        0xFF,
        0xFF,
        0xFF
    },

    // ASL

    {
        "ASL A", 
        {0x0A},
        2
    },
    {
        "ASL ZP", 
        {0x06, 0},
        5
    },
    {
        "ASL ZP,X", 
        {0x16, 0},
        6
    },
    {
        "ASL ABS", 
        {0x0E, 0, 0},
        6
    },
    {
        "ASL ABS,X", 
        {0x1E, 0, 0},
        7
    },

    // BCC
    {
        "BCC No Branch", 
        {0x90, 0x70},  // BCC $1072 (but won't branch)
        2,
        0, 0, 0, 0x01  // Set carry flag (C=1)
    },
    {
        "BCC Branch Same Page", 
        {0x90, 0x70},  // BCC $1072
        3,
        0, 0, 0, 0x00  // Clear carry flag (C=0)
    },
    {
        "BCC Branch Different Page", 
        {0x90, 0xF0},  // BCC $0FF2 (backwards)
        4,
        0, 0, 0, 0x00  // Clear carry flag (C=0)
    },

    // BCS
    {
        "BCS No Branch", 
        {0xB0, 0x70},  // BCS $1072 (but won't branch)
        2,
        0, 0, 0, 0x00  // Clear carry flag (C=0)
    },
    {
        "BCS Branch Same Page", 
        {0xB0, 0x70},  // BCS $1072
        3,
        0, 0, 0, 0x01  // Set carry flag (C=1)
    },
    {
        "BCS Branch Different Page", 
        {0xB0, 0xF0},  // BCS $0FF2 (backwards)
        4,
        0, 0, 0, 0x01  // Set carry flag (C=1)
    },

    // BEQ
    {
        "BEQ No Branch", 
        {0xF0, 0x70},  // BEQ $1072 (but won't branch)
        2,
        0, 0, 0, 0x00  // Set zero flag (Z=1)
    },
    {
        "BEQ Branch Same Page", 
        {0xF0, 0x70},  // BEQ $1072
        3,
        0, 0, 0, 0x02  // Clear zero flag (Z=0)
    },
    {
        "BEQ Branch Different Page", 
        {0xF0, 0xF0},  // BEQ $0FF2 (backwards)
        4,
        0, 0, 0, 0x02  // Clear zero flag (Z=0)
    },


    // BIT
    {
        "BIT ZP", 
        {0x24, 0},
        3
    },
    {
        "BIT ABS", 
        {0x2C, 0, 0},
        4
    },

    // BMI
    {
        "BMI No Branch", 
        {0x30, 0x70},  // BMI $1072 (but won't branch)
        2,
        0, 0, 0, 0x00  // Clear negative flag (N=0)
    },
    {
        "BMI Branch Same Page", 
        {0x30, 0x70},  // BMI $1072
        3,
        0, 0, 0, 0x80  // Set negative flag (N=1)
    },
    {
        "BMI Branch Different Page", 
        {0x30, 0xF0},  // BMI $0FF2 (backwards)
        4,
        0, 0, 0, 0x80  // Set negative flag (N=1)
    },

    // BNE

    {
        "BNE No Branch", 
        {0xD0, 0x70},  // BNE $1072 (but won't branch)
        2,
        0, 0, 0, 0x02  // Set zero flag (Z=1)
    },
    {
        "BNE Branch Same Page", 
        {0xD0, 0x70},  // BNE $1072
        3,
        0, 0, 0, 0x00  // Clear zero flag (Z=0)
    },
    {
        "BNE Branch Different Page", 
        {0xD0, 0xF0},  // BNE $0FF2 (backwards)
        4,
        0, 0, 0, 0x00  // Clear zero flag (Z=0)
    },

    // BPL
    {
        "BPL No Branch", 
        {0x10, 0x70},  // BPL $1072 (but won't branch)
        2,
        0, 0, 0, 0x80  // Set negative flag (N=1)
    },
    {
        "BPL Branch Same Page", 
        {0x10, 0x70},  // BPL $1072
        3,
        0, 0, 0, 0x00  // Clear negative flag (N=0)
    },
    {
        "BPL Branch Different Page", 
        {0x10, 0xF0},  // BPL $0FF2 (backwards)
        4,
        0, 0, 0, 0x00  // Clear negative flag (N=0)
    },

    // BRK
    {
        "BRK", 
        {0x00},
        7
    },

    // BVC
    {
        "BVC No Branch", 
        {0x50, 0x70},  // BVC $1072 (but won't branch)
        2,
        0, 0, 0, 0x40  // Set overflow flag (V=1)
    },
    {
        "BVC Branch Same Page", 
        {0x50, 0x70},  // BVC $1072
        3,
        0, 0, 0, 0x00  // Clear overflow flag (V=0)
    },
    {
        "BVC Branch Different Page", 
        {0x50, 0xF0},  // BVC $0FF2 (backwards)
        4,
        0, 0, 0, 0x00  // Clear overflow flag (V=0)
    },

    // BVS
    {
        "BVS No Branch", 
        {0x70, 0x70},  // BVS $1072 (but won't branch)
        2,
        0, 0, 0, 0x00  // Clear overflow flag (V=0)
    },
    {
        "BVS Branch Same Page", 
        {0x70, 0x70},  // BVS $1072
        3,
        0, 0, 0, 0x40  // Set overflow flag (V=1)
    },
    {
        "BVS Branch Different Page", 
        {0x70, 0xF0},  // BVS $0FF2 (backwards)
        4,
        0, 0, 0, 0x40  // Set overflow flag (V=1)
    },

    // CLC
    {
        "CLC", 
        {0x18},
        2,
        0, 0, 0x01, 0  // Set carry flag (C=1) to test clearing
    },

    // CLD
    {
        "CLD", 
        {0xD8},
        2,
        0, 0, 0, 0x08  // Set decimal flag (D=1) to test clearing
    },

    // CLI
    {
        "CLI", 
        {0x58},
        2,
        0, 0, 0, 0x04  // Set interrupt disable flag (I=1) to test clearing
    },

    // CLV
    {
        "CLV", 
        {0xB8},
        2,
        0, 0, 0, 0x40  // Set overflow flag (V=1) to test clearing
    },

    // CMP

    {
        "CMP #", 
        {0xC9, 0x42},  // CMP #$42
        2,
        0x50, 0, 0, 0  // A=0x50 to test comparison with 0x42
    },
    {
        "CMP ZP", 
        {0xC5, 0x42},  // CMP $42
        3,
        0x50, 0, 0, 0  // A=0x50 to test comparison with value at $42
    },
    {
        "CMP ZP,X", 
        {0xD5, 0x40},  // CMP $40,X
        4,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test comparison with value at $42
    },
    {
        "CMP ABS", 
        {0xCD, 0x42, 0x00},  // CMP $0042
        4,
        0x50, 0, 0, 0  // A=0x50 to test comparison with value at $0042
    },
    {
        "CMP ABS,X No Page Cross", 
        {0xDD, 0x40, 0x00},  // CMP $0040,X
        4,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test comparison with value at $0042
    },
    {
        "CMP ABS,X Page Cross", 
        {0xDD, 0xFF, 0x00},  // CMP $00FF,X
        5,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test comparison with value at $0101
    },
    {
        "CMP ABS,Y No Page Cross", 
        {0xD9, 0x40, 0x00},  // CMP $0040,Y
        4,
        0x50, 0, 0x02, 0  // A=0x50, Y=0x02 to test comparison with value at $0042
    },
    {
        "CMP ABS,Y Page Cross", 
        {0xD9, 0xFF, 0x00},  // CMP $00FF,Y
        5,
        0x50, 0, 0x02, 0  // A=0x50, Y=0x02 to test comparison with value at $0101
    },
    {
        "CMP (IND,X)", 
        {0xC1, 0x40},  // CMP ($40,X)
        6,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test comparison with value at ($42)
    },
    {
        "CMP (IND),Y No Page Cross", 
        {0xD1, 0x00},  // CMP ($00),Y
        5,
        0x50, 0, 0xFF, 0  
    },
    {
        "CMP (IND),Y Page Cross", 
        {0xD1, 0x02},  // CMP ($02),Y
        6,
        0x50, 0, 0xFF, 0  
    },

    // CPX tests
    {
        "CPX #", 
        {0xE0, 0x42},  // CPX #$42
        2,
        0, 0x50, 0, 0  // X=0x50 to test comparison with immediate value 0x42
    },
    {
        "CPX ZP", 
        {0xE4, 0x42},  // CPX $42
        3,
        0, 0x50, 0, 0  // X=0x50 to test comparison with value at $42
    },
    {
        "CPX ABS", 
        {0xEC, 0x42, 0x00},  // CPX $0042
        4,
        0, 0x50, 0, 0  // X=0x50 to test comparison with value at $0042
    },

    // CPY tests
    {
        "CPY #", 
        {0xC0, 0x42},  // CPY #$42
        2,
        0, 0, 0x50, 0  // Y=0x50 to test comparison with immediate value 0x42
    },
    {
        "CPY ZP", 
        {0xC4, 0x42},  // CPY $42
        3,
        0, 0, 0x50, 0  // Y=0x50 to test comparison with value at $42
    },
    {
        "CPY ABS", 
        {0xCC, 0x42, 0x00},  // CPY $0042
        4,
        0, 0, 0x50, 0  // Y=0x50 to test comparison with value at $0042
    },

    // DEC tests
    {
        "DEC ZP", 
        {0xC6, 0x42},  // DEC $42
        5
    },
    {
        "DEC ZP,X", 
        {0xD6, 0x42},  // DEC $42,X
        6
    },
    {
        "DEC ABS", 
        {0xCE, 0x42, 0x00},  // DEC $0042
        6
    },
    {
        "DEC ABS,X", 
        {0xDE, 0x42, 0x00},  // DEC $0042,X
        7
    },

    // DEX test
    {
        "DEX", 
        {0xCA},  // DEX
        2
    },

    // DEY test
    {
        "DEY", 
        {0x88},  // DEY
        2
    },

    // EOR tests
    {
        "EOR #IMM", 
        {0x49, 0},
        2
    },
    {
        "EOR ZP", 
        {0x45, 0},
        3
    },
    {
        "EOR ZP,X", 
        {0x55, 0},
        4
    },
    {
        "EOR ABS", 
        {0x4D, 0, 0},
        4
    },
    {
        "EOR ABS,X", 
        {0x5D, 0, 0},
        4
    },
    {
        "EOR ABS,X w/Crossing", 
        {0x5D, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "EOR ABS,Y", 
        {0x59, 0, 0},
        4
    },
    {
        "EOR ABS,Y w/Crossing", 
        {0x59, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "EOR (IND,X)", 
        {0x41, 0},
        6
    },
    {
        "EOR (IND),Y", 
        {0x51, 0},
        5
    },
    {
        "EOR (IND),Y w/Crossing", 
        {0x51, 0x02},
        6,
        0xFF,
        0xFF,
        0xFF
    },

    // INC
    {
        "INC ZP", 
        {0xE6, 0},
        5
    },
    {
        "INC ZP,X", 
        {0xF6, 0},
        6
    },
    {
        "INC ABS", 
        {0xEE, 0, 0},
        6
    },
    {
        "INC ABS,X", 
        {0xFE, 0, 0},
        7
    },

    // INX
    {
        "INX", 
        {0xE8},
        2
    },

    // INY
    {
        "INY", 
        {0xC8},
        2
    },

    // JMP
    {
        "JMP ABS", 
        {0x4C, 0, 0},
        3
    },
    {
        "JMP Indirect", 
        {0x6C, 0, 0},
        5
    },

    // JSR
    {
        "JSR ABS", 
        {0x20, 0, 0},
        6
    },

    // LDA
    {
        "LDA #IMM", 
        {0xA9, 0},
        2
    },
    {
        "LDA ZP", 
        {0xA5, 0},
        3
    },
    {
        "LDA ZP,X", 
        {0xB5, 0},
        4
    },
    {
        "LDA ABS", 
        {0xAD, 0, 0},
        4
    },
    {
        "LDA ABS,X", 
        {0xBD, 0, 0},
        4
    },
    {
        "LDA ABS,X w/Crossing", 
        {0xBD, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "LDA ABS,Y", 
        {0xB9, 0, 0},
        4
    },
    {
        "LDA ABS,Y w/Crossing", 
        {0xB9, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "LDA (IND,X)", 
        {0xA1, 0},
        6
    },
    {
        "LDA (IND),Y", 
        {0xB1, 0},
        5
    },
    {
        "LDA (IND),Y w/Crossing", 
        {0xB1, 0x02},
        6,
        0xFF,
        0xFF,
        0xFF
    },

    // LDX
    {
        "LDX #IMM", 
        {0xA2, 0},
        2
    },
    {
        "LDX ZP", 
        {0xA6, 0},
        3
    },
    {
        "LDX ZP,Y", 
        {0xB6, 0},
        4
    },
    {
        "LDX ABS", 
        {0xAE, 0, 0},
        4
    },
    {
        "LDX ABS,Y", 
        {0xBE, 0, 0},
        4
    },
    {
        "LDX ABS,Y w/Crossing", 
        {0xBE, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },

    // LDY
    {
        "LDY #IMM", 
        {0xA0, 0},
        2
    },
    {
        "LDY ZP", 
        {0xA4, 0},
        3
    },
    {
        "LDY ZP,X", 
        {0xB4, 0},
        4
    },
    {
        "LDY ABS", 
        {0xAC, 0, 0},
        4
    },
    {
        "LDY ABS,X", 
        {0xBC, 0, 0},
        4
    },
    {
        "LDY ABS,X w/Crossing", 
        {0xBC, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },

    // LSR
    {
        "LSR A", 
        {0x4A},  // LSR A
        2,
        0x80, 0, 0, 0  // A=0x80 to test shifting right (should become 0x40)
    },
    {
        "LSR ZP", 
        {0x46, 0x42},  // LSR $42
        5,
        0, 0, 0, 0  // Value at $42 will be 0x80 to test shifting right
    },
    {
        "LSR ZP,X", 
        {0x56, 0x40},  // LSR $40,X
        6,
        0, 0x02, 0, 0  // X=0x02, value at $42 will be 0x80 to test shifting right
    },
    {
        "LSR ABS", 
        {0x4E, 0x42, 0x00},  // LSR $0042
        6,
        0, 0, 0, 0  // Value at $0042 will be 0x80 to test shifting right
    },
    {
        "LSR ABS,X", 
        {0x5E, 0x40, 0x00},  // LSR $0040,X
        7,
        0, 0x02, 0, 0  // X=0x02, value at $0042 will be 0x80 to test shifting right
    },

    // NOP
    {
        "NOP", 
        {0xEA},  // NOP
        2
    },

    // ORA

    {
        "ORA #IMM", 
        {0x09, 0},
        2
    },
    {
        "ORA ZP", 
        {0x05, 0},
        3
    },
    {
        "ORA ZP,X", 
        {0x15, 0},
        4
    },
    {
        "ORA ABS", 
        {0x0D, 0, 0},
        4
    },
    {
        "ORA ABS,X", 
        {0x1D, 0, 0},
        4
    },
    {
        "ORA ABS,X w/Crossing", 
        {0x1D, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "ORA ABS,Y", 
        {0x19, 0, 0},
        4
    },
    {
        "ORA ABS,Y w/Crossing", 
        {0x19, 0xFF, 0},
        5,
        0xFF,
        0xFF,
        0xFF
    },
    {
        "ORA (IND,X)", 
        {0x01, 0},
        6
    },
    {
        "ORA (IND),Y", 
        {0x11, 0},
        5
    },
    {
        "ORA (IND),Y w/Crossing", 
        {0x11, 0x02},
        6,
        0xFF,
        0xFF,
        0xFF
    },

    // PHA
    {
        "PHA", 
        {0x48},  // PHA
        3,
        0x42, 0, 0, 0  // A=0x42 to test pushing to stack
    },

    {
        "PHP", 
        {0x08},  // PHP
        3,
        0, 0, 0, 0xFF  // All flags set to test pushing status register
    },

    {
        "PLA", 
        {0x68},  // PLA
        4,
        0, 0, 0, 0  // Initial A value doesn't matter as we're pulling from stack
    },

    {
        "PLP", 
        {0x28},  // PLP
        4,
        0, 0, 0, 0  // Initial status doesn't matter as we're pulling from stack
    },

    // ROL

    {
        "ROL A", 
        {0x2A},  // ROL A
        2
    },
    {
        "ROL ZP", 
        {0x26, 0},  // ROL $00
        5
    },
    {
        "ROL ZP,X", 
        {0x36, 0},  // ROL $00,X
        6
    },
    {
        "ROL ABS", 
        {0x2E, 0, 0},  // ROL $0000
        6
    },
    {
        "ROL ABS,X", 
        {0x3E, 0, 0},  // ROL $0000,X
        7
    },

    // ROR 

    {
        "ROR A", 
        {0x6A},
        2
    },
    {
        "ROR ZP", 
        {0x66, 0 },
        5
    },
    {
        "ROR ZP,X", 
        {0x76, 0}, 
        6
    },
    {
        "ROR ABS", 
        {0x6E, 0, 0}, 
        6
    },
    {
        "ROR ABS,X", 
        {0x7E, 0, 0}, 
        7
    },

    // RTI
    {
        "RTI", 
        {0x40},  // RTI
        6,
        0, 0, 0, 0x00  // Initial status register value (will be pulled from stack)
    },

    // RTS
    {
        "RTS", 
        {0x60},  // RTS
        6
    },

    // SBC tests
    {
        "SBC Immediate", 
        {0xE9, 0x42},  // SBC #$42
        2,
        0x50, 0, 0, 0x01  // A=0x50, C=1 to test subtraction with borrow
    },
    {
        "SBC Zero Page", 
        {0xE5, 0x42},  // SBC $42
        3,
        0x50, 0, 0, 0x01  // A=0x50, C=1 to test subtraction with borrow
    },
    {
        "SBC Zero Page,X", 
        {0xF5, 0x40},  // SBC $40,X
        4,
        0x50, 0x02, 0, 0x01  // A=0x50, X=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC Absolute", 
        {0xED, 0x42, 0x00},  // SBC $0042
        4,
        0x50, 0, 0, 0x01  // A=0x50, C=1 to test subtraction with borrow
    },
    {
        "SBC Absolute,X No Page Cross", 
        {0xFD, 0x40, 0x00},  // SBC $0040,X
        4,
        0x50, 0x02, 0, 0x01  // A=0x50, X=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC Absolute,X Page Cross", 
        {0xFD, 0xFF, 0x00},  // SBC $00FF,X
        5,
        0x50, 0x02, 0, 0x01  // A=0x50, X=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC Absolute,Y No Page Cross", 
        {0xF9, 0x40, 0x00},  // SBC $0040,Y
        4,
        0x50, 0, 0x02, 0x01  // A=0x50, Y=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC Absolute,Y Page Cross", 
        {0xF9, 0xFF, 0x00},  // SBC $00FF,Y
        5,
        0x50, 0, 0x02, 0x01  // A=0x50, Y=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC (Indirect,X)", 
        {0xE1, 0x40},  // SBC ($40,X)
        6,
        0x50, 0x02, 0, 0x01  // A=0x50, X=0x02, C=1 to test subtraction with borrow
    },
    {
        "SBC (Indirect),Y No Page Cross", 
        {0xF1, 0x00},  // SBC ($00),Y
        5,
        0x50, 0, 0xFF, 0x01  // A=0x50, Y=0xFF, C=1 to test subtraction with borrow
    },
    {
        "SBC (Indirect),Y Page Cross", 
        {0xF1, 0x02},  // SBC ($02),Y
        6,
        0x50, 0, 0xFF, 0x01  // A=0x50, Y=0xFF, C=1 to test subtraction with borrow
    },

    // SEC
    {
        "SEC", 
        {0x38},  // SEC
        2,
        0, 0, 0, 0x00  // Clear carry flag (C=0) to test setting
    },

    // SED
    {
        "SED", 
        {0xF8},  // SED
        2,
        0, 0, 0, 0x00  // Clear decimal flag (D=0) to test setting
    },

    // SEI
    {
        "SEI", 
        {0x78},  // SEI
        2,
        0, 0, 0, 0x00  // Clear interrupt disable flag (I=0) to test setting
    },

    // STA
    {
        "STA Zero Page", 
        {0x85, 0x42},  // STA $42
        3,
        0x50, 0, 0, 0  // A=0x50 to test storing
    },
    {
        "STA Zero Page,X", 
        {0x95, 0x40},  // STA $40,X
        4,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test storing at $42
    },
    {
        "STA Absolute", 
        {0x8D, 0x42, 0x00},  // STA $0042
        4,
        0x50, 0, 0, 0  // A=0x50 to test storing
    },
    {
        "STA Absolute,X", 
        {0x9D, 0x40, 0x00},  // STA $0040,X
        5,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test storing at $0042
    },
    {
        "STA Absolute,Y", 
        {0x99, 0x40, 0x00},  // STA $0040,Y
        5,
        0x50, 0, 0x02, 0  // A=0x50, Y=0x02 to test storing at $0042
    },
    {
        "STA (Indirect,X)", 
        {0x81, 0x40},  // STA ($40,X)
        6,
        0x50, 0x02, 0, 0  // A=0x50, X=0x02 to test storing at ($42)
    },
    {
        "STA (Indirect),Y", 
        {0x91, 0x00},  // STA ($00),Y
        6,
        0x50, 0, 0xFF, 0  // A=0x50, Y=0xFF to test storing
    },

    // STX tests
    {
        "STX Zero Page", 
        {0x86, 0x42},  // STX $42
        3,
        0, 0x50, 0, 0  // X=0x50 to test storing
    },
    {
        "STX Zero Page,Y", 
        {0x96, 0x40},  // STX $40,Y
        4,
        0, 0x50, 0x02, 0  // X=0x50, Y=0x02 to test storing at $42
    },
    {
        "STX Absolute", 
        {0x8E, 0x42, 0x00},  // STX $0042
        4,
        0, 0x50, 0, 0  // X=0x50 to test storing
    },

    // STY tests
    {
        "STY Zero Page", 
        {0x84, 0x42},  // STY $42
        3,
        0, 0, 0x50, 0  // Y=0x50 to test storing
    },
    {
        "STY Zero Page,X", 
        {0x94, 0x40},  // STY $40,X
        4,
        0, 0x02, 0x50, 0  // X=0x02, Y=0x50 to test storing at $42
    },
    {
        "STY Absolute", 
        {0x8C, 0x42, 0x00},  // STY $0042
        4,
        0, 0, 0x50, 0  // Y=0x50 to test storing
    },

    // TAX
    {
        "TAX", 
        {0xAA},  // TAX
        2,
        0x42, 0, 0, 0  // A=0x42 to test transfer to X
    },

    // TAY
    {
        "TAY", 
        {0xA8},  // TAY
        2,
        0x42, 0, 0, 0  // A=0x42 to test transfer to Y
    },

    // TSX
    {
        "TSX", 
        {0xBA},  // TSX
        2,
        0, 0, 0, 0  // Initial A value doesn't matter as we're transferring from SP
    },

    // TXA
    {
        "TXA", 
        {0x8A},  // TXA
        2,
        0, 0x42, 0, 0  // X=0x42 to test transfer to A
    },

    // TXS
    {
        "TXS", 
        {0x9A},  // TXS
        2,
        0, 0x42, 0, 0  // X=0x42 to test transfer to SP
    },

    // TYA
    {
        "TYA", 
        {0x98},  // TYA
        2,
        0, 0, 0x42, 0  // Y=0x42 to test transfer to A
    },


};

int test_records_count = sizeof(test_records) / sizeof(test_records[0]);

bool test_instruction(std::unique_ptr<BaseCPU> &cpux, cpu_state *cpu, test_record *testrec) {
    uint64_t start_cycles = cpu->cycles;

    (cpux->execute_next)(cpu);
    uint64_t elapsed = cpu->cycles - start_cycles;
    //printf("Instruction %04X took %llu cycles\n", testrec->operation.op[0], elapsed);
    uint64_t expected_cycles = testrec->expected_cycles;
    if (elapsed != expected_cycles) {
        //printf("Expected %llu cycles, got %llu cycles\n", expected_cycles, elapsed);
        return false;
    }
    return true;
}

/**
 * ------------------------------------------------------------------------------------
 * Main
 */

int main(int argc, char **argv) {
    bool trace_on = true;
    processor_type cputype = PROCESSOR_6502;


    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "trace") == 0) {
            trace_on = true;
        } else if (strcmp(argv[i], "6502") == 0) {
            cputype = PROCESSOR_6502;
        } else if (strcmp(argv[i], "65c02") == 0) {
            cputype = PROCESSOR_65C02;
        } else if (strcmp(argv[i], "65816") == 0) {
            cputype = PROCESSOR_65816;
        } else {
            printf("usage: cputest [trace] [6502|65c02] [test6502|test65c02|testdecimal]\n");
        }
    }

    printf("   CPU Type: ");
    switch (cputype) {
        case PROCESSOR_6502:
            printf(" 6502\n");
            break;
        case PROCESSOR_65C02:
            printf(" 65c02\n");
            break;
        case PROCESSOR_65816:
            printf(" 65816\n");
            break;
        default:
            printf("Invalid CPU type\n");
            exit(1);
    }

    printf("Starting CPU Cycle Counts test...\n");
    if (argc > 1) {
        trace_on = atoi(argv[1]);
    }

    gs2_app_inues.base_path = "./";
    gs2_app_inues.pref_path = gs2_app_inues.base_path;
    gs2_app_inues.console_mode = false;


// create MMU, map all pages to our "ram"
    MMU *mmu = new MMU(256, GS2_PAGE_SIZE);
    for (int i = 0; i < 256; i++) {
        mmu->map_page_both(i, &memory[i*256], "TEST RAM");
    }

    cpu_state *cpu = new cpu_state(cputype);
    //cpu->set_processor(PROCESSOR_6502);
    cpu->cpun = createCPU(cputype);
    cpu->core = cpu->cpun.get();

    //cpu->init();
    cpu->trace = trace_on;
    cpu->set_mmu(mmu);

    cpu->reset(); // reset the CPU - AFTER setting MMU - required to set additional registers etc on powerup/reset.

    uint64_t start_time = SDL_GetTicksNS();

// set up page crossing test for (ZP),Y
    mmu->write(0x0002, 0xFF);
    mmu->write(0x0003, 0x10);

    int failedtests = 0;
    for (int i = 0; i < test_records_count; i++) {

        mmu->write(0x0000, 0x00);
        mmu->write(0x0001, 0x00);
        
        cpu->cycles = 0;
        cpu->pc = 0x1000;
        cpu->a = test_records[i].a_in;
        cpu->x = test_records[i].x_in;
        cpu->y = test_records[i].y_in;
        cpu->p = test_records[i].p_in;
        mmu->write(0x1000, test_records[i].operation.op[0]);
        mmu->write(0x1001, test_records[i].operation.op[1]);
        mmu->write(0x1002, test_records[i].operation.op[2]);

        bool result = test_instruction(cpu->cpun, cpu, &test_records[i]);
        char * trace_entry = cpu->trace_buffer->decode_trace_entry(&cpu->trace_entry);
        // trim the start (cycles always 0 is not informative)
        trace_entry[90] = '\0';
        printf("%-30s [%1llu] %s ", test_records[i].description.c_str(), test_records[i].expected_cycles, trace_entry + 20);

        if (!result) {
            printf("FAILED [%llu != %llu]", cpu->cycles, test_records[i].expected_cycles);
            failedtests++;
        }
        printf("\n");
    }

    printf("Failed tests: %d\n", failedtests);

    return 0;
}

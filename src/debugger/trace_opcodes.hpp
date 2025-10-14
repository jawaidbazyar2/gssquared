#pragma once

#include <cstdint>
#include <stddef.h>


enum address_mode {
    NONE = 0,
    ACC = 1,
    ABS = 2,
    ABS_X = 3,
    ABS_Y = 4,
    IMM = 5,
    IMP = 6,
    INDIR = 7,
    INDEX_INDIR = 8,
    INDIR_INDEX = 9,
    REL = 10,
    ZP = 11,
    ZP_X = 12,
    ZP_Y = 13,
    ABS_IND_X = 14, // 65c02
    ZP_IND = 15, // 65c02
    ABSL = 16, // 65816
    ABSL_X = 17, // 65816
    IND_LONG = 18, // 65816
    IND_Y_LONG = 19, // 65816
    REL_S = 20, // 65816
    REL_S_Y = 21, // 65816
    REL_L = 22, // 65816
    ABS_IND_LONG = 23, // 65816
    IMM_S = 24, // 65816
    MOVE = 25, // 65816
};

#define CPU_6502 0x0001
#define CPU_65C02 0x0002
#define CPU_65816 0x0004
#define MIN_6502 (CPU_6502 | CPU_65C02 | CPU_65816)
#define CPU_ALL (CPU_6502 | CPU_65C02 | CPU_65816)
#define CPU_65C02_UP (CPU_65C02 | CPU_65816)
#define MIN_65C02 (CPU_65C02 | CPU_65816)
#define MIN_65816 (CPU_65816)

struct disasm_entry {
    const char *opcode;
    address_mode mode;
    uint16_t cpu_mask;
};

struct address_mode_entry {
    const char *format;
    uint8_t size; // how many bytes is the instruction
};

extern const disasm_entry disasm_table[256];
extern const address_mode_entry address_mode_formats[];

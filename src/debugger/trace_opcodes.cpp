/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

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

#include "debugger/trace_opcodes.hpp"

const address_mode_entry address_mode_formats[] = {
   { "???", 1 },       // unknown
    { "A", 1 },        // ACC
    { "$%04X", 3 },    // ABS
    { "$%04X,X", 3 },    // ABS_X
    { "$%04X,Y", 3 },    // ABS_Y
    { "#$%02X", 2 },    // IMM
    { "", 1 },           // IMP
    { "($%04X)", 3 },    // INDIR
    { "($%02X,X)", 2 },  // INDEX_INDIR
    { "($%02X),Y", 2 },  // INDIR_INDEX
    { "$%02X", 2 },      // REL
    { "$%02X", 2 },      // ZP
    { "$%02X,X", 2 },    // ZP_X
    { "$%02X,Y", 2 },    // ZP,Y
    { "($%04X,X)", 3},   // ABS_IND_X
    { "($%02X)", 2},     // ZP_IND
    { "$%06X", 4},       // ABSL
    { "$%06X,X", 4},     // ABSL_X
    { "[%02X]", 2},       // IND_LONG
    { "[%02X],Y", 2},     // IND_Y_LONG
    { "$%02X,S", 2},     // REL_S
    { "($%02X,S),Y", 2}, // REL_S_Y
    { "$%04X", 3},       // REL_L
    { "[$%04X]", 3},       // ABS_IND_LONG
    { "#$%04X", 2},       // IMM_S
    { "$%02X,$%02X", 3},       // MOVE
};

const disasm_entry disasm_table[256] = { 
    { "BRK", IMP, MIN_6502 },  /* 00 */
    { "ORA", INDEX_INDIR, MIN_6502 }, /* 01 */
    { "COP",  IMP, MIN_65816 }, /* 02 */
    { "ORA",  REL_S, MIN_65816 }, /* 03 */
    { "TSB",  ZP, MIN_65C02 }, /* 04 */
    { "ORA", ZP, MIN_6502 }, /* 05 */
    { "ASL", ZP, MIN_6502 }, /* 06 */
    { "ORA", IND_LONG, MIN_65816 }, /* 07 */
    { "PHP", IMP, MIN_6502 }, /* 08 */
    { "ORA", IMM, MIN_6502 }, /* 09 */
    { "ASL", ACC, MIN_6502 }, /* 0a */
    { "PHD",  IMP, MIN_65816 }, /* 0b */
    { "TSB", ABS, MIN_65C02 }, /* 0c */
    { "ORA", ABS, MIN_6502 }, /* 0d */
    { "ASL", ABS, MIN_6502 }, /* 0e */
    { "ORA", ABSL, MIN_65816 }, /* 0f */
    { "BPL", REL, MIN_6502 }, /* 10 */
    { "ORA", INDEX_INDIR, MIN_6502 }, /* 11 */
    { "ORA", ZP_IND, MIN_65C02 }, /* 12 */
    { "ORA",  REL_S_Y, MIN_65816 }, /* 13 */
    { "TRB",  ZP, MIN_65C02 }, /* 14 */
    { "ORA", ZP_X, MIN_6502 }, /* 15 */
    { "ASL", ZP_X, MIN_6502 }, /* 16 */
    { "ORA", IND_Y_LONG, MIN_65816 }, /* 17 */
    { "CLC", IMP, MIN_6502 }, /* 18 */
    { "ORA", ABS_Y, MIN_6502 }, /* 19 */
    { "INA",  IMP, MIN_65C02 }, /* 1a */
    { "TCS",  IMP, MIN_65816 }, /* 1b */
    { "TRB", ABS, MIN_65C02 }, /* 1c */
    { "ORA", ABS_X, MIN_6502 }, /* 1d */
    { "ASL", ABS_X, MIN_6502 }, /* 1e */
    { "ORA", ABSL_X, MIN_65816 }, /* 1f */
    { "JSR", ABS, MIN_6502 }, /* 20 */
    { "AND", INDEX_INDIR, MIN_6502 }, /* 21 */
    { "JSL",  ABSL, MIN_65816 }, /* 22 */
    { "AND",  REL_S, MIN_65816 }, /* 23 */
    { "BIT", ZP, MIN_6502 }, /* 24 */
    { "AND", ZP, MIN_6502 }, /* 25 */
    { "ROL", ZP , MIN_6502}, /* 26 */
    { "AND", IND_LONG, MIN_65816 }, /* 27 */
    { "PLP", IMP, MIN_6502 }, /* 28 */
    { "AND", IMM , MIN_6502}, /* 29 */
    { "ROL", ACC, MIN_6502 }, /* 2a */
    { "PLD",  IMP, MIN_65816 }, /* 2b */
    { "BIT", ABS, MIN_6502 }, /* 2c */
    { "AND", ABS, MIN_6502 }, /* 2d */
    { "ROL", ABS, MIN_6502 }, /* 2e */
    { "AND", ABSL, MIN_65816 }, /* 2f */
    { "BMI", REL, MIN_6502 }, /* 30 */
    { "AND", INDIR_INDEX, MIN_6502 }, /* 31 */
    { "AND", ZP_IND, MIN_65C02 }, /* 32 */
    { "AND",  REL_S_Y, MIN_65816 }, /* 33 */
    { "BIT", ZP_X, MIN_65C02 }, /* 34 */
    { "AND", ZP_X, MIN_6502 }, /* 35 */
    { "ROL", ZP_X, MIN_6502 }, /* 36 */
    { "AND", IND_Y_LONG, MIN_65816 }, /* 37 */
    { "SEC", IMP, MIN_6502 }, /* 38 */
    { "AND", ABS_Y, MIN_6502 }, /* 39 */
    { "DEA",  IMP, MIN_65C02 }, /* 3a */
    { "TSC",  IMP, MIN_65816 }, /* 3b */
    { "BIT", ABS_X, MIN_65C02 }, /* 3c */
    { "AND", ABS_X, MIN_6502 }, /* 3d */
    { "ROL", ABS_X, MIN_6502 }, /* 3e */
    { "AND", ABSL_X, MIN_65816 }, /* 3f */
    { "RTI", IMP, MIN_6502 }, /* 40 */
    { "EOR", INDEX_INDIR, MIN_6502 }, /* 41 */
    { "WDM",  IMP, MIN_65816 }, /* 42 */
    { "EOR",  REL_S, MIN_65816 }, /* 43 */
    { NULL,  NONE, MIN_6502 }, /* 44 */
    { "EOR", ZP, MIN_6502 }, /* 45 */
    { "LSR", ZP, MIN_6502 }, /* 46 */
    { "EOR", IND_LONG, MIN_65816 }, /* 47 */
    { "PHA", IMP, MIN_6502 }, /* 48 */
    { "EOR", IMM, MIN_6502 }, /* 49 */
    { "LSR", ACC, MIN_6502 }, /* 4a */
    { "PHK",  IMP, MIN_65816 }, /* 4b */
    { "JMP", ABS, MIN_6502 }, /* 4c */
    { "EOR", ABS, MIN_6502 }, /* 4d */
    { "LSR", ABS, MIN_6502 }, /* 4e */
    { "EOR", ABSL, MIN_65816 }, /* 4f */
    { "BVC", REL, MIN_6502 }, /* 50 */
    { "EOR", INDIR_INDEX, MIN_6502 }, /* 51 */
    { "EOR", ZP_IND, MIN_65C02 }, /* 52 */
    { "EOR",  REL_S_Y, MIN_65816 }, /* 53 */
    { "MVN",  MOVE, MIN_65816 }, /* 54 */
    { "EOR", ZP_X, MIN_6502 }, /* 55 */
    { "LSR", ZP_X, MIN_6502 }, /* 56 */
    { "EOR", IND_Y_LONG, MIN_65816 }, /* 57 */
    { "CLI", IMP, MIN_6502 }, /* 58 */
    { "EOR", ABS_Y, MIN_6502 }, /* 59 */
    { "PHY", IMP, MIN_65C02 }, /* 5a */
    { "TCD",  IMP, MIN_65816 }, /* 5b */
    { "JML",  ABSL, MIN_65816 }, /* 5c */
    { "EOR", ABS_X, MIN_6502 }, /* 5d */
    { "LSR", ABS_X , MIN_6502}, /* 5e */
    { "EOR", ABSL_X, MIN_65816 }, /* 5f */
    { "RTS", IMP, MIN_6502 }, /* 60 */
    { "ADC", INDEX_INDIR, MIN_6502 }, /* 61 */
    { "PER",  REL_L, MIN_65816 }, /* 62 */
    { "ADC",  REL_S, MIN_65816 }, /* 63 */
    { "STZ", ZP, MIN_65C02 }, /* 64 */
    { "ADC", ZP, MIN_6502 }, /* 65 */
    { "ROR", ZP, MIN_6502 }, /* 66 */
    { "ADC", IND_LONG, MIN_65816 }, /* 67 */
    { "PLA", IMP, MIN_6502 }, /* 68 */
    { "ADC", IMM, MIN_6502 }, /* 69 */
    { "ROR", ACC, MIN_6502 }, /* 6a */
    { "RTL",  IMP, MIN_65816 }, /* 6b */
    { "JMP", INDIR, MIN_6502 }, /* 6c */
    { "ADC", ABS, MIN_6502 }, /* 6d */
    { "ROR", ABS, MIN_6502 }, /* 6e */
    { "ADC", ABSL, MIN_65816 }, /* 6f */
    { "BVS", REL, MIN_6502 }, /* 70 */
    { "ADC", INDIR_INDEX, MIN_6502 }, /* 71 */
    { "ADC", ZP_IND, MIN_65C02 }, /* 72 */
    { "ADC",  REL_S_Y, MIN_65816 }, /* 73 */
    { "STZ", ZP_X, MIN_65C02 }, /* 74 */
    { "ADC", ZP_X, MIN_6502 }, /* 75 */
    { "ROR", ZP_X, MIN_6502 }, /* 76 */
    { "ADC", IND_Y_LONG, MIN_65816 }, /* 77 */
    { "SEI", IMP, MIN_6502 }, /* 78 */
    { "ADC", ABS_Y, MIN_6502 }, /* 79 */
    { "PLY", IMP, MIN_65C02 }, /* 7a */
    { "TDC", IMP, MIN_65816 }, /* 7b */
    { "JMP", ABS_IND_X, MIN_65C02 }, /* 7c */
    { "ADC", ABS_X, MIN_6502 }, /* 7d */
    { "ROR", ABS_X, MIN_6502 }, /* 7e */
    { "ADC", ABSL_X, MIN_65816 }, /* 7f */
    { "BRA", REL, MIN_65C02 }, /* 80 */
    { "STA", INDEX_INDIR, MIN_6502 }, /* 81 */
    { "BRL", REL_L, MIN_65816 }, /* 82 */
    { "STA",  REL_S, MIN_65816 }, /* 83 */
    { "STY", ZP, MIN_6502 }, /* 84 */
    { "STA", ZP, MIN_6502 }, /* 85 */
    { "STX", ZP, MIN_6502 }, /* 86 */
    { "STA", IND_LONG, MIN_65816 }, /* 87 */
    { "DEY", IMP, MIN_6502 }, /* 88 */
    { "BIT", IMM, MIN_65C02 }, /* 89 */
    { "TXA", IMP, MIN_6502 }, /* 8a */
    { "PHB", IMP, MIN_65816 }, /* 8b */
    { "STY", ABS, MIN_6502 }, /* 8c */
    { "STA", ABS, MIN_6502 }, /* 8d */
    { "STX", ABS, MIN_6502 }, /* 8e */
    { "STA", ABSL, MIN_65816 }, /* 8f */
    { "BCC", REL, MIN_6502 }, /* 90 */
    { "STA", INDIR_INDEX, MIN_6502 }, /* 91 */
    { "STA", ZP_IND, MIN_65C02 }, /* 92 */
    { "STA",  REL_S_Y, MIN_65816 }, /* 93 */
    { "STY", ZP_X, MIN_6502 }, /* 94 */
    { "STA", ZP_X, MIN_6502 }, /* 95 */
    { "STX", ZP_Y, MIN_6502 }, /* 96 */
    { "STA", IND_Y_LONG, MIN_65816 }, /* 97 */
    { "TYA", IMP, MIN_6502 }, /* 98 */
    { "STA", ABS_Y, MIN_6502 }, /* 99 */
    { "TXS", IMP, MIN_6502 }, /* 9a */
    { "TXY", IMP, MIN_65816 }, /* 9b */
    { "STZ", ABS, MIN_65C02 }, /* 9c */
    { "STA", ABS_X, MIN_6502 }, /* 9d */
    { "STZ", ABS_X, MIN_65C02 }, /* 9e */
    { "STA", ABSL_X, MIN_65816 }, /* 9f */
    { "LDY", IMM, MIN_6502 }, /* a0 */
    { "LDA", INDEX_INDIR, MIN_6502 }, /* a1 */
    { "LDX", IMM, MIN_6502 }, /* a2 */
    { "LDA",  REL_S, MIN_65816 }, /* a3 */
    { "LDY", ZP, MIN_6502 }, /* a4 */
    { "LDA", ZP, MIN_6502 }, /* a5 */
    { "LDX", ZP, MIN_6502 }, /* a6 */
    { "LDA", IND_LONG, MIN_65816 }, /* a7 */
    { "TAY", IMP, MIN_6502 }, /* a8 */
    { "LDA", IMM, MIN_6502 }, /* a9 */
    { "TAX", IMP, MIN_6502 }, /* aa */
    { "PLB", IMP, MIN_65816 }, /* ab */
    { "LDY", ABS, MIN_6502 }, /* ac */
    { "LDA", ABS, MIN_6502 }, /* ad */
    { "LDX", ABS, MIN_6502 }, /* ae */
    { "LDA", ABSL, MIN_65816 }, /* af */
    { "BCS", REL, MIN_6502 }, /* b0 */
    { "LDA", INDIR_INDEX, MIN_6502 }, /* b1 */
    { "LDA", ZP_IND, MIN_65C02 }, /* b2 */
    { "LDA",  REL_S_Y, MIN_65816 }, /* b3 */
    { "LDY", ZP_X, MIN_6502 }, /* b4 */
    { "LDA", ZP_X, MIN_6502 }, /* b5 */
    { "LDX", ZP_Y, MIN_6502 }, /* b6 */
    { "LDA", IND_Y_LONG, MIN_65816 }, /* b7 */
    { "CLV", IMP, MIN_6502 }, /* b8 */
    { "LDA", ABS_Y, MIN_6502 }, /* b9 */
    { "TSX", IMP, MIN_6502 }, /* ba */
    { "TYX", IMP, MIN_65816 }, /* bb */
    { "LDY", ABS_X, MIN_6502 }, /* bc */
    { "LDA", ABS_X, MIN_6502 }, /* bd */
    { "LDX", ABS_Y, MIN_6502 }, /* be */
    { "LDA", ABSL_X, MIN_65816 }, /* bf */
    { "CPY", IMM, MIN_6502 }, /* c0 */
    { "CMP", INDEX_INDIR, MIN_6502 }, /* c1 */
    { "REP", IMM, MIN_65816 }, /* c2 */
    { "CMP",  REL_S, MIN_65816 }, /* c3 */
    { "CPY", ZP, MIN_6502 }, /* c4 */
    { "CMP", ZP, MIN_6502 }, /* c5 */
    { "DEC", ZP, MIN_6502 }, /* c6 */
    { "CMP", IND_LONG, MIN_65816 }, /* c7 */
    { "INY", IMP, MIN_6502 }, /* c8 */
    { "CMP", IMM, MIN_6502 }, /* c9 */
    { "DEX", IMP, MIN_6502 }, /* ca */
    { "WAI", IMP, MIN_65816 }, /* cb */
    { "CPY", ABS, MIN_6502 }, /* cc */
    { "CMP", ABS, MIN_6502 }, /* cd */
    { "DEC", ABS, MIN_6502 }, /* ce */
    { "CMP", ABSL, MIN_65816 }, /* cf */
    { "BNE", REL, MIN_6502 }, /* d0 */
    { "CMP", INDIR_INDEX, MIN_6502 }, /* d1 */
    { "CMP", ZP_IND, MIN_65C02 }, /* d2 */
    { "CMP",  REL_S_Y, MIN_65816 }, /* d3 */
    { "PEI", ZP, MIN_65816 }, /* d4 */
    { "CMP", ZP_X, MIN_6502 }, /* d5 */
    { "DEC", ZP_X, MIN_6502 }, /* d6 */
    { "CMP", IND_Y_LONG, MIN_65816 }, /* d7 */
    { "CLD", IMP, MIN_6502 }, /* d8 */
    { "CMP", ABS_Y, MIN_6502 }, /* d9 */
    { "PHX", IMP, MIN_65C02 }, /* da */
    { "STP", IMP, MIN_65816 }, /* db */
    { "JML", ABS_IND_LONG, MIN_65816 }, /* dc */
    { "CMP", ABS_X, MIN_6502 }, /* dd */
    { "DEC", ABS_X, MIN_6502 }, /* de */
    { "CMP", ABSL_X, MIN_65816 }, /* df */
    { "CPX", IMM, MIN_6502 }, /* e0 */
    { "SBC", INDEX_INDIR, MIN_6502 }, /* e1 */
    { "SEP", IMM, MIN_65816 }, /* e2 */
    { "SBC", REL_S, MIN_65816 }, /* e3 */
    { "CPX", ZP, MIN_6502 }, /* e4 */
    { "SBC", ZP, MIN_6502 }, /* e5 */
    { "INC", ZP, MIN_6502 }, /* e6 */
    { "SBC", IND_LONG, MIN_65816 }, /* e7 */
    { "INX", IMP, MIN_6502 }, /* e8 */
    { "SBC", IMM, MIN_6502 }, /* e9 */
    { "NOP", IMP, MIN_6502 }, /* ea */
    { "XBA", IMP, MIN_65816 }, /* eb */
    { "CPX", ABS, MIN_6502 }, /* ec */
    { "SBC", ABS, MIN_6502 }, /* ed */
    { "INC", ABS, MIN_6502 }, /* ee */
    { "SBC", ABSL, MIN_65816 }, /* ef */
    { "BEQ", REL, MIN_6502 }, /* f0 */
    { "SBC", INDIR_INDEX, MIN_6502 }, /* f1 */
    { "SBC", ZP_IND, MIN_65C02 }, /* f2 */
    { "SBC", REL_S_Y, MIN_65816 }, /* f3 */
    { "PEA", IMM_S, MIN_65816 }, /* f4 */
    { "SBC", ZP_X, MIN_6502 }, /* f5 */
    { "INC", ZP_X, MIN_6502 }, /* f6 */
    { "SBC", IND_Y_LONG, MIN_65816 }, /* f7 */
    { "SED", IMP, MIN_6502 }, /* f8 */
    { "SBC", ABS_Y, MIN_6502 }, /* f9 */
    { "PLX", IMP, MIN_65C02 }, /* fa */
    { "XCE", IMP, MIN_65816 }, /* fb */
    { "JSR", ABS_IND_X, MIN_65816 }, /* fc */
    { "SBC", ABS_X, MIN_6502 }, /* fd */
    { "INC", ABS_X, MIN_6502 }, /* fe */
    { "SBC", ABSL_X, MIN_65816 }, /* ff */
} ;


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

#include "debugger/trace_opcodes.hpp"

const address_mode_entry address_mode_formats[] = {
   { "???", 1 },
    { "A", 1 },
    { "$%04X", 3 },
    { "$%04X,X", 3 },
    { "$%04X,Y", 3 },
    { "#$%02X", 2 },
    { "", 1 },
    { "($%04X)", 3 },
    { "($%02X,X)", 2 },
    { "($%02X),Y", 2 },
    { "$%02X", 2 },
    { "$%02X", 2 },
    { "$%02X,X", 2 },
    { "$%02X,Y", 2 },
    { "($%04X,X)", 3},
    { "($%02X)", 2}
};

const disasm_entry disasm_table[256] = { 
    { "BRK", IMP, CPU_ALL },  /* 00 */
    { "ORA", INDEX_INDIR, CPU_ALL }, /* 01 */
    { NULL,  NONE, CPU_ALL }, /* 02 */
    { NULL,  NONE, CPU_ALL }, /* 03 */
    { "TSB",  ZP, CPU_65C02 }, /* 04 */
    { "ORA", ZP, CPU_ALL }, /* 05 */
    { "ASL", ZP, CPU_ALL }, /* 06 */
    { NULL,  NONE, CPU_ALL }, /* 07 */
    { "PHP", IMP, CPU_ALL }, /* 08 */
    { "ORA", IMM, CPU_ALL }, /* 09 */
    { "ASL", ACC, CPU_ALL }, /* 0a */
    { NULL,  NONE, CPU_ALL }, /* 0b */
    { "TSB", ABS, CPU_65C02 }, /* 0c */
    { "ORA", ABS, CPU_ALL }, /* 0d */
    { "ASL", ABS, CPU_ALL }, /* 0e */
    { "BBR0", REL, CPU_65C02 }, /* 0f */
    { "BPL", REL, CPU_ALL }, /* 10 */
    { "ORA", INDEX_INDIR, CPU_ALL }, /* 11 */
    { "ORA", ZP_IND, CPU_65C02 }, /* 12 */
    { NULL,  NONE, CPU_ALL }, /* 13 */
    { "TRB",  ZP, CPU_65C02 }, /* 14 */
    { "ORA", ZP_X, CPU_ALL }, /* 15 */
    { "ASL", ZP_X, CPU_ALL }, /* 16 */
    { NULL,  NONE, CPU_ALL }, /* 17 */
    { "CLC", IMP, CPU_ALL }, /* 18 */
    { "ORA", ABS_Y, CPU_ALL }, /* 19 */
    { "INA",  IMP, CPU_65C02 }, /* 1a */
    { NULL,  NONE, CPU_ALL }, /* 1b */
    { "TRB", ABS, CPU_65C02 }, /* 1c */
    { "ORA", ABS_X, CPU_ALL }, /* 1d */
    { "ASL", ABS_X, CPU_ALL }, /* 1e */
    { "BBR1", REL, CPU_65C02 }, /* 1f */
    { "JSR", ABS, CPU_ALL }, /* 20 */
    { "AND", INDEX_INDIR, CPU_ALL }, /* 21 */
    { NULL,  NONE, CPU_ALL }, /* 22 */
    { NULL,  NONE, CPU_ALL }, /* 23 */
    { "BIT", ZP, CPU_ALL }, /* 24 */
    { "AND", ZP, CPU_ALL }, /* 25 */
    { "ROL", ZP , CPU_ALL}, /* 26 */
    { NULL,  NONE, CPU_ALL }, /* 27 */
    { "PLP", IMP, CPU_ALL }, /* 28 */
    { "AND", IMM , CPU_ALL}, /* 29 */
    { "ROL", ACC, CPU_ALL }, /* 2a */
    { NULL,  NONE, CPU_ALL }, /* 2b */
    { "BIT", ABS, CPU_ALL }, /* 2c */
    { "AND", ABS, CPU_ALL }, /* 2d */
    { "ROL", ABS, CPU_ALL }, /* 2e */
    { "BBR2", REL, CPU_65C02 }, /* 2f */
    { "BMI", REL, CPU_ALL }, /* 30 */
    { "AND", INDIR_INDEX, CPU_ALL }, /* 31 */
    { "AND", ZP_IND, CPU_65C02 }, /* 32 */
    { NULL,  NONE, CPU_ALL }, /* 33 */
    { "BIT", ZP_X, CPU_65C02 }, /* 34 */
    { "AND", ZP_X, CPU_ALL }, /* 35 */
    { "ROL", ZP_X, CPU_ALL }, /* 36 */
    { NULL,  NONE, CPU_ALL }, /* 37 */
    { "SEC", IMP, CPU_ALL }, /* 38 */
    { "AND", ABS_Y, CPU_ALL }, /* 39 */
    { "DEA",  IMP, CPU_65C02 }, /* 3a */
    { NULL,  NONE, CPU_ALL }, /* 3b */
    { "BIT", ABS_X, CPU_65C02 }, /* 3c */
    { "AND", ABS_X, CPU_ALL }, /* 3d */
    { "ROL", ABS_X, CPU_ALL }, /* 3e */
    { "BBR3", REL, CPU_65C02 }, /* 3f */
    { "RTI", IMP, CPU_ALL }, /* 40 */
    { "EOR", INDEX_INDIR, CPU_ALL }, /* 41 */
    { NULL,  NONE, CPU_ALL }, /* 42 */
    { NULL,  NONE, CPU_ALL }, /* 43 */
    { NULL,  NONE, CPU_ALL }, /* 44 */
    { "EOR", ZP, CPU_ALL }, /* 45 */
    { "LSR", ZP, CPU_ALL }, /* 46 */
    { NULL,  NONE, CPU_ALL }, /* 47 */
    { "PHA", IMP, CPU_ALL }, /* 48 */
    { "EOR", IMM, CPU_ALL }, /* 49 */
    { "LSR", ACC, CPU_ALL }, /* 4a */
    { NULL,  NONE, CPU_ALL }, /* 4b */
    { "JMP", ABS, CPU_ALL }, /* 4c */
    { "EOR", ABS, CPU_ALL }, /* 4d */
    { "LSR", ABS, CPU_ALL }, /* 4e */
    { "BBR4", REL, CPU_65C02 }, /* 4f */
    { "BVC", REL, CPU_ALL }, /* 50 */
    { "EOR", INDIR_INDEX, CPU_ALL }, /* 51 */
    { "EOR", ZP_IND, CPU_65C02 }, /* 52 */
    { NULL,  NONE, CPU_ALL }, /* 53 */
    { NULL,  NONE, CPU_ALL }, /* 54 */
    { "EOR", ZP_X, CPU_ALL }, /* 55 */
    { "LSR", ZP_X, CPU_ALL }, /* 56 */
    { NULL,  NONE, CPU_ALL }, /* 57 */
    { "CLI", IMP, CPU_ALL }, /* 58 */
    { "EOR", ABS_Y, CPU_ALL }, /* 59 */
    { "PHY", IMP, CPU_65C02 }, /* 5a */
    { NULL,  NONE, CPU_ALL }, /* 5b */
    { NULL,  NONE, CPU_ALL }, /* 5c */
    { "EOR", ABS_X, CPU_ALL }, /* 5d */
    { "LSR", ABS_X , CPU_ALL}, /* 5e */
    { "BBR5", REL, CPU_65C02 }, /* 5f */
    { "RTS", IMP, CPU_ALL }, /* 60 */
    { "ADC", INDEX_INDIR, CPU_ALL }, /* 61 */
    { NULL,  NONE, CPU_ALL }, /* 62 */
    { NULL,  NONE, CPU_ALL }, /* 63 */
    { "STZ", ZP, CPU_65C02 }, /* 64 */
    { "ADC", ZP, CPU_ALL }, /* 65 */
    { "ROR", ZP, CPU_ALL }, /* 66 */
    { NULL,  NONE, CPU_ALL }, /* 67 */
    { "PLA", IMP, CPU_ALL }, /* 68 */
    { "ADC", IMM, CPU_ALL }, /* 69 */
    { "ROR", ACC, CPU_ALL }, /* 6a */
    { NULL,  NONE, CPU_ALL }, /* 6b */
    { "JMP", INDIR, CPU_ALL }, /* 6c */
    { "ADC", ABS, CPU_ALL }, /* 6d */
    { "ROR", ABS, CPU_ALL }, /* 6e */
    { "BBR6", REL, CPU_65C02 }, /* 6f */
    { "BVS", REL, CPU_ALL }, /* 70 */
    { "ADC", INDIR_INDEX, CPU_ALL }, /* 71 */
    { "ADC", ZP_IND, CPU_65C02 }, /* 72 */
    { NULL,  NONE, CPU_ALL }, /* 73 */
    { "STZ", ZP_X, CPU_65C02 }, /* 74 */
    { "ADC", ZP_X, CPU_ALL }, /* 75 */
    { "ROR", ZP_X, CPU_ALL }, /* 76 */
    { NULL, NONE, CPU_ALL }, /* 77 */
    { "SEI", IMP, CPU_ALL }, /* 78 */
    { "ADC", ABS_Y, CPU_ALL }, /* 79 */
    { "PLY", IMP, CPU_65C02 }, /* 7a */
    { NULL, NONE, CPU_ALL }, /* 7b */
    { "JMP", ABS_IND_X, CPU_65C02 }, /* 7c */
    { "ADC", ABS_X, CPU_ALL }, /* 7d */
    { "ROR", ABS_X, CPU_ALL }, /* 7e */
    { "BBR7", REL, CPU_65C02 }, /* 7f */
    { "BRA", REL, CPU_65C02 }, /* 80 */
    { "STA", INDEX_INDIR, CPU_ALL }, /* 81 */
    { NULL, NONE, CPU_ALL }, /* 82 */
    { NULL, NONE, CPU_ALL }, /* 83 */
    { "STY", ZP, CPU_ALL }, /* 84 */
    { "STA", ZP, CPU_ALL }, /* 85 */
    { "STX", ZP, CPU_ALL }, /* 86 */
    { NULL, NONE, CPU_ALL }, /* 87 */
    { "DEY", IMP, CPU_ALL }, /* 88 */
    { "BIT", IMM, CPU_65C02 }, /* 89 */
    { "TXA", IMP, CPU_ALL }, /* 8a */
    { NULL, NONE, CPU_ALL }, /* 8b */
    { "STY", ABS, CPU_ALL }, /* 8c */
    { "STA", ABS, CPU_ALL }, /* 8d */
    { "STX", ABS, CPU_ALL }, /* 8e */
    { "BBS0", REL, CPU_65C02 }, /* 8f */
    { "BCC", REL, CPU_ALL }, /* 90 */
    { "STA", INDIR_INDEX, CPU_ALL }, /* 91 */
    { "STA", ZP_IND, CPU_65C02 }, /* 92 */
    { NULL, NONE, CPU_ALL }, /* 93 */
    { "STY", ZP_X, CPU_ALL }, /* 94 */
    { "STA", ZP_X, CPU_ALL }, /* 95 */
    { "STX", ZP_Y, CPU_ALL }, /* 96 */
    { NULL, NONE, CPU_ALL }, /* 97 */
    { "TYA", IMP, CPU_ALL }, /* 98 */
    { "STA", ABS_Y, CPU_ALL }, /* 99 */
    { "TXS", IMP, CPU_ALL }, /* 9a */
    { NULL, NONE, CPU_ALL }, /* 9b */
    { "STZ", ABS, CPU_65C02 }, /* 9c */
    { "STA", ABS_X, CPU_ALL }, /* 9d */
    { "STZ", ABS_X, CPU_65C02 }, /* 9e */
    { "BBS1", REL, CPU_65C02 }, /* 9f */
    { "LDY", IMM, CPU_ALL }, /* a0 */
    { "LDA", INDEX_INDIR, CPU_ALL }, /* a1 */
    { "LDX", IMM, CPU_ALL }, /* a2 */
    { NULL, NONE, CPU_ALL }, /* a3 */
    { "LDY", ZP, CPU_ALL }, /* a4 */
    { "LDA", ZP, CPU_ALL }, /* a5 */
    { "LDX", ZP, CPU_ALL }, /* a6 */
    { NULL, NONE, CPU_ALL }, /* a7 */
    { "TAY", IMP, CPU_ALL }, /* a8 */
    { "LDA", IMM, CPU_ALL }, /* a9 */
    { "TAX", IMP, CPU_ALL }, /* aa */
    { NULL, NONE, CPU_ALL }, /* ab */
    { "LDY", ABS, CPU_ALL }, /* ac */
    { "LDA", ABS, CPU_ALL }, /* ad */
    { "LDX", ABS, CPU_ALL }, /* ae */
    { "BBS2", REL, CPU_65C02 }, /* af */
    { "BCS", REL, CPU_ALL }, /* b0 */
    { "LDA", INDIR_INDEX, CPU_ALL }, /* b1 */
    { "LDA", ZP_IND, CPU_65C02 }, /* b2 */
    { NULL, NONE, CPU_ALL }, /* b3 */
    { "LDY", ZP_X, CPU_ALL }, /* b4 */
    { "LDA", ZP_X, CPU_ALL }, /* b5 */
    { "LDX", ZP_Y, CPU_ALL }, /* b6 */
    { NULL, NONE, CPU_ALL }, /* b7 */
    { "CLV", IMP, CPU_ALL }, /* b8 */
    { "LDA", ABS_Y, CPU_ALL }, /* b9 */
    { "TSX", IMP, CPU_ALL }, /* ba */
    { NULL, NONE, CPU_ALL }, /* bb */
    { "LDY", ABS_X, CPU_ALL }, /* bc */
    { "LDA", ABS_X, CPU_ALL }, /* bd */
    { "LDX", ABS_Y, CPU_ALL }, /* be */
    { "BBS3", REL, CPU_65C02 }, /* bf */
    { "CPY", IMM, CPU_ALL }, /* c0 */
    { "CMP", INDEX_INDIR, CPU_ALL }, /* c1 */
    { NULL, NONE, CPU_ALL }, /* c2 */
    { NULL, NONE, CPU_ALL }, /* c3 */
    { "CPY", ZP, CPU_ALL }, /* c4 */
    { "CMP", ZP, CPU_ALL }, /* c5 */
    { "DEC", ZP, CPU_ALL }, /* c6 */
    { NULL, NONE, CPU_ALL }, /* c7 */
    { "INY", IMP, CPU_ALL }, /* c8 */
    { "CMP", IMM, CPU_ALL }, /* c9 */
    { "DEX", IMP, CPU_ALL }, /* ca */
    { NULL, NONE, CPU_ALL }, /* cb */
    { "CPY", ABS, CPU_ALL }, /* cc */
    { "CMP", ABS, CPU_ALL }, /* cd */
    { "DEC", ABS, CPU_ALL }, /* ce */
    { "BBS4", REL, CPU_65C02 }, /* cf */
    { "BNE", REL, CPU_ALL }, /* d0 */
    { "CMP", INDIR_INDEX, CPU_ALL }, /* d1 */
    { "CMP", ZP_IND, CPU_65C02 }, /* d2 */
    { NULL, NONE, CPU_ALL }, /* d3 */
    { NULL, NONE, CPU_ALL }, /* d4 */
    { "CMP", ZP_X, CPU_ALL }, /* d5 */
    { "DEC", ZP_X, CPU_ALL }, /* d6 */
    { NULL, NONE, CPU_ALL }, /* d7 */
    { "CLD", IMP, CPU_ALL }, /* d8 */
    { "CMP", ABS_Y, CPU_ALL }, /* d9 */
    { "PHX", IMP, CPU_65C02 }, /* da */
    { NULL, NONE, CPU_ALL }, /* db */
    { NULL, NONE, CPU_ALL }, /* dc */
    { "CMP", ABS_X, CPU_ALL }, /* dd */
    { "DEC", ABS_X, CPU_ALL }, /* de */
    { "BBS5", REL, CPU_65C02 }, /* df */
    { "CPX", IMM, CPU_ALL }, /* e0 */
    { "SBC", INDEX_INDIR, CPU_ALL }, /* e1 */
    { NULL, NONE, CPU_ALL }, /* e2 */
    { NULL, NONE, CPU_ALL }, /* e3 */
    { "CPX", ZP, CPU_ALL }, /* e4 */
    { "SBC", ZP, CPU_ALL }, /* e5 */
    { "INC", ZP, CPU_ALL }, /* e6 */
    { NULL, NONE, CPU_ALL }, /* e7 */
    { "INX", IMP, CPU_ALL }, /* e8 */
    { "SBC", IMM, CPU_ALL }, /* e9 */
    { "NOP", IMP, CPU_ALL }, /* ea */
    { NULL, NONE, CPU_ALL }, /* eb */
    { "CPX", ABS, CPU_ALL }, /* ec */
    { "SBC", ABS, CPU_ALL }, /* ed */
    { "INC", ABS, CPU_ALL }, /* ee */
    { "BBS6", REL, CPU_65C02 }, /* ef */
    { "BEQ", REL, CPU_ALL }, /* f0 */
    { "SBC", INDIR_INDEX, CPU_ALL }, /* f1 */
    { "SBC", ZP_IND, CPU_65C02 }, /* f2 */
    { NULL, NONE, CPU_ALL }, /* f3 */
    { NULL, NONE, CPU_ALL }, /* f4 */
    { "SBC", ZP_X, CPU_ALL }, /* f5 */
    { "INC", ZP_X, CPU_ALL }, /* f6 */
    { NULL, NONE, CPU_ALL }, /* f7 */
    { "SED", IMP, CPU_ALL }, /* f8 */
    { "SBC", ABS_Y, CPU_ALL }, /* f9 */
    { "PLX", IMP, CPU_65C02 }, /* fa */
    { NULL, NONE, CPU_ALL }, /* fb */
    { NULL, NONE, CPU_ALL }, /* fc */
    { "SBC", ABS_X, CPU_ALL }, /* fd */
    { "INC", ABS_X, CPU_ALL }, /* fe */
    { "BBS7", REL, CPU_65C02 }, /* ff */
} ;


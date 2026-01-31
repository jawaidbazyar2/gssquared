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

#include <iostream>
#include <unistd.h>

#include "gs2.hpp"
#include "opcodes.hpp"

const char * opcode_names[256] = { 
    "BRK", /* 00 */
    "ORA", /* 01 */
    NULL, /* 02 */
    NULL, /* 03 */
    NULL, /* 04 */
    "ORA", /* 05 */
    "ASL", /* 06 */
    NULL, /* 07 */
    "PHP", /* 08 */
    "ORA", /* 09 */
    "ASL", /* 0a */
    NULL, /* 0b */
    NULL, /* 0c */
    "ORA", /* 0d */
    "ASL", /* 0e */
    NULL, /* 0f */
    "BPL", /* 10 */
    "ORA", /* 11 */
    NULL, /* 12 */
    NULL, /* 13 */
    NULL, /* 14 */
    "ORA", /* 15 */
    "ASL", /* 16 */
    NULL, /* 17 */
    "CLC", /* 18 */
    "ORA", /* 19 */
    NULL, /* 1a */
    NULL, /* 1b */
    NULL, /* 1c */
    "ORA", /* 1d */
    "ASL", /* 1e */
    NULL, /* 1f */
    "JSR", /* 20 */
    "AND", /* 21 */
    NULL, /* 22 */
    NULL, /* 23 */
    "BIT", /* 24 */
    "AND", /* 25 */
    "ROL", /* 26 */
    NULL, /* 27 */
    "PLP", /* 28 */
    "AND", /* 29 */
    "ROL", /* 2a */
    NULL, /* 2b */
    "BIT", /* 2c */
    "AND", /* 2d */
    "ROL", /* 2e */
    NULL, /* 2f */
    "BMI", /* 30 */
    "AND", /* 31 */
    NULL, /* 32 */
    NULL, /* 33 */
    NULL, /* 34 */
    "AND", /* 35 */
    "ROL", /* 36 */
    NULL, /* 37 */
    "SEC", /* 38 */
    "AND", /* 39 */
    NULL, /* 3a */
    NULL, /* 3b */
    NULL, /* 3c */
    "AND", /* 3d */
    "ROL", /* 3e */
    NULL, /* 3f */
    "RTI", /* 40 */
    "EOR", /* 41 */
    NULL, /* 42 */
    NULL, /* 43 */
    NULL, /* 44 */
    "EOR", /* 45 */
    "LSR", /* 46 */
    NULL, /* 47 */
    "PHA", /* 48 */
    "EOR", /* 49 */
    "LSR", /* 4a */
    NULL, /* 4b */
    "JMP", /* 4c */
    "EOR", /* 4d */
    "LSR", /* 4e */
    NULL, /* 4f */
    "BVC", /* 50 */
    "EOR", /* 51 */
    NULL, /* 52 */
    NULL, /* 53 */
    NULL, /* 54 */
    "EOR", /* 55 */
    "LSR", /* 56 */
    NULL, /* 57 */
    "CLI", /* 58 */
    "EOR", /* 59 */
    NULL, /* 5a */
    NULL, /* 5b */
    NULL, /* 5c */
    "EOR", /* 5d */
    "LSR", /* 5e */
    NULL, /* 5f */
    "RTS", /* 60 */
    "ADC", /* 61 */
    NULL, /* 62 */
    NULL, /* 63 */
    NULL, /* 64 */
    "ADC", /* 65 */
    "ROR", /* 66 */
    NULL, /* 67 */
    "PLA", /* 68 */
    "ADC", /* 69 */
    NULL, /* 6a */
    NULL, /* 6b */
    "JMP", /* 6c */
    "ADC", /* 6d */
    NULL, /* 6e */
    NULL, /* 6f */
    "BVS", /* 70 */
    "ADC", /* 71 */
    NULL, /* 72 */
    NULL, /* 73 */
    NULL, /* 74 */
    "ADC", /* 75 */
    NULL, /* 76 */
    NULL, /* 77 */
    "SEI", /* 78 */
    "ADC", /* 79 */
    NULL, /* 7a */
    NULL, /* 7b */
    NULL, /* 7c */
    "ADC", /* 7d */
    NULL, /* 7e */
    NULL, /* 7f */
    NULL, /* 80 */
    "STA", /* 81 */
    NULL, /* 82 */
    NULL, /* 83 */
    "STY", /* 84 */
    "STA", /* 85 */
    "STX", /* 86 */
    NULL, /* 87 */
    "DEY", /* 88 */
    NULL, /* 89 */
    "TXA", /* 8a */
    NULL, /* 8b */
    "STY", /* 8c */
    "STA", /* 8d */
    "STX", /* 8e */
    NULL, /* 8f */
    "BCC", /* 90 */
    "STA", /* 91 */
    NULL, /* 92 */
    NULL, /* 93 */
    "STY", /* 94 */
    "STA", /* 95 */
    "STX", /* 96 */
    NULL, /* 97 */
    "TYA", /* 98 */
    "STA", /* 99 */
    "TXS", /* 9a */
    NULL, /* 9b */
    NULL, /* 9c */
    "STA", /* 9d */
    NULL, /* 9e */
    NULL, /* 9f */
    "LDY", /* a0 */
    "LDA", /* a1 */
    "LDX", /* a2 */
    NULL, /* a3 */
    "LDY", /* a4 */
    "LDA", /* a5 */
    "LDX", /* a6 */
    NULL, /* a7 */
    "TAY", /* a8 */
    "LDA", /* a9 */
    "TAX", /* aa */
    NULL, /* ab */
    "LDY", /* ac */
    "LDA", /* ad */
    "LDX", /* ae */
    NULL, /* af */
    "BCS", /* b0 */
    "LDA", /* b1 */
    NULL, /* b2 */
    NULL, /* b3 */
    "LDY", /* b4 */
    "LDA", /* b5 */
    "LDX", /* b6 */
    NULL, /* b7 */
    "CLV", /* b8 */
    "LDA", /* b9 */
    "TSX", /* ba */
    NULL, /* bb */
    "LDY", /* bc */
    "LDA", /* bd */
    "LDX", /* be */
    NULL, /* bf */
    "CPY", /* c0 */
    "CMP", /* c1 */
    NULL, /* c2 */
    NULL, /* c3 */
    "CPY", /* c4 */
    "CMP", /* c5 */
    "DEC", /* c6 */
    NULL, /* c7 */
    "INY", /* c8 */
    "CMP", /* c9 */
    "DEX", /* ca */
    NULL, /* cb */
    "CPY", /* cc */
    "CMP", /* cd */
    "DEC", /* ce */
    NULL, /* cf */
    "BNE", /* d0 */
    "CMP", /* d1 */
    NULL, /* d2 */
    NULL, /* d3 */
    NULL, /* d4 */
    "CMP", /* d5 */
    "DEC", /* d6 */
    NULL, /* d7 */
    "CLD", /* d8 */
    "CMP", /* d9 */
    NULL, /* da */
    NULL, /* db */
    NULL, /* dc */
    "CMP", /* dd */
    "DEC", /* de */
    NULL, /* df */
    "CPX", /* e0 */
    "SBC", /* e1 */
    NULL, /* e2 */
    NULL, /* e3 */
    "CPX", /* e4 */
    "SBC", /* e5 */
    "INC", /* e6 */
    NULL, /* e7 */
    "INX", /* e8 */
    "SBC", /* e9 */
    "NOP", /* ea */
    NULL, /* eb */
    "CPX", /* ec */
    "SBC", /* ed */
    "INC", /* ee */
    NULL, /* ef */
    "BEQ", /* f0 */
    "SBC", /* f1 */
    NULL, /* f2 */
    NULL, /* f3 */
    NULL, /* f4 */
    "SBC", /* f5 */
    "INC", /* f6 */
    NULL, /* f7 */
    "SED", /* f8 */
    "SBC", /* f9 */
    NULL, /* fa */
    NULL, /* fb */
    NULL, /* fc */
    "SBC", /* fd */
    "INC", /* fe */
    "HLT" /* ff */
} ;

const char *get_opcode_name(uint8_t opcode) {
    const char *opcn = opcode_names[opcode];

    if (opcn == NULL) {
        return "???";
    }
    return opcn;
}
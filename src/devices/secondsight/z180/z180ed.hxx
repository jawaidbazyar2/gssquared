// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   ED-prefixed extended instructions for Second Sight's Zilog Z8S180.
 *   Z180.cpp includes this file; its decoder calls OP(family, hex-byte),
 *   which expands to a void member with no C++ parameters. Operands come
 *   from CPU registers or instruction bytes at PC. Results change those
 *   registers, flags or board memory; the handler returns no C++ value.
 *   Z180 owns the state and only its execution thread calls these functions.
 *   Memory and port helpers pass through the bus scheduler before SSCard.
 *
 *   A is the accumulator; F holds arithmetic flags. B/C, D/E and H/L form
 *   16-bit pairs BC, DE and HL. IX/IY are index registers, SP the stack pointer,
 *   and PC the next instruction address. HX/LX and HY/LY mean the high/low
 *   bytes of IX and IY. I supplies the interrupt-vector high byte; R counts
 *   opcode fetches. A prime denotes the alternate register set. In mnemonic
 *   notation, parentheses mean memory or a port; n/m are immediate bytes,
 *   w/a an immediate word/address, and o a signed 8-bit displacement. XY means
 *   the IX or IY selected by the prefix; ea holds its computed address.
 *
 *   Flags record sign, zero, half-carry (carry across bit 3), parity/overflow,
 *   subtraction and carry. PHI is the processor clock; a T-state is one PHI.
 *   The decoder supplies base timing; helpers add conditional/repeat timing.
 *   Empty self-load handlers still consume clocks. A trap is the processor's
 *   entry to address zero for an undefined instruction. The decoder traps
 *   every encoding the Z180 rejects before a handler runs, so this file has
 *   handlers only for the encodings it accepts.
 *
 *   NMI is the non-maskable interrupt; INT0 is the first maskable input.
 *   M1 identifies an opcode fetch. IOCR controls internal I/O and CCR the
 *   processor clock. An interrupt-enable flag decides whether a request
 *   may be accepted between instructions.
 *
 *   Sources: Zilog UM005004-0918 pp. 68-72 for traps, 173-183 for registers,
 *   pp. 211-236 for operations, pp. 247-250 for valid encodings, and
 *   pp. 251-280 for bus ordering. Page citations below use printed pages.
 */


/**********************************************************
 * special opcodes (ED prefix)
 **********************************************************/
// IN0 affects SZHPN exactly like IN g,(C), preserving carry (UM0050 p231).
// Read the immediate 8-bit port into B. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 00) {
    unsigned n = ARG();
    b8 = IN(n);
    f8 = (f8 & CF) | SZP[b8];
}
// Write B to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 01) {
    unsigned n = ARG();
    OUT(n, b8);
}
// Set flags from A AND B without changing either operand. (UM0050 p. 214).
OP(ed, 04) {
    TST(b8);
}

// Read the immediate 8-bit port into C. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 08) {
    unsigned n = ARG();
    c8 = IN(n);
    f8 = (f8 & CF) | SZP[c8];
}
// Write C to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 09) {
    unsigned n = ARG();
    OUT(n, c8);
}
// Set flags from A AND C without changing either operand. (UM0050 p. 214).
OP(ed, 0c) {
    TST(c8);
}

// Read the immediate 8-bit port into D. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 10) {
    unsigned n = ARG();
    d8 = IN(n);
    f8 = (f8 & CF) | SZP[d8];
}
// Write D to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 11) {
    unsigned n = ARG();
    OUT(n, d8);
}
// Set flags from A AND D without changing either operand. (UM0050 p. 214).
OP(ed, 14) {
    TST(d8);
}

// Read the immediate 8-bit port into E. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 18) {
    unsigned n = ARG();
    e8 = IN(n);
    f8 = (f8 & CF) | SZP[e8];
}
// Write E to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 19) {
    unsigned n = ARG();
    OUT(n, e8);
}
// Set flags from A AND E without changing either operand. (UM0050 p. 214).
OP(ed, 1c) {
    TST(e8);
}

// Read the immediate 8-bit port into H. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 20) {
    unsigned n = ARG();
    h8 = IN(n);
    f8 = (f8 & CF) | SZP[h8];
}
// Write H to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 21) {
    unsigned n = ARG();
    OUT(n, h8);
}
// Set flags from A AND H without changing either operand. (UM0050 p. 214).
OP(ed, 24) {
    TST(h8);
}

// Read the immediate 8-bit port into L. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 28) {
    unsigned n = ARG();
    l8 = IN(n);
    f8 = (f8 & CF) | SZP[l8];
}
// Write L to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 29) {
    unsigned n = ARG();
    OUT(n, l8);
}
// Set flags from A AND L without changing either operand. (UM0050 p. 214).
OP(ed, 2c) {
    TST(l8);
}

// Read the immediate 8-bit port; discard the byte but update flags. (UM0050 pp. 231-234).
OP(ed, 30) {
    unsigned n = ARG();
    uint8_t res = IN(n);
    f8 = (f8 & CF) | SZP[res];
}
// Set flags from A AND memory at HL without changing either operand. (UM0050 p. 214).
OP(ed, 34) {
    TST(RM(hl16));
}

// Read the immediate 8-bit port into A. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 38) {
    unsigned n = ARG();
    a8 = IN(n);
    f8 = (f8 & CF) | SZP[a8];
}
// Write A to the immediate 8-bit port. (UM0050 pp. 231-234).
OP(ed, 39) {
    unsigned n = ARG();
    OUT(n, a8);
}
// Set flags from A AND A without changing either operand. (UM0050 p. 214).
OP(ed, 3c) {
    TST(a8);
}

// Read the 16-bit port BC into B. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 40) {
    b8 = IN(bc16);
    f8 = (f8 & CF) | SZP[b8];
}
// Write B to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 41) {
    OUT(bc16, b8);
}
// Subtract BC and carry from HL. Store the wrapped result and update 16-bit arithmetic flags.
// (UM0050 p. 221).
OP(ed, 42) {
    SBC16(BC);
}
// Copy BC to memory at the encoded address. (UM0050 pp. 222-224).
OP(ed, 43) {
    ea = ARG16();
    WM16(ea, &BC);
}
// Replace A with zero minus A and set subtraction flags. (UM0050 p. 215).
OP(ed, 44) {
    NEG;
}
// Pop the return PC and restore the interrupt enable saved on NMI entry. (UM0050 pp. 73-75,
// 230).
OP(ed, 45) {
    RETN;
}
// Select INT0 interrupt mode 0. Other interrupt sources retain their fixed vector rules.
// (UM0050 pp. 75-86, 235).
OP(ed, 46) {
    IM = 0;
}
// Copy A to I. (UM0050 pp. 222-224).
OP(ed, 47) {
    LD_I_A;
}

// Read the 16-bit port BC into C. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 48) {
    c8 = IN(bc16);
    f8 = (f8 & CF) | SZP[c8];
}
// Write C to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 49) {
    OUT(bc16, c8);
}
// Add BC and carry to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050
// p. 221).
OP(ed, 4a) {
    ADC16(BC);
}
// Copy memory at the encoded address to BC. (UM0050 pp. 222-224).
OP(ed, 4b) {
    ea = ARG16();
    RM16(ea, &BC);
}
// Multiply the low and high bytes of BC as unsigned values; store their 16-bit product in that
// pair. (UM0050 p. 221).
OP(ed, 4c) {
    MLT(BC);
}
// Pop the return PC and signal return-from-interrupt to the board; M1 mode may add refetch
// cycles. (UM0050 pp. 84-85, 230).
OP(ed, 4d) {
    RETI;
}
// Copy A to R. (UM0050 pp. 222-224).
OP(ed, 4f) {
    LD_R_A;
}

// Read the 16-bit port BC into D. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 50) {
    d8 = IN(bc16);
    f8 = (f8 & CF) | SZP[d8];
}
// Write D to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 51) {
    OUT(bc16, d8);
}
// Subtract DE and carry from HL. Store the wrapped result and update 16-bit arithmetic flags.
// (UM0050 p. 221).
OP(ed, 52) {
    SBC16(DE);
}
// Copy DE to memory at the encoded address. (UM0050 pp. 222-224).
OP(ed, 53) {
    ea = ARG16();
    WM16(ea, &DE);
}
// Select INT0 interrupt mode 1. Other interrupt sources retain their fixed vector rules.
// (UM0050 pp. 75-86, 235).
OP(ed, 56) {
    IM = 1;
}
// Copy I to A. Set parity/overflow from the saved interrupt enable. (UM0050 pp. 222-224).
OP(ed, 57) {
    LD_A_I;
}

// Read the 16-bit port BC into E. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 58) {
    e8 = IN(bc16);
    f8 = (f8 & CF) | SZP[e8];
}
// Write E to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 59) {
    OUT(bc16, e8);
}
// Add DE and carry to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050
// p. 221).
OP(ed, 5a) {
    ADC16(DE);
}
// Copy memory at the encoded address to DE. (UM0050 pp. 222-224).
OP(ed, 5b) {
    ea = ARG16();
    RM16(ea, &DE);
}
// Multiply the low and high bytes of DE as unsigned values; store their 16-bit product in that
// pair. (UM0050 p. 221).
OP(ed, 5c) {
    MLT(DE);
}
// Select INT0 interrupt mode 2. Other interrupt sources retain their fixed vector rules.
// (UM0050 pp. 75-86, 235).
OP(ed, 5e) {
    IM = 2;
}
// Copy R to A. Set parity/overflow from the saved interrupt enable. (UM0050 pp. 222-224).
OP(ed, 5f) {
    LD_A_R;
}

// Read the 16-bit port BC into H. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 60) {
    h8 = IN(bc16);
    f8 = (f8 & CF) | SZP[h8];
}
// Write H to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 61) {
    OUT(bc16, h8);
}
// Subtract HL and carry from HL. Store the wrapped result and update 16-bit arithmetic flags.
// (UM0050 p. 221).
OP(ed, 62) {
    SBC16(HL);
}
// Copy HL to memory at the encoded address. (UM0050 pp. 222-224).
OP(ed, 63) {
    ea = ARG16();
    WM16(ea, &HL);
}
// Set flags from A AND the next immediate mask without changing either operand. (UM0050 p.
// 214).
OP(ed, 64) {
    unsigned m = ARG();
    TST(m);
}
// Rotate four-bit digits right between A's low digit and memory at HL; preserve carry. (UM0050
// p. 218).
OP(ed, 67) {
    RRD;
}

// Read the 16-bit port BC into L. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 68) {
    l8 = IN(bc16);
    f8 = (f8 & CF) | SZP[l8];
}
// Write L to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 69) {
    OUT(bc16, l8);
}
// Add HL and carry to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050
// p. 221).
OP(ed, 6a) {
    ADC16(HL);
}
// Copy memory at the encoded address to HL. (UM0050 pp. 222-224).
OP(ed, 6b) {
    ea = ARG16();
    RM16(ea, &HL);
}
// Multiply the low and high bytes of HL as unsigned values; store their 16-bit product in that
// pair. (UM0050 p. 221).
OP(ed, 6c) {
    MLT(HL);
}
// Rotate four-bit digits left between A's low digit and memory at HL; preserve carry. (UM0050
// p. 218).
OP(ed, 6f) {
    RLD;
}

// Read the 16-bit port BC; discard the byte but update flags. (UM0050 pp. 231-234).
OP(ed, 70) {
    uint8_t res = IN(bc16);
    f8 = (f8 & CF) | SZP[res];
}
// Subtract SP and carry from HL. Store the wrapped result and update 16-bit arithmetic flags.
// (UM0050 p. 221).
OP(ed, 72) {
    SBC16(SP);
}
// Copy SP to memory at the encoded address. (UM0050 pp. 222-224).
OP(ed, 73) {
    ea = ARG16();
    WM16(ea, &SP);
}
// UM0050 p233: TSTIO is a test/AND, not IN; H is set and C is cleared.
// Read port C, AND it with the immediate mask, and set flags without retaining the byte.
// (UM0050 p. 234).
OP(ed, 74) {
    unsigned m = ARG();
    f8 = HF | SZP[IN(c8) & m];
}
// Enter SLP; clock stopping and wake delay depend on IOCR/CCR and silicon revision. (UM0050 pp.
// 34-41, 235).
OP(ed, 76) {
    SLP;
}

// Read the 16-bit port BC into A. Set sign/zero/parity and preserve carry. (UM0050 pp.
// 231-234).
OP(ed, 78) {
    a8 = IN(bc16);
    f8 = (f8 & CF) | SZP[a8];
}
// Write A to the 16-bit port BC. (UM0050 pp. 231-234).
OP(ed, 79) {
    OUT(bc16, a8);
}
// Add SP and carry to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050
// p. 221).
OP(ed, 7a) {
    ADC16(SP);
}
// Copy memory at the encoded address to SP. (UM0050 pp. 222-224).
OP(ed, 7b) {
    ea = ARG16();
    RM16(ea, &SP);
}
// Multiply the low and high bytes of SP as unsigned values; store their 16-bit product in that
// pair. (UM0050 p. 221).
OP(ed, 7c) {
    MLT(SP);
}

// Write memory at HL to port C; decrement B and increment HL. Move C in the same direction.
// (UM0050 pp. 232-234).
OP(ed, 83) {
    OTIM;
}

// Write memory at HL to port C; decrement B and decrement HL. Move C in the same direction.
// (UM0050 pp. 232-234).
OP(ed, 8b) {
    OTDM;
}

// Write memory at HL to port C; decrement B and increment HL. Move C in the same direction.
// Repeat until B reaches zero; PC backs up so each byte remains interruptible. (UM0050 pp.
// 232-234).
OP(ed, 93) {
    OTIMR;
}

// Write memory at HL to port C; decrement B and decrement HL. Move C in the same direction.
// Repeat until B reaches zero; PC backs up so each byte remains interruptible. (UM0050 pp.
// 232-234).
OP(ed, 9b) {
    OTDMR;
}

// Copy one byte from HL to DE; increment both addresses and decrement BC. (UM0050 p. 225).
OP(ed, a0) {
    LDI;
}
// Compare A with memory at HL; increment HL and decrement BC. (UM0050 p. 226).
OP(ed, a1) {
    CPI;
}
// Read port BC into memory at HL; decrement B and increment HL. (UM0050 pp. 232-234).
OP(ed, a2) {
    INI;
}
// Read memory at HL, decrement B, then write to port BC and increment HL. (UM0050 pp. 232-234).
OP(ed, a3) {
    OUTI;
}

// Copy one byte from HL to DE; decrement both addresses and decrement BC. (UM0050 p. 225).
OP(ed, a8) {
    LDD;
}
// Compare A with memory at HL; decrement HL and decrement BC. (UM0050 p. 226).
OP(ed, a9) {
    CPD;
}
// Read port BC into memory at HL; decrement B and decrement HL. (UM0050 pp. 232-234).
OP(ed, aa) {
    IND;
}
// Read memory at HL, decrement B, then write to port BC and decrement HL. (UM0050 pp. 232-234).
OP(ed, ab) {
    OUTD;
}

// Copy one byte from HL to DE; increment both addresses and decrement BC. Repeat by backing up
// PC while BC is nonzero, allowing interrupts between bytes. (UM0050 p. 225).
OP(ed, b0) {
    LDIR;
}
// Compare A with memory at HL; increment HL and decrement BC. Repeat until a match or BC
// reaches zero. (UM0050 p. 226).
OP(ed, b1) {
    CPIR;
}
// Read port BC into memory at HL; decrement B and increment HL. Repeat until B reaches zero; PC
// backs up so each byte remains interruptible. (UM0050 pp. 232-234).
OP(ed, b2) {
    INIR;
}
// Read memory at HL, decrement B, then write to port BC and increment HL. Repeat until B reaches zero; PC
// backs up so each byte remains interruptible. (UM0050 pp. 232-234).
OP(ed, b3) {
    OTIR;
}

// Copy one byte from HL to DE; decrement both addresses and decrement BC. Repeat by backing up
// PC while BC is nonzero, allowing interrupts between bytes. (UM0050 p. 225).
OP(ed, b8) {
    LDDR;
}
// Compare A with memory at HL; decrement HL and decrement BC. Repeat until a match or BC
// reaches zero. (UM0050 p. 226).
OP(ed, b9) {
    CPDR;
}
// Read port BC into memory at HL; decrement B and decrement HL. Repeat until B reaches zero; PC
// backs up so each byte remains interruptible. (UM0050 pp. 232-234).
OP(ed, ba) {
    INDR;
}
// Read memory at HL, decrement B, then write to port BC and decrement HL. Repeat until B reaches zero; PC
// backs up so each byte remains interruptible. (UM0050 pp. 232-234).
OP(ed, bb) {
    OTDR;
}









// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   CB-prefixed bit, rotate and shift instructions for Second Sight's Zilog Z8S180.
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
 *   Sources: Zilog UM005004-0918 pp. 173-183 for registers/addressing,
 *   pp. 211-236 for operations, pp. 247-250 for valid encodings, and
 *   pp. 251-280 for bus ordering. Page citations below use printed pages.
 */

/**********************************************************
 * opcodes with CB prefix
 * rotate, shift and bit operations
 **********************************************************/
// Rotate B left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 00) {
    b8 = RLC(b8);
}
// Rotate C left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 01) {
    c8 = RLC(c8);
}
// Rotate D left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 02) {
    d8 = RLC(d8);
}
// Rotate E left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 03) {
    e8 = RLC(e8);
}
// Rotate H left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 04) {
    h8 = RLC(h8);
}
// Rotate L left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 05) {
    l8 = RLC(l8);
}
// Rotate memory at HL left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and
// flags from the result. (UM0050 pp. 216-220).
OP(cb, 06) {
    WM(hl16, RLC(RM(hl16)));
}
// Rotate A left, wrapping bit 7 into bit 0. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 07) {
    a8 = RLC(a8);
}

// Rotate B right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 08) {
    b8 = RRC(b8);
}
// Rotate C right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 09) {
    c8 = RRC(c8);
}
// Rotate D right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 0a) {
    d8 = RRC(d8);
}
// Rotate E right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 0b) {
    e8 = RRC(e8);
}
// Rotate H right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 0c) {
    h8 = RRC(h8);
}
// Rotate L right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 0d) {
    l8 = RRC(l8);
}
// Rotate memory at HL right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and
// flags from the result. (UM0050 pp. 216-220).
OP(cb, 0e) {
    WM(hl16, RRC(RM(hl16)));
}
// Rotate A right, wrapping bit 0 into bit 7. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 0f) {
    a8 = RRC(a8);
}

// Rotate B left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 10) {
    b8 = RL(b8);
}
// Rotate C left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 11) {
    c8 = RL(c8);
}
// Rotate D left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 12) {
    d8 = RL(d8);
}
// Rotate E left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 13) {
    e8 = RL(e8);
}
// Rotate H left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 14) {
    h8 = RL(h8);
}
// Rotate L left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 15) {
    l8 = RL(l8);
}
// Rotate memory at HL left through carry. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 16) {
    WM(hl16, RL(RM(hl16)));
}
// Rotate A left through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 17) {
    a8 = RL(a8);
}

// Rotate B right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 18) {
    b8 = RR(b8);
}
// Rotate C right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 19) {
    c8 = RR(c8);
}
// Rotate D right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 1a) {
    d8 = RR(d8);
}
// Rotate E right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 1b) {
    e8 = RR(e8);
}
// Rotate H right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 1c) {
    h8 = RR(h8);
}
// Rotate L right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 1d) {
    l8 = RR(l8);
}
// Rotate memory at HL right through carry. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 1e) {
    WM(hl16, RR(RM(hl16)));
}
// Rotate A right through carry. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 1f) {
    a8 = RR(a8);
}

// Shift B left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 20) {
    b8 = SLA(b8);
}
// Shift C left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 21) {
    c8 = SLA(c8);
}
// Shift D left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 22) {
    d8 = SLA(d8);
}
// Shift E left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 23) {
    e8 = SLA(e8);
}
// Shift H left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 24) {
    h8 = SLA(h8);
}
// Shift L left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 25) {
    l8 = SLA(l8);
}
// Shift memory at HL left, inserting zero. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 26) {
    WM(hl16, SLA(RM(hl16)));
}
// Shift A left, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 27) {
    a8 = SLA(a8);
}

// Shift B right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 28) {
    b8 = SRA(b8);
}
// Shift C right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 29) {
    c8 = SRA(c8);
}
// Shift D right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 2a) {
    d8 = SRA(d8);
}
// Shift E right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 2b) {
    e8 = SRA(e8);
}
// Shift H right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 2c) {
    h8 = SRA(h8);
}
// Shift L right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 2d) {
    l8 = SRA(l8);
}
// Shift memory at HL right, retaining its sign bit. Set carry from the outgoing bit and flags
// from the result. (UM0050 pp. 216-220).
OP(cb, 2e) {
    WM(hl16, SRA(RM(hl16)));
}
// Shift A right, retaining its sign bit. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 2f) {
    a8 = SRA(a8);
}


// Shift B right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 38) {
    b8 = SRL(b8);
}
// Shift C right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 39) {
    c8 = SRL(c8);
}
// Shift D right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 3a) {
    d8 = SRL(d8);
}
// Shift E right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 3b) {
    e8 = SRL(e8);
}
// Shift H right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 3c) {
    h8 = SRL(h8);
}
// Shift L right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 3d) {
    l8 = SRL(l8);
}
// Shift memory at HL right, inserting zero. Set carry from the outgoing bit and flags from the
// result. (UM0050 pp. 216-220).
OP(cb, 3e) {
    WM(hl16, SRL(RM(hl16)));
}
// Shift A right, inserting zero. Set carry from the outgoing bit and flags from the result.
// (UM0050 pp. 216-220).
OP(cb, 3f) {
    a8 = SRL(a8);
}

// Test bit 0 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 40) {
    BIT_TEST(0, b8);
}
// Test bit 0 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 41) {
    BIT_TEST(0, c8);
}
// Test bit 0 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 42) {
    BIT_TEST(0, d8);
}
// Test bit 0 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 43) {
    BIT_TEST(0, e8);
}
// Test bit 0 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 44) {
    BIT_TEST(0, h8);
}
// Test bit 0 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 45) {
    BIT_TEST(0, l8);
}
// Test bit 0 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 46) {
    BIT_TEST(0, RM(hl16));
}
// Test bit 0 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 47) {
    BIT_TEST(0, a8);
}

// Test bit 1 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 48) {
    BIT_TEST(1, b8);
}
// Test bit 1 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 49) {
    BIT_TEST(1, c8);
}
// Test bit 1 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4a) {
    BIT_TEST(1, d8);
}
// Test bit 1 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4b) {
    BIT_TEST(1, e8);
}
// Test bit 1 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4c) {
    BIT_TEST(1, h8);
}
// Test bit 1 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4d) {
    BIT_TEST(1, l8);
}
// Test bit 1 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4e) {
    BIT_TEST(1, RM(hl16));
}
// Test bit 1 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 4f) {
    BIT_TEST(1, a8);
}

// Test bit 2 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 50) {
    BIT_TEST(2, b8);
}
// Test bit 2 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 51) {
    BIT_TEST(2, c8);
}
// Test bit 2 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 52) {
    BIT_TEST(2, d8);
}
// Test bit 2 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 53) {
    BIT_TEST(2, e8);
}
// Test bit 2 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 54) {
    BIT_TEST(2, h8);
}
// Test bit 2 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 55) {
    BIT_TEST(2, l8);
}
// Test bit 2 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 56) {
    BIT_TEST(2, RM(hl16));
}
// Test bit 2 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 57) {
    BIT_TEST(2, a8);
}

// Test bit 3 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 58) {
    BIT_TEST(3, b8);
}
// Test bit 3 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 59) {
    BIT_TEST(3, c8);
}
// Test bit 3 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5a) {
    BIT_TEST(3, d8);
}
// Test bit 3 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5b) {
    BIT_TEST(3, e8);
}
// Test bit 3 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5c) {
    BIT_TEST(3, h8);
}
// Test bit 3 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5d) {
    BIT_TEST(3, l8);
}
// Test bit 3 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5e) {
    BIT_TEST(3, RM(hl16));
}
// Test bit 3 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 5f) {
    BIT_TEST(3, a8);
}

// Test bit 4 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 60) {
    BIT_TEST(4, b8);
}
// Test bit 4 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 61) {
    BIT_TEST(4, c8);
}
// Test bit 4 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 62) {
    BIT_TEST(4, d8);
}
// Test bit 4 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 63) {
    BIT_TEST(4, e8);
}
// Test bit 4 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 64) {
    BIT_TEST(4, h8);
}
// Test bit 4 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 65) {
    BIT_TEST(4, l8);
}
// Test bit 4 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 66) {
    BIT_TEST(4, RM(hl16));
}
// Test bit 4 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 67) {
    BIT_TEST(4, a8);
}

// Test bit 5 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 68) {
    BIT_TEST(5, b8);
}
// Test bit 5 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 69) {
    BIT_TEST(5, c8);
}
// Test bit 5 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6a) {
    BIT_TEST(5, d8);
}
// Test bit 5 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6b) {
    BIT_TEST(5, e8);
}
// Test bit 5 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6c) {
    BIT_TEST(5, h8);
}
// Test bit 5 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6d) {
    BIT_TEST(5, l8);
}
// Test bit 5 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6e) {
    BIT_TEST(5, RM(hl16));
}
// Test bit 5 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 6f) {
    BIT_TEST(5, a8);
}

// Test bit 6 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 70) {
    BIT_TEST(6, b8);
}
// Test bit 6 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 71) {
    BIT_TEST(6, c8);
}
// Test bit 6 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 72) {
    BIT_TEST(6, d8);
}
// Test bit 6 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 73) {
    BIT_TEST(6, e8);
}
// Test bit 6 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 74) {
    BIT_TEST(6, h8);
}
// Test bit 6 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 75) {
    BIT_TEST(6, l8);
}
// Test bit 6 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 76) {
    BIT_TEST(6, RM(hl16));
}
// Test bit 6 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 77) {
    BIT_TEST(6, a8);
}

// Test bit 7 of B in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 78) {
    BIT_TEST(7, b8);
}
// Test bit 7 of C in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 79) {
    BIT_TEST(7, c8);
}
// Test bit 7 of D in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7a) {
    BIT_TEST(7, d8);
}
// Test bit 7 of E in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7b) {
    BIT_TEST(7, e8);
}
// Test bit 7 of H in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7c) {
    BIT_TEST(7, h8);
}
// Test bit 7 of L in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7d) {
    BIT_TEST(7, l8);
}
// Test bit 7 of memory at HL in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7e) {
    BIT_TEST(7, RM(hl16));
}
// Test bit 7 of A in the flags; leave the operand unchanged. (UM0050 pp. 219-220).
OP(cb, 7f) {
    BIT_TEST(7, a8);
}

// Clear bit 0 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 80) {
    b8 = RES(0, b8);
}
// Clear bit 0 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 81) {
    c8 = RES(0, c8);
}
// Clear bit 0 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 82) {
    d8 = RES(0, d8);
}
// Clear bit 0 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 83) {
    e8 = RES(0, e8);
}
// Clear bit 0 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 84) {
    h8 = RES(0, h8);
}
// Clear bit 0 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 85) {
    l8 = RES(0, l8);
}
// Clear bit 0 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 86) {
    WM(hl16, RES(0, RM(hl16)));
}
// Clear bit 0 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 87) {
    a8 = RES(0, a8);
}

// Clear bit 1 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 88) {
    b8 = RES(1, b8);
}
// Clear bit 1 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 89) {
    c8 = RES(1, c8);
}
// Clear bit 1 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8a) {
    d8 = RES(1, d8);
}
// Clear bit 1 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8b) {
    e8 = RES(1, e8);
}
// Clear bit 1 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8c) {
    h8 = RES(1, h8);
}
// Clear bit 1 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8d) {
    l8 = RES(1, l8);
}
// Clear bit 1 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8e) {
    WM(hl16, RES(1, RM(hl16)));
}
// Clear bit 1 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 8f) {
    a8 = RES(1, a8);
}

// Clear bit 2 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 90) {
    b8 = RES(2, b8);
}
// Clear bit 2 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 91) {
    c8 = RES(2, c8);
}
// Clear bit 2 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 92) {
    d8 = RES(2, d8);
}
// Clear bit 2 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 93) {
    e8 = RES(2, e8);
}
// Clear bit 2 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 94) {
    h8 = RES(2, h8);
}
// Clear bit 2 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 95) {
    l8 = RES(2, l8);
}
// Clear bit 2 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 96) {
    WM(hl16, RES(2, RM(hl16)));
}
// Clear bit 2 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 97) {
    a8 = RES(2, a8);
}

// Clear bit 3 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 98) {
    b8 = RES(3, b8);
}
// Clear bit 3 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 99) {
    c8 = RES(3, c8);
}
// Clear bit 3 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9a) {
    d8 = RES(3, d8);
}
// Clear bit 3 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9b) {
    e8 = RES(3, e8);
}
// Clear bit 3 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9c) {
    h8 = RES(3, h8);
}
// Clear bit 3 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9d) {
    l8 = RES(3, l8);
}
// Clear bit 3 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9e) {
    WM(hl16, RES(3, RM(hl16)));
}
// Clear bit 3 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, 9f) {
    a8 = RES(3, a8);
}

// Clear bit 4 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a0) {
    b8 = RES(4, b8);
}
// Clear bit 4 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a1) {
    c8 = RES(4, c8);
}
// Clear bit 4 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a2) {
    d8 = RES(4, d8);
}
// Clear bit 4 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a3) {
    e8 = RES(4, e8);
}
// Clear bit 4 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a4) {
    h8 = RES(4, h8);
}
// Clear bit 4 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a5) {
    l8 = RES(4, l8);
}
// Clear bit 4 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a6) {
    WM(hl16, RES(4, RM(hl16)));
}
// Clear bit 4 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a7) {
    a8 = RES(4, a8);
}

// Clear bit 5 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a8) {
    b8 = RES(5, b8);
}
// Clear bit 5 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, a9) {
    c8 = RES(5, c8);
}
// Clear bit 5 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, aa) {
    d8 = RES(5, d8);
}
// Clear bit 5 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ab) {
    e8 = RES(5, e8);
}
// Clear bit 5 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ac) {
    h8 = RES(5, h8);
}
// Clear bit 5 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ad) {
    l8 = RES(5, l8);
}
// Clear bit 5 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ae) {
    WM(hl16, RES(5, RM(hl16)));
}
// Clear bit 5 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, af) {
    a8 = RES(5, a8);
}

// Clear bit 6 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b0) {
    b8 = RES(6, b8);
}
// Clear bit 6 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b1) {
    c8 = RES(6, c8);
}
// Clear bit 6 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b2) {
    d8 = RES(6, d8);
}
// Clear bit 6 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b3) {
    e8 = RES(6, e8);
}
// Clear bit 6 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b4) {
    h8 = RES(6, h8);
}
// Clear bit 6 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b5) {
    l8 = RES(6, l8);
}
// Clear bit 6 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b6) {
    WM(hl16, RES(6, RM(hl16)));
}
// Clear bit 6 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b7) {
    a8 = RES(6, a8);
}

// Clear bit 7 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b8) {
    b8 = RES(7, b8);
}
// Clear bit 7 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, b9) {
    c8 = RES(7, c8);
}
// Clear bit 7 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ba) {
    d8 = RES(7, d8);
}
// Clear bit 7 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, bb) {
    e8 = RES(7, e8);
}
// Clear bit 7 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, bc) {
    h8 = RES(7, h8);
}
// Clear bit 7 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, bd) {
    l8 = RES(7, l8);
}
// Clear bit 7 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, be) {
    WM(hl16, RES(7, RM(hl16)));
}
// Clear bit 7 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, bf) {
    a8 = RES(7, a8);
}

// Set bit 0 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c0) {
    b8 = SET(0, b8);
}
// Set bit 0 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c1) {
    c8 = SET(0, c8);
}
// Set bit 0 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c2) {
    d8 = SET(0, d8);
}
// Set bit 0 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c3) {
    e8 = SET(0, e8);
}
// Set bit 0 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c4) {
    h8 = SET(0, h8);
}
// Set bit 0 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c5) {
    l8 = SET(0, l8);
}
// Set bit 0 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c6) {
    WM(hl16, SET(0, RM(hl16)));
}
// Set bit 0 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c7) {
    a8 = SET(0, a8);
}

// Set bit 1 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c8) {
    b8 = SET(1, b8);
}
// Set bit 1 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, c9) {
    c8 = SET(1, c8);
}
// Set bit 1 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ca) {
    d8 = SET(1, d8);
}
// Set bit 1 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, cb) {
    e8 = SET(1, e8);
}
// Set bit 1 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, cc) {
    h8 = SET(1, h8);
}
// Set bit 1 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, cd) {
    l8 = SET(1, l8);
}
// Set bit 1 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ce) {
    WM(hl16, SET(1, RM(hl16)));
}
// Set bit 1 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, cf) {
    a8 = SET(1, a8);
}

// Set bit 2 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d0) {
    b8 = SET(2, b8);
}
// Set bit 2 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d1) {
    c8 = SET(2, c8);
}
// Set bit 2 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d2) {
    d8 = SET(2, d8);
}
// Set bit 2 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d3) {
    e8 = SET(2, e8);
}
// Set bit 2 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d4) {
    h8 = SET(2, h8);
}
// Set bit 2 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d5) {
    l8 = SET(2, l8);
}
// Set bit 2 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d6) {
    WM(hl16, SET(2, RM(hl16)));
}
// Set bit 2 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d7) {
    a8 = SET(2, a8);
}

// Set bit 3 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d8) {
    b8 = SET(3, b8);
}
// Set bit 3 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, d9) {
    c8 = SET(3, c8);
}
// Set bit 3 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, da) {
    d8 = SET(3, d8);
}
// Set bit 3 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, db) {
    e8 = SET(3, e8);
}
// Set bit 3 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, dc) {
    h8 = SET(3, h8);
}
// Set bit 3 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, dd) {
    l8 = SET(3, l8);
}
// Set bit 3 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, de) {
    WM(hl16, SET(3, RM(hl16)));
}
// Set bit 3 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, df) {
    a8 = SET(3, a8);
}

// Set bit 4 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e0) {
    b8 = SET(4, b8);
}
// Set bit 4 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e1) {
    c8 = SET(4, c8);
}
// Set bit 4 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e2) {
    d8 = SET(4, d8);
}
// Set bit 4 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e3) {
    e8 = SET(4, e8);
}
// Set bit 4 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e4) {
    h8 = SET(4, h8);
}
// Set bit 4 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e5) {
    l8 = SET(4, l8);
}
// Set bit 4 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e6) {
    WM(hl16, SET(4, RM(hl16)));
}
// Set bit 4 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e7) {
    a8 = SET(4, a8);
}

// Set bit 5 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e8) {
    b8 = SET(5, b8);
}
// Set bit 5 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, e9) {
    c8 = SET(5, c8);
}
// Set bit 5 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ea) {
    d8 = SET(5, d8);
}
// Set bit 5 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, eb) {
    e8 = SET(5, e8);
}
// Set bit 5 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ec) {
    h8 = SET(5, h8);
}
// Set bit 5 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ed) {
    l8 = SET(5, l8);
}
// Set bit 5 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ee) {
    WM(hl16, SET(5, RM(hl16)));
}
// Set bit 5 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ef) {
    a8 = SET(5, a8);
}

// Set bit 6 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f0) {
    b8 = SET(6, b8);
}
// Set bit 6 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f1) {
    c8 = SET(6, c8);
}
// Set bit 6 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f2) {
    d8 = SET(6, d8);
}
// Set bit 6 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f3) {
    e8 = SET(6, e8);
}
// Set bit 6 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f4) {
    h8 = SET(6, h8);
}
// Set bit 6 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f5) {
    l8 = SET(6, l8);
}
// Set bit 6 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f6) {
    WM(hl16, SET(6, RM(hl16)));
}
// Set bit 6 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f7) {
    a8 = SET(6, a8);
}

// Set bit 7 of B; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f8) {
    b8 = SET(7, b8);
}
// Set bit 7 of C; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, f9) {
    c8 = SET(7, c8);
}
// Set bit 7 of D; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, fa) {
    d8 = SET(7, d8);
}
// Set bit 7 of E; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, fb) {
    e8 = SET(7, e8);
}
// Set bit 7 of H; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, fc) {
    h8 = SET(7, h8);
}
// Set bit 7 of L; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, fd) {
    l8 = SET(7, l8);
}
// Set bit 7 of memory at HL; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, fe) {
    WM(hl16, SET(7, RM(hl16)));
}
// Set bit 7 of A; leave flags unchanged. (UM0050 pp. 219-220).
OP(cb, ff) {
    a8 = SET(7, a8);
}

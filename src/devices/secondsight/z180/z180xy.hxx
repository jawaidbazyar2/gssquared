// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   DD/FD-CB indexed bit operations for Second Sight's Zilog Z8S180.
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
 * opcodes with DD/FD CB prefix
 * rotate, shift and bit operations with (IX+o)
 **********************************************************/
// Rotate memory at the saved indexed address left, wrapping bit 7 into bit 0. Set carry from
// the outgoing bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 06) {
    WM(ea, RLC(RM(ea)));
}

// Rotate memory at the saved indexed address right, wrapping bit 0 into bit 7. Set carry from
// the outgoing bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 0e) {
    WM(ea, RRC(RM(ea)));
}

// Rotate memory at the saved indexed address left through carry. Set carry from the outgoing
// bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 16) {
    WM(ea, RL(RM(ea)));
}

// Rotate memory at the saved indexed address right through carry. Set carry from the outgoing
// bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 1e) {
    WM(ea, RR(RM(ea)));
}

// Shift memory at the saved indexed address left, inserting zero. Set carry from the outgoing
// bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 26) {
    WM(ea, SLA(RM(ea)));
}

// Shift memory at the saved indexed address right, retaining its sign bit. Set carry from the
// outgoing bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 2e) {
    WM(ea, SRA(RM(ea)));
}


// Shift memory at the saved indexed address right, inserting zero. Set carry from the outgoing
// bit and flags from the result. (UM0050 pp. 216-220).
OP(xycb, 3e) {
    WM(ea, SRL(RM(ea)));
}

// Test bit 0 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 46) {
    BIT_XY(0, RM(ea));
}

// Test bit 1 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 4e) {
    BIT_XY(1, RM(ea));
}

// Test bit 2 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 56) {
    BIT_XY(2, RM(ea));
}

// Test bit 3 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 5e) {
    BIT_XY(3, RM(ea));
}

// Test bit 4 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 66) {
    BIT_XY(4, RM(ea));
}

// Test bit 5 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 6e) {
    BIT_XY(5, RM(ea));
}

// Test bit 6 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 76) {
    BIT_XY(6, RM(ea));
}

// Test bit 7 of memory at the saved indexed address in the flags; leave the operand unchanged.
// (UM0050 pp. 219-220).
OP(xycb, 7e) {
    BIT_XY(7, RM(ea));
}

// Clear bit 0 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, 86) {
    WM(ea, RES(0, RM(ea)));
}

// Clear bit 1 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, 8e) {
    WM(ea, RES(1, RM(ea)));
}

// Clear bit 2 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, 96) {
    WM(ea, RES(2, RM(ea)));
}

// Clear bit 3 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, 9e) {
    WM(ea, RES(3, RM(ea)));
}

// Clear bit 4 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, a6) {
    WM(ea, RES(4, RM(ea)));
}

// Clear bit 5 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, ae) {
    WM(ea, RES(5, RM(ea)));
}

// Clear bit 6 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, b6) {
    WM(ea, RES(6, RM(ea)));
}

// Clear bit 7 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, be) {
    WM(ea, RES(7, RM(ea)));
}

// Set bit 0 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, c6) {
    WM(ea, SET(0, RM(ea)));
}

// Set bit 1 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, ce) {
    WM(ea, SET(1, RM(ea)));
}

// Set bit 2 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, d6) {
    WM(ea, SET(2, RM(ea)));
}

// Set bit 3 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, de) {
    WM(ea, SET(3, RM(ea)));
}

// Set bit 4 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, e6) {
    WM(ea, SET(4, RM(ea)));
}

// Set bit 5 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, ee) {
    WM(ea, SET(5, RM(ea)));
}

// Set bit 6 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, f6) {
    WM(ea, SET(6, RM(ea)));
}

// Set bit 7 of memory at the saved indexed address; leave flags unchanged. (UM0050 pp.
// 219-220).
OP(xycb, fe) {
    WM(ea, SET(7, RM(ea)));
}

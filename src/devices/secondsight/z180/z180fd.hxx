// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   FD-prefixed IY index-register instructions for Second Sight's Zilog Z8S180.
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
 *   Sources: Zilog UM005004-0918 pp. 68-72 for traps, 173-183 for registers,
 *   pp. 211-236 for operations, pp. 247-250 for valid encodings, and
 *   pp. 251-280 for bus ordering. Page citations below use printed pages.
 */

/**********************************************************
 * IY register related opcodes (FD prefix)
 **********************************************************/

// Add BC to IY. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(fd, 09) {
    ADD16(IY, BC);
}


// Add DE to IY. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(fd, 19) {
    ADD16(IY, DE);
}

// Copy the next immediate word to IY. (UM0050 pp. 222-224).
OP(fd, 21) {
    iy16 = ARG16();
}
// Copy IY to memory at the encoded address. (UM0050 pp. 222-224).
OP(fd, 22) {
    ea = ARG16();
    WM16(ea, &IY);
}
// Increment IY with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(fd, 23) {
    iy16++;
}

// Add IY to IY. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(fd, 29) {
    ADD16(IY, IY);
}
// Copy memory at the encoded address to IY. (UM0050 pp. 222-224).
OP(fd, 2a) {
    ea = ARG16();
    RM16(ea, &IY);
}
// Decrement IY with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(fd, 2b) {
    iy16--;
}

// Increment memory at IY plus the signed displacement with 8-bit wrap; update arithmetic flags
// and preserve carry. (UM0050 p. 213).
OP(fd, 34) {
    EAY();
    WM(ea, INC(RM(ea)));
}
// Decrement memory at IY plus the signed displacement with 8-bit wrap; update arithmetic flags
// and preserve carry. (UM0050 p. 213).
OP(fd, 35) {
    EAY();
    WM(ea, DEC(RM(ea)));
}
// Copy the next immediate byte to memory at IY plus the signed displacement. (UM0050 pp.
// 222-224).
OP(fd, 36) {
    EAY();
    WM(ea, ARG());
}

// Add SP to IY. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(fd, 39) {
    ADD16(IY, SP);
}

// Copy memory at IY plus the signed displacement to B. (UM0050 pp. 222-224).
OP(fd, 46) {
    EAY();
    b8 = RM(ea);
}

// Copy memory at IY plus the signed displacement to C. (UM0050 pp. 222-224).
OP(fd, 4e) {
    EAY();
    c8 = RM(ea);
}

// Copy memory at IY plus the signed displacement to D. (UM0050 pp. 222-224).
OP(fd, 56) {
    EAY();
    d8 = RM(ea);
}

// Copy memory at IY plus the signed displacement to E. (UM0050 pp. 222-224).
OP(fd, 5e) {
    EAY();
    e8 = RM(ea);
}

// Copy memory at IY plus the signed displacement to H. (UM0050 pp. 222-224).
OP(fd, 66) {
    EAY();
    h8 = RM(ea);
}

// Copy memory at IY plus the signed displacement to L. (UM0050 pp. 222-224).
OP(fd, 6e) {
    EAY();
    l8 = RM(ea);
}

// Copy B to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 70) {
    EAY();
    WM(ea, b8);
}
// Copy C to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 71) {
    EAY();
    WM(ea, c8);
}
// Copy D to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 72) {
    EAY();
    WM(ea, d8);
}
// Copy E to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 73) {
    EAY();
    WM(ea, e8);
}
// Copy H to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 74) {
    EAY();
    WM(ea, h8);
}
// Copy L to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 75) {
    EAY();
    WM(ea, l8);
}
// Copy A to memory at IY plus the signed displacement. (UM0050 pp. 222-224).
OP(fd, 77) {
    EAY();
    WM(ea, a8);
}

// Copy memory at IY plus the signed displacement to A. (UM0050 pp. 222-224).
OP(fd, 7e) {
    EAY();
    a8 = RM(ea);
}

// Add memory at IY plus the signed displacement to A. Store the wrapped result and update 8-bit
// arithmetic flags. (UM0050 pp. 211-212).
OP(fd, 86) {
    EAY();
    ADD(RM(ea));
}

// Add memory at IY plus the signed displacement and carry to A. Store the wrapped result and
// update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(fd, 8e) {
    EAY();
    ADC(RM(ea));
}

// Subtract memory at IY plus the signed displacement from A. Store the wrapped result and
// update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(fd, 96) {
    EAY();
    SUB(RM(ea));
}

// Subtract memory at IY plus the signed displacement and carry from A. Store the wrapped result
// and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(fd, 9e) {
    EAY();
    SBC(RM(ea));
}

// Combine A with memory at IY plus the signed displacement using bitwise AND. Store A and set
// logical-result flags. (UM0050 p. 214).
OP(fd, a6) {
    EAY();
    AND(RM(ea));
}

// Combine A with memory at IY plus the signed displacement using bitwise exclusive OR. Store A
// and set logical-result flags. (UM0050 p. 214).
OP(fd, ae) {
    EAY();
    XOR(RM(ea));
}

// Combine A with memory at IY plus the signed displacement using bitwise OR. Store A and set
// logical-result flags. (UM0050 p. 214).
OP(fd, b6) {
    EAY();
    OR(RM(ea));
}

// Compare A with memory at IY plus the signed displacement by setting subtraction flags; leave
// A unchanged. (UM0050 p. 213).
OP(fd, be) {
    EAY();
    CP(RM(ea));
}


// Read the signed index displacement, save its effective address, then fetch and dispatch the
// CB bit-operation byte (UM0050 pp. 182, 249, 267-271).
OP(fd, cb) {
    EAY();
    extra_cycles += exec_xycb(ROP());
}



// Read the low then high stack bytes into IY and advance SP. (UM0050 p. 227).
OP(fd, e1) {
    POP(IY);
}
// Exchange the word at SP and IY without changing arithmetic flags. (UM0050 p. 228).
OP(fd, e3) {
    EXSP(IY);
}
// Store IY on the descending stack, high byte first. (UM0050 p. 227).
OP(fd, e5) {
    PUSH(IY);
}

// Set PC to IY (the register value, without a memory read). (UM0050 pp. 229-230).
OP(fd, e9) {
    pc16 = iy16;
}


// Copy IY to SP. (UM0050 pp. 222-224).
OP(fd, f9) {
    sp16 = iy16;
}

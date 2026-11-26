// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   DD-prefixed IX index-register instructions for Second Sight's Zilog Z8S180.
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
 * IX register related opcodes (DD prefix)
 **********************************************************/

// Add BC to IX. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(dd, 09) {
    ADD16(IX, BC);
}


// Add DE to IX. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(dd, 19) {
    ADD16(IX, DE);
}

// Copy the next immediate word to IX. (UM0050 pp. 222-224).
OP(dd, 21) {
    ix16 = ARG16();
}
// Copy IX to memory at the encoded address. (UM0050 pp. 222-224).
OP(dd, 22) {
    ea = ARG16();
    WM16(ea, &IX);
}
// Increment IX with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(dd, 23) {
    ix16++;
}

// Add IX to IX. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(dd, 29) {
    ADD16(IX, IX);
}
// Copy memory at the encoded address to IX. (UM0050 pp. 222-224).
OP(dd, 2a) {
    ea = ARG16();
    RM16(ea, &IX);
}
// Decrement IX with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(dd, 2b) {
    ix16--;
}

// Increment memory at IX plus the signed displacement with 8-bit wrap; update arithmetic flags
// and preserve carry. (UM0050 p. 213).
OP(dd, 34) {
    EAX();
    WM(ea, INC(RM(ea)));
}
// Decrement memory at IX plus the signed displacement with 8-bit wrap; update arithmetic flags
// and preserve carry. (UM0050 p. 213).
OP(dd, 35) {
    EAX();
    WM(ea, DEC(RM(ea)));
}
// Copy the next immediate byte to memory at IX plus the signed displacement. (UM0050 pp.
// 222-224).
OP(dd, 36) {
    EAX();
    WM(ea, ARG());
}

// Add SP to IX. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(dd, 39) {
    ADD16(IX, SP);
}

// Copy memory at IX plus the signed displacement to B. (UM0050 pp. 222-224).
OP(dd, 46) {
    EAX();
    b8 = RM(ea);
}

// Copy memory at IX plus the signed displacement to C. (UM0050 pp. 222-224).
OP(dd, 4e) {
    EAX();
    c8 = RM(ea);
}

// Copy memory at IX plus the signed displacement to D. (UM0050 pp. 222-224).
OP(dd, 56) {
    EAX();
    d8 = RM(ea);
}

// Copy memory at IX plus the signed displacement to E. (UM0050 pp. 222-224).
OP(dd, 5e) {
    EAX();
    e8 = RM(ea);
}

// Copy memory at IX plus the signed displacement to H. (UM0050 pp. 222-224).
OP(dd, 66) {
    EAX();
    h8 = RM(ea);
}

// Copy memory at IX plus the signed displacement to L. (UM0050 pp. 222-224).
OP(dd, 6e) {
    EAX();
    l8 = RM(ea);
}

// Copy B to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 70) {
    EAX();
    WM(ea, b8);
}
// Copy C to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 71) {
    EAX();
    WM(ea, c8);
}
// Copy D to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 72) {
    EAX();
    WM(ea, d8);
}
// Copy E to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 73) {
    EAX();
    WM(ea, e8);
}
// Copy H to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 74) {
    EAX();
    WM(ea, h8);
}
// Copy L to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 75) {
    EAX();
    WM(ea, l8);
}
// Copy A to memory at IX plus the signed displacement. (UM0050 pp. 222-224).
OP(dd, 77) {
    EAX();
    WM(ea, a8);
}

// Copy memory at IX plus the signed displacement to A. (UM0050 pp. 222-224).
OP(dd, 7e) {
    EAX();
    a8 = RM(ea);
}

// Add memory at IX plus the signed displacement to A. Store the wrapped result and update 8-bit
// arithmetic flags. (UM0050 pp. 211-212).
OP(dd, 86) {
    EAX();
    ADD(RM(ea));
}

// Add memory at IX plus the signed displacement and carry to A. Store the wrapped result and
// update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(dd, 8e) {
    EAX();
    ADC(RM(ea));
}

// Subtract memory at IX plus the signed displacement from A. Store the wrapped result and
// update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(dd, 96) {
    EAX();
    SUB(RM(ea));
}

// Subtract memory at IX plus the signed displacement and carry from A. Store the wrapped result
// and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(dd, 9e) {
    EAX();
    SBC(RM(ea));
}

// Combine A with memory at IX plus the signed displacement using bitwise AND. Store A and set
// logical-result flags. (UM0050 p. 214).
OP(dd, a6) {
    EAX();
    AND(RM(ea));
}

// Combine A with memory at IX plus the signed displacement using bitwise exclusive OR. Store A
// and set logical-result flags. (UM0050 p. 214).
OP(dd, ae) {
    EAX();
    XOR(RM(ea));
}

// Combine A with memory at IX plus the signed displacement using bitwise OR. Store A and set
// logical-result flags. (UM0050 p. 214).
OP(dd, b6) {
    EAX();
    OR(RM(ea));
}

// Compare A with memory at IX plus the signed displacement by setting subtraction flags; leave
// A unchanged. (UM0050 p. 213).
OP(dd, be) {
    EAX();
    CP(RM(ea));
}


// Read the signed index displacement, save its effective address, then fetch and dispatch the
// CB bit-operation byte (UM0050 pp. 182, 249, 267-271).
OP(dd, cb) {
    EAX();
    extra_cycles += exec_xycb(ROP());
}



// Read the low then high stack bytes into IX and advance SP. (UM0050 p. 227).
OP(dd, e1) {
    POP(IX);
}
// Exchange the word at SP and IX without changing arithmetic flags. (UM0050 p. 228).
OP(dd, e3) {
    EXSP(IX);
}
// Store IX on the descending stack, high byte first. (UM0050 p. 227).
OP(dd, e5) {
    PUSH(IX);
}

// Set PC to IX (the register value, without a memory read). (UM0050 pp. 229-230).
OP(dd, e9) {
    pc16 = ix16;
}


// Copy IX to SP. (UM0050 pp. 222-224).
OP(dd, f9) {
    sp16 = ix16;
}

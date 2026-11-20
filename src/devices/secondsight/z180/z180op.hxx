// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   Unprefixed instructions for Second Sight's Zilog Z8S180 processor.
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
 * main opcodes
 **********************************************************/
// Consume instruction time without changing programmer registers. (UM0050 p. 235).
OP(op, 00) {}
// Copy the next immediate word to BC. (UM0050 pp. 222-224).
OP(op, 01) {
    bc16 = ARG16();
}
// Copy A to memory at BC. (UM0050 pp. 222-224).
OP(op, 02) {
    WM(bc16, a8);
}
// Increment BC with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 03) {
    bc16++;
}
// Increment B with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 04) {
    b8 = INC(b8);
}
// Decrement B with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 05) {
    b8 = DEC(b8);
}
// Copy the next immediate byte to B. (UM0050 pp. 222-224).
OP(op, 06) {
    b8 = ARG();
}
// Rotate A left with bit 7 wrapping to bit 0 and carry; preserve sign, zero and parity. (UM0050
// p. 216).
OP(op, 07) {
    RLCA;
}

// Exchange A and F with their alternate pair, including the flag bits (UM0050 p. 228).
OP(op, 08) {
    EX_AF;
}
// Add BC to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(op, 09) {
    ADD16(HL, BC);
}
// Copy memory at BC to A. (UM0050 pp. 222-224).
OP(op, 0a) {
    a8 = RM(bc16);
}
// Decrement BC with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 0b) {
    bc16--;
}
// Increment C with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 0c) {
    c8 = INC(c8);
}
// Decrement C with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 0d) {
    c8 = DEC(c8);
}
// Copy the next immediate byte to C. (UM0050 pp. 222-224).
OP(op, 0e) {
    c8 = ARG();
}
// Rotate A right with bit 0 wrapping to bit 7 and carry; preserve sign, zero and parity.
// (UM0050 p. 216).
OP(op, 0f) {
    RRCA;
}

// Decrement B; branch by the signed displacement when B remains nonzero. Flags stay unchanged.
// (UM0050 pp. 229-230).
OP(op, 10) {
    b8--;
    JR_COND(b8, 0x10);
}
// Copy the next immediate word to DE. (UM0050 pp. 222-224).
OP(op, 11) {
    de16 = ARG16();
}
// Copy A to memory at DE. (UM0050 pp. 222-224).
OP(op, 12) {
    WM(de16, a8);
}
// Increment DE with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 13) {
    de16++;
}
// Increment D with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 14) {
    d8 = INC(d8);
}
// Decrement D with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 15) {
    d8 = DEC(d8);
}
// Copy the next immediate byte to D. (UM0050 pp. 222-224).
OP(op, 16) {
    d8 = ARG();
}
// Rotate A left through carry; preserve sign, zero and parity. (UM0050 p. 216).
OP(op, 17) {
    RLA;
}

// Add the signed displacement to PC after reading it. (UM0050 pp. 229-230).
OP(op, 18) {
    JR();
}
// Add DE to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(op, 19) {
    ADD16(HL, DE);
}
// Copy memory at DE to A. (UM0050 pp. 222-224).
OP(op, 1a) {
    a8 = RM(de16);
}
// Decrement DE with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 1b) {
    de16--;
}
// Increment E with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 1c) {
    e8 = INC(e8);
}
// Decrement E with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 1d) {
    e8 = DEC(e8);
}
// Copy the next immediate byte to E. (UM0050 pp. 222-224).
OP(op, 1e) {
    e8 = ARG();
}
// Rotate A right through carry; preserve sign, zero and parity. (UM0050 p. 216).
OP(op, 1f) {
    RRA;
}

// Add the signed displacement to PC after reading it if zero is clear. (UM0050 pp. 229-230).
OP(op, 20) {
    JR_COND(!(f8 & ZF), 0x20);
}
// Copy the next immediate word to HL. (UM0050 pp. 222-224).
OP(op, 21) {
    hl16 = ARG16();
}
// Copy HL to memory at the encoded address. (UM0050 pp. 222-224).
OP(op, 22) {
    ea = ARG16();
    WM16(ea, &HL);
}
// Increment HL with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 23) {
    hl16++;
}
// Increment H with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 24) {
    h8 = INC(h8);
}
// Decrement H with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 25) {
    h8 = DEC(h8);
}
// Copy the next immediate byte to H. (UM0050 pp. 222-224).
OP(op, 26) {
    h8 = ARG();
}
// Correct A after decimal-digit arithmetic using the carry, half-carry and subtract flags.
// (UM0050 p. 215).
OP(op, 27) {
    DAA;
}

// Add the signed displacement to PC after reading it if zero is set. (UM0050 pp. 229-230).
OP(op, 28) {
    JR_COND(f8 & ZF, 0x28);
}
// Add HL to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(op, 29) {
    ADD16(HL, HL);
}
// Copy memory at the encoded address to HL. (UM0050 pp. 222-224).
OP(op, 2a) {
    ea = ARG16();
    RM16(ea, &HL);
}
// Decrement HL with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 2b) {
    hl16--;
}
// Increment L with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 2c) {
    l8 = INC(l8);
}
// Decrement L with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 2d) {
    l8 = DEC(l8);
}
// Copy the next immediate byte to L. (UM0050 pp. 222-224).
OP(op, 2e) {
    l8 = ARG();
}
// Invert all bits of A; set half-carry/subtract and preserve sign, zero, parity and carry.
// (UM0050 p. 215).
OP(op, 2f) {
    a8 ^= 0xff;
    f8 = (f8 & (SF | ZF | PF | CF)) | HF | NF | (a8 & (YF | XF));
}

// Add the signed displacement to PC after reading it if carry is clear. (UM0050 pp. 229-230).
OP(op, 30) {
    JR_COND(!(f8 & CF), 0x30);
}
// Copy the next immediate word to SP. (UM0050 pp. 222-224).
OP(op, 31) {
    sp16 = ARG16();
}
// Copy A to memory at the encoded address. (UM0050 pp. 222-224).
OP(op, 32) {
    ea = ARG16();
    WM(ea, a8);
}
// Increment SP with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 33) {
    sp16++;
}
// Increment memory at HL with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050
// p. 213).
OP(op, 34) {
    WM(hl16, INC(RM(hl16)));
}
// Decrement memory at HL with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050
// p. 213).
OP(op, 35) {
    WM(hl16, DEC(RM(hl16)));
}
// Copy the next immediate byte to memory at HL. (UM0050 pp. 222-224).
OP(op, 36) {
    WM(hl16, ARG());
}
// Set carry and clear half-carry/subtract; preserve sign, zero and parity. (UM0050 p. 235).
OP(op, 37) {
    f8 = (f8 & (SF | ZF | PF)) | CF | (a8 & (YF | XF));
}

// Add the signed displacement to PC after reading it if carry is set. (UM0050 pp. 229-230).
OP(op, 38) {
    JR_COND(f8 & CF, 0x38);
}
// Add SP to HL. Store the wrapped result and update 16-bit arithmetic flags. (UM0050 p. 221).
OP(op, 39) {
    ADD16(HL, SP);
}
// Copy memory at the encoded address to A. (UM0050 pp. 222-224).
OP(op, 3a) {
    ea = ARG16();
    a8 = RM(ea);
}
// Decrement SP with 16-bit wrap; flags stay unchanged. (UM0050 p. 221).
OP(op, 3b) {
    sp16--;
}
// Increment A with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 3c) {
    a8 = INC(a8);
}
// Decrement A with 8-bit wrap; update arithmetic flags and preserve carry. (UM0050 p. 213).
OP(op, 3d) {
    a8 = DEC(a8);
}
// Copy the next immediate byte to A. (UM0050 pp. 222-224).
OP(op, 3e) {
    a8 = ARG();
}
// UM0050 p. 235: CCF clears half-carry and subtract as well as complementing carry.
// Invert carry and clear half-carry/subtract; preserve sign, zero and parity. (UM0050 p. 235).
OP(op, 3f) {
    f8 = ((f8 & (SF | ZF | PF | CF)) | (a8 & (YF | XF))) ^ CF;
}

// Leave B unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 40) {}
// Copy C to B. (UM0050 pp. 222-224).
OP(op, 41) {
    b8 = c8;
}
// Copy D to B. (UM0050 pp. 222-224).
OP(op, 42) {
    b8 = d8;
}
// Copy E to B. (UM0050 pp. 222-224).
OP(op, 43) {
    b8 = e8;
}
// Copy H to B. (UM0050 pp. 222-224).
OP(op, 44) {
    b8 = h8;
}
// Copy L to B. (UM0050 pp. 222-224).
OP(op, 45) {
    b8 = l8;
}
// Copy memory at HL to B. (UM0050 pp. 222-224).
OP(op, 46) {
    b8 = RM(hl16);
}
// Copy A to B. (UM0050 pp. 222-224).
OP(op, 47) {
    b8 = a8;
}

// Copy B to C. (UM0050 pp. 222-224).
OP(op, 48) {
    c8 = b8;
}
// Leave C unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 49) {}
// Copy D to C. (UM0050 pp. 222-224).
OP(op, 4a) {
    c8 = d8;
}
// Copy E to C. (UM0050 pp. 222-224).
OP(op, 4b) {
    c8 = e8;
}
// Copy H to C. (UM0050 pp. 222-224).
OP(op, 4c) {
    c8 = h8;
}
// Copy L to C. (UM0050 pp. 222-224).
OP(op, 4d) {
    c8 = l8;
}
// Copy memory at HL to C. (UM0050 pp. 222-224).
OP(op, 4e) {
    c8 = RM(hl16);
}
// Copy A to C. (UM0050 pp. 222-224).
OP(op, 4f) {
    c8 = a8;
}

// Copy B to D. (UM0050 pp. 222-224).
OP(op, 50) {
    d8 = b8;
}
// Copy C to D. (UM0050 pp. 222-224).
OP(op, 51) {
    d8 = c8;
}
// Leave D unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 52) {}
// Copy E to D. (UM0050 pp. 222-224).
OP(op, 53) {
    d8 = e8;
}
// Copy H to D. (UM0050 pp. 222-224).
OP(op, 54) {
    d8 = h8;
}
// Copy L to D. (UM0050 pp. 222-224).
OP(op, 55) {
    d8 = l8;
}
// Copy memory at HL to D. (UM0050 pp. 222-224).
OP(op, 56) {
    d8 = RM(hl16);
}
// Copy A to D. (UM0050 pp. 222-224).
OP(op, 57) {
    d8 = a8;
}

// Copy B to E. (UM0050 pp. 222-224).
OP(op, 58) {
    e8 = b8;
}
// Copy C to E. (UM0050 pp. 222-224).
OP(op, 59) {
    e8 = c8;
}
// Copy D to E. (UM0050 pp. 222-224).
OP(op, 5a) {
    e8 = d8;
}
// Leave E unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 5b) {}
// Copy H to E. (UM0050 pp. 222-224).
OP(op, 5c) {
    e8 = h8;
}
// Copy L to E. (UM0050 pp. 222-224).
OP(op, 5d) {
    e8 = l8;
}
// Copy memory at HL to E. (UM0050 pp. 222-224).
OP(op, 5e) {
    e8 = RM(hl16);
}
// Copy A to E. (UM0050 pp. 222-224).
OP(op, 5f) {
    e8 = a8;
}

// Copy B to H. (UM0050 pp. 222-224).
OP(op, 60) {
    h8 = b8;
}
// Copy C to H. (UM0050 pp. 222-224).
OP(op, 61) {
    h8 = c8;
}
// Copy D to H. (UM0050 pp. 222-224).
OP(op, 62) {
    h8 = d8;
}
// Copy E to H. (UM0050 pp. 222-224).
OP(op, 63) {
    h8 = e8;
}
// Leave H unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 64) {}
// Copy L to H. (UM0050 pp. 222-224).
OP(op, 65) {
    h8 = l8;
}
// Copy memory at HL to H. (UM0050 pp. 222-224).
OP(op, 66) {
    h8 = RM(hl16);
}
// Copy A to H. (UM0050 pp. 222-224).
OP(op, 67) {
    h8 = a8;
}

// Copy B to L. (UM0050 pp. 222-224).
OP(op, 68) {
    l8 = b8;
}
// Copy C to L. (UM0050 pp. 222-224).
OP(op, 69) {
    l8 = c8;
}
// Copy D to L. (UM0050 pp. 222-224).
OP(op, 6a) {
    l8 = d8;
}
// Copy E to L. (UM0050 pp. 222-224).
OP(op, 6b) {
    l8 = e8;
}
// Copy H to L. (UM0050 pp. 222-224).
OP(op, 6c) {
    l8 = h8;
}
// Leave L unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 6d) {}
// Copy memory at HL to L. (UM0050 pp. 222-224).
OP(op, 6e) {
    l8 = RM(hl16);
}
// Copy A to L. (UM0050 pp. 222-224).
OP(op, 6f) {
    l8 = a8;
}

// Copy B to memory at HL. (UM0050 pp. 222-224).
OP(op, 70) {
    WM(hl16, b8);
}
// Copy C to memory at HL. (UM0050 pp. 222-224).
OP(op, 71) {
    WM(hl16, c8);
}
// Copy D to memory at HL. (UM0050 pp. 222-224).
OP(op, 72) {
    WM(hl16, d8);
}
// Copy E to memory at HL. (UM0050 pp. 222-224).
OP(op, 73) {
    WM(hl16, e8);
}
// Copy H to memory at HL. (UM0050 pp. 222-224).
OP(op, 74) {
    WM(hl16, h8);
}
// Copy L to memory at HL. (UM0050 pp. 222-224).
OP(op, 75) {
    WM(hl16, l8);
}
// Stop executing new instructions while bus/refresh clocks continue until an interrupt. (UM0050
// pp. 33, 235).
OP(op, 76) {
    ENTER_HALT();
}
// Copy A to memory at HL. (UM0050 pp. 222-224).
OP(op, 77) {
    WM(hl16, a8);
}

// Copy B to A. (UM0050 pp. 222-224).
OP(op, 78) {
    a8 = b8;
}
// Copy C to A. (UM0050 pp. 222-224).
OP(op, 79) {
    a8 = c8;
}
// Copy D to A. (UM0050 pp. 222-224).
OP(op, 7a) {
    a8 = d8;
}
// Copy E to A. (UM0050 pp. 222-224).
OP(op, 7b) {
    a8 = e8;
}
// Copy H to A. (UM0050 pp. 222-224).
OP(op, 7c) {
    a8 = h8;
}
// Copy L to A. (UM0050 pp. 222-224).
OP(op, 7d) {
    a8 = l8;
}
// Copy memory at HL to A. (UM0050 pp. 222-224).
OP(op, 7e) {
    a8 = RM(hl16);
}
// Leave A unchanged; this self-load still consumes instruction time. (UM0050 pp. 222-224).
OP(op, 7f) {}

// Add B to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 80) {
    ADD(b8);
}
// Add C to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 81) {
    ADD(c8);
}
// Add D to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 82) {
    ADD(d8);
}
// Add E to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 83) {
    ADD(e8);
}
// Add H to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 84) {
    ADD(h8);
}
// Add L to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 85) {
    ADD(l8);
}
// Add memory at HL to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050
// pp. 211-212).
OP(op, 86) {
    ADD(RM(hl16));
}
// Add A to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp. 211-212).
OP(op, 87) {
    ADD(a8);
}

// Add B and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 88) {
    ADC(b8);
}
// Add C and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 89) {
    ADC(c8);
}
// Add D and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 8a) {
    ADC(d8);
}
// Add E and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 8b) {
    ADC(e8);
}
// Add H and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 8c) {
    ADC(h8);
}
// Add L and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 8d) {
    ADC(l8);
}
// Add memory at HL and carry to A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 8e) {
    ADC(RM(hl16));
}
// Add A and carry to A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 8f) {
    ADC(a8);
}

// Subtract B from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 90) {
    SUB(b8);
}
// Subtract C from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 91) {
    SUB(c8);
}
// Subtract D from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 92) {
    SUB(d8);
}
// Subtract E from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 93) {
    SUB(e8);
}
// Subtract H from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 94) {
    SUB(h8);
}
// Subtract L from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 95) {
    SUB(l8);
}
// Subtract memory at HL from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 96) {
    SUB(RM(hl16));
}
// Subtract A from A. Store the wrapped result and update 8-bit arithmetic flags. (UM0050 pp.
// 211-212).
OP(op, 97) {
    SUB(a8);
}

// Subtract B and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 98) {
    SBC(b8);
}
// Subtract C and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 99) {
    SBC(c8);
}
// Subtract D and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 9a) {
    SBC(d8);
}
// Subtract E and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 9b) {
    SBC(e8);
}
// Subtract H and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 9c) {
    SBC(h8);
}
// Subtract L and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 9d) {
    SBC(l8);
}
// Subtract memory at HL and carry from A. Store the wrapped result and update 8-bit arithmetic
// flags. (UM0050 pp. 211-212).
OP(op, 9e) {
    SBC(RM(hl16));
}
// Subtract A and carry from A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, 9f) {
    SBC(a8);
}

// Combine A with B using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a0) {
    AND(b8);
}
// Combine A with C using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a1) {
    AND(c8);
}
// Combine A with D using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a2) {
    AND(d8);
}
// Combine A with E using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a3) {
    AND(e8);
}
// Combine A with H using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a4) {
    AND(h8);
}
// Combine A with L using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a5) {
    AND(l8);
}
// Combine A with memory at HL using bitwise AND. Store A and set logical-result flags. (UM0050
// p. 214).
OP(op, a6) {
    AND(RM(hl16));
}
// Combine A with A using bitwise AND. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, a7) {
    AND(a8);
}

// Combine A with B using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, a8) {
    XOR(b8);
}
// Combine A with C using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, a9) {
    XOR(c8);
}
// Combine A with D using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, aa) {
    XOR(d8);
}
// Combine A with E using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, ab) {
    XOR(e8);
}
// Combine A with H using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, ac) {
    XOR(h8);
}
// Combine A with L using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, ad) {
    XOR(l8);
}
// Combine A with memory at HL using bitwise exclusive OR. Store A and set logical-result flags.
// (UM0050 p. 214).
OP(op, ae) {
    XOR(RM(hl16));
}
// Combine A with A using bitwise exclusive OR. Store A and set logical-result flags. (UM0050 p.
// 214).
OP(op, af) {
    XOR(a8);
}

// Combine A with B using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b0) {
    OR(b8);
}
// Combine A with C using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b1) {
    OR(c8);
}
// Combine A with D using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b2) {
    OR(d8);
}
// Combine A with E using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b3) {
    OR(e8);
}
// Combine A with H using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b4) {
    OR(h8);
}
// Combine A with L using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b5) {
    OR(l8);
}
// Combine A with memory at HL using bitwise OR. Store A and set logical-result flags. (UM0050
// p. 214).
OP(op, b6) {
    OR(RM(hl16));
}
// Combine A with A using bitwise OR. Store A and set logical-result flags. (UM0050 p. 214).
OP(op, b7) {
    OR(a8);
}

// Compare A with B by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, b8) {
    CP(b8);
}
// Compare A with C by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, b9) {
    CP(c8);
}
// Compare A with D by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, ba) {
    CP(d8);
}
// Compare A with E by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, bb) {
    CP(e8);
}
// Compare A with H by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, bc) {
    CP(h8);
}
// Compare A with L by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, bd) {
    CP(l8);
}
// Compare A with memory at HL by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, be) {
    CP(RM(hl16));
}
// Compare A with A by setting subtraction flags; leave A unchanged. (UM0050 p. 213).
OP(op, bf) {
    CP(a8);
}

// Pop the return address into PC if zero is clear. (UM0050 pp. 229-230).
OP(op, c0) {
    RET_COND(!(f8 & ZF), 0xc0);
}
// Read the low then high stack bytes into BC and advance SP. (UM0050 p. 227).
OP(op, c1) {
    POP(BC);
}
// Set PC to the encoded address if zero is clear. (UM0050 pp. 229-230).
OP(op, c2) {
    JP_COND(!(f8 & ZF));
}
// Set PC to the encoded address. (UM0050 pp. 229-230).
OP(op, c3) {
    JP;
}
// Push the address after this instruction and jump to the encoded address if zero is clear.
// (UM0050 pp. 229-230).
OP(op, c4) {
    CALL_COND(!(f8 & ZF), 0xc4);
}
// Store BC on the descending stack, high byte first. (UM0050 p. 227).
OP(op, c5) {
    PUSH(BC);
}
// Add the next immediate byte to A. Store the wrapped result and update 8-bit arithmetic flags.
// (UM0050 pp. 211-212).
OP(op, c6) {
    ADD(ARG());
}
// Push the return address and enter fixed routine $00. (UM0050 p. 230).
OP(op, c7) {
    RST(0x00);
}

// Pop the return address into PC if zero is set. (UM0050 pp. 229-230).
OP(op, c8) {
    RET_COND(f8 & ZF, 0xc8);
}
// Pop the return address into PC. (UM0050 pp. 229-230).
OP(op, c9) {
    POP(PC);
}
// Set PC to the encoded address if zero is set. (UM0050 pp. 229-230).
OP(op, ca) {
    JP_COND(f8 & ZF);
}
// Fetch the next opcode byte and dispatch the $CB prefix family; include its timing in this
// instruction (UM0050 pp. 247-250).
OP(op, cb) {
    extra_cycles += exec_cb(ROP());
}
// Push the address after this instruction and jump to the encoded address if zero is set.
// (UM0050 pp. 229-230).
OP(op, cc) {
    CALL_COND(f8 & ZF, 0xcc);
}
// Push the address after this instruction and jump to the encoded address. (UM0050 pp.
// 229-230).
OP(op, cd) {
    CALL();
}
// Add the next immediate byte and carry to A. Store the wrapped result and update 8-bit
// arithmetic flags. (UM0050 pp. 211-212).
OP(op, ce) {
    ADC(ARG());
}
// Push the return address and enter fixed routine $08. (UM0050 p. 230).
OP(op, cf) {
    RST(0x08);
}

// Pop the return address into PC if carry is clear. (UM0050 pp. 229-230).
OP(op, d0) {
    RET_COND(!(f8 & CF), 0xd0);
}
// Read the low then high stack bytes into DE and advance SP. (UM0050 p. 227).
OP(op, d1) {
    POP(DE);
}
// Set PC to the encoded address if carry is clear. (UM0050 pp. 229-230).
OP(op, d2) {
    JP_COND(!(f8 & CF));
}
// Write A to the port formed by A as high byte and the immediate low byte. (UM0050 pp.
// 231-234).
OP(op, d3) {
    unsigned n = ARG() | (a8 << 8);
    OUT(n, a8);
}
// Push the address after this instruction and jump to the encoded address if carry is clear.
// (UM0050 pp. 229-230).
OP(op, d4) {
    CALL_COND(!(f8 & CF), 0xd4);
}
// Store DE on the descending stack, high byte first. (UM0050 p. 227).
OP(op, d5) {
    PUSH(DE);
}
// Subtract the next immediate byte from A. Store the wrapped result and update 8-bit arithmetic
// flags. (UM0050 pp. 211-212).
OP(op, d6) {
    SUB(ARG());
}
// Push the return address and enter fixed routine $10. (UM0050 p. 230).
OP(op, d7) {
    RST(0x10);
}

// Pop the return address into PC if carry is set. (UM0050 pp. 229-230).
OP(op, d8) {
    RET_COND(f8 & CF, 0xd8);
}
// Exchange BC, DE and HL with their alternate pairs; leave AF unchanged. (UM0050 p. 228).
OP(op, d9) {
    EXX;
}
// Set PC to the encoded address if carry is set. (UM0050 pp. 229-230).
OP(op, da) {
    JP_COND(f8 & CF);
}
// Read the port formed by A as high byte and the immediate low byte into A. (UM0050 pp.
// 231-234).
OP(op, db) {
    unsigned n = ARG() | (a8 << 8);
    a8 = IN(n);
}
// Push the address after this instruction and jump to the encoded address if carry is set.
// (UM0050 pp. 229-230).
OP(op, dc) {
    CALL_COND(f8 & CF, 0xdc);
}
// Fetch the next opcode byte and dispatch the $DD prefix family; include its timing in this
// instruction (UM0050 pp. 247-250).
OP(op, dd) {
    extra_cycles += exec_dd(ROP());
}
// Subtract the next immediate byte and carry from A. Store the wrapped result and update 8-bit
// arithmetic flags. (UM0050 pp. 211-212).
OP(op, de) {
    SBC(ARG());
}
// Push the return address and enter fixed routine $18. (UM0050 p. 230).
OP(op, df) {
    RST(0x18);
}

// Pop the return address into PC if parity/overflow is clear. (UM0050 pp. 229-230).
OP(op, e0) {
    RET_COND(!(f8 & PF), 0xe0);
}
// Read the low then high stack bytes into HL and advance SP. (UM0050 p. 227).
OP(op, e1) {
    POP(HL);
}
// Set PC to the encoded address if parity/overflow is clear. (UM0050 pp. 229-230).
OP(op, e2) {
    JP_COND(!(f8 & PF));
}
// Exchange HL and the word at SP without changing arithmetic flags. (UM0050 p. 228).
OP(op, e3) {
    EXSP(HL);
}
// Push the address after this instruction and jump to the encoded address if parity/overflow is
// clear. (UM0050 pp. 229-230).
OP(op, e4) {
    CALL_COND(!(f8 & PF), 0xe4);
}
// Store HL on the descending stack, high byte first. (UM0050 p. 227).
OP(op, e5) {
    PUSH(HL);
}
// Combine A with the next immediate byte using bitwise AND. Store A and set logical-result
// flags. (UM0050 p. 214).
OP(op, e6) {
    AND(ARG());
}
// Push the return address and enter fixed routine $20. (UM0050 p. 230).
OP(op, e7) {
    RST(0x20);
}

// Pop the return address into PC if parity/overflow is set. (UM0050 pp. 229-230).
OP(op, e8) {
    RET_COND(f8 & PF, 0xe8);
}
// Set PC to HL (the register value, without a memory read). (UM0050 pp. 229-230).
OP(op, e9) {
    pc16 = hl16;
}
// Set PC to the encoded address if parity/overflow is set. (UM0050 pp. 229-230).
OP(op, ea) {
    JP_COND(f8 & PF);
}
// Exchange DE and HL without changing arithmetic flags. (UM0050 p. 228).
OP(op, eb) {
    EX_DE_HL;
}
// Push the address after this instruction and jump to the encoded address if parity/overflow is
// set. (UM0050 pp. 229-230).
OP(op, ec) {
    CALL_COND(f8 & PF, 0xec);
}
// Fetch the next opcode byte and dispatch the $ED prefix family; include its timing in this
// instruction (UM0050 pp. 247-250).
OP(op, ed) {
    extra_cycles += exec_ed(ROP());
}
// Combine A with the next immediate byte using bitwise exclusive OR. Store A and set logical-
// result flags. (UM0050 p. 214).
OP(op, ee) {
    XOR(ARG());
}
// Push the return address and enter fixed routine $28. (UM0050 p. 230).
OP(op, ef) {
    RST(0x28);
}

// Pop the return address into PC if sign is clear. (UM0050 pp. 229-230).
OP(op, f0) {
    RET_COND(!(f8 & SF), 0xf0);
}
// Read the low then high stack bytes into AF and advance SP. (UM0050 p. 227).
OP(op, f1) {
    POP(AF);
}
// Set PC to the encoded address if sign is clear. (UM0050 pp. 229-230).
OP(op, f2) {
    JP_COND(!(f8 & SF));
}
// Clear both interrupt enables (UM0050 pp. 68-69, 235). This implementation
// also delays its next interrupt check through the following instruction.
OP(op, f3) {
    IFF1 = IFF2 = 0;
    after_EI = 1;
}
// Push the address after this instruction and jump to the encoded address if sign is clear.
// (UM0050 pp. 229-230).
OP(op, f4) {
    CALL_COND(!(f8 & SF), 0xf4);
}
// Store AF on the descending stack, high byte first. (UM0050 p. 227).
OP(op, f5) {
    PUSH(AF);
}
// Combine A with the next immediate byte using bitwise OR. Store A and set logical-result
// flags. (UM0050 p. 214).
OP(op, f6) {
    OR(ARG());
}
// Push the return address and enter fixed routine $30. (UM0050 p. 230).
OP(op, f7) {
    RST(0x30);
}

// Pop the return address into PC if sign is set. (UM0050 pp. 229-230).
OP(op, f8) {
    RET_COND(f8 & SF, 0xf8);
}
// Copy HL to SP. (UM0050 pp. 222-224).
OP(op, f9) {
    sp16 = hl16;
}
// Set PC to the encoded address if sign is set. (UM0050 pp. 229-230).
OP(op, fa) {
    JP_COND(f8 & SF);
}
// Set both interrupt enables; acceptance waits until the following instruction completes.
// (UM0050 pp. 68-69, 235).
OP(op, fb) {
    EI;
}
// Push the address after this instruction and jump to the encoded address if sign is set.
// (UM0050 pp. 229-230).
OP(op, fc) {
    CALL_COND(f8 & SF, 0xfc);
}
// Fetch the next opcode byte and dispatch the $FD prefix family; include its timing in this
// instruction (UM0050 pp. 247-250).
OP(op, fd) {
    extra_cycles += exec_fd(ROP());
}
// Compare A with the next immediate byte by setting subtraction flags; leave A unchanged.
// (UM0050 p. 213).
OP(op, fe) {
    CP(ARG());
}
// Push the return address and enter fixed routine $38. (UM0050 p. 230).
OP(op, ff) {
    RST(0x38);
}

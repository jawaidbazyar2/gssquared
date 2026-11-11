/*
 *   Direct opcode dispatch for Second Sight's Zilog Z8S180 instruction core.
 *   Included by z180tbl.hpp while defining the six decoder entry points.
 *   The macro selects the named family and calls its byte-specific handler;
 *   handlers mutate the owning Z180 registers and submit timed bus accesses.
 *   Only that CPU's execution thread uses this switch. There is no shared
 *   mutable state in this file.
 *
 *   An opcode is the byte selecting an instruction. A prefix byte selects a
 *   second opcode family: CB bit operations, ED extensions, DD/FD IX/IY index
 *   operations, or DD/FD-CB indexed bit operations. The decoder checks Z180
 *   validity before this switch, so each family lists only the encodings that
 *   the Z180 accepts; the others enter the undefined-instruction trap.
 *
 *   Source for opcode assignment: Zilog UM005004-0918 pp. 247-250.
 *   The explicit switch is a compiler dispatch choice; it adds no hardware
 *   timing. z180tbl.hpp supplies the PHI (processor-clock) counts.
 */

// Valid final bytes for each opcode family: X(prefix, opcode) for every
// encoding that z180tbl.hpp's validity rule accepts (UM0050 pp. 247-250).
#define Z180_VALID_op(X) \
    X(op, 00) X(op, 01) X(op, 02) X(op, 03) X(op, 04) X(op, 05) X(op, 06) X(op, 07) \
    X(op, 08) X(op, 09) X(op, 0a) X(op, 0b) X(op, 0c) X(op, 0d) X(op, 0e) X(op, 0f) \
    X(op, 10) X(op, 11) X(op, 12) X(op, 13) X(op, 14) X(op, 15) X(op, 16) X(op, 17) \
    X(op, 18) X(op, 19) X(op, 1a) X(op, 1b) X(op, 1c) X(op, 1d) X(op, 1e) X(op, 1f) \
    X(op, 20) X(op, 21) X(op, 22) X(op, 23) X(op, 24) X(op, 25) X(op, 26) X(op, 27) \
    X(op, 28) X(op, 29) X(op, 2a) X(op, 2b) X(op, 2c) X(op, 2d) X(op, 2e) X(op, 2f) \
    X(op, 30) X(op, 31) X(op, 32) X(op, 33) X(op, 34) X(op, 35) X(op, 36) X(op, 37) \
    X(op, 38) X(op, 39) X(op, 3a) X(op, 3b) X(op, 3c) X(op, 3d) X(op, 3e) X(op, 3f) \
    X(op, 40) X(op, 41) X(op, 42) X(op, 43) X(op, 44) X(op, 45) X(op, 46) X(op, 47) \
    X(op, 48) X(op, 49) X(op, 4a) X(op, 4b) X(op, 4c) X(op, 4d) X(op, 4e) X(op, 4f) \
    X(op, 50) X(op, 51) X(op, 52) X(op, 53) X(op, 54) X(op, 55) X(op, 56) X(op, 57) \
    X(op, 58) X(op, 59) X(op, 5a) X(op, 5b) X(op, 5c) X(op, 5d) X(op, 5e) X(op, 5f) \
    X(op, 60) X(op, 61) X(op, 62) X(op, 63) X(op, 64) X(op, 65) X(op, 66) X(op, 67) \
    X(op, 68) X(op, 69) X(op, 6a) X(op, 6b) X(op, 6c) X(op, 6d) X(op, 6e) X(op, 6f) \
    X(op, 70) X(op, 71) X(op, 72) X(op, 73) X(op, 74) X(op, 75) X(op, 76) X(op, 77) \
    X(op, 78) X(op, 79) X(op, 7a) X(op, 7b) X(op, 7c) X(op, 7d) X(op, 7e) X(op, 7f) \
    X(op, 80) X(op, 81) X(op, 82) X(op, 83) X(op, 84) X(op, 85) X(op, 86) X(op, 87) \
    X(op, 88) X(op, 89) X(op, 8a) X(op, 8b) X(op, 8c) X(op, 8d) X(op, 8e) X(op, 8f) \
    X(op, 90) X(op, 91) X(op, 92) X(op, 93) X(op, 94) X(op, 95) X(op, 96) X(op, 97) \
    X(op, 98) X(op, 99) X(op, 9a) X(op, 9b) X(op, 9c) X(op, 9d) X(op, 9e) X(op, 9f) \
    X(op, a0) X(op, a1) X(op, a2) X(op, a3) X(op, a4) X(op, a5) X(op, a6) X(op, a7) \
    X(op, a8) X(op, a9) X(op, aa) X(op, ab) X(op, ac) X(op, ad) X(op, ae) X(op, af) \
    X(op, b0) X(op, b1) X(op, b2) X(op, b3) X(op, b4) X(op, b5) X(op, b6) X(op, b7) \
    X(op, b8) X(op, b9) X(op, ba) X(op, bb) X(op, bc) X(op, bd) X(op, be) X(op, bf) \
    X(op, c0) X(op, c1) X(op, c2) X(op, c3) X(op, c4) X(op, c5) X(op, c6) X(op, c7) \
    X(op, c8) X(op, c9) X(op, ca) X(op, cb) X(op, cc) X(op, cd) X(op, ce) X(op, cf) \
    X(op, d0) X(op, d1) X(op, d2) X(op, d3) X(op, d4) X(op, d5) X(op, d6) X(op, d7) \
    X(op, d8) X(op, d9) X(op, da) X(op, db) X(op, dc) X(op, dd) X(op, de) X(op, df) \
    X(op, e0) X(op, e1) X(op, e2) X(op, e3) X(op, e4) X(op, e5) X(op, e6) X(op, e7) \
    X(op, e8) X(op, e9) X(op, ea) X(op, eb) X(op, ec) X(op, ed) X(op, ee) X(op, ef) \
    X(op, f0) X(op, f1) X(op, f2) X(op, f3) X(op, f4) X(op, f5) X(op, f6) X(op, f7) \
    X(op, f8) X(op, f9) X(op, fa) X(op, fb) X(op, fc) X(op, fd) X(op, fe) X(op, ff)

#define Z180_VALID_cb(X) \
    X(cb, 00) X(cb, 01) X(cb, 02) X(cb, 03) X(cb, 04) X(cb, 05) X(cb, 06) X(cb, 07) \
    X(cb, 08) X(cb, 09) X(cb, 0a) X(cb, 0b) X(cb, 0c) X(cb, 0d) X(cb, 0e) X(cb, 0f) \
    X(cb, 10) X(cb, 11) X(cb, 12) X(cb, 13) X(cb, 14) X(cb, 15) X(cb, 16) X(cb, 17) \
    X(cb, 18) X(cb, 19) X(cb, 1a) X(cb, 1b) X(cb, 1c) X(cb, 1d) X(cb, 1e) X(cb, 1f) \
    X(cb, 20) X(cb, 21) X(cb, 22) X(cb, 23) X(cb, 24) X(cb, 25) X(cb, 26) X(cb, 27) \
    X(cb, 28) X(cb, 29) X(cb, 2a) X(cb, 2b) X(cb, 2c) X(cb, 2d) X(cb, 2e) X(cb, 2f) \
    X(cb, 38) X(cb, 39) X(cb, 3a) X(cb, 3b) X(cb, 3c) X(cb, 3d) X(cb, 3e) X(cb, 3f) \
    X(cb, 40) X(cb, 41) X(cb, 42) X(cb, 43) X(cb, 44) X(cb, 45) X(cb, 46) X(cb, 47) \
    X(cb, 48) X(cb, 49) X(cb, 4a) X(cb, 4b) X(cb, 4c) X(cb, 4d) X(cb, 4e) X(cb, 4f) \
    X(cb, 50) X(cb, 51) X(cb, 52) X(cb, 53) X(cb, 54) X(cb, 55) X(cb, 56) X(cb, 57) \
    X(cb, 58) X(cb, 59) X(cb, 5a) X(cb, 5b) X(cb, 5c) X(cb, 5d) X(cb, 5e) X(cb, 5f) \
    X(cb, 60) X(cb, 61) X(cb, 62) X(cb, 63) X(cb, 64) X(cb, 65) X(cb, 66) X(cb, 67) \
    X(cb, 68) X(cb, 69) X(cb, 6a) X(cb, 6b) X(cb, 6c) X(cb, 6d) X(cb, 6e) X(cb, 6f) \
    X(cb, 70) X(cb, 71) X(cb, 72) X(cb, 73) X(cb, 74) X(cb, 75) X(cb, 76) X(cb, 77) \
    X(cb, 78) X(cb, 79) X(cb, 7a) X(cb, 7b) X(cb, 7c) X(cb, 7d) X(cb, 7e) X(cb, 7f) \
    X(cb, 80) X(cb, 81) X(cb, 82) X(cb, 83) X(cb, 84) X(cb, 85) X(cb, 86) X(cb, 87) \
    X(cb, 88) X(cb, 89) X(cb, 8a) X(cb, 8b) X(cb, 8c) X(cb, 8d) X(cb, 8e) X(cb, 8f) \
    X(cb, 90) X(cb, 91) X(cb, 92) X(cb, 93) X(cb, 94) X(cb, 95) X(cb, 96) X(cb, 97) \
    X(cb, 98) X(cb, 99) X(cb, 9a) X(cb, 9b) X(cb, 9c) X(cb, 9d) X(cb, 9e) X(cb, 9f) \
    X(cb, a0) X(cb, a1) X(cb, a2) X(cb, a3) X(cb, a4) X(cb, a5) X(cb, a6) X(cb, a7) \
    X(cb, a8) X(cb, a9) X(cb, aa) X(cb, ab) X(cb, ac) X(cb, ad) X(cb, ae) X(cb, af) \
    X(cb, b0) X(cb, b1) X(cb, b2) X(cb, b3) X(cb, b4) X(cb, b5) X(cb, b6) X(cb, b7) \
    X(cb, b8) X(cb, b9) X(cb, ba) X(cb, bb) X(cb, bc) X(cb, bd) X(cb, be) X(cb, bf) \
    X(cb, c0) X(cb, c1) X(cb, c2) X(cb, c3) X(cb, c4) X(cb, c5) X(cb, c6) X(cb, c7) \
    X(cb, c8) X(cb, c9) X(cb, ca) X(cb, cb) X(cb, cc) X(cb, cd) X(cb, ce) X(cb, cf) \
    X(cb, d0) X(cb, d1) X(cb, d2) X(cb, d3) X(cb, d4) X(cb, d5) X(cb, d6) X(cb, d7) \
    X(cb, d8) X(cb, d9) X(cb, da) X(cb, db) X(cb, dc) X(cb, dd) X(cb, de) X(cb, df) \
    X(cb, e0) X(cb, e1) X(cb, e2) X(cb, e3) X(cb, e4) X(cb, e5) X(cb, e6) X(cb, e7) \
    X(cb, e8) X(cb, e9) X(cb, ea) X(cb, eb) X(cb, ec) X(cb, ed) X(cb, ee) X(cb, ef) \
    X(cb, f0) X(cb, f1) X(cb, f2) X(cb, f3) X(cb, f4) X(cb, f5) X(cb, f6) X(cb, f7) \
    X(cb, f8) X(cb, f9) X(cb, fa) X(cb, fb) X(cb, fc) X(cb, fd) X(cb, fe) X(cb, ff)

#define Z180_VALID_dd(X) \
    X(dd, 09) X(dd, 19) X(dd, 21) X(dd, 22) X(dd, 23) X(dd, 29) X(dd, 2a) X(dd, 2b) \
    X(dd, 34) X(dd, 35) X(dd, 36) X(dd, 39) X(dd, 46) X(dd, 4e) X(dd, 56) X(dd, 5e) \
    X(dd, 66) X(dd, 6e) X(dd, 70) X(dd, 71) X(dd, 72) X(dd, 73) X(dd, 74) X(dd, 75) \
    X(dd, 77) X(dd, 7e) X(dd, 86) X(dd, 8e) X(dd, 96) X(dd, 9e) X(dd, a6) X(dd, ae) \
    X(dd, b6) X(dd, be) X(dd, cb) X(dd, e1) X(dd, e3) X(dd, e5) X(dd, e9) X(dd, f9)

#define Z180_VALID_fd(X) \
    X(fd, 09) X(fd, 19) X(fd, 21) X(fd, 22) X(fd, 23) X(fd, 29) X(fd, 2a) X(fd, 2b) \
    X(fd, 34) X(fd, 35) X(fd, 36) X(fd, 39) X(fd, 46) X(fd, 4e) X(fd, 56) X(fd, 5e) \
    X(fd, 66) X(fd, 6e) X(fd, 70) X(fd, 71) X(fd, 72) X(fd, 73) X(fd, 74) X(fd, 75) \
    X(fd, 77) X(fd, 7e) X(fd, 86) X(fd, 8e) X(fd, 96) X(fd, 9e) X(fd, a6) X(fd, ae) \
    X(fd, b6) X(fd, be) X(fd, cb) X(fd, e1) X(fd, e3) X(fd, e5) X(fd, e9) X(fd, f9)

#define Z180_VALID_ed(X) \
    X(ed, 00) X(ed, 01) X(ed, 04) X(ed, 08) X(ed, 09) X(ed, 0c) X(ed, 10) X(ed, 11) \
    X(ed, 14) X(ed, 18) X(ed, 19) X(ed, 1c) X(ed, 20) X(ed, 21) X(ed, 24) X(ed, 28) \
    X(ed, 29) X(ed, 2c) X(ed, 30) X(ed, 34) X(ed, 38) X(ed, 39) X(ed, 3c) X(ed, 40) \
    X(ed, 41) X(ed, 42) X(ed, 43) X(ed, 44) X(ed, 45) X(ed, 46) X(ed, 47) X(ed, 48) \
    X(ed, 49) X(ed, 4a) X(ed, 4b) X(ed, 4c) X(ed, 4d) X(ed, 4f) X(ed, 50) X(ed, 51) \
    X(ed, 52) X(ed, 53) X(ed, 56) X(ed, 57) X(ed, 58) X(ed, 59) X(ed, 5a) X(ed, 5b) \
    X(ed, 5c) X(ed, 5e) X(ed, 5f) X(ed, 60) X(ed, 61) X(ed, 62) X(ed, 63) X(ed, 64) \
    X(ed, 67) X(ed, 68) X(ed, 69) X(ed, 6a) X(ed, 6b) X(ed, 6c) X(ed, 6f) X(ed, 70) \
    X(ed, 72) X(ed, 73) X(ed, 74) X(ed, 76) X(ed, 78) X(ed, 79) X(ed, 7a) X(ed, 7b) \
    X(ed, 7c) X(ed, 83) X(ed, 8b) X(ed, 93) X(ed, 9b) X(ed, a0) X(ed, a1) X(ed, a2) \
    X(ed, a3) X(ed, a8) X(ed, a9) X(ed, aa) X(ed, ab) X(ed, b0) X(ed, b1) X(ed, b2) \
    X(ed, b3) X(ed, b8) X(ed, b9) X(ed, ba) X(ed, bb)

#define Z180_VALID_xycb(X) \
    X(xycb, 06) X(xycb, 0e) X(xycb, 16) X(xycb, 1e) X(xycb, 26) X(xycb, 2e) X(xycb, 3e) X(xycb, 46) \
    X(xycb, 4e) X(xycb, 56) X(xycb, 5e) X(xycb, 66) X(xycb, 6e) X(xycb, 76) X(xycb, 7e) X(xycb, 86) \
    X(xycb, 8e) X(xycb, 96) X(xycb, 9e) X(xycb, a6) X(xycb, ae) X(xycb, b6) X(xycb, be) X(xycb, c6) \
    X(xycb, ce) X(xycb, d6) X(xycb, de) X(xycb, e6) X(xycb, ee) X(xycb, f6) X(xycb, fe)

// Call the handler for one accepted final byte. Each case returns through the
// enclosing decoder, which supplies timing; no case performs a second fetch.
#define Z180_DISPATCH_CASE(prefix, opcode) \
    case 0x##opcode:                       \
        prefix##_##opcode();               \
        break;
#define Z180_DIRECT_DISPATCH(prefix)                  \
    switch (opcode_arg) {                             \
        Z180_VALID_##prefix(Z180_DISPATCH_CASE)       \
    default:                                          \
        break;                                        \
    }

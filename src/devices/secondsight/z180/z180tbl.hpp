// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   Instruction decoder and timing tables for Second Sight's Zilog Z8S180.
 *   Z180.cpp includes these tables and macros before the opcode bodies.
 *   The decoder validates an opcode, calls its handler, and returns nominal
 *   clock cost. The bus engine uses the same table to place internal clocks
 *   around memory/port cycles. Tables are immutable and shared; decoder
 *   methods use only their owning CPU's execution thread.
 *
 *   An opcode is an instruction byte. A prefix selects its family: op is
 *   unprefixed, cb bit operations, ed extensions, xy the IX/IY index-register
 *   families, and xycb indexed bit operations. PHI is the CPU clock; a T-state
 *   is one PHI. cc values count whole PHI, excluding inserted wait clocks.
 *   ex supplies conditional/repeat additions. Handler rows use dd/fd separately;
 *   timing rows share xy. A trap enters address zero for an undefined opcode.
 *
 *   Sources: Zilog UM005004-0918 pp. 237-246 (timing summary), pp. 247-250
 *   (valid opcodes), pp. 251-280 (bus sequence), pp. 68-72 (trap). The block-I/O
 *   parity tables implement undocumented flag behavior; they do not
 *   represent additional registers. They come from the MAME Z180 core:
 *   https://github.com/mamedev/mame/blob/master/src/devices/cpu/z180/z180tbl.h
 */

// Parity contribution for incrementing block I/O: indexed by the low two
// bits of C and of the transferred byte.
static const uint8_t irep_tmp1[4][4] = {{0, 0, 1, 0}, {0, 1, 0, 1}, {1, 0, 1, 1}, {0, 1, 1, 0}};

// The corresponding parity contribution for decrementing block I/O.
static const uint8_t drep_tmp1[4][4] = {{0, 1, 0, 0}, {1, 0, 0, 1}, {0, 0, 1, 0}, {0, 1, 0, 1}};

// Parity contribution indexed by the eight-bit B count after decrement.
static const uint8_t breg_tmp2[256] = {
    0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1,
    0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0,
    1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1,
    1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1,
    0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1,
    0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1};

// Each 256-entry table is indexed by the final opcode byte. Zero means a
// prefix handler supplies the duration from another row, not a zero-time
// instruction. Low nibble runs across each logical group of sixteen entries.
static const uint8_t cc_op[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    3, 9, 7,  4,  4, 4,  6, 3,  4, 7, 6,  4, 4, 4,  6, 3,  7, 9, 7,  4,  4,  4,  6, 3,  8, 7, 6,  4, 4, 4, 6, 3,
    6, 9, 16, 4,  4, 4,  6, 4,  6, 7, 15, 4, 4, 4,  6, 3,  6, 9, 13, 4,  10, 10, 9, 3,  6, 7, 12, 4, 4, 4, 6, 3,
    4, 4, 4,  4,  4, 4,  6, 4,  4, 4, 4,  4, 4, 4,  6, 4,  4, 4, 4,  4,  4,  4,  6, 4,  4, 4, 4,  4, 4, 4, 6, 4,
    4, 4, 4,  4,  4, 4,  6, 4,  4, 4, 4,  4, 4, 4,  6, 4,  7, 7, 7,  7,  7,  7,  3, 7,  4, 4, 4,  4, 4, 4, 6, 4,
    4, 4, 4,  4,  4, 4,  6, 4,  4, 4, 4,  4, 4, 4,  6, 4,  4, 4, 4,  4,  4,  4,  6, 4,  4, 4, 4,  4, 4, 4, 6, 4,
    4, 4, 4,  4,  4, 4,  6, 4,  4, 4, 4,  4, 4, 4,  6, 4,  4, 4, 4,  4,  4,  4,  6, 4,  4, 4, 4,  4, 4, 4, 6, 4,
    5, 9, 6,  9,  6, 11, 6, 11, 5, 9, 6,  0, 6, 16, 6, 11, 5, 9, 6,  10, 6,  11, 6, 11, 5, 3, 6,  9, 6, 0, 6, 11,
    5, 9, 6,  16, 6, 11, 6, 11, 5, 3, 6,  3, 6, 0,  6, 11, 5, 9, 6,  3,  6,  11, 6, 11, 5, 4, 6,  3, 6, 0, 6, 11};

// CB: rotate, shift and bit operations, including the prefix fetch.
static const uint8_t cc_cb[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7,
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7,
    6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6,
    6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6, 6, 6, 6, 6, 6, 6, 9,  6,
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7,
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7,
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7,
    7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7, 7, 7, 7, 7, 7, 7, 13, 7};

// ED: extended operations. Invalid encodings are intercepted by valid_ed.
static const uint8_t cc_ed[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    12, 13, 6,  6,  7,  6,  6,  6,  12, 13, 6,  6,  7,  6,  6,  6,  12, 13, 6,  6,  7,  6,  6,  6,  12, 13, 6,  6,  7,
    6,  6,  6,  12, 13, 6,  6,  7,  6,  6,  6,  12, 13, 6,  6,  7,  6,  6,  6,  12, 13, 6,  6,  10, 6,  6,  6,  12, 13,
    6,  6,  7,  6,  6,  6,  9,  10, 10, 19, 6,  12, 6,  6,  9,  10, 10, 18, 17, 12, 6,  6,  9,  10, 10, 19, 6,  12, 6,
    6,  9,  10, 10, 18, 17, 12, 6,  6,  9,  10, 10, 19, 9,  12, 6,  16, 9,  10, 10, 18, 17, 12, 6,  16, 9,  10, 10, 19,
    12, 12, 8,  6,  9,  10, 10, 18, 17, 12, 6,  6,  6,  6,  6,  14, 6,  6,  6,  6,  6,  6,  6,  14, 6,  6,  6,  6,  6,
    6,  6,  14, 6,  6,  6,  6,  6,  6,  6,  14, 6,  6,  6,  6,  12, 12, 12, 12, 6,  6,  6,  6,  12, 12, 12, 12, 6,  6,
    6,  6,  12, 12, 12, 12, 6,  6,  6,  6,  12, 12, 12, 12, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6};

// DD/FD: shared timing for IX/IY instructions. A CB prefix delegates again.
static const uint8_t cc_xy[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    4, 4,  4,  4,  4, 4,  4,  4, 4, 10, 4,  4, 4, 4, 4,  4, 4,  4,  4,  4,  4,  4,  4,  4,  4, 10, 4, 4, 4, 4, 4,  4,
    4, 12, 19, 7,  9, 9,  15, 4, 4, 10, 18, 7, 9, 9, 9,  4, 4,  4,  4,  4,  18, 18, 15, 4,  4, 10, 4, 4, 4, 4, 4,  4,
    4, 4,  4,  4,  9, 9,  14, 4, 4, 4,  4,  4, 9, 9, 14, 4, 4,  4,  4,  4,  9,  9,  14, 4,  4, 4,  4, 4, 9, 9, 14, 4,
    9, 9,  9,  9,  9, 9,  14, 9, 9, 9,  9,  9, 9, 9, 14, 9, 15, 15, 15, 15, 15, 15, 4,  15, 4, 4,  4, 4, 9, 9, 14, 4,
    4, 4,  4,  4,  9, 9,  14, 4, 4, 4,  4,  4, 9, 9, 14, 4, 4,  4,  4,  4,  9,  9,  14, 4,  4, 4,  4, 4, 9, 9, 14, 4,
    4, 4,  4,  4,  9, 9,  14, 4, 4, 4,  4,  4, 9, 9, 14, 4, 4,  4,  4,  4,  9,  9,  14, 4,  4, 4,  4, 4, 9, 9, 14, 4,
    4, 4,  4,  4,  4, 4,  4,  4, 4, 4,  4,  0, 4, 4, 4,  4, 4,  4,  4,  4,  4,  4,  4,  4,  4, 4,  4, 4, 4, 4, 4,  4,
    4, 12, 4,  19, 4, 14, 4,  4, 4, 6,  4,  4, 4, 4, 4,  4, 4,  4,  4,  4,  4,  4,  4,  4,  4, 7,  4, 4, 4, 4, 4,  4};

// Indexed CB: displacement and final opcode fetch are included in these totals.
static const uint8_t cc_xycb[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19};

/* extra cycles if jr/jp/call taken and 'interrupt latency' on rst 0-7 */
// Extra PHI for taken branches, repeated block steps and interrupt-related calls.
static const uint8_t cc_ex[0x100] = {
    /*-0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -a -b -c -d -e -f */
    0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0, 0, 2, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0, 0, /* DJNZ */
    2, 0, 0, 0,  0,  0, 0, 0, 2, 0, 0, 0,  0,  0,  0, 0, /* JR NZ/JR Z */
    2, 0, 0, 0,  0,  0, 0, 0, 2, 0, 0, 0,  0,  0,  0, 0, /* JR NC/JR C */
    0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  10, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  10, 0, 0, 0,
    0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  10, 0,  0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  10, 0,  0, 0, 0,
    0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,  0, 0, 0,
    0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  2,  2, 2, 2, 0, 0, 0,  0,  2, 2, 2, 2, 0, 0, 0,  0, /* UM0050
                                                                                                      pp238,241-242:
                                                                                                      repeat adds two
                                                                                                      states */
    5, 0, 3, 0,  10, 0, 0, 2, 5, 0, 3, 0,  10, 0,  0, 2, 5, 0, 3, 0,  10, 0, 0, 2, 5, 0, 3, 0,  10, 0,  0, 2, 5,
    0, 3, 0, 10, 0,  0, 2, 5, 0, 3, 0, 10, 0,  0,  2, 5, 0, 3, 0, 10, 0,  0, 2, 5, 0, 3, 0, 10, 0,  0,  2};

static const uint8_t *const cc_default[6] = {cc_op, cc_cb, cc_ed, cc_xy, cc_xycb, cc_ex};

#define Z180_TABLE_dd Z180_TABLE_xy
#define Z180_TABLE_fd Z180_TABLE_xy

/***************************************************************
 * define an opcode function
 ***************************************************************/
#define OP(prefix, opcode_arg) inline void Z180::prefix##_##opcode_arg()

/***************************************************************
 * adjust cycle count by n T-states
 ***************************************************************/
#define CC(prefix, opcode_arg) extra_cycles += cc[Z180_TABLE_##prefix][opcode_arg]

/***************************************************************
 * execute an opcode
 ***************************************************************/

// Defined opcode map, UM0050 Tables 48-50 (pp248-250). Z80 aliases,
// SLL, IXH/IXL/IYH/IYL and indexed-CB register destinations trap on Z180.
// Sixteen bitsets: high opcode nibble selects the word, low nibble selects
// its bit. One permits execution, zero enters the undefined-instruction trap.
static constexpr uint16_t valid_ed[16] = {0x1313, 0x1313, 0x1313, 0x1311, 0xbfff, 0xdfcf, 0x9f9f, 0x1f5d,
                                          0x0808, 0x0808, 0x0f0f, 0x0f0f, 0,      0,      0,      0};
static constexpr uint16_t valid_index[16] = {0x0200, 0x0200, 0x0e0e, 0x0270, 0x4040, 0x4040, 0x4040, 0x40bf,
                                             0x4040, 0x4040, 0x4040, 0x4040, 0x0800, 0,      0x022a, 0x0200};

#include "z180dispatch.hxx"

// Define a decoder returning nominal PHI for opcode_arg. valid is the
// silicon validity rule; third selects the indexed third-byte trap sequence.
#define EXEC_PROTOTYPE(prefix, valid, third)                                                                           \
    int Z180::exec##_##prefix(const uint8_t opcode_arg) {                                                              \
        cycle_decode(Z180_TABLE_##prefix, opcode_arg);                                                                 \
        if (!(valid))                                                                                                  \
            return take_trap(third);                                                                                   \
        Z180_DIRECT_DISPATCH(prefix)                                                                                   \
        return cc[Z180_TABLE_##prefix][opcode_arg];                                                                    \
    }

// op: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(op, true, false)
// cb: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(cb, (opcode_arg & 0xf8) != 0x30, false)
// dd: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(dd, (valid_index[opcode_arg >> 4] >> (opcode_arg & 15)) & 1, false)
// ed: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(ed, (valid_ed[opcode_arg >> 4] >> (opcode_arg & 15)) & 1, false)
// fd: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(fd, (valid_index[opcode_arg >> 4] >> (opcode_arg & 15)) & 1, false)
// xycb: validate this family, execute it, and return its nominal timing.
EXEC_PROTOTYPE(xycb, (opcode_arg & 7) == 6 && opcode_arg != 0x36, true)

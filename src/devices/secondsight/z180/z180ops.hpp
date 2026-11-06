// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   Shared operations for Second Sight's Zilog Z8S180 instruction handlers.
 *   Z180.cpp includes this file before the opcode files. Every function and
 *   macro works on the owning Z180 instance on its execution thread. They
 *   read operands, update registers/flags, and submit memory/port accesses to
 *   the resumable bus scheduler; they do not call host memory directly.
 *
 *   PC is the program counter, SP the descending stack pointer, A the byte
 *   accumulator, F its flags. BC/DE/HL are register pairs; IX/IY are address
 *   index registers. I supplies interrupt-vector high bits and R counts
 *   opcode fetches. IFF1/2 hold interrupt enables. MMU means memory management
 *   unit: it translates 16-bit program addresses to 20-bit physical addresses.
 *   PHI is the CPU clock; a T-state is one PHI. A machine cycle is one bus or
 *   internal operation, possibly spanning several T-states.
 *
 *   CF is carry, NF marks subtraction, HF is carry across bit 3 (bit 11 for
 *   16-bit arithmetic), ZF is zero and SF sign. PF/VF share one bit for parity
 *   or signed overflow. XF/YF are undocumented flag bits 3/5; their values
 *   depend on the operation. SZ, SZP
 *   and SZHV tables produce sign/zero, parity,
 *   and increment/decrement flags from a byte result. A nibble is four bits.
 *
 *   Sources: Zilog UM005004-0918 pp. 55-64 (MMU), 68-85 (interrupts/traps),
 *   173-183 (registers), 211-236 (operations), 251-280 (bus order). The macros
 *   preserve the instruction helper interface; their named arguments are
 *   operands, not independent hardware devices. Undocumented flag behavior
 *   retains the MAME Z180 instruction helpers:
 *   https://github.com/mamedev/mame/blob/master/src/devices/cpu/z180/z180ops.h
 */

// Keep PC on HALT so the idle engine can repeat its fetch timing; interrupt
// exit advances past it (UM0050 pp. 32-33).
#define ENTER_HALT()                                                                                                   \
    {                                                                                                                  \
        pc16--;                                                                                                        \
        HALT = 1;                                                                                                      \
    }

// Advance PC past the stopped instruction: HALT is one byte, SLP two.
// Clear the stopped-state marker when an interrupt/wake resumes execution (pp. 33-36).
#define LEAVE_HALT()                                                                                                   \
    {                                                                                                                  \
        if (HALT) {                                                                                                    \
            if (HALT == 2) {                                                                                           \
                pc16 += 2;                                                                                             \
            } else {                                                                                                   \
                pc16++;                                                                                                \
            }                                                                                                          \
            HALT = 0;                                                                                                  \
        }                                                                                                              \
    }

// Read one byte from 16-bit port. Decode the internal register window
// before external board I/O; machine-cycle execution retains sampling order
// and waits (UM0050 pp. 23, 42-48, 231). Return the sampled byte.
inline uint8_t Z180::IN(uint16_t port) {
    if (cycle_executing)
        return cycle_access(cycle_kind::input, port);
    if (is_internal_io_address(port))
        return z180_readcontrol(port);
    extra_cycles += io_wait_states();
    return read_io(port);
}

// Write value to 16-bit port with internal-register decode and port-cycle
// timing. No byte is returned (UM0050 pp. 23, 42-48, 231).
inline void Z180::OUT(uint16_t port, uint8_t value) {
    if (cycle_executing) {
        cycle_access(cycle_kind::output, port, value);
        return;
    }
    if (is_internal_io_address(port))
        z180_writecontrol(port, value);
    else {
        extra_cycles += io_wait_states();
        write_io(port, value);
    }
}

// Rebuild sixteen 4-KiB translated page bases. CBAR selects the bank/common
// boundaries; BBR/CBR add page offsets above them. Mask to 20 bits, then
// refresh direct-read views (UM0050 pp. 58-64).
void Z180::z180_mmu() {
    uint32_t addr, page, bb, cb;
    bb = mmu_cbar & 15;
    cb = mmu_cbar >> 4;
    for (page = 0; page < 16; page++) {
        addr = page << 12;
        if (page >= bb) {
            if (page >= cb)
                addr += (mmu_cbr << 12);
            else
                addr += (mmu_bbr << 12);
        }
        mmu[page] = (addr & 0xfffff);
    }
    cycle_update_direct_reads();
}

// Translate addr by its 4-KiB page base and unchanged low 12 bits (UM0050 pp. 58-64).
#define MMU_REMAP_ADDR(addr) (mmu[((addr) >> 12) & 15] | ((addr) & 4095))

#include "z180cycles_inline.hxx"

// Read and return a byte at logical address addr modulo 64 KiB. Translate
// through the MMU and retain bus waits/sample timing (UM0050 pp. 21-22, 58-64).
inline uint8_t Z180::RM(uint32_t addr) {
    if (cycle_executing)
        return cycle_access(cycle_kind::read, uint16_t(addr));
    extra_cycles += memory_wait_states();
    return z180_read_memory(MMU_REMAP_ADDR(addr));
}

// Write data at logical address addr modulo 64 KiB through the same MMU
// and bus timing path as RM (UM0050 pp. 21-22, 58-64).
inline void Z180::write_mem(uint32_t addr, uint8_t data) {
    if (cycle_executing) {
        cycle_access(cycle_kind::write, uint16_t(addr), data);
        return;
    }
    extra_cycles += memory_wait_states();
    z180_write_memory(MMU_REMAP_ADDR(addr), data);
}
// Opcode macro spelling for write_mem; value is the outgoing byte at logical addr.
#define WM(addr, value) write_mem(addr, value)

// Read low then high bytes at logical addr and addr+1 into r. Each read
// is a distinct cycle; address wrap occurs at 64 KiB (UM0050 pp. 258-261).
void Z180::RM16(uint32_t addr, Z180Register *r) {
    r->b.l = RM(addr);
    r->b.h = RM(addr + 1);
}

// Write r low then high bytes to logical addr and addr+1 as distinct
// cycles. Stack pushes use their own high-first order (UM0050 pp. 258-261, 272-273).
void Z180::WM16(uint32_t addr, Z180Register *r) {
    WM(addr, r->b.l);
    WM(addr + 1, r->b.h);
}

// Fetch and return the instruction byte at PC, then advance PC and R.
// Opcode sampling occurs earlier in T3 than ordinary data (UM0050 pp. 19-22, 177).
uint8_t Z180::ROP() {
    ++R; // UM0050 p177: once per opcode_arg fetch, not per prefix handler.
    uint32_t addr = pc32;
    pc16++;
    if (cycle_executing)
        return cycle_access(cycle_kind::fetch, uint16_t(addr));
    extra_cycles += memory_wait_states();
    return read_memory(MMU_REMAP_ADDR(addr));
}

// Read and return the next instruction operand byte, advancing PC without
// advancing R. Operand reads use memory-data timing (UM0050 pp. 177, 251-278).
uint8_t Z180::ARG() {
    uint32_t addr = pc32;
    pc16++;
    if (cycle_executing)
        return cycle_access(cycle_kind::operand, uint16_t(addr));
    extra_cycles += memory_wait_states();
    return read_memory(MMU_REMAP_ADDR(addr));
}

// Read two consecutive instruction operands, low byte first, and return
// the 16-bit word. Separate calls preserve cycle ordering and PC wrap.
uint32_t Z180::ARG16() {
    if (cycle_executing) {
        uint8_t const low = ARG();
        return low | (uint16_t(ARG()) << 8);
    }
    uint32_t addr = pc32;
    pc16 += 2;
    extra_cycles += memory_wait_states() * 2;
    return read_memory(MMU_REMAP_ADDR(addr)) | (read_memory(MMU_REMAP_ADDR(addr + 1)) << 8);
}

// Read a signed displacement and add it to IX, wrapping at 16 bits; ea is
// the resulting logical address (UM0050 p. 182).
#define EAX() ea = (uint32_t)(uint16_t)(ix16 + (int8_t)ARG())
// Read a signed displacement and add it to IY with 16-bit wrap (UM0050 p. 182).
#define EAY() ea = (uint32_t)(uint16_t)(iy16 + (int8_t)ARG())

// Read the stack word into register pair DR, then increase SP by two (UM0050 p. 227).
#define POP(DR)                                                                                                        \
    {                                                                                                                  \
        RM16(sp32, &DR);                                                                                               \
        sp16 += 2;                                                                                                     \
    }

// Decrement SP and write register pair SR high byte, then decrement and
// write its low byte. Bus order matters to mapped devices (UM0050 pp. 272-273).
#define PUSH(SR)                                                                                                       \
    {                                                                                                                  \
        --sp16;                                                                                                        \
        WM(sp32, SR.b.h);                                                                                              \
        --sp16;                                                                                                        \
        WM(sp32, SR.b.l);                                                                                              \
    } // UM0050 pp272-273: high byte first

// Read the encoded destination word and put it in PC (UM0050 p. 229).
#define JP                                                                                                             \
    {                                                                                                                  \
        pc32 = ARG16();                                                                                                \
    }


// Take the absolute jump only when cond is true. A false conditional jump
// reads the low operand but skips the high-byte read; PC still advances over
// both bytes. Taken timing adds three PHI (UM0050 pp. 239-240, 257).
#define JP_COND(cond)                                                                                                  \
    if (cond) {                                                                                                        \
        pc32 = ARG16();                                                                                                \
        extra_cycles += 3; /* UM0050 pp239-240: JP cc taken */                                                         \
    } else {                                                                                                           \
        (void)ARG();                                                                                                   \
        pc16++;                                                                                                        \
    }

// Read a signed byte and add it to the already advanced PC (UM0050 p. 229).
#define JR()                                                                                                           \
    {                                                                                                                  \
        int8_t arg = (int8_t)ARG(); /* ARG() also increments pc16 */                                                   \
        pc16 += arg;                /* so don't do pc16 += ARG() */                                                    \
    }

// Read a signed displacement regardless of cond. If true, branch and add
// the taken-path timing selected by opcode_arg (UM0050 pp. 229-230, 257).
#define JR_COND(cond, opcode_arg)                                                                                      \
    {                                                                                                                  \
        int8_t arg = (int8_t)ARG(); /* ARG() also increments pc16 */                                                   \
        if (cond) {                                                                                                    \
            pc16 += arg; /* so don't do pc16 += ARG() */                                                               \
            CC(ex, opcode_arg);                                                                                        \
        }                                                                                                              \
    }
// Fetch the destination, push the following PC high-byte first and enter
// the subroutine (UM0050 pp. 230, 252-253).
#define CALL()                                                                                                         \
    ea = ARG16();                                                                                                      \
    PUSH(PC);                                                                                                          \
    pc32 = ea;

// Call only when cond is true; opcode_arg selects its extra PHI. A false
// condition skips the high operand read as specified in Table 51 (UM0050 pp. 252-253).
#define CALL_COND(cond, opcode_arg)                                                                                    \
    if (cond) {                                                                                                        \
        ea = ARG16();                                                                                                  \
        PUSH(PC);                                                                                                      \
        pc32 = ea;                                                                                                     \
        CC(ex, opcode_arg);                                                                                            \
    } else {                                                                                                           \
        (void)ARG();                                                                                                   \
        pc16++;                                                                                                        \
    }

// Pop PC only when cond is true and add opcode_arg taken timing (UM0050 pp. 230, 273).
#define RET_COND(cond, opcode_arg)                                                                                     \
    if (cond) {                                                                                                        \
        POP(PC);                                                                                                       \
        CC(ex, opcode_arg);                                                                                            \
    }

// Return from NMI: pop PC and restore the saved interrupt enable (UM0050 pp. 73-75).
#define RETN                                                                                                           \
    {                                                                                                                  \
                                                                                                                       \
        POP(PC);                                                                                                       \
        IFF1 = IFF2;                                                                                                   \
    }

// Z8S180 DS006002 p67: M1E=0 refetches RETI with Z80 bus timing.
// UM0050 pp84-85 gives 22 states; the ordinary Z8S180 path takes 12.
// Return from a maskable interrupt and notify the board once. M1-compatible
// mode repeats the opcode fetches before popping PC (UM0050 pp. 84-85).
#define RETI                                                                                                           \
    {                                                                                                                  \
        if (!(omcr & Z180_OMCR_M1E)) {                                                                                 \
            pc16 -= 2;                                                                                                 \
            ROP();                                                                                                     \
            ROP();                                                                                                     \
            extra_cycles += 10;                                                                                        \
        }                                                                                                              \
        POP(PC);                                                                                                       \
        cycle_reti();                                                                                                  \
    }

// Copy A to R, retaining bit 7 separately because refresh increments only
// the low seven bits (UM0050 pp. 177, 224).
#define LD_R_A                                                                                                         \
    {                                                                                                                  \
        R = a8;                                                                                                        \
        R2 = a8 & 0x80; /* keep bit 7 of R */                                                                          \
    }

// Read R with its separately retained bit 7; parity/overflow reflects IFF2 (UM0050 p. 224).
#define LD_A_R                                                                                                         \
    {                                                                                                                  \
        a8 = (R & 0x7f) | R2;                                                                                          \
        f8 = (f8 & CF) | SZ[a8] | (IFF2 << 2);                                                                         \
    }

// Set the interrupt-vector high byte I from A without flag changes (UM0050 p. 224).
#define LD_I_A                                                                                                         \
    {                                                                                                                  \
        I = a8;                                                                                                        \
    }

// Copy I to A; preserve carry and put IFF2 in parity/overflow (UM0050 p. 224).
#define LD_A_I                                                                                                         \
    {                                                                                                                  \
        a8 = I;                                                                                                        \
        f8 = (f8 & CF) | SZ[a8] | (IFF2 << 2);                                                                         \
    }

// Push PC and branch to fixed vector addr (UM0050 p. 230).
#define RST(addr)                                                                                                      \
    PUSH(PC);                                                                                                          \
    pc32 = addr;

// Return value+1 with 8-bit wrap. Set result/half-carry/overflow flags and
// preserve carry; 0x7f to 0x80 is signed overflow (UM0050 p. 213).
uint8_t Z180::INC(uint8_t value) {
    uint8_t res = value + 1;
    f8 = (f8 & CF) | SZHV_inc[res];
    return (uint8_t)res;
}

// Return value-1 with 8-bit wrap. Set subtract/result/half-borrow/overflow
// flags and preserve carry; 0x80 to 0x7f overflows (UM0050 p. 213).
uint8_t Z180::DEC(uint8_t value) {
    uint8_t res = value - 1;
    f8 = (f8 & CF) | SZHV_dec[res];
    return res;
}

// Rotate A left with end-around carry; preserve sign/zero/parity (UM0050 p. 216).
#define RLCA                                                                                                           \
    a8 = (a8 << 1) | (a8 >> 7);                                                                                        \
    f8 = (f8 & (SF | ZF | PF)) | (a8 & (YF | XF | CF))

// Rotate A right with end-around carry; preserve sign/zero/parity (UM0050 p. 216).
#define RRCA                                                                                                           \
    f8 = (f8 & (SF | ZF | PF)) | (a8 & (YF | XF | CF));                                                                \
    a8 = (a8 >> 1) | (a8 << 7)

// Rotate A left through carry, preserving sign/zero/parity (UM0050 p. 216).
#define RLA                                                                                                            \
    {                                                                                                                  \
        uint8_t res = (a8 << 1) | (f8 & CF);                                                                           \
        uint8_t c = (a8 & 0x80) ? CF : 0;                                                                              \
        f8 = (f8 & (SF | ZF | PF)) | c | (res & (YF | XF));                                                            \
        a8 = res;                                                                                                      \
    }

// Rotate A right through carry, preserving sign/zero/parity (UM0050 p. 216).
#define RRA                                                                                                            \
    {                                                                                                                  \
        uint8_t res = (a8 >> 1) | (f8 << 7);                                                                           \
        uint8_t c = (a8 & 0x01) ? CF : 0;                                                                              \
        f8 = (f8 & (SF | ZF | PF)) | c | (res & (YF | XF));                                                            \
        a8 = res;                                                                                                      \
    }

// Rotate the three low decimal nibbles right between A and memory at HL;
// A high nibble and carry stay unchanged (UM0050 p. 218).
#define RRD                                                                                                            \
    {                                                                                                                  \
        uint8_t n = RM(hl16);                                                                                          \
        WM(hl16, (n >> 4) | (a8 << 4));                                                                                \
        a8 = (a8 & 0xf0) | (n & 0x0f);                                                                                 \
        f8 = (f8 & CF) | SZP[a8];                                                                                      \
    }

// Rotate the three low decimal nibbles left between A and memory at HL;
// A high nibble and carry stay unchanged (UM0050 p. 218).
#define RLD                                                                                                            \
    {                                                                                                                  \
        uint8_t n = RM(hl16);                                                                                          \
        WM(hl16, (n << 4) | (a8 & 0x0f));                                                                              \
        a8 = (a8 & 0xf0) | (n >> 4);                                                                                   \
        f8 = (f8 & CF) | SZP[a8];                                                                                      \
    }

// Add byte value to A, retain the low eight result bits and derive flags
// from both the original A and wrapped result (UM0050 p. 211).
#define ADD(value)                                                                                                     \
    {                                                                                                                  \
        uint32_t ah = af32 & 0xff00;                                                                                   \
        uint32_t res = (uint8_t)((ah >> 8) + value);                                                                   \
        f8 = add_flags(ah >> 8, res);                                                                                      \
        a8 = res;                                                                                                      \
    }

// Add byte value and carry to A; wrap to eight bits and derive flags (UM0050 p. 211).
#define ADC(value)                                                                                                     \
    {                                                                                                                  \
        uint32_t ah = af32 & 0xff00, c = af32 & 1;                                                                     \
        uint32_t res = (uint8_t)((ah >> 8) + value + c);                                                               \
        f8 = add_flags(ah >> 8, res, c);                                                                          \
        a8 = res;                                                                                                      \
    }

// Subtract byte value from A; wrap to eight bits and derive borrow/overflow (UM0050 p. 212).
#define SUB(value)                                                                                                     \
    {                                                                                                                  \
        uint32_t ah = af32 & 0xff00;                                                                                   \
        uint32_t res = (uint8_t)((ah >> 8) - value);                                                                   \
        f8 = sub_flags(ah >> 8, res);                                                                                      \
        a8 = res;                                                                                                      \
    }

// Subtract byte value and carry from A, retaining an eight-bit result (UM0050 p. 212).
#define SBC(value)                                                                                                     \
    {                                                                                                                  \
        uint32_t ah = af32 & 0xff00, c = af32 & 1;                                                                     \
        uint32_t res = (uint8_t)((ah >> 8) - value - c);                                                               \
        f8 = sub_flags(ah >> 8, res, c);                                                                          \
        a8 = res;                                                                                                      \
    }

// Form two's complement A by subtracting its saved value from zero (UM0050 p. 215).
#define NEG                                                                                                            \
    {                                                                                                                  \
        uint8_t value = a8;                                                                                            \
        a8 = 0;                                                                                                        \
        SUB(value);                                                                                                    \
    }

// Adjust A after binary-coded decimal arithmetic: each nibble stores one
// decimal digit. Add/subtract 6 or 0x60 when a digit or its carry requires
// correction; derive flags from the corrected byte (UM0050 p. 215).
#define DAA                                                                                                            \
    {                                                                                                                  \
        uint8_t r = a8;                                                                                                \
        if (f8 & NF) {                                                                                                 \
            if ((f8 & HF) | ((a8 & 0xf) > 9))                                                                          \
                r -= 6;                                                                                                \
            if ((f8 & CF) | (a8 > 0x99))                                                                               \
                r -= 0x60;                                                                                             \
        } else {                                                                                                       \
            if ((f8 & HF) | ((a8 & 0xf) > 9))                                                                          \
                r += 6;                                                                                                \
            if ((f8 & CF) | (a8 > 0x99))                                                                               \
                r += 0x60;                                                                                             \
        }                                                                                                              \
        f8 = (f8 & 3) | (a8 > 0x99) | ((a8 ^ r) & HF) | SZP[r];                                                        \
        a8 = r;                                                                                                        \
    }

// AND byte value with A; result gives sign/zero/parity, half-carry is set (UM0050 p. 214).
#define AND(value)                                                                                                     \
    a8 &= value;                                                                                                       \
    f8 = SZP[a8] | HF

// OR byte value into A; derive sign/zero/parity and clear carry/subtract/half-carry (UM0050 p. 214).
#define OR(value)                                                                                                      \
    a8 |= value;                                                                                                       \
    f8 = SZP[a8]

// Exclusive-OR byte value into A and derive logical-result flags (UM0050 p. 214).
#define XOR(value)                                                                                                     \
    a8 ^= value;                                                                                                       \
    f8 = SZP[a8]

// Subtract byte value only to set flags; preserve A for later code (UM0050 p. 213).
#define CP(value)                                                                                                      \
    {                                                                                                                  \
        uint32_t ah = af32 & 0xff00;                                                                                   \
        uint32_t res = (uint8_t)((ah >> 8) - value);                                                                   \
        f8 = sub_flags(ah >> 8, res);                                                                                      \
    }

// Exchange the accumulator/flag pair with its alternate pair (UM0050 p. 228).
#define EX_AF                                                                                                          \
    {                                                                                                                  \
        Z180Register tmp;                                                                                              \
        tmp = AF;                                                                                                      \
        AF = AF2;                                                                                                      \
        AF2 = tmp;                                                                                                     \
    }

// Exchange the two 16-bit register pairs without changing flags (UM0050 p. 228).
#define EX_DE_HL                                                                                                       \
    {                                                                                                                  \
        Z180Register tmp;                                                                                              \
        tmp = DE;                                                                                                      \
        DE = HL;                                                                                                       \
        HL = tmp;                                                                                                      \
    }

// Exchange BC, DE and HL with their alternate pairs; AF is separate (UM0050 p. 228).
#define EXX                                                                                                            \
    {                                                                                                                  \
        Z180Register tmp;                                                                                              \
        tmp = BC;                                                                                                      \
        BC = BC2;                                                                                                      \
        BC2 = tmp;                                                                                                     \
        tmp = DE;                                                                                                      \
        DE = DE2;                                                                                                      \
        DE2 = tmp;                                                                                                     \
        tmp = HL;                                                                                                      \
        HL = HL2;                                                                                                      \
        HL2 = tmp;                                                                                                     \
    }

// Exchange register pair DR with the stack word. Read low/high, then write
// high/low, preserving the bus sequence even across suspension (UM0050 pp. 256-257).
#define EXSP(DR)                                                                                                       \
    {                                                                                                                  \
        Z180Register tmp = {};                                                                                         \
        RM16(sp32, &tmp);                                                                                              \
        WM(uint16_t(sp16 + 1), DR.b.h); /* UM0050 pp256-257 */                                                         \
        WM(sp32, DR.b.l);                                                                                              \
        DR = tmp;                                                                                                      \
    }

// Add source pair SR into destination DR with 16-bit wrap. Derive carry
// from bit 16 and half-carry from bit 12; preserve sign/zero/overflow (UM0050 p. 221).
#define ADD16(DR, SR)                                                                                                  \
    {                                                                                                                  \
        uint32_t res = DR.d + SR.d;                                                                                    \
        f8 = (f8 & (SF | ZF | VF)) | (((DR.d ^ res ^ SR.d) >> 8) & HF) | ((res >> 16) & CF);                           \
        DR.w.l = (uint16_t)res;                                                                                        \
    }

// Add pair DR and carry into HL. Wider temporary retains bit 16 carry;
// sign changes distinguish signed overflow from carry (UM0050 p. 221).
#define ADC16(DR)                                                                                                      \
    {                                                                                                                  \
        uint32_t res = hl32 + DR.d + (f8 & CF);                                                                        \
        f8 = (((hl32 ^ res ^ DR.d) >> 8) & HF) | ((res >> 16) & CF) | ((res >> 8) & SF) | ((res & 0xffff) ? 0 : ZF) |  \
             (((DR.d ^ hl32 ^ 0x8000) & (DR.d ^ res) & 0x8000) >> 13);                                                 \
        hl16 = (uint16_t)res;                                                                                          \
    }

// Subtract pair DR and carry from HL. Derive borrow, half-borrow and
// signed overflow before truncating the result to 16 bits (UM0050 p. 221).
#define SBC16(DR)                                                                                                      \
    {                                                                                                                  \
        uint32_t res = hl32 - DR.d - (f8 & CF);                                                                        \
        f8 = (((hl32 ^ res ^ DR.d) >> 8) & HF) | NF | ((res >> 16) & CF) | ((res >> 8) & SF) |                         \
             ((res & 0xffff) ? 0 : ZF) | (((DR.d ^ hl32) & (hl32 ^ res) & 0x8000) >> 13);                              \
        hl16 = (uint16_t)res;                                                                                          \
    }

// Return value rotated left with bit 7 wrapping into bit 0 and carry.
// Set sign/zero/parity from the result (UM0050 pp. 216-217).
uint8_t Z180::RLC(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x80) ? CF : 0;
    res = ((res << 1) | (res >> 7)) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

// Return value rotated right with bit 0 wrapping into bit 7 and carry.
// Set sign/zero/parity from the result (UM0050 pp. 216-217).
uint8_t Z180::RRC(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x01) ? CF : 0;
    res = ((res >> 1) | (res << 7)) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

// Return value rotated left through carry; its old bit 7 becomes carry.
// Set sign/zero/parity from the result (UM0050 pp. 216-217).
uint8_t Z180::RL(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x80) ? CF : 0;
    res = ((res << 1) | (f8 & CF)) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

// Return value rotated right through carry; its old bit 0 becomes carry.
// Set sign/zero/parity from the result (UM0050 pp. 216-217).
uint8_t Z180::RR(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x01) ? CF : 0;
    res = ((res >> 1) | (f8 << 7)) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

// Return value shifted left with zero inserted; bit 7 becomes carry.
// Set sign/zero/parity from the result (UM0050 pp. 218-219).
uint8_t Z180::SLA(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x80) ? CF : 0;
    res = (res << 1) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

// Return value shifted right with its sign bit retained; bit 0 becomes
// carry. Set sign/zero/parity from the result (UM0050 pp. 218-219).
uint8_t Z180::SRA(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x01) ? CF : 0;
    res = ((res >> 1) | (res & 0x80)) & 0xff;
    f8 = SZP[res] | c;
    return res;
}


// Return value shifted right with zero inserted; bit 0 becomes carry.
// Set sign/zero/parity from the result (UM0050 pp. 218-219).
uint8_t Z180::SRL(uint8_t value) {
    unsigned res = value;
    unsigned c = (res & 0x01) ? CF : 0;
    res = (res >> 1) & 0xff;
    f8 = SZP[res] | c;
    return res;
}

#undef BIT
// Test bit (0-7) of byte reg. Preserve carry; set half-carry and zero/parity
// when the bit is clear. Only bit 7 can set sign (UM0050 p. 220).
#define BIT_TEST(bit, reg) f8 = (f8 & CF) | HF | SZ_BIT[reg & (1 << bit)]

// Set indexed BIT flags like BIT_TEST, but use the Z80 rule
// for undocumented flags: bits 3/5 come from the effective address high byte.
// Defined flags follow UM0050 p. 220.
#define BIT_XY(bit, reg) f8 = (f8 & CF) | HF | (SZ_BIT[reg & (1 << bit)] & ~(YF | XF)) | ((ea >> 8) & (YF | XF))

// Return value with bit (0-7) cleared; leave flags unchanged (UM0050 p. 220).
uint8_t Z180::RES(uint8_t bit, uint8_t value) { return value & ~(1 << bit); }

// Return value with bit (0-7) set; leave flags unchanged (UM0050 p. 220).
uint8_t Z180::SET(uint8_t bit, uint8_t value) { return value | (1 << bit); }

// Copy one byte HL to DE, advance both addresses and decrement BC.
// Parity/overflow means more bytes remain. Undocumented X and Y take bits
// 3 and 1 of A+byte respectively (count behavior: UM0050 p. 225).
#define LDI                                                                                                            \
    {                                                                                                                  \
        uint8_t io = RM(hl16);                                                                                         \
        WM(de16, io);                                                                                                  \
        f8 &= SF | ZF | CF;                                                                                            \
        if ((a8 + io) & 0x02)                                                                                          \
            f8 |= YF; /* bit 1 -> flag 5 */                                                                            \
        if ((a8 + io) & 0x08)                                                                                          \
            f8 |= XF; /* bit 3 -> flag 3 */                                                                            \
        hl16++;                                                                                                        \
        de16++;                                                                                                        \
        bc16--;                                                                                                        \
        if (bc16)                                                                                                      \
            f8 |= VF;                                                                                                  \
    }

// Compare A with memory at HL, advance HL and decrement BC. Preserve
// carry; parity/overflow means count nonzero. The half-borrow correction
// feeds only the undocumented X/Y flags (UM0050 p. 226).
#define CPI                                                                                                            \
    {                                                                                                                  \
        uint8_t val = RM(hl16);                                                                                        \
        uint8_t res = a8 - val;                                                                                        \
        hl16++;                                                                                                        \
        bc16--;                                                                                                        \
        f8 = (f8 & CF) | (SZ[res] & ~(YF | XF)) | ((a8 ^ val ^ res) & HF) | NF;                                        \
        if (f8 & HF)                                                                                                   \
            res -= 1;                                                                                                  \
        if (res & 0x02)                                                                                                \
            f8 |= YF; /* bit 1 -> flag 5 */                                                                            \
        if (res & 0x08)                                                                                                \
            f8 |= XF; /* bit 3 -> flag 3 */                                                                            \
        if (bc16)                                                                                                      \
            f8 |= VF;                                                                                                  \
    }

// Read port BC into memory at HL; decrement B and move HL forward.
// This core sets X/Y from B bits 3/5 and N from the byte's high bit.
// H/C use the carry or borrow of C+byte+1 or C+byte-1; parity uses the
// tables in z180tbl.hpp. Transfer/count rules: UM0050 pp. 232-234.
#define INI                                                                                                            \
    {                                                                                                                  \
        uint8_t io = IN(bc16);                                                                                         \
        b8--;                                                                                                          \
        WM(hl16, io);                                                                                                  \
        hl16++;                                                                                                        \
        f8 = SZ[b8];                                                                                                   \
        if (io & SF)                                                                                                   \
            f8 |= NF;                                                                                                  \
        if ((c8 + io + 1) & 0x100)                                                                                     \
            f8 |= HF | CF;                                                                                             \
        if ((irep_tmp1[c8 & 3][io & 3] ^ breg_tmp2[b8] ^ (c8 >> 2) ^ (io >> 2)) & 1)                                   \
            f8 |= PF;                                                                                                  \
    }

// Read memory at HL, decrement B, then write to port BC and move HL forward.
// This core sets X/Y from B bits 3/5 and N from the byte's high bit.
// H/C use the carry or borrow of C+byte+1 or C+byte-1; parity uses the
// tables in z180tbl.hpp. Transfer/count rules: UM0050 pp. 232-234.
#define OUTI                                                                                                           \
    {                                                                                                                  \
        uint8_t io = RM(hl16);                                                                                         \
        b8--;                                                                                                          \
        OUT(bc16, io);                                                                                                 \
        hl16++;                                                                                                        \
        f8 = SZ[b8];                                                                                                   \
        if (io & SF)                                                                                                   \
            f8 |= NF;                                                                                                  \
        if ((c8 + io + 1) & 0x100)                                                                                     \
            f8 |= HF | CF;                                                                                             \
        if ((irep_tmp1[c8 & 3][io & 3] ^ breg_tmp2[b8] ^ (c8 >> 2) ^ (io >> 2)) & 1)                                   \
            f8 |= PF;                                                                                                  \
    }

// Apply the register half of LDD after copied byte io reaches memory: move
// HL/DE backward, decrement BC, preserve sign/zero/carry and report BC nonzero
// in parity/overflow. X and Y take bits 3 and 1 of A+byte respectively
// (count behavior: UM0050 p. 225).
inline void Z180::ldd_registers(uint8_t io) {
    f8 &= SF | ZF | CF;
    if ((a8 + io) & 0x02)
        f8 |= YF;
    if ((a8 + io) & 0x08)
        f8 |= XF;
    --hl16;
    --de16;
    --bc16;
    if (bc16)
        f8 |= VF;
}
// Copy one byte HL to DE, then apply backward pointer/count/flag changes (UM0050 p. 225).
#define LDD                                                                                                            \
    {                                                                                                                  \
        uint8_t const io = RM(hl16);                                                                                   \
        WM(de16, io);                                                                                                  \
        ldd_registers(io);                                                                                             \
    }

// Compare A with memory at HL and move backward; decrement BC and set
// comparison/count flags without changing A (UM0050 p. 226).
#define CPD                                                                                                            \
    {                                                                                                                  \
        uint8_t val = RM(hl16);                                                                                        \
        uint8_t res = a8 - val;                                                                                        \
        hl16--;                                                                                                        \
        bc16--;                                                                                                        \
        f8 = (f8 & CF) | (SZ[res] & ~(YF | XF)) | ((a8 ^ val ^ res) & HF) | NF;                                        \
        if (f8 & HF)                                                                                                   \
            res -= 1;                                                                                                  \
        if (res & 0x02)                                                                                                \
            f8 |= YF; /* bit 1 -> flag 5 */                                                                            \
        if (res & 0x08)                                                                                                \
            f8 |= XF; /* bit 3 -> flag 3 */                                                                            \
        if (bc16)                                                                                                      \
            f8 |= VF;                                                                                                  \
    }

// Read port BC into memory at HL; decrement B and move HL backward.
// This core sets X/Y from B bits 3/5 and N from the byte's high bit.
// H/C use the carry or borrow of C+byte+1 or C+byte-1; parity uses the
// tables in z180tbl.hpp. Transfer/count rules: UM0050 pp. 232-234.
#define IND                                                                                                            \
    {                                                                                                                  \
        uint8_t io = IN(bc16);                                                                                         \
        b8--;                                                                                                          \
        WM(hl16, io);                                                                                                  \
        hl16--;                                                                                                        \
        f8 = SZ[b8];                                                                                                   \
        if (io & SF)                                                                                                   \
            f8 |= NF;                                                                                                  \
        if ((c8 + io - 1) & 0x100)                                                                                     \
            f8 |= HF | CF;                                                                                             \
        if ((drep_tmp1[c8 & 3][io & 3] ^ breg_tmp2[b8] ^ (c8 >> 2) ^ (io >> 2)) & 1)                                   \
            f8 |= PF;                                                                                                  \
    }

// Read memory at HL, decrement B, then write to port BC and move HL backward.
// This core sets X/Y from B bits 3/5 and N from the byte's high bit.
// H/C use the carry or borrow of C+byte+1 or C+byte-1; parity uses the
// tables in z180tbl.hpp. Transfer/count rules: UM0050 pp. 232-234.
#define OUTD                                                                                                           \
    {                                                                                                                  \
        uint8_t io = RM(hl16);                                                                                         \
        b8--;                                                                                                          \
        OUT(bc16, io);                                                                                                 \
        hl16--;                                                                                                        \
        f8 = SZ[b8];                                                                                                   \
        if (io & SF)                                                                                                   \
            f8 |= NF;                                                                                                  \
        if ((c8 + io - 1) & 0x100)                                                                                     \
            f8 |= HF | CF;                                                                                             \
        if ((drep_tmp1[c8 & 3][io & 3] ^ breg_tmp2[b8] ^ (c8 >> 2) ^ (io >> 2)) & 1)                                   \
            f8 |= PF;                                                                                                  \
    }

// Perform one block step and back PC up by two while BC is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define LDIR                                                                                                           \
    LDI;                                                                                                               \
    if (bc16) {                                                                                                        \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb0);                                                                                                  \
    }

// Perform one block step and back PC up by two while BC is nonzero and no match is found.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define CPIR                                                                                                           \
    CPI;                                                                                                               \
    if (bc16 && !(f8 & ZF)) {                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb1);                                                                                                  \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define INIR                                                                                                           \
    INI;                                                                                                               \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb2);                                                                                                  \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define OTIR                                                                                                           \
    OUTI;                                                                                                              \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb3);                                                                                                  \
    }

// Perform one block step and back PC up by two while BC is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define LDDR                                                                                                           \
    LDD;                                                                                                               \
    if (bc16) {                                                                                                        \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb8);                                                                                                  \
    }

// Perform one block step and back PC up by two while BC is nonzero and no match is found.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define CPDR                                                                                                           \
    CPD;                                                                                                               \
    if (bc16 && !(f8 & ZF)) {                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xb9);                                                                                                  \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define INDR                                                                                                           \
    IND;                                                                                                               \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xba);                                                                                                  \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define OTDR                                                                                                           \
    OUTD;                                                                                                              \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        CC(ex, 0xbb);                                                                                                  \
    }

// Enable interrupts but defer acceptance through the following instruction (UM0050 pp. 68-69).
#define EI                                                                                                             \
    {                                                                                                                  \
        IFF1 = IFF2 = 1;                                                                                               \
        after_EI = 1;                                                                                                  \
    }

// Set sign/zero/parity and half-carry from A AND value; retain both operands (UM0050 p. 214).
#define TST(value) f8 = SZP[a8 & value] | HF

// Multiply the two unsigned bytes of pair DR and store their 16-bit product (UM0050 p. 221).
#define MLT(DR)                                                                                                        \
    {                                                                                                                  \
        DR.w.l = DR.b.l * DR.b.h;                                                                                      \
    }

// Write memory at HL to port C; decrement B and move HL forward.
// Move C too; flags reflect B decrement and outgoing data sign.
// Transfer/count rules: UM0050 pp. 232-234.
#define OTIM                                                                                                           \
    {                                                                                                                  \
        uint8_t const data = RM(hl16);                                                                                 \
        OUT(c8, data);                                                                                                 \
        b8--;                                                                                                          \
        hl16++;                                                                                                        \
        c8++;                                                                                                          \
        /* Retained block-I/O flags: borrow/parity of B-1; N is data bit 7. */                                                \
        f8 = SZP[b8] | ((b8 & 15) == 15 ? HF : 0) | (b8 == 255 ? CF : 0) | ((data & 0x80) ? NF : 0);                   \
    }

// Write memory at HL to port C; decrement B and move HL backward.
// Move C too; flags reflect B decrement and outgoing data sign.
// Transfer/count rules: UM0050 pp. 232-234.
#define OTDM                                                                                                           \
    {                                                                                                                  \
        uint8_t const data = RM(hl16);                                                                                 \
        OUT(c8, data);                                                                                                 \
        b8--;                                                                                                          \
        hl16--;                                                                                                        \
        c8--;                                                                                                          \
        /* Retained block-I/O flags: borrow/parity of B-1; N is data bit 7. */                                                \
        f8 = SZP[b8] | ((b8 & 15) == 15 ? HF : 0) | (b8 == 255 ? CF : 0) | ((data & 0x80) ? NF : 0);                   \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define OTIMR                                                                                                          \
    OTIM;                                                                                                              \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        extra_cycles += 2; /* UM0050 pp232-234 */                                                                      \
    }

// Perform one block step and back PC up by two while B is nonzero.
// The next iteration is another instruction, so interrupts can occur between
// bytes. Add repeat-path PHI only when repeating (UM0050 pp. 225-226, 232-234).
#define OTDMR                                                                                                          \
    OTDM;                                                                                                              \
    if (b8) {                                                                                                          \
        pc16 -= 2;                                                                                                     \
        extra_cycles += 2; /* UM0050 pp232-234 */                                                                      \
    }

// Keep PC on the two-byte SLP instruction while its low-power state is
// active; wake logic advances it once (UM0050 pp. 34-41).
#define SLP                                                                                                            \
    {                                                                                                                  \
        pc16 -= 2;                                                                                                     \
        HALT = 2;                                                                                                      \
    }

// UM0050 pp68-72, Figures 32/33. TRAP leaves both IEFs unchanged.
// Enter undefined-instruction handling at address zero and return its
// nominal PHI count. third selects the indexed third-opcode trap, including
// its operand read and stacked PC offset; interrupt enables stay unchanged
// (UM0050 pp. 68-72, Figs. 32-33).
int Z180::take_trap(bool third) {
    if (cycle_executing) {
        trapping = true;
        third_trap = third;
    }
    if (third)
        RM(ea); // The indexed operand read precedes the trap (Figure 33).
    // The test executor may re-enter after a bus edge; publish trap status
    // only once. Production retains the active instruction call stack.
#ifdef GS2_SS_TESTING
    if (!cycle_executing || !trap_started)
#endif
    {
        itc = (itc & ~Z180_ITC_UFO) | Z180_ITC_TRAP | (third ? Z180_ITC_UFO : 0);
#ifdef GS2_SS_TESTING
        trap_started = true;
#endif
    }
    pc16 = previous_pc + (third ? 2 : 1);
    PUSH(PC);
    pc16 = 0;
    return third ? 26 : 17;
}

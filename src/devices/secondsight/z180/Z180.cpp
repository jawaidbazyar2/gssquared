// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   The Zilog Z8S180 is Second Sight's own processor. It runs the card ROM
 *   and copies Apple video bytes into the Oak controller's video memory.
 *   SSCard supplies memory, port and pin callbacks. This file connects the
 *   instruction handlers to the on-chip timers, serial ports and bus scheduler.
 *   SSCard runs the CPU on its worker while a job is outstanding. Without
 *   a worker, the caller executes it directly.
 *   Callbacks run on that same thread and must not enter execution recursively.
 *
 *   PHI is the processor clock. One T-state is one PHI period; a half-PHI is
 *   one rising-to-falling or falling-to-rising interval. A machine cycle is a
 *   bus operation or internal step made of T-states. T1, T2 and T3 name its
 *   successive clocks; TW is an inserted wait clock and Ti an internal clock.
 *   M1 identifies an instruction-byte fetch. IORQ selects an input/output port.
 *   WAIT lengthens a transfer. BUSREQ asks the processor to release its bus;
 *   BUSACK confirms that it has done so. These names denote asserted signals,
 *   regardless of whether the electrical pin is active low.
 *
 *   The MMU (memory management unit) maps a 16-bit program address to a
 *   20-bit physical address. DMA (direct memory access) has two channels,
 *   each a source, destination and byte counter that can copy without an
 *   instruction for each byte. DREQ is a channel's external request input.
 *   INT0-2 are maskable interrupt inputs; NMI is a non-maskable interrupt.
 *   PRT means programmable reload timer. FRC is the free-running counter
 *   used to divide PHI. ASCI is asynchronous serial communication; CSI/O
 *   is serial communication with a separate clock wire.
 *
 *   Sources: Zilog UM005004-0918 (UM0050), printed pp. 15-30 for bus timing,
 *   pp. 55-64 for the MMU, pp. 65-115 for interrupts, refresh and DMA,
 *   pp. 116-164 for serial ports and timers, and pp. 251-280 for instruction
 *   bus sequences: https://www.zilog.com/docs/z180/um0050.pdf.
 *   The scheduler grants a bounded interval of CPU time, ending at a board
 *   event or another execution deadline. Fast paths combine bus phases only
 *   when the interval and input state allow it. Direct reads use board-provided
 *   memory pointers only where reads have no device side effects. Test replay
 *   records completed accesses so restarting a handler does not repeat them.
 *   These are emulator mechanisms, not extra processor registers. K/N revision
 *   differences use Zilog TN0053
 *   and the Z8S180 data sheets cited at the affected operation.
 */

#include "Z180.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <mutex>
#include <stdexcept>

// Format an invariant failure for SSCard to retain and report after the worker
// stops. Arguments have printf meanings; this never returns to a bus operation.
template <class... Args> [[noreturn]] static void z180_error(const char *format, Args... args) {
    char message[256];
    std::snprintf(message, sizeof(message), format, args...);
    throw std::runtime_error(message);
}
// Internal priority order (Zilog UM0050, Interrupts); these are model indices.
#define Z180_INT_TRAP 0   /* Undefined opcode_arg */
#define Z180_INT_NMI 1    /* NMI */
#define Z180_INT_IRQ0 2   /* Execute IRQ0 */
#define Z180_INT_IRQ1 3   /* Execute IRQ1 */
#define Z180_INT_IRQ2 4   /* Execute IRQ2 */
#define Z180_INT_PRT0 5   /* Internal PRT channel 0 */
#define Z180_INT_PRT1 6   /* Internal PRT channel 1 */
#define Z180_INT_DMA0 7   /* Internal DMA channel 0 */
#define Z180_INT_DMA1 8   /* Internal DMA channel 1 */
#define Z180_INT_CSIO 9   /* Internal CSI/O */
#define Z180_INT_ASCI0 10 /* Internal ASCI channel 0 */
#define Z180_INT_ASCI1 11 /* Internal ASCI channel 1 */
#define Z180_INT_MAX Z180_INT_ASCI1

// F is the arithmetic-status byte (UM0050 pp. 177-179). S is the sign bit,
// Z means zero, H means a carry/borrow across bit 3, N marks subtraction,
// and C means carry/borrow across bit 7. P/V is parity for logical operations
// or signed overflow for arithmetic. Zilog does not document flag bits 3 and 5.
// For ordinary arithmetic this core copies result bits 3 and 5 into X and Y;
// other instructions use different values. These rules came from MAME's Z180
// core, z180.cpp flag-table initialization:
// https://github.com/mamedev/mame/blob/master/src/devices/cpu/z180/z180.cpp
#define CF 0x01
#define NF 0x02
#define PF 0x04
#define VF PF
#define XF 0x08
#define HF 0x10
#define YF 0x20
#define ZF 0x40
#define SF 0x80

/* I/O line status flags */
#define Z180_CKA0 0x00000001     /* I/O asynchronous clock 0 (active high) or DREQ0 (mux) */
#define Z180_CKA1 0x00000002     /* I/O asynchronous clock 1 (active high) or TEND1 (mux) */
#define Z180_CKS 0x00000004      /* I/O serial clock (active high) */
#define Z180_CTS0 0x00000100     /* I   clear to send 0 (active low) */
#define Z180_CTS1 0x00000200     /* I   clear to send 1 (active low) or RXS (mux) */
#define Z180_DCD0 0x00000400     /* I   data carrier detect (active low) */
#define Z180_DREQ0 0x00000800    /* I   data request DMA ch 0 (active low) or CKA0 (mux) */
#define Z180_DREQ1 0x00001000    /* I   data request DMA ch 1 (active low) */
#define Z180_RXA0 0x00002000     /* I   asynchronous receive data 0 (active high) */
#define Z180_RXA1 0x00004000     /* I   asynchronous receive data 1 (active high) */
#define Z180_RXS 0x00008000      /* I   clocked serial receive data (active high) or CTS1 (mux) */
#define Z180_RTS0 0x00010000     /*   O request to send (active low) */
#define Z180_TEND0 0x00020000    /*   O transfer end 0 (active low) or CKA1 (mux) */
#define Z180_TEND1 0x00040000    /*   O transfer end 1 (active low) */
#define Z180_A18_TOUT 0x00080000 /*   O transfer out (PRT channel, active low) or A18 (mux) */
#define Z180_TXA0 0x00100000     /*   O asynchronous transmit data 0 (active high) */
#define Z180_TXA1 0x00200000     /*   O asynchronous transmit data 1 (active high) */
#define Z180_TXS 0x00400000      /*   O clocked serial transmit data (active high) */

/*
 * NetBSD headers can define these short register names as macros.
 */
#undef b8
#undef c8
#undef l8

#define previous_pc PREPC.d /* previous program counter */

#define pc32 PC.d
#define pc16 PC.w.l

#define sp32 SP.d
#define sp16 SP.w.l

#define af32 AF.d
#define af16 AF.w.l
#define a8 AF.b.h
#define f8 AF.b.l

#define bc32 BC.d
#define bc16 BC.w.l
#define b8 BC.b.h
#define c8 BC.b.l

#define de32 DE.d
#define de16 DE.w.l
#define d8 DE.b.h
#define e8 DE.b.l

#define hl32 HL.d
#define hl16 HL.w.l
#define h8 HL.b.h
#define l8 HL.b.l

#define ix32 IX.d
#define ix16 IX.w.l

#define iy32 IY.d
#define iy16 IY.w.l

// Zilog UM0050 pp. 157-164: PRT registers and flag-clearing sequence.
/* 10 TIMER control register (TIF1 and TIF0 are read-only) */
// TIF0/1: timeout occurred; TIE0/1: enable timeout interrupt; TDE0/1:
// enable counting. TOC1:0 select A18 or timer output and its toggle rate.
#define Z180_TCR_TIF1 0x80
#define Z180_TCR_TIF0 0x40
#define Z180_TCR_TIE1 0x20
#define Z180_TCR_TIE0 0x10
#define Z180_TCR_TOC1 0x08
#define Z180_TCR_TOC0 0x04
#define Z180_TCR_TDE1 0x02
#define Z180_TCR_TDE0 0x01

/* 1e (Z8S180/Z8L180) clock multiplier */
// X2 enables the N-revision multiplier. LOW_NOISE changes output drive,
// which is stored but has no analogue electrical effect here (UM0050 pp. 49-52).
#define Z180_CMR_X2 0x80
#define Z180_CMR_LOW_NOISE 0x40

#define Z180_CMR_MASK 0xc0

/* 1f (Z8S180/Z8L180) chip control register */
// CLOCK_DIVIDE=1 bypasses divide-by-two. STDBY_IDLE1:0 choose the low-power
// mode; BREXT permits BUSREQ wake-up. LNPHI, LNIO, LNCPU_CTL and LNAD_DATA
// select reduced drive for clock, I/O, CPU-control and address/data pins.
// UM0050 pp. 37-41, 49-52; the K revision reserves STDBY_IDLE0.
#define Z180_CCR_CLOCK_DIVIDE 0x80
#define Z180_CCR_STDBY_IDLE1 0x40
#define Z180_CCR_BREXT 0x20
#define Z180_CCR_LNPHI 0x10
#define Z180_CCR_STDBY_IDLE0 0x08
#define Z180_CCR_LNIO 0x04
#define Z180_CCR_LNCPU_CTL 0x02
#define Z180_CCR_LNAD_DATA 0x01

// Zilog UM0050 pp. 95-109: DMA register fields and request modes.
/* 20-22 DMA source address register ch 0 L, H, B */
#define Z180_SAR0_MASK 0x0fffff

/* 23-25 DMA destination address register ch 0 L, H, B */
#define Z180_DAR0_MASK 0x0fffff

/* 28-2a DMA memory address register ch 1 L, H, B */
#define Z180_MAR1_MASK 0x0fffff

/* 2b-2d DMA I/O address register ch 1 L, H, (Z8S180/Z8L180) B */
// IAR1 high-byte fields: ATF is the automatic-transfer flag; ATC controls
// automatic transfer; TOUT_DREQ selects the request source; SS selects
// external I/O or an ASCI channel. These extension fields are retained even
// where no Second Sight connection uses them (Z8S180 DS006002, pp. 56-57).
#define Z180_IAR1_ATF 0x800000
#define Z180_IAR1_ATC 0x400000
#define Z180_IAR1_TOUT_DREQ 0x080000
#define Z180_IAR1_SS 0x070000

#define Z180_IAR1_MASK 0xcfffff

/* 30 DMA status register (DWE1 and DWE0 are write-only, DME is read-only) */
#define Z180_DSTAT_DE1 0x80  /* DMA enable ch 1 */
#define Z180_DSTAT_DE0 0x40  /* DMA enable ch 0 */
#define Z180_DSTAT_DWE1 0x20 /* DMA write enable ch 1 (active low) */
#define Z180_DSTAT_DWE0 0x10 /* DMA write enable ch 0 (active low) */
#define Z180_DSTAT_DIE1 0x08 /* DMA IRQ enable ch 1 */
#define Z180_DSTAT_DIE0 0x04 /* DMA IRQ enable ch 0 */
#define Z180_DSTAT_DME 0x01  /* DMA enable (read only) */

#define Z180_DSTAT_MASK 0xfd

/* 31 DMA mode register */
#define Z180_DMODE_DM 0x30   /* DMA ch 0 destination addressing mode */
#define Z180_DMODE_SM 0x0c   /* DMA ch 0 source addressing mode */
#define Z180_DMODE_MMOD 0x02 /* DMA cycle steal/burst mode select */

#define Z180_DMODE_MASK 0x3e

/* 32 DMA/WAIT control register */
// MWI1:0 insert 0-3 memory waits. IWI1:0 select 1-4 external I/O waits.
// DMS0/1 select edge (1) or level (0) DREQ sensing for each channel.
// DIM1 selects DMA1 I/O-to-memory (1) or memory-to-I/O (0); DIM0 chooses
// decreasing (1) or increasing (0) memory addresses (UM0050 pp. 29-30, 101-103).
#define Z180_DCNTL_MWI1 0x80
#define Z180_DCNTL_MWI0 0x40
#define Z180_DCNTL_IWI1 0x20
#define Z180_DCNTL_IWI0 0x10
#define Z180_DCNTL_DMS1 0x08
#define Z180_DCNTL_DMS0 0x04
#define Z180_DCNTL_DIM1 0x02
#define Z180_DCNTL_DIM0 0x01

/* 33 INT vector low register */
#define Z180_IL_IL 0xe0

#define Z180_IL_MASK 0xe0

/* 34 INT/TRAP control register (UFO is read-only) */
// TRAP records an undefined instruction. UFO distinguishes a third-byte
// undefined opcode from a second-byte one. ITE0-2 enable INT0-2 separately
// from the CPU interrupt-enable flag (UM0050 pp. 69-72).
#define Z180_ITC_TRAP 0x80
#define Z180_ITC_UFO 0x40
#define Z180_ITC_ITE2 0x04
#define Z180_ITC_ITE1 0x02
#define Z180_ITC_ITE0 0x01

#define Z180_ITC_MASK 0xc7

// Zilog UM0050, Refresh Control: enable, extra wait and interval select.
/* 36 refresh control register */
// REFE enables dynamic-RAM refresh; REFW adds a refresh wait; CYC selects
// the interval. Second Sight SRAM needs no refresh (UM0050 pp. 87-90).
#define Z180_RCR_REFE 0x80
#define Z180_RCR_REFW 0x40
#define Z180_RCR_CYC 0x03

#define Z180_RCR_MASK 0xc3

// Zilog UM0050, Memory Management Unit: CBAR splits common/banked areas.
/* 3a MMU common/bank area register */
// CA and BA are 4-KiB boundaries: common area 1 starts at CA and the
// movable bank starts at BA. Common area 0 stays untranslated (pp. 58-64).
#define Z180_CBAR_CA 0xf0
#define Z180_CBAR_BA 0x0f

/* 3e operation mode control register (M1TE is write-only) */
// M1E selects M1-compatible timing; M1TE temporarily enables M1 output.
// IOC selects the IORQ/RD/WR timing convention (UM0050 pp. 15-17).
#define Z180_OMCR_M1E 0x80
#define Z180_OMCR_M1TE 0x40
#define Z180_OMCR_IOC 0x20

#define Z180_OMCR_MASK 0xe0

/* 3f I/O control register */
// IOSTP stops the internal I/O clocks. IOCR bits 7:6 relocate the 64-byte
// internal register window; port high bits must be zero (UM0050 pp. 42-48).
#define Z180_IOCR_IOSTP 0x20

static uint8_t SZ[256];       /* zero and sign flags */
static uint8_t SZ_BIT[256];   /* zero, sign and parity/overflow (=zero) flags for BIT opcode_arg */
static uint8_t SZP[256];      /* zero, sign and parity flags */
static uint8_t SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static uint8_t SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */

// Derive ADD/ADC flags from the input/result pair without a large lookup table.
static uint8_t add_flags(uint8_t before, uint8_t result, unsigned carry = 0) {
    const int operand = int(result) - before - carry;
    return SZ[result] | (((result & 15) < (before & 15) + carry) ? HF : 0) |
           ((unsigned(result) < unsigned(before) + carry) ? CF : 0) |
           (((operand ^ before ^ 0x80) & (operand ^ result) & 0x80) ? VF : 0);
}

// Derive SUB/SBC flags with the same nibble carry and signed-overflow rules.
static uint8_t sub_flags(uint8_t before, uint8_t result, unsigned carry = 0) {
    const int operand = int(before) - result - carry;
    return SZ[result] | NF | (((result & 15) + carry > (before & 15)) ? HF : 0) |
           ((unsigned(result) + carry > before) ? CF : 0) |
           (((operand ^ before) & (before ^ result) & 0x80) ? VF : 0);
}

#include "z180ops.hpp"
#include "z180tbl.hpp"

#include "z180cb.hxx"
#include "z180xy.hxx"
#include "z180dd.hxx"
#include "z180fd.hxx"
#include "z180ed.hxx"
#include "z180op.hxx"

// Build byte-result tables for sign, zero, parity and INC/DEC flag effects.
static void init_flags() {
    int i, p;
    for (i = 0; i < 256; i++) {
        p = 0;
        if (i & 0x01)
            ++p;
        if (i & 0x02)
            ++p;
        if (i & 0x04)
            ++p;
        if (i & 0x08)
            ++p;
        if (i & 0x10)
            ++p;
        if (i & 0x20)
            ++p;
        if (i & 0x40)
            ++p;
        if (i & 0x80)
            ++p;
        SZ[i] = i ? i & SF : ZF;
        SZ[i] |= (i & (YF | XF)); /* undocumented flag bits 5+3 */
        SZ_BIT[i] = i ? i & SF : ZF | PF;
        SZ_BIT[i] |= (i & (YF | XF)); /* undocumented flag bits 5+3 */
        SZP[i] = SZ[i] | ((p & 1) ? 0 : PF);
        SZHV_inc[i] = SZ[i];
        if (i == 0x80)
            SZHV_inc[i] |= VF;
        if ((i & 0x0f) == 0x00)
            SZHV_inc[i] |= HF;
        SZHV_dec[i] = SZ[i] | NF;
        if (i == 0x7f)
            SZHV_dec[i] |= VF;
        if ((i & 0x0f) == 0x0f)
            SZHV_dec[i] |= HF;
    }
}
// Select internal I/O after IOCR relocation; the low six port bits identify
// the register (Zilog UM0050, internal I/O map and IOCR).
uint8_t Z180::z180_readcontrol(uint32_t port) {
    instruction_fast = false;
    // normal external readport (ignore the data)
    (void)read_io(port);

    // read the internal register
    return z180_internal_port_read(port & (0x3f));
}

// Read peripheral registers with their latch/acknowledgement side effects.
// Reserved bits and debugger peeks must not create interrupt acknowledgements.
// Offsets within the relocated internal-port window (UM0050 pp. 293-302):
// 00/01 CNTLA: serial control A; 02/03 CNTLB: serial control B;
// 04/05 STAT: serial status; 06/07 TDR: transmit byte; 08/09 RDR: receive byte;
// 0A CNTR: clocked-serial control/status; 0B TRDR: its shared data byte;
// 0C/0D TMDR0: timer 0 low/high count; 0E/0F RLDR0: its low/high reload;
// 10 TCR: timer control; 12/13 ASEXT: enhanced serial extension controls;
// 14/15 TMDR1 and 16/17 RLDR1: timer 1 count/reload low/high bytes;
// 18 FRC: free-running counter; 1A-1D ASTC: enhanced serial time constants;
// 1E CMR: clock multiplier; 1F CCR: clock/power/drive control;
// 20-22 SAR0: DMA0 source low/high/bank; 23-25 DAR0: destination low/high/bank;
// 26/27 BCR0: DMA0 count low/high; 28-2A MAR1: DMA1 memory low/high/bank;
// 2B-2D IAR1: DMA1 port low/high/extensions; 2E/2F BCR1: count low/high;
// 30 DSTAT: DMA status/enables; 31 DMODE: DMA0 addressing/burst modes;
// 32 DCNTL: wait insertion and DMA1/request modes; 33 IL: vector low bits;
// 34 ITC: interrupt/trap control; 36 RCR: refresh control;
// 38 CBR: MMU common offset; 39 BBR: bank offset; 3A CBAR: area boundaries;
// 3E OMCR: bus operation mode; 3F IOCR: port-window base and I/O clock stop.
// Other offsets read 0xff and ignore writes. Masks above describe bit meanings.
uint8_t Z180::z180_internal_port_read(uint8_t port) {
    uint8_t data = 0xff;

    // The timer and free-running counter registers expose their current PHI
    // state (UM0050 pp. 157-164), including the TCR read/clear sequence.
    if ((port >= 0x0c && port <= 0x10) || (port >= 0x14 && port <= 0x17))
        sync_timers();

    // TN0053 pp1-2: these enhancements are absent from original K silicon.
    if (silicon == Silicon::K && (port == 0x12 || port == 0x13 || (port >= 0x1a && port <= 0x1e)))
        return 0xff;
    switch (port) {
    case 0x12:
    case 0x13:
        data = asci[port & 1].asext_r();
        break;

    case 0x1a:
    case 0x1c:
        data = asci[(port - 0x1a) >> 1].astcl_r();
        break;

    case 0x1b:
    case 0x1d:
        data = asci[(port - 0x1a) >> 1].astch_r();
        break;

    case 0x1e:
        data = cmr | ~Z180_CMR_MASK;

        break;

    case 0x1f:
        data = ccr;

        break;

    case 0x00:
    case 0x01:
        data = asci[port & 1].cntla_r();
        break;

    case 0x02:
    case 0x03:
        data = asci[port & 1].cntlb_r();
        break;

    case 0x04:
    case 0x05:
        data = asci[port & 1].stat_r();
        break;

    case 0x06:
    case 0x07:
        data = asci[port & 1].tdr_r();
        break;

    case 0x08:
    case 0x09:
        data = asci[port & 1].rdr_r();
        break;

    case 0x0a:
        data = csio.cntr_r();
        break;

    case 0x0b:
        data = csio.trdr_r();
        break;

    case 0x0c:
        data = tmdr_value[0] & 0x00ff;

        /* Hold the high byte while reading low so a later high read cannot tear across a decrement (p. 159). */
        if (tcr & Z180_TCR_TDE0) {
            tmdr_latch |= 1;
            tmdrh[0] = (tmdr_value[0] & 0xff00) >> 8;
        }

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[0] & 1) {
            tcr &= ~Z180_TCR_TIF0;
            read_tcr_tmdr[0] = 0;
        } else
            read_tcr_tmdr[0] = 2;
        break;

    case 0x0d:
        /* Consume a previously held high byte; otherwise read the live counter (p. 159). */
        if (tmdr_latch & 1) {
            tmdr_latch &= ~1;
            data = tmdrh[0];
        } else {
            data = (tmdr_value[0] & 0xff00) >> 8;
        }

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[0] & 1) {
            tcr &= ~Z180_TCR_TIF0;
            read_tcr_tmdr[0] = 0;
        } else
            read_tcr_tmdr[0] = 2;

        break;

    case 0x0e:
        data = rldr[0].b.l;

        break;

    case 0x0f:
        data = rldr[0].b.h;

        break;

    case 0x10:
        data = tcr;

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[0] & 2) {
            tcr &= ~Z180_TCR_TIF0;
            read_tcr_tmdr[0] = 0;
        } else
            read_tcr_tmdr[0] = 1;

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[1] & 2) {
            tcr &= ~Z180_TCR_TIF1;
            read_tcr_tmdr[1] = 0;
        } else
            read_tcr_tmdr[1] = 1;

        break;

    case 0x14:
        data = tmdr_value[1];

        /* Hold the high byte while reading low so a later high read cannot tear across a decrement (p. 159). */
        if (tcr & Z180_TCR_TDE1) {
            tmdr_latch |= 2;
            tmdrh[1] = (tmdr_value[1] & 0xff00) >> 8;
        }

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[1] & 1) {
            tcr &= ~Z180_TCR_TIF1;
            read_tcr_tmdr[1] = 0;
        } else
            read_tcr_tmdr[1] = 2;
        break;

    case 0x15:
        /* Consume a previously held high byte; otherwise read the live counter (p. 159). */
        if (tmdr_latch & 2) {
            tmdr_latch &= ~2;
            data = tmdrh[1];
        } else {
            data = (tmdr_value[1] & 0xff00) >> 8;
        }

        // UM0050 p162: acknowledge by reading both TCR and TMDR.
        if (read_tcr_tmdr[1] & 1) {
            tcr &= ~Z180_TCR_TIF1;
            read_tcr_tmdr[1] = 0;
        } else
            read_tcr_tmdr[1] = 2;

        break;

    case 0x16:
        data = rldr[1].b.l;

        break;

    case 0x17:
        data = rldr[1].b.h;

        break;

    case 0x18:
        // FRC reads do not change its divider or either PRT. Project the
        // byte without materializing both timers (UM0050 pp. 157-158).
        data = frc - (frc_prescale + (half_phi - phi_half - timer_anchor) / 2) / 10;

        break;

    case 0x19:
        data = 0xff;

        break;

    case 0x20:
        data = dma_sar0.b.l;

        break;

    case 0x21:
        data = dma_sar0.b.h;

        break;

    case 0x22:
        data = dma_sar0.b.h2 & (Z180_SAR0_MASK >> 16);

        break;

    case 0x23:
        data = dma_dar0.b.l;

        break;

    case 0x24:
        data = dma_dar0.b.h;

        break;

    case 0x25:
        data = dma_dar0.b.h2 & (Z180_DAR0_MASK >> 16);

        break;

    case 0x26:
        data = dma_bcr[0].b.l;

        break;

    case 0x27:
        data = dma_bcr[0].b.h;

        break;

    case 0x28:
        data = dma_mar1.b.l;

        break;

    case 0x29:
        data = dma_mar1.b.h;

        break;

    case 0x2a:
        data = dma_mar1.b.h2 & (Z180_MAR1_MASK >> 16);

        break;

    case 0x2b:
        data = dma_iar1.b.l;

        break;

    case 0x2c:
        data = dma_iar1.b.h;

        break;

    case 0x2d:
        data = dma_iar1.b.h2 & (Z180_IAR1_MASK >> 16);

        break;

    case 0x2e:
        data = dma_bcr[1].b.l;

        break;

    case 0x2f:
        data = dma_bcr[1].b.h;

        break;

    case 0x30:
        data = dstat | ~Z180_DSTAT_MASK | Z180_DSTAT_DWE0 | Z180_DSTAT_DWE1;

        break;

    case 0x31:
        data = dmode | ~Z180_DMODE_MASK;

        break;

    case 0x32:
        data = dcntl;

        break;

    case 0x33:
        data = il & Z180_IL_MASK;

        break;

    case 0x34:
        data = itc | ~Z180_ITC_MASK;

        break;

    case 0x36:
        data = rcr | ~Z180_RCR_MASK;

        break;

    case 0x38:
        data = mmu_cbr;

        break;

    case 0x39:
        data = mmu_bbr;

        break;

    case 0x3a:
        data = mmu_cbar;

        break;

    case 0x3e:
        data = omcr | Z180_OMCR_M1TE | ~Z180_OMCR_MASK;

        break;

    case 0x3f:
        data = iocr | ~(0xe0);

        break;

    default:
        data = 0xff;

        break;
    }

    return data;
}

// Route relocated internal-register writes through the same peripheral decode.
void Z180::z180_writecontrol(uint32_t port, uint8_t data) {
    instruction_fast = false;
    // normal external write port
    write_io(port, data);

    // store the data in the internal register
    z180_internal_port_write(port & (0x3f), data);
}

// Apply register-specific masks and side effects, including DMA requests,
// clock changes and MMU mapping (Zilog UM0050, internal I/O registers).
// port is the six-bit internal offset; data is the sampled CPU bus byte.
// Use the map above the read decoder. Register writes can change clocks and
// memory views, so affected cases notify the scheduler before it runs again.
void Z180::z180_internal_port_write(uint8_t port, uint8_t data) {
    bool const timer_register = (port >= 0x0c && port <= 0x10) ||
                                (port >= 0x14 && port <= 0x18) || port == 0x3f;
    if (timer_register)
        sync_timers();
    // TN0053 pp1-2: these enhancements are absent from original K silicon.
    if (silicon == Silicon::K && (port == 0x12 || port == 0x13 || (port >= 0x1a && port <= 0x1e)))
        return;
    switch (port) {
    case 0x12:
    case 0x13:
        asci[port & 1].asext_w(data);
        break;

    case 0x1a:
    case 0x1c:
        asci[(port - 0x1a) >> 1].astcl_w(data);
        break;

    case 0x1b:
    case 0x1d:
        asci[(port - 0x1a) >> 1].astch_w(data);
        break;

    case 0x1e:

        prepare_deferred_clock_change();
        cmr = data & Z180_CMR_MASK;

        notify_clock_changed();
        break;

    case 0x1f:

        prepare_deferred_clock_change();
        ccr = silicon == Silicon::K ? data & ~0x08 : data;

        notify_clock_changed();
        break;

    case 0x00:
    case 0x01:
        asci[port & 1].cntla_w(data);
        break;

    case 0x02:
    case 0x03:
        asci[port & 1].cntlb_w(data);
        break;

    case 0x04:
    case 0x05:
        asci[port & 1].stat_w(data);
        break;

    case 0x06:
    case 0x07:
        asci[port & 1].tdr_w(data);
        break;

    case 0x08:
    case 0x09:
        asci[port & 1].rdr_w(data);
        break;

    case 0x0a:
        csio.cntr_w(data);
        break;

    case 0x0b:
        csio.trdr_w(data);
        break;

    case 0x0c:

        tmdr[0].b.l = data;
        tmdr_value[0] = (tmdr_value[0] & 0xff00) | tmdr[0].b.l;
        break;

    case 0x0d:

        tmdr[0].b.h = data;
        tmdr_value[0] = (tmdr_value[0] & 0x00ff) | (tmdr[0].b.h << 8);
        break;

    case 0x0e:

        rldr[0].b.l = data;
        break;

    case 0x0f:

        rldr[0].b.h = data;
        break;

    case 0x10:

        // UM0050 pp158,163: enabling counting preserves a programmed TMDR.
        tcr = (tcr & (Z180_TCR_TIF1 | Z180_TCR_TIF0)) | (data & ~(Z180_TCR_TIF1 | Z180_TCR_TIF0));
        // TOUT is the timer output shared with address pin A18 (UM0050 p. 164).
        // The model retains timeout parity across pin selection: before the
        // first timeout it is high. Forced levels are applied by handle_io_timers
        // at a timeout, rather than immediately by this write.
        tout_level = !prt1_timeout_odd;
        cycle_update_direct_reads();
        break;

    case 0x14:

        tmdr[1].b.l = data;
        tmdr_value[1] = (tmdr_value[1] & 0xff00) | tmdr[1].b.l;
        break;

    case 0x15:

        tmdr[1].b.h = data;
        tmdr_value[1] = (tmdr_value[1] & 0x00ff) | (tmdr[1].b.h << 8);
        break;

    case 0x16:

        rldr[1].b.l = data;
        break;

    case 0x17:

        rldr[1].b.h = data;
        break;

    case 0x18:

        // FRC is read-only
        break;

    case 0x20:

        dma_sar0.b.l = data;
        break;

    case 0x21:

        dma_sar0.b.h = data;
        break;

    case 0x22:

        dma_sar0.b.h2 = data & (Z180_SAR0_MASK >> 16);
        break;

    case 0x23:

        dma_dar0.b.l = data;
        break;

    case 0x24:

        dma_dar0.b.h = data;
        break;

    case 0x25:

        dma_dar0.b.h2 = data & (Z180_DAR0_MASK >> 16);
        break;

    case 0x26:

        dma_bcr[0].b.l = data;
        break;

    case 0x27:

        dma_bcr[0].b.h = data;
        break;

    case 0x28:

        dma_mar1.b.l = data;
        break;

    case 0x29:

        dma_mar1.b.h = data;
        break;

    case 0x2a:

        dma_mar1.b.h2 = data & (Z180_MAR1_MASK >> 16);
        break;

    case 0x2b:

        dma_iar1.b.l = data;
        break;

    case 0x2c:

        dma_iar1.b.h = data;
        break;

    case 0x2d:

        dma_iar1.b.h2 = data & (Z180_IAR1_MASK >> 16);
        break;

    case 0x2e:

        dma_bcr[1].b.l = data;
        break;

    case 0x2f:

        dma_bcr[1].b.h = data;
        break;

    case 0x30:

        // UM0050 pp95-97: DWE=1 protects DE, and reads back as 1.
        {
            uint8_t const previous = dstat;
            uint8_t const writable = Z180_DSTAT_DIE0 | Z180_DSTAT_DIE1 |
                                     ((data & Z180_DSTAT_DWE0) ? 0 : Z180_DSTAT_DE0) |
                                     ((data & Z180_DSTAT_DWE1) ? 0 : Z180_DSTAT_DE1);
            dstat = (dstat & ~writable) | (data & writable) | Z180_DSTAT_DWE0 | Z180_DSTAT_DWE1;
            if (data & writable & (Z180_DSTAT_DE0 | Z180_DSTAT_DE1))
                dstat |= Z180_DSTAT_DME;
            // UM0050 pp107-109: after enabling a channel, edge-sense
            // transfers are requested by DREQ falling edges. Events from
            // before that enable are not a queue of pending DMA requests.
            for (unsigned ch = 0; ch < 2; ++ch)
                if ((dstat & (0x40 << ch)) && (!(previous & (0x40 << ch)) || !(previous & Z180_DSTAT_DME)))
                    dma_edge_sampled[ch] = dma_edge_accepted[ch] = dma_edge_generation[ch];
            // UM0050 p106: a memory-to-memory start follows one further CPU
            // MC. The register-write MC itself does not satisfy that delay.
            if ((data & writable & Z180_DSTAT_DE0) && ((dmode >> 2) & 3) < 2 &&
                ((dmode >> 4) & 3) < 2) {
                dma_allowed = false;
                dma_start_pending = cycle_executing;
            }
        }
        break;

    case 0x31:

        dmode = data & Z180_DMODE_MASK;
        break;

    case 0x32:

        // Switching to edge-sensed DREQ starts with a fresh edge history;
        // a previously asserted level is not a new request (UM0050 pp. 107-110).
        for (unsigned ch = 0; ch < 2; ++ch)
            if ((data & ~dcntl) & (4 << ch))
                dma_edge_sampled[ch] = dma_edge_accepted[ch] = dma_edge_generation[ch];
        dcntl = data;
        break;

    case 0x33:

        il = data & Z180_IL_MASK;
        break;

    case 0x34:

        // UFO is read-only; TRAP can be cleared but not set by a write.
        // Low three bits independently gate INT0-2 (UM0050 pp. 69-72).
        itc = (itc & Z180_ITC_UFO) | (itc & data & Z180_ITC_TRAP) | (data & 7);
        break;

    case 0x36:

        rcr = data & Z180_RCR_MASK;
        break;

    case 0x38:

        mmu_cbr = data;
        z180_mmu();
        break;

    case 0x39:

        mmu_bbr = data;
        z180_mmu();
        break;

    case 0x3a:

        mmu_cbar = data;
        z180_mmu();
        break;

    case 0x3e:

        omcr = data & Z180_OMCR_MASK;
        break;

    case 0x3f:

        iocr = data & (0xe0);
        for (auto &a : asci)
            a.set_iostop(iocr & 0x20);
        csio.set_iostop(iocr & 0x20);
        break;

    default:

        break;
    }
    if (timer_register)
        schedule_timers();
}

// Merge the supplied input-bit image into iol. Output bits belong to their
// peripheral callbacks and are ignored here. This helper does not dispatch
// serial edges (UM0050 pp. 7-13).
void Z180::z180_write_iolines(uint32_t data) {
    uint32_t changes = iol ^ data;

    /* I/O asynchronous clock 0 (active high) or DREQ0 (mux) */
    if (changes & Z180_CKA0) {

        iol = (iol & ~Z180_CKA0) | (data & Z180_CKA0);
    }

    /* I/O asynchronous clock 1 (active high) or TEND1 (mux) */
    if (changes & Z180_CKA1) {

        iol = (iol & ~Z180_CKA1) | (data & Z180_CKA1);
    }

    /* I/O serial clock (active high) */
    if (changes & Z180_CKS) {

        iol = (iol & ~Z180_CKS) | (data & Z180_CKS);
    }

    /* I   clear to send 0 (active low) */
    if (changes & Z180_CTS0) {

        iol = (iol & ~Z180_CTS0) | (data & Z180_CTS0);
    }

    /* I   clear to send 1 (active low) or RXS (mux) */
    if (changes & Z180_CTS1) {

        iol = (iol & ~Z180_CTS1) | (data & Z180_CTS1);
    }

    /* I   data carrier detect (active low) */
    if (changes & Z180_DCD0) {

        iol = (iol & ~Z180_DCD0) | (data & Z180_DCD0);
    }

    /* I   data request DMA ch 0 (active low) or CKA0 (mux) */
    if (changes & Z180_DREQ0) {

        iol = (iol & ~Z180_DREQ0) | (data & Z180_DREQ0);
    }

    /* I   data request DMA ch 1 (active low) */
    if (changes & Z180_DREQ1) {

        iol = (iol & ~Z180_DREQ1) | (data & Z180_DREQ1);
    }

    /* I   asynchronous receive data 0 (active high) */
    if (changes & Z180_RXA0) {

        iol = (iol & ~Z180_RXA0) | (data & Z180_RXA0);
    }

    /* I   asynchronous receive data 1 (active high) */
    if (changes & Z180_RXA1) {

        iol = (iol & ~Z180_RXA1) | (data & Z180_RXA1);
    }

    /* I   clocked serial receive data (active high) or CTS1 (mux) */
    if (changes & Z180_RXS) {

        iol = (iol & ~Z180_RXS) | (data & Z180_RXS);
    }

    /*   O request to send (active low) */
    if (changes & Z180_RTS0) {
    }

    /*   O transfer end 0 (active low) or CKA1 (mux) */
    if (changes & Z180_TEND0) {
    }

    /*   O transfer end 1 (active low) */
    if (changes & Z180_TEND1) {
    }

    /*   O transfer out (PRT channel, active low) or A18 (mux) */
    if (changes & Z180_A18_TOUT) {
    }

    /*   O asynchronous transmit data 0 (active high) */
    if (changes & Z180_TXA0) {
    }

    /*   O asynchronous transmit data 1 (active high) */
    if (changes & Z180_TXA1) {
    }

    /*   O clocked serial transmit data (active high) */
    if (changes & Z180_TXS) {
    }
}

// Restore CPU and peripheral reset state while keeping external bus callbacks.
// Zilog UM0050, Reset and register reset-value descriptions.
void Z180::reset_core() {
    previous_pc = 0;
    pc32 = 0;
    sp32 = 0;
    af32 = 0;
    bc32 = 0;
    de32 = 0;
    hl32 = 0;
    ix32 = 0;
    iy32 = 0;
    AF2.d = 0;
    BC2.d = 0;
    DE2.d = 0;
    HL2.d = 0;
    R = 0;
    R2 = 0;
    IFF1 = 0;
    IFF2 = 0;
    HALT = 0;
    IM = 0;
    I = 0;
    tmdr_latch = 0;
    read_tcr_tmdr[0] = 0;
    read_tcr_tmdr[1] = 0;
    iol = 0;
    memset(mmu, 0, sizeof(mmu));
    tmdrh[0] = 0;
    tmdrh[1] = 0;
    tmdr_value[0] = 0xffff;
    tmdr_value[1] = 0xffff;
    tmdr[0].w = tmdr[1].w = 0xffff;
    rldr[0].w = rldr[1].w = 0xffff; // UM0050 pp. 157, 159, 164: timer reset values.
    nmi_state = 0;
    nmi_pending = 0;
    irq_state[0] = 0;
    irq_state[1] = 0;
    irq_state[2] = 0;
    after_EI = 0;
    ea = 0;

    std::copy(std::begin(cc_default), std::end(cc_default), cc);
    ix16 = iy16 = 0xffff; /* IX and IY are FFFF after a reset! */
    f8 = 0;               // UM0050 p178: all defined F bits reset to zero.

    for (int i = 0; i <= Z180_INT_MAX; i++) {
        int_pending[i] = 0;
    }

    frc = 0xff;
    frc_prescale = 0;
    bus_released = false;

    /* reset io registers */
    tcr = 0x00;
    prt1_timeout_odd = false;
    tout_level = true;
    dma_iar1.b.h2 = 0x00;
    dstat = Z180_DSTAT_DWE1 | Z180_DSTAT_DWE0;
    dmode = 0x00;
    dcntl = 0xf0; // maximum number of memory and I/O wait states
    il = 0x00;
    itc = Z180_ITC_ITE0;
    rcr = Z180_RCR_REFE | Z180_RCR_REFW;
    mmu_cbr = 0x00;
    mmu_bbr = 0x00;
    mmu_cbar = 0xf0;
    omcr = Z180_OMCR_M1E | Z180_OMCR_M1TE | Z180_OMCR_IOC;
    iocr = 0x00;

    z180_mmu();
    cycle_reset();
    timer_anchor = 0;
    timer_deadline = UINT64_MAX;
}

// Advance the FRC divider and PRT counters by cycles whole PHI. Serial
// deadlines are dispatched separately by the scheduler (UM0050 pp. 157-164).
void Z180::handle_io_timers(int cycles) {
    if (cycles <= 0)
        return;
    // The FRC falls every ten clocks; a falling edge to an even FRC clocks
    // both PRTs. Retain the timeout parity and the final multiplexed TOUT
    // level; instruction/DMA boundaries observe those outputs.
    const unsigned frc_ticks = (frc_prescale + cycles) / 10;
    frc_prescale = (frc_prescale + cycles) % 10;
    const unsigned ticks = (frc_ticks + (frc & 1)) / 2;
    frc -= frc_ticks;
    if (!ticks || (iocr & 0x20))
        return;
    for (unsigned ch = 0; ch != 2; ++ch) {
        if (!(tcr & (1 << ch)))
            continue;
        const unsigned value = tmdr_value[ch];
        const unsigned period = unsigned(rldr[ch].w) + 1;
        // Zero is a real count state: reload occurs on its following tick.
        const unsigned first = value ? value : period;
        const unsigned timeouts = ticks < first ? 0 : 1 + (ticks - first) / period;
        if (timeouts) {
            tcr |= Z180_TCR_TIF0 << ch;
            if (ch == 1) {
                prt1_timeout_odd ^= bool(timeouts & 1);
                bool const old_tout = tout_level;
                switch (tcr & (Z180_TCR_TOC1 | Z180_TCR_TOC0)) {
                case 0:
                    tout_level = !prt1_timeout_odd;
                    break;
                case Z180_TCR_TOC0:
                    tout_level ^= bool(timeouts & 1);
                    break;
                case Z180_TCR_TOC1:
                    tout_level = false;
                    break;
                case Z180_TCR_TOC1 | Z180_TCR_TOC0:
                    tout_level = true;
                    break;
                }
                if (old_tout != tout_level && (tcr & 0x0c))
                    cycle_update_direct_reads();
            }
        }
        if (ticks <= value)
            tmdr_value[ch] = value - ticks;
        else {
            const unsigned remainder = (ticks - value) % period;
            tmdr_value[ch] = remainder ? period - remainder : 0;
        }
    }
}

// Run the machine-cycle engine for the current budget.
void Z180::execute_run() {
#ifdef GS2_SS_TESTING
    // Direct cycle fixtures can change private register fields between runs.
    if (!executing)
        schedule_timers();
#endif
    execute_machine_cycles();
#ifdef GS2_SS_TESTING
    if (!executing)
        sync_timers();
#endif
}

// Latch external interrupt/DMA pin changes; NMI edge memory is distinct
// from level-sensitive inputs (UM0050, Interrupts and DMA chapters).
void Z180::execute_set_input(int irqline, int state) {
    instruction_fast = false;
    if (irqline == INPUT_LINE_NMI) {
        /* A new logical assertion latches NMI; the physical active-low pin falls (UM0050 p. 73). */
        if (nmi_state == 0 && state != 0)
            nmi_pending = 1;
        nmi_state = state;
    } else {

        if (irqline == Z180_INPUT_LINE_IRQ0 || irqline == Z180_INPUT_LINE_IRQ1 || irqline == Z180_INPUT_LINE_IRQ2) {
            /* update the IRQ state */
            irq_state[irqline] = state;

            /* the main execute loop will take the interrupt */
        } else if (irqline == Z180_INPUT_LINE_DREQ0) {
            if (state == 1 && !(iol & Z180_DREQ0))
                ++dma_edge_generation[0];
            uint32_t iol_arg = iol & ~Z180_DREQ0;
            if (state == 1)
                iol_arg |= Z180_DREQ0;
            z180_write_iolines(iol_arg);
        } else if (irqline == Z180_INPUT_LINE_DREQ1) {
            if (state == 1 && !(iol & Z180_DREQ1))
                ++dma_edge_generation[1];
            uint32_t iol_arg = iol & ~Z180_DREQ1;
            if (state == 1)
                iol_arg |= Z180_DREQ1;
            z180_write_iolines(iol_arg);
        }
    }
}

// Z180 machine-cycle execution, UM0050 Tables 51-52 and Figs 9-18,32-49.
// Instruction handlers remain the only implementation of ISA arithmetic.
// Production handlers stay on their worker stack. Test execution can suspend
// at a clock edge and replay completed reads; completed writes and peripheral
// reads never repeat.

// Rebuild direct-read pages after MMU or board-overlay changes. Pages without
// board-provided pointers continue through the memory callbacks.
void Z180::cycle_update_direct_reads() {
    instruction_fast = false;
    if (!direct_read_bus) {
        direct_logical.fill(nullptr);
        return;
    }
    for (unsigned page = 0; page < 16; ++page) {
        uint32_t const physical = a18_tout_address(mmu[page]);
        auto const base = direct_read[(physical >> 16) & 15];
        direct_logical[page] = base ? base + (physical & 0xffff) : nullptr;
    }
}

// Save instruction-entry registers for replay after a mid-instruction deadline.
#ifdef GS2_SS_TESTING
void Z180::cycle_save_state() {
    Z180Register const *r[] = {&PC, &SP, &AF, &BC, &DE, &HL, &IX, &IY, &AF2, &BC2, &DE2, &HL2};
    for (unsigned n = 0; n < 12; ++n)
        instruction_start.regs[n] = r[n]->d;
    uint8_t const c[] = {R, R2, IFF1, IFF2, HALT, IM, I, after_EI, 0};
    std::copy(std::begin(c), std::end(c), instruction_start.control);
}

// Restore the instruction-entry registers while retaining recorded bus accesses;
// replay consumes recorded reads and must not repeat completed writes.
void Z180::cycle_restore_state() {
    Z180Register *r[] = {&PC, &SP, &AF, &BC, &DE, &HL, &IX, &IY, &AF2, &BC2, &DE2, &HL2};
    for (unsigned n = 0; n < 12; ++n)
        r[n]->d = instruction_start.regs[n];
    uint8_t *c[] = {&R, &R2, &IFF1, &IFF2, &HALT, &IM, &I, &after_EI};
    for (unsigned n = 0; n < 8; ++n)
        *c[n] = instruction_start.control[n];
    previous_pc = instruction_start.regs[0];
    cursor = bus_number = nominal = 0;
    family = Z180_TABLE_op;
    opcode = 0;
    extra_cycles = 0;
    trapping = third_trap = false;
}

#endif
// Clear the resumable bus-cycle state at processor reset.
void Z180::cycle_reset() {
    board_edge_at = UINT64_MAX;
    cycle = {};
    instruction_fast = instruction_whole = instruction_active = cycle_executing = false;
    reset_cancelled = scheduler_in_handler = false;
    fast_irq_valid = false;
    // No decoded instruction survives reset, including a cancelled tail.
    family = opcode = extra_cycles = 0;
    trapping = third_trap = false;
#ifdef GS2_SS_TESTING
    history_count = cursor = 0;
    // Discard the test continuation and its recorded bus accesses on reset.
    instruction_start = {};
    std::fill(std::begin(history), std::end(history), cycle_record{});
    reti_notified = trap_started = interrupt_started = false;
#endif
    bus_number = nominal = 0;
    bus_ack = sampled_bus_request = false;
    boundary_allowed = true;
    dma_allowed = dma_start_pending = false;
    refresh_pending = refresh_after_sleep = false;
    refresh_phase = phi_half = advance_left = refresh_address = 0;
    half_phi = retired = 0;
    timer_anchor = 0;
    timer_deadline = UINT64_MAX;
    power_mode = 0;
    wake_remaining = 0;
#ifdef GS2_SS_TESTING
#endif
    dma_channel = dma_continue = 0xff;
    dma_stage = dma_byte = 0;
    dma_edge[0] = dma_edge[1] = false;
    sampled_iol = 0;
    for (unsigned n = 0; n < 2; ++n)
        dma_edge_generation[n] = dma_edge_accepted[n] = dma_edge_sampled[n] = 0;
    instruction_interrupt = sampled_interrupt = -1;
    ack_vector = -1;
    interrupt_recovery = dma_nmi = false;
}

// Publish the selected bus edge to the board and optional test observer.
// IORQ assertion/release is distinct from the data sample (UM0050 pp. 16-17).
inline void Z180::cycle_emit(uint8_t edge) {
#ifdef GS2_SS_TESTING
    if (cycle_observer || io_strobe)
        sync_timers();
#endif
    // UM0050 p108 Fig49: TEND is low from T1 falling through T3
    // falling of the last write cycle, including all inserted waits.
    if (cycle.owner == cycle_owner::dma && dma_stage && dma_bcr[dma_channel].w == 1 && (edge == 1 || edge == 4)) {
        unsigned const ch = dma_channel;
        if (edge == 1)
            iol |= 0x20000 << ch;
        else
            iol &= ~(0x20000 << ch);
        tend(ch, edge == 1 ? 1 : 0);
    }
    // UM0050 pp16-17 Figs7/8: IOC=1 selects at T1 falling;
    // IOC=0 selects at T2 rising. Internal I/O also asserts IORQ (UM p44).
    if (io_strobe && cycle.io_select_edge && (edge == cycle.io_select_edge || edge == 4))
        io_strobe(cycle.address, cycle.kind == cycle_kind::output, cycle.data, edge != 4);
    if (cycle_observer)
        cycle_observer(half_phi, uint8_t(cycle.kind) + 16 * uint8_t(cycle.owner), edge, cycle.address, cycle.data);
#ifdef GS2_SS_TESTING
    if (cycle_observer || io_strobe)
        schedule_timers();
#endif
}

// Meet the board at an exact half-PHI boundary. The card worker services
// its inputs here and resumes this same handler after the next grant.
void Z180::cycle_suspend() {
    if (reset_cancelled) return;
    if (timer_limit <= half_phi) {
        // Only the timer deadline was reached. The phase engine clocks the
        // timeout edge; the next instruction can then use the fast path.
        timer_limit = UINT64_MAX;
        fast_irq_valid = false;
        instruction_fast = false;
        uint64_t const end = std::min(budget_end, board_edge_at);
        if (!yield_requested && end > half_phi) {
            icount = int(end - half_phi);
            return;
        }
    }
    if (scheduler) {
        if (board_edge_at <= half_phi) {
            board_edge_at = UINT64_MAX;
            timer_limit = UINT64_MAX;
            board_edge();
            fast_irq_valid = false;
            instruction_fast = false;
            // Only the current time slice ended. No other board or serial event
            // was scheduled within it.
            // The callback may have scheduled the board's next edge.
            if (!yield_requested && scheduler_limit > half_phi) {
                icount = int(std::min(scheduler_limit, board_edge_at) - half_phi);
                return;
            }
        }
        if (refill_budget(true) || reset_cancelled)
            return;
    }
    // Test execution can suspend in the middle of an instruction handler.
    // Production execution stays on this stack until more CPU time is available.
#ifdef GS2_SS_TESTING
    throw cycle_yield{};
#else
    z180_error("%s", "Z180: active instruction has no scheduler grant");
#endif
}

// Retain the completed bus state so an observer can inspect it.
void Z180::cycle_completed_record(cycle_kind kind, uint32_t address, uint8_t data, bool no_arbitration) {
    bool const internal = kind == cycle_kind::internal;
    cycle_initialize(cycle_owner::cpu, kind, internal ? 0 : MMU_REMAP_ADDR(address), data, no_arbitration);
    cycle.nominal = internal ? 1 : 3;
    cycle.elapsed = internal ? 2 : 6 + 2 * memory_wait_states();
    cycle.sampled = !internal;
    cycle_complete(internal ? 1 : 9);
}

// Derive the next PRT timeout from the materialized FRC phase. Each timeout
// can set TIF or change TOUT, even before the CPU next reads a register
// (UM0050 pp. 157-164). Both PRTs share the FRC's divide-by-20 clock.
void Z180::schedule_timers() {
    timer_deadline = UINT64_MAX;
    if (!(tcr & 3) || (iocr & 0x20))
        return;
    unsigned ticks = UINT_MAX;
    for (unsigned ch = 0; ch != 2; ++ch) {
        if (tcr & (1 << ch)) {
            unsigned const value = tmdr_value[ch];
            ticks = std::min(ticks, value ? value : unsigned(rldr[ch].w) + 1);
        }
    }
    // Each PRT decrement needs two FRC steps; the current odd/even phase
    // may supply one already. Each FRC step takes ten PHI. Subtract its
    // elapsed prescaler clocks, then convert the remaining PHI to half-PHI.
    uint64_t const falls = 2 * uint64_t(ticks) - (frc & 1);
    timer_deadline = timer_anchor + 2 * (10 * falls - frc_prescale);
}

// Bring the divider/reload counters to the last completed PHI edge. An odd
// half-PHI remains pending. Chunk long spans because handle_io_timers takes
// a signed clock count, then recompute the next timeout from the new phase.
void Z180::sync_timers() {
    uint64_t const edge = half_phi - phi_half;
    uint64_t elapsed = (edge - timer_anchor) / 2;
    while (elapsed) {
        int const clocks = int(std::min<uint64_t>(elapsed, INT_MAX));
        handle_io_timers(clocks);
        elapsed -= clocks;
    }
    timer_anchor = edge;
    schedule_timers();
}

// Counter arithmetic can wait until a timeout or register observation.
// half_phi already includes the span; the phi argument is not used here.
inline void Z180::advance_dividers(unsigned phi) {
    (void)phi;
    if (half_phi >= timer_deadline)
        sync_timers();
}

// Find the next timer edge that limits fast execution: the cached timer
// timeout. UINT64_MAX means no timer stop is needed.
uint64_t Z180::timer_horizon() const {
    return timer_deadline;
}

// Limit CPU time so a fast opcode fetch cannot cross a timer change. Return
// false to use individual bus phases when the next edge is too close.
bool Z180::admit_timers() {
    if (timer_limit != UINT64_MAX) {
        // Replace the earlier stop; a TCR or reload write may have moved it.
        timer_limit = UINT64_MAX;
        if (!yield_requested)
            icount = int(std::min(budget_end, board_edge_at) - half_phi);
    }
    if (!(tcr & 3))
        return true;
    uint64_t const horizon = timer_horizon();
    if (horizon == UINT64_MAX)
        return true;
    // Each machine cycle samples inputs before it ends, so a fast span ending
    // at the timeout sees the earlier counter state. The opcode fetch and its
    // DCNTL waits must fit before that edge.
    if (horizon < half_phi + 8 + 2 * memory_wait_states())
        return false;
    if (horizon - half_phi < uint64_t(icount)) {
        timer_limit = horizon;
        icount = int(horizon - half_phi);
    }
    return true;
}

// Apply one granted clock span. Both the quiet and resumable paths use the
// same divider and refresh accounting (UM0050 pp. 86, 157-164).
inline void Z180::cycle_clock_step(unsigned ticks) {
    icount -= ticks;
    half_phi += ticks;
    cycle.elapsed += ticks;
    unsigned const phi = (phi_half + ticks) / 2;
    phi_half = (phi_half + ticks) & 1;
    advance_dividers(phi);
    unsigned const period = 20U << (rcr & 3);
    if ((rcr & 0x80) && (refresh_phase % period + ticks >= period) && !bus_ack)
        refresh_pending = true;
    refresh_phase = (refresh_phase + ticks) % 160;
}

// Advance only a fully granted interval with no pending partial step.
inline void Z180::cycle_advance_quiet(unsigned ticks) {
    assert(!advance_left && icount >= int(ticks));
    cycle_clock_step(ticks);
}

// Consume half-PHI clocks and dispatch peripheral events in emulated-time order.
bool Z180::cycle_advance(unsigned ticks) {
    if (!advance_left)
        advance_left = ticks;
    // No callback occurs inside this span. FRC/PRT already implement
    // their divider and reload recurrences arithmetically; update them
    // once at the next externally observable edge or scheduler boundary.
    while (advance_left) {
        if (icount <= 0) {
            cycle_suspend();
            if (reset_cancelled) { advance_left = 0; cycle.active = false; return false; }
        }
        unsigned const n = std::min<unsigned>(advance_left, icount);
        advance_left -= n;
        cycle_clock_step(n);
        if (advance_left)
            cycle_suspend();
    }
    return true;
}

// A step wholly inside the available time needs no scheduler callback. Use
// the resumable path for a partial step or a cancelled slice.
inline bool Z180::cycle_advance_granted(unsigned ticks) {
    if (!advance_left && !reset_cancelled && !yield_requested &&
        icount >= int(ticks)) {
        cycle_advance_quiet(ticks);
        return true;
    }
    return cycle_advance(ticks);
}

// Create a bus cycle with its initial strobes, synchronization and wait count.
// Zilog UM0050 pp. 16-30 describes the external memory/I/O phases.
void Z180::cycle_begin(cycle_owner owner, cycle_kind kind, uint32_t address, uint8_t data, bool no_arbitration,
                       bool force_memory_wait) {
    assert(!cycle.active);
    cycle_initialize(owner, kind, address, data, no_arbitration);
    bool const io = kind == cycle_kind::input || kind == cycle_kind::output;
    if (io)
        cycle.io_select_edge = (omcr & 0x20) ? 1 : 2;
    bool const write = kind == cycle_kind::write || kind == cycle_kind::output;
    bool const internal = kind == cycle_kind::internal || kind == cycle_kind::release;
    if (internal)
        cycle.nominal = 1;
    else if (kind == cycle_kind::refresh)
        cycle.nominal = (rcr & 0x40) ? 3 : 2;
    else if (kind == cycle_kind::acknowledge) {
        cycle.nominal = 5;
        cycle.waits = owner == cycle_owner::cpu && instruction_interrupt == 2
                          ? (((dcntl >> 4) & 3) ? ((dcntl >> 4) & 3) + 3 : 2)
                          : 2;
    } else if (kind == cycle_kind::nmi)
        cycle.waits = 0;
    else {
        cycle.waits = io ? (is_internal_io_address(address) ? 0 : io_wait_states()) : memory_wait_states();
        if (io && is_internal_io_address(address)) {
            unsigned const reg = address & 0x3f;
            bool const data_register = (write ? reg == 6 || reg == 7 : reg == 8 || reg == 9) || reg == 0x0b ||
                                       (reg >= 0x0c && reg <= 0x0f) || (reg >= 0x14 && reg <= 0x17);
            // Model the UM0050 p. 30 synchronization bound with a shared five-PHI phase.
            // Flush counters before using their phase. T3's candidate rising
            // edge is four half-PHI ahead; derive q from the shared FRC phase.
            // This is the same future phase as evaluation at T2 falling.
            if (data_register) {
                sync_timers();
                unsigned const q = (frc_prescale + (phi_half + 4) / 2) % 5;
                cycle.waits = (5 - q) % 5;
            }
        }
        if (bus_wait && !(io && is_internal_io_address(address)) &&
            (io || write || force_memory_wait || !cycle_direct_read(address))) {
#ifdef GS2_SS_TESTING
            sync_timers();
#endif
            cycle.waits = std::max<unsigned>(cycle.waits, bus_wait(address, write, io));
        }
    }
    cycle_emit(0);
}

// Perform a memory transfer at its documented sample edge. The direct-page
// and mapped paths use the same byte and bus-drive rules (UM0050 pp. 19-23).
inline void Z180::cycle_sample_memory() {
    auto const direct = cycle.kind != cycle_kind::write ? cycle_direct_read(cycle.address) : nullptr;
    if (direct)
        cycle.data = drive_direct_read(*direct);
    else if (cycle.kind == cycle_kind::write)
        write_memory(cycle.address, cycle.data);
    else
        cycle.data = read_memory(cycle.address);
}

// Sample a fetched opcode once; test execution can replay the recorded value.
void Z180::cycle_sample_opcode() {
    if (cycle.kind == cycle_kind::fetch || cycle.kind == cycle_kind::halt || cycle.kind == cycle_kind::nmi) {
        cycle_sample_memory();
        cycle.sampled = true;
        cycle_emit(4);
    } else if (cycle.kind == cycle_kind::acknowledge) {
        if (instruction_interrupt == 2 && ack_vector < 0) {
            ack_vector = bus.irq_ack ? bus.irq_ack() : 0xff;
        }
        cycle.data = ack_vector < 0 ? 0 : uint8_t(ack_vector);
        cycle.sampled = true;
        cycle_emit(4);
    }
}

// Perform the read/write callback at the bus data edge, not at instruction
// retirement. Test replay records completed effects across a suspension.
void Z180::cycle_sample_data() {
    if (!cycle.sampled) {
        // Set the guard before a handler that can abort this timeslice.
        cycle.sampled = true;
        switch (cycle.kind) {
            case cycle_kind::operand:
            case cycle_kind::read:
            case cycle_kind::write:
                cycle_sample_memory();
                break;
            case cycle_kind::input:
                cycle.data =
                    is_internal_io_address(cycle.address) ? z180_readcontrol(cycle.address) : read_io(cycle.address);
                break;
            case cycle_kind::output:
                if (is_internal_io_address(cycle.address))
                    z180_writecontrol(cycle.address, cycle.data);
                else
                    write_io(cycle.address, cycle.data);
                break;
            default:
                break;
            }
        cycle_emit(4);
        // I/O can alter the clock, timers, DMA or pins. Re-evaluate
        // arbitration for the rest of this instruction after that edge.
        if (cycle.kind == cycle_kind::input || cycle.kind == cycle_kind::output)
            instruction_fast = false;
    }
}

// The waits recorded for this access extend it at T2/TW.
bool Z180::cycle_wait_pending() const {
    return cycle.waits;
}

// Execute machine-cycle edges, sampling WAIT at T2/TW falling. Internal I/O
// retains strobes but ignores external wait insertion (UM0050 pp. 19, 27, 44).
// phase is a continuation label, not a T-state number. 0 starts T1; 1 is
// T1 falling; 2 is T2 rising; 3 is T2 falling and the WAIT decision. 7/8
// perform the two halves of each TW. 4 reaches T3; 5 resumes after an
// opcode sample and 6 after an ordinary data sample. 9 completes the cycle;
// 10 handles TTP, the extra trap-read clock. Internal/release cycles use
// phases 0/1 for their last falling/rising edges. A deadline may stop any phase.
// Data callbacks occur only at the sample phase, so resuming cannot write twice.
void Z180::cycle_run() {
    // Phase numbers distinguish T1 rise/fall, T2 rise/fall and T3 rise/fall.
    // WAIT is sampled at T2/TW falling (UM0050 pp19,27); no memory or I/O
    // operation is committed until its documented sample/WR release edge.
    while (cycle.active) {
        // The same phase rules below can combine adjacent unobserved edges
        // only within the current grant. TEND, I/O and live clocks stay exact.
        bool const span = !cycle_observer && icount > 0 && !advance_left &&
            cycle.nominal == 3 && cycle.kind < cycle_kind::internal && !cycle.ttp && !cycle.trailing &&
            !cycle.io_select_edge && !(tcr & 3) && cycle.waits <= 32765 &&
            !(cycle.owner == cycle_owner::dma && dma_stage && dma_bcr[dma_channel].w == 1);
        bool const internal = cycle.kind == cycle_kind::internal || cycle.kind == cycle_kind::release;
        if (internal || cycle.kind == cycle_kind::refresh) {
            if (!cycle.phase) {
                if (!advance_left && internal) {
                    cycle_sample_dma_inputs();
                }
                if (!cycle_advance(2 * cycle.nominal - 1)) return;
                cycle.phase = 1;
                if (internal) {
                    sampled_bus_request = bus_released;
                    cycle_sample_irq_edge();
                }
                cycle_emit(1);
            }
            if (!cycle_advance_granted(1)) return;
            cycle_complete(cycle.phase);
            cycle_emit(5);
            return;
        }
        switch (cycle.phase) {
        case 0:
            if (span && icount >= 2) {
                cycle_advance_quiet(2);
                cycle.phase = 2;
                cycle_sample_dma_inputs();
                break;
            }
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 1;
            cycle_emit(1);
            break;
        case 1:
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 2;
            cycle_sample_dma_inputs();
            cycle_emit(2);
            break;
        case 2:
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 3;
            break;
        case 3:
            // UM0050 p44: internal I/O duplicates the external strobes but
            // ignores external WAIT and DCNTL.IWI. Data synchronization is
            // independent and selected from the shared five-PHI service phase.
            if (cycle_wait_pending()) {
                cycle.phase = 7;
                break;
            }
            sampled_bus_request = bus_released;
            cycle_sample_irq_edge();
            cycle.phase = cycle.ttp ? 10 : 4;
            break;
        case 10:
            if (!cycle_advance(2)) return;
            cycle.phase = 4;
            break; // UM0050 p72: TTP is not a bus boundary.
        case 7:
            if (span && cycle.waits && icount >= 2) {
                unsigned const n = std::min<unsigned>(icount, 2U * cycle.waits);
                cycle_advance_quiet(n);
                cycle_sample_dma_inputs();
                cycle.waits -= n / 2;
                cycle.phase = (n & 1) ? 8 : 3;
                break;
            }
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 8;
            cycle_sample_dma_inputs();
            break;
        case 8:
            if (!cycle_advance_granted(1)) return;
            if (cycle.waits)
                --cycle.waits;
            cycle_emit(3);
            cycle.phase = 3;
            break;
        case 4:
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 5;
            cycle_sample_opcode();
            break;
        case 5:
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 6;
            cycle_sample_data();
            break;
        case 6:
            if (!cycle_advance_granted(1)) return;
            cycle.phase = 9;
            break;
        case 9:
            if (cycle.trailing)
                if (!cycle_advance(2 * cycle.trailing)) return;
            cycle_complete(cycle.phase);
            cycle_emit(5);
            return;
        }
    }
}

// Record the decoded family and opcode for the shared Ti timing table.
// Check whether the remaining nominal instruction clocks fit in the available time.
void Z180::cycle_decode(unsigned family_arg, unsigned opcode_arg) {
    family = family_arg;
    opcode = opcode_arg;
    // Once the final opcode is known, compare its maximum nominal duration
    // with the time remaining before the next event. I/O and pin changes
    // invalidate the fast path; an external wait may still require a pause.
    unsigned const bound = cc[family_arg][opcode_arg] + cc[Z180_TABLE_ex][opcode_arg];
    instruction_whole = instruction_fast && !cycle_observer && cc[family_arg][opcode_arg] &&
                        icount >= int(2 * (bound > nominal ? bound - nominal : 0));
}

// Return Ti clocks before a bus access using the explicit timing-table rules.
// Zilog UM0050 Tables 51-52 specify the machine-cycle sequence.
unsigned Z180::cycle_prelude_reference(cycle_kind kind) const {
    if (trapping) {
        if (kind == cycle_kind::write && bus_number == (third_trap ? 5U : 2U))
            return third_trap ? 4 : 5;
        return 0;
    }
    return z180_prelude_rule(family, opcode, bus_number, kind == cycle_kind::write, !(omcr & 0x80));
}

// Run each bus phase when a direct access cannot use the fast path.
uint8_t Z180::cycle_access_slow(cycle_kind kind, uint32_t address, uint8_t data, unsigned pre) {
    for (unsigned n = 0; n < pre; ++n)
        cycle_transaction(cycle_kind::internal, 0, 0, family == Z180_TABLE_op && opcode == 0x10);
    ++bus_number;
    return cycle_transaction(kind, address, data);
}

// Advance internal instruction clocks while preserving arbitration boundaries.
void Z180::cycle_whole_internal(unsigned count, bool no_arbitration) {
    assert(count && cursor == history_count && !cycle.active);
#ifdef GS2_SS_TESTING
    if (history_count + count > std::size(history))
        z180_error("Z180 instruction journal overflow at %04x", previous_pc);

#endif
    // These Ti boundaries have identical sampled inputs and no requester.
    // The exact divider recurrence covers the span. Test builds record each
    // Ti clock in case test execution later suspends.
    if (!fast_irq_valid)
        cycle_sample_stable_inputs();
    cycle_clock_span(count);
#ifdef GS2_SS_TESTING
    for (unsigned n = 0; n < count; ++n)
        cycle_record_access(cycle_kind::internal, 0, 0, 1);
#else
#endif
    cycle_completed_record(cycle_kind::internal, 0, 0, no_arbitration);
    nominal += count;
    boundary_allowed = !no_arbitration;
    dma_allowed = true;
    dma_start_pending = false;
    refresh_after_sleep = false;
}

// Use the shared phase engine for a mapped memory access.  address is logical
// for completed-access bookkeeping; physical is the MMU-translated bus address.
// Its board wait callback is required even if a direct physical page exists.
uint8_t Z180::cycle_mapped_access(cycle_kind kind, uint32_t address, uint32_t physical, uint8_t data) {
    cycle_begin(cycle_owner::cpu, kind, physical, data, false, true);
    cycle_run();
    cycle_finish_access(kind, address, cycle.data, cycle.nominal, cycle.no_arbitration);
    if (!instruction_fast)
        cycle_resume_admission();
    return cycle.data;
}

// Restore pending arbitration before resuming an instruction access.
void Z180::cycle_resume_admission(bool interrupt) {
    // The pending cycle and any test replay are finished. Check again whether
    // the rest of the instruction can run before the next event. Invalidate, but do
    // not take a new pin sample here: a late IRQ after T2 must wait for the
    // next real Ti/memory cycle, if this instruction has one at all.
    if (icount >= 2 && !cycle_observer && !trapping && (interrupt || instruction_interrupt < 0) &&
        !cycle.active &&
        cursor == history_count && boundary_allowed &&
        !bus_released && !sampled_bus_request && !bus_ack && !nmi_pending && !refresh_pending &&
        (!(dstat & 0xc0) || dma_idle()) && ((!(tcr & 3) && timer_limit == UINT64_MAX) || admit_timers())) {
        instruction_fast = true;
        fast_irq_valid = false;
    }
}

// Perform a port cycle of a fast instruction with the begin, edges and
// callbacks of cycle_transaction, without its test replay and arbitration layers.
// Fast execution has no DMA, refresh or BUSREQ requester at this boundary.
uint8_t Z180::cycle_whole_io(cycle_kind kind, uint32_t port, uint8_t data, unsigned pre) {
    if (pre)
        cycle_whole_internal(pre, family == Z180_TABLE_op && opcode == 0x10);
    ++bus_number;
    cycle_begin(cycle_owner::cpu, kind, port, data);
    cycle_run();
    cycle_finish_access(kind, port, cycle.data, cycle.nominal, cycle.no_arbitration);
    return cycle.data;
}

// Perform one bus operation; test execution can replay a completed access
// without repeating device effects. address is logical for CPU memory (ports
// remain 16-bit); data is the outgoing byte and the return is the sampled byte.
// no_arbitration suppresses a bus hand-off where instruction timing requires it.
uint8_t Z180::cycle_transaction(cycle_kind kind, uint32_t address, uint8_t data, bool no_arbitration) {
#ifdef GS2_SS_TESTING
    if (cursor < history_count) {
        auto const &h = history[cursor++];
        if (h.kind != kind || h.address != address)
            z180_error("Z180 continuation diverged at %04x transaction %u", previous_pc, cursor - 1);
        nominal += h.nominal;
        return h.data;
    }

#endif
    if (icount <= 0 && reset_cancelled) {
        // Let a cancelled handler finish its finite Ti accounting loop,
        // without clocking the processor or producing an external edge.
        nominal += kind == cycle_kind::internal;
        return 0;
    }
    if (!cycle.active || cycle.owner != cycle_owner::cpu || cycle.kind == cycle_kind::release) {
        // The instruction's stable-input path has no DMA/refresh requester.
        // I/O or external input changes invalidate that condition. Arbitrate
        // explicitly for observed edges or test replay.
        if (!instruction_fast || cycle_observer || sampled_bus_request || refresh_pending)
            cycle_arbitrate();
        if (icount <= 0) {
            cycle_suspend();
            if (reset_cancelled) { cycle.active = false; return 0; }
        }
        // Bus ownership or a scheduler callback may have ended at this boundary.
        // If the fast-path conditions hold again, run the complete access.
        if (kind <= cycle_kind::write && !trapping && !cycle_observer) {
            if (!instruction_fast) {
                cycle_resume_admission();
            }
            // Same routing as cycle_whole_access, which also needs the whole cycle.
            if (instruction_fast && !refresh_pending && icount >= int(6 + 2 * memory_wait_states())) {
                auto const page = kind != cycle_kind::write && direct_reads_enabled ? direct_logical[(address >> 12) & 15]
                                                                                    : nullptr;
                if (page)
                    return cycle_certified_read(kind, address, page + (address & 4095));
                return cycle_mapped_access(kind, address, MMU_REMAP_ADDR(address), data);
            }
        }
        uint32_t physical = address;
        if (kind == cycle_kind::fetch || kind == cycle_kind::operand || kind == cycle_kind::read ||
            kind == cycle_kind::write || kind == cycle_kind::nmi || kind == cycle_kind::halt)
            physical = MMU_REMAP_ADDR(address);
        cycle_begin(cycle_owner::cpu, kind, physical, data, no_arbitration);
        if (trapping && third_trap && kind == cycle_kind::read && bus_number == 5) {
            cycle.ttp = true;
            ++cycle.nominal;
        }
    }
    cycle_run();
    cycle_finish_access(kind, address, cycle.data, cycle.nominal, cycle.no_arbitration);
    uint8_t const result = cycle.data;
    if (!instruction_fast && kind <= cycle_kind::write)
        cycle_resume_admission();
    return result;
}

// Account for internal clocks left after explicit bus cycles, then retire
// the instruction at its interrupt/arbitration boundary.
void Z180::cycle_finish_cpu(int nominal_arg) {
    if (nominal > unsigned(nominal_arg))
        z180_error("Z180 cycle table overrun at %04x: %u > %u", previous_pc, nominal, nominal_arg);
    bool const admitted =
        instruction_whole || (!cycle_observer && !cycle.active && cursor == history_count && !trapping);
    if (admitted && instruction_fast && !(rcr & 0x80) && nominal < unsigned(nominal_arg) &&
        icount >= int(2 * (nominal_arg - nominal)))
        cycle_whole_internal(nominal_arg - nominal);
    while (nominal < unsigned(nominal_arg))
        cycle_transaction(cycle_kind::internal, 0, 0);
    if (cursor != history_count)
        z180_error("Z180 unused continuation at %04x", previous_pc);
}

// Notify the board once for RETI (return from interrupt); test replay stays
// silent. A peripheral interrupt chain may release its in-service state.
void Z180::cycle_reti() {
    if (icount <= 0 && reset_cancelled) return;
#ifdef GS2_SS_TESTING
    if (!cycle_executing || !reti_notified)
#endif
    {
        if (bus.reti)
            bus.reti();
#ifdef GS2_SS_TESTING
        reti_notified = true;
#endif
    }
}

// Check whether DMA is enabled and requested without consuming the request edge.
// Zilog UM0050 pp. 95-109 distinguishes edge and level request modes.
inline bool Z180::cycle_dma_ready(unsigned ch) const { return dma_request(ch, sampled_iol, dma_edge_sampled[ch]); }

inline bool Z180::dma_idle() const {
    if (!(dstat & 0xc0))
        return true;
    if (dma_channel != 0xff || dma_continue != 0xff)
        return false;
    // Fast spans neither arbitrate nor resample DREQ. They are safe while
    // no enabled channel requests on the last sample or on the present pins:
    // pins change only at a grant boundary, ASCI flags at serial deadlines or
    // through I/O cycles, and each of those ends the span.
    for (unsigned ch = 0; ch != 2; ++ch)
        if ((dstat & (0x40 << ch)) && (cycle_dma_ready(ch) || dma_request(ch, iol, dma_edge_generation[ch])))
            return false;
    return true;
}

inline bool Z180::dma_request(unsigned ch, uint32_t lines, uint64_t edge) const {
    if (HALT == 2 || !(dstat & 1) || !(dstat & (0x40 << ch)))
        return false;
    unsigned route = 0;
    bool receive = false;
    if (!ch) {
        unsigned sm = (dmode >> 2) & 3, dm = (dmode >> 4) & 3;
        if (sm >= 2 && dm >= 2)
            return false;
        if (sm < 2 && dm < 2)
            return true;
        if (sm == 3) {
            route = (dma_sar0.d >> 16) & 3;
            receive = true;
        } else if (dm == 3)
            route = (dma_dar0.d >> 16) & 3;
    } else {
        route = (dma_iar1.d >> 16) & 7;
        receive = (dcntl & 2) != 0;
    }
    if (route == 1 || route == 2)
        return (asci[route - 1].dma_request(receive));
    if (route)
        return false; // DS006002 p57: selectors 3-7 are reserved.
    return (dcntl & (4 << ch)) ? (edge != dma_edge_accepted[ch]) : bool(lines & (0x800 << ch));
}

// Latch a DMA byte transfer and its memory/I/O directions before taking
// bus ownership (UM0050 DMA modes, pp. 95-109).
void Z180::cycle_dma_begin(unsigned ch) {
    dma_channel = ch;
    dma_stage = 0;
    if (!ch) {
        dma_source_mode = (dmode >> 2) & 3;
        dma_destination_mode = (dmode >> 4) & 3;
        dma_source = dma_sar0.d;
        dma_destination = dma_dar0.d;
        dma_source_io = dma_source_mode == 3;
        dma_destination_io = dma_destination_mode == 3;
    } else {
        bool receive = (dcntl & 2) != 0;
        dma_source = receive ? dma_iar1.d : dma_mar1.d;
        dma_destination = receive ? dma_mar1.d : dma_iar1.d;
        dma_source_io = receive;
        dma_destination_io = !receive;
        dma_source_mode = receive ? 3 : (dcntl & 1);
        dma_destination_mode = receive ? (dcntl & 1) : 3;
    }
    unsigned const req = ch;
    if (dcntl & (4 << req))
        dma_edge_accepted[req] = dma_edge_sampled[req];
}

// Execute DMA read/write phases and update addresses/count after the byte.
// BUSREQ or refresh may suspend a burst without ending it (UM0050 pp. 105, 113).
// dma_stage=0 reads one byte; stage=1 writes that retained byte. Only the
// write completion decrements the byte count. Address arithmetic wraps at
// 20 bits; crossing a 64-KiB boundary costs the carry clock (UM0050 pp. 104-105).
void Z180::cycle_dma_run() {
    unsigned const ch = dma_channel;
    bool const writing = dma_stage != 0;
    bool const io = writing ? dma_destination_io : dma_source_io;
    uint32_t const address = writing ? dma_destination : dma_source;
    unsigned const mode = writing ? dma_destination_mode : dma_source_mode;
    if (!cycle.active) {
        cycle_begin(cycle_owner::dma,
                    io ? (writing ? cycle_kind::output : cycle_kind::input)
                       : (writing ? cycle_kind::write : cycle_kind::read),
                    io ? uint16_t(address) : address, dma_byte);
        if (mode < 2 && ((mode == 0 && (address & 0xffff) == 0xffff) || (mode == 1 && !(address & 0xffff)))) {
            cycle.trailing = 1;
            ++cycle.nominal;
        }
    }
    cycle_run();
    if (reset_cancelled) { dma_channel = dma_continue = 0xff; dma_allowed = false; return; }
    if (!writing)
        dma_byte = cycle.data;
    uint32_t const next = mode < 2 ? (address + (mode ? -1 : 1)) & 0xfffff : address;
    if (!ch) {
        if (writing)
            dma_dar0.d = next;
        else
            dma_sar0.d = next;
    } else if (!io)
        dma_mar1.d = next;
    boundary_allowed = true;
    if (!writing) {
        dma_stage = 1;
        return;
    }
    // A programmed zero count means 65536 bytes: the first decrement wraps
    // to 65535, and only the final decrement disables the channel (p. 96).
    --dma_bcr[ch].w;
    if (!dma_bcr[ch].w) {
        dstat &= ~(0x40 << ch);
    }
    // Memory-to-memory cycle steal returns one MC to the CPU. I/O DMA
    // continues while the sampled level/next edge requests another byte
    // (UM0050 pp105-108); DMODE.MM applies only to memory-to-memory.
    if (dma_nmi) {
        dstat &= ~1;
        dma_nmi = false;
        // Preserve the CPU's interrupt sampling point across DMA. A newly
        // latched NMI resumes and completes the interrupted CPU instruction
        // (Zilog AN0066 pp. 6-6, 6-8).
    }
    bool const memory = dma_source_mode < 2 && dma_destination_mode < 2;
    dma_continue = (!memory || (dmode & 2)) && cycle_dma_ready(ch) ? ch : 0xff;
    dma_channel = 0xff;
    dma_allowed = false;
}

// Choose external grant, refresh, DMA or CPU at a legal bus boundary.
// A suspended burst retains priority over CPU execution (UM0050 pp. 281-282).
// The loop returns only when the CPU may own the next cycle. Each competing
// owner either consumes clock budget, retains a partial cycle for suspension,
// or releases its request. It must not execute a CPU access inside a DMA burst.
void Z180::cycle_arbitrate() {
    if (!boundary_allowed)
        return;
    // Each continue below follows work by another bus owner.
    for (;;) {
        if (icount <= 0 && reset_cancelled) return;
        if (cycle.active) {
            if (cycle.owner == cycle_owner::dma) {
                cycle_dma_run();
                continue;
            }
            if (cycle.owner == cycle_owner::refresh) {
                cycle_run();
                ++refresh_address;
                // Refresh takes the bus without consuming the suspended DMA
                // grant or the CPU breakpoint (UM0050 pp86-90,281-282).
                continue;
            }
        }
        if (sampled_bus_request || bus_ack) {
            if (!bus_ack) {
                bus_ack = true;
                refresh_pending = false;
                if (busack_cb)
                    busack_cb(true);
            }
            if (!cycle.active && !cycle_observer && bus_released && icount >= 2) {
                // BUSREQ is constant until this scheduler grant ends. Compose
                // its complete one-PHI release cycles with the exact divider
                // recurrences; leave an odd final half-PHI to the phase engine.
                // No IRQ is accepted or refresh cycle issued while BUSACK is
                // asserted. The next loop still constructs the pending cycle
                // at a zero budget, preserving its continuation state.
                unsigned const ticks = unsigned(icount) & ~1U;
                cycle_sample_dma_inputs();
                sampled_bus_request = true;
                icount -= ticks;
                half_phi += ticks;
                unsigned const phi = (phi_half + ticks) / 2;
                phi_half = (phi_half + ticks) & 1;
                advance_dividers(phi);
                refresh_phase = (refresh_phase + ticks) % 160;
                cycle = {};
                cycle.kind = cycle_kind::release;
                cycle.nominal = 1;
                cycle.elapsed = 2;
                cycle.phase = 1;
            } else {
                if (!cycle.active)
                    cycle_begin(cycle_owner::cpu, cycle_kind::release, 0);
                cycle_run();
            }
            // UM0050 pp. 105, 113: BUSREQ suspends DMA.
            // Resume its grant after release. Clearing it here allows CPU
            // cycles inside a burst, including writes to the live DMA registers.
            if (!sampled_bus_request) {
                bus_ack = false;
                if (busack_cb)
                    busack_cb(false);
            }
            continue;
        }
        if (refresh_pending && (rcr & 0x80) && HALT != 2 && !refresh_after_sleep) {
            refresh_pending = false;
            cycle_begin(cycle_owner::refresh, cycle_kind::refresh, refresh_address);
            continue;
        }
        if (dma_channel != 0xff) {
            cycle_dma_run();
            continue;
        }
        if (dma_continue != 0xff) {
            unsigned const ch = dma_continue;
            dma_continue = 0xff;
            if (cycle_dma_ready(ch)) {
                cycle_dma_begin(ch);
                continue;
            }
        }
        if (dma_allowed) {
            if (cycle_dma_ready(0)) {
                cycle_dma_begin(0);
                continue;
            }
            if (cycle_dma_ready(1)) {
                cycle_dma_begin(1);
                continue;
            }
        }
        return;
    }
}

// Latch interrupt inputs at the documented final-cycle sampling edge,
// separately from DMA stopping on NMI (UM0050 pp. 73, 75, 81).
void Z180::cycle_sample_interrupts() {
    // UM0050 pp73,75,81: falling edge before T3/T1 of the final MC.
    // Sampling each CPU MC and retaining the last sample implements that
    // rule without predicting whether a conditional instruction will stop.
    if (nmi_pending) {
        sampled_interrupt = 1;
        return;
    }
    // Priority encoder: lower-priority sources need not be evaluated once
    // a higher-priority asserted source fixes the result.
    unsigned const pins = (irq_state[0] ? 1 : 0) | (irq_state[1] ? 2 : 0) | (irq_state[2] ? 4 : 0);
    if (unsigned const enabled = pins & itc & 7) {
        sampled_interrupt = enabled & 1 ? 2 : enabled & 2 ? 3 : 4;
        return;
    }
    int irq = -1;
    bool const io = !(iocr & 0x20);
    if (io && (tcr & 0x50) == 0x50)
        irq = 5;
    else if (io && (tcr & 0xa0) == 0xa0)
        irq = 6;
    else if ((dstat & 0x44) == 4)
        irq = 7;
    else if ((dstat & 0x88) == 8)
        irq = 8;
    else if (io && csio.check_interrupt())
        irq = 9;
    else if (io && asci[0].check_interrupt())
        irq = 10;
    else if (io && asci[1].check_interrupt())
        irq = 11;
    sampled_interrupt = irq;
}

// Resolve enabled interrupt sources in the Z180 priority order.
// Return a priority index, or -1 if no interrupt may start. EI defers acceptance
// through the following instruction. SLP can wake from a masked interrupt
// without taking its handler (UM0050 pp. 34-36, 68-69).
int Z180::cycle_choose_interrupt() {
    if (after_EI || interrupt_recovery)
        return -1;
    int const irq = sampled_interrupt;
    if (irq >= 0 && HALT == 2) {
        refresh_after_sleep = true;
        LEAVE_HALT();
    }
    return irq == 1 || IFF1 ? irq : -1;
}

// Run one Ti of an interrupt sequence, using the fast path when it fits.
void Z180::cycle_interrupt_internal() {
    if (instruction_fast && !cycle_observer && !cycle.active && cursor == history_count && icount >= 2 &&
        !(rcr & 0x80))
        cycle_whole_internal(1);
    else
        cycle_transaction(cycle_kind::internal, 0, 0);
}

// Perform acknowledge/vector and stack cycles for the selected interrupt.
// Zilog UM0050 Tables 51-52 distinguishes IM0, IM1, IM2 and NMI sequences.
int Z180::cycle_interrupt() {
    int const irq = instruction_interrupt;
    LEAVE_HALT();
    if (irq == 1) {
        IFF2 = IFF1;
        IFF1 = 0;
#ifdef GS2_SS_TESTING
        if (!interrupt_started) {
            interrupt_started = true;
            dstat &= ~1;
        }
#else
        dstat &= ~1;
#endif
        cycle_transaction(cycle_kind::nmi, pc16, 0);
        cycle_resume_admission(true);
        cycle_interrupt_internal();
        cycle_interrupt_internal();
        PUSH(PC);
        pc16 = 0x66;
        return 11;
    }
    IFF1 = IFF2 = 0;
    uint8_t const vector = cycle_transaction(cycle_kind::acknowledge, pc16, 0);
    // The stack and vector cycles after the acknowledge are ordinary memory
    // cycles; IM0 executes its bus byte through the instruction path instead.
    if (irq != 2 || IM != 0)
        cycle_resume_admission(true);
    if (irq == 2 && IM == 0) {
        // UM0050 Table52 p279: subsequent operands are memory reads at PC,
        // PC+1; an inserted CALL stacks PC+2. Only the initial byte is ACK.
        ++R;
        ++bus_number;
        int const nominal_arg = exec_op(vector);
        return nominal_arg + extra_cycles + 2;
    }
    if (irq == 2 && IM == 1) {
        PUSH(PC);
        pc16 = 0x38;
        return 11;
    }
    cycle_interrupt_internal();
    unsigned const low = irq == 2 ? vector : ((il & 0xe0) + (irq - 3) * 2) & 0xff;
    PUSH(PC);
    RM16((I << 8) | low, &PC);
    return 18;
}

// Run instructions, low-power states and DMA within the supplied half-PHI
// budget; meet the board before crossing any external deadline.
// The worker keeps the instruction handler on its stack across bus callbacks.
// Test execution records accesses for replay; production handlers never restart.
// Each iteration retires an instruction, advances an idle/bus cycle, or leaves
// when the half-PHI budget is exhausted. No external deadline may be crossed.
void Z180::execute_machine_cycles() {
    // Debugger/non-mutating execution keeps the address-space handlers.
    // This context belongs to the scheduler grant, not a hardware edge.
    direct_reads_enabled = !side_effects_disabled;
    // Peripheral timers and inputs may have run since the previous slice,
    // including while HALT/SLP had no active instruction to restart.
    fast_irq_valid = false;
    instruction_fast = false;
    timer_limit = UINT64_MAX;
    while (icount > 0) {
        // A board time slice can end here without suspending an instruction handler.
        // An unfinished instruction still obeys the hard half-PHI budget.
        if (!instruction_active && half_phi >= run_soft_target)
            break;
#ifdef GS2_SS_TESTING
        try {
#endif
#ifdef GS2_SS_TESTING
            bool const resuming = instruction_active;
#endif
            if (!instruction_active) {
                // run_low_power() acts only in SLP; test that before the call.
                if (HALT == 2 && run_low_power())
                    continue;
                // Finish an idle HALT/SLP cycle before testing interrupts.
                if (cycle.active && cycle.owner == cycle_owner::cpu && cycle.kind != cycle_kind::release) {
                    cycle_run();
                    boundary_allowed = true;
                    dma_allowed = HALT != 2;
                }
                if (!instruction_fast || cycle_observer || sampled_bus_request || refresh_pending)
                    cycle_arbitrate();
                if (icount <= 0) {
                    return;
                }
                instruction_interrupt = cycle_choose_interrupt();
                if (instruction_interrupt == 1)
                    nmi_pending = 0;
                if (instruction_interrupt < 0 && HALT) {
                    if (HALT == 2) {
                        refresh_after_sleep = true;
                        if (!cycle.active)
                            cycle_begin(cycle_owner::cpu, cycle_kind::internal, 0);
                    } else if (!cycle.active)
                        cycle_begin(cycle_owner::cpu, cycle_kind::halt, MMU_REMAP_ADDR(uint16_t(pc16 + 1)));
                    cycle_run();
                    boundary_allowed = true;
                    dma_allowed = HALT != 2;
                    continue;
                }
                after_EI = 0;
#ifdef GS2_SS_TESTING
                cycle_save_state();
                history_count = 0;
#endif
                previous_pc = pc16;
                instruction_active = true;
                ack_vector = -1;
#ifdef GS2_SS_TESTING
                reti_notified = trap_started = interrupt_started = false;
#endif
                if (instruction_interrupt < 0 && (trace != nullptr)) {
                    if (trace)
                        trace(get_state());
                    // Debugger state entries can change divider/pin registers
                    // without passing through the CPU's external-input API.
                    instruction_fast = false;
                }
                // The CPU time budget ends at the next external pin or timer event.
                // cycle_run checks it before each machine cycle and services
                // the board before the handler can perform a later access.
                if (instruction_fast && !cycle_observer) {
                    // No callback or timed source has changed these inputs
                    // since the previous fast instruction. Only the budget and
                    // instruction-boundary interrupt decision are new.
                    instruction_fast = instruction_interrupt < 0 && icount >= int(6 + 2 * memory_wait_states());
                } else {
                    fast_irq_valid = false;
                    instruction_fast = instruction_interrupt < 0 &&
                                       icount >= int(6 + 2 * memory_wait_states()) &&
                                       !bus_released &&
                                       !nmi_pending && (!(dstat & 0xc0) || dma_idle()) &&
                                       ((!(tcr & 3) && timer_limit == UINT64_MAX) || admit_timers());
                }
            }
#ifdef GS2_SS_TESTING
            else {
                instruction_fast = false;
            }
#endif
            instruction_whole = false;
#ifdef GS2_SS_TESTING
            if (resuming || (trace != nullptr))
                cycle_restore_state();
            else
#endif
            {
#ifdef GS2_SS_TESTING
                previous_pc = instruction_start.regs[0];
                cursor = 0;
#endif
                bus_number = nominal = 0;
                family = Z180_TABLE_op;
                opcode = 0;
                extra_cycles = 0;
                trapping = third_trap = false;
            }
            cycle_executing = true;
            int nominal_arg;
            if (instruction_interrupt >= 0)
                nominal_arg = cycle_interrupt();
            else {
                uint8_t opcode_arg;
                if (instruction_fast && !cycle_observer) {
                    ++R;
                    uint16_t const pc = pc16++;
                    opcode_arg = cycle_whole_access(cycle_kind::fetch, pc, 0, 0);
                } else
                    opcode_arg = ROP();
                nominal_arg = exec_op(opcode_arg);
                nominal_arg += extra_cycles;
            }
            if (icount <= 0 && reset_cancelled) {
                cycle_executing = instruction_active = false;
                break;
            }
            // When both counts already match, finish_cpu only rechecks them.
            // Keep its internal-cycle and error paths for every other case.
            if (nominal != unsigned(nominal_arg) || cursor != history_count)
                cycle_finish_cpu(nominal_arg);
            cycle_executing = false;
            instruction_active = false;
            ++retired;
            interrupt_recovery = instruction_interrupt >= 0;
            if (cycle_observer && !reset_cancelled)
                cycle_observer(half_phi, 0, 6, pc16, 0);

#ifdef GS2_SS_TESTING
        } catch (cycle_yield const &) {
            cycle_executing = false;
        }
#endif
    }
}

// The card supplies scheduler deadlines; local serial timers shorten each grant.
Z180::Z180(Bus callbacks, uint32_t extal, Silicon revision)
    : bus(std::move(callbacks)), extal_hz(extal), silicon(revision) {
    // Flag tables are process-wide and immutable after initialization. Several
    // card instances may be created on different threads; call_once publishes
    // the finished tables before any instance executes an instruction.
    static std::once_flag initialized;
    std::call_once(initialized, init_flags);
    auto now = [this] { return time(); };
    for (auto &a : asci) {
        a.ext = silicon == Silicon::N;
        a.bind_time(now);
    }
    csio.bind_time(now);
    if (bus.extra_wait)
        set_bus_wait([this](uint32_t a, bool write, bool io) {
            return bus.extra_wait(io ? (write ? Access::IOWrite : Access::IORead)
                                     : (write ? Access::MemoryWrite : Access::MemoryRead),
                                  a);
        });
    reset();
}

// Return the active PHI frequency in Hz, after CMR/CCR clock selection
// (UM0050 pp. 49-52). EXTAL is the external oscillator input frequency.
uint32_t Z180::clock_hz() const {
    return clock_multiplier_enabled() ? extal_hz * 2 : (ccr & 0x80) ? extal_hz : extal_hz / 2;
}
// Convert EXTAL clocks to the active execution clock using CMR/CCR divisors.
uint64_t Z180::clocks_to_cycles(uint64_t clocks) const {
    return clock_multiplier_enabled() ? clocks * 4 : (ccr & 0x80) ? clocks * 2 : clocks;
}
// Invalidate derived time after restoring counters or changing clock rate.

// Derive elapsed time from the current oscillator epoch without accumulating
// rounding error per instruction.

// Convert execution ticks to an elapsed emulated duration. Ticks are half-PHI
// in the cycle engine and whole PHI in the optional instruction engine.
Z180Time Z180::cycles_to_time(uint64_t n) const { return Z180Time::from_ticks(n, clocks_to_cycles(extal_hz)); }
// Return complete execution ticks contained in duration t; round down so
// the caller cannot grant execution beyond its deadline.
uint64_t Z180::time_to_cycles(Z180Time t) const { return t.floor_ticks(clocks_to_cycles(extal_hz)); }
// Save time under the old divider before a register write changes clock rate.
void Z180::prepare_deferred_clock_change() {
    epoch_time = time();
    epoch_half = half_phi;
}
// Rebase the CPU time epoch and serial clocks, then tell the board to
// recompute waits that depend on PHI.
void Z180::notify_clock_changed() {
    board_edge_at = UINT64_MAX;
    epoch_rate = clock_hz() * 2;
    attoseconds_per_cycle = Z180Time::units_per_second / clocks_to_cycles(extal_hz);
    for (auto &a : asci)
        a.set_clock(clock_hz());
    csio.set_clock(clock_hz());
    if (bus.clock_changed)
        bus.clock_changed(clock_hz());
    if (executing)
        request_yield();
}
// Reset CPU execution and peripheral clocks together; board-retained latches
// and SRAM are owned by SSCard rather than this processor reset.
void Z180::reset() {
    Z180Time const now = epoch_rate ? time() : Z180Time::zero;
    reset_core();
    power_mode = 0;
    wake_remaining = 0;
    epoch_time = now;
    epoch_half = 0;
    epoch_rate = extal_hz;
    cmr = ccr = 0;
    for (auto &a : asci)
        a.reset();
    csio.reset();
    notify_clock_changed();
}
// Use one absolute half-PHI time as both the soft and hard deadline.
// Production must have enough grant to reach an instruction boundary.
void Z180::run_half_until(uint64_t target) {
    run_half_until(target, target);
}

// Stop starting instructions at soft_end without granting past hard_limit.
// Production keeps an active handler on the stack through board meetings;
// only the test engine can yield and replay a partial instruction.
void Z180::run_half_until(uint64_t soft_end, uint64_t hard_limit) {
    assert(!executing);
    assert(soft_end <= hard_limit);
    executing = true;
    run_target = hard_limit;
    run_soft_target = soft_end;
    yield_requested = false;
    while (half_phi < run_soft_target && !yield_requested) {
        if (scheduler) {
            if (!refill_budget())
                break;
            execute_run();
            continue;
        }
        auto next = std::min({asci[0].next_event(), asci[1].next_event(), csio.next_event()});
        uint64_t span = std::min<uint64_t>(hard_limit - half_phi, 0x3fffffff);
        if (!next.is_never()) {
            auto now = time();
            if (next <= now) {
                for (auto &a : asci)
                    a.dispatch(now);
                csio.dispatch(now);
                continue;
            }
            auto delta = next - now;
            uint64_t ticks = delta.ceil_ticks(epoch_rate);
            span = std::min(span, ticks);
        }
        icount = int(span);
        budget_end = half_phi + span;
        uint64_t before = half_phi;
        execute_run();
        assert(half_phi > before || yield_requested);
    }
    // Direct test/debug execution has only its icount grant, not a board limit.
    run_soft_target = UINT64_MAX;
    executing = false;
#ifdef GS2_SS_TESTING
    sync_timers();
#endif
}

// Meet the board scheduler at the current deadline and accept its next
// grant without recursively entering a suspended instruction handler.
bool Z180::refill_budget(bool in_handler) {
    fast_irq_valid = false;
    instruction_fast = false;
    yield_requested = false;
    scheduler_in_handler = in_handler;
    uint64_t limit = std::min(run_target, scheduler());
    scheduler_in_handler = false;
    if (limit <= half_phi) {
        yield_requested = true;
        icount = 0;
        return false;
    }
    // A future deadline includes the callback's input changes. A zero-length
    // interval above tells the scheduler to exit.
    yield_requested = false;
    auto next = std::min({asci[0].next_event(), asci[1].next_event(), csio.next_event()});
    if (!next.is_never()) {
        auto now = time();
        if (next <= now) {
            for (auto &a : asci)
                a.dispatch(now);
            csio.dispatch(now);
            next = std::min({asci[0].next_event(), asci[1].next_event(), csio.next_event()});
        }
        if (!next.is_never())
            limit = std::min(limit, half_phi + (next - now).ceil_ticks(epoch_rate));
    }
    scheduler_limit = half_phi + std::min<uint64_t>(limit - half_phi, 0x3fffffff);
    budget_end = scheduler_limit;
    timer_limit = UINT64_MAX;
    icount = int(std::min(scheduler_limit, board_edge_at) - half_phi);
    return icount > 0;
}

// Arrange a board edge inside a granted interval so a batch need not
// return at every idle half-PHI.
bool Z180::schedule_board_edge(uint64_t half_deadline) {
    if (!scheduler || !executing || cycle_observer || power_mode ||
        !board_edge || half_deadline <= half_phi)
        return false;
    board_edge_at = half_deadline;
    if (half_deadline - half_phi < uint64_t(std::max(icount, 0)))
        icount = int(half_deadline - half_phi);
    return true;
}
#ifdef GS2_SS_TESTING
// Test execution can step through part of an instruction. The production
// card owns execution and requests stops at instruction boundaries.
void Z180::step() {
    uint64_t const goal = retired + 1, stop = half_phi + 2'000'000;
    do {
        run_half_until(half_phi + 1);
        if (!instruction_active && HALT)
            break;
        if (bus_ack)
            break;
    } while (retired < goal && half_phi < stop);
}
#endif
// Translate the public pin name to the internal input index. asserted is the
// logical active state, not voltage; a new NMI assertion is latched once.
void Z180::set_input(Input input, bool asserted) {
    constexpr int lines[] = {Z180_INPUT_LINE_IRQ0, Z180_INPUT_LINE_IRQ1,  Z180_INPUT_LINE_IRQ2,
                             INPUT_LINE_NMI,       Z180_INPUT_LINE_DREQ0, Z180_INPUT_LINE_DREQ1};
    execute_set_input(lines[unsigned(input)], asserted);
}
// Export CPU registers separately from the test replay records.
Z180::State Z180::get_state() const {
    return {PC.w.l,
            SP.w.l,
            AF.w.l,
            BC.w.l,
            DE.w.l,
            HL.w.l,
            IX.w.l,
            IY.w.l,
            AF2.w.l,
            BC2.w.l,
            DE2.w.l,
            HL2.w.l,
            uint8_t((R & 0x7f) | (R2 & 0x80)),
            I,
            IM,
            IFF1,
            IFF2,
            HALT,
            cycles()};
}
// Import debugger registers and invalidate execution assumptions tied to the
// previous PC, interrupt state or address mapping.
void Z180::set_state(const State &s) {
    PC.w.l = s.pc;
    SP.w.l = s.sp;
    AF.w.l = s.af;
    BC.w.l = s.bc;
    DE.w.l = s.de;
    HL.w.l = s.hl;
    IX.w.l = s.ix;
    IY.w.l = s.iy;
    AF2.w.l = s.af2;
    BC2.w.l = s.bc2;
    DE2.w.l = s.de2;
    HL2.w.l = s.hl2;
    R = s.r & 0x7f;
    R2 = s.r & 0x80;
    I = s.i;
    IM = s.im;
    IFF1 = s.iff1;
    IFF2 = s.iff2;
    HALT = s.halt;
#ifdef GS2_SS_TESTING
    if (instruction_active) {
        instruction_start.control[2] = IFF1;
        instruction_start.control[3] = IFF2;
        instruction_start.control[4] = HALT;
    }

#endif
    instruction_fast = false;
}

// Combine established idle cycles only. The available time is already bounded
// by every external and serial event. Timers/refresh that can alter a sampled
// input stay on the phase engine. No bus callback or observer is bypassed.
// Advance HALT clocks only to the next interrupt/peripheral/bus event.

// K: 1992 Zilog data book pp374-377, STANDBY is 2^19 cycles; CCR3
// is reserved. N: UM0050 pp37-41,49 adds IDLE and quick recovery.
// While PHI is stopped, half_phi is elapsed scheduler time in half-PHI units;
// it must not clock FRC, refresh, PRT, serial or DMA.
// Apply silicon-specific SLEEP/standby clock stopping and wake-up timing;
// K silicon must not inherit N-only idle/quick-recovery behaviour.
bool Z180::run_low_power() {
    if (HALT != 2 || !(iocr & 0x20) || !(ccr & 0x48))
        return false;
    if (!power_mode) {
        power_mode = silicon == Silicon::K ? 2 : ((ccr >> 5) & 2) | ((ccr >> 3) & 1);
        wake_remaining = 0;
    }
    int source = -1;
    if (nmi_pending)
        source = 1;
    else if (irq_state[0] && (itc & 1))
        source = 2;
    else if (irq_state[1] && (itc & 2))
        source = 3;
    else if (irq_state[2] && (itc & 4))
        source = 4;
    bool const bus_wake = (ccr & 0x20) && bus_released;
    // K BUSREQ-only wake temporarily runs the released bus. On release it
    // re-enters STANDBY without executing the instruction following SLP (p375).
    if (power_mode == 4) {
        if (source >= 0) {
            power_mode = 0;
            sampled_interrupt = source;
            LEAVE_HALT();
            return false;
        }
        if (bus_released)
            return false;
        // Finish the already-started release cycle and drop BUSACK first.
        if (bus_ack) {
            cycle_arbitrate();
            if (bus_ack)
                return true;
        }
        power_mode = 2;
    }
    if (source < 0 && !bus_wake) {
        wake_remaining = 0; // Level-triggered recovery withdrawn: restart standby.
        timer_anchor += icount;
        if (timer_deadline != UINT64_MAX)
            timer_deadline += icount;
        half_phi += icount;
        icount = 0;
        return true;
    }
    if (!wake_remaining) {
        unsigned const clocks = power_mode == 1 ? 8 : power_mode == 2 ? 131072 : 64;
        wake_remaining = silicon == Silicon::K ? 2 * 524288 : 2 * clocks + 3;
    }
    unsigned const ticks = std::min<unsigned>(icount, wake_remaining);
    timer_anchor += ticks;
    if (timer_deadline != UINT64_MAX)
        timer_deadline += ticks;
    half_phi += ticks;
    icount -= ticks;
    wake_remaining -= ticks;
    if (wake_remaining)
        return true;
    power_mode = silicon == Silicon::K && source < 0 && bus_wake ? 4 : 0;
    sampled_interrupt = source;
    sampled_bus_request = bus_released;
    refresh_pending = false;
    refresh_after_sleep = true;
    boundary_allowed = true;
    dma_allowed = false;
    if (power_mode != 4)
        LEAVE_HALT();
    return false;
}

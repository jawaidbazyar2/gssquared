// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic

/*
 *   The two asynchronous serial ports inside Second Sight's Zilog Z8S180.
 *   ASCI means asynchronous serial communication interface: a byte is sent
 *   as a start bit, data bits, optional parity and stop bits without a separate
 *   clock wire. A shift register holds the bits as they move onto or off a pin.
 *   The CPU's internal-port decoder calls the register methods. Its scheduler
 *   calls dispatch at emulated deadlines; pin callbacks report changed outputs.
 *
 *   Second Sight has no external ASCI connection. Channel 0 RTS (request to
 *   send) instead selects the card ROM overlay through SSCard's callback.
 *   TXA/RXA are transmitted/received data, CKA is an optional external clock,
 *   CTS is clear-to-send and DCD is data-carrier-detect. Pin methods take
 *   voltage levels 0/1; CTS, DCD and RTS are active low. PHI is the CPU clock.
 *   A baud clock measures serial bits; the receiver can sample at 16 or 64
 *   clocks per bit. A FIFO is a first-in, first-out queue of received bytes.
 *
 *   Each channel belongs to Z180 and runs only on its execution thread.
 *   Second Sight uses K silicon, with one receive byte; N silicon has four.
 *   Sources: Zilog UM005004-0918 pp. 116-146 (ASCI) and Zilog TN0053 p. 3
 *   (K/N receive buffering and error recovery). The receive-register write
 *   convention comes from MAME (rdr_w):
 *   https://github.com/mamedev/mame/blob/master/src/devices/cpu/z180/z180asci.cpp
 */

#include "Z180ASCI.hpp"
// Zilog UM0050 ASCI register map: CNTLA controls enables, RTS and framing.
/* 00 ASCI control register A ch 0 */
// CNTLA (pp. 125-129): MPE filters multiprocessor messages; RE/TE enable
// receive/transmit; RTS0 drives channel 0 request-to-send. MPBR reads the
// received multiprocessor bit; EFR writes zero to reset errors. MODE_DATA
// selects 8 versus 7 data bits, MODE_PARITY enables parity, MODE_STOPB selects
// two versus one stop bit. Channel 1 bit 4 (CKA1D) disables its external clock.
static constexpr uint8_t Z180_CNTLA_MPE = 0x80;
static constexpr uint8_t Z180_CNTLA_RE = 0x40;
static constexpr uint8_t Z180_CNTLA_TE = 0x20;
static constexpr uint8_t Z180_CNTLA_RTS0 = 0x10;
static constexpr uint8_t Z180_CNTLA_MPBR_EFR = 0x08;
static constexpr uint8_t Z180_CNTLA_MODE = 0x07;
static constexpr uint8_t Z180_CNTLA_MODE_DATA = 0x04;
static constexpr uint8_t Z180_CNTLA_MODE_PARITY = 0x02;
static constexpr uint8_t Z180_CNTLA_MODE_STOPB = 0x01;

/* 01 ASCI control register A ch 1 */
static constexpr uint8_t Z180_CNTLA1_CKA1D = 0x10;

/* 02/03 ASCI control register B ch 0 */
// CNTLB (pp. 131-134): MPBT is the transmitted multiprocessor bit; MP selects
// that ninth-bit protocol. CTS_PS reads CTS and writes the /10 or /30 prescale.
// PEO chooses odd parity; DR chooses /64 versus /16 sampling; SS selects
// a power-of-two baud divisor, with 7 selecting an external clock.
static constexpr uint8_t Z180_CNTLB_MPBT = 0x80;
static constexpr uint8_t Z180_CNTLB_MP = 0x40;
static constexpr uint8_t Z180_CNTLB_CTS_PS = 0x20;
static constexpr uint8_t Z180_CNTLB_PEO = 0x10;
static constexpr uint8_t Z180_CNTLB_DR = 0x08;
static constexpr uint8_t Z180_CNTLB_SS = 0x07;

/* 04 ASCI status register 0 (all bits read-only except RIE and TIE) */
// STAT (pp. 135-139): RDRF means receive data ready; OVRN is overrun (a byte
// arrived without buffer space); PE is parity error; FE is a missing stop bit.
// RIE/TIE enable receive/transmit interrupts. DCD0 reports carrier state;
// TDRE means transmit holding register empty. Channel 1 bit 2 enables CTS gating.
static constexpr uint8_t Z180_STAT_RDRF = 0x80;
static constexpr uint8_t Z180_STAT_OVRN = 0x40;
static constexpr uint8_t Z180_STAT_PE = 0x20;
static constexpr uint8_t Z180_STAT_FE = 0x10;
static constexpr uint8_t Z180_STAT_RIE = 0x08;
static constexpr uint8_t Z180_STAT_DCD0 = 0x04;
static constexpr uint8_t Z180_STAT_TDRE = 0x02;
static constexpr uint8_t Z180_STAT_TIE = 0x01;

/* 05 ASCI status register 1 (all bits read-only except RIE, CTS1E and TIE) */
static constexpr uint8_t Z180_STAT1_CTS1E = 0x04;

/* 12/13 (Z8S180/Z8L180) ASCI extension control register 0 (break detect is read-only) */
// ASEXT (pp. 122-124): DCD0/CTS0 disable their automatic gating; X1_BIT_CLK0
// selects one clock per bit; BRG0_MODE selects the programmable time constant.
// BRK_EN enables break control, BRK_DET records an all-low frame, BRK_SEND
// holds TXA low. Bit 7 selects receive interrupt behavior on enhanced silicon.
// Channel 1 lacks the DCD0/CTS0 fields, hence its 0x9f writable mask.
static constexpr uint8_t Z180_ASEXT_DCD0 = 0x40;
static constexpr uint8_t Z180_ASEXT_CTS0 = 0x20;
static constexpr uint8_t Z180_ASEXT_X1_BIT_CLK0 = 0x10;
static constexpr uint8_t Z180_ASEXT_BRG0_MODE = 0x08;
static constexpr uint8_t Z180_ASEXT_BRK_EN = 0x04;
static constexpr uint8_t Z180_ASEXT_BRK_DET = 0x02;
static constexpr uint8_t Z180_ASEXT_BRK_SEND = 0x01;

static constexpr uint8_t Z180_ASEXT0_MASK = 0xff;
static constexpr uint8_t Z180_ASEXT1_MASK = 0x9f;

// Recompute bit/sample periods from PHI, prescale and time constant;
// external-clock mode has no scheduled deadline (UM0050 pp. 141-146).
void Z180ASCI::update_clock() {
    uint32_t brg_divisor;
    // Divide ratio
    divisor = (asci_ext & Z180_ASEXT_X1_BIT_CLK0) ? 1 : ((asci_cntlb & Z180_CNTLB_DR) ? 64 : 16);

    if ((asci_cntlb & Z180_CNTLB_SS) == Z180_CNTLB_SS) {
        // External clock
        brg_divisor = 0;
    } else {
        if (asci_ext & Z180_ASEXT_BRG0_MODE) {
            // UM0050 p144: BRG = PHI / (2 * (TC + 2)).
            brg_divisor = 2 * (uint32_t(asci_tc.w) + 2);
        } else {
            // Regular bitrate generator mode
            brg_divisor = 1 << (asci_cntlb & Z180_CNTLB_SS);
            brg_divisor *= ((asci_cntlb & Z180_CNTLB_CTS_PS) ? 30 : 10); // Prescale
        }
    }

    if (brg_divisor) {

        bit_rate = Z180Time::from_ticks(uint64_t(brg_divisor) * divisor, clock_rate);
        sample_rate = Z180Time::from_ticks(brg_divisor, clock_rate);
    } else {

        bit_rate = Z180Time::never;
        sample_rate = Z180Time::never;
    }

    transmit_timer.adjust(Z180Time::never);
    rx_state = STATE_START;
    rx_count_to = 1;
    if (!sample_rate.is_never()) {
        receive_timer.adjust(sample_rate, 0, sample_rate);
    }
}

// Return control A, including the received multiprocessor bit (UM0050 pp. 125-129).
uint8_t Z180ASCI::cntla_r() {

    return asci_cntla;
}

// Return control B with live CTS replacing its write-only prescaler field (UM0050 pp. 131-134).
uint8_t Z180ASCI::cntlb_r() {
    uint8_t data = (asci_cntlb & 0xdf) | (cts << 5);

    return data;
}

// Return status and acknowledge channel 0 DCD on a normal read; a debugger peek has no effect (pp. 138-139).
uint8_t Z180ASCI::stat_r() {
    uint8_t const data = asci_stat;
    // UM0050 pp138-139: the first read after DCD goes low still returns
    // DCD=1, then clears the latch and permits auto-enabled reception.
    if (!id && !dcd && !side_effects_disabled) {
        asci_stat &= ~Z180_STAT_DCD0;
        rx_enabled = true;
    }
    return data;
}

// Return the transmit holding byte without consuming it (UM0050 p. 121).
uint8_t Z180ASCI::tdr_r() {

    return asci_tdr;
}

// Return the oldest receive byte and consume it unless peeking; an empty queue returns the last byte (pp. 118-120).
uint8_t Z180ASCI::rdr_r() {
    uint8_t const data = fifo_count ? data_fifo[fifo_rd] : asci_rdr;
    if (!side_effects_disabled && fifo_count) {
        asci_rdr = data;
        fifo_rd = (fifo_rd + 1) & 3;
        --fifo_count;
        update_fifo_status();
    }
    return data;
}

// Return the extension-control byte, including detected break status (UM0050 pp. 122-124).
uint8_t Z180ASCI::asext_r() {

    return asci_ext;
}

// Return the low eight bits of the serial baud-rate time constant (UM0050 pp. 143-146).
uint8_t Z180ASCI::astcl_r() {

    return asci_tc.b.l;
}

// Return the high eight bits of the serial baud-rate time constant (UM0050 pp. 143-146).
uint8_t Z180ASCI::astch_r() {

    return asci_tc.b.h;
}

// The start edge is already detected; count only the remaining frame samples.
// Count data, parity/multiprocessor and stop samples after the start edge (UM0050 p. 131, Table 17).
void Z180ASCI::update_total_bits() {
    rx_total_bits = (asci_cntla & Z180_CNTLA_MODE_DATA) ? 8 : 7;
    rx_total_bits += (asci_cntla & Z180_CNTLA_MODE_STOPB) ? 2 : 1;
    rx_total_bits += (asci_cntlb & Z180_CNTLB_MP) ? 1 : ((asci_cntla & Z180_CNTLA_MODE_PARITY) ? 1 : 0);
}

// Write enables, framing and RTS from data; zero in EFR acknowledges errors (UM0050 pp. 125-129; TN0053 p. 3).
void Z180ASCI::cntla_w(uint8_t data) {
    if (io_stopped)
        data &= ~0x60;

    asci_cntla = (data & ~Z180_CNTLA_MPBR_EFR) | (asci_cntla & Z180_CNTLA_MPBR_EFR);
    output_rts(z180_bit(data, 4));     // Z180_CNTLA_RTS0
    if (!(data & Z180_CNTLA_MPBR_EFR)) // UM0050 pp126,129; TN0053 p3: EFR clears on zero.
    {
        // Later N silicon holds reception after overrun until EFR acknowledges
        // the last good byte. DS006002 p44 has the wrong EFR polarity.
        if (asci_stat & Z180_STAT_OVRN)
            fifo_overrun = false;
        if (fifo_count)
            error_fifo[fifo_rd] = 0;
        asci_stat &= ~(Z180_STAT_OVRN | Z180_STAT_PE | Z180_STAT_FE);
        asci_ext &= ~(Z180_ASEXT_BRK_DET);
    }
    if (!(data & Z180_CNTLA_TE)) {
        tx_state = STATE_WAIT;
        transmit_timer.adjust(Z180Time::never);
        output_txa(1);
    } else if (!tdr_empty && !bit_rate.is_never() && !transmit_timer.enabled())
        transmit_timer.adjust(bit_rate, 0, bit_rate);
    update_total_bits();
}

// Write data as parity/multiprocessor and clock selection; recompute bit timing (UM0050 pp. 131-134).
void Z180ASCI::cntlb_w(uint8_t data) {

    asci_cntlb = data;
    update_clock();
    update_total_bits();
}

// Queue byte data for transmission; the holding register becomes empty only when moved to the shifter (p. 121).
void Z180ASCI::tdr_w(uint8_t data) {

    asci_tdr = data;
    tdr_empty = false;
    update_tdre();

    if ((asci_cntla & Z180_CNTLA_TE) && !bit_rate.is_never() && !transmit_timer.enabled())
        transmit_timer.adjust(bit_rate, 0, bit_rate);
}

// Queue a receive-register write only when no byte is ready.
// This write behavior comes from the MAME Z180 core; it is not a
// documented serial input path.
void Z180ASCI::rdr_w(uint8_t data) {

    if (!(asci_stat & Z180_STAT_RDRF))
        set_fifo_data(data, 0);
}

// Apply break mode and recalculate clocks after extension storage is updated.
// The data argument is retained by the interface; this helper uses asci_ext.
void Z180ASCI::asext_base_w(uint8_t data) {
    if (asci_ext & Z180_ASEXT_BRK_EN)
        tx_state = (asci_ext & Z180_ASEXT_BRK_SEND) ? STATE_BREAK : STATE_WAIT;
    update_clock();
}

// Replace the time constant low byte with data and recalculate baud periods (UM0050 pp. 143-146).
void Z180ASCI::astcl_w(uint8_t data) {

    asci_tc.b.l = data;
    update_clock();
}

// Replace the time constant high byte with data and recalculate baud periods (UM0050 pp. 143-146).
void Z180ASCI::astch_w(uint8_t data) {

    asci_tc.b.h = data;
    update_clock();
}

// Set transmit-ready status from holding-buffer emptiness and CTS gating (UM0050 pp. 138-139).
void Z180ASCI::update_tdre() {
    // UM0050 pp138-139: CTS masks TDRE, not the physical TDR state
    // or an ongoing transmission. CTS1 applies only when selected.
    bool const auto_cts = id ? (asci_stat & Z180_STAT1_CTS1E) : (!ext || !(asci_ext & Z180_ASEXT_CTS0));
    if (tdr_empty && !(auto_cts && cts))
        asci_stat |= Z180_STAT_TDRE;
    else
        asci_stat &= ~Z180_STAT_TDRE;
}

// Build the transmit shift register from start, data, parity/multiprocessor and stop bits (UM0050 pp. 130-133).
void Z180ASCI::prepare_tsr() {
    int bits = (asci_cntla & Z180_CNTLA_MODE_DATA) ? 8 : 7;

    tsr = (asci_cntla & Z180_CNTLA_MODE_STOPB) ? 3 : 1; // stop bit(s)

    if ((asci_cntlb & Z180_CNTLB_MP) || (asci_cntla & Z180_CNTLA_MODE_PARITY)) {
        tsr <<= 1;
        if (asci_cntlb & Z180_CNTLB_MP) {
            tsr |= (asci_cntlb & Z180_CNTLB_MPBT) ? 1 : 0;
        } else {
            uint8_t parity = 0;
            for (int i = 0; i < bits; i++)
                parity ^= z180_bit(asci_tdr, i);
            if (asci_cntlb & Z180_CNTLB_PEO)
                parity ^= 1; // odd parity
            tsr |= parity;
        }
    }
    tsr <<= bits;
    tsr |= asci_tdr & ((1U << bits) - 1); // UM0050 Table 17: 7 or 8 data bits
    tsr <<= 1;                            // start bit
    tdr_empty = true;                     // TDR -> TSR releases the buffer (UM0050 p121).
    update_tdre();
}

// Emit the next framed bit, or enter idle when no byte remains (UM0050 pp. 121, 140-141).
void Z180ASCI::transmit_edge() {
    if (io_stopped)
        return;
    if (!(asci_cntla & Z180_CNTLA_TE))
        return;
    switch (tx_state) {
    case STATE_WAIT:
        if (tdr_empty) {
            transmit_timer.adjust(Z180Time::never);
            return;
        }
        prepare_tsr();
        tx_state = STATE_DATA;
        [[fallthrough]];
    case STATE_DATA:
        output_txa(z180_bit(tsr, 0));
        tsr >>= 1;
        // Stop bits are ones at the top of the frame. Once the last one has
        // shifted out, zero means the whole frame has left the pin.
        if (!tsr)
            tx_state = STATE_WAIT;
        break;
    case STATE_BREAK:
        output_txa(0);
        break;
    }
}

// Validate the sampled frame, attach parity/framing errors, and apply multiprocessor filtering (pp. 118-120, 125).
void Z180ASCI::update_received() {
    uint8_t rx_error = 0;
    int data_bits = (asci_cntla & Z180_CNTLA_MODE_DATA) ? 8 : 7;
    if (rsr == 0) // Break detect
    {
        rx_error |= Z180_ASEXT_BRK_DET;
    }
    if (rxa == 0)
        rx_error |= Z180_STAT_FE;

    if (asci_cntlb & Z180_CNTLB_MP) {
        asci_cntla = (asci_cntla & ~Z180_CNTLA_MPBR_EFR) | (((rsr >> data_bits) & 1) ? Z180_CNTLA_MPBR_EFR : 0);
    } else if (asci_cntla & Z180_CNTLA_MODE_PARITY) {
        uint8_t parity = 0;
        for (int i = 0; i < data_bits; i++)
            parity ^= z180_bit(rsr, i);
        if (asci_cntlb & Z180_CNTLB_PEO)
            parity ^= 1; // odd parity
        if (((rsr >> data_bits) & 1) != parity)
            rx_error |= Z180_STAT_PE;
    }
    // Skip only if MPE mode active and MPB is 0
    if (!((asci_cntlb & Z180_CNTLB_MP) && (asci_cntla & Z180_CNTLA_MPE) && ((asci_cntla & Z180_CNTLA_MPBR_EFR) == 0))) {

        set_fifo_data(rsr & ((1U << data_bits) - 1), rx_error);
    }
}

// Detect a low start bit, then sample data near bit centers and complete a frame (UM0050 pp. 118-120, 141).
void Z180ASCI::receive_edge() {
    if (io_stopped)
        return;
    if (!rx_enabled)
        return;

    if (asci_cntla & Z180_CNTLA_RE) {
        switch (rx_state) {
        case STATE_START:
            if (rxa == 0) {
                rx_state = STATE_DATA;
                rx_bits = 0;
                rsr = 0;
                // Skip the rest of the start bit and reach the first data center.
                // At one sample per bit there is no finer half-bit phase.
                if (!(bit_rate.is_never()))
                    receive_timer.adjust((divisor == 1) ? bit_rate : (bit_rate * 3) / 2, 0, bit_rate);
                else
                    rx_count_to = (divisor == 1) ? divisor : (divisor * 3) / 2;
            }
            break;
        case STATE_DATA:
            rsr |= rxa << rx_bits;
            rx_bits++;
            rx_count_to = divisor;
            if (rx_bits == rx_total_bits) {
                update_received();
                rx_state = STATE_START;
                if (!sample_rate.is_never())
                    receive_timer.adjust(sample_rate, 0, sample_rate);
                else
                    rx_count_to = 1;
            }
            break;
        }
    }
}

// Expose the oldest byte's error bits and ready state, retaining silicon-specific error latches (TN0053 p. 3).
void Z180ASCI::update_fifo_status() {
    if (fifo_count) {
        asci_stat = (asci_stat & (ext ? ~0x70 : ~0)) | Z180_STAT_RDRF | (error_fifo[fifo_rd] & 0x70);
        asci_ext = (asci_ext & ~Z180_ASEXT_BRK_DET) | (error_fifo[fifo_rd] & Z180_ASEXT_BRK_DET);
    } else
        asci_stat &= ~Z180_STAT_RDRF;
    if ((asci_stat & Z180_STAT_RIE) && (asci_stat & 0xf0))
        irq = 1;
}

// Queue received byte data with its error bits; preserve K/N capacity and overrun rules (TN0053 p. 3).
void Z180ASCI::set_fifo_data(uint8_t data, uint8_t error) {
    if (fifo_overrun)
        return;
    if (fifo_count == (ext ? 4 : 1)) {
        error_fifo[(fifo_wr + 3) & 3] |= Z180_STAT_OVRN;
        fifo_overrun = ext; // TN0053 p3: K resumes after RDR is read; N requires EFR.
    } else {
        data_fifo[fifo_wr] = data;
        error_fifo[fifo_wr] = error;
        fifo_wr = (fifo_wr + 1) & 3;
        ++fifo_count;
    }
    update_fifo_status();
}

// Publish transmitted-data voltage txa_arg only when it changes.
void Z180ASCI::output_txa(int txa_arg) {
    if (txa != txa_arg) {
        txa = txa_arg;
        if (txa_handler)
            txa_handler(txa);
    }
}

// Publish request-to-send voltage rts_arg only when it changes; this updates Second Sight ROM selection.
void Z180ASCI::output_rts(int rts_arg) {
    if (rts != rts_arg) {
        rts = rts_arg;
        if (rts_handler)
            rts_handler(rts);
    }
}

// Apply register reset values and idle output levels, retaining callbacks (UM0050 pp. 118-130).
void Z180ASCI::reset() {
    io_stopped = false;
    asci_ext = 0;
    asci_tc.w = 0;

    fifo_wr = 0;
    fifo_rd = 0;
    fifo_count = 0;
    fifo_overrun = false;
    irq = 0;

    output_txa(1);
    output_rts(1);

    tsr = 0;
    tdr_empty = true;
    rxa = 1;
    clock_state = 0;
    tx_state = STATE_WAIT;
    rx_state = STATE_START;
    rx_bits = 0;
    rx_enabled = true;

    tx_counter = 0;
    rx_counter = 0;
    rx_count_to = 1;
    // Preserve the documented indeterminate/control bits while disabling
    // reception and transmission. The control writes also recompute clocks.
    if (id == 0) {
        cntla_w((asci_cntla & Z180_CNTLA_MPBR_EFR) | Z180_CNTLA_RTS0);
        cntlb_w((asci_cntlb & (Z180_CNTLB_MPBT | Z180_CNTLB_CTS_PS)) | 0x07);
        asci_stat = (asci_stat & Z180_STAT_DCD0) | Z180_STAT_TDRE;
    } else {
        cntla_w((asci_cntla & Z180_CNTLA_MPBR_EFR) | Z180_CNTLA1_CKA1D);
        cntlb_w((asci_cntlb & Z180_CNTLB_MPBT) | 0x07);
        asci_stat = Z180_STAT_TDRE;
    }
}
// Service one internal receive sample deadline.
void Z180ASCI::receive_clock() {
    receive_edge();
}
// Change only interrupt enables and channel 1 CTS selection from data; preserve hardware status (pp. 135-139).
void Z180ASCI::stat_w(uint8_t data) {
    if (id == 0) {

        asci_stat = (asci_stat & ~(Z180_STAT_RIE | Z180_STAT_TIE)) | (data & (Z180_STAT_RIE | Z180_STAT_TIE));
    } else {

        asci_stat = (asci_stat & ~(Z180_STAT_RIE | Z180_STAT1_CTS1E | Z180_STAT_TIE)) |
                    (data & (Z180_STAT_RIE | Z180_STAT1_CTS1E | Z180_STAT_TIE));
        update_tdre();
    }
}
// Store writable extension bits from data and preserve break-detect status (UM0050 pp. 122-124).
void Z180ASCI::asext_w(uint8_t data) {
    if (id == 0) {

        asci_ext = (asci_ext & Z180_ASEXT_BRK_DET) | (data & Z180_ASEXT0_MASK & ~Z180_ASEXT_BRK_DET);
        asext_base_w(data);
        update_tdre();
    } else {

        asci_ext = (asci_ext & Z180_ASEXT_BRK_DET) | (data & Z180_ASEXT1_MASK & ~Z180_ASEXT_BRK_DET);
        asext_base_w(data);
        update_tdre();
    }
}

// Apply IOCR internal-I/O stop. stopped clears enables and received state;
// resume retains the clock grid rather than receiving missed bytes (UM0050 pp. 120-129, 172).
void Z180ASCI::set_iostop(bool stopped) {
    if (io_stopped == stopped)
        return;
    io_stopped = stopped;
    if (stopped) {
        // UM0050 pp120-129: clear RE/TE, RDRF and errors; keep format,
        // interrupt enables and baud selection. This is not a device reset.
        cntla_w(asci_cntla & ~0x60);
        fifo_count = fifo_rd = fifo_wr = 0;
        fifo_overrun = false;
        asci_stat &= ~0xf0;
        tdr_empty = true;
        update_tdre();
        rx_state = STATE_START;
        rx_count_to = 1;
    } else if (receive_timer.enabled() && receive_timer.deadline <= now()) {
        // FRC/dividers keep running in IOSTOP (UM p172). Retain the existing
        // receive sample grid; no missed sample can receive a disabled byte.
        auto const period = receive_timer.period;
        if (!period.is_never() && period.attoseconds()) {
            auto const delta = now() - receive_timer.deadline;
            auto const ticks = delta.periods(period) + 1;
            receive_timer.deadline = receive_timer.deadline + period * ticks;
        }
    }
}

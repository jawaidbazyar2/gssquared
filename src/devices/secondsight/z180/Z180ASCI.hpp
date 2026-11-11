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

#pragma once
#include "Z180Types.hpp"
#include "Z180Time.hpp"
// One asynchronous channel with its own bit clocks, holding/shift registers
// and interrupt state (Zilog UM0050, ASCI chapter).
class Z180ASCI {
public:
    // Construct channel 0 or 1 owned by Z180; the receive pin starts at idle high.
    explicit Z180ASCI(unsigned channel) : id(channel) { rxa = 1; }
    // Apply register reset values and idle output levels, retaining callbacks (UM0050 pp. 118-130).
    void reset();
    // Apply IOCR internal-I/O stop. stopped clears enables and received state;
    // resume retains the clock grid rather than receiving missed bytes (UM0050 pp. 120-129, 172).
    void set_iostop(bool stopped);
    // Set PHI frequency hz and recompute deadlines after a CPU divider change.
    void set_clock(uint32_t hz) {
        clock_rate = hz;
        update_clock();
    }
    // Return the next serial deadline, or never while I/O clocks are stopped.
    Z180Time next_event() const {
        return io_stopped ? Z180Time::never : std::min(receive_timer.deadline, transmit_timer.deadline);
    }
    // Process due receive/transmit events at emulated instant at; the CPU calls
    // again if more events remain due. Callbacks run synchronously on this thread.
    void dispatch(Z180Time at) {
        if (io_stopped)
            return;
        if (receive_timer.deadline <= at) {
            receive_timer.expire();
            receive_clock();
        }
        if (transmit_timer.deadline <= at) {
            transmit_timer.expire();
            transmit_edge();
        }
    }
    // Attach the CPU time reader used by both timer records; it must outlive the channel.
    void bind_time(std::function<Z180Time()> time) {
        now = time;
        receive_timer.now = time;
        transmit_timer.now = time;
    }
    // Synchronous output-voltage callbacks; RTS changes the board ROM map.
    std::function<void(bool)> txa_handler, rts_handler, cka_handler;
    std::function<Z180Time()> now;
    bool side_effects_disabled = false;
    bool io_stopped = false;
    // Return control A, including the received multiprocessor bit (UM0050 pp. 125-129).
    uint8_t cntla_r();
    // Return control B with live CTS replacing its write-only prescaler field (UM0050 pp. 131-134).
    uint8_t cntlb_r();
    // Return status and acknowledge channel 0 DCD on a normal read; a debugger peek has no effect (pp. 138-139).
    uint8_t stat_r();
    // Return the transmit holding byte without consuming it (UM0050 p. 121).
    uint8_t tdr_r();
    // Return the oldest receive byte and consume it unless peeking; an empty queue returns the last byte (pp. 118-120).
    uint8_t rdr_r();
    // Return the extension-control byte, including detected break status (UM0050 pp. 122-124).
    uint8_t asext_r();
    // Return the low eight bits of the serial baud-rate time constant (UM0050 pp. 143-146).
    uint8_t astcl_r();
    // Return the high eight bits of the serial baud-rate time constant (UM0050 pp. 143-146).
    uint8_t astch_r();
    // Write enables, framing and RTS from data; zero in EFR acknowledges errors (UM0050 pp. 125-129; TN0053 p. 3).
    void cntla_w(uint8_t data);
    // Write data as parity/multiprocessor and clock selection; recompute bit timing (UM0050 pp. 131-134).
    void cntlb_w(uint8_t data);
    // Change only interrupt enables and channel 1 CTS selection from data; preserve hardware status (pp. 135-139).
    void stat_w(uint8_t data);
    // Queue byte data for transmission; the holding register becomes empty only when moved to the shifter (p. 121).
    void tdr_w(uint8_t data);
    // Queue a receive-register write only when no byte is ready.
    // This write behavior comes from the MAME Z180 core; it is not a
    // documented serial input path.
    void rdr_w(uint8_t data);
    // Store writable extension bits from data and preserve break-detect status (UM0050 pp. 122-124).
    void asext_w(uint8_t data);
    // Replace the time constant low byte with data and recalculate baud periods (UM0050 pp. 143-146).
    void astcl_w(uint8_t data);
    // Replace the time constant high byte with data and recalculate baud periods (UM0050 pp. 143-146).
    void astch_w(uint8_t data);


    // Return nonzero if an enabled transmit/receive condition requests a CPU interrupt (UM0050 p. 140).
    [[gnu::always_inline]] int check_interrupt() const {
        uint8_t const rx = (asci_stat & ((ext && (asci_ext & 0x80)) ? 0x70 : 0xf0)) | (id ? 0 : (asci_stat & 4));
        return ((asci_stat & 3) == 3) || ((asci_stat & 8) && rx);
    }
    // Return receive-ready when receive is true, otherwise transmit-ready; no byte is consumed (UM0050 p. 120).
    bool dma_request(bool receive) const { return (asci_stat & (receive ? 0x80 : 2)) != 0; }

private:
    friend class Z180;
    // Recompute bit/sample periods from PHI, prescale and time constant;
    // external-clock mode has no scheduled deadline (UM0050 pp. 141-146).
    void update_clock();
    // Publish transmitted-data voltage txa_arg only when it changes.
    void output_txa(int txa_arg);
    // Publish request-to-send voltage rts_arg only when it changes; this updates Second Sight ROM selection.
    void output_rts(int rts_arg);
    // Service one internal receive sample deadline.
    void receive_clock();
    // Apply break mode and recalculate clocks after extension storage is updated.
    // The data argument is retained by the interface; this helper uses asci_ext.
    void asext_base_w(uint8_t data);
    // Local bit-machine states: START seeks a low start bit; DATA shifts a
    // frame; WAIT waits for a transmit byte; BREAK holds output low. These
    // are execution-thread state, not register encodings.
    enum SerialState { STATE_START, STATE_DATA, STATE_WAIT, STATE_BREAK };
    uint32_t clock_rate = 18'000'000;
    unsigned id;
    // Enable the later N-revision (SL1919) serial features. The card uses
    // K-revision (SL1960) silicon, which lacks them (Zilog TN0053 pp. 1-3).
    bool ext = true;
    Z180Time bit_rate = Z180Time::never, sample_rate = Z180Time::never;
    Z180Timer receive_timer, transmit_timer;
    // Emit the next framed bit, or enter idle when no byte remains (UM0050 pp. 121, 140-141).
    void transmit_edge();
    // Detect a low start bit, then sample data near bit centers and complete a frame (UM0050 pp. 118-120, 141).
    void receive_edge();
    // Queue received byte data with its error bits; preserve K/N capacity and overrun rules (TN0053 p. 3).
    void set_fifo_data(uint8_t data, uint8_t error);
    // Expose the oldest byte's error bits and ready state, retaining silicon-specific error latches (TN0053 p. 3).
    void update_fifo_status();
    // Build the transmit shift register from start, data, parity/multiprocessor and stop bits (UM0050 pp. 130-133).
    void prepare_tsr();
    // Set transmit-ready status from holding-buffer emptiness and CTS gating (UM0050 pp. 138-139).
    void update_tdre();
    // Validate the sampled frame, attach parity/framing errors, and apply multiprocessor filtering (pp. 118-120, 125).
    void update_received();
    // Count data, parity/multiprocessor and stop samples after the start edge (UM0050 p. 131, Table 17).
    void update_total_bits();

    uint8_t asci_cntla{}; // ASCI control register A
    uint8_t asci_cntlb{}; // ASCI control register B
    uint8_t asci_stat{};  // ASCI status register
    uint8_t asci_tdr{};   // ASCI transmit data register
    uint8_t asci_rdr{};   // ASCI receive data register
    uint8_t asci_ext{};   // ASCI extension control register
    Z180Word asci_tc{};   // ASCI time constant

    bool tdr_empty = true;
    uint16_t tsr{};
    uint16_t rsr{};
    uint8_t data_fifo[4]{};
    uint8_t error_fifo[4]{};
    uint8_t fifo_wr{};
    uint8_t fifo_rd{};
    uint8_t fifo_count = 0;
    bool fifo_overrun = false;

    uint8_t cts{};
    uint8_t dcd{};
    uint8_t irq{};
    uint8_t txa{};
    uint8_t rxa{};
    uint8_t rts{};

    uint32_t divisor{};

    uint8_t clock_state{};
    uint8_t tx_state{};
    uint8_t rx_state{};
    uint8_t rx_bits{};

    uint8_t tx_counter{};
    uint8_t rx_counter{};
    uint8_t rx_count_to{};
    uint8_t rx_total_bits{};

    bool rx_enabled{};
};

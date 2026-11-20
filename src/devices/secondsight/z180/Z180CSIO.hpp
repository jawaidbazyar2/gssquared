// license:BSD-3-Clause
// copyright-holders:Vas Crabb, Sandro Ronco, Miodrag Milanovic

/*
 *   Clocked serial input/output inside Second Sight's Zilog Z8S180.
 *   CSI/O sends or receives one byte using a separate CKS clock wire. It is
 *   half duplex: software chooses transmit or receive, not both at once.
 *   TXS is transmitted data and RXS received data. CNTR is the control/status
 *   register; TRDR is the unbuffered transmit/receive data register.
 *
 *   The CPU internal-port decoder calls these register methods. The CPU
 *   scheduler calls dispatch for internal-clock edges; this board model drives
 *   no external clock or data input. Output callbacks report changed voltage
 *   levels. Second Sight leaves these serial outputs unconnected, but its CPU
 *   still has the registers.
 *   The Z180 owns this device and all methods run on its execution thread.
 *
 *   PHI is the processor clock. A deadline is an emulated instant, not host
 *   wall time. Source: Zilog UM005004-0918 pp. 147-156, especially Table 22
 *   and Figs. 59-62 for the clock divider and data edges.
 */

#pragma once
#include "Z180Types.hpp"
#include "Z180Time.hpp"
// Half-duplex clocked serial channel driven by internal or external CKS
// edges (Zilog UM0050 pp. 149-153).
class Z180CSIO {
public:
    // Reset control and shift position, cancel timing, and publish idle-high outputs (UM0050 pp. 148-150).
    void reset();
    // Stop internal I/O when stopped is true; clear enables/completion and retain data (UM0050 pp. 148-149).
    void set_iostop(bool stopped);
    // Store PHI frequency hz; the next scheduled edge uses it to choose a new period.
    void set_clock(uint32_t hz) { clock_rate = hz; }
    // Return the next CKS deadline, or never while internal I/O is stopped.
    Z180Time next_event() const { return io_stopped ? Z180Time::never : timer.deadline; }
    // At emulated instant at, deliver one due clock edge and reschedule if transfer remains active.
    void dispatch(Z180Time at) {
        if (!io_stopped && timer.deadline <= at) {
            timer.expire();
            internal_clock();
        }
    }
    // Attach the owning CPU time reader used to schedule CKS; it must outlive this device.
    void bind_time(std::function<Z180Time()> time) { timer.now = time; }
    // Synchronous voltage callbacks owned by the board; no host thread is started.
    std::function<void(bool)> cks_cb, txs_cb;
    bool side_effects_disabled = false;
    bool io_stopped = false;
    // Return control/status with reserved bit 3 zero; reading does not acknowledge completion (p. 149).
    // trdr_r returns the data byte and clears completion unless peeking (p. 151).
    uint8_t cntr_r(), trdr_r();
    // Write byte data as enables and clock selection while preserving the read-only end flag (pp. 149-152).
    // trdr_w loads data and clears completion; it does not start the transfer (p. 151).
    void cntr_w(uint8_t data), trdr_w(uint8_t data);
    // Return true when both completion and its interrupt enable are set (UM0050 p. 151).
    [[gnu::always_inline]] bool check_interrupt() const { return (cntr & 0xc0) == 0xc0; }

private:
    friend class Z180;
    // Handle CKS voltage cks: falling sends a bit, rising receives/counts it.
    // After eight bits set completion and clear both enables (UM0050 pp. 153-156).
    // internal_clock toggles CKS and schedules its next half-period (p. 150).
    void clock_edge(uint8_t cks), internal_clock();
    uint32_t clock_rate = 18'000'000;
    Z180Timer timer;
    uint8_t cntr = 0, trdr = 0, shift_cnt = 0, irq = 0;
    uint8_t cks_in = 1, rxs_in = 1, cks_out = 1, txs_out = 1;
};

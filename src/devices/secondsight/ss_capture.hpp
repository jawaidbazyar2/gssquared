/*
 *   Second Sight's U7 Lattice ispLSI1016 chip watches the Apple bus. This
 *   model decodes each access into a video-memory copy or a firmware event.
 *   Capture means retaining the access's address/data; an event means firmware
 *   must interpret a soft-switch change or host command. A soft switch is an
 *   Apple address whose read or write changes a hardware setting.
 *
 *   SSCard calls transfer() to decode an access, then phase() at Apple bus
 *   clock and acknowledgement edges. These load the U2 Apple data-in latch
 *   and retain a pending event across any wait. A latch holds a
 *   byte until another load. The U17 copy controller writes captured bytes
 *   to card RAM; the firmware reads event addresses through Z180 ports.
 *
 *   Mega II supplies the IIgs slow Apple II bus. /M2SEL is its active-low
 *   select signal; M2B0 is the CPU bank's low bit during the address phase.
 *   RDY (Ready) holds the Apple CPU until the previous event is acknowledged.
 *   Sources: Apple IIgs Technical Note 68, Mega II Select/Bank 0 sections;
 *   Second Sight manual and Jawaid Bazyar's ROM_BEEF.asm Reset and event
 *   handlers at https://github.com/jawaidbazyar2/secondsight . The selectable
 *   BE8C/BBB1 capture equations model card logic; the manuals describe the
 *   signals and firmware interface, not every equation or internal delay.
 *   Capture equations and analysis: https://github.com/Webifi/second-sight-reference,
 *   data/lattice/Lattice_original-BE8C.ldf, data/lattice/Lattice_v2.0.ldf
 *   and docs/04-capture-logic.md.
 */

#pragma once

#include <cstdint>
#include "mmus/slot_bus.hpp"

// One decode result, returned by value to SSCard on the card execution thread.
struct ss_capture_result_t {
    bool copy = false;  // Copy this write into the card's Apple video-memory image.
    bool bank = false;  // Destination: false = main 64 KiB, true = auxiliary 64 KiB.
    bool event = false; // Save the low address byte for firmware to interpret.
    bool drive = false; // The U3 Apple data-out latch supplies this Apple read.
    bool stall = false; // An older event must be acknowledged before this access.
};

// Owned by SSCard. Only the card execution thread mutates it; host inspection
// must wait for the worker. No Apple RAM pointer is retained here.
class SSCapture {
public:
    // BE8C selects the shipped equations; V2 selects the BBB1 revised equations.
    enum class LatticeMap { BE8C, V2 };
    LatticeMap lattice_map;
    // State of the Lattice capture equations, independent of CPU scheduling.
    // SSCard owns event acknowledgement and the subsequent SRAM copy.
    explicit SSCapture(LatticeMap map = LatticeMap::BE8C) : lattice_map(map) {}

    // Port $A1 D3 enables IIgs bank/select handling; D2 inhibits video copies.
    // bank_held retains M2B0 through the later data phase. HIRES selects Apple
    // high-resolution graphics; PAGE2 selects the second display page; RAMWRT
    // directs ordinary writes to auxiliary RAM; 80STORE changes how PAGE2
    // selects the text/graphics bank (Apple IIgs Hardware Reference, memory map).
    bool mode_d3 = false;
    bool copy_inhibit = false;
    bool bank_held = false;
    bool hires = false;
    bool page2 = false;
    bool ramwrt = false;
    bool store80 = false;
    bool pending = false;
    // Bits 0-7 track writes to $2000-$9FFF in 4 KiB bands, including super-high-resolution (SHR) control/palette bytes.
    uint8_t bands = 0;
    // The band of a copy whose PH0-high window is open. The chip sets a band
    // flag as a level while CAPTURE and PH0 are high, so a $A4/$A5 read in
    // that window returns 1 and leaves it set. See the chip equation listing
    // in data/lattice/Lattice_v2.0.ldf, lines 576-582 and 634-640.
    uint8_t window_bands = 0;
    uint8_t event_address = 0;
    uint8_t host_input = 0;

    // These are feedback latches, not a pending-event shortcut for RDY.
    // Both capture designs use the same feedback equations: see the chip
    // equation listings in data/lattice/Lattice_original-BE8C.ldf, lines
    // 424-445, and data/lattice/Lattice_v2.0.ldf, lines 479-500.
    // PH0 is the Apple bus clock; ACK is the Z180 host acknowledgement.
    // IRQ_PENDING holds a firmware event, RUN allows another capture, and
    // STALL holds Apple RDY low. ACK clears IRQ_PENDING, RUN recovers in
    // PH0 low, and STALL releases on the next PH0 high. The host accepts only
    // the subsequent eligible fall.
    bool run = true, stall = false, event_latch = false;
    bool data_in_active = false, catch_active = false;

    // Changes returned by value to SSCard on the card thread. copy starts
    // a U17 copy controller grant; event counts a newly latched request;
    // interrupt tells SSCard to update the Z180 INT1 and DREQ1 input levels.
    struct Edge {
        bool copy = false;
        bool event = false;
        bool interrupt = false;
    };

    // Apply a PH0 or ACK edge with the retained address/data and decoded
    // inputs. Parallel feedback evaluation preserves the recovered equations;
    // it is Boolean settling, not a host wait or a gate propagation model.
    Edge phase(const slot_bus_transfer_t &t, const ss_capture_result_t &decode, bool ph0, bool ack) {
        const bool old_event = event_latch, old_pending = pending;
        // Evaluate all feedback terms from the same previous state. Stop
        // only when another pass would change none of the retained levels.
        for (;;) {
            const bool next_run = (!pending && !ack && !ph0) || (!pending && run) ||
                                  (!decode.event && run && !decode.copy) || (run && !decode.copy && !ph0);
            const bool next_pending = (!ack && pending) || (!ack && !ph0 && event_latch);
            const bool next_stall = !run || (!ph0 && stall);
            const bool next_event = (event_latch && !ack && !ph0) || (event_latch && !pending && !ack) ||
                                    (!pending && run && ph0 && decode.event);
            if (next_run == run && next_pending == pending && next_stall == stall && next_event == event_latch)
                break;
            run = next_run;
            pending = next_pending;
            stall = next_stall;
            event_latch = next_event;
        }
        // Falling ADIN_ACTIVE loads the U2 Apple data-in latch. Falling
        // CATCH_ACTIVE requests a U17 copy controller transfer, even with RUN
        // low. A held copy reuses the latch byte only until ADIN_ACTIVE falls
        // again (both output pins invert). The original chip's equation listing,
        // data/lattice/Lattice_original-BE8C.ldf, blocks A7/A2, shows both.
        // The bank was retained in the address phase. M2B0 is undriven in
        // the data phase (Apple IIgs Technical Note 68, Fig. 1) and must read
        // low for a copy to occur. The drawing has no pull-down; what holds it
        // low on a production card is not known (second-sight-reference
        // docs/04-capture-logic.md, "Copying video writes").
        const bool data_in = ph0 && ((decode.copy && run) || (!t.read && event_latch));
        const bool copy = ph0 && decode.copy;
        if (data_in_active && !data_in) host_input = t.data;
        Edge edge{catch_active && !copy, !old_event && event_latch, old_pending != pending};
        if (edge.event) event_address = t.address;
        window_bands = copy ? band_bit(t.address) : 0;
        bands |= window_bands;
        data_in_active = data_in;
        catch_active = copy;
        return edge;
    }

    // Clear volatile latches at power-on while retaining the selected equations.
    void power_on() { *this = SSCapture(lattice_map); }
    // Latch capture inhibit and IIgs bank-selection mode from Z180 port $A1.
    void configure(uint8_t data) {
        copy_inhibit = data & 4;
        mode_d3 = data & 8;
        if (!mode_d3)
            bank_held = false;
    }

    // Decode t's address, read/write direction and bank pins. Return actions
    // for SSCard without consuming the pending event. This also updates the
    // chip's remembered soft switches; phase() or complete() sets the band
    // bits. BE8C preserves
    // its bank-selection behavior and does not capture border-color writes.
    ss_capture_result_t transfer(const slot_bus_transfer_t &t) {
        // Retain the address-phase bank signal while /M2SEL is low. The
        // transfer record supplies the value sampled before the data phase.
        // M2B0 later stops driving, so sampling it in the data phase would
        // lose auxiliary-bank writes (Apple IIgs Technical Note 68, Fig. 1).
        bank_held = mode_d3 && t.m2b0 && (bank_held || !t.m2sel_n);
        const uint16_t a = t.address;
        const bool valid = !mode_d3 || !t.m2sel_n;
        const bool c0 = (a & 0xff00) == 0xc000 && valid;
        ss_capture_result_t result;
        // Text pages occupy $0400-$0BFF; high-resolution pages occupy
        // $2000-$5FFF. IIgs super-high-resolution (SHR) pixels, scan-line
        // controls and palettes extend the auxiliary-bank range to $9FFF.
        // These are bus address tests, not reads of the emulator's RAM.
        result.copy = !t.read && !copy_inhibit && valid &&
                      ((a >= 0x0400 && a < 0x0c00) || (a >= 0x2000 && a < 0x6000) ||
                       (a >= 0x2000 && a < 0xa000 && mode_d3 &&
                        (lattice_map == LatticeMap::V2 ? (bank_held || ramwrt) : (bank_held != ramwrt))));
        // $C050-$C057 select graphics/text, mixed mode, page and resolution;
        // $C05E/$C05F select double resolution. $C00C-$C00F control 80-column
        // text/font selection, $C07E/$C07F the IOUDIS switch (I/O-unit disable), and
        // $C0B0-$C0B7 send card commands. IIgs writes $C022/$C029/$C02B
        // change text colors, video mode and monochrome control. V2 also
        // observes $C034 border color. Reads of write-only switches do not act.
        result.event =
            c0 && ((a >= 0xc050 && a <= 0xc053) || (a >= 0xc054 && a <= 0xc055 && !store80) ||
                   (a >= 0xc056 && a <= 0xc057) || (a >= 0xc05e && a <= 0xc05f) || (a >= 0xc0b0 && a <= 0xc0b7) ||
                   (!t.read &&
                    ((a >= 0xc00c && a <= 0xc00f) || a == 0xc07e || a == 0xc07f ||
                     (mode_d3 && (a == 0xc022 || a == 0xc029 || a == 0xc02b || (lattice_map == LatticeMap::V2 && a == 0xc034))))));
        result.drive = c0 && t.read && (a & 0xf0) == 0xb0;
        result.stall = pending && (result.copy || result.event);
        // Track switch values from the bus before choosing the destination
        // bank. $C068 is STATEREG, a byte containing several switch states.
        // It can change PAGE2 and RAMWRT together (Apple IIgs Tech Note 68).
        if (c0) {
            if (a == 0xc056 || a == 0xc057)
                hires = a & 1;
            if (a == 0xc054 || a == 0xc055)
                page2 = a & 1;
            if (!t.read && (a == 0xc004 || a == 0xc005))
                ramwrt = a & 1;
            if (!t.read && (a == 0xc000 || a == 0xc001))
                store80 = a & 1;
            if (a == 0xc068) {
                page2 = t.data & 0x40;
                ramwrt = t.data & 0x10;
            }
        }
        if (lattice_map == LatticeMap::V2) {
            // 80STORE selects PAGE2 only for text page 1
            // and (with HIRES on) hires page 1. All other copies use RAMWRT.
            const bool page1 = (a >= 0x0400 && a < 0x0800) || (hires && a >= 0x2000 && a < 0x4000);
            result.bank = bank_held || (store80 && page1 ? page2 : ramwrt);
        } else {
            result.bank = (bank_held && mode_d3 && !ramwrt) || (!bank_held && page2 && store80) ||
                          (!bank_held && ramwrt && !store80) || (!bank_held && mode_d3 && ramwrt && !hires);
        }
        return result;
    }

    // Whole-transfer helper for decode fixtures that do not clock phase().
    // Accept t after any RDY hold, using the decode result from transfer().
    // Load the U2 Apple data-in latch only for copied writes or event writes.
    // Event reads retain its byte. Set pending once; held bus phases must
    // not overwrite the byte that firmware has yet to acknowledge.
    void complete(const slot_bus_transfer_t &t, const ss_capture_result_t &result) {
        if (result.copy || (result.event && !t.read))
            host_input = t.data;
        if (result.copy)
            bands |= band_bit(t.address);
        if (result.event) {
            event_address = t.address;
            pending = true;
        }
    }

    // Whole-transfer fixture acknowledgement. The running card uses phase()
    // so acknowledgement also settles RUN, STALL and the retained event.
    void acknowledge() { pending = false; }
    // Read status at port $A4-$A7 (only its low three bits are used).
    // A nibble is four bits. $A4/$A5 return and clear low/high dirty-band
    // nibbles; $A6/$A7 return low/high event-address nibbles. The selector
    // drives only four pins, so keep the previous bus byte's upper nibble.
    // See ROM_BEEF.asm event dispatch reads of ports $A4-$A7.
    uint8_t status(unsigned port, uint8_t bus) {
        uint8_t nibble = 0;
        switch (port & 7) {
        case 4:
            nibble = bands & 15;
            bands &= 0xf0 | window_bands;
            break;
        case 5:
            nibble = bands >> 4;
            bands &= 0x0f | window_bands;
            break;
        case 6:
            nibble = event_address & 15;
            break;
        case 7:
            nibble = event_address >> 4;
            break;
        }
        return (bus & 0xf0) | nibble;
    }

private:
    // The 4 KiB dirty-band bit of a copied write from $2000 through $9FFF;
    // SHR palette and scan-line bytes are tracked with their containing band.
    static uint8_t band_bit(uint16_t address) {
        return address >= 0x2000 && address < 0xa000 ? uint8_t(1 << ((address - 0x2000) >> 12)) : 0;
    }
};

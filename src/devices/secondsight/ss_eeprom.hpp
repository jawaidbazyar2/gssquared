/*
 *   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright holders: Aaron Giles.
 *   See secondsight-video.LICENSE for the BSD-3-Clause terms.
 *
 *   Second Sight's 93C46 serial EEPROM keeps 64 sixteen-bit configuration
 *   words after power-off. EEPROM means electrically erasable programmable
 *   read-only memory. OTI087 drives its data-input, chip-select and clock pins;
 *   firmware reads the output bit through Oak. The application loads and saves
 *   the word array separately from the serial protocol.
 *
 *   OTI087 owns this chip. Pin operations run on the card execution thread;
 *   host persistence waits for that thread before copying contents. drive()
 *   accepts electrical levels and absolute emulated seconds; output() returns
 *   the current data/ready level. A rising clock shifts one command or data bit.
 *   Source: Oak OTI-087 databook pp. 23, 41 describes the EEPROM connection;
 *   Jawaid Bazyar's ROM_BEEF.asm at https://github.com/jawaidbazyar2/secondsight
 *   contains the configuration-memory accesses. Serial commands follow the
 *   93C46 sixteen-bit organization. Busy durations below are model constants.
 */

#pragma once
#include <array>
#include <cstdint>
#include <limits>

// Serial command state and nonvolatile words; board reset need not erase
// the words, and pin edges control command progress.
class SerialEEPROM93C46 {
public:
    // Initialize every word to erased $FFFF; no file I/O occurs in the chip.
    SerialEEPROM93C46() { words.fill(0xffff); }
    // Supply DI=data input, CS=chip select, clock level and absolute seconds.
    void drive(bool di, bool cs, bool clock, double now);
    // In start/busy state report ready when programming finishes; otherwise data bit.
    bool output(double now) const { return state == State::Start ? now >= busy_until : data_out; }
    // Return the busy-release time in seconds, or infinity if no work remains.
    double deadline(double now) const {
        return busy_until > now ? busy_until : std::numeric_limits<double>::infinity();
    }
    // Borrow persistent words after synchronization; does not clock the chip.
    const std::array<uint16_t, 64> &contents() const { return words; }
    // Restore the complete persistent image without changing serial protocol state.
    void set_contents(const std::array<uint16_t, 64> &data) { words = data; }

private:
    // Serial phases: Reset ignores clocks, Start waits for a one, Command
    // collects opcode/address, Read shifts out, Write collects data, Done
    // waits for chip select to end the command. All belong to this chip.
    enum class State { Reset, Start, Command, Read, Write, Done };
    State state = State::Reset;
    std::array<uint16_t, 64> words;
    uint32_t shift = 0;
    unsigned bits = 0, address = 0, opcode = 0;
    bool selected = false, last_clock = false, locked = true, data_out = true;
    double selected_at = 0, busy_until = 0;
    // Decode eight collected command bits at time now; enter data or busy state.
    void command(double now);
};

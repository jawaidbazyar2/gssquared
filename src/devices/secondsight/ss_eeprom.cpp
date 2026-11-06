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

#include "ss_eeprom.hpp"

// Nominal emulator busy intervals, not guaranteed timings for every 93C46 vendor.
static constexpr double ERASE_SECONDS = 0.001;
static constexpr double WRITE_SECONDS = 0.00175;
static constexpr double BULK_SECONDS = 0.008;

// Decode the two opcode bits and six address bits after the start bit.
// Write/erase commands obey EWEN/EWDS protection (93C46 instruction set).
void SerialEEPROM93C46::command(double now) {
    // After the start bit: 10=READ, 01=WRITE, 11=ERASE. Opcode 00 uses
    // address bits 5:4: 00=EWDS (disable writes), 01=WRAL (write all),
    // 10=ERAL (erase all), 11=EWEN (enable writes). Six address bits select
    // one of 64 words; writes/erases require EWEN after power-on.
    address = shift & 63;
    opcode = shift >> 6;
    bits = 0;
    shift = 0;
    if (opcode == 2) {
        state = State::Read;
        data_out = false; // dummy bit before the first data clock
    } else if (opcode == 1 || (opcode == 0 && (address >> 4) == 1)) {
        state = State::Write;
    } else if (opcode == 3 || (opcode == 0 && (address >> 4) == 2)) {
        state = locked ? State::Reset : State::Done;
        if (!locked) {
            if (opcode == 3)
                words[address] = 0xffff;
            else
                words.fill(0xffff);
            busy_until = now + (opcode == 3 ? ERASE_SECONDS : BULK_SECONDS);
        }
    } else {
        locked = (address >> 4) != 3;
        state = State::Reset;
    }
}

// Sample input on rising serial-clock edges while selected; READ emits its
// dummy zero before data, and write/erase holds the output busy until completion.
void SerialEEPROM93C46::drive(bool di, bool cs, bool clock, double now) {
    // A chip-select transition ends any partially shifted command. Starting
    // a command requires a later clock edge, not the selection edge itself.
    if (cs != selected) {
        selected = cs;
        state = cs ? State::Start : State::Reset;
        data_out = true;
        if (cs)
            selected_at = now;
    }
    bool rising = clock && !last_clock;
    last_clock = clock;
    if (!cs || !rising)
        return;
    switch (state) {
        case State::Start:
            if (di && now >= busy_until && now > selected_at) {
                bits = 0;
                shift = 0;
                state = State::Command;
            }
            break;
        case State::Command:
            shift = (shift << 1) | di;
            if (++bits == 8)
                command(now);
            break;
        // The command emitted a dummy zero. Subsequent clocks send the word
        // most significant bit first, then ones once all sixteen bits are out.
        case State::Read:
            if (bits == 0)
                shift = uint32_t(words[address]) << 16;
            else
                shift = (shift << 1) | 1;
            bits++;
            data_out = shift >> 31;
            break;
        case State::Write:
            shift = (shift << 1) | di;
            if (++bits == 16) {
                state = locked ? State::Reset : State::Done;
                if (!locked) {
                    if (opcode == 1)
                        words[address] = shift;
                    else
                        // Bulk programming clears selected bits in this model;
                        // a separate erase command restores ones.
                        for (auto &word : words)
                            word &= shift;
                    busy_until = now + (opcode == 1 ? WRITE_SECONDS : BULK_SECONDS);
                }
                data_out = true;
            }
            break;
        default:
            break;
    }
}

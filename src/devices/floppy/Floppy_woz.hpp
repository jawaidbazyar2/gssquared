/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstdio>

#include "util/woz.hpp"
#include "FloppyDrive.hpp"

class EventTimer;

// Abstract base for floppy drives backed by a WOZ in-memory bit stream.
//
// The WOZ bit-stream model, the angular head position, read/write-pulse
// plumbing, and the mount/unmount/writeback/status path are all identical
// for 5.25 and 3.5 drives. Subclasses differ in:
//
//   - how set_phase()/set_hdsel() affects the drive (5.25 stepper detent
//     vs. 3.5 CA0-2/SEL/LSTRB 16-way status/control selector);
//   - how CPU cycles convert to head advance (head_advance_per_cycle(),
//     4 cycles per bit cell on 5.25 vs. 2 cycles per bit cell on 3.5);
//   - which WOZ TMAP slot corresponds to the current head position
//     (current_tmap_index(), quarter-track vs. track*2+side).
class Floppy_woz : public FloppyDrive {
protected:
    Woz woz;
    // Bit stream for the currently-selected track; nullptr = empty track.
    // Non-const because write_pulse() mutates bits in place.
    woz_track_t *cur_track_ptr = nullptr;

    // Physical disk position in 1/8-bit-cell units.
    //   bit index into track = position >> 3  (mod track_bits)
    //   advance per CPU cycle = head_advance_per_cycle()
    uint64_t last_cycle = 0;
    uint64_t read_position = 0;  // where the LSS has consumed bits to
    uint64_t head_position = 0;  // true simulated angular head position

    bool     enable        = false;  // IWM drive selected (ENABLE'). On 5.25 this
                                     // doubles as motor-on; on 3.5 the spindle is
                                     // separate — see lss_disk_spinning().
    uint8_t  write_protect = 0;

    bool is_mounted = false;
    bool modified   = false;
    media_descriptor *media_d = nullptr;

    uint64_t random_bits = 0x5FCB9E767DC3523A;
    uint32_t windowBits  = 0;

    EventTimer *event_timer = nullptr;
    FILE       *dbglog      = nullptr;

    virtual uint64_t get_current_time() { return clock->get_cycles(); }

    // Angular-preserving track switch. Looks up the new track via
    // current_tmap_index() and rescales the angular head position so disk
    // rotation continues naturally when the new track has a different
    // bit_count (critical for 3.5 variable-length zones; a no-op when
    // lengths match, as they do on 5.25).
    void update_track_ptr();

    // RNG used when four zero bits in a row trip the "fake bit" shim,
    // mirroring real WOZ-unclean track behaviour.
    uint8_t get_random_bit();

    // Hook: how many 1/8-bit-cell units head_position advances per CPU cycle.
    //   5.25: 2  (4 cycles per bit cell @ 4 us)
    //   3.5 : 4  (2 cycles per bit cell @ 2 us)
    virtual uint32_t head_advance_per_cycle() const = 0;

    // Hook: which WOZ TMAP slot (0..159) identifies the track currently
    // under the head. 5.25 returns the quarter-track index; 3.5 returns
    // (track_num << 1) | side.
    virtual int current_tmap_index() const = 0;

    // True while the LSS should advance the bit stream (fast_forward /
    // read_pulse / write_pulse). 5.25: same as enable. 3.5: enable plus
    // spindle command and media present.
    virtual bool lss_disk_spinning() const { return enable; }

    void note_spinning_inputs_changed(bool was_spinning) {
        if (!was_spinning && lss_disk_spinning()) {
            last_cycle = get_current_time();
        }
    }

public:
    Floppy_woz(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer)
        : FloppyDrive(sound_effect, clock), event_timer(event_timer) {}
    virtual ~Floppy_woz() = default;

    // ── FloppyDrive contract: shared across 5.25 and 3.5 ─────────────────
    bool mount(uint64_t key, media_descriptor *media) override;
    bool unmount(uint64_t key) override;
    bool writeback() override;
    drive_status_t status() override;
    void reset() override;

    uint8_t read_pulse() override;
    void    write_pulse(uint8_t bit) override;

    bool get_enable() override { return enable; }
    void set_enable(bool on) override {
        const bool was_spinning = lss_disk_spinning();
        enable = on;
        note_spinning_inputs_changed(was_spinning);
    }

    bool is_bitstream_spinning() const { return lss_disk_spinning(); }

    // Advance the head based on elapsed cycles since the previous call;
    // returns how many whole bit cells the LSS should clock through.
    virtual uint64_t fast_forward(/* uint64_t now */);

    inline uint8_t get_write_protect() { return write_protect; }

    // 3.5 SEL line (DISKREG bit 7). Base is a no-op so 5.25 can ignore it.
    virtual void set_hdsel(bool on) { (void)on; }

    // Legacy nybble API: unused in the WOZ/LSS path but required by the
    // FloppyDrive interface.
    void    write_nybble(uint8_t nybble) override { (void)nybble; }
    uint8_t read_nybble() override { return 0; }

    virtual uint8_t read_sense() = 0;

    // Legacy command-reg shims: intentionally asserts — Floppy_woz drives
    // are always driven through the LSS path in IWM/diskii_controller.
    uint8_t read_cmd(uint16_t address) override;
    void    write_cmd(uint16_t address, uint8_t data) override;

    void set_dbglog(FILE *f) { dbglog = f; }

    // Accessors for debugging
    virtual bool get_motor_on() { return enable; }

    virtual int get_side() = 0;

    virtual void debug(DebugFormatter *f) = 0;

    void tick_no_write() {
        read_position += 8;
        if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
            read_position %= cur_track_ptr->bit_count * 8;
        }
    }
};

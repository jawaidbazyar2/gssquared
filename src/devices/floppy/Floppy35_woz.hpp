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

#include "Floppy_woz.hpp"
#include "util/DebugFormatter.hpp"

// Apple 3.5 (Sony-mechanism / double-density) drive.
//
// Unlike the 5.25 Shugart, CA0/CA1/CA2 and the external SEL line
// (DISKREG bit 7) form a four-bit selector that addresses a 16-way
// status table (read via IWM status register bit 7) and a 16-way
// control table (triggered by an LSTRB on/off strobe).  The IIGS ROM's
// SEL35 routine rearranges those bits into its own parameter encoding,
// but this implementation uses the raw CA2|CA1|CA0|SEL ordering
// directly from the Neil Parker status/control tables (see
// src/devices/iwm/NeilA235Floppy.md lines 386..475).
//
// Head motion is broken into two steps:
//   1) Set step direction via CONT35 $0 (inward) or $8 (outward).
//   2) Strobe step via CONT35 $2; each strobe advances track_num by 1
//      in the configured direction.
// Side is latched by reading STAT35 $8 (side=0) or $9 (side=1).
// Spindle motor is gated by CONT35 $4 (on) / $C (off); ENABLE just
// locks the disk and lights the LED.
class Floppy35_woz : public Floppy_woz {
    // Control-line state pushed from IWM (after decode of CA0/CA1/CA2/LSTRB
    // switches and the DISKREG SEL bit).
    uint8_t ca0   = 0;
    uint8_t ca1   = 0;
    uint8_t ca2   = 0;
    uint8_t hdsel = 0;  // SEL line from DISKREG bit 7
    uint8_t lstrb = 0;

    // Drive mechanics.
    int     track_num      = 0;     // 0..79
    int     side           = 0;     // 0 = lower, 1 = upper
    uint8_t step_dir       = 0;     // 0 = inward (toward higher tracks), 1 = outward
    bool    motor_on       = false; // spindle motor (CONT35 $4/$C)
    bool    disk_in_place  = false; // true after a successful mount()
    bool    disk_switched  = false; // set by eject control, cleared by reset-flag control
    bool    double_sided   = true;  // 3.5 WOZs in this emu are all 2-sided

    // Bit 7 presented to the IWM status register while this drive is
    // selected. Recomputed any time the CA*/SEL state changes.
    uint8_t sense_out = 0;

    // On track change or motor_on, set this to a positive number.
    // when it counts down to 0 (based on number of reads of this sense)
    // then set disk
    uint64_t ready_cycles_end = 0;
    bool     disk_ready = false;

    uint64_t stepping_cycles_end = 0;
    bool     disk_stepping = false;

    uint64_t instanceID = 0;

    virtual uint64_t get_current_time() override { return clock->get_vid_cycles(); }

    // Build the 4-bit CA2|CA1|CA0|SEL index used for both the status
    // read table and the LSTRB-strobed control table.
    inline uint8_t select_index() const {
        //fprintf(dbglog, "[%llu] 3.5 select_index: %d%d%d%d = %X\n", clock->get_vid_cycles(), ca2, ca1, ca0, hdsel, static_cast<uint8_t>((ca2 << 3) | (ca1 << 2) | (ca0 << 1) | hdsel));
        return static_cast<uint8_t>((ca2 << 3) | (ca1 << 2) | (ca0 << 1) | hdsel);
    }

    // Re-evaluate sense_out based on the current CA*/SEL selector.
    // Reading $8/$9 is also spec'd to configure the drive's active
    // head; we latch side here when that selector is sampled.
    void refresh_sense();

    // Execute the control function selected by the current CA*/SEL
    // index; invoked on the rising edge of LSTRB.
    void trigger_control();

    // Update the current track buffer pointer and angular position
    // (delegates to base) after any change to (track_num, side).
    void track_side_changed();

    // Recompute whether the shared Floppy_woz::enable latch (which gates
    // fast_forward/read_pulse/write_pulse) should be on. 3.5 spins only
    // when the drive is selected AND the spindle motor is commanded on.
    void update_spinning();
    FILE *dbglog = nullptr;

protected:
    uint64_t head_advance_per_cycle() const override { 
        //return 4; 
        // 16 here is standard bit timing for 3.5" drives.
        return (4 * POSITION_ADV_PER_CYCLE * 16) / woz.image().info.optimal_bit_timing;
    }  // 2us bit cell
    int      current_tmap_index()     const override { return (track_num << 1) | side; }

public:
    Floppy35_woz(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer, uint16_t drive_index)
        : Floppy_woz(sound_effect, clock, event_timer) {
            instanceID = 0xABAC0000 + drive_index;
            //dbglog = fopen("3.5_woz.dbg", "w");
        }
        ~Floppy35_woz() {
            if (dbglog) fclose(dbglog);
        }

    // ── FloppyDrive contract ─────────────────────────────────────────────

    void set_phase(uint8_t phase, uint8_t onoff) override;

    int get_track() override { return (track_num << 1) | side; }

    // 3.5 ENABLE just locks the disk + lights the LED. It does NOT
    // start the spindle — the motor is controlled by CONT35 $4/$C.
    // The base-class `enable` latch (which gates LSS spinning) is
    // computed from (drive_selected && motor_on) via update_spinning().
    void set_enable(bool on) override {
        if (!on && enable) {
            schedule_motor_off();
            // TODO: if a head movement is in progress, the motor off must be delayed until after the movement is complete, so have a "pending motor off" flag.
        }
        if (on) {
            event_timer->cancelEvents(instanceID); // cancel any pending motor off event
        }
        enable = on;
        update_spinning();
        refresh_sense();
        //fprintf(dbglog, "[%llu] 3.5 set_enable: %d\n", clock->get_vid_cycles(), on);
    }

    void update_timers(uint64_t now) {
        if (stepping_cycles_end && (now > stepping_cycles_end)) {
            stepping_cycles_end = 0;
            disk_stepping = false;
        }
        if (ready_cycles_end && (now > ready_cycles_end)) {
            ready_cycles_end = 0;
            disk_ready = true;
        }
        //fprintf(dbglog, "[%llu] 3.5 timers: (%d,%d) stepping_cycles_end: %llu, ready_cycles_end: %llu\n", now, disk_stepping, disk_ready, stepping_cycles_end, ready_cycles_end);
    }

    uint64_t fast_forward(/* uint64_t now */) override {
        //update_timers(now); // if they're just cruising the disk, update timers
        refresh_sense();
        return Floppy_woz::fast_forward(/* now */);
    }

    // ── 3.5-specific API used by IWM ─────────────────────────────────────

    void set_hdsel(bool on) override {
        hdsel = on ? 1 : 0;
        //fprintf(dbglog, "[%llu] 3.5 set_hdsel: %d\n", clock->get_vid_cycles(), hdsel);
        refresh_sense();

    }

    // Bit 7 of the IWM status register while this drive is selected.
    uint8_t read_sense() override { 
        return sense_out; 
    }

    virtual bool get_motor_on() override { return motor_on; }
    int  get_side() override           { return side; }

    void schedule_motor_off();
    static void motor_off_callback(uint64_t cycles, void *userData);
    virtual Woz_Nibblizer* make_nibblizer(media_descriptor *media) override;

    // ── Mount policy: 3.5 is WOZ-only this phase ────────────────────────
    virtual bool mount(uint64_t key, media_descriptor *media) override;
    virtual bool unmount(uint64_t key) override;
    virtual drive_status_t status() override;

    static constexpr const char *statusNames[16] = {
        "stepDirection",
        "diskInPlace",
        "diskIsStepping",
        "diskLocked",
        "motorOn",
        "atTrack0",
        "diskSwitched",
        "tachometer",
        "lowerHeadData",
        "upperHeadData",
        "",
        "",
        "numberSides",
        "diskReady",
        "",
        "driveInstalled",
    };
    static constexpr const char *controlNames[16] = {
        "stepDirectionInward",
        "",
        "stepOneTrack",
        "",
        "spindleMotorOn",
        "",
        "",
        "",
        "stepDirectionOutward",
        "resetDiskSwitched",
        "",
        "",
        "spindleMotorOff",
        "",
        "ejectDisk",
        "",
    };

    void debug(DebugFormatter *f) override {
        if (disk_in_place) f->addLine("+++ Image: %s", woz.get_current_filename().c_str());
        f->addLine("CA[210]: %d%d%d SEL: %d LSTRB: %d  sel-idx: %X (%s) => %d",
                   ca2, ca1, ca0, hdsel, lstrb, select_index(), 
                   lstrb ? controlNames[select_index()] : statusNames[select_index()], sense_out);
        f->addLine("Optimal Bit Timing: %d", woz.image().info.optimal_bit_timing);
        f->addLine("Track: %d side %d  Track Bits: %llu",
                   track_num, side, (unsigned long long)(cur_track_ptr ? cur_track_ptr->bit_count : 0));
        Position pos_head = get_pos_head();
        Position pos_read = get_pos_read();
        f->addLine("Head Position: %llu.%llu", pos_head.pos, pos_head.fract);
        f->addLine("Read Position: %llu.%llu", pos_read.pos, pos_read.fract);
/*         f->addLine("Position: Head: %llu  Read: %llu", head_position >> 3, read_position >> 3);
        f->addLine("inPlace: %d  switched: %d step_dir: %s", disk_in_place, disk_switched, step_dir ? "out" : "in");
 */    }
};

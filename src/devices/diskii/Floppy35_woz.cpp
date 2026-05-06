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

#include <cstdint>
#include <cstdio>

#include "Floppy35_woz.hpp"

// ─── Mount policy (3.5 = WOZ-only in Phase 1) ──────────────────────────────

bool Floppy35_woz::mount(uint64_t key, media_descriptor *media_in) {
    // Phase 1: only native WOZ images for 3.5 drives. Block/raw
    // containers (.po, .2mg, ...) are a follow-up task.
    if (media_in->media_type != MEDIA_WOZ) {
        fprintf(stderr,
                "Floppy35_woz: refusing non-WOZ media '%s' (type=%d)"
                " — only .woz is supported for 3.5 drives in this build\n",
                media_in->filename.c_str(), (int)media_in->media_type);
        return false;
    }

    if (!Floppy_woz::mount(key, media_in)) return false;

    // Sanity-check the WOZ disk_type byte (1 = 5.25", 2 = 3.5").
    // Only warn rather than refuse: some community tools have been known
    // to mis-tag 3.5 images, and the bit stream itself is what matters.
    if (woz.image().info.disk_type != 2) {
        fprintf(stderr,
                "Floppy35_woz: warning — WOZ INFO disk_type=%d (expected 2 for 3.5)\n",
                (int)woz.image().info.disk_type);
    }

    disk_in_place = true;
    disk_switched = false;

    // Start at track 0 side 0; angular position irrelevant at mount time.
    // TODO: should not reset track or side here.
    track_num = 0;
    side      = 0;
    track_side_changed();

    refresh_sense();
    return true;
}

bool Floppy35_woz::unmount(uint64_t key) {
    disk_in_place = false;
    motor_on      = false;
    disk_switched = true;
    //track_num     = 0;
    //side          = 0;
    //step_dir      = 0;
    update_spinning();
    refresh_sense();
    return Floppy_woz::unmount(key);
}

// status is different because there is a separate motor_on status
drive_status_t Floppy35_woz::status() {
    if (is_mounted) {
        return {is_mounted, media_d->filestub, motor_on, get_track(), modified,
                media_d->write_protected};
    }
    return {is_mounted, "", motor_on, get_track(), modified, false};
}

// ─── Phase / HDSEL / LSTRB state plumbing ──────────────────────────────────

void Floppy35_woz::set_phase(uint8_t phase, bool onoff) {
    // IWM2 pushes all four switch lines through set_phase() keyed by
    // the IWM switch index (0 = CA0, 1 = CA1, 2 = CA2, 3 = LSTRB).
    // See IWM_Drive.hpp for the iwm_switch_t enum definition.
    const uint8_t bit = onoff ? 1 : 0;
    bool lstrb_rise = false;

    switch (phase) {
        case 0: ca0 = bit; break;
        case 1: ca1 = bit; break;
        case 2: ca2 = bit; break;
        case 3:
            lstrb_rise = (!lstrb && bit);
            lstrb = bit;
            break;
        default: return;  // ENABLE/SELECT/Q6/Q7 are not our lines
    }

    // Control table is latched on the LSTRB off->on edge. We run the
    // selected function once per rising strobe and ignore the falling
    // edge; the ROM always toggles LSTRB+1 / LSTRB back to off which
    // matches hardware.
    if (lstrb_rise) {
        trigger_control();
    }

    refresh_sense();
}

// ─── 16-way status decode (CA2|CA1|CA0|SEL) ────────────────────────────────

void Floppy35_woz::refresh_sense() {
    // Status semantics mirror NeilA235Floppy.md lines 386..434 but use
    // the natural CA2|CA1|CA0|SEL ordering instead of the scrambled
    // SEL35 "Param" encoding. Bit 0 = 1 generally means the negative
    // condition (drive empty / motor off / not at track 0 / ...), as
    // documented at the foot of the status table.
    uint8_t out = 0;
    update_timers(clock->get_vid_cycles());
    switch (select_index()) {
        case 0b0000:  // step direction: 0 = inward, 1 = outward
            out = step_dir ? 1 : 0;
            break;
        case 0b0001:  // disk-in-place: 0 = present, 1 = empty
            out = disk_in_place ? 0 : 1;
            break;
        case 0b0010:  // disk-is-stepping: 0 = stepping, 1 = done. We
            // emulate head motion synchronously so we are never
            // "stepping": always return 1 (done).
            //out = 1;
            
            out = disk_stepping ? 0 : 1;
            break;
        case 0b0011:  // write-protect: 0 = WP, 1 = writable
            out = write_protect ? 0 : 1;
            break;
        case 0b0100:  // motor on: 0 = spinning, 1 = off
            out = motor_on ? 0 : 1;
            break;
        case 0b0101:  // at track 0: 0 = at T0, 1 = elsewhere
            out = (track_num == 0) ? 0 : 1;
            break;
        case 0b0110:  // disk-switched: 0 = user ejected, 1 = not ejected
        // TODO: the documentation may be wrong here. This is "normal sense" and it seems to be working..
            out = disk_switched ? 1 : 0; // reversed sense.
            break;
        case 0b0111:  // tachometer pulses (60/rev). Stubbed to 0 this phase.
            out = 0;
            break;
        case 0b1000:  // instantaneous lower-head data: reading also
                   // latches side = 0 per the Neil Parker note that
                   // sampling this selector configures the active head.
            if (side != 0) {
                side = 0;
                track_side_changed();
            }
            out = 0;  // raw track bit unavailable at status-read time
            break;
        case 0b1001:  // instantaneous upper-head data: latches side = 1
            if (side != 1) {
                side = 1;
                track_side_changed();
            }
            out = 0;
            break;
        case 0b1100:  // number of sides: 0 = SS, 1 = DS
            out = double_sided ? 1 : 0;
            break;
        case 0b1101:  // disk-ready-for-reading. Stubbed to 0 = ready so
                   // firmware's READYT wait loop returns immediately.
           
            out = disk_ready ? 0 : 1;
            break;
        case 0b1111:  // drive installed: 0 = connected
            out = 0;
            break;
        default:
            // Reserved / undocumented selector — ground it.
            out = 0;
            break;
    }
    fprintf(dbglog, "[%llu] 3.5 sense_out: %X (%s) = %d\n", clock->get_vid_cycles(), select_index(), statusNames[select_index()], out);
    sense_out = out;
}

// ─── 16-way control-strobe decode (CA2|CA1|CA0|SEL) ────────────────────────

void Floppy35_woz::trigger_control() {
    // Control semantics mirror NeilA235Floppy.md lines 460..475 but use
    // the natural CA2|CA1|CA0|SEL ordering.
    switch (select_index()) {
        // CA2 CA1 CA0 SEL
        case 0b0000:  // set step direction inward (toward higher tracks)
            step_dir = 0;
            break;
        case 0b1000:  // set step direction outward (toward lower tracks)
            step_dir = 1;
            break;
        case 0x1001:  // reset disk-switched flag (firmware clears the
                      // "user ejected while mounted" latch here)
            disk_switched = false;
            break;
        case 0b0010: {  // step one track in the current direction
            int new_track = track_num + (step_dir ? -1 : +1);
            if (new_track < 0)  new_track = 0;
            if (new_track > 79) new_track = 79;
            if (new_track != track_num) {
                if (new_track/0x10 != track_num/0x10) {
                    ready_cycles_end = clock->get_vid_cycles() + 5000; disk_ready = false;
                } else {
                    ready_cycles_end = clock->get_vid_cycles() + 1000; disk_ready = false;
                }
                stepping_cycles_end = clock->get_vid_cycles() + 300; disk_stepping = true;
                track_num = new_track;
                track_side_changed();
            }
            /* ready_counter = 3; disk_ready = false; */
            
            break;
        }
        case 0b0100:  // spindle motor on
            motor_on = true;
            update_spinning();
            ready_cycles_end = clock->get_vid_cycles() + 5000; disk_ready = false;
            break;
        case 0b1100:  // spindle motor off
            motor_on = false;
            update_spinning();
            ready_cycles_end = 0;
            disk_ready = false;
            break;
        case 0b1110:  // eject disk: Phase 1 flags only, no auto-unmount
            disk_in_place = false;
            disk_switched = true;
            break;
        default:
            // Unknown / reserved control selector — ignore.
            break;
    }
    fprintf(dbglog, "[%llu] 3.5 control_out: %X (%s)\n", clock->get_vid_cycles(), select_index(), controlNames[select_index()]);
    refresh_sense();
}

// ─── Track/side/spin helpers ───────────────────────────────────────────────

void Floppy35_woz::track_side_changed() {
    // Delegates to the base class, which handles the angular-preserving
    // re-wrap needed for 3.5's variable-length zones.
    update_track_ptr();
}

void Floppy35_woz::update_spinning() {
    
    /* const bool should_spin = enable && motor_on && disk_in_place;
    // Go through the base Floppy_woz::set_enable() so the cycle baseline
    // is reset on spin-up — otherwise the first fast_forward() would
    // replay a huge idle gap as random bits.
    Floppy_woz::set_enable(should_spin); */
}

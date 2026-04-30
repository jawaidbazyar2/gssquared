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

 /** Begin Refactoring this to be ONLY the Disk II drive itself interface, separating out all controller elements  */

#include <iostream>
#include <cstdint>
#include <cstdio>
#include "util/printf_helper.hpp"
#include "Floppy525_woz.hpp"
#include "devices/diskii/diskii.hpp"
#include "util/SoundEffectKeys.hpp"
#include "debug.hpp"

// ---- Start Storage Device Interface ────────────────────────────────────────────

// ─── Mount / unmount / writeback ─────────────────────────────────────────────

bool Floppy525_woz::mount(uint64_t key, media_descriptor *media_in) {
    if (is_mounted) {
        unmount(key);
    }

    int rc = (media_in->media_type == MEDIA_WOZ)
                 ? woz.load(media_in->filename)
                 : woz.import_from_media(media_in);

    if (rc != 0) {
        fprintf(stderr, "Floppy525_woz: failed to load/import '%s'\n",
                media_in->filename.c_str());
        return false;
    }

    //bit_fp = 0;
    read_position = 0;
    head_position = 0;
    last_cycle = clock->get_cycles();
    //lss_shift = 0;
    
    //data_register = 0; // this lives in controller.
    
    //track               = 0; // don't reset track. however, we need point to the current track.
    //cur_track_ptr = woz.get_track_ptr(track); // handled inside update_track_ptr()

    update_track_ptr();

    write_protect = media_in->write_protected;
    is_mounted    = true;
    media_d       = media_in;
    modified      = false;

    play_sound(SE_SHUGART_CLOSE);

    std::cout << "Floppy525_woz: mounted " << media_in->filestub << std::endl;
    return true;
}

bool Floppy525_woz::unmount(uint64_t key) {
    // Reset Woz image to a clean blank state
    woz = Woz{};
    cur_track_ptr = nullptr;
    //bit_fp        = 0;
    read_position = 0;
    head_position = 0;
    last_cycle    = 0;
    //lss_shift            = 0;
    //data_register  = 0;

    is_mounted = false;
    media_d    = nullptr;
    modified   = false;

    play_sound(SE_SHUGART_OPEN);
    return true;
}

bool Floppy525_woz::writeback() {
    if (!media_d) return false;

    // For WOZ files save in-place; for imported block images write back as WOZ, but change the file extension to .woz.
    std::string filename = media_d->filename;
    std::cout << "Floppy525_woz: writing back disk image" << media_d->filename << "  " << media_d->media_type << std::endl;
    if (media_d->media_type == MEDIA_NYBBLE) { // poorly named
        std::cout << "Floppy525_woz: writing back WOZ disk image" << std::endl;
        filename = filename.substr(0, filename.find_last_of('.')) + ".woz";
        // woz.export
    } else {
        std::cout << "Floppy525_woz: writing back block disk image" << std::endl;
        // TODO: write back the block disk image.
        int rc = woz.save(filename);
        if (rc != 0) {
            fprintf(stderr, "Floppy525_woz: writeback failed for '%s'\n",
                    media_d->filename.c_str());
            return false;
        }   
    }

    modified = false;
    return true;
}

// ─── Status / reset ───────────────────────────────────────────────────────────

drive_status_t Floppy525_woz::status() {
    if (is_mounted)
        return {is_mounted, media_d->filestub, enable, track, modified,
                media_d->write_protected};
    return {is_mounted, "", enable, track, modified, false};
}

// TODO: this should be moved to controller which will reset a bunch of stuff.
// also, if drive not selected, it should turn everything off.
void Floppy525_woz::reset() {
    enable = false;
}

// ---------------------- END Storage Device Interface ────────────────────────────

// ─── Head position helpers ────────────────────────────────────────────────────

/* void Floppy525_woz::set_track(int track_num) {
    track = track_num;
}

void Floppy525_woz::move_head(int direction) {
    track += direction;
} */

/* General methods for setting / reading floppy bus signals */
void Floppy525_woz::set_phase(uint8_t phase, bool onoff) {

    // first update phase vars..
    switch (phase) {
        case 0:
            phase0 = onoff;
            break;
        case 1:
            phase1 = onoff;
            break;
        case 2:
            phase2 = onoff;
            break;
        case 3:
            phase3 = onoff;
            break;
    }

    // schedule a phase change in 0.5ms (500 cycles-ish) out.
    // TODO: come up with a better, calculated instanceID
    // TODO: I'm a doof, this timer is 14m's.
    event_timer->scheduleEvent(clock->get_cycles() + 520, phase_change_callback, 0xABAB0001, this);
    //fprintf(dbglog, "schedule_phase_change: %lld + 520 = %lld\n", clock->get_cycles(), clock->get_cycles() + 520);
}

/**
| 2        | PHS0    |
| 4        | PHS1    |
| 6        | PHS2    |
| 8        | PHS3    |
| 10       | WR REQUEST'   |
| 14       | ENABLE'   |
| 16       | RDPULSE   |
| 18       | WRITE SIGNAL   |
| 20       | WRPROTECT'   |
 */

// ─── Bit-stream helpers ───────────────────────────────────────────────────────

void Floppy525_woz::update_track_ptr() {
    // `track` is a half-track index (same as Floppy525).  WOZ TMAP is indexed
    // by quarter-track, so multiply by 2: half-track N → quarter-track N*2.
    // (Full track T = half-track T*2 = quarter-track T*4.)
    cur_track_ptr = woz.get_track_ptr(track);

    // Re-wrap read_position for the new track's revolution length so start_bit
    // is always in-range.  The disk keeps spinning so the angular position
    // is preserved; only the modulus changes.
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        //bit_fp = bit_fp % (cur_track_ptr->bit_count * 8);
        head_position = head_position % (cur_track_ptr->bit_count * 8);
        read_position = head_position;
    }
}

namespace {

// Index: phase0 | (phase1 << 1) | (phase2 << 2) | (phase3 << 3). Value: detent 0…7
// (0° = phase1 / +x, then 45° steps CCW in math coords with phase0 as +y).
constexpr int8_t kDetentFromPhases[16] = {
    // Phase0 | Phase1 | Phase2 | Phase3 | Detent
    //   0    | 0      | 0      | 0      | -1  N/A
    //   1    | 0      | 0      | 0      | 0
    //   0    | 1      | 0      | 0      | 2
    //   1    | 1      | 0      | 0      | 1
    
    //   0    | 0      | 1      | 0      | 4
    //   1    | 0      | 1      | 0      | -1  N/A
    //   0    | 1      | 1      | 0      | 3
    //   1    | 1      | 1      | 0      | 2

    //   0    | 0      | 0      | 1      | 6
    //   1    | 0      | 0      | 1      | 7
    //   0    | 1      | 0      | 1      | -1  N/A
    //   1    | 1      | 0      | 1      | 0

    //   0    | 0      | 1      | 1      | 5
    //   1    | 0      | 1      | 1      | 6
    //   0    | 1      | 1      | 1      | 4
    //   1    | 1      | 1      | 1      | -1  N/A

    -1, 0,2,1,4,-1,3,2,6,7,-1,0,5,6,4,-1

};

} // namespace

// ─── Controller register dispatch ────────────────────────────────────────────

void Floppy525_woz::update_track() {

    int8_t cur_track = track;
    int8_t cur_phase = (cur_track % 8);

    const unsigned phase_bits =
        (phase0 << 0) | (phase1 << 1) | (phase2 << 2) | (phase3 << 3);
    const int8_t detent = kDetentFromPhases[phase_bits];
    //if (dbglog) fprintf(dbglog, "--- %lld update_phases: detent: %d\n", clock->get_cycles(), detent);
    if (detent == -1) return;  // forces cancel
    if (detent == cur_phase) return;

    // we now know which way the force is pointing.
    // need to determine which direction the head will move.
    // if will be whichever direction around the circle is a shortest path
    // from the current phase to the new phase.
    // phase and detent define two pie slices; one is smaller. the smaller one is the direction we'll go.
    uint8_t slice_add = (detent - cur_phase) & 7;
    uint8_t slice_subtract = (cur_phase - detent) & 7;
    if (slice_add == slice_subtract) return;
    
    if (slice_subtract < slice_add) {
        track -= slice_subtract;
    } else {
        track += slice_add;
    }
    //if (dbglog) fprintf(dbglog, "update_phases: track: %d slice_subtract: %d slice_add: %d\n", track, slice_subtract, slice_add);
    // if current phase is 0, and detent is < 4; subtract; if detent is > 4, add.
    
    if (track < 0) track = 0;
    if (track > 139) track = 139;

    if (track != cur_track) {
        update_track_ptr();
    }
}

void Floppy525_woz::phase_change_callback(uint64_t instanceID, void *userData) {
    Floppy525_woz *floppy = static_cast<Floppy525_woz *>(userData);
    floppy->update_track();
}

uint8_t Floppy525_woz::read_cmd(uint16_t address) {
    assert(false);
    return 0;
}

void Floppy525_woz::write_cmd(uint16_t address, uint8_t data) {
    assert(false);
}

uint64_t Floppy525_woz::fast_forward(uint64_t now) {

    uint64_t elapsed = now - last_cycle;
    last_cycle = now;

    if (!enable) {
        return 0; // the disk is not spinning
    }
    if ( !cur_track_ptr || cur_track_ptr->bit_count == 0) {
        // Motor off or no track: still update bit_fp so position is consistent
        // when the track becomes available, but don't shift any bits.
        // if there is no track we need to generate random bits..
        // we just need to return bits_to_sim based on the elapsed time.
        uint64_t bits_to_sim = elapsed * 2;
        return bits_to_sim;
        //return 0;
    }

    uint64_t track_bits = cur_track_ptr->bit_count;

    // Compute new physical position BEFORE updating read_position so we can derive
    // the exact number of whole bit-cells to simulate from the fractional
    // position already accumulated in bit_fp.
    //
    // Using (new_fp >> 3) - (bit_fp >> 3) instead of elapsed/4 is critical:
    // bit_fp may be mid-cell (e.g. bit_fp=30 = bit 3 + 6/8 of a cell elapsed).
    // 7 elapsed cycles (14 units) from that position completes cells 3 AND 4,
    // but elapsed/4=1 would only simulate 1, permanently skipping bits.

    head_position += (elapsed * 2);
    uint64_t bits_to_sim   = ((head_position >> 3) - (read_position >> 3)) % track_bits;
    return bits_to_sim;

}

inline uint8_t Floppy525_woz::get_random_bit() {
    random_bits = (random_bits << 1) | (random_bits >>63);
    return random_bits & 1;
}

uint8_t Floppy525_woz::read_pulse() {
    uint8_t bit;
    if (!enable || !cur_track_ptr || cur_track_ptr->bit_count == 0) {
        bit = 0; //get_random_bit(); // this will get randomized below when we check the window
    } else {
        // bit_fp is in 1/8-bit-cell units (see fast_forward / update_track_ptr).
        // Track bit index = bit_fp >> 3; byte in packed buffer = that / 8.
        uint64_t track_bits = cur_track_ptr->bit_count;
        uint64_t bi         = (read_position >> 3) % track_bits;
        uint64_t byte_idx   = bi >> 3;
        uint64_t bit_in_byte = bi & 7;
        size_t   need_bytes = (static_cast<size_t>(track_bits) + 7) / 8;
        if (byte_idx < cur_track_ptr->bits.size() && cur_track_ptr->bits.size() >= need_bytes) {
            bit = (cur_track_ptr->bits[byte_idx] >> (7 - static_cast<int>(bit_in_byte))) & 1;
        } else {
            // corrupted / inconsistent WOZ track metadata vs buffer
            bit = 0; // get_random_bit(); // this will get randomized below when we check the window
        }
    }

    windowBits = (windowBits << 1) | bit;
    if ((windowBits & 0x0F) == 0) { // four zero bits in a row start to insert random bits.
        bit = get_random_bit();
    }
    read_position += 8;
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        read_position %= cur_track_ptr->bit_count * 8;
    }
    return bit;
}

void Floppy525_woz::write_pulse(uint8_t bit) {
    if (enable && cur_track_ptr && cur_track_ptr->bit_count > 0) {
        uint64_t track_bits  = cur_track_ptr->bit_count;
        uint64_t bi          = (read_position >> 3) % track_bits;
        uint64_t byte_idx    = bi >> 3;
        uint64_t bit_in_byte = bi & 7;
        size_t   need_bytes  = (static_cast<size_t>(track_bits) + 7) / 8;
        if (byte_idx < cur_track_ptr->bits.size() &&
            cur_track_ptr->bits.size() >= need_bytes) {
            uint8_t mask = static_cast<uint8_t>(1u << (7 - static_cast<int>(bit_in_byte)));
            if (bit & 1) cur_track_ptr->bits[byte_idx] |= mask;
            else         cur_track_ptr->bits[byte_idx] &= static_cast<uint8_t>(~mask);
            modified = true;
        }
    }
    read_position += 8;
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        read_position %= cur_track_ptr->bit_count * 8;
    }
}
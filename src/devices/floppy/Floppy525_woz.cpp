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

#include <algorithm>
#include <cstdint>

#include "Floppy525_woz.hpp"
#include "util/woz_nibblizer_525.hpp"
#include "util/EventTimer.hpp"

// ─── Mount / head range ─────────────────────────────────────────────────────

bool Floppy525_woz::mount(uint64_t key, media_descriptor *media) {
    if (!Floppy_woz::mount(key, media)) {
        return false;
    }

    int hi_quarter = -1;
    const uint8_t *tmap = woz.image().tmap;
    for (int q = 0; q < 160; ++q) {
        if (tmap[q] != 0xFF) {
            hi_quarter = q;
        }
    }
    const unsigned span_slots =
        (hi_quarter < 0) ? 0u : static_cast<unsigned>(hi_quarter + 1);
    max_tracks = static_cast<int16_t>(std::max(140u, span_slots) - 1);

    if (track > max_tracks) {
        track = max_tracks;
        update_track_ptr();
    }
    return true;
}

bool Floppy525_woz::unmount(uint64_t key) {
    max_tracks = 139;
    return Floppy_woz::unmount(key);
}

// ─── Phase-line handling (5.25-specific stepper) ────────────────────────────

void Floppy525_woz::set_phase(uint8_t phase, uint8_t onoff) {
    switch (phase) {
        case 0: phase0 = onoff; break;
        case 1: phase1 = onoff; break;
        case 2: phase2 = onoff; break;
        case 3: phase3 = onoff; break;
    }

    // Schedule the actual detent resolution roughly 0.5 ms out so rapid
    // multi-phase toggles (typical DOS head-step sequence) collapse into
    // a single head-settling event.
    event_timer->scheduleEvent(clock->get_cycles() + 520, phase_change_callback,
                               0xABAB0001, this);
}

uint8_t Floppy525_woz::read_sense() {
    return write_protect;
}

namespace {

// Index: phase0 | (phase1 << 1) | (phase2 << 2) | (phase3 << 3). Value: detent
// 0..7 (-1 = invalid/no force).
//
// 0  = no phases on (no force)
// 1  = phase0 only -> detent 0
// 2  = phase1 only -> detent 2 (45°)
// 3  = phase0+1    -> detent 1 (midway)
// ...
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

void Floppy525_woz::update_track() {
    int8_t cur_track = track;
    int8_t cur_phase = (cur_track % 8);

    const unsigned phase_bits =
        (phase0 << 0) | (phase1 << 1) | (phase2 << 2) | (phase3 << 3);
    const int8_t detent = kDetentFromPhases[phase_bits];

    if (detent == -1) return;           // no net force / ambiguous
    if (detent == cur_phase) return;    // already at requested detent

    // Shortest-path rotation around the phase octagon: pick the smaller
    // of the CW/CCW arcs, and step the head by that many quarter-tracks.
    uint8_t slice_add      = (detent - cur_phase) & 7;
    uint8_t slice_subtract = (cur_phase - detent) & 7;
    if (slice_add == slice_subtract) return;

    if (slice_subtract < slice_add) {
        track -= slice_subtract;
    } else {
        track += slice_add;
    }

    if (track < 0) track = 0;
    if (track > max_tracks) track = max_tracks;

    if (track != cur_track) {
        update_track_ptr();
    }
}

void Floppy525_woz::phase_change_callback(uint64_t instanceID, void *userData) {
    (void)instanceID;
    Floppy525_woz *floppy = static_cast<Floppy525_woz *>(userData);
    floppy->update_track();
}

Woz_Nibblizer* Floppy525_woz::make_nibblizer(media_descriptor *media) {
    return new Woz_Nibblizer_525();
}
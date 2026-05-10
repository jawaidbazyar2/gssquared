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

// 5.25 Shugart-style drive: the four phase lines form a stepper-motor
// detent that directly positions the head in quarter-track increments.
class Floppy525_woz : public Floppy_woz {
    uint8_t rw_mode = 0;

    uint8_t phase0 = 0;
    uint8_t phase1 = 0;
    uint8_t phase2 = 0;
    uint8_t phase3 = 0;

    uint16_t image_index = 0;
    int16_t  track       = 0;  // quarter-track index (0..139 used, 0..159 in TMAP)

    void update_track();
    static void phase_change_callback(uint64_t instanceID, void *userData);

protected:
    uint32_t head_advance_per_cycle() const override { return 2; }
    int      current_tmap_index()     const override { return track; }

public:
    Floppy525_woz(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer)
        : Floppy_woz(sound_effect, clock, event_timer) {}

    virtual void set_phase(uint8_t phase, uint8_t onoff) override;

    virtual int get_track() override { return track; }
   
    virtual int get_side() override { return 0; }

    virtual uint8_t read_sense() override;

    void debug(DebugFormatter *f) override {
        f->addLine("Image: %s", woz.get_current_filename().c_str());
        f->addLine("enable: %d ph [%d,%d,%d,%d]", enable, phase0, phase1, phase2, phase3);
        f->addLine("Track: %d.%d", track/4, track%4);
        f->addLine("Track Bits: %llu", (unsigned long long)(cur_track_ptr ? cur_track_ptr->bit_count : 0));

        uint64_t pos = head_position>>3;
        f->addLine("Head Position: %llu (%d.%d)", pos, pos / 8, pos % 8);
        pos = read_position>>3;
        f->addLine("Read Position: %llu (%d.%d)", pos, pos / 8, pos % 8);
        f->addLine("Last Cycle: %llu", (unsigned long long)last_cycle);
        f->addLine("Modified: %d", modified);
    }
};

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
    int16_t  track       = 0;  // quarter-track index (clamped by max_tracks at mount)
    // Inclusive max quarter-track index from max(140, WOZ TMAP span); default 139.
    int16_t  max_tracks  = 139;

    void update_track();
    static void phase_change_callback(uint64_t instanceID, void *userData);

protected:
    uint64_t head_advance_per_cycle() const override { 
        //return 2;
        // 32 here is standard bit timing for 5.25" drives.
        return (2 * POSITION_ADV_PER_CYCLE * 32) / woz.image().info.optimal_bit_timing;
    }

    int      current_tmap_index()     const override { return track; }

public:
    Floppy525_woz(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer)
        : Floppy_woz(sound_effect, clock, event_timer) {
        }

    bool mount(uint64_t key, media_descriptor *media) override;
    bool unmount(uint64_t key) override;

    virtual Woz_Nibblizer* make_nibblizer(media_descriptor *media) override;

    virtual void set_phase(uint8_t phase, uint8_t onoff) override;

    virtual int get_track() override { return track; }
   
    virtual int get_side() override { return 0; }

    virtual uint8_t read_sense() override;

    void debug(DebugFormatter *f) override {
        f->addLine("Image: %s", woz.get_current_filename().c_str());
        f->addLine("Optimal Bit Timing: %d (advance_per_cycle: %llu)", woz.image().info.optimal_bit_timing, advance_per_cycle);
        f->addLine("enable: %d ph [%d,%d,%d,%d]", enable, phase0, phase1, phase2, phase3);
        f->addLine("Track: %d.%d [ max: %d.%d ]", track/4, track%4, max_tracks/4, max_tracks%4);
        f->addLine("Track Bits: %llu", (unsigned long long)(cur_track_ptr ? cur_track_ptr->bit_count : 0));

        Position pos_head = get_pos_head();
        Position pos_read = get_pos_read();
        f->addLine("Head Position: %llu.%llu", pos_head.pos, pos_head.fract);
        f->addLine("Read Position: %llu.%llu", pos_read.pos, pos_read.fract);
        f->addLine("Last Cycle: %llu", (unsigned long long)last_cycle);
        f->addLine("Modified: %d", modified);
    }
};

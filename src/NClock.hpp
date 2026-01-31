/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include "ui/MainAtlas.hpp" 
#include "devices/displaypp/VideoScannerII.hpp"
#include "platforms.hpp"

typedef enum {
    CLOCK_FREE_RUN = 0,
    CLOCK_1_024MHZ,
    CLOCK_2_8MHZ,
    CLOCK_7_159MHZ,
    CLOCK_14_3MHZ,
    NUM_CLOCK_MODES,
    INVALID_CLOCK_MODE = -1
} clock_mode_t;

enum clock_set_t {
    CLOCK_SET_US = 0,
    CLOCK_SET_PAL,
    NUM_CLOCK_SETS
};

class NClock {
 
protected:

    typedef struct {
        uint64_t hz_rate;                   // really just informational
        uint64_t c14M_per_second;           // primarily used by sound systems, the precise 14M clock rate.
        uint64_t cycles_per_frame;          // for VideoScanner, Mouse - precise number of CPU cycles per frame.
        uint64_t c_14M_per_cpu_cycle;       // helps tie CPU clock to 14M clock.
        uint64_t extra_per_scanline;        // this is the "stretched" cycle to keep video scanner in sync with CRT horizontal scan freq.
        uint64_t cycles_per_scanline;       // number CPU cycles per scanline.
        uint64_t c14M_per_frame;            // either 238944 (US) or 284544 (PAL)
        uint64_t us_per_frame_even;         // microseconds per frame for even frames
        uint64_t us_per_frame_odd;          // microseconds per frame for odd frames

    } clock_mode_info_t;

    clock_mode_info_t us_clock_mode_info[NUM_CLOCK_MODES] = {
        { 14'318'180, 14318180, 238420, 1, 0, 912, 238944, 16688154, 16688155 }, // cycle times here are fake.
        { 1'020'484, 14318180, 17030, 14, 2, 65, 238944, 16688154, 16688155  },
        { 2'857'370, 14318180,  47684, 5, 2, 182, 238944, 16688154, 16688155 },
        { 7'143'390, 14318180, 119210, 2, 2, 455, 238944, 16688154, 16688155 },
        { 14'286'780, 14318180, 238420, 1, 2, 912, 238944, 16688154, 16688155 }
    };
    
    clock_mode_info_t pal_clock_mode_info[NUM_CLOCK_MODES] = {
        { 14'250'450, 14250450, 283920, 1, 0, 912, 284544, 19967369, 19967370 }, // cycle times here are fake.
        { 1'015'657, 14250450,  20280, 14, 2, 65, 284544, 19967369, 19967370  },
        { 2'857'370, 14250450,  56784, 5, 2, 182, 284544, 19967369, 19967370 },
        { 7'143'390,14250450,  141960, 2, 2, 455, 284544, 19967369, 19967370 },
        { 14'250'450, 14250450, 283920, 1, 2, 910, 284544, 19967369, 19967370 }
        //     { 14'250'450,14250450,  284232, 1, 0, 912, 284544, 19967369, 19967370, 14'219'199 } // it was this which didn't work..
    };
    const char *clock_mode_names[NUM_CLOCK_MODES] = {
        "Ludicrous Speed",
        "1.0205 MHz",
        "2.8 MHz",
        "7.1435 MHz",
        "14.318 MHz"
    };

    uint32_t clock_mode_asset_ids[NUM_CLOCK_MODES] = {
        MHzInfinityButton,
        MHz1_0Button,
        MHz2_8Button,
        MHz7_159Button,
        MHz14_318Button
    };

    clock_mode_info_t *system_clock_mode_info = us_clock_mode_info;
    clock_mode_info_t current = us_clock_mode_info[CLOCK_1_024MHZ];

    clock_mode_t clock_mode = INVALID_CLOCK_MODE;

    // don't let anyone else touch these.
    uint64_t cycles;                // CPU cycle count
    uint64_t c_14M = 0;             // 14MHz cycles count

    // tight integration aka cross-dependency here.
    uint64_t video_cycle_14M_count = 0;  // 14MHz cycles since last video cycle
    uint64_t scanline_14M_count = 0;  // 14MHz cycles since last scanline

    VideoScannerII *video_scanner = nullptr;

public:
    
    NClock(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) {
        select_system_clock(clock_set);
    }

    inline uint64_t get_cycles() { return cycles; } // this should make accessing cycles fast still.
    inline uint64_t get_c14m() { return c_14M; }
    inline uint64_t get_c14m_per_cpu_cycle() { return current.c_14M_per_cpu_cycle; }
    inline uint64_t get_c14m_per_frame() { return current.c14M_per_frame; }
    inline uint64_t get_us_per_frame_even() { return current.us_per_frame_even; }
    inline uint64_t get_us_per_frame_odd() { return current.us_per_frame_odd; }
    inline const char *get_clock_mode_name() { return clock_mode_names[clock_mode]; }
    inline uint32_t get_clock_mode_asset_id(clock_mode_t mode) { return clock_mode_asset_ids[mode]; }
    inline uint32_t get_current_mode_asset_id() { return clock_mode_asset_ids[clock_mode]; }
    inline uint64_t get_video_cycle_14M_count() { return video_cycle_14M_count; }
    inline uint64_t get_scanline_14M_count() { return scanline_14M_count; }
    inline VideoScannerII *get_video_scanner() { return video_scanner; }

    void set_video_scanner(VideoScannerII *video_scanner) {
        this->video_scanner = video_scanner;
    }

    void select_system_clock(clock_set_t clock_set) {
        if (clock_set == CLOCK_SET_US) {
            system_clock_mode_info = us_clock_mode_info;
        } else if (clock_set == CLOCK_SET_PAL) {
            system_clock_mode_info = pal_clock_mode_info;
        }
    }

    clock_mode_t get_clock_mode() { return clock_mode; }

    void set_clock_mode(clock_mode_t mode) {
        clock_mode = mode;
        current = system_clock_mode_info[mode]; // copy the whole struct, avoids another pointer dereference.
    }

    // I forget who uses this.
    inline clock_mode_info_t *get_clock_line() {
        return &current;
    }

    clock_mode_t toggle(int direction) {
        int new_mode = (int)clock_mode + direction;
        
        if (new_mode < 0) {
            new_mode = (NUM_CLOCK_MODES - 1);
        } else if (new_mode >= NUM_CLOCK_MODES) {
            new_mode = 0;
        }
        fprintf(stdout, "Clock mode: %d\n", new_mode);
        return (clock_mode_t)new_mode;
    }

    inline virtual void slow_incr_cycles() {
        cycles++; 
    }
    
    inline void incr_cycles() {
        if (clock_mode == CLOCK_FREE_RUN) cycles++;
        else slow_incr_cycles();
    }
};

class NClockII : public NClock {
    public:
    NClockII(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) : NClock(clock_set, clock_mode) {
        clock_mode = CLOCK_1_024MHZ;
    }

    // II, II+, IIe
    inline virtual void slow_incr_cycles() override {
        cycles++; 
        c_14M += current.c_14M_per_cpu_cycle;
        
        if (video_scanner) {
            video_cycle_14M_count += current.c_14M_per_cpu_cycle;
            scanline_14M_count += current.c_14M_per_cpu_cycle;

            if (video_cycle_14M_count >= 14) {
                video_cycle_14M_count -= 14;
                video_scanner->video_cycle();
            }
            if (scanline_14M_count >= 910) {  // end of scanline
                c_14M += current.extra_per_scanline;
                scanline_14M_count = 0;
            }
        }
    }
};

class NClockFactory {
    public:
    static NClock *create_clock(PlatformId_t platform, clock_set_t clock_set) {
        if (platform == PLATFORM_APPLE_II) {
            return new NClockII(clock_set);
        } else if (platform == PLATFORM_APPLE_II_PLUS) {
            return new NClockII(clock_set);
        } else if (platform == PLATFORM_APPLE_IIE) {
            return new NClockII(clock_set);
        } else if (platform == PLATFORM_APPLE_IIGS) {
            return new NClockII(clock_set);
        } else {
            return new NClock(clock_set);
        }
    }
};
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
#include "PlatformIDs.hpp"

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

enum cycle_type_t {
    CYCLE_TYPE_SYNC = 0,
    CYCLE_TYPE_FAST_ROM = 1,
    CYCLE_TYPE_FAST = 2,
    CYCLE_TYPE_REFRESH = 3,
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
        set_clock_mode(clock_mode);
        cycles = 0;
        c_14M = 0;
        video_cycle_14M_count = 0;
        scanline_14M_count = 0;
    }

    inline uint64_t get_cycles() { return cycles; } // this should make accessing cycles fast still.
    inline uint64_t get_c14m() { return c_14M; }
    inline uint64_t get_hz_rate() { return current.hz_rate; }
    inline uint64_t get_c14m_per_cpu_cycle() { return current.c_14M_per_cpu_cycle; }
    inline uint64_t get_c14m_per_second() { return current.c14M_per_second; }
    inline uint64_t get_c14m_per_frame() { return current.c14M_per_frame; }
    inline uint64_t get_us_per_frame_even() { return current.us_per_frame_even; }
    inline uint64_t get_us_per_frame_odd() { return current.us_per_frame_odd; }
    inline const char *get_clock_mode_name() { return clock_mode_names[clock_mode]; }
    inline uint32_t get_clock_mode_asset_id(clock_mode_t mode) { return clock_mode_asset_ids[mode]; }
    inline uint32_t get_current_mode_asset_id() { return clock_mode_asset_ids[clock_mode]; }
    inline uint64_t get_video_cycle_14M_count() { return video_cycle_14M_count; }
    inline uint64_t get_scanline_14M_count() { return scanline_14M_count; }
    inline uint64_t get_cycles_per_scanline() { return current.cycles_per_scanline; }
    inline uint64_t get_cycles_per_frame() { return current.cycles_per_frame; }
    inline VideoScannerII *get_video_scanner() { return video_scanner; }
    inline void adjust_c14m(uint64_t amount) { c_14M += amount; }

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

class NClockIIgs : public NClockII {
protected:
    uint64_t ram_refresh_cycles = 0;
    uint64_t vidlinecycles = 0;
    uint64_t video_c14m = 0;
    bool slow_mode = false;

    public:
    NClockIIgs(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_2_8MHZ) : NClockII(clock_set, clock_mode) {
        clock_mode = CLOCK_2_8MHZ;
    }

    cycle_type_t cycle_type = CYCLE_TYPE_FAST;

    /* set the next cycle type. This is called by the MMU when it knows what kind of cycle it's doing. */
    inline void set_next_cycle_type(cycle_type_t type) { cycle_type = type; }

    inline void set_slow_mode(bool value) { slow_mode = value; }

    // IIgs
    inline void slow_incr_cycles()  {
        cycles++; 
        if (slow_mode) cycle_type = CYCLE_TYPE_SYNC;
        
        uint64_t c14m_this_cycle;
        if (cycle_type == CYCLE_TYPE_SYNC) {
    
            c14m_this_cycle = 14;                                   // if PH2 start lines up with PH0 start.. 
            if (video_cycle_14M_count) c14m_this_cycle += (14 - video_cycle_14M_count); // otherwise wait until end of next PH0, then 14 more C14s.
            ram_refresh_cycles = 0;                                 // our refresh needs are satisfied for a bit.
    
        } else if (cycle_type == CYCLE_TYPE_FAST_ROM) {
    
            c14m_this_cycle = current.c_14M_per_cpu_cycle;
            ram_refresh_cycles = 0;
            //ram_refresh_cycles ++;
            //if (ram_refresh_cycles == 9)  ram_refresh_cycles = 0;   // fast ROM cycle - free refresh, no extra 14Ms.        
    
        } else {  // regular "fast" cycle
            
            c14m_this_cycle = current.c_14M_per_cpu_cycle;
            ram_refresh_cycles += current.c_14M_per_cpu_cycle;
            if (ram_refresh_cycles >= 45) {
                ram_refresh_cycles -= 45;
                c14m_this_cycle += 5; // a refresh cycle is 10 14M's long total.
            } 
        } 
        c_14M += c14m_this_cycle;
    
        // if a slow cycle we can use 14-video_accum (or, 16-video_accum for h=64) to get the number of 14Ms to add to
        // c_14M to sync.
    
        if (video_scanner) {
            // Here we need to update the video clock to match with the CPU clock.
            // the previous video clock is video_c14m.
    
            // delta between previous video clock and current CPU clock.
            uint64_t delta = c_14M - video_c14m;
            video_cycle_14M_count += delta;
            //scanline_14M += delta;
    
            video_c14m = c_14M; // this is now caught up
    
            while (video_cycle_14M_count >= 14) {
                video_cycle_14M_count -= 14;
                // Do-video-cycle here
                
                video_scanner->video_cycle();
                
                vidlinecycles++;
            }
            if (vidlinecycles >= 65) {  // end of scanline
                vidlinecycles -= 65;
                c_14M += current.extra_per_scanline;
                video_c14m += current.extra_per_scanline;
            }
        }
        cycle_type = CYCLE_TYPE_FAST; // reset here so MMU doesn't have to set for all possible addresses
    }

};

class NClockFactory {
    public:
    static NClockII *create_clock(PlatformId_t platform, clock_set_t clock_set) {
        switch (platform) {
            case PLATFORM_APPLE_II:
                return new NClockII(clock_set);
            case PLATFORM_APPLE_II_PLUS:
                return new NClockII(clock_set);
            case PLATFORM_APPLE_IIE:
                return new NClockII(clock_set);
            case PLATFORM_APPLE_IIE_ENHANCED:
                return new NClockII(clock_set);
            case PLATFORM_APPLE_IIE_65816:
                return new NClockII(clock_set);
            case PLATFORM_APPLE_IIGS:
                return new NClockIIgs(clock_set);
            default:
                assert(false && "Unknown platform in NClockFactory::create_clock");    
                break;
        }
    }
};
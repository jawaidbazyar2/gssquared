#pragma once

#include <algorithm>

#include "VideoScannerII.hpp"

// AppleColor Composite Monitor IIe color killer (issues #217, #223).
// The monitor's burst gate only sees the last two of the IIe's four burst
// cycles (horz 15-16; the IIe emits burst on horz 13-16). It integrates burst
// energy: each line adds the number of gated burst cycles (0-2), and each line
// without burst bleeds the level off. Color turns on above kOnLevel and off
// below kOffLevel.
// Fit to hardware:
//   100 full-burst lines then text: color holds 131 lines, gone by next frame (#217)
//   VIDSYNC phase 0: graphics ends before horz 15, never color (#223)
//   VIDSYNC phase 1 (1 gated cycle): color from box line ~25, mono ~14 lines after the box
//   VIDSYNC phases 2-4 (2 gated cycles): color from box line ~12, mono ~40 lines after
// Known gap: #217 (full-line graphics) turns color on at line 7 on hardware,
// faster than VIDSYNC's full burst; this model gives line 13 for both.
// Level units are quarter burst cycles.
struct AppleColorKiller {
    static constexpr int kPerCycle = 4;
    static constexpr int kDecay = 5;
    static constexpr int kOnLevel = 24 * kPerCycle;
    static constexpr int kOffLevel = 15 * kPerCycle;
    static constexpr int kMaxLevel = 179 * kPerCycle;

    bool color = false;
    int level = 0;

    bool observe(int burst_cycles) noexcept {
        if (burst_cycles > 0) level = std::min(level + burst_cycles * kPerCycle, kMaxLevel);
        else level = std::max(level - kDecay, 0);
        if (!color && level > kOnLevel) color = true;
        else if (color && level < kOffLevel) color = false;
        return color;
    }
};

// One observe() per scanline, including VBL blanks that never become picture lines.
// After the line's emitted burst is known, color_mode.colorburst is the monitor decision.
struct AppleColorBurstGate {
    // The monitor gates the 3rd and 4th burst-window cycles.
    static constexpr int kGateFirst = 2;
    static constexpr int kGateCycles = 2;

    AppleColorKiller killer;
    bool line_noted = false;
    bool monitor_color = false;
    int burst_cycles = 0;
    int window_pos = 0;

    void begin_line() noexcept { line_noted = false; }

    void on_scan(color_mode_t &cm, bool &burst_latched, uint8_t flags,
                 bool mode_fallback, bool fallback_burst, bool suppress_burst) noexcept {
        // The last line of the frame has no HSYNC, so the previous latch would stick.
        if ((flags & VS_FL_BURST_WIN) && line_noted) {
            line_noted = false;
            burst_latched = false;
        }
        if (flags & VS_FL_BURST_WIN) {
            if (!burst_latched) {
                burst_cycles = 0;
                window_pos = 0;
            }
            if ((flags & VS_FL_COLORBURST) && window_pos >= kGateFirst) burst_cycles++;
            window_pos++;
            burst_latched = true;
        }
        if (!line_noted && burst_latched && !(flags & VS_FL_BURST_WIN))
            note(suppress_burst ? 0 : burst_cycles);
        else if (!line_noted && mode_fallback && !burst_latched)
            note((fallback_burst && !suppress_burst) ? kGateCycles : 0);
        if (line_noted)
            cm.colorburst = monitor_color ? 1 : 0;
    }

private:
    void note(int cycles) noexcept {
        line_noted = true;
        monitor_color = killer.observe(cycles);
    }
};

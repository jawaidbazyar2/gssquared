#pragma once

#include "VideoScannerII.hpp"

// AppleColor Composite Monitor IIe color killer (issue #217).
// The IIe emits colorburst on the burst window of a scanline. This monitor
// decodes the first 6 burst lines as mono and starts color on the 7th, and it
// keeps decoding color for 131 lines after burst stops (half of a 262-line
// NTSC frame). The hold counts vertical-blank lines, so it expires during VBL
// and the next picture starts mono.
struct AppleColorKiller {
    static constexpr int kOnDelay = 6;
    static constexpr int kOffHold = 131;

    bool color = false;
    int on_count = 0;
    int off_count = 0;

    bool observe(bool emitted) noexcept {
        if (emitted) {
            off_count = 0;
            if (!color && ++on_count > kOnDelay) color = true;
        } else {
            on_count = 0;
            if (color && ++off_count > kOffHold) color = false;
        }
        return color;
    }
};

// One observe() per scanline, including VBL blanks that never become picture lines.
// After the line's emitted burst is known, color_mode.colorburst is the monitor decision.
struct AppleColorBurstGate {
    AppleColorKiller killer;
    bool line_noted = false;
    bool monitor_color = false;

    void begin_line() noexcept { line_noted = false; }

    void on_scan(color_mode_t &cm, bool &burst_latched, uint8_t flags,
                 bool mode_fallback, bool fallback_burst, bool suppress_burst) noexcept {
        // The last line of the frame has no HSYNC, so the previous latch would stick.
        if ((flags & VS_FL_BURST_WIN) && line_noted) {
            line_noted = false;
            burst_latched = false;
        }
        latch_colorburst(cm, burst_latched, flags);
        if (!line_noted && burst_latched && !(flags & VS_FL_BURST_WIN))
            note(cm.colorburst && !suppress_burst);
        else if (!line_noted && mode_fallback && !burst_latched)
            note(fallback_burst && !suppress_burst);
        if (line_noted)
            cm.colorburst = monitor_color ? 1 : 0;
    }

private:
    void note(bool emitted) noexcept {
        line_noted = true;
        monitor_color = killer.observe(emitted);
    }
};

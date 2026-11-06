/*
 * The monitor attached to the Second Sight card: the window of the signal
 * it shows, from the signal's timing, and whether it can lock to it.
 */
#pragma once

#include <cmath>
#include <cstdint>

struct SecondSightMonitorWindow {
    int width = 0, height = 0;
    bool locked = false;
};

namespace ss_monitor {

// The monitors the card drives are CRTs. Each places the picture by the
// signal's timing; the user can center it with the position controls.
//
// VGA, a multisync monitor: it keeps its raster a fixed part of the line and
// of the frame. At 31.5 kHz the sync polarities select the IBM vertical size
// for 350, 400 or 480 lines (IBM PS/2 Video Subsystem, Oct 1990, Fig. 103);
// the windows are 656 of 800 dots (Fig. 108) and 362 of 449, 414 of 449 and
// 496 of 525 lines (Figs. 104-106). It locks from the IBM 31.469 kHz and
// 50 Hz (Fig. 6) to the fastest VESA DMT 1.10 timings the card's 78 MHz clock
// makes: 1024 x 768 at 75 Hz (60.023 kHz) and 1024 x 768 interlaced
// (86.96 Hz fields). The card's own VGA record $61 runs 1.0% below the IBM
// rates, so each limit has that tolerance.
inline constexpr double vga_line = 25'175'000.0 / 800, vga_tolerance = 0.01;
inline constexpr double vga_window_part = 656.0 / 800.0;
inline constexpr double vga_line_min = vga_line * (1 - vga_tolerance), vga_line_max = 78'750'000.0 / 1312 * (1 + vga_tolerance);
inline constexpr double vga_field_min = 50.0 * (1 - vga_tolerance), vga_field_max = 44'900'000.0 / 1264 / 817 * 2 * (1 + vga_tolerance);
// AppleColor RGB Monitor (Apple TIL01930): its sweep is set at the factory so
// that the IIgs 640 x 200 picture fills 200 x 150 mm of the 11.5-inch
// viewable face. The IIgs line is 65 counts of 14 cycles of the 14.318 MHz
// clock plus 2 for the long count, and 40 counts show (Apple IIe Technical
// Reference, video display), so the face spans that time x 292.1 / 250.
inline constexpr double applecolor_face = 11.5 * 25.4 / 250.0;
inline constexpr double applecolor_window_time = 40 * 14 / 14'318'180.0 * applecolor_face;
inline constexpr double applecolor_window_frame = 200 * (65 * 14 + 2) / 14'318'180.0 * applecolor_face;
// Apple specifies 15.734 kHz and 50 or 60 Hz. The AppleColor RGB records
// Sequential shipped for the card run from 13.31 kHz (M640X200.18M) to
// 18.75 kHz (M80X25.1575M) and up to 74.5 Hz (M560X192); the monitor shows
// nothing at 31 kHz.
inline constexpr double applecolor_line_min = 28'322'000.0 / 2 / 1064, applecolor_line_max = 72'000'000.0 / 4 / 960;
inline constexpr double applecolor_field_min = 50.0, applecolor_field_max = 28'322'000.0 / 2 / 856 / 222;

// The signal's line, field and pixel rates, its line in pixels, its frame in
// lines (both fields when interlaced), fields per frame and sync polarities.
inline SecondSightMonitorWindow card_monitor(bool vga, double line_hz, double field_hz, double pixel_hz,
                                             int total_width, int total_height, int fields,
                                             bool h_negative, bool v_negative) {
    if (vga) {
        double part = 496.0 / 525.0;
        if (std::abs(line_hz / vga_line - 1.0) <= vga_tolerance && h_negative != v_negative)
            part = (v_negative ? 362.0 : 414.0) / 449.0;
        return {int(total_width * vga_window_part + 0.5), int(total_height * part + 0.5),
                line_hz >= vga_line_min && line_hz <= vga_line_max && field_hz >= vga_field_min && field_hz <= vga_field_max};
    }
    return {int(applecolor_window_time * pixel_hz + 0.5), int(applecolor_window_frame * line_hz * fields + 0.5),
            line_hz >= applecolor_line_min && line_hz <= applecolor_line_max &&
            field_hz >= applecolor_field_min && field_hz <= applecolor_field_max};
}

// Floor of half a signed difference, so both directions round the same way.
inline int floor_half(int value) {
    return value >= 0 ? value / 2 : -((1 - value) / 2);
}

} // namespace ss_monitor

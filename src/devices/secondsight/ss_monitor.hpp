/*
 * The monitor attached to the Second Sight card: the window of the signal
 * it shows, from the signal's timing, and whether it can lock to it.
 */
#pragma once

#include "oti087.hpp"
#include "ss_crt.hpp"
#include "ss_lcd.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct SecondSightMonitorWindow {
    int width = 0, height = 0;
    bool locked = false;
};

namespace ss_monitor {

// The CRT monitors place the picture by the signal's timing. The user sets
// the size and position controls for the best picture of each mode: the
// picture keeps the aspect that the monitor's factory raster, below, gives
// its scan rates, and fills the screen. Both sizes change together until the
// unblanked signal, the active picture and its border, covers the screen, and
// further only where the whole active picture needs it. The unblanked signal
// is centered: border that does not fit is cropped evenly, the active picture
// never; any rest of the window is blanked signal.
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
// lines (both fields when interlaced), its unblanked and active pixels and
// lines, fields per frame and sync polarities.
inline SecondSightMonitorWindow card_monitor(bool vga, double line_hz, double field_hz, double pixel_hz,
                                             int total_width, int total_height, int unblanked_width,
                                             int unblanked_height, int active_width, int active_height, int fields,
                                             bool h_negative, bool v_negative) {
    double width, height;
    bool locked;
    if (vga) {
        double part = 496.0 / 525.0;
        if (std::abs(line_hz / vga_line - 1.0) <= vga_tolerance && h_negative != v_negative)
            part = (v_negative ? 362.0 : 414.0) / 449.0;
        width = total_width * vga_window_part;
        height = total_height * part;
        locked = line_hz >= vga_line_min && line_hz <= vga_line_max && field_hz >= vga_field_min && field_hz <= vga_field_max;
    } else {
        width = applecolor_window_time * pixel_hz;
        height = applecolor_window_frame * line_hz * fields;
        locked = line_hz >= applecolor_line_min && line_hz <= applecolor_line_max &&
                 field_hz >= applecolor_field_min && field_hz <= applecolor_field_max;
    }
    const double scale = std::max({std::min(unblanked_width / width, unblanked_height / height),
                                   active_width / width, active_height / height});
    return {int(width * scale + 0.5), int(height * scale + 0.5), locked};
}

// Floor of half a signed difference, so both directions round the same way.
inline int floor_half(int value) {
    return value >= 0 ? value / 2 : -((1 - value) / 2);
}

// The unblanked part of one axis, in pixels or lines from the start of the
// active display: blanking starts at blank_first and lasts blank_length of
// total. start is negative when the unblanked part begins before the active
// display, in the border of the preceding line or frame.
struct Unblanked {
    int start, length;
};
inline Unblanked unblanked(int total, int active, int blank_first, int blank_length) {
    const int length = total - std::clamp(blank_length, 0, total);
    const int end = (blank_first + blank_length) % total;
    return {end < active ? end : end - total, length};
}

// Offset of the active display in a window that is centered on the unblanked
// part. With no unblanked part, center the window on the active display. The
// active display always stays in the window.
inline int active_offset(int window, int active, Unblanked span) {
    const int centered = span.length <= 0 ? floor_half(window - active) : floor_half(window - span.length) - span.start;
    return std::clamp(centered, 0, std::max(0, window - active));
}

// On the AppleColor RGB, a mode with the IIgs screen's shape mirrors the IIgs
// screen: text of 24 rows, or graphics of 192 or 200 lines. The user sets the
// monitor to show it as GSSquared shows the IIgs screen (content_rec_vsg2 in
// display.cpp): the 19 lines above the picture and 240 lines in all, the lines
// with border in the IIgs RGB signal (Apple A2M6014 RGB Monitor timing chart),
// and 3/40 of the width at each side. The whole frame is border color, also
// where the card's signal blanks.
struct Frame {
    int width, height, left, top;
};
inline bool iigs_mirror(const OTI087 &video) {
    const auto &timing = video.display_timing();
    if (video.vga_monitor() || timing.interlaced) return false;
    if (video.gc_read(6) & 1) return timing.height == 192 || timing.height == 200;
    const int row = video.crtc_state().maximum_scan_line;
    return row > 0 && timing.height == 24 * row;
}
inline Frame iigs_frame(int active_width, int active_height) {
    const int side = (active_width * 3 + 20) / 40;
    return {side + active_width + side, std::max(240, active_height + 19), side, 19};
}

// The part of the card's signal that the monitor shows, and whether the
// monitor locks to it. Reads stopped Oak state only.
struct Scan {
    SecondSightCrt::Axis horizontal, vertical;
    bool locked;
};

// The output blanking in pixels and lines of the frame, from the start of
// the active display.
struct Blanking {
    int hfirst, hlength, vfirst, vlength;
};
inline Blanking blanking(const OTI087 &video) {
    const auto &timing = video.display_timing();
    // IBM VGA/XGA CRTC blanking excludes retrace, while attribute $11 colors
    // every visible overscan sample. Read stored state after synchronization;
    // neither monitor sense nor native image dimensions select a cosmetic mode.
    const auto &crtc = video.crtc_state();
    const int rows = timing.height / (crtc.vert_disp_end + 1);
    return {int(video.horizontal_blank_first() * timing.total_width / timing.line_characters) % timing.total_width,
            std::min(timing.total_width,
                int(video.horizontal_blank_length() * timing.total_width / timing.line_characters)),
            int(video.vertical_blank_first()) * rows % timing.total_height,
            std::min(timing.total_height, int(video.vertical_blank_length()) * rows)};
}

inline Scan scan(const OTI087 &video) {
    const auto &timing = video.display_timing();
    const auto [hfirst, hlength, vfirst, vlength] = blanking(video);
    const auto hspan = unblanked(timing.total_width, timing.width, hfirst, hlength);
    const auto vspan = unblanked(timing.total_height, timing.height, vfirst, vlength);
    // The monitor window, as the user's controls would set it.
    const int fields = timing.interlaced ? 2 : 1;
    SecondSightMonitorWindow window = card_monitor(video.vga_monitor(), 1.0 / timing.line_period(),
        fields / timing.frame_period(), 1.0 / timing.pixel_period(), timing.total_width, timing.total_height,
        hspan.length > 0 ? hspan.length : timing.width, vspan.length > 0 ? vspan.length : timing.height,
        timing.width, timing.height, fields, video.misc_output() & 0x40, video.misc_output() & 0x80);
    if (!window.locked) {
        // No picture, as on a monitor that cannot lock.
        window.width = timing.width;
        window.height = timing.height;
    }
    int window_width = std::clamp(window.width, 1, timing.total_width);
    int window_height = std::clamp(window.height, 1, timing.total_height);
    int left = window.locked ? active_offset(window_width, timing.width, hspan) : floor_half(window_width - timing.width);
    int top = window.locked ? active_offset(window_height, timing.height, vspan) : floor_half(window_height - timing.height);
    const bool mirror = window.locked && iigs_mirror(video);
    if (mirror) {
        const auto frame = iigs_frame(timing.width, timing.height);
        window_width = std::min(frame.width, timing.total_width);
        window_height = std::min(frame.height, timing.total_height);
        left = frame.left;
        top = frame.top;
    }
    return {{timing.total_width, (timing.total_width - left) % timing.total_width, window_width,
             hfirst, mirror ? 0 : hlength},
            {timing.total_height, (timing.total_height - top) % timing.total_height, window_height,
             vfirst, mirror ? 0 : vlength},
            window.locked};
}

// The HSYNC leading edge at the card's VGA connector, in pixels from the
// start of the active display there. HSYNC and VSYNC go from the Oak to the
// connector through 22 ohm resistors only; the pixels and blanking go through
// the Bt481, which the Oak's PCLK clocks. PCLK passes VCLK through on this
// board's Type 0 color-chip interface (OTI-087 data book p. 37).
// IBM's VGA, with the registers that IBM programs, starts HSync one border
// and one front porch after the active display (VGA/XGA TRM pp. 4-4, 4-5,
// timing set 1): its video leaves 2 characters after the CRTC count in
// graphics and 3 in text, which also reads the character generator. 3 dot
// clocks of that are IBM's DAC (TRM Fig. 2-97, T8). No source gives the
// Oak's own delay; this takes IBM's, because the Oak BIOS and the card's
// firmware program IBM's horizontal values unchanged. The Bt481 takes
// pipeline_clocks().
inline int connector_hsync(const OTI087 &video) {
    const auto &timing = video.display_timing();
    const auto &crtc = video.crtc_state();
    const int characters = int(timing.line_characters), pixels = timing.total_width;
    const int delay = (video.gc_read(6) & 1) ? 2 : 3;
    const int start = ((crtc.horz_retrace_start + crtc.horz_retrace_skew - delay) % characters + characters) %
        characters;
    // The Bt481 runs on the Oak's clock input, which U12 halves on the RGB path.
    const double clock = double(timing.oscillator_clock) / (timing.board_half_clock ? 2 : 1);
    const int dac = int(std::lround((int(video.dac().pipeline_clocks()) - 3) / (clock * timing.pixel_period())));
    return ((start * pixels / characters - dac) % pixels + pixels) % pixels;
}

// The card's VGA output on the LCD monitor: its sync as the monitor's
// controller measures it, and the leading edges of HSync and VSync in pixels
// and lines of a field from the start of the active display.
struct LcdSignal {
    bool present = false;
    ss_lcd::Timing timing;
    int hsync = 0, vsync = 0;
};
inline LcdSignal lcd_signal(const OTI087 &video) {
    LcdSignal signal;
    const auto &timing = video.display_timing();
    const auto &crtc = video.crtc_state();
    if (!crtc.sync_en || !crtc.horz_total || !crtc.horz_disp_end || !crtc.vert_disp_end) return signal;
    const int characters = int(timing.line_characters), pixels = timing.total_width;
    const int line_total = crtc.vert_total + 2;
    const int hwidth =
        OTI087::retrace_length(crtc.horz_retrace_start, crtc.horz_retrace_end & 31, 31, characters);
    const int vwidth =
        OTI087::retrace_length(crtc.vert_retrace_start, crtc.vert_retrace_end & 15, 15, line_total);
    // A pulse that never starts or never ends gives no sync edge.
    if (crtc.horz_retrace_start >= characters || crtc.vert_retrace_start >= line_total || !hwidth || !vwidth)
        return signal;
    const int divisor = crtc.sldiv ? 2 : 1;
    const int fields = timing.interlaced ? 2 : 1;
    // VSYNC is the retrace XOR Miscellaneous Output bit 7 XOR the Oak ext03
    // bit 5 toggle (vsync_r); HSYNC is the retrace XOR bit 6 (hsync_r).
    const bool v_negative = ((video.misc_output() >> 7) ^ (video.extended_read(3, true) >> 5)) & 1;
    signal.hsync = connector_hsync(video);
    signal.vsync = crtc.vert_retrace_start * divisor;
    signal.present = ss_lcd::measure(1.0 / timing.line_period(), timing.total_height / fields,
        double(hwidth) * pixels / characters * timing.pixel_period(), vwidth * divisor,
        !(video.misc_output() & 0x40), !v_negative, timing.interlaced, signal.timing);
    return signal;
}

// The window of the LCD monitor: the capture of the preset its controller
// picks, sampled with the preset's ADC clock and phase. An interlaced signal
// shows both fields woven.
inline Scan lcd_scan(const OTI087 &video, const ss_lcd::Monitor &lcd, const LcdSignal &signal) {
    const auto &timing = video.display_timing();
    const auto [hfirst, hlength, vfirst, vlength] = blanking(video);
    const auto &capture = lcd.capture();
    if (!lcd.supported())
        return {SecondSightCrt::Axis::visible(timing.total_width, timing.width, timing.total_width - timing.width),
                SecondSightCrt::Axis::visible(timing.total_height, timing.height, timing.total_height - timing.height),
                false};
    const int fields = timing.interlaced ? 2 : 1;
    return {{timing.total_width, signal.hsync % timing.total_width, capture.width, hfirst, hlength,
             capture.samples, capture.hstart, capture.phase},
            {timing.total_height, (signal.vsync + capture.vstart + 1) * fields % timing.total_height,
             capture.height * fields, vfirst, vlength},
            true};
}

// What sets the LCD picture's shape: the active display, the blanking, the
// border's brightness and the sync leading edges.
struct LcdShape {
    int width = 0, height = 0, total_width = 0, total_height = 0;
    Blanking blank = {};
    bool border = false;
    int hsync = 0, vsync = 0;
    bool operator==(const LcdShape &o) const {
        return width == o.width && height == o.height && total_width == o.total_width &&
            total_height == o.total_height && blank.hfirst == o.blank.hfirst && blank.hlength == o.blank.hlength &&
            blank.vfirst == o.blank.vfirst && blank.vlength == o.blank.vlength && border == o.border &&
            hsync == o.hsync && vsync == o.vsync;
    }
};
inline bool lcd_lit(uint32_t color) {
    return ((color >> 16) & 255) > 0x40 || ((color >> 8) & 255) > 0x40 || (color & 255) > 0x40;
}
inline LcdShape lcd_shape(const OTI087 &video, const LcdSignal &signal) {
    const auto &timing = video.display_timing();
    return {timing.width, timing.height, timing.total_width, timing.total_height, blanking(video),
            lcd_lit(video.border_color().value), signal.hsync, signal.vsync};
}

// The screen that the user runs Auto on, one field: the finest detail that
// the mode can show, white and black columns one pixel wide with white at both
// ends of the active display, the border in its color, and black where the
// card blanks. Auto finds a sharp clock only on a screen with detail; on a
// plain screen the clock it finds depends on the edges of the signal. The
// edges take the Bt481's rise and fall time, 3 ns from 10% to 90% (L481A_C
// Table 13).
inline ss_lcd::Picture lcd_picture(const OTI087 &video, const LcdSignal &signal) {
    const auto &timing = video.display_timing();
    const auto [hfirst, hlength, vfirst, vlength] = blanking(video);
    const int fields = timing.interlaced ? 2 : 1;
    ss_lcd::Picture picture;
    picture.width = timing.total_width;
    picture.lines = timing.total_height / fields;
    picture.hsync = signal.hsync;
    picture.vsync = signal.vsync;
    picture.edge = 3e-9 / 0.8 / timing.pixel_period();
    picture.active_width = timing.width;
    picture.active_lines = (timing.height + fields - 1) / fields;
    picture.level.assign(size_t(picture.width) * picture.lines, 0);
    picture.blue.assign(size_t(picture.width) * picture.lines, 0);
    const uint32_t border = video.border_color().value;
    const uint8_t border_level = std::max({uint8_t(border >> 16), uint8_t(border >> 8), uint8_t(border)});
    for (int line = 0; line < picture.lines; ++line) {
        const int y = line * fields;
        if ((y + timing.total_height - vfirst) % timing.total_height < vlength) continue;
        uint8_t *level = picture.level.data() + size_t(line) * picture.width;
        uint8_t *blue = picture.blue.data() + size_t(line) * picture.width;
        for (int x = 0; x < picture.width; ++x) {
            if ((x + timing.total_width - hfirst) % timing.total_width < hlength) continue;
            if (x < timing.width && y < timing.height) {
                level[x] = blue[x] = (x % 2 == 0 || x == timing.width - 1) ? 255 : 0;
            } else {
                level[x] = border_level;
                blue[x] = uint8_t(border);
            }
        }
    }
    return picture;
}

} // namespace ss_monitor

/*
 *   Check Second Sight presentation geometry without opening a window or
 *   writing frame dumps. The original 1.4-BEEF firmware and BE8C Lattice chip
 *   set each mode through captured Apple switches or the SetMode byte protocol
 *   in Sequential Systems' VGALOW.ASM. Oak timing defines the scan window;
 *   the host-selected aspect fits that whole window into the destination.
 *   The worker owns card state; this test thread inspects it only after
 *   catch_up() stops the worker at the requested host time.
 *
 *   One run visits text40, lores, hires, IIgs super-high-resolution (SHR), and
 *   the 640x480 mode used by picture viewers. It checks native and browser
 *   viewports and compares all native pixels before/after scan presentation.
 *   Small synthetic scans also check border, blanking and field pixels exactly.
 *   Source: ROM_BEEF.asm Apple video handlers and SetMode; the monitor's 4:3
 *   calibration is presentation policy, not an extra Oak register. Firmware
 *   errata: https://github.com/Webifi/second-sight-reference,
 *   docs/reference/errata.md.
 */

#include "devices/secondsight/ss_card.hpp"
#include "devices/secondsight/ss_display.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

// This test-thread bus source borrows the card owned by main(). RDY (Ready)
// holds make the Apple wait; charge them to the same 14.31818 MHz host clock
// as subsequent transfers. A bounded poll prevents an unending firmware wait.
struct Host {
    static constexpr uint64_t rate = 14'318'180;
    SSCard &card;
    uint64_t ticks = 0;
    // Present a slow Apple transfer and return the byte driven on reads.
    // bank selects the auxiliary bank; charge any card wait before proceeding.
    uint8_t access(uint16_t address, uint8_t value = 0, bool read = false, bool bank = false) {
        slot_bus_transfer_t t{ticks += 14, address, value, read, bank};
        ticks += card.observe(t);
        return t.data;
    }
    // Allow one fifth of a host second for firmware work after soft switches.
    void settle() { card.catch_up(ticks += rate / 5); }
    // Poll the card reply port for expected, failing after a bounded wait.
    void wait(uint8_t expected) {
        for (unsigned i = 0; i < 1000; ++i) {
            if (access(0xc0b8, 0, true) == expected) return;
            // Poll at millisecond intervals while the firmware performs
            // its potentially long screen clear; do not assume it is finished
            // after the fixed settling interval used for Apple soft switches.
            card.catch_up(ticks += rate / 1000);
        }
        assert(false && "card handshake timed out");
    }
    // Command 1, mode byte, native-video flag: the public viewer SetMode call.
    void mode(uint8_t number) {
        access(0xc0b0, 1);
        wait(1);
        access(0xc0b1, number);
        access(0xc0b1, 1);
        // The reply is 0, then $A5 when the mode is set. ROM 1.4's main loop
        // reads its mode-change flag before its reply flag ($1A3B, $1E32), so a
        // SetMode between those reads gets an early $A5 within microseconds and
        // the real one after the mode set's delay loop (second-sight-reference
        // docs/reference/errata.md, E29, fixed in firmware 1.5). Wait for $A5,
        // then past the second.
        wait(0xa5);
        settle();
    }
};

// Compare coordinates at subpixel precision; these are SDL floating rectangles.
static bool near(double a, double b) { return std::abs(a - b) < 0.001; }

// Quarter intensity is the presentation filter's beam-gap policy. Calculate
// each color channel separately so the check does not reuse its packed masks.
static uint32_t quarter(uint32_t pixel) {
    uint32_t result = 0xff000000;
    for (int shift : {0, 8, 16}) result |= (((pixel >> shift) & 255) / 4) << shift;
    return result;
}

// A small invented scan makes every expected coordinate explicit. The active
// image is four pixels by two lines. Overscan is the programmed border color;
// blanking suppresses the signal to black, including inside a monitor window.
// Negative window starts exercise counter wrap. IBM VGA/XGA TRM pp. 2-57/58,
// 2-73 describes blanking counters; the beam-gap intensity is host policy.
static void check_crt() {
    using Axis = SecondSightCrt::Axis;
    const auto visible = Axis::visible(8, 6, 4);
    assert(visible.start == 2 && visible.count == 4);
    for (int i = 0; i < 8; ++i) assert(visible.blank(i) == (i < 2 || i >= 6));
    assert(visible.position(0) == 2 && visible.position(3) == 5);
    assert(Axis::visible(8, 3, 8).count == 0);

    const Axis horizontal{8, -1, 8, 5, 2};
    const Axis vertical{6, -1, 6, 3, 2};
    // Window columns visit 7,0,1,2,3,4,5,6; rows visit 5,0,1,2,3,4.
    // -1 denotes overscan and -2 blanking. Other entries index the source.
    const int columns[] = {-1, 0, 1, 2, 3, -1, -2, -2};
    const int rows[] = {-1, 0, 1, -1, -2, -2};
    const int parity[] = {1, 0, 1, 0, 1, 0};
    const std::vector<uint32_t> original = {
        0xff010203, 0xfffefdfc, 0xff112233, 0xff445566,
        0xff778899, 0xffabcdef, 0xff123456, 0xff987654
    };
    for (bool filtered : {false, true}) {
        for (bool interlaced : {false, true}) {
            // Each filter instance belongs to this test thread. It reads a
            // completed image; no card state or worker is involved here.
            SecondSightCrt crt;
            auto source = original;
            uint32_t border = 0xff936a25;
            const int repeat = filtered && !interlaced ? 2 : 1;
            const int fields = filtered && interlaced ? 2 : 1;
            crt.configure(horizontal, vertical, filtered, interlaced, border);
            crt.update(source.data(), 4, 2, 0, 1);
            assert(crt.width() == 8 && crt.height() == 6 * repeat);
            assert(crt.fields() == (fields == 2));
            // Compare every output sample, including border and black samples.
            // Field parity follows the scan counter, not the window row index.
            auto check_pixels = [&](bool signal) {
                for (int field = 0; field < fields; ++field) {
                    for (int y = 0; y < 6; ++y) {
                        for (int x = 0; x < 8; ++x) {
                            uint32_t expected = border;
                            if (!signal || rows[y] == -2 || columns[x] == -2)
                                expected = 0xff000000;
                            else if (rows[y] >= 0 && columns[x] >= 0)
                                expected = source[rows[y] * 4 + columns[x]];
                            if (fields == 2 && parity[y] != field) expected = 0xff000000;
                            assert(crt.pixels(field)[y * repeat * 8 + x] == expected);
                            if (repeat == 2)
                                assert(crt.pixels(field)[(y * repeat + 1) * 8 + x] == quarter(expected));
                        }
                    }
                }
            };
            check_pixels(true);
            // Only this rectangle contains source pixels. Direct texture
            // uploads must not overwrite its surrounding overscan or blanking.
            const auto &regions = crt.active_regions();
            assert(regions.size() == 1);
            const auto &region = regions.front();
            assert(region.left == 1 && region.top == 1 && region.width == 4 && region.height == 2);
            assert(region.source_x == 0 && region.source_y == 0);
            crt.uploaded();
            assert(crt.direct_upload(4, 2) == !filtered);
            const auto *retained = crt.pixels();
            // An empty damage interval means the card produced no changed rows.
            crt.update(source.data(), 4, 2, 2, 1);
            assert(crt.bottom() < crt.top() && crt.pixels() == retained);
            source[5] ^= 0x00ffffff;
            crt.update(source.data(), 4, 2, 1, 1);
            assert(crt.top() == 2 * repeat && crt.bottom() == 3 * repeat - 1);
            check_pixels(true);
            crt.full_upload();
            assert(crt.top() == 0 && crt.bottom() == crt.height() - 1);
            crt.uploaded();
            if (!filtered) {
                // A direct GPU upload bypasses the retained CPU image. The
                // next CPU reconstruction must include that earlier change,
                // even when the newly supplied damage interval is empty.
                source[0] ^= 0x00ffffff;
                crt.source_uploaded();
                crt.update(source.data(), 4, 2, 2, 1);
                check_pixels(true);
                assert(crt.top() == 0 && crt.bottom() == crt.height() - 1);
            }
            // Border and signal changes invalidate the whole retained scan.
            border = 0xff123abc;
            crt.configure(horizontal, vertical, filtered, interlaced, border);
            crt.update(source.data(), 4, 2, 2, 1);
            check_pixels(true);
            crt.configure(horizontal, vertical, filtered, interlaced, border, false);
            crt.update(source.data(), 4, 2, 2, 1);
            check_pixels(false);
            assert(crt.active_regions().empty());
        }
    }
    std::puts("PASS scan wrap, overscan, blanking, beam gaps, fields, damage and retained pixels");
}

// The complete monitor window has one host-selected aspect. Active pixels and
// overscan share that fit; source raster dimensions do not select another fit.
// Explicit rectangles cover native, offset browser, tall and widescreen views.
static void check_rectangles() {
    auto check = [](SDL_FRect target, float aspect, SDL_FRect expected) {
        const auto actual = SecondSightDisplay::frame_rect(target, aspect);
        assert(near(actual.x, expected.x) && near(actual.y, expected.y));
        assert(near(actual.w, expected.w) && near(actual.h, expected.h));
    };
    check({0, 0, 640, 480}, 4.0f / 3, {0, 0, 640, 480});
    check({700, 20, 700, 480}, 4.0f / 3, {730, 20, 640, 480});
    check({0, 0, 320, 480}, 4.0f / 3, {0, 120, 320, 240});
    check({0, 0, 640, 480}, 16.0f / 9, {0, 60, 640, 360});
    std::puts("PASS native/browser complete-window aspect fit");
}

// Feed each real firmware raster through an unblanked scan window. This checks
// active-region copying and source preservation. It does not check monitor
// calibration or SDL texture submission.
static void check(Host &host, const char *name, int width, int height) {
    host.settle();
    auto &video = host.card.video();
    const auto timing = video.display_timing();
    assert(timing.width == width && timing.height == height);
    std::vector<uint32_t> before(width * height), after(before.size());
    video.render_frame({before.data(), width, height, width});
    const auto &crtc = video.crtc_state();
    // CRTC means cathode-ray tube controller. Horizontal blank counts are in
    // characters; vertical counts include scan repetition and both fields.
    const int rows = timing.height / (crtc.vert_disp_end + 1);
    const auto horizontal = SecondSightCrt::Axis::visible(timing.total_width,
        video.horizontal_blank_first() * timing.total_width / timing.line_characters,
        video.horizontal_blank_length() * timing.total_width / timing.line_characters);
    const auto vertical = SecondSightCrt::Axis::visible(timing.total_height,
        video.vertical_blank_first() * rows, video.vertical_blank_length() * rows);
    assert(horizontal.count > 0 && vertical.count > 0);
    SecondSightCrt scan;
    scan.configure(horizontal, vertical, false, timing.interlaced, video.border_color().value);
    scan.update(before.data(), width, height, 0, height - 1);
    assert(scan.width() == horizontal.count && scan.height() == vertical.count);
    assert(!scan.active_regions().empty());
    for (const auto &region : scan.active_regions()) {
        assert(region.left >= 0 && region.left + region.width <= scan.width());
        assert(region.top >= 0 && region.top + region.height <= scan.height());
        assert(region.source_x >= 0 && region.source_x + region.width <= width);
        assert(region.source_y >= 0 && region.source_y + region.height <= height);
        for (int y = 0; y < region.height; ++y)
            for (int x = 0; x < region.width; ++x)
                assert(scan.pixels()[(region.top + y) * scan.width() + region.left + x] ==
                    before[(region.source_y + y) * width + region.source_x + x]);
    }
    video.render_frame({after.data(), width, height, width});
    assert(before == after);
    std::printf("PASS %s raster=%dx%d scan=%dx%d regions=%zu source_pixels_equal=%zu\n",
        name, width, height, scan.width(), scan.height(), scan.active_regions().size(), before.size());
}

// Read the supplied original firmware image.
int main(int argc, char **argv) {
    // The optional selection permits a failed viewer handshake to be checked
    // without repeating the already completed Apple-mode cases.
    assert(argc == 2 || (argc == 3 && std::string(argv[2]) == "--viewer"));
    check_crt();
    check_rectangles();
    std::array<uint8_t, SSCard::ROM_SIZE> rom;
    std::ifstream in(argv[1], std::ios::binary);
    in.read(reinterpret_cast<char *>(rom.data()), rom.size());
    assert(in.gcount() == rom.size());
    SSCard card(rom.data(), Host::rate);
    card.start_worker();
    Host host{card};
    host.settle();
    if (argc == 2) {
        host.access(0xc00c); // 40-column text, full screen, page 1.
        host.access(0xc051);
        host.access(0xc052);
        host.access(0xc054);
        for (unsigned i = 0; i < 1024; ++i) host.access(0x400 + i, 0xa0 + i % 64);
        check(host, "text40", 360, 384);
        host.access(0xc056); // Lores and hires use the same captured page bytes.
        host.access(0xc050);
        check(host, "lores", 640, 400);
        host.access(0xc057);
        for (unsigned i = 0; i < 8192; ++i) host.access(0x2000 + i, i);
        check(host, "hires", 320, 400);
        // SHR scan-line controls request 640 pixels. Bank is sampled before mapping.
        for (unsigned i = 0; i < 200; ++i) host.access(0x9d00 + i, 0x80, false, true);
        host.access(0xc029, 0x81);
        check(host, "SHR", 640, 400);
    }
    host.mode(0x53); // 640x480 indexed-color mode used by the picture viewers.
    check(host, "viewer mode $53", 640, 480);
    // Original firmware RGB mode $61 uses two 200-line fields. Select it through
    // the documented SetMode handshake to exercise a real interlaced Oak scan.
    card.video().monitor_type_w(1);
    host.mode(0x61);
    assert(card.video().display_interlaced());
    check(host, "RGB mode $61", 640, 400);
    std::puts("PASS selected firmware modes, native/browser geometry, source pixels unchanged");
}

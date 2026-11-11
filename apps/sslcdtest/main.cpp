/*
 *   Check the VGA LCD monitor model. Known timings must give the presets that
 *   the monitor's RTD2556 firmware picks for them, and its limits must hold.
 *   Then the original 1.4-BEEF firmware sets two of its modes through the
 *   SetMode byte protocol, and Auto must put the whole active picture in the
 *   capture.
 */

#include "devices/secondsight/ss_card.hpp"
#include "devices/secondsight/ss_monitor.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

// This test-thread bus source borrows the card owned by main(), as in the
// geometry test.
struct Host {
    static constexpr uint64_t rate = 14'318'180;
    SSCard &card;
    uint64_t ticks = 0;
    uint8_t access(uint16_t address, uint8_t value = 0, bool read = false) {
        slot_bus_transfer_t t{ticks += 14, address, value, read, false};
        ticks += card.observe(t);
        return t.data;
    }
    void settle() { card.catch_up(ticks += rate / 5); }
    void wait(uint8_t expected) {
        for (unsigned i = 0; i < 1000; ++i) {
            if (access(0xc0b8, 0, true) == expected) return;
            card.catch_up(ticks += rate / 1000);
        }
        assert(false && "card handshake timed out");
    }
    // Command 1, mode byte, native-video flag. Wait for $A5, SetMode's
    // completion byte, then run the card 0.2 s more: with Apple emulation off,
    // ROM 1.4 programs the mode again (and sends $A5 again) after each change
    // of an Apple display switch, and that must be over before the test reads.
    void mode(uint8_t number) {
        access(0xc0b0, 1);
        wait(1);
        access(0xc0b1, number);
        access(0xc0b1, 1);
        wait(0xa5);
        settle();
    }
};

// The preset for a timing: pixel clock, pixels and lines per frame, sync
// widths in pixels and lines, sync polarities.
static int preset(double clock, int pixels, int lines, int hsync, int vsync, bool hp, bool vp) {
    ss_lcd::Timing t;
    if (!ss_lcd::measure(clock / pixels, lines, hsync / clock, vsync, hp, vp, false, t)) return -2;
    const int index = ss_lcd::search(t);
    return index >= 0 && ss_lcd::supports(t, index, ss_lcd::presets[index].samples) ? index : -1;
}

int main(int argc, char **argv) {
    // VGA 640 x 480 and 720 x 400; 640 x 350 has HSync positive and VSync negative.
    assert(preset(25'175'000, 800, 525, 96, 2, false, false) == 9);
    assert(preset(28'322'000, 900, 449, 108, 2, false, true) == 4);
    assert(preset(25'175'000, 800, 449, 96, 2, true, false) == 0);
    // 640 x 480 with a VSync of 5 lines becomes 720 x 480.
    assert(preset(25'175'000, 800, 525, 96, 5, false, false) == 15);
    assert(preset(25'175'000, 800, 525, 96, 5, true, true) == 119);
    // VESA 800 x 600 at 60 Hz. A 640-line mode at 38.17 kHz gets the same
    // preset: of the presets near its rates, the controller
    // picks the one nearest its estimate of the active lines.
    assert(preset(40'000'000, 1056, 628, 128, 4, true, true) == 19);
    assert(preset(38'170.0 * 1056, 1056, 640, 115, 3, false, false) == 19);
    // Out of range: a 9 kHz line, and fields slower than the 60 ms VSync wait.
    assert(preset(9'000.0 * 800, 800, 300, 64, 2, false, false) == -1);
    assert(preset(31'469.0 * 800, 800, 2200, 96, 2, false, false) == -2);

    // Auto puts the active picture of the firmware's 640 x 480 and 640 x 400
    // modes in the capture at one sample a pixel; for 640 x 400 the user picks
    // the 640-wide preset of that timing.
    std::array<uint8_t, SSCard::ROM_SIZE> rom;
    std::ifstream in(argv[1], std::ios::binary);
    assert(argc == 2 && in.read(reinterpret_cast<char *>(rom.data()), rom.size()));
    SSCard card(rom.data(), Host::rate);
    card.video().monitor_type_w(false);
    card.start_worker();
    Host host{card};
    host.settle();
    struct Case { uint8_t mode; int index, samples; };
    for (const Case c : {Case{0x53, 9, 800}, Case{0x61, 5, 808}}) {
        host.mode(c.mode);
        auto &video = card.video();
        const auto &timing = video.display_timing();
        ss_lcd::Monitor lcd;
        const auto signal = ss_monitor::lcd_signal(video);
        assert(lcd.follow(signal.present, signal.timing) && lcd.supported() && lcd.needs_picture());
        lcd.picture(ss_monitor::lcd_picture(video, signal));
        const auto &capture = lcd.capture();
        const auto scan = ss_monitor::lcd_scan(video, lcd, signal);
        std::printf("SetMode $%02X: preset %d, %d samples, start %d,%d\n", c.mode, capture.index, capture.samples,
                    capture.hstart, capture.vstart);
        assert(capture.index == c.index && capture.samples == c.samples);
        assert(scan.horizontal.position(0) == 0 && scan.horizontal.count == timing.width);
        assert(scan.vertical.position(0) == 0 && scan.vertical.position(capture.height - 1) == timing.height - 1);
    }
    std::puts("sslcdtest passed");
    return 0;
}

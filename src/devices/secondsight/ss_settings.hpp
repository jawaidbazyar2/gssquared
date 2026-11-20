/*
 *   The Second Sight card's preferences. The application keeps them in its
 *   device settings (SystemSettings::device_setting) under the device name
 *   "second_sight". Firmware, Lattice chip and INT0 pull-up are choices of
 *   hardware: the card takes them at power-on or Restart. The monitor and the
 *   view apply at once.
 */

#pragma once

struct SSSettings {
    enum Firmware { FW_14, FW_15, FW_20, FW_COUNT };
    // The monitor on the card's output. The firmware senses VGA or RGB.
    enum Monitor { MONITOR_RGB, MONITOR_VGA_CRT, MONITOR_VGA_LCD, MONITOR_COUNT };
    // The monitors that the application shows: the Apple's, the card's or both.
    enum View { VIEW_APPLE, VIEW_CARD, VIEW_BOTH, VIEW_COUNT };

    int firmware = FW_14;
    bool lattice_v2 = false;
    bool int0_pullup = false;
    // AppleColor RGB is the IIgs's own monitor. Both monitors show by default,
    // so the firmware's startup does not hide the Apple screen.
    int monitor = MONITOR_RGB;
    int view = VIEW_BOTH;

    // Read the stored preferences. A missing or unknown value keeps its default.
    void load();
    void set_firmware(int value);
    void set_lattice_v2(bool value);
    void set_int0_pullup(bool value);
    void set_monitor(int value);
    void set_view(int value);
    // The EPROM image of the selected firmware.
    const char *rom_filename() const;
};

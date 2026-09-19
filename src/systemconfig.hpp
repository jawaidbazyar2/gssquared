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

#include "PlatformIDs.hpp"
#include "Device_ID.hpp"
#include "NClock.hpp"
#include "devices/displaypp/VideoScanner.hpp"

/** Host monitor type in .gs2 `display`. Values match MenuMonitorID. */
enum display_monitor_t {
    DISPLAY_MONITOR_UNSET = -1,
    DISPLAY_MONITOR_COMPOSITE = 200,
    DISPLAY_MONITOR_RGB = 201,
    DISPLAY_MONITOR_GREEN = 202,
    DISPLAY_MONITOR_AMBER = 203,
    DISPLAY_MONITOR_WHITE = 204,
};

/**
 * a System Configuraiton is a platform, and, a list of devices and their slots.
 */

struct SystemConfig_t {
    const char *name;
    PlatformId_t platform_id;
    //int image_id;
    bool builtin;
    clock_set_t clock_set;
    video_scanner_t scanner_type;
    const char *description;
    const char *id;  // machine identity (UUID); IIgs BRAM lives in the .gs2 `bram` field
    device_id slot_devices[NUM_SLOTS];
    clock_mode_t clock_mode = INVALID_CLOCK_MODE; // host speed; unset → platform default
    int display_monitor = DISPLAY_MONITOR_UNSET;  // unset → NTSC, or RGB on IIgs
};

extern SystemConfig_t BuiltinSystemConfigs[];
extern const int NUM_SYSTEM_CONFIGS;

SystemConfig_t *get_system_config(int index);

// Walks BuiltinSystemConfigs and returns the first system index whose
// platform_id matches `platform_id`, or -1 if none match. Used for the
// command-line `-p PLATFORM` auto-launch path so we can pick a system
// without going through the SelectSystem UI.
int find_first_system_for_platform(int platform_id);

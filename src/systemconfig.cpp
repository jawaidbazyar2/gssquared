/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include "gs2.hpp"
#include "systemconfig.hpp"
#include "ui/MainAtlas.hpp"
#include "devices/displaypp/VideoScanner.hpp"

DeviceMap_t DeviceMap_II[] = {
    {DEVICE_ID_KEYBOARD_IIPLUS, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_DISPLAY, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_PARALLEL, SLOT_1},
    {DEVICE_ID_LANGUAGE_CARD, SLOT_0},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIPLUS[] = {
    {DEVICE_ID_KEYBOARD_IIPLUS, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_DISPLAY, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_LANGUAGE_CARD, SLOT_0},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_PRODOS_CLOCK, SLOT_2},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MEM_EXPANSION, SLOT_7},
    {DEVICE_ID_PARALLEL, SLOT_1},
    {DEVICE_ID_VIDEX, SLOT_3},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE[] = {
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_MOUSE, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE_MOUSE[] = {
    {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOUSE, SLOT_4},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIX[] = {
    {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOUSE, SLOT_4},
    {DEVICE_ID_VIDHD, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE_ENH_2MB[] = {
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_MOCKINGBOARD, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIGS[] = {
    {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, 
    {DEVICE_ID_KEYGLOO, SLOT_NONE}, // Keyboard should be before IIGS_MEMORY
    //{DEVICE_ID_IIGS_MEMORY, SLOT_NONE},
    {DEVICE_ID_RTC_PRAM, SLOT_NONE},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_ENSONIQ, SLOT_NONE},
    {DEVICE_ID_SCC8530, SLOT_NONE},
    {DEVICE_ID_PD_BLOCK2, SLOT_7},
    /* {DEVICE_ID_DISK_II, SLOT_7}, */
    {DEVICE_ID_END, SLOT_NONE}
};

SystemConfig_t BuiltinSystemConfigs[] = {
    {
        "Apple ][", 
        PLATFORM_APPLE_II, 
        DeviceMap_II,
        Badge_II,
        true,
        CLOCK_SET_US,
        Scanner_AppleII,
        "48K RAM; Disk II",
    },
    {
        "Apple ][+", 
        PLATFORM_APPLE_II_PLUS, 
        DeviceMap_IIPLUS,
        Badge_IIPlus,
        true,
        CLOCK_SET_US,
        Scanner_AppleII,
        "64K RAM (incl Lang Card); Disk II; ProDOS Clock; Parallel Port; VIDEX 80-col; Mockingboard",
    },
    {
        "Apple IIe",
        PLATFORM_APPLE_IIE,
        DeviceMap_IIE,
        Badge_IIE,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; ProDOS Clock; Parallel Port; Mockingboard",
    },
    {
        "Apple IIe Enhanced",
        PLATFORM_APPLE_IIE_ENHANCED,
        DeviceMap_IIE,
        Badge_IIEEnh,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; ProDOS Clock; Parallel Port; Mockingboard",
    },
    {
        "Apple IIe Enhanced w/Mouse",
        PLATFORM_APPLE_IIE_ENHANCED,
        DeviceMap_IIE_MOUSE,
        Badge_IIEEnh,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; ProDOS Clock; Mouse",
    },
    {
        "Apple IIe Enhanced Dual Mockingboard",
        PLATFORM_APPLE_IIE_ENHANCED,
        DeviceMap_IIE_ENH_2MB,
        Badge_IIEEnh,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; ProDOS Clock; DUAL Mockingboard",
    },
    {
        "Apple IIe 65816 w/Mouse",
        PLATFORM_APPLE_IIE_65816,
        DeviceMap_IIX,
        Badge_IIEEnh,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIgs,
        "128K RAM; Disk II; ProDOS Clock; Mouse; 65816"
    }, 
    {
        "Apple IIe Enhanced / PAL",
        PLATFORM_APPLE_IIE_ENHANCED,
        DeviceMap_IIE,
        Badge_IIEEnh,
        true,
        CLOCK_SET_PAL,
        Scanner_AppleIIePAL,
        "PAL Video; 128K RAM; Disk II; ProDOS Clock; Parallel Port; Mockingboard",
    },
    {
        "Apple IIgs",
        PLATFORM_APPLE_IIGS,
        DeviceMap_IIGS,
        Badge_IIGS,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIgs,
        "Apple IIgs 8MB RAM"
    },
/*     {
        "Apple IIc",
        PLATFORM_APPLE_IIC,
        DeviceMap_IIC,
        true
    }, */
    // Add more built-in configurations as needed
};

const int NUM_SYSTEM_CONFIGS = sizeof(BuiltinSystemConfigs) / sizeof(BuiltinSystemConfigs[0]);

SystemConfig_t *get_system_config(int index) {
    return &BuiltinSystemConfigs[index];
}

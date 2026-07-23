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

#include "gs2.hpp"
#include "systemconfig.hpp"
#include "devices/displaypp/VideoScanner.hpp"
#include "devices.hpp"

DeviceMap_t DeviceMap_II[] = {
    /* {DEVICE_ID_KEYBOARD_IIPLUS, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_DISPLAY, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_PARALLEL, SLOT_1},
    {DEVICE_ID_LANGUAGE_CARD, SLOT_0},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIPLUS[] = {
    /* {DEVICE_ID_KEYBOARD_IIPLUS, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_DISPLAY, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_LANGUAGE_CARD, SLOT_0},
    {DEVICE_ID_PD_BLOCK3, SLOT_5},
    {DEVICE_ID_PRODOS_CLOCK, SLOT_2},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MEM_EXPANSION, SLOT_7},
    {DEVICE_ID_PARALLEL, SLOT_1},
    {DEVICE_ID_VIDEX, SLOT_3},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE[] = {
    /* {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK3, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_MOUSE, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE_MOUSE[] = {
    /* {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK3, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOUSE, SLOT_4},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIX[] = {
    /* {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK3, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOUSE, SLOT_4},
    {DEVICE_ID_VIDHD, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE_ENH_2MB[] = {
    /* {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_CASSETTE, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE}, */
    {DEVICE_ID_PD_BLOCK3, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    {DEVICE_ID_MOCKINGBOARD, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIGS[] = {
    /* {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, 
    {DEVICE_ID_KEYGLOO, SLOT_NONE}, // Keyboard should be before IIGS_MEMORY
    {DEVICE_ID_RTC_PRAM, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_ENSONIQ, SLOT_NONE},
    {DEVICE_ID_SCC8530, SLOT_NONE},
    {DEVICE_ID_IWM, SLOT_NONE}, */
    {DEVICE_ID_PD_BLOCK3, SLOT_7},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIGS_2[] = {
   /*  {DEVICE_ID_SPEAKER, SLOT_NONE}, // speaker must be before display so iigs can override some things.
    {DEVICE_ID_DISPLAY, SLOT_NONE}, 
    {DEVICE_ID_KEYGLOO, SLOT_NONE}, // Keyboard should be before IIGS_MEMORY
    {DEVICE_ID_RTC_PRAM, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_ENSONIQ, SLOT_NONE},
    {DEVICE_ID_SCC8530, SLOT_NONE},
    {DEVICE_ID_IWM, SLOT_NONE}, */
    {DEVICE_ID_END, SLOT_NONE}
};

SystemConfig_t BuiltinSystemConfigs[] = {
    {
        "Apple ][", 
        PLATFORM_APPLE_II, 
        //Badge_II,
        true,
        CLOCK_SET_US,
        Scanner_AppleII,
        "48K RAM; Disk II",
        "6b2f0c1a-8e4d-4a91-9c3b-1f7e2d4a5b01",
        {
            DEVICE_ID_LANGUAGE_CARD,
            DEVICE_ID_PARALLEL,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_DISK_II,
            DEVICE_ID_NONE        
        },
    },
    {
        "Apple ][+", 
        PLATFORM_APPLE_II_PLUS, 
        //Badge_IIPlus,
        true,
        CLOCK_SET_US,
        Scanner_AppleII,
        "64K RAM (incl Lang Card); Disk II; Clock; Parallel Port; VIDEX 80-col; Mockingboard",
        "6b2f0c1a-8e4d-4a91-9c3b-1f7e2d4a5b02",
        {
            DEVICE_ID_LANGUAGE_CARD,
            DEVICE_ID_PARALLEL,
            DEVICE_ID_PRODOS_CLOCK,
            DEVICE_ID_VIDEX,
            DEVICE_ID_MOCKINGBOARD,
            DEVICE_ID_PD_BLOCK3,
            DEVICE_ID_DISK_II,
            DEVICE_ID_MEM_EXPANSION,
        },
    },
    {
        "Apple IIe",
        PLATFORM_APPLE_IIE,
        //Badge_IIE,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; AppleMouse II; Mockingboard",
        "6b2f0c1a-8e4d-4a91-9c3b-1f7e2d4a5b03",
        {
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_MEM_EXPANSION,
            DEVICE_ID_NONE,
            DEVICE_ID_MOCKINGBOARD,
            DEVICE_ID_PD_BLOCK3,
            DEVICE_ID_DISK_II,
            DEVICE_ID_MOUSE,
        },
    },
    {
        "Apple IIe Enhanced",
        PLATFORM_APPLE_IIE_ENHANCED,
        //Badge_IIEEnh,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIe,
        "128K RAM; Disk II; Clock; AppleMouse II; Mockingboard",
        "6b2f0c1a-8e4d-4a91-9c3b-1f7e2d4a5b04",
        {
            DEVICE_ID_NONE,
            DEVICE_ID_PRODOS_CLOCK,
            DEVICE_ID_MEM_EXPANSION,
            DEVICE_ID_NONE,
            DEVICE_ID_MOCKINGBOARD,
            DEVICE_ID_MOUSE,
            DEVICE_ID_DISK_II,
            DEVICE_ID_PD_BLOCK3
        },
    },
    /* Extra configs (Dual Mockingboard, IIe 65816, IIe PAL, IIgs Disk II)
       ship as .gs2 under assets/gs2/ and are seeded into PrefPath/SystemConfigs. */
    {
        "Apple IIgs",
        PLATFORM_APPLE_IIGS,
        //DeviceMap_IIGS,
        //Badge_IIGS,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIgs,
        "Apple IIgs 8MB RAM",
        "6b2f0c1a-8e4d-4a91-9c3b-1f7e2d4a5b05",
        {
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_SECOND_SIGHT,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_PD_BLOCK3,
        }
    },
    {
        "Apple IIgs (ROM 3)",
        PLATFORM_APPLE_IIGS_ROM3,
        //DeviceMap_IIGS,
        //Badge_IIGS,
        true,
        CLOCK_SET_US,
        Scanner_AppleIIgs,
        "Apple IIgs (ROM 3) 8MB RAM",
        "ec541ad4-f57c-4889-96b8-9c854b11dd10",
        {
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_SECOND_SIGHT,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_NONE,
            DEVICE_ID_PD_BLOCK3,
        }
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

int find_first_system_for_platform(int platform_id) {
    for (int i = 0; i < NUM_SYSTEM_CONFIGS; ++i) {
        if (BuiltinSystemConfigs[i].platform_id == platform_id) {
            return i;
        }
    }
    return -1;
}

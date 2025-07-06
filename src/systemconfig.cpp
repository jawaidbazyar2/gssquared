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

DeviceMap_t DeviceMap_II[] = {
    {DEVICE_ID_KEYBOARD_IIPLUS, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
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
   // {DEVICE_ID_MOCKINGBOARD, SLOT_1},
    {DEVICE_ID_ANNUNCIATOR, SLOT_NONE},
    {DEVICE_ID_END, SLOT_NONE}
};

DeviceMap_t DeviceMap_IIE[] = {
    {DEVICE_ID_DISPLAY, SLOT_NONE}, // display must be before IIE_MEMORY
    {DEVICE_ID_KEYBOARD_IIE, SLOT_NONE}, // Keyboard should be before IIE_MEMORY
    {DEVICE_ID_IIE_MEMORY, SLOT_NONE},
    {DEVICE_ID_SPEAKER, SLOT_NONE},
    {DEVICE_ID_GAMECONTROLLER, SLOT_NONE},
    {DEVICE_ID_MEM_EXPANSION, SLOT_2},
    {DEVICE_ID_PD_BLOCK2, SLOT_5},
    {DEVICE_ID_DISK_II, SLOT_6},
    {DEVICE_ID_MOCKINGBOARD, SLOT_4},
    //{DEVICE_ID_ANNUNCIATOR, SLOT_NONE}, // no annunciator because these are overridden by display - maybe have a IIe annunciator with less registers.
    {DEVICE_ID_END, SLOT_NONE}
};

SystemConfig_t BuiltinSystemConfigs[] = {
    {
        "Apple ][", 
        PLATFORM_APPLE_II, 
        DeviceMap_II,
        Badge_II,
        true
    },
    {
        "Apple ][+", 
        PLATFORM_APPLE_II_PLUS, 
        DeviceMap_IIPLUS,
        Badge_IIPlus,
        true
    },
    {
        "Apple IIe",
        PLATFORM_APPLE_IIE,
        DeviceMap_IIE,
        Badge_IIE,
        true
    },
        {
        "Apple IIe Enhanced",
        PLATFORM_APPLE_IIE_ENHANCED,
        DeviceMap_IIE,
        Badge_IIEEnh,
        true
    },
/*     {
        "Apple IIc",
        PLATFORM_APPLE_IIC,
        DeviceMap_IIC,
        true
    }, */
    // Add more built-in configurations as needed
};

SystemConfig_t *get_system_config(int index) {
    return &BuiltinSystemConfigs[index];
}

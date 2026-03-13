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

#include "devices.hpp"

#include "devices/keyboard/keyboard.hpp"
#include "devices/speaker/speaker.hpp"
#include "display/display.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "devices/prodos_clock/prodos_clock.hpp"
#include "devices/diskii/ndiskii.hpp"
#include "devices/memoryexpansion/memexp.hpp"
#include "devices/thunderclock_plus/thunderclockplus.hpp"
#include "devices/pdblock2/pdblock2.hpp"
#include "devices/pdblock3/pdblock3.hpp"
#include "devices/parallel/parallel.hpp"
#include "devices/videx/videx.hpp"
#include "devices/mockingboard/mb.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "devices/applemouseii/mouse.hpp"
#include "devices/cassette/cassette.hpp"
#include "devices/vidhd/vidhd.hpp"
#include "devices/rtc/rtc_pram.hpp"
#include "devices/adb/keygloo.hpp"
#include "devices/es5503/soundglu.hpp"
#include "devices/scc8530/scc8530.hpp"
#include "devices/iwm/iwm_device.hpp"

#include "PlatformIDs.hpp"

Device_t NoDevice = {
        DEVICE_ID_END,
        "No Device",
        false,
        0,
        PLATFLAG_NONE,
        NULL,
        NULL
    };

Device_t Devices[NUM_DEVICE_IDS] = {
    {
        DEVICE_ID_KEYBOARD_IIPLUS,
        "Apple II Plus Keyboard",
        false,
        0,
        PLATFLAG_APPLE_II_PLUS,
        init_mb_iiplus_keyboard,
        NULL
    },
    {
        DEVICE_ID_KEYBOARD_IIE,
        "Apple IIe Keyboard",
        false,
        0,
        PLATFLAG_APPLE_IIE|PLATFLAG_APPLE_IIE_ENHANCED|PLATFLAG_APPLE_IIE_65816,
        init_mb_iie_keyboard,
        NULL
    },
    {
        DEVICE_ID_SPEAKER,
        "Speaker",
        false,
        0,
        PLATFLAG_ALL,
        init_mb_speaker,
        NULL
    },
    {
        DEVICE_ID_DISPLAY,
        "Display",
        false,
        0,
        PLATFLAG_ALL,
        init_mb_device_display,
        NULL
    },
    {
        DEVICE_ID_GAMECONTROLLER,
        "Game Controller",  
        false,
        0,
        PLATFLAG_ALL,
        init_mb_game_controller,
        NULL
    },
    {
        DEVICE_ID_LANGUAGE_CARD,
        "II/II+ Language Card",
        false,
        0b00000001, // only slot 0
        PLATFLAG_APPLE_II|PLATFLAG_APPLE_II_PLUS,
        init_slot_languagecard,
        NULL
    },
    {
        DEVICE_ID_PRODOS_BLOCK,
        "Generic ProDOS Block",
        true,
        0b11111110,
        PLATFLAG_ALL,
        NULL, //init_prodos_block,
        NULL
    },
    {
        DEVICE_ID_PRODOS_CLOCK,
        "Generic ProDOS Clock",
        false,
        0b11111110,
        PLATFLAG_APPLE_II|PLATFLAG_APPLE_II_PLUS|PLATFLAG_APPLE_IIE|PLATFLAG_APPLE_IIE_ENHANCED|PLATFLAG_APPLE_IIE_65816,
        init_slot_prodosclock,
        NULL
    },
    {
        DEVICE_ID_DISK_II,
        "Disk II Controller",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_slot_ndiskII,
        NULL
    },
    {
        DEVICE_ID_MEM_EXPANSION,
        "Memory Expansion (Slinky)",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_slot_memexp,
        NULL
    },
    {
        DEVICE_ID_THUNDER_CLOCK,
        "Thunder Clock Plus",
        false,
        0b11111110,
        PLATFLAG_APPLE_II|PLATFLAG_APPLE_II_PLUS|PLATFLAG_APPLE_IIE|PLATFLAG_APPLE_IIE_ENHANCED|PLATFLAG_APPLE_IIE_65816,
        init_slot_thunderclock,
        NULL
    },
    {
        DEVICE_ID_PD_BLOCK2,
        "Generic ProDOS Block 2",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_pdblock2,
        NULL
    },
    {
        DEVICE_ID_PARALLEL,
        "Apple II Parallel Interface",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_slot_parallel,
        NULL
    },
    {
        DEVICE_ID_VIDEX,
        "Videx VideoTerm",
        false,
        0b00001000,
        PLATFLAG_APPLE_II|PLATFLAG_APPLE_II_PLUS,
        init_slot_videx,
        NULL
    },
    {
        DEVICE_ID_MOCKINGBOARD,
        "Mockingboard",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_slot_mockingboard,
        NULL
    },
    {
        DEVICE_ID_IIE_MEMORY,
        "IIe Memory",
        false,
        0,
        PLATFLAG_APPLE_IIE|PLATFLAG_APPLE_IIE_ENHANCED|PLATFLAG_APPLE_IIE_65816,
        init_iiememory,
        NULL
    },
    {
        DEVICE_ID_MOUSE,
        "Apple Mouse II",
        false,
        0b11111110,
        PLATFLAG_APPLE_II|PLATFLAG_APPLE_II_PLUS|PLATFLAG_APPLE_IIE|PLATFLAG_APPLE_IIE_ENHANCED|PLATFLAG_APPLE_IIE_65816,
        init_mouse,
        NULL
    },
    {
        DEVICE_ID_CASSETTE,
        "Cassette",
        false,
        0,
        PLATFLAG_ALL,
        init_mb_cassette,
        NULL
    },
    {
        DEVICE_ID_VIDHD,
        "VIDHD",
        false,
        0b11111110,
        PLATFLAG_APPLE_IIE_65816,
        init_slot_vidhd,
        NULL
    },
    {
        DEVICE_ID_RTC_PRAM,
        "RTC (Clock + Battery RAM)",
        false,
        0,
        PLATFLAG_APPLE_IIGS,
        init_slot_rtc_pram,
        NULL
    },
    {
        DEVICE_ID_KEYGLOO,
        "ADB (KeyGloo)",
        false,
        0,
        PLATFLAG_APPLE_IIGS,
        init_slot_keygloo,
        NULL
    },
    {
        DEVICE_ID_ENSONIQ,
        "Ensoniq",
        false,
        0,
        PLATFLAG_APPLE_IIGS,
        init_ensoniq_slot,
        NULL
    },
    {
        DEVICE_ID_SCC8530,
        "SCC8530",
        false,
        0,
        PLATFLAG_APPLE_IIGS,
        init_scc8530_slot,
        NULL
    },
    {
        DEVICE_ID_IWM,
        "IWM",
        false,
        0,
        PLATFLAG_APPLE_IIGS,
        init_iwm_slot,
        NULL
    },
    {
        DEVICE_ID_PD_BLOCK3,
        "Generic ProDOS Block 3",
        true,
        0b11111110,
        PLATFLAG_ALL,
        init_pdblock3,
        NULL
    },
};

Device_t *get_device(device_id id) {
    return &Devices[id-1];
}

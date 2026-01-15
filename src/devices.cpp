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

#include "devices.hpp"

#include "devices/keyboard/keyboard.hpp"
#include "devices/speaker/speaker.hpp"
#include "display/display.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "devices/prodos_clock/prodos_clock.hpp"
#include "devices/diskii/diskii.hpp"
#include "devices/memoryexpansion/memexp.hpp"
#include "devices/thunderclock_plus/thunderclockplus.hpp"
#include "devices/pdblock2/pdblock2.hpp"
#include "devices/parallel/parallel.hpp"
#include "devices/videx/videx.hpp"
#include "devices/mockingboard/mb.hpp"
#include "devices/annunciator/annunciator.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "devices/applemouseii/mouse.hpp"
#include "devices/cassette/cassette.hpp"
#include "devices/vidhd/vidhd.hpp"
#include "devices/rtc/rtc_pram.hpp"
#include "devices/adb/KeyGloo.hpp"
#include "devices/iigsmemory/iigsmemory.hpp"

Device_t NoDevice = {
        DEVICE_ID_END,
        "No Device",
        false,
        NULL,
        NULL
    };

Device_t Devices[NUM_DEVICE_IDS] = {
    {
        DEVICE_ID_KEYBOARD_IIPLUS,
        "Apple II Plus Keyboard",
        false,
        init_mb_iiplus_keyboard,
        NULL
    },
    {
        DEVICE_ID_KEYBOARD_IIE,
        "Apple IIe Keyboard",
        false,
        init_mb_iie_keyboard,
        NULL
    },
    {
        DEVICE_ID_SPEAKER,
        "Speaker",
        false,
        init_mb_speaker,
        NULL
    },
    {
        DEVICE_ID_DISPLAY,
        "Display",
        false,
        init_mb_device_display,
        NULL
    },
    {
        DEVICE_ID_GAMECONTROLLER,
        "Game Controller",  
        false,
        init_mb_game_controller,
        NULL
    },
    {
        DEVICE_ID_LANGUAGE_CARD,
        "II/II+ Language Card",
        false,
        init_slot_languagecard,
        NULL
    },
    {
        DEVICE_ID_PRODOS_BLOCK,
        "Generic ProDOS Block",
        true,
        NULL, //init_prodos_block,
        NULL
    },
    {
        DEVICE_ID_PRODOS_CLOCK,
        "Generic ProDOS Clock",
        false,
        init_slot_prodosclock,
        NULL
    },
    {
        DEVICE_ID_DISK_II,
        "Disk II Controller",
        true,
        init_slot_diskII,
        NULL
    },
    {
        DEVICE_ID_MEM_EXPANSION,
        "Memory Expansion (Slinky)",
        true,
        init_slot_memexp,
        NULL
    },
    {
        DEVICE_ID_THUNDER_CLOCK,
        "Thunder Clock Plus",
        false,
        init_slot_thunderclock,
        NULL
    },
    {
        DEVICE_ID_PD_BLOCK2,
        "Generic ProDOS Block 2",
        true,
        init_pdblock2,
        NULL
    },
    {
        DEVICE_ID_PARALLEL,
        "Apple II Parallel Interface",
        true,
        init_slot_parallel,
        NULL
    },
    {
        DEVICE_ID_VIDEX,
        "Videx VideoTerm",
        false,
        init_slot_videx,
        NULL
    },
    {
        DEVICE_ID_MOCKINGBOARD,
        "Mockingboard",
        true,
        init_slot_mockingboard,
        NULL
    },
    {
        DEVICE_ID_ANNUNCIATOR,
        "Annunciator",
        false,
        init_annunciator,
        NULL
    },
    {
        DEVICE_ID_IIE_MEMORY,
        "IIe Memory",
        false,
        init_iiememory,
        NULL
    },
    {
        DEVICE_ID_MOUSE,
        "Apple Mouse II",
        false,
        init_mouse,
        NULL
    },
    {
        DEVICE_ID_CASSETTE,
        "Cassette",
        false,
        init_mb_cassette,
        NULL
    },
    {
        DEVICE_ID_VIDHD,
        "VIDHD",
        false,
        init_slot_vidhd,
        NULL
    },
    {
        DEVICE_ID_RTC_PRAM,
        "RTC (Clock + Battery RAM)",
        false,
        init_slot_rtc_pram,
        NULL
    },
    {
        DEVICE_ID_KEYGLOO,
        "ADB (KeyGloo)",
        false,
        init_slot_keygloo,
        NULL
    },
    {
        DEVICE_ID_IIGS_MEMORY,
        "IIGS Memory",
        false,
        init_iigsmemory,
        NULL
    }
};

Device_t *get_device(device_id id) {
    return &Devices[id-1];
}

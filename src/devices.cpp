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
#include "device_info.hpp"

#include "devices/keyboard/keyboard.hpp"
#include "devices/speaker/speaker.hpp"
#include "display/display.hpp"
#include "devices/game/gamecontroller.hpp"
#include "devices/languagecard/languagecard.hpp"
#include "devices/prodos_clock/prodos_clock.hpp"
#include "devices/diskii/ndiskii_woz.hpp"
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
#include "devices/secondsight/secondsight.hpp"

namespace {

Device_t make_device(device_id id,
                     void (*power_on)(computer_t *, SlotType_t),
                     void (*power_off)(void *)) {
    const DeviceInfo_t *info = get_device_info(id);
    return {
        info->id,
        info->name,
        info->multipleInstances,
        info->slots_allowed,
        info->platform_flags,
        power_on,
        power_off,
    };
}

} // namespace

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
    make_device(DEVICE_ID_KEYBOARD_IIPLUS, init_mb_iiplus_keyboard, NULL),
    make_device(DEVICE_ID_KEYBOARD_IIE, init_mb_iie_keyboard, NULL),
    make_device(DEVICE_ID_SPEAKER, init_mb_speaker, NULL),
    make_device(DEVICE_ID_DISPLAY, init_mb_device_display, NULL),
    make_device(DEVICE_ID_GAMECONTROLLER, init_mb_game_controller, NULL),
    make_device(DEVICE_ID_LANGUAGE_CARD, init_slot_languagecard, NULL),
    make_device(DEVICE_ID_PRODOS_BLOCK, NULL, NULL),
    make_device(DEVICE_ID_PRODOS_CLOCK, init_slot_prodosclock, NULL),
    make_device(DEVICE_ID_DISK_II, init_slot_ndiskII_woz, NULL),
    make_device(DEVICE_ID_MEM_EXPANSION, init_slot_memexp, NULL),
    make_device(DEVICE_ID_THUNDER_CLOCK, init_slot_thunderclock, NULL),
    make_device(DEVICE_ID_PD_BLOCK2, init_pdblock2, NULL),
    make_device(DEVICE_ID_PARALLEL, init_slot_parallel, NULL),
    make_device(DEVICE_ID_VIDEX, init_slot_videx, NULL),
    make_device(DEVICE_ID_MOCKINGBOARD, init_slot_mockingboard, NULL),
    make_device(DEVICE_ID_IIE_MEMORY, init_iiememory, NULL),
    make_device(DEVICE_ID_MOUSE, init_mouse, NULL),
    make_device(DEVICE_ID_CASSETTE, init_mb_cassette, NULL),
    make_device(DEVICE_ID_VIDHD, init_slot_vidhd, NULL),
    make_device(DEVICE_ID_RTC_PRAM, init_slot_rtc_pram, NULL),
    make_device(DEVICE_ID_KEYGLOO, init_slot_keygloo, NULL),
    make_device(DEVICE_ID_ENSONIQ, init_ensoniq_slot, NULL),
    make_device(DEVICE_ID_SCC8530, init_scc8530_slot, NULL),
    make_device(DEVICE_ID_IWM, init_iwm_slot, NULL),
    make_device(DEVICE_ID_PD_BLOCK3, init_pdblock3, NULL),
    make_device(DEVICE_ID_SECOND_SIGHT, init_secondsight, NULL),
};

Device_t *get_device(device_id id) {
    return &Devices[id-1];
}

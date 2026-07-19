/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "device_info.hpp"

// Indexed as Devices[]: DeviceInfos[id - 1] for id in 1..NUM_DEVICE_IDS-1.
static const DeviceInfo_t DeviceInfos[NUM_DEVICE_IDS - 1] = {
    {DEVICE_ID_KEYBOARD_IIPLUS, "Apple II Plus Keyboard", false, 0, PLATFLAG_APPLE_II_PLUS},
    {DEVICE_ID_KEYBOARD_IIE, "Apple IIe Keyboard", false, 0,
        PLATFLAG_APPLE_IIE | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_SPEAKER, "Speaker", false, 0, PLATFLAG_ALL},
    {DEVICE_ID_DISPLAY, "Display", false, 0, PLATFLAG_ALL},
    {DEVICE_ID_GAMECONTROLLER, "Game Controller", false, 0, PLATFLAG_ALL},
    {DEVICE_ID_LANGUAGE_CARD, "II/II+ Language Card", false, 0b00000001,
        PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS},
    {DEVICE_ID_PRODOS_BLOCK, "Generic ProDOS Block", true, 0b11111110, PLATFLAG_NONE},
    {DEVICE_ID_PRODOS_CLOCK, "Generic ProDOS Clock", false, 0b11111110,
        PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_DISK_II, "Disk II Controller", true, 0b11111110, PLATFLAG_ALL},
    {DEVICE_ID_MEM_EXPANSION, "Memory Expansion (Slinky)", true, 0b11111110, PLATFLAG_ALL},
    {DEVICE_ID_THUNDER_CLOCK, "Thunder Clock Plus", false, 0b11111110,
        PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_PD_BLOCK2, "Generic ProDOS Block 2", true, 0b11111110, PLATFLAG_NONE},
    {DEVICE_ID_PARALLEL, "Apple II Parallel Interface", true, 0b11111110, PLATFLAG_ALL},
    {DEVICE_ID_VIDEX, "Videx VideoTerm", false, 0b00001000,
        PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS},
    {DEVICE_ID_MOCKINGBOARD, "Mockingboard", true, 0b11111110, PLATFLAG_ALL},
    {DEVICE_ID_IIE_MEMORY, "IIe Memory", false, 0,
        PLATFLAG_APPLE_IIE | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_MOUSE, "Apple Mouse II", false, 0b11111110,
        PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_CASSETTE, "Cassette", false, 0, PLATFLAG_ALL},
    {DEVICE_ID_VIDHD, "VIDHD", false, 0b11111110, PLATFLAG_APPLE_IIE_65816},
    {DEVICE_ID_RTC_PRAM, "RTC (Clock + Battery RAM)", false, 0, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_KEYGLOO, "ADB (KeyGloo)", false, 0, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_ENSONIQ, "Ensoniq", false, 0, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_SCC8530, "SCC8530", false, 0, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_IWM, "IWM", false, 0, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_PD_BLOCK3, "BazFast 3 (DMA Storage)", false, 0b11111110, PLATFLAG_ALL},
    {DEVICE_ID_SECOND_SIGHT, "Second Sight", false, 0b00001000, PLATFLAG_APPLE_IIGS},
    {DEVICE_ID_HOST_FST, "Host FST", false, 0, PLATFLAG_APPLE_IIGS},
};

const DeviceInfo_t *get_device_info(device_id id) {
    if (id <= DEVICE_ID_NONE || id >= NUM_DEVICE_IDS) {
        return nullptr;
    }
    return &DeviceInfos[id - 1];
}

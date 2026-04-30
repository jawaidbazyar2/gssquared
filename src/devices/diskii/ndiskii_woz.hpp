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

#pragma once

#include "computer.hpp"
#include "NClock.hpp"
#include "util/DebugFormatter.hpp"
#include "util/media.hpp"
#include "util/SoundEffectKeys.hpp"
#include "devices/diskii/Floppy525_woz.hpp"
#include "util/ResetController.hpp"

// Soft-switch offsets within the slot's I/O page (same as ndiskii.hpp)
#define DiskII_Ph0_Off       0x00
#define DiskII_Ph0_On        0x01
#define DiskII_Ph1_Off       0x02
#define DiskII_Ph1_On        0x03
#define DiskII_Ph2_Off       0x04
#define DiskII_Ph2_On        0x05
#define DiskII_Ph3_Off       0x06
#define DiskII_Ph3_On        0x07
#define DiskII_Motor_Off     0x08
#define DiskII_Motor_On      0x09
#define DiskII_Drive1_Select 0x0A
#define DiskII_Drive2_Select 0x0B
#define DiskII_Q6L           0x0C
#define DiskII_Q6H           0x0D
#define DiskII_Q7L           0x0E
#define DiskII_Q7H           0x0F

class DiskII_WOZ_Controller;

class ndiskII_woz_controller : public SlotData {
public:
    computer_t          *computer;
    NClockII            *clock;
    DiskII_WOZ_Controller *dc;
    ResetController     *reset_control;

    int powerup_reset_cycles = 6;
};

void init_slot_ndiskII_woz(computer_t *computer, SlotType_t slot);

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

#include "gs2.hpp"
#include "cpu.hpp"
#include "util/ResourceFile.hpp"
#include "SlotData.hpp"
#include "computer.hpp"

#define PRODOS_CLOCK_PV_TRIGGER 0x00

#define PRODOS_CLOCK_GETLN_TRIGGER 0xAE

struct prodos_clock_state: public SlotData {
    char buf[64];    
};

void init_slot_prodosclock(computer_t *computer, SlotType_t slot);

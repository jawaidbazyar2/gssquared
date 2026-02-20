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

/**
 *               READ     WRITE     Bank
 * C080 - 0000 -  RAM      None      2
 * C081 - 0001 -  ROM      RAM       2
 * C082 - 0010 -  ROM      None      2
 * C083 - 0011 -  RAM      RAM       2
 * C084 - C087 - same as above
 * 
 * C088 - 1000 -  RAM      None      1
 * C089 - 1001 -  ROM      RAM       1
 * C08A - 1010 -  ROM      None      1
 * C08B - 1011 -  RAM      RAM       1
 * C08C - C08F - same as above
 * 
 * Bits 0111 - Weirdness.
 * Bit 1000 - 1 = Bank 1; 0 = Bank 2
 * 

* C011 - Read (bit 7) - bank 2 active (1) or bank 1 active (0)
* C012 - Read (bit 7) - RAM (1) or ROM (0)


 * 
 * There are 3 toggles:
 * Read RAM vs Read ROM
 * Write RAM vs No Write
 * Bank 1 vs Bank 2
 * 
 */

#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "LanguageCardLogic.hpp"

/* #define LANG_A3             0b00001000
#define LANG_A0A1           0b00000011 */

#define LANG_UNUSED_BIT     0b00100

#define RAM_NONE            0b000
#define ROM_RAM             0b001
#define ROM_NONE            0b010
#define RAM_RAM             0b011

struct languagecard_state_t {
    MMU_II *mmu;  // we need to know II-plus specific stuff like restoring the ROM
    cpu_state *cpu;

    LanguageCardLogic ll;

   /*  uint32_t FF_BANK_1;
    uint32_t FF_READ_ENABLE;
    uint32_t FF_PRE_WRITE;
    uint32_t _FF_WRITE_ENABLE; */

    uint8_t *ram_bank;
};

void init_slot_languagecard(computer_t *computer, SlotType_t slot);
void reset_languagecard(cpu_state *cpu);

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
#include "util/ResourceFile.hpp"
#include "SlotData.hpp"
#include "computer.hpp"

#define MEMEXP_ADDR_LOW 0x0000
#define MEMEXP_ADDR_MED 0x0001
#define MEMEXP_ADDR_HIGH 0x0002
#define MEMEXP_DATA 0x0003
#define MEMEXP_BANK_SELECT 0x000F

#define MEMEXP_SIZE 1024*1024

typedef struct memexp_data: public SlotData {
    union {
        uint32_t addr;
        struct {
            uint8_t addr_low;
            uint8_t addr_med;
            uint8_t addr_high;
            uint8_t dummy;
        };
    };
    uint8_t *data;
    ResourceFile *rom;
    MMU_II *mmu;
} memexp_data;

void init_slot_memexp(computer_t *computer, SlotType_t slot);
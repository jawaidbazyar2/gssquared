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

#include <stdio.h>
#include "iigsmemory.hpp"
#include "Module_ID.hpp"


void init_iigsmemory(computer_t *computer, SlotType_t slot) {
    
    iigsmemory_state_t *gsm = new iigsmemory_state_t;
    gsm->computer = computer;

    gsm->mmu_iigs = (MMU_IIgs *)computer->cpu->mmu;

    computer->set_module_state(MODULE_IIGSMEMORY, gsm);
    
    /* C050 read/write */
  /*   for (uint16_t i = 0xC050; i <= 0xC057; i++) {
        computer->mmu->set_C0XX_write_handler(i, { iigsmemory_write_c0xx, gsm });
        computer->mmu->set_C0XX_read_handler(i, { iigsmemory_read_c0xx, gsm });
    } */

}

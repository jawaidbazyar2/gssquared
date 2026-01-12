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

#include <stdio.h>
#include "iigsmemory.hpp"
#include "Module_ID.hpp"

void iigsmemory_write_c0xx(void *context, uint32_t address, uint8_t value) {
    iigsmemory_state_t *gsm = (iigsmemory_state_t *)context;
    switch (address) {
        case 0xC050:
            gsm->c050_wh.write(gsm->c050_wh.context, address, value);
            gsm->mmu_iigs->set_text(false);
            break;
        case 0xC051:
            gsm->c051_wh.write(gsm->c051_wh.context, address, value);
            gsm->mmu_iigs->set_text(true);
            break;
        case 0xC052:
            gsm->c052_wh.write(gsm->c052_wh.context, address, value);
            gsm->mmu_iigs->set_mixed(false);
            break;
        case 0xC053:
            gsm->c053_wh.write(gsm->c053_wh.context, address, value);
            gsm->mmu_iigs->set_mixed(true);
            break;
        case 0xC054:
            gsm->c054_wh.write(gsm->c054_wh.context, address, value);
            gsm->mmu_iigs->set_page2(false);
            break;
        case 0xC055:
            gsm->c055_wh.write(gsm->c055_wh.context, address, value);
            gsm->mmu_iigs->set_page2(true);
            break;
        case 0xC056:
            gsm->c056_wh.write(gsm->c056_wh.context, address, value);
            gsm->mmu_iigs->set_hires(false);
            break;
        case 0xC057:
            gsm->c057_wh.write(gsm->c057_wh.context, address, value);
            gsm->mmu_iigs->set_hires(true);
            break;
        default:
            // Do nothing for other addresses
            break;
    }
}

uint8_t iigsmemory_read_c0xx(void *context, uint32_t address) {
    iigsmemory_state_t *gsm = (iigsmemory_state_t *)context;
    switch (address) {
        case 0xC050:
            gsm->mmu_iigs->set_text(false);
            return gsm->c050_rh.read(gsm->c050_rh.context, address);
        case 0xC051:
            gsm->mmu_iigs->set_text(true);
            return gsm->c051_rh.read(gsm->c051_rh.context, address);
        case 0xC052:
            gsm->mmu_iigs->set_mixed(false);
            return gsm->c052_rh.read(gsm->c052_rh.context, address);
        case 0xC053:
            gsm->mmu_iigs->set_mixed(true);
            return gsm->c053_rh.read(gsm->c053_rh.context, address);
        case 0xC054:
            gsm->mmu_iigs->set_page2(false);
            return gsm->c054_rh.read(gsm->c054_rh.context, address);
        case 0xC055:
            gsm->mmu_iigs->set_page2(true);
            return gsm->c055_rh.read(gsm->c055_rh.context, address);
        case 0xC056:
            gsm->mmu_iigs->set_hires(false);
            return gsm->c056_rh.read(gsm->c056_rh.context, address);
        case 0xC057:
            gsm->mmu_iigs->set_hires(true);
            return gsm->c057_rh.read(gsm->c057_rh.context, address);
        default:
            return 0xEE;
    }
}


void init_iigsmemory(computer_t *computer, SlotType_t slot) {
    
    iigsmemory_state_t *gsm = new iigsmemory_state_t;
    gsm->computer = computer;

    gsm->mmu_iigs = (MMU_IIgs *)computer->cpu->mmu;

    gsm->display_state = (display_state_t *)computer->get_module_state(MODULE_DISPLAY);

    computer->set_module_state(MODULE_IIGSMEMORY, gsm);
    
    /* C050 read/write */
    computer->mmu->get_C0XX_write_handler(0xC050, gsm->c050_wh);
    computer->mmu->get_C0XX_read_handler(0xC050, gsm->c050_rh);

    computer->mmu->set_C0XX_write_handler(0xC050, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC050, { iigsmemory_read_c0xx, gsm });

    /* C051 read/write */
    computer->mmu->get_C0XX_write_handler(0xC051, gsm->c051_wh);
    computer->mmu->get_C0XX_read_handler(0xC051, gsm->c051_rh);

    computer->mmu->set_C0XX_write_handler(0xC051, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC051, { iigsmemory_read_c0xx, gsm });

    /* C052 read/write */
    computer->mmu->get_C0XX_write_handler(0xC052, gsm->c052_wh);
    computer->mmu->get_C0XX_read_handler(0xC052, gsm->c052_rh);

    computer->mmu->set_C0XX_write_handler(0xC052, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC052, { iigsmemory_read_c0xx, gsm });

    /* C053 read/write */
    computer->mmu->get_C0XX_write_handler(0xC053, gsm->c053_wh);
    computer->mmu->get_C0XX_read_handler(0xC053, gsm->c053_rh);

    computer->mmu->set_C0XX_write_handler(0xC053, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC053, { iigsmemory_read_c0xx, gsm });

    /* C054 read/write */
    computer->mmu->get_C0XX_write_handler(0xC054, gsm->c054_wh);
    computer->mmu->get_C0XX_read_handler(0xC054, gsm->c054_rh);

    computer->mmu->set_C0XX_write_handler(0xC054, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC054, { iigsmemory_read_c0xx, gsm });

    /* C055 read/write */
    computer->mmu->get_C0XX_write_handler(0xC055, gsm->c055_wh);
    computer->mmu->get_C0XX_read_handler(0xC055, gsm->c055_rh);

    computer->mmu->set_C0XX_write_handler(0xC055, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC055, { iigsmemory_read_c0xx, gsm });

    /* C056 read/write */
    computer->mmu->get_C0XX_write_handler(0xC056, gsm->c056_wh);
    computer->mmu->get_C0XX_read_handler(0xC056, gsm->c056_rh);

    computer->mmu->set_C0XX_write_handler(0xC056, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC056, { iigsmemory_read_c0xx, gsm });

    /* C057 read/write */
    computer->mmu->get_C0XX_write_handler(0xC057, gsm->c057_wh);
    computer->mmu->get_C0XX_read_handler(0xC057, gsm->c057_rh);

    computer->mmu->set_C0XX_write_handler(0xC057, { iigsmemory_write_c0xx, gsm });
    computer->mmu->set_C0XX_read_handler(0xC057, { iigsmemory_read_c0xx, gsm });
}

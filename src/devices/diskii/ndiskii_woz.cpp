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

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "util/strndup.h"
#include "cpu.hpp"
#include "ndiskii_woz.hpp"
#include "util/Event.hpp"
#include "util/media.hpp"
#include "util/ResourceFile.hpp"
#include "debug.hpp"
#include "util/mount.hpp"
#include "util/SoundEffectKeys.hpp"
#include "util/SoundEffect.hpp"
#include "util/printf_helper.hpp"
#include "util/ResetController.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "devices/diskii/diskii_controller.hpp"

uint8_t ndiskII_woz_read_C0xx(void *context, uint32_t address) {
    ndiskII_woz_controller *d = (ndiskII_woz_controller *)context;
    return d->dc->read_cmd(address);
}

void ndiskII_woz_write_C0xx(void *context, uint32_t address, uint8_t data) {
    ndiskII_woz_controller *d = (ndiskII_woz_controller *)context;
    d->dc->write_cmd(address, data);
}

void ndiskii_woz_reset(ndiskII_woz_controller *d) {
    printf("diskii_woz_reset\n");
    d->dc->reset();
}


void init_slot_ndiskII_woz(computer_t *computer, SlotType_t slot) {

    ndiskII_woz_controller *diskII_d = new ndiskII_woz_controller();
    diskII_d->_slot      = slot;
    diskII_d->computer   = computer;
    diskII_d->clock      = computer->clock;

    diskII_d->id = DEVICE_ID_DISK_II;

    fprintf(stdout, "diskII_woz_register_slot %d\n", slot);

    ResourceFile *rom = new ResourceFile("roms/cards/diskii/diskii_firmware.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load diskii/diskii_firmware.rom\n");
        return;
    }
    rom->load();

    uint8_t *rom_data = (uint8_t *)(rom->get_data());

    uint16_t slot_base = 0xC080 + (slot * 0x10);

    for (uint16_t i = 0; i < 16; i++) {
        computer->mmu->set_C0XX_read_handler(slot_base + i,
            { ndiskII_woz_read_C0xx, diskII_d });
    }
    for (uint16_t i = 8; i < 16; i++) {
        computer->mmu->set_C0XX_write_handler(slot_base + i,
            { ndiskII_woz_write_C0xx, diskII_d });
    }

    computer->mmu->set_slot_rom(slot, rom_data, "DISK2_ROM");

    DiskII_WOZ_Controller *dc = new DiskII_WOZ_Controller(
        computer->sound_effect, computer->clock, computer->cpu_event_timer);
    diskII_d->dc = dc;

    storage_key_t key;
    key.slot  = slot;
    key.drive = 0;
    computer->mounts->register_storage_device(key, dc, DRIVE_TYPE_DISKII);
    key.drive = 1;
    computer->mounts->register_storage_device(key, dc, DRIVE_TYPE_DISKII);

    diskII_d->powerup_reset_cycles = 6;
    diskII_d->reset_control        = computer->reset_control;
    diskII_d->reset_control->assert_reset((device_reset_id)(slot), true);

    computer->register_reset_handler(
        [diskII_d]() {
            ndiskii_woz_reset(diskII_d);
            return true;
        });

    computer->device_frame_dispatcher->registerHandler(
        [diskII_d]() {
            // we call the disk controller class frame update because it owns its own sound effects.
            // but we tell it whether to play sound effects or not.
            diskII_d->dc->frameUpdate(diskII_d->computer->execution_mode == EXEC_NORMAL);

            if (diskII_d->powerup_reset_cycles > 0) {
                diskII_d->powerup_reset_cycles--;
                if (diskII_d->powerup_reset_cycles == 0) {
                    diskII_d->reset_control->assert_reset(
                        (device_reset_id)(diskII_d->_slot), false);
                }
            }
            return true;
        });

    computer->register_debug_display_handler(
        "diskii_woz",
        DH_DISKII,
        [diskII_d]() -> DebugFormatter * {
            return diskII_d->dc->debug();
        }
    );
}

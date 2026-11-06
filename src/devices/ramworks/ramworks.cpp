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

#include "ramworks.hpp"

#include "mmus/mmu_iie2.hpp"

/**
 * Applied Engineering RamWorks, fixed at 8MB (AUX 0..127).
 * Slot 0 is the config stand-in for the IIe auxiliary slot.
 * $C073 write selects the bank. $C071 read returns it.
 * The game controller already owns $C070–$C07F, so these handlers
 * sit in the second slot. The $C071 read replaces the floating bus.
 * Reset clears the bank in MMU_IIe2::reset.
 */

static uint8_t ramworks_read_C071(void *context, uint32_t address) {
    (void)address;
    computer_t *computer = static_cast<computer_t *>(context);
    return static_cast<MMU_IIe2 *>(computer->mmu)->get_aux_bank();
}

static void ramworks_write_C073(void *context, uint32_t address, uint8_t value) {
    (void)address;
    computer_t *computer = static_cast<computer_t *>(context);
    static_cast<MMU_IIe2 *>(computer->mmu)->set_aux_bank(value);
}

void init_slot_ramworks(computer_t *computer, SlotType_t slot) {
    (void)slot;
    MMU_IIe2 *mmu = static_cast<MMU_IIe2 *>(computer->mmu);
    mmu->install_ramworks();

    mmu->set_C0XX_read_handler(0xC071, { ramworks_read_C071, computer });
    mmu->set_C0XX_read_replaces(0xC071);
    mmu->set_C0XX_write_handler(0xC073, { ramworks_write_C073, computer });
}

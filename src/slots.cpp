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

#include <cassert>

#include "gs2.hpp"
#include "slots.hpp"


SlotManager_t::SlotManager_t() {
    for (int i = SLOT_0; i < NUM_SLOTS; i++) {
        Slots[static_cast<SlotType_t>(i)].slot_number = static_cast<SlotType_t>(i);
        Slots[static_cast<SlotType_t>(i)].card = NULL;
    }
}

SlotManager_t::~SlotManager_t() {
    for (int i = SLOT_0; i < NUM_SLOTS; i++) {
        if (Slots[static_cast<SlotType_t>(i)].card != NULL) {
            unregister_slot(static_cast<SlotType_t>(i));
        }
    }
}

/**
 * Register a device in a slot.
 * @param device The device to register.
 * @param slot_number The slot number to register the device in.
 */
void SlotManager_t::register_slot(Device_t *device, SlotType_t slot_number) {
    assert(slot_number < NUM_SLOTS && "Slot number out of range");
    Slots[slot_number].card = device;
}

/**
 * Unregister a device from a slot.
 * @param slot_number The slot number to unregister the device from.
 */
void SlotManager_t::unregister_slot(SlotType_t slot_number) {
    assert(slot_number < NUM_SLOTS && "Slot number out of range");
    Slots[slot_number].card = NULL;
}

/**
 * Get the device in a slot.
 * @param slot_number The slot number to get the device from.
 * @return The device in the slot.
 */
Device_t *SlotManager_t::get_device(SlotType_t slot_number) {  
    assert(slot_number < NUM_SLOTS && "Slot number out of range");

    if (Slots[slot_number].card == NULL) {
        return &NoDevice;
    }
    return Slots[slot_number].card;
}

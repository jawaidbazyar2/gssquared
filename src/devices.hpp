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
#include "computer.hpp"

#include "Device_ID.hpp"

struct DeviceMap_t {
    device_id id;
    SlotType_t slot;
};

struct Device_t {
    device_id id;
    const char *name;
    bool multipleInstances = false;
    void (*power_on)(computer_t *computer, SlotType_t slot_number);
    void (*power_off)(void *context);
};

Device_t *get_device(device_id id);

extern Device_t NoDevice;

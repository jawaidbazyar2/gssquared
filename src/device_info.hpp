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

#include <cstdint>

#include "Device_ID.hpp"
#include "PlatformIDs.hpp"

/**
 * Placement / identity metadata for a device.
 * slots_allowed == 0 means motherboard / non-slot (not assignable via [[cards]]).
 * Otherwise bit N set means slot N is allowed.
 */
struct DeviceInfo_t {
    device_id id;
    const char *name;
    bool multipleInstances;
    uint8_t slots_allowed;
    PlatformFlags_t platform_flags;
};

/** Lookup by device_id. Returns nullptr for DEVICE_ID_NONE / out of range. */
const DeviceInfo_t *get_device_info(device_id id);

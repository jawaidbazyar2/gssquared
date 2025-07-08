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

#pragma once

#include "gs2.hpp"
#include "platforms.hpp"
#include "devices.hpp"
/**
 * a System Configuraiton is a platform, and, a list of devices and their slots.
 */

struct SystemConfig_t {
    const char *name;
    PlatformId_t platform_id;
    DeviceMap_t *device_map;
    int image_id;
    bool builtin;
    const char *description;
};

extern SystemConfig_t BuiltinSystemConfigs[];
extern const int NUM_SYSTEM_CONFIGS;

SystemConfig_t *get_system_config(int index);

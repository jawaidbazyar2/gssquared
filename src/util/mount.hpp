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

#include <string>
#include <vector>
#include <unordered_map>

#include "cpu.hpp"
#include "media.hpp"
#include "slots.hpp"
#include "drive_status.hpp"
#include "StorageDevice.hpp"

typedef struct {
    uint16_t slot;
    uint16_t drive;
    std::string filename;
    media_descriptor *media;
} disk_mount_t;

enum drive_type_t {
    DRIVE_TYPE_DISKII,
    DRIVE_TYPE_PRODOS_BLOCK,
    DRIVE_TYPE_APPLEDISK_525,
};

struct drive_media_t {
    uint64_t key;
    drive_type_t drive_type;
    media_descriptor *media;
};

struct drive_info_t {
    storage_key_t key;              // slot/drive identifier
    drive_type_t drive_type;   // for selecting button asset
    drive_status_t status;     // motor, track, mounted, etc.
};

enum unmount_action_t {
    UNMOUNT_ACTION_NONE,
    SAVE_AND_UNMOUNT,
    DISCARD
};

class Mounts {

    struct storage_device_registration_t {
        StorageDevice *device;
        drive_type_t drive_type;
    };   

    protected:
    SlotManager_t *slot_manager;
    std::unordered_map<storage_key_t, storage_device_registration_t> storage_devices;
    std::unordered_map<storage_key_t, media_descriptor*> mounted_media;
    mutable std::vector<drive_info_t> cached_drive_info;

public:
    Mounts(SlotManager_t *slot_managerx) : slot_manager(slot_managerx) {}
    bool mount_media(disk_mount_t disk_mount);
    bool unmount_media(storage_key_t key, unmount_action_t action);
    drive_status_t media_status(storage_key_t key);
    const std::vector<drive_info_t>& get_all_drives();
    //int register_drive(drive_type_t drive_type, uint64_t key);
    int register_storage_device(storage_key_t key, StorageDevice *storage_device, drive_type_t drive_type);
    void dump();
};

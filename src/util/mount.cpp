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

#include <unordered_map>
#include <iostream>
#include <algorithm>

#include "media.hpp"
#include "mount.hpp"

#include "util/printf_helper.hpp"

// this and umount should work on the basis of a disk device registering callbacks for mount, unmount, status, whatever else.

bool Mounts::mount_media(disk_mount_t disk_mount) {
    //uint64_t key = (disk_mount.slot << 8) | disk_mount.drive;
    storage_key_t key;
    key.slot = disk_mount.slot;
    key.drive = disk_mount.drive;
    key.partition = 0;
    key.subunit = 0;
    
    auto it = storage_devices.find(key);
    if (it == storage_devices.end()) {
        std::cerr << "No drive registered at " << key << std::endl;
        return false;
    }
    
    // Identify media
    media_descriptor *media = new media_descriptor();
    media->filename = disk_mount.filename;
    if (identify_media(*media) != 0) {
        delete media;
        return false;
    }
    
    // Call drive's mount method - polymorphic!
    if (!it->second.device->mount(key, media)) {
        delete media;
        return false;
    }
    
    mounted_media[key] = media;
    return true;
}

bool Mounts::unmount_media(storage_key_t key, unmount_action_t action) {
    auto it = storage_devices.find(key);
    if (it == storage_devices.end()) {
        return false;
    }
    
    if (action == SAVE_AND_UNMOUNT) {
        it->second.device->writeback(key);
    }
    
    it->second.device->unmount(key);
    
    // Clean up media descriptor
    auto media_it = mounted_media.find(key);
    if (media_it != mounted_media.end()) {
        delete media_it->second;
        mounted_media.erase(media_it);
    }
    
    return true;
}

drive_status_t Mounts::media_status(storage_key_t key) {
    auto it = storage_devices.find(key);
    if (it == storage_devices.end()) {
        return {false, nullptr, false, 0, false};
    }
    return it->second.device->status(key);
}


void Mounts::dump() {
    for (auto it = mounted_media.begin(); it != mounted_media.end(); it++) {
        drive_status_t status = media_status(it->first);
        //fprintf(stdout, "Mounted media: %llu typ: %d mnt: %d mot:%d pos: %d\n", u64_t(it->first), it->second.drive_type, status.is_mounted, status.motor_on, status.position);
        fprintf(stdout, "Mounted media: %llu typ: %d mnt: %d mot:%d pos: %d\n", u64_t(it->first), 0xEE, /* it->second.drive_type, */ status.is_mounted, status.motor_on, status.position);
    }
}

int Mounts::register_storage_device(storage_key_t key, StorageDevice *storage_device, drive_type_t drive_type) {
    storage_devices[key] = {storage_device, drive_type};
    return 0;
}

const std::vector<drive_info_t>& Mounts::get_all_drives() {
    cached_drive_info.clear();  // doesn't deallocate capacity
    cached_drive_info.reserve(storage_devices.size());
    
    for (const auto& [key, registration] : storage_devices) {
        drive_info_t info;
        info.key = key;
        info.drive_type = registration.drive_type;
        info.status = registration.device->status(key);
        
        cached_drive_info.push_back(info);
    }
    
    // Sort by key: primary sort by slot (high byte) descending, secondary by drive (low byte) ascending
    std::sort(cached_drive_info.begin(), cached_drive_info.end(),
              [](const drive_info_t& a, const drive_info_t& b) {
                  uint8_t slot_a = a.key.slot;
                  uint8_t slot_b = b.key.slot;
                  uint8_t drive_a = a.key.drive;
                  uint8_t drive_b = b.key.drive;
                  
                  if (slot_a != slot_b) {
                      return slot_a > slot_b;  // Higher slots first
                  }
                  return drive_a < drive_b;  // Lower drive numbers first within slot
              });
    
    return cached_drive_info;
}
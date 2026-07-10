/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "util/ConfigDraft.hpp"

#include <algorithm>
#include <filesystem>

#include "devices/displaypp/VideoScanner.hpp"
#include "util/SystemConfig.hpp"
#include "util/uuid.hpp"

namespace {

video_scanner_t draft_derive_scanner(PlatformId_t platform, clock_set_t clock_set) {
    switch (platform) {
        case PLATFORM_APPLE_II:
        case PLATFORM_APPLE_II_PLUS:
            return Scanner_AppleII;
        case PLATFORM_APPLE_IIGS:
            return Scanner_AppleIIgs;
        case PLATFORM_APPLE_IIE:
        case PLATFORM_APPLE_IIE_ENHANCED:
        case PLATFORM_APPLE_IIE_65816:
            return (clock_set == CLOCK_SET_PAL) ? Scanner_AppleIIePAL : Scanner_AppleIIe;
        default:
            return Scanner_AppleII;
    }
}

void push_drive(std::vector<drive_spec_t>& out, uint16_t slot, uint16_t drive, drive_type_t type) {
    drive_spec_t spec{};
    spec.key.slot = slot;
    spec.key.drive = drive;
    spec.key.partition = 0;
    spec.key.subunit = 0;
    spec.drive_type = type;
    out.push_back(spec);
}

bool device_allowed_on_platform(device_id id, PlatformId_t platform) {
    auto choices = cards_allowed_for_slot(platform, 0);
    // Check all slots — some cards are slot-restricted.
    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        for (const auto& c : cards_allowed_for_slot(platform, slot)) {
            if (c.id == id) return true;
        }
    }
    (void)choices;
    return false;
}

} // namespace

std::string storage_display_name(const std::string& path) {
    if (path.empty()) return {};
    return std::filesystem::path(path).filename().string();
}

std::vector<drive_spec_t> derive_drives_from_config(PlatformId_t platform_id,
                                                    const device_id slot_devices[NUM_SLOTS]) {
    std::vector<drive_spec_t> out;

    if (platform_id == PLATFORM_APPLE_IIGS) {
        push_drive(out, 6, 0, DRIVE_TYPE_APPLEDISK_525);
        push_drive(out, 6, 1, DRIVE_TYPE_APPLEDISK_525);
        push_drive(out, 5, 0, DRIVE_TYPE_APPLEDISK_35);
        push_drive(out, 5, 1, DRIVE_TYPE_APPLEDISK_35);
    }

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = slot_devices[slot];
        switch (id) {
            case DEVICE_ID_DISK_II:
                push_drive(out, static_cast<uint16_t>(slot), 0, DRIVE_TYPE_DISKII);
                push_drive(out, static_cast<uint16_t>(slot), 1, DRIVE_TYPE_DISKII);
                break;
            case DEVICE_ID_PD_BLOCK3:
                for (uint16_t d = 0; d < 6; ++d) {
                    push_drive(out, static_cast<uint16_t>(slot), d, DRIVE_TYPE_PRODOS_BLOCK);
                }
                break;
            case DEVICE_ID_PD_BLOCK2:
            case DEVICE_ID_PRODOS_BLOCK:
                push_drive(out, static_cast<uint16_t>(slot), 0, DRIVE_TYPE_PRODOS_BLOCK);
                push_drive(out, static_cast<uint16_t>(slot), 1, DRIVE_TYPE_PRODOS_BLOCK);
                break;
            default:
                break;
        }
    }

    std::sort(out.begin(), out.end(), [](const drive_spec_t& a, const drive_spec_t& b) {
        if (a.key.slot != b.key.slot) return a.key.slot > b.key.slot;
        return a.key.drive < b.key.drive;
    });
    return out;
}

void ConfigDraft::sync_pointers() {
    config_.name = name_.c_str();
    config_.description = description_.c_str();
    config_.id = id_.c_str();
}

ConfigDraft::ConfigDraft() {
    reset_for_platform(PLATFORM_APPLE_IIE_ENHANCED);
}

void ConfigDraft::reset_for_platform(PlatformId_t platform_id) {
    path_.clear();
    mounts_.clear();
    config_ = {};
    for (int i = 0; i < NUM_SLOTS; ++i) {
        config_.slot_devices[i] = DEVICE_ID_NONE;
    }
    config_.platform_id = platform_id;
    config_.builtin = false;
    config_.clock_set = CLOCK_SET_US;
    config_.scanner_type = draft_derive_scanner(platform_id, config_.clock_set);

    name_ = std::string("New ") + platform_name(platform_id);
    description_ = "Custom system configuration";
    id_ = generate_uuid_v4();

    if (platform_id != PLATFORM_APPLE_IIGS) {
        config_.slot_devices[6] = DEVICE_ID_DISK_II;
    }

    sync_pointers();
}

void ConfigDraft::load_from(const SystemConfig& config) {
    path_ = config.path();
    name_ = config.config().name ? config.config().name : "";
    description_ = config.config().description ? config.config().description : "";
    id_ = config.id();
    if (id_.empty()) {
        id_ = generate_uuid_v4();
    }
    config_ = config.config();
    mounts_ = config.mounts();
    sync_pointers();
}

void ConfigDraft::load_from_builtin(const SystemConfig_t& config) {
    path_.clear();
    name_ = config.name ? config.name : "";
    description_ = config.description ? config.description : "";
    id_ = generate_uuid_v4();
    config_ = config;
    config_.builtin = false;
    mounts_.clear();
    sync_pointers();
}

void ConfigDraft::set_name(const std::string& name) {
    name_ = name;
    sync_pointers();
}

void ConfigDraft::set_description(const std::string& description) {
    description_ = description;
    sync_pointers();
}

void ConfigDraft::set_path(const std::string& path) {
    path_ = path;
}

void ConfigDraft::set_id(const std::string& id) {
    id_ = id;
    sync_pointers();
}

void ConfigDraft::set_platform(PlatformId_t platform_id) {
    config_.platform_id = platform_id;
    if (platform_id == PLATFORM_APPLE_IIGS) {
        config_.clock_set = CLOCK_SET_US;
    }
    config_.scanner_type = draft_derive_scanner(platform_id, config_.clock_set);

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        device_id id = config_.slot_devices[slot];
        if (id == DEVICE_ID_NONE) continue;
        if (!device_allowed_on_platform(id, platform_id)) {
            config_.slot_devices[slot] = DEVICE_ID_NONE;
            clear_mounts_for_slot(slot);
        }
    }

    if (platform_id == PLATFORM_APPLE_IIGS && config_.slot_devices[6] == DEVICE_ID_DISK_II) {
        config_.slot_devices[6] = DEVICE_ID_NONE;
        clear_mounts_for_slot(6);
    }
}

void ConfigDraft::set_slot_device(int slot, device_id id) {
    if (slot < 0 || slot >= NUM_SLOTS) return;
    config_.slot_devices[slot] = id;
    clear_mounts_for_slot(slot);
}

void ConfigDraft::clear_mounts_for_slot(int slot) {
    mounts_.erase(std::remove_if(mounts_.begin(), mounts_.end(),
                                 [slot](const disk_mount_t& m) { return m.slot == slot; }),
                  mounts_.end());
}

void ConfigDraft::set_mount(uint16_t slot, uint16_t drive, const std::string& filename) {
    for (auto& m : mounts_) {
        if (m.slot == slot && m.drive == drive) {
            m.filename = filename;
            return;
        }
    }
    mounts_.push_back({slot, drive, filename});
}

void ConfigDraft::clear_mount(uint16_t slot, uint16_t drive) {
    mounts_.erase(std::remove_if(mounts_.begin(), mounts_.end(),
                                 [slot, drive](const disk_mount_t& m) {
                                     return m.slot == slot && m.drive == drive;
                                 }),
                  mounts_.end());
}

std::vector<drive_spec_t> ConfigDraft::drive_specs() const {
    std::vector<drive_spec_t> specs = derive_drives_from_config(config_.platform_id, config_.slot_devices);
    for (auto& spec : specs) {
        for (const auto& m : mounts_) {
            if (m.slot == spec.key.slot && m.drive == spec.key.drive) {
                spec.status.is_mounted = true;
                spec.status.filename = storage_display_name(m.filename);
                break;
            }
        }
    }
    return specs;
}

std::string ConfigDraft::slot_device_name(int slot) const {
    if (slot < 0 || slot >= NUM_SLOTS) return {};
    device_id id = config_.slot_devices[slot];
    if (id == DEVICE_ID_NONE) return {};
    for (const auto& c : cards_allowed_for_slot(config_.platform_id, slot)) {
        if (c.id == id) return c.display_name;
    }
    return card_type_name(id);
}

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "util/ConfigDraft.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "Device_ID.hpp"
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
        case PLATFORM_APPLE_IIGS_ROM3:
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

void push_port(std::vector<connection_port_spec_t>& out, int slot, const std::string& name,
               connection_port_kind_t kind) {
    connection_port_spec_t spec;
    spec.key = connection_key_t{slot, "a"};
    spec.display_name = name;
    spec.kind = kind;
    spec.device = connection_device_type_t::NONE;
    out.push_back(std::move(spec));
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

    if (platform_is_iigs(platform_id)) {
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

std::vector<connection_port_spec_t> derive_ports_from_config(
    PlatformId_t platform_id, const device_id slot_devices[NUM_SLOTS]) {
    std::vector<connection_port_spec_t> out;

    if (platform_is_iigs(platform_id)) {
        push_port(out, 1, "Serial Slot 1", connection_port_kind_t::SERIAL);
        push_port(out, 2, "Serial Slot 2", connection_port_kind_t::SERIAL);
    }

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = slot_devices[slot];
        if (id == DEVICE_ID_SUPER_SERIAL) {
            char name[32];
            std::snprintf(name, sizeof(name), "SSC Slot %d", slot);
            push_port(out, slot, name, connection_port_kind_t::SERIAL);
        } else if (id == DEVICE_ID_PARALLEL) {
            char name[32];
            std::snprintf(name, sizeof(name), "Parallel Slot %d", slot);
            push_port(out, slot, name, connection_port_kind_t::PARALLEL);
        }
    }

    std::sort(out.begin(), out.end(),
              [](const connection_port_spec_t& a, const connection_port_spec_t& b) {
                  if (a.key.slot != b.key.slot) return a.key.slot < b.key.slot;
                  return a.key.port < b.key.port;
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
    connections_.clear();
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

    if (!platform_is_iigs(platform_id)) {
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
    connections_ = config.connections();
    prune_orphan_connections();
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
    connections_.clear();
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
    if (platform_is_iigs(platform_id)) {
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

    if (platform_is_iigs(platform_id) && config_.slot_devices[6] == DEVICE_ID_DISK_II) {
        config_.slot_devices[6] = DEVICE_ID_NONE;
        clear_mounts_for_slot(6);
    }
    prune_orphan_connections();
}

void ConfigDraft::set_slot_device(int slot, device_id id) {
    if (slot < 0 || slot >= NUM_SLOTS) return;
    config_.slot_devices[slot] = id;
    clear_mounts_for_slot(slot);
    prune_orphan_connections();
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

void ConfigDraft::prune_orphan_connections() {
    const auto ports = derive_ports_from_config(config_.platform_id, config_.slot_devices);
    std::vector<connection_config_t> kept;
    for (auto c : connections_) {
        const connection_key_t nk = normalize_connection_key(c.slot, c.port);
        c.slot = nk.slot;
        c.port = nk.port;
        bool matched = false;
        for (const auto& p : ports) {
            if (p.key != nk) continue;
            matched = true;
            if (p.kind == connection_port_kind_t::PARALLEL) {
                const connection_device_type_t dt = connection_device_type_from_string(c.device);
                if (dt == connection_device_type_t::MODEM ||
                    dt == connection_device_type_t::SERIAL) {
                    c.device = "file";
                    c.path.clear();
                }
            }
            break;
        }
        if (matched) {
            kept.push_back(std::move(c));
        }
    }
    connections_ = std::move(kept);
}

void ConfigDraft::set_connection(connection_key_t key, connection_device_type_t device,
                                const std::string &path) {
    key = normalize_connection_key(key.slot, key.port);
    // Find port kind for validation.
    const auto ports = derive_ports_from_config(config_.platform_id, config_.slot_devices);
    connection_port_kind_t kind = connection_port_kind_t::SERIAL;
    bool found = false;
    for (const auto& p : ports) {
        if (p.key == key) {
            kind = p.kind;
            found = true;
            break;
        }
    }
    if (!found) return;
    if (!connection_device_allowed(kind, device)) return;

    auto apply_fields = [&](connection_config_t &c) {
        c.slot = key.slot;
        c.port = key.port;
        c.device = connection_device_type_name(device);
        if (device == connection_device_type_t::SERIAL) {
            c.path = path;
        } else {
            c.path.clear();
        }
    };

    for (auto& c : connections_) {
        const connection_key_t nk = normalize_connection_key(c.slot, c.port);
        if (nk == key) {
            apply_fields(c);
            return;
        }
    }
    connection_config_t c;
    apply_fields(c);
    connections_.push_back(std::move(c));
}

void ConfigDraft::clear_connection(connection_key_t key) {
    key = normalize_connection_key(key.slot, key.port);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [&key](const connection_config_t& c) {
                           return normalize_connection_key(c.slot, c.port) == key;
                       }),
        connections_.end());
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

std::vector<connection_port_spec_t> ConfigDraft::port_specs() const {
    std::vector<connection_port_spec_t> specs =
        derive_ports_from_config(config_.platform_id, config_.slot_devices);
    for (auto& spec : specs) {
        for (const auto& c : connections_) {
            if (normalize_connection_key(c.slot, c.port) == spec.key) {
                spec.device = connection_device_type_from_string(c.device);
                spec.path = c.path;
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

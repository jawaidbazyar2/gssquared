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
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../systemconfig.hpp"
#include "devices/displaypp/VideoScanner.hpp"
#include "util/mount.hpp"

struct card_extra_t {
    int slot = -1;
    std::string parallel_output;
};

struct connection_config_t {
    std::optional<int> slot;
    std::string port;
    std::string device;
    std::string path;
    std::string remote_url;
};

enum class ConfigFileKind {
    Gs2,
    Settings,
    Profiles,
    Unknown,
};

ConfigFileKind detect_config_file_kind(const std::string& path);

const char* platform_name(PlatformId_t platform);
const char* clock_name(clock_set_t clock_set);
const char* scanner_name(video_scanner_t scanner);
const char* card_type_name(device_id id);

struct slot_card_choice_t {
    device_id id;
    const char* toml_name;
    const char* display_name;
};

/**
 * Cards allowed in this slot on this platform (for the editor picker).
 * If occupied_slots is non-null, omit !multipleInstances cards already used in another slot.
 */
std::vector<slot_card_choice_t> cards_allowed_for_slot(
    PlatformId_t platform, int slot, const device_id occupied_slots[NUM_SLOTS] = nullptr);

/** Validate slot_devices[] against Devices[] (slot, platform, multiplicity). */
bool validate_slot_devices(const SystemConfig_t& config, std::string& error_out);

class SystemConfig {
    std::string path_;
    std::string name_;
    std::string description_;
    int gs2_version_ = 0;
    bool settings_source_ = false;
    SystemConfig_t config_data_{};
    std::vector<disk_mount_t> mounts_;
    std::vector<connection_config_t> connections_;
    std::vector<card_extra_t> card_extras_;
    std::vector<std::string> warnings_;
    std::vector<std::pair<std::string, std::string>> extensions_;

    void sync_config_pointers();
    void clear();
    bool load_gs2(const std::string& path, std::string& error_out);
    bool load_settings(const std::string& path, std::string& error_out);
    bool fallback_to_settings(const std::string& path, std::string& error_out);
    bool finalize_load(std::string& error_out);

public:
    SystemConfig() = default;

    bool load(const std::string& path, std::string& error_out);

    /** Write a .gs2 TOML file from the current in-memory config + mounts. */
    bool save(const std::string& path, std::string& error_out) const;

    /**
     * Populate this object from a SystemConfig_t + mounts (e.g. editor draft)
     * so save() can write it. Owned strings are copied.
     */
    void set_from_parts(const SystemConfig_t& config, const std::vector<disk_mount_t>& mounts);

    const SystemConfig_t& config() const { return config_data_; }
    const std::vector<disk_mount_t>& mounts() const { return mounts_; }
    const std::vector<connection_config_t>& connections() const { return connections_; }
    const std::vector<card_extra_t>& card_extras() const { return card_extras_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const std::vector<std::pair<std::string, std::string>>& extensions() const { return extensions_; }
    int gs2_version() const { return gs2_version_; }
    bool is_settings_source() const { return settings_source_; }
    const std::string& path() const { return path_; }

    void dump(std::ostream& out) const;

    /**
     * Copy shipped default .gs2 configs from resources/gs2 into PrefPath/SystemConfigs
     * when those files are not already present. Creates SystemConfigs if needed, and
     * sets the Launch Config dialog start folder there if unset.
     */
    static void ensure_default_system_configs();
};

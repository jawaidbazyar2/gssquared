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

#include "util/SystemConfig.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "device_info.hpp"
#include "devices/displaypp/VideoScanner.hpp"
#include "paths.hpp"
#include "util/SystemSettings.hpp"
#include "util/toml.hpp"
#include "util/uuid.hpp"

namespace {

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string join_path(const std::string& dir, const std::string& file) {
    if (dir.empty()) {
        return file;
    }
    std::filesystem::path p(dir);
    p /= file;
    return p.lexically_normal().string();
}

std::string resolve_path(const std::string& base_dir, const std::string& path) {
    if (path.empty() || Paths::is_absolute(path)) {
        return path;
    }
    return join_path(base_dir, path);
}

PlatformFlags_t platform_flag(PlatformId_t platform) {
    switch (platform) {
        case PLATFORM_APPLE_II: return PLATFLAG_APPLE_II;
        case PLATFORM_APPLE_II_PLUS: return PLATFLAG_APPLE_II_PLUS;
        case PLATFORM_APPLE_IIE: return PLATFLAG_APPLE_IIE;
        case PLATFORM_APPLE_IIE_ENHANCED: return PLATFLAG_APPLE_IIE_ENHANCED;
        case PLATFORM_APPLE_IIE_65816: return PLATFLAG_APPLE_IIE_65816;
        case PLATFORM_APPLE_IIGS: return PLATFLAG_APPLE_IIGS;
        case PLATFORM_APPLE_IIGS_ROM3: return PLATFLAG_APPLE_IIGS_ROM3;
        default: return PLATFLAG_NONE;
    }
}

std::optional<PlatformId_t> parse_platform(const std::string& value, std::string& error_out) {
    static const std::unordered_map<std::string, PlatformId_t> map = {
        {"apple2", PLATFORM_APPLE_II},
        {"apple2plus", PLATFORM_APPLE_II_PLUS},
        {"apple2e", PLATFORM_APPLE_IIE},
        {"apple2e_enhanced", PLATFORM_APPLE_IIE_ENHANCED},
        {"apple2e_65816", PLATFORM_APPLE_IIE_65816},
        {"apple2gs", PLATFORM_APPLE_IIGS},
        {"apple2gs_rom3", PLATFORM_APPLE_IIGS_ROM3},
    };
    const auto it = map.find(value);
    if (it == map.end()) {
        error_out = "Unknown platform: " + value;
        return std::nullopt;
    }
    return it->second;
}

std::optional<clock_set_t> parse_clock(const std::string& value, std::string& error_out) {
    if (value == "ntsc") return CLOCK_SET_US;
    if (value == "pal") return CLOCK_SET_PAL;
    error_out = "Unknown clock: " + value;
    return std::nullopt;
}

video_scanner_t derive_scanner(PlatformId_t platform, clock_set_t clock_set) {
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
            if (clock_set == CLOCK_SET_PAL) {
                return Scanner_AppleIIePAL;
            }
            return Scanner_AppleIIe;
        default:
            return Scanner_AppleII;
    }
}

std::optional<video_scanner_t> parse_scanner(const std::string& value, std::string& error_out) {
    static const std::unordered_map<std::string, video_scanner_t> map = {
        {"apple2", Scanner_AppleII},
        {"apple2e", Scanner_AppleIIe},
        {"apple2e_pal", Scanner_AppleIIePAL},
        {"apple2gs", Scanner_AppleIIgs},
    };
    const auto it = map.find(value);
    if (it == map.end()) {
        error_out = "Unknown scanner: " + value;
        return std::nullopt;
    }
    return it->second;
}

// TODO: I'm not sure this is really necessary.
std::string canonical_card_name(const std::string& value) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"pdblock3", "bazfast3"},
        {"smartport", "bazfast3"},
        {"pdblock2", "prodos_block2"},
    };
    const auto it = aliases.find(value);
    if (it != aliases.end()) {
        return it->second;
    }
    return value;
}

std::optional<device_id> parse_card_type(const std::string& value, std::string& error_out) {
    static const std::unordered_map<std::string, device_id> map = {
        {"language_card", DEVICE_ID_LANGUAGE_CARD},
        {"prodos_block", DEVICE_ID_PRODOS_BLOCK},
        {"prodos_clock", DEVICE_ID_PRODOS_CLOCK},
        {"disk_ii", DEVICE_ID_DISK_II},
        {"mem_expansion", DEVICE_ID_MEM_EXPANSION},
        {"thunder_clock", DEVICE_ID_THUNDER_CLOCK},
        {"prodos_block2", DEVICE_ID_PD_BLOCK2},
        {"parallel", DEVICE_ID_PARALLEL},
        {"videx", DEVICE_ID_VIDEX},
        {"mockingboard", DEVICE_ID_MOCKINGBOARD},
        {"mouse", DEVICE_ID_MOUSE},
        {"vidhd", DEVICE_ID_VIDHD},
        {"bazfast3", DEVICE_ID_PD_BLOCK3},
        {"second_sight", DEVICE_ID_SECOND_SIGHT},
    };
    const std::string canonical = canonical_card_name(value);
    const auto it = map.find(canonical);
    if (it == map.end()) {
        error_out = "Unknown card type: " + value;
        return std::nullopt;
    }
    return it->second;
}

bool slot_allows_device(int slot, const DeviceInfo_t* device) {
    if (slot < 0 || slot >= NUM_SLOTS) {
        return false;
    }
    // slots_allowed == 0 means motherboard / non-slot device, not "any slot".
    if (device->slots_allowed == 0) {
        return false;
    }
    return (device->slots_allowed & (1u << slot)) != 0;
}

bool validate_cards(const SystemConfig_t& config, PlatformId_t platform,
                    const std::vector<card_extra_t>* card_extras,
                    std::vector<std::string>* warnings, std::string& error_out) {
    std::unordered_map<device_id, int> instance_counts;
    const PlatformFlags_t flag = platform_flag(platform);

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = config.slot_devices[slot];
        if (id == DEVICE_ID_NONE) {
            continue;
        }

        const DeviceInfo_t* device = get_device_info(id);
        if (device == nullptr) {
            error_out = "Invalid device id in slot " + std::to_string(slot);
            return false;
        }

        if (device->slots_allowed == 0) {
            error_out = "Device " + std::string(device->name) + " is not a slot card (slot "
                        + std::to_string(slot) + ")";
            return false;
        }

        if ((device->platform_flags & flag) == 0) {
            error_out = "Card " + std::string(card_type_name(id)) + " is not allowed on platform "
                        + std::string(platform_name(platform));
            return false;
        }

        if (!slot_allows_device(slot, device)) {
            error_out = "Card " + std::string(card_type_name(id)) + " is not allowed in slot "
                        + std::to_string(slot);
            return false;
        }

        instance_counts[id]++;
        if (!device->multipleInstances && instance_counts[id] > 1) {
            error_out = "Multiple instances of card " + std::string(card_type_name(id))
                        + " are not allowed";
            return false;
        }
    }

    if (card_extras != nullptr && warnings != nullptr) {
        for (const auto& extra : *card_extras) {
            if (extra.slot < 0 || extra.slot >= NUM_SLOTS) {
                continue;
            }
            if (config.slot_devices[extra.slot] != DEVICE_ID_PARALLEL) {
                warnings->push_back("parallel output on slot " + std::to_string(extra.slot)
                                    + " ignored: card is not parallel");
            }
        }
    }

    return true;
}

bool validate_storage(const std::vector<disk_mount_t>& mounts, std::string& error_out) {
    std::unordered_set<uint32_t> seen;
    for (const auto& mount : mounts) {
        if (mount.slot >= NUM_SLOTS) {
            error_out = "Storage slot out of range: " + std::to_string(mount.slot);
            return false;
        }
        const int drive_1based = static_cast<int>(mount.drive) + 1;
        if (drive_1based < 1 || drive_1based > 6) {
            error_out = "Storage drive out of range (1-6): " + std::to_string(drive_1based);
            return false;
        }
        const uint32_t key = (static_cast<uint32_t>(mount.slot) << 16)
                           | static_cast<uint32_t>(mount.drive);
        if (seen.count(key)) {
            error_out = "Duplicate storage entry for slot " + std::to_string(mount.slot)
                        + " drive " + std::to_string(drive_1based);
            return false;
        }
        seen.insert(key);
        if (mount.filename.empty()) {
            error_out = "Storage image path is empty for slot " + std::to_string(mount.slot)
                        + " drive " + std::to_string(drive_1based);
            return false;
        }
    }
    return true;
}

// TODO: none of this has been tested yet.
bool validate_connections(PlatformId_t platform,
                          const std::vector<connection_config_t>& connections,
                          std::string& error_out) {
    struct conn_key {
        int slot;
        std::string port;
        bool operator==(const conn_key& other) const {
            return slot == other.slot && port == other.port;
        }
    };
    struct conn_hash {
        size_t operator()(const conn_key& key) const {
            return std::hash<int>()(key.slot) ^ (std::hash<std::string>()(key.port) << 1);
        }
    };

    std::unordered_set<conn_key, conn_hash> seen;

    for (const auto& conn : connections) {
        if (!conn.slot.has_value() && !platform_is_iigs(platform)) {
            error_out = "Built-in serial port connections require an Apple IIgs platform";
            return false;
        }

        std::string port = conn.port.empty() ? "a" : to_lower(conn.port);
        if (port != "a" && port != "b") {
            error_out = "Invalid connection port: " + conn.port;
            return false;
        }

        const int slot_key = conn.slot.value_or(-1);
        const conn_key key{slot_key, port};
        if (seen.count(key)) {
            if (conn.slot.has_value()) {
                error_out = "Duplicate connection for slot " + std::to_string(*conn.slot)
                            + " port " + port;
            } else {
                error_out = "Duplicate connection for built-in port " + port;
            }
            return false;
        }
        seen.insert(key);

        const std::string device = to_lower(conn.device);
        if (device != "none" && device != "file" && device != "echo" && device != "modem") {
            error_out = "Unknown connection device: " + conn.device;
            return false;
        }
        if (device == "file" && conn.path.empty()) {
            error_out = "Connection device=file requires path";
            return false;
        }
    }
    return true;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

bool is_valid_settings_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    for (char c : key) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.') {
            return false;
        }
    }
    return true;
}

bool looks_like_settings_file(const std::string& path) {
    const std::string basename = std::filesystem::path(path).filename().string();
    if (Paths::ends_with_icase(basename, "Settings.txt")) {
        return true;
    }

    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line.rfind("gs2_version", 0) == 0) {
            return false;
        }
        const size_t split = line.find_first_of(" \t");
        if (split == std::string::npos) {
            return false;
        }
        const std::string key = trim(line.substr(0, split));
        const std::string value = trim(line.substr(split + 1));
        return is_valid_settings_key(key) && !value.empty();
    }
    return false;
}

} // namespace

const char* platform_name(PlatformId_t platform) {
    switch (platform) {
        case PLATFORM_APPLE_II: return "apple2";
        case PLATFORM_APPLE_II_PLUS: return "apple2plus";
        case PLATFORM_APPLE_IIE: return "apple2e";
        case PLATFORM_APPLE_IIE_ENHANCED: return "apple2e_enhanced";
        case PLATFORM_APPLE_IIE_65816: return "apple2e_65816";
        case PLATFORM_APPLE_IIGS: return "apple2gs";
        case PLATFORM_APPLE_IIGS_ROM3: return "apple2gs_rom3";
        default: return "unknown";
    }
}

const char* clock_name(clock_set_t clock_set) {
    switch (clock_set) {
        case CLOCK_SET_US: return "ntsc";
        case CLOCK_SET_PAL: return "pal";
        default: return "unknown";
    }
}

const char* scanner_name(video_scanner_t scanner) {
    switch (scanner) {
        case Scanner_AppleII: return "apple2";
        case Scanner_AppleIIe: return "apple2e";
        case Scanner_AppleIIePAL: return "apple2e_pal";
        case Scanner_AppleIIgs: return "apple2gs";
        default: return "unknown";
    }
}

const char* card_type_name(device_id id) {
    switch (id) {
        case DEVICE_ID_LANGUAGE_CARD: return "language_card";
        case DEVICE_ID_PRODOS_BLOCK: return "prodos_block";
        case DEVICE_ID_PRODOS_CLOCK: return "prodos_clock";
        case DEVICE_ID_DISK_II: return "disk_ii";
        case DEVICE_ID_MEM_EXPANSION: return "mem_expansion";
        case DEVICE_ID_THUNDER_CLOCK: return "thunder_clock";
        case DEVICE_ID_PD_BLOCK2: return "prodos_block2";
        case DEVICE_ID_PARALLEL: return "parallel";
        case DEVICE_ID_VIDEX: return "videx";
        case DEVICE_ID_MOCKINGBOARD: return "mockingboard";
        case DEVICE_ID_MOUSE: return "mouse";
        case DEVICE_ID_VIDHD: return "vidhd";
        case DEVICE_ID_PD_BLOCK3: return "bazfast3";
        case DEVICE_ID_SECOND_SIGHT: return "second_sight";
        default: return "none";
    }
}

static const char* card_display_name(device_id id) {
    switch (id) {
        case DEVICE_ID_LANGUAGE_CARD: return "Language Card";
        case DEVICE_ID_PRODOS_BLOCK: return "ProDOS Block";
        case DEVICE_ID_PRODOS_CLOCK: return "ProDOS Clock";
        case DEVICE_ID_DISK_II: return "Disk II Controller";
        case DEVICE_ID_MEM_EXPANSION: return "Memory Expansion";
        case DEVICE_ID_THUNDER_CLOCK: return "Thunder Clock";
        case DEVICE_ID_PD_BLOCK2: return "ProDOS Block 2";
        case DEVICE_ID_PARALLEL: return "Parallel Interface";
        case DEVICE_ID_VIDEX: return "Videx VideoTerm";
        case DEVICE_ID_MOCKINGBOARD: return "Mockingboard";
        case DEVICE_ID_MOUSE: return "Apple Mouse II";
        case DEVICE_ID_VIDHD: return "VIDHD";
        case DEVICE_ID_PD_BLOCK3: return "BazFast 3";
        case DEVICE_ID_SECOND_SIGHT: return "Second Sight";
        default: return "None";
    }
}

std::vector<slot_card_choice_t> cards_allowed_for_slot(
    PlatformId_t platform, int slot, const device_id occupied_slots[NUM_SLOTS]) {
    std::vector<slot_card_choice_t> out;
    if (slot < 0 || slot >= NUM_SLOTS) {
        return out;
    }

    const PlatformFlags_t flag = platform_flag(platform);
    for (int i = 1; i < NUM_DEVICE_IDS; ++i) {
        const device_id id = static_cast<device_id>(i);
        const DeviceInfo_t* device = get_device_info(id);
        if (device == nullptr || device->slots_allowed == 0) {
            continue; // motherboard / non-slot device
        }
        if ((device->platform_flags & flag) == 0) {
            continue;
        }
        if ((device->slots_allowed & (1u << slot)) == 0) {
            continue;
        }
        if (occupied_slots != nullptr && !device->multipleInstances) {
            bool used_elsewhere = false;
            for (int s = 0; s < NUM_SLOTS; ++s) {
                if (s != slot && occupied_slots[s] == id) {
                    used_elsewhere = true;
                    break;
                }
            }
            if (used_elsewhere) {
                continue;
            }
        }
        out.push_back({id, card_type_name(id), card_display_name(id)});
    }
    return out;
}

bool validate_slot_devices(const SystemConfig_t& config, std::string& error_out) {
    return validate_cards(config, config.platform_id, nullptr, nullptr, error_out);
}

static std::string toml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

void SystemConfig::set_from_parts(const SystemConfig_t& config, const std::vector<disk_mount_t>& mounts) {
    clear();
    name_ = config.name ? config.name : "";
    description_ = config.description ? config.description : "";
    id_ = config.id ? config.id : "";
    if (id_.empty()) {
        id_ = generate_uuid_v4();
    }
    config_data_ = config;
    config_data_.builtin = false;
    mounts_ = mounts;
    gs2_version_ = 1;
    sync_config_pointers();
}

bool SystemConfig::save(const std::string& path, std::string& error_out) {
    if (name_.empty() && !(config_data_.name && config_data_.name[0])) {
        error_out = "name must not be empty";
        return false;
    }
    if (id_.empty()) {
        id_ = generate_uuid_v4();
        sync_config_pointers();
    }
    const std::string& name_to_write = !name_.empty() ? name_ : std::string(config_data_.name);

    std::ofstream out(path);
    if (!out) {
        error_out = "Failed to open file for writing: " + path;
        return false;
    }

    out << "gs2_version = 1\n";
    out << "id = \"" << toml_escape(id_) << "\"\n";
    out << "name = \"" << toml_escape(name_to_write) << "\"\n";
    if (!description_.empty()) {
        out << "description = \"" << toml_escape(description_) << "\"\n";
    }
    out << "platform = \"" << platform_name(config_data_.platform_id) << "\"\n";
    out << "clock = \"" << clock_name(config_data_.clock_set) << "\"\n";
    out << "scanner = \"" << scanner_name(config_data_.scanner_type) << "\"\n";
    out << "builtin = false\n";

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = config_data_.slot_devices[slot];
        if (id == DEVICE_ID_NONE) continue;
        out << "\n[[cards]]\n";
        out << "slot = " << slot << "\n";
        out << "card = \"" << card_type_name(id) << "\"\n";
    }

    const std::string base_dir = Paths::get_directory(path);
    for (const auto& mount : mounts_) {
        out << "\n[[storage]]\n";
        out << "slot = " << mount.slot << "\n";
        out << "drive = " << (mount.drive + 1) << "\n";
        std::string image = mount.filename;
        // Prefer relative path when image is under the config directory.
        if (!base_dir.empty() && image.rfind(base_dir, 0) == 0) {
            std::string rel = image.substr(base_dir.size());
            while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) {
                rel.erase(rel.begin());
            }
            if (!rel.empty()) image = rel;
        }
        out << "image = \"" << toml_escape(image) << "\"\n";
    }

    if (!out) {
        error_out = "Write failed: " + path;
        return false;
    }
    return true;
}

bool SystemConfig::fallback_to_settings(const std::string& path, std::string& error_out) {
    if (!looks_like_settings_file(path)) {
        return false;
    }
    clear();
    path_ = path;
    settings_source_ = true;
    return load_settings(path, error_out);
}

void SystemConfig::sync_config_pointers() {
    config_data_.name = name_.c_str();
    config_data_.description = description_.c_str();
    config_data_.id = id_.c_str();
}

void SystemConfig::clear() {
    path_.clear();
    name_.clear();
    description_.clear();
    id_.clear();
    gs2_version_ = 0;
    settings_source_ = false;
    config_data_ = {};
    mounts_.clear();
    connections_.clear();
    card_extras_.clear();
    warnings_.clear();
    extensions_.clear();
    for (int i = 0; i < NUM_SLOTS; ++i) {
        config_data_.slot_devices[i] = DEVICE_ID_NONE;
    }
    config_data_.builtin = false;
    sync_config_pointers();
}

ConfigFileKind detect_config_file_kind(const std::string& path) {
    const std::string basename = std::filesystem::path(path).filename().string();
    if (Paths::ends_with_icase(basename, "Profiles.txt")) {
        return ConfigFileKind::Profiles;
    }
    if (Paths::ends_with_icase(basename, "Settings.txt")) {
        return ConfigFileKind::Settings;
    }
    if (Paths::ends_with_icase(basename, ".gs2")) {
        return ConfigFileKind::Gs2;
    }
    return ConfigFileKind::Unknown;
}

bool SystemConfig::load(const std::string& path, std::string& error_out) {
    clear();
    path_ = path;

    switch (detect_config_file_kind(path)) {
        case ConfigFileKind::Gs2:
            return load_gs2(path, error_out);
        case ConfigFileKind::Settings:
            settings_source_ = true;
            return load_settings(path, error_out);
        case ConfigFileKind::Profiles:
            error_out = "Profiles.txt is a catalog file, not a system configuration";
            return false;
        case ConfigFileKind::Unknown:
            error_out = "Not a .gs2 or Settings.txt file";
            return false;
    }
    error_out = "Unknown config file type";
    return false;
}

bool SystemConfig::finalize_load(std::string& error_out) {
    if (!validate_cards(config_data_, config_data_.platform_id, &card_extras_, &warnings_, error_out)) {
        return false;
    }
    if (!validate_storage(mounts_, error_out)) {
        return false;
    }
    if (!validate_connections(config_data_.platform_id, connections_, error_out)) {
        return false;
    }
    return true;
}

bool SystemConfig::load_gs2(const std::string& path, std::string& error_out) {
    toml::table table;
    try {
        table = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        if (fallback_to_settings(path, error_out)) {
            return true;
        }
        error_out = std::string("TOML parse error: ") + err.what();
        return false;
    }

    const auto version_node = table["gs2_version"];
    if (!version_node.is_integer()) {
        if (fallback_to_settings(path, error_out)) {
            return true;
        }
        error_out = "Missing or invalid gs2_version";
        return false;
    }
    gs2_version_ = static_cast<int>(*version_node.value<int64_t>());
    if (gs2_version_ != 1) {
        error_out = "Unsupported gs2_version: " + std::to_string(gs2_version_);
        return false;
    }

    const auto name_node = table["name"];
    if (!name_node.is_string()) {
        error_out = "Missing or invalid name";
        return false;
    }
    name_ = std::string(*name_node.value<std::string>());
    if (name_.empty()) {
        error_out = "name must not be empty";
        return false;
    }

    if (const auto desc_node = table["description"]; desc_node.is_string()) {
        description_ = std::string(*desc_node.value<std::string>());
    }

    if (const auto id_node = table["id"]; id_node.is_string()) {
        id_ = std::string(*id_node.value<std::string>());
    }

    const auto platform_node = table["platform"];
    if (!platform_node.is_string()) {
        error_out = "Missing or invalid platform";
        return false;
    }
    const auto platform = parse_platform(std::string(*platform_node.value<std::string>()), error_out);
    if (!platform.has_value()) {
        return false;
    }
    config_data_.platform_id = *platform;

    clock_set_t clock_set = CLOCK_SET_US;
    if (const auto clock_node = table["clock"]; clock_node.is_string()) {
        const auto parsed = parse_clock(std::string(*clock_node.value<std::string>()), error_out);
        if (!parsed.has_value()) {
            return false;
        }
        clock_set = *parsed;
    }
    config_data_.clock_set = clock_set;

    if (platform_is_iigs(config_data_.platform_id) && clock_set == CLOCK_SET_PAL) {
        error_out = "clock=pal is not valid for Apple IIgs platforms";
        return false;
    }

    if (const auto scanner_node = table["scanner"]; scanner_node.is_string()) {
        const auto parsed = parse_scanner(std::string(*scanner_node.value<std::string>()), error_out);
        if (!parsed.has_value()) {
            return false;
        }
        config_data_.scanner_type = *parsed;
    } else {
        config_data_.scanner_type = derive_scanner(config_data_.platform_id, clock_set);
    }

    if (const auto builtin_node = table["builtin"]; builtin_node.is_boolean()) {
        config_data_.builtin = *builtin_node.value<bool>();
    } else {
        config_data_.builtin = false;
    }

    sync_config_pointers();

    const std::string base_dir = Paths::get_directory(path);

    if (const auto cards = table["cards"].as_array()) {
        for (const auto& entry : *cards) {
            const auto* card_table = entry.as_table();
            if (card_table == nullptr) {
                error_out = "Invalid [[cards]] entry";
                return false;
            }

            const auto slot_node = (*card_table)["slot"];
            if (!slot_node.is_integer()) {
                error_out = "[[cards]] entry missing slot";
                return false;
            }
            const int slot = static_cast<int>(*slot_node.value<int64_t>());
            if (slot < 0 || slot >= NUM_SLOTS) {
                error_out = "[[cards]] slot out of range: " + std::to_string(slot);
                return false;
            }
            if (config_data_.slot_devices[slot] != DEVICE_ID_NONE) {
                error_out = "Duplicate [[cards]] slot: " + std::to_string(slot);
                return false;
            }

            const auto card_node = (*card_table)["card"];
            if (!card_node.is_string()) {
                error_out = "[[cards]] entry missing card";
                return false;
            }
            const auto card_id = parse_card_type(std::string(*card_node.value<std::string>()), error_out);
            if (!card_id.has_value()) {
                return false;
            }
            config_data_.slot_devices[slot] = *card_id;

            static const std::unordered_set<std::string> common_keys = {"slot", "card"};
            for (const auto& [key, value] : *card_table) {
                const std::string key_str(key);
                if (common_keys.count(key_str)) {
                    continue;
                }
                if (*card_id == DEVICE_ID_PARALLEL && key_str == "output") {
                    if (!value.is_string()) {
                        error_out = "parallel output must be a string";
                        return false;
                    }
                    card_extra_t extra;
                    extra.slot = slot;
                    extra.parallel_output = std::string(*value.value<std::string>());
                    card_extras_.push_back(extra);
                    continue;
                }
                warnings_.push_back("Unknown key on [[cards]] entry: " + key_str);
            }
        }
    }

    if (const auto storage = table["storage"].as_array()) {
        for (const auto& entry : *storage) {
            const auto* storage_table = entry.as_table();
            if (storage_table == nullptr) {
                error_out = "Invalid [[storage]] entry";
                return false;
            }

            const auto slot_node = (*storage_table)["slot"];
            const auto drive_node = (*storage_table)["drive"];
            const auto image_node = (*storage_table)["image"];
            if (!slot_node.is_integer() || !drive_node.is_integer() || !image_node.is_string()) {
                error_out = "[[storage]] entry requires slot, drive, and image";
                return false;
            }

            disk_mount_t mount;
            mount.slot = static_cast<uint16_t>(*slot_node.value<int64_t>());
            const int drive_1based = static_cast<int>(*drive_node.value<int64_t>());
            mount.drive = static_cast<uint16_t>(drive_1based - 1);
            mount.filename = resolve_path(base_dir, std::string(*image_node.value<std::string>()));
            mounts_.push_back(mount);
        }
    }

    if (const auto connections = table["connections"].as_array()) {
        for (const auto& entry : *connections) {
            const auto* conn_table = entry.as_table();
            if (conn_table == nullptr) {
                error_out = "Invalid [[connections]] entry";
                return false;
            }

            connection_config_t conn;
            if (const auto device_node = (*conn_table)["device"]; device_node.is_string()) {
                conn.device = std::string(*device_node.value<std::string>());
            } else {
                error_out = "[[connections]] entry missing device";
                return false;
            }

            if (const auto port_node = (*conn_table)["port"]; port_node.is_string()) {
                conn.port = to_lower(std::string(*port_node.value<std::string>()));
            } else {
                conn.port = "a";
            }

            if (const auto slot_node = (*conn_table)["slot"]; slot_node.is_integer()) {
                conn.slot = static_cast<int>(*slot_node.value<int64_t>());
            }

            if (const auto path_node = (*conn_table)["path"]; path_node.is_string()) {
                conn.path = resolve_path(base_dir, std::string(*path_node.value<std::string>()));
            }
            if (const auto url_node = (*conn_table)["remote_url"]; url_node.is_string()) {
                conn.remote_url = std::string(*url_node.value<std::string>());
            }

            connections_.push_back(conn);
        }
    }

    bool minted_id = false;
    if (id_.empty()) {
        id_ = generate_uuid_v4();
        minted_id = true;
    }
    sync_config_pointers();

    if (!finalize_load(error_out)) {
        return false;
    }

    if (minted_id) {
        std::string save_err;
        if (!save(path, save_err)) {
            std::cerr << "Failed to persist machine id to '" << path << "': " << save_err << std::endl;
        }
    }
    return true;
}

void SystemConfig::dump(std::ostream& out) const {
    out << "SystemConfig: " << path_ << "\n";
    out << "  source: " << (settings_source_ ? "settings.txt" : "gs2") << "\n";
    out << "  gs2_version: " << gs2_version_ << "\n";
    out << "  id: " << id_ << "\n";
    out << "  name: " << name_ << "\n";
    out << "  description: " << description_ << "\n";
    out << "  platform: " << platform_name(config_data_.platform_id)
        << " (" << static_cast<int>(config_data_.platform_id) << ")\n";
    out << "  clock: " << clock_name(config_data_.clock_set)
        << " (" << static_cast<int>(config_data_.clock_set) << ")\n";
    out << "  scanner: " << scanner_name(config_data_.scanner_type)
        << " (" << static_cast<int>(config_data_.scanner_type) << ")\n";
    out << "  builtin: " << (config_data_.builtin ? "true" : "false") << "\n";

    out << "  slot_devices:\n";
    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = config_data_.slot_devices[slot];
        out << "    slot " << slot << ": " << card_type_name(id)
            << " (" << static_cast<int>(id) << ")\n";
    }

    out << "  storage (" << mounts_.size() << "):\n";
    for (const auto& mount : mounts_) {
        out << "    slot " << mount.slot << " drive " << (mount.drive + 1)
            << " image: " << mount.filename << "\n";
    }

    out << "  connections (" << connections_.size() << ", not applied at runtime):\n";
    for (const auto& conn : connections_) {
        out << "    ";
        if (conn.slot.has_value()) {
            out << "slot " << *conn.slot << " ";
        } else {
            out << "builtin ";
        }
        out << "port " << conn.port << " device " << conn.device;
        if (!conn.path.empty()) {
            out << " path " << conn.path;
        }
        if (!conn.remote_url.empty()) {
            out << " remote_url " << conn.remote_url;
        }
        out << "\n";
    }

    out << "  card extras (" << card_extras_.size() << ", not applied at runtime):\n";
    for (const auto& extra : card_extras_) {
        out << "    slot " << extra.slot << " parallel output: " << extra.parallel_output << "\n";
    }

    if (!warnings_.empty()) {
        out << "  warnings:\n";
        for (const auto& warning : warnings_) {
            out << "    " << warning << "\n";
        }
    }

    if (!extensions_.empty()) {
        out << "  extensions (" << extensions_.size() << "):\n";
        for (const auto& [key, value] : extensions_) {
            out << "    " << key << " = " << value << "\n";
        }
    }
}

void SystemConfig::ensure_default_system_configs() {
    namespace fs = std::filesystem;

    std::string dest_str;
    Paths::calc_pref(dest_str, "SystemConfigs");
    const fs::path dest_dir(dest_str);

    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec) {
        std::cerr << "Failed to create SystemConfigs directory '" << dest_dir.string()
                  << "': " << ec.message() << std::endl;
        return;
    }

    SystemSettings::instance().set_file_dialog_dir_if_unset(FileDialogKind::Config,
                                                            dest_dir.string());

    std::string src_str;
    Paths::calc_base(src_str, "gs2");
    const fs::path src_dir(src_str);
    if (!fs::is_directory(src_dir)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(src_dir, ec)) {
        if (ec) {
            std::cerr << "Failed to read shipped configs in '" << src_dir.string()
                      << "': " << ec.message() << std::endl;
            return;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".gs2") {
            continue;
        }

        const fs::path dest = dest_dir / entry.path().filename();
        if (fs::exists(dest)) {
            continue;
        }

        ec.clear();
        fs::copy_file(entry.path(), dest, ec);
        if (ec) {
            std::cerr << "Failed to copy default system config '" << entry.path().filename().string()
                      << "' to '" << dest.string() << "': " << ec.message() << std::endl;
        } else {
            std::cout << "Installed default system config: " << dest.string() << std::endl;
        }
    }
}

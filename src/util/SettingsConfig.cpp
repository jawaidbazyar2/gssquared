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
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "paths.hpp"
#include "util/uuid.hpp"

namespace {

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
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

std::string join_path(const std::string& dir, const std::string& file) {
    if (dir.empty()) {
        return file;
    }
    std::filesystem::path p(dir);
    p /= file;
    return p.lexically_normal().string();
}

std::string resolve_settings_path(const std::string& base_dir, const std::string& path) {
    std::string normalized = path;
    if (Paths::ends_with_icase(normalized, ":/") || normalized.size() >= 3) {
        const std::string prefix = to_lower(normalized.substr(0, 3));
        if (prefix == "sd:") {
            normalized = normalized.substr(3);
            while (!normalized.empty() && (normalized.front() == '/' || normalized.front() == '\\')) {
                normalized.erase(normalized.begin());
            }
        }
    }
    if (normalized.empty() || Paths::is_absolute(normalized)) {
        return normalized;
    }
    return join_path(base_dir, normalized);
}

std::optional<PlatformId_t> parse_machine_token(const std::string& token) {
    static const std::unordered_map<std::string, PlatformId_t> map = {
        {"apple2", PLATFORM_APPLE_II},
        {"apple2plus", PLATFORM_APPLE_II_PLUS},
        {"apple2e", PLATFORM_APPLE_IIE},
        {"apple2e_enhanced", PLATFORM_APPLE_IIE_ENHANCED},
        {"apple2e_65816", PLATFORM_APPLE_IIE_65816},
        {"a2gs", PLATFORM_APPLE_IIGS},
    };

    const std::string key = to_lower(token);
    if (key == "apple2c") {
        return std::nullopt;
    }
    const auto it = map.find(key);
    if (it == map.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<PlatformId_t> parse_machine_value(const std::string& value,
                                                 std::vector<std::string>& warnings,
                                                 std::string& error_out) {
    std::stringstream ss(value);
    std::string token;
    bool saw_token = false;
    bool saw_unsupported = false;

    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        saw_token = true;
        const std::string lower = to_lower(token);
        if (lower == "apple2c") {
            warnings.push_back("Unsupported machine token: " + token);
            saw_unsupported = true;
            continue;
        }
        const auto platform = parse_machine_token(token);
        if (platform.has_value()) {
            return *platform;
        }
        warnings.push_back("Unknown machine token: " + token);
    }

    if (!saw_token) {
        error_out = "machine key has no valid tokens";
        return std::nullopt;
    }
    if (saw_unsupported || saw_token) {
        error_out = "No supported machine token in: " + value;
    }
    return std::nullopt;
}

std::optional<clock_set_t> parse_clock(const std::string& value, std::string& error_out) {
    const std::string lower = to_lower(value);
    if (lower == "ntsc") return CLOCK_SET_US;
    if (lower == "pal") return CLOCK_SET_PAL;
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
    const auto it = map.find(to_lower(value));
    if (it == map.end()) {
        error_out = "Unknown scanner: " + value;
        return std::nullopt;
    }
    return it->second;
}

std::string canonical_card_name(const std::string& value) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"pdblock3", "bazfast3"},
        {"smartport", "bazfast3"},
        {"pdblock2", "prodos_block2"},
        {"floppy", "disk_ii"},
        {"floppy2", "disk_ii"},
        {"floppy3", "disk_ii"},
        {"floppy4", "disk_ii"},
    };
    const auto it = aliases.find(to_lower(value));
    if (it != aliases.end()) {
        return it->second;
    }
    return to_lower(value);
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

bool parse_settings_line(const std::string& line, std::string& key_out, std::string& value_out) {
    std::string work = line;
    const size_t comment = work.find('#');
    if (comment != std::string::npos) {
        work = work.substr(0, comment);
    }
    work = trim(work);
    if (work.empty()) {
        return false;
    }

    const size_t split = work.find_first_of(" \t");
    if (split == std::string::npos) {
        return false;
    }

    key_out = trim(work.substr(0, split));
    value_out = trim(work.substr(split + 1));
    while (!value_out.empty()) {
        const size_t end = value_out.find_last_not_of(" \t");
        if (end == std::string::npos) {
            value_out.clear();
            break;
        }
        value_out = value_out.substr(0, end + 1);
        if (value_out.find('#') != std::string::npos) {
            return false;
        }
        break;
    }

    return !key_out.empty() && !value_out.empty();
}

std::string settings_name_from_path(const std::string& path) {
    std::filesystem::path p(path);
    std::string basename = p.filename().string();
    const std::string suffix = "Settings.txt";
    if (Paths::ends_with_icase(basename, suffix)) {
        basename = basename.substr(0, basename.size() - suffix.size());
    }
    return trim(basename);
}

bool is_preference_key(const std::string& key_lower) {
    static const std::unordered_set<std::string> prefixes = {
        "video.", "machine.speed", "sound", "networking.", "ui.", "profile.image",
    };
    if (key_lower == "machine.speed" || key_lower == "sound") {
        return true;
    }
    for (const auto& prefix : prefixes) {
        if (prefix.back() == '.') {
            if (key_lower.rfind(prefix, 0) == 0) {
                return true;
            }
        } else if (key_lower == prefix) {
            return true;
        }
    }
    return false;
}

std::optional<int> find_bazfast_slot(const SystemConfig_t& config) {
    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        if (config.slot_devices[slot] == DEVICE_ID_PD_BLOCK3) {
            return slot;
        }
    }
    return std::nullopt;
}

std::vector<int> find_disk_ii_slots(const SystemConfig_t& config) {
    std::vector<int> slots;
    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        if (config.slot_devices[slot] == DEVICE_ID_DISK_II) {
            slots.push_back(slot);
        }
    }
    return slots;
}

int default_floppy_slot(PlatformId_t platform) {
    return platform == PLATFORM_APPLE_IIGS ? 6 : 6;
}

int floppy_controller_index(const std::string& prefix_lower) {
    static const std::regex re(R"(^floppy(\d*)$)");
    std::smatch match;
    if (!std::regex_match(prefix_lower, match, re)) {
        return -1;
    }
    if (match[1].str().empty()) {
        return 1;
    }
    return std::stoi(match[1].str());
}

void upsert_mount(std::vector<disk_mount_t>& mounts, const disk_mount_t& mount) {
    for (auto& existing : mounts) {
        if (existing.slot == mount.slot && existing.drive == mount.drive) {
            existing = mount;
            return;
        }
    }
    mounts.push_back(mount);
}

constexpr int kDefaultSmartportSlot = 7;

void ensure_default_smartport_card(SystemConfig_t& config, bool slot7_specified,
                                   std::vector<std::string>& warnings) {
    if (slot7_specified || config.slot_devices[kDefaultSmartportSlot] != DEVICE_ID_NONE) {
        return;
    }
    config.slot_devices[kDefaultSmartportSlot] = DEVICE_ID_PD_BLOCK3;
    warnings.push_back("Default smartport (bazfast3) installed in slot "
                       + std::to_string(kDefaultSmartportSlot));
}

} // namespace

bool SystemConfig::load_settings(const std::string& path, std::string& error_out) {
    std::ifstream in(path);
    if (!in) {
        error_out = "Failed to open Settings file: " + path;
        return false;
    }

    std::unordered_map<std::string, std::string> entries;
    std::string line;
    int line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        std::string key;
        std::string value;
        if (!parse_settings_line(line, key, value)) {
            continue;
        }
        if (!is_valid_settings_key(key)) {
            warnings_.push_back("Invalid settings key on line " + std::to_string(line_number) + ": " + key);
            continue;
        }
        entries[to_lower(key)] = value;
    }

    const std::string base_dir = Paths::get_directory(path);

    if (const auto it = entries.find("profile.name"); it != entries.end()) {
        name_ = it->second;
    } else {
        name_ = settings_name_from_path(path);
    }
    if (name_.empty()) {
        error_out = "Settings file has no profile name";
        return false;
    }

    bool machine_specified = false;
    if (const auto it = entries.find("machine"); it != entries.end()) {
        machine_specified = true;
        const auto platform = parse_machine_value(it->second, warnings_, error_out);
        if (!platform.has_value()) {
            return false;
        }
        config_data_.platform_id = *platform;
    } else {
        config_data_.platform_id = PLATFORM_APPLE_IIE_ENHANCED;
        warnings_.push_back("No machine specified; defaulting to APPLE2E_ENHANCED");
    }
    (void)machine_specified;

    if (const auto it = entries.find("gssquared.description"); it != entries.end()) {
        description_ = it->second;
    }

    clock_set_t clock_set = CLOCK_SET_US;
    if (const auto it = entries.find("gssquared.clock"); it != entries.end()) {
        const auto parsed = parse_clock(it->second, error_out);
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

    if (const auto it = entries.find("gssquared.scanner"); it != entries.end()) {
        const auto parsed = parse_scanner(it->second, error_out);
        if (!parsed.has_value()) {
            return false;
        }
        config_data_.scanner_type = *parsed;
    } else {
        config_data_.scanner_type = derive_scanner(config_data_.platform_id, clock_set);
    }

    config_data_.builtin = false;
    sync_config_pointers();

    static const std::regex slot_re(R"(^slot([0-7])$)");
    bool slot7_specified = false;
    for (const auto& [key, value] : entries) {
        std::smatch match;
        if (!std::regex_match(key, match, slot_re)) {
            continue;
        }
        const int slot = std::stoi(match[1].str());
        if (slot == kDefaultSmartportSlot) {
            slot7_specified = true;
        }
        std::string card_error;
        const auto card_id = parse_card_type(value, card_error);
        if (!card_id.has_value()) {
            warnings_.push_back("slot" + std::to_string(slot) + ": " + card_error);
            continue;
        }
        if (config_data_.slot_devices[slot] != DEVICE_ID_NONE) {
            error_out = "Duplicate slot" + std::to_string(slot) + " in Settings file";
            return false;
        }
        config_data_.slot_devices[slot] = *card_id;
    }

    ensure_default_smartport_card(config_data_, slot7_specified, warnings_);

    static const std::regex smartport_re(R"(^smartport\.disk([1-8])$)");
    static const std::regex controller_disk_re(R"(^([a-z0-9]+)\.disk([1-8])$)");

    for (const auto& [key, value] : entries) {
        std::smatch match;
        if (std::regex_match(key, match, smartport_re)) {
            const int drive_1based = std::stoi(match[1].str());
            int slot = kDefaultSmartportSlot;
            if (const auto found = find_bazfast_slot(config_data_); found.has_value()) {
                slot = *found;
            }
            disk_mount_t mount;
            mount.slot = static_cast<uint16_t>(slot);
            mount.drive = static_cast<uint16_t>(drive_1based - 1);
            mount.filename = resolve_settings_path(base_dir, value);
            upsert_mount(mounts_, mount);
            continue;
        }

        if (!std::regex_match(key, match, controller_disk_re)) {
            continue;
        }
        const std::string prefix = match[1].str();
        if (prefix == "smartport") {
            continue;
        }
        const int drive_1based = std::stoi(match[2].str());
        const int controller = floppy_controller_index(prefix);
        if (controller < 1) {
            continue;
        }

        int slot = default_floppy_slot(config_data_.platform_id);
        const auto disk_slots = find_disk_ii_slots(config_data_);
        if (!disk_slots.empty()) {
            const int index = controller - 1;
            if (index < static_cast<int>(disk_slots.size())) {
                slot = disk_slots[static_cast<size_t>(index)];
            } else {
                warnings_.push_back(prefix + ".disk" + std::to_string(drive_1based)
                                    + ": no matching floppy controller; using slot "
                                    + std::to_string(slot));
            }
        } else if (platform_is_iigs(config_data_.platform_id)) {
            slot = 6;
        } else {
            warnings_.push_back(prefix + ".disk" + std::to_string(drive_1based)
                                + ": no disk_ii card; using slot " + std::to_string(slot));
        }

        disk_mount_t mount;
        mount.slot = static_cast<uint16_t>(slot);
        mount.drive = static_cast<uint16_t>(drive_1based - 1);
        mount.filename = resolve_settings_path(base_dir, value);
        upsert_mount(mounts_, mount);
    }

    static const std::unordered_set<std::string> handled_keys = {
        "profile.name", "machine", "gssquared.description", "gssquared.clock", "gssquared.scanner",
    };

    for (const auto& [key, value] : entries) {
        if (handled_keys.count(key)) {
            continue;
        }
        if (std::regex_match(key, slot_re)) {
            continue;
        }
        if (std::regex_match(key, smartport_re)) {
            continue;
        }
        std::smatch match;
        if (std::regex_match(key, match, controller_disk_re) && match[1].str() != "smartport") {
            continue;
        }
        if (key.rfind("gssquared.", 0) == 0) {
            extensions_.emplace_back(key, value);
            continue;
        }
        if (is_preference_key(key)) {
            extensions_.emplace_back(key, value);
            warnings_.push_back("Preference key not applied at load time: " + key);
            continue;
        }
        extensions_.emplace_back(key, value);
        warnings_.push_back("Unknown settings key: " + key);
    }

    // Ephemeral machine id for Settings.txt (not persisted).
    if (id_.empty()) {
        id_ = generate_uuid_v4();
        sync_config_pointers();
    }

    return finalize_load(error_out);
}

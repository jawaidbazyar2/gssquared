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
#include <iostream>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "devices/displaypp/VideoScanner.hpp"
#include "paths.hpp"
#include "util/toml.hpp"

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
        default: return PLATFLAG_NONE;
    }
}

const char* platform_name(PlatformId_t platform) {
    switch (platform) {
        case PLATFORM_APPLE_II: return "apple2";
        case PLATFORM_APPLE_II_PLUS: return "apple2plus";
        case PLATFORM_APPLE_IIE: return "apple2e";
        case PLATFORM_APPLE_IIE_ENHANCED: return "apple2e_enhanced";
        case PLATFORM_APPLE_IIE_65816: return "apple2e_65816";
        case PLATFORM_APPLE_IIGS: return "apple2gs";
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

std::optional<PlatformId_t> parse_platform(const std::string& value, std::string& error_out) {
    static const std::unordered_map<std::string, PlatformId_t> map = {
        {"apple2", PLATFORM_APPLE_II},
        {"apple2plus", PLATFORM_APPLE_II_PLUS},
        {"apple2e", PLATFORM_APPLE_IIE},
        {"apple2e_enhanced", PLATFORM_APPLE_IIE_ENHANCED},
        {"apple2e_65816", PLATFORM_APPLE_IIE_65816},
        {"apple2gs", PLATFORM_APPLE_IIGS},
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

const char* card_type_name(device_id id) {
    switch (id) {
        case DEVICE_ID_LANGUAGE_CARD: return "language_card";
        case DEVICE_ID_PRODOS_BLOCK: return "prodos_block"; // TODO: deprecated
        case DEVICE_ID_PRODOS_CLOCK: return "prodos_clock";
        case DEVICE_ID_DISK_II: return "disk_ii";
        case DEVICE_ID_MEM_EXPANSION: return "mem_expansion";
        case DEVICE_ID_THUNDER_CLOCK: return "thunder_clock";
        case DEVICE_ID_PD_BLOCK2: return "prodos_block2"; // TODO: deprecated
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

struct device_validation_t {
    device_id id;
    bool multiple_instances;
    uint8_t slots_allowed;
    uint32_t platform_flags;
};

const device_validation_t* lookup_device_validation(device_id id) {
    static const device_validation_t kDevices[] = {
        {DEVICE_ID_LANGUAGE_CARD, false, 0b00000001, PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS},
        {DEVICE_ID_PRODOS_BLOCK, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_PRODOS_CLOCK, false, 0b11111110,
            PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
        {DEVICE_ID_DISK_II, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_MEM_EXPANSION, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_THUNDER_CLOCK, false, 0b11111110,
            PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
        {DEVICE_ID_PD_BLOCK2, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_PARALLEL, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_VIDEX, false, 0b00001000, PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS},
        {DEVICE_ID_MOCKINGBOARD, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_MOUSE, false, 0b11111110,
            PLATFLAG_APPLE_II | PLATFLAG_APPLE_II_PLUS | PLATFLAG_APPLE_IIE
            | PLATFLAG_APPLE_IIE_ENHANCED | PLATFLAG_APPLE_IIE_65816},
        {DEVICE_ID_VIDHD, false, 0b11111110, PLATFLAG_APPLE_IIE_65816},
        {DEVICE_ID_PD_BLOCK3, true, 0b11111110, PLATFLAG_ALL},
        {DEVICE_ID_SECOND_SIGHT, false, 0, PLATFLAG_APPLE_IIGS},
    };
    for (const auto& device : kDevices) {
        if (device.id == id) {
            return &device;
        }
    }
    return nullptr;
}

bool slot_allows_device(int slot, const device_validation_t* device) {
    if (slot < 0 || slot >= NUM_SLOTS) {
        return false;
    }
    if (device->slots_allowed == 0) {
        return true;
    }
    return (device->slots_allowed & (1u << slot)) != 0;
}

bool validate_cards(const SystemConfig_t& config, PlatformId_t platform,
                    const std::vector<card_extra_t>& card_extras,
                    std::vector<std::string>& warnings, std::string& error_out) {
    std::unordered_map<device_id, int> instance_counts;

    for (int slot = 0; slot < NUM_SLOTS; ++slot) {
        const device_id id = config.slot_devices[slot];
        if (id == DEVICE_ID_NONE) {
            continue;
        }

        const device_validation_t* device = lookup_device_validation(id);
        if (device == nullptr) {
            error_out = "Invalid device id in slot " + std::to_string(slot);
            return false;
        }

        const PlatformFlags_t flag = platform_flag(platform);
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
        if (!device->multiple_instances && instance_counts[id] > 1) {
            error_out = "Multiple instances of card " + std::string(card_type_name(id))
                        + " are not allowed";
            return false;
        }
    }

    for (const auto& extra : card_extras) {
        if (extra.slot < 0 || extra.slot >= NUM_SLOTS) {
            continue;
        }
        if (config.slot_devices[extra.slot] != DEVICE_ID_PARALLEL) {
            warnings.push_back("parallel output on slot " + std::to_string(extra.slot)
                               + " ignored: card is not parallel");
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
        if (!conn.slot.has_value() && platform != PLATFORM_APPLE_IIGS) {
            error_out = "Built-in serial port connections require platform apple2gs";
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

} // namespace

void SystemConfig::sync_config_pointers() {
    config_data_.name = name_.c_str();
    config_data_.description = description_.c_str();
}

void SystemConfig::clear() {
    path_.clear();
    name_.clear();
    description_.clear();
    gs2_version_ = 0;
    config_data_ = {};
    mounts_.clear();
    connections_.clear();
    card_extras_.clear();
    warnings_.clear();
    for (int i = 0; i < NUM_SLOTS; ++i) {
        config_data_.slot_devices[i] = DEVICE_ID_NONE;
    }
    config_data_.builtin = false;
    sync_config_pointers();
}

bool SystemConfig::load(const std::string& path, std::string& error_out) {
    clear();
    path_ = path;

    toml::table table;
    try {
        table = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        error_out = std::string("TOML parse error: ") + err.what();
        return false;
    }

    const auto version_node = table["gs2_version"];
    if (!version_node.is_integer()) {
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

    if (config_data_.platform_id == PLATFORM_APPLE_IIGS && clock_set == CLOCK_SET_PAL) {
        error_out = "clock=pal is not valid for platform apple2gs";
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

    if (!validate_cards(config_data_, config_data_.platform_id, card_extras_, warnings_, error_out)) {
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

void SystemConfig::dump(std::ostream& out) const {
    out << "SystemConfig: " << path_ << "\n";
    out << "  gs2_version: " << gs2_version_ << "\n";
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
}

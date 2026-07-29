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

#include <functional>
#include <optional>
#include <string>
#include <cctype>
#include <unordered_map>
#include <vector>

class SerialDevice;
class EventQueue;
class DeviceFrameDispatcher;

/** Firmware-aligned port key: IIgs SCC A=slot1, B=slot2; cards use their expansion slot. */
struct connection_key_t {
    int slot = 0;
    std::string port = "a";

    bool operator==(const connection_key_t &other) const {
        return slot == other.slot && port == other.port;
    }
    bool operator!=(const connection_key_t &other) const {
        return !(*this == other);
    }
};

struct connection_key_hash {
    size_t operator()(const connection_key_t &key) const {
        return std::hash<int>()(key.slot) ^ (std::hash<std::string>()(key.port) << 1);
    }
};

enum class connection_port_kind_t {
    SERIAL,
    PARALLEL,
};

enum class connection_device_type_t {
    NONE,
    FILE,
    MODEM,
};

using ConnectionAttachFn = std::function<void(SerialDevice *device)>;

struct connection_port_info_t {
    connection_key_t key;
    std::string display_name;
    connection_port_kind_t kind = connection_port_kind_t::SERIAL;
    connection_device_type_t device = connection_device_type_t::NONE;
};

/** Spec for building a serial-port UI button (live OSD or config editor). */
struct connection_port_spec_t {
    connection_key_t key;
    std::string display_name;
    connection_port_kind_t kind = connection_port_kind_t::SERIAL;
    connection_device_type_t device = connection_device_type_t::NONE;
};

/**
 * Normalize a TOML [[connections]] address to the canonical slot form.
 * Legacy bare port "a"/"b" (no slot) → IIgs firmware slots 1/2 with port "a".
 */
inline connection_key_t normalize_connection_key(const std::optional<int> &slot,
                                                 const std::string &port) {
    connection_key_t key;
    std::string p = port.empty() ? "a" : port;
    for (char &c : p) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (slot.has_value()) {
        key.slot = *slot;
        key.port = "a";
        return key;
    }

    if (p == "b") {
        key.slot = 2;
        key.port = "a";
    } else {
        key.slot = 1;
        key.port = "a";
    }
    return key;
}

inline connection_key_t normalize_connection_key(int slot, const std::string &port) {
    return normalize_connection_key(std::optional<int>(slot), port);
}

inline const char *connection_device_type_name(connection_device_type_t type) {
    switch (type) {
        case connection_device_type_t::NONE:  return "none";
        case connection_device_type_t::FILE:  return "file";
        case connection_device_type_t::MODEM: return "modem";
    }
    return "none";
}

inline connection_device_type_t connection_device_type_from_string(const std::string &s) {
    if (s == "file") return connection_device_type_t::FILE;
    if (s == "modem") return connection_device_type_t::MODEM;
    return connection_device_type_t::NONE;
}

inline bool connection_device_allowed(connection_port_kind_t kind, connection_device_type_t type) {
    if (type == connection_device_type_t::NONE || type == connection_device_type_t::FILE) {
        return true;
    }
    if (type == connection_device_type_t::MODEM) {
        return kind == connection_port_kind_t::SERIAL;
    }
    return false;
}

inline std::vector<connection_device_type_t> connection_allowed_devices(connection_port_kind_t kind) {
    if (kind == connection_port_kind_t::PARALLEL) {
        return {connection_device_type_t::NONE, connection_device_type_t::FILE};
    }
    return {
        connection_device_type_t::NONE,
        connection_device_type_t::FILE,
        connection_device_type_t::MODEM,
    };
}

class Connections {
    struct port_registration_t {
        std::string display_name;
        connection_port_kind_t kind = connection_port_kind_t::SERIAL;
        connection_device_type_t default_device = connection_device_type_t::NONE;
        connection_device_type_t current_device = connection_device_type_t::NONE;
        bool attached_once = false; // true after any attach(), including explicit none
        ConnectionAttachFn attach_fn;
        std::string port_id;
        SerialDevice *device = nullptr;
    };

    EventQueue *event_queue_ = nullptr;
    DeviceFrameDispatcher *device_frame_dispatcher_ = nullptr;
    std::unordered_map<connection_key_t, port_registration_t, connection_key_hash> ports_;
    mutable std::vector<connection_port_info_t> cached_ports_;

    SerialDevice *create_device(connection_device_type_t type, const std::string &port_id);

public:
    Connections(EventQueue *event_queue, DeviceFrameDispatcher *device_frame_dispatcher);
    ~Connections();

    int register_port(connection_key_t key,
                      const std::string &display_name,
                      connection_port_kind_t kind,
                      connection_device_type_t default_device,
                      ConnectionAttachFn attach_fn,
                      const std::string &port_id);

    bool attach(connection_key_t key, connection_device_type_t type);
    connection_device_type_t current_device(connection_key_t key) const;
    std::vector<connection_device_type_t> allowed_devices(connection_key_t key) const;
    const std::vector<connection_port_info_t> &get_all_ports() const;

    /** Apply defaults to ports that still have NONE (no config row applied yet). */
    void apply_defaults();

    void clear();
};

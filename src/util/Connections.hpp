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

#include <cstdlib>
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
    CLIPBOARD,
    MODEM,
    SERIAL,
    PRINTER,
    NET_PRINTER,
};

using ConnectionAttachFn = std::function<void(SerialDevice *device, connection_device_type_t type)>;

struct connection_port_info_t {
    connection_key_t key;
    std::string display_name;
    connection_port_kind_t kind = connection_port_kind_t::SERIAL;
    connection_device_type_t device = connection_device_type_t::NONE;
    std::string path;
    std::string host;
    int tcp_port = 0;
};

/** Spec for building a serial-port UI button (live OSD or config editor). */
struct connection_port_spec_t {
    connection_key_t key;
    std::string display_name;
    connection_port_kind_t kind = connection_port_kind_t::SERIAL;
    connection_device_type_t device = connection_device_type_t::NONE;
    std::string path;
    std::string host;
    int tcp_port = 0;
    /** SSC and IIgs SCC. Printer is a file capture plus gs2print; SSC also sets SIC P8. */
    bool offers_printer = false;
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
        case connection_device_type_t::NONE:      return "none";
        case connection_device_type_t::FILE:      return "file";
        case connection_device_type_t::CLIPBOARD: return "clipboard";
        case connection_device_type_t::MODEM:     return "modem";
        case connection_device_type_t::SERIAL:    return "serial";
        case connection_device_type_t::PRINTER:     return "printer";
        case connection_device_type_t::NET_PRINTER: return "netprinter";
    }
    return "none";
}

inline connection_device_type_t connection_device_type_from_string(const std::string &s) {
    if (s == "file") return connection_device_type_t::FILE;
    if (s == "clipboard") return connection_device_type_t::CLIPBOARD;
    if (s == "modem") return connection_device_type_t::MODEM;
    if (s == "serial") return connection_device_type_t::SERIAL;
    if (s == "printer") return connection_device_type_t::PRINTER;
    if (s == "netprinter") return connection_device_type_t::NET_PRINTER;
    return connection_device_type_t::NONE;
}

inline bool connection_device_allowed(connection_port_kind_t kind, connection_device_type_t type,
                                      bool offers_printer = false) {
    if (type == connection_device_type_t::PRINTER) {
        return offers_printer;
    }
    if (type == connection_device_type_t::NONE ||
        type == connection_device_type_t::FILE ||
        type == connection_device_type_t::CLIPBOARD) {
        return true;
    }
    if (type == connection_device_type_t::MODEM) {
        return kind == connection_port_kind_t::SERIAL;
    }
#if !defined(__EMSCRIPTEN__)
    if (type == connection_device_type_t::SERIAL ||
        type == connection_device_type_t::NET_PRINTER) {
        return kind == connection_port_kind_t::SERIAL;
    }
#endif
    return false;
}

inline std::vector<connection_device_type_t> connection_allowed_devices(connection_port_kind_t kind,
                                                                        bool offers_printer = false) {
    std::vector<connection_device_type_t> devices = {
        connection_device_type_t::NONE,
        connection_device_type_t::FILE,
    };
    if (offers_printer) {
        devices.push_back(connection_device_type_t::PRINTER);
    }
    devices.push_back(connection_device_type_t::CLIPBOARD);
    if (kind != connection_port_kind_t::PARALLEL) {
        devices.push_back(connection_device_type_t::MODEM);
#if !defined(__EMSCRIPTEN__)
        devices.push_back(connection_device_type_t::NET_PRINTER);
#endif
    }
    return devices;
}

class Connections {
    struct port_registration_t {
        std::string display_name;
        connection_port_kind_t kind = connection_port_kind_t::SERIAL;
        connection_device_type_t default_device = connection_device_type_t::NONE;
        connection_device_type_t current_device = connection_device_type_t::NONE;
        bool attached_once = false; // true after any attach(), including explicit none
        bool offers_printer = false;
        ConnectionAttachFn attach_fn;
        std::string port_id;
        std::string path;
        std::string host;
        int tcp_port = 0;
        SerialDevice *device = nullptr;
    };

    EventQueue *event_queue_ = nullptr;
    DeviceFrameDispatcher *device_frame_dispatcher_ = nullptr;
    std::unordered_map<connection_key_t, port_registration_t, connection_key_hash> ports_;
    mutable std::vector<connection_port_info_t> cached_ports_;

    SerialDevice *create_device(connection_device_type_t type, const std::string &port_id,
                                const std::string &path, const std::string &host, int tcp_port);

public:
    Connections(EventQueue *event_queue, DeviceFrameDispatcher *device_frame_dispatcher);
    ~Connections();

    int register_port(connection_key_t key,
                      const std::string &display_name,
                      connection_port_kind_t kind,
                      connection_device_type_t default_device,
                      ConnectionAttachFn attach_fn,
                      const std::string &port_id,
                      bool offers_printer = false);

    bool attach(connection_key_t key, connection_device_type_t type,
                const std::string &path = {}, const std::string &host = {}, int tcp_port = 0);
    connection_device_type_t current_device(connection_key_t key) const;
    SerialDevice *device(connection_key_t key) const;
    std::vector<connection_device_type_t> allowed_devices(connection_key_t key) const;
    const std::vector<connection_port_info_t> &get_all_ports() const;

    /** Apply defaults to ports that still have NONE (no config row applied yet). */
    void apply_defaults();

    void clear();
};

inline constexpr int kNetPrinterDefaultPort = 9100;

/** `host:port`, or `[host]:port` when host is an IPv6 literal. Empty if host is unset. */
inline std::string format_net_printer_endpoint(const std::string &host, int tcp_port) {
    if (host.empty() || tcp_port <= 0) {
        return {};
    }
    if (host.find(':') != std::string::npos) {
        return "[" + host + "]:" + std::to_string(tcp_port);
    }
    return host + ":" + std::to_string(tcp_port);
}

inline std::string trim_copy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

inline bool parse_tcp_port(const std::string &text, int &out) {
    const std::string s = trim_copy(text);
    if (s.empty()) {
        return false;
    }
    char *end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || (end && *end != '\0')) {
        return false;
    }
    if (v < 1 || v > 65535) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct host_serial_info_t {
    std::string path;    // stored in TOML as-is
    std::string display; // basename / friendly label for OSD
};

enum host_serial_parity_t : uint8_t {
    HOST_SERIAL_PARITY_NONE = 0,
    HOST_SERIAL_PARITY_EVEN = 1,
    HOST_SERIAL_PARITY_ODD = 2,
    HOST_SERIAL_PARITY_MARK = 3,
    HOST_SERIAL_PARITY_SPACE = 4,
};

struct host_serial_line_t {
    uint32_t baud = 9600;
    uint8_t data_bits = 8;  // 5–8
    uint8_t stop_bits = 1;  // 1, 2, or 15 = 1.5
    uint8_t parity = HOST_SERIAL_PARITY_NONE;
};

/** Pack line settings into SerialMessage.data (MESSAGE_LINE). */
inline uint64_t host_serial_pack_line(const host_serial_line_t &line) {
    uint64_t data = line.baud;
    data |= static_cast<uint64_t>(line.data_bits & 0x0F) << 32;
    data |= static_cast<uint64_t>(line.stop_bits & 0x0F) << 36;
    data |= static_cast<uint64_t>(line.parity & 0x0F) << 40;
    return data;
}

inline host_serial_line_t host_serial_unpack_line(uint64_t data) {
    host_serial_line_t line;
    line.baud = static_cast<uint32_t>(data);
    line.data_bits = static_cast<uint8_t>((data >> 32) & 0x0F);
    line.stop_bits = static_cast<uint8_t>((data >> 36) & 0x0F);
    line.parity = static_cast<uint8_t>((data >> 40) & 0x0F);
    return line;
}

inline std::string host_serial_basename(const std::string &path) {
    const auto pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    if (name.size() > 22) {
        name.resize(19);
        name += "...";
    }
    return name;
}

/**
 * Non-blocking host UART. attach/send/receive never block the caller.
 * Platform .cpp supplies the methods and host_serial_enumerate().
 */
class HostSerial {
    int fd_ = -1;

public:
    HostSerial() = default;
    ~HostSerial();
    HostSerial(const HostSerial &) = delete;
    HostSerial &operator=(const HostSerial &) = delete;

    bool attach(const char *path);
    void detach();
    bool configure(const host_serial_line_t &line);
    int send(const uint8_t *data, int n);
    int receive(uint8_t *data, int n);
    bool is_attached() const { return fd_ >= 0; }
};

std::vector<host_serial_info_t> host_serial_enumerate();

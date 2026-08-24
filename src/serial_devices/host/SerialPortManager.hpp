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
#include <vector>

#include "serial_devices/host/HostSerial.hpp"

/** Cached host-port list. Call poll() while UI is up; it refreshes every 2 s. */
class SerialPortManager {
    std::vector<host_serial_info_t> ports_;
    uint64_t generation_ = 0;
    uint32_t last_ms_ = 0;
    static constexpr uint32_t kIntervalMs = 2000;

public:
    bool poll(uint32_t now_ms) {
        if (last_ms_ != 0 && (now_ms - last_ms_) < kIntervalMs) {
            return false;
        }
        last_ms_ = now_ms;
        std::vector<host_serial_info_t> next = host_serial_enumerate();
        bool same = (generation_ != 0) && (next.size() == ports_.size());
        if (same) {
            for (size_t i = 0; i < next.size(); i++) {
                if (next[i].path != ports_[i].path) {
                    same = false;
                    break;
                }
            }
        }
        if (same) {
            return false;
        }
        ports_ = std::move(next);
        generation_++;
        return true;
    }

    const std::vector<host_serial_info_t> &ports() const { return ports_; }
    uint64_t generation() const { return generation_; }
};

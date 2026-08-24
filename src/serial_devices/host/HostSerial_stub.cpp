/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "serial_devices/host/HostSerial.hpp"

HostSerial::~HostSerial() {
    detach();
}

bool HostSerial::attach(const char * /*path*/) {
    return false;
}

void HostSerial::detach() {
    fd_ = -1;
}

bool HostSerial::configure(const host_serial_line_t & /*line*/) {
    return false;
}

int HostSerial::send(const uint8_t * /*data*/, int /*n*/) {
    return -1;
}

int HostSerial::receive(uint8_t * /*data*/, int /*n*/) {
    return -1;
}

std::vector<host_serial_info_t> host_serial_enumerate() {
    return {};
}

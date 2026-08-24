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

#include "Connections.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>

#include "serial_devices/SerialDevice.hpp"
#include "serial_devices/clipboard/ClipboardDevice.hpp"
#include "serial_devices/file/FileDevice.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "util/EventQueue.hpp"
#if !defined(__EMSCRIPTEN__)
#include "serial_devices/modem/ModemDevice.hpp"
#include "serial_devices/serial/SerialPortDevice.hpp"
#endif

Connections::Connections(EventQueue *event_queue, DeviceFrameDispatcher *device_frame_dispatcher)
    : event_queue_(event_queue), device_frame_dispatcher_(device_frame_dispatcher) {}

Connections::~Connections() {
    clear();
}

SerialDevice *Connections::create_device(connection_device_type_t type, const std::string &port_id,
                                         const std::string &path) {
    switch (type) {
        case connection_device_type_t::NONE:
            return nullptr;
        case connection_device_type_t::FILE:
            return new FileDevice(event_queue_, device_frame_dispatcher_, port_id.c_str());
        case connection_device_type_t::CLIPBOARD:
            return new ClipboardDevice(event_queue_, device_frame_dispatcher_, port_id.c_str());
        case connection_device_type_t::MODEM:
#if defined(__EMSCRIPTEN__)
            // No SDL_net/modem on the web; fall back to file capture.
            return new FileDevice(event_queue_, device_frame_dispatcher_, port_id.c_str());
#else
            return new ModemDevice(nullptr, port_id.c_str());
#endif
        case connection_device_type_t::SERIAL:
#if defined(__EMSCRIPTEN__)
            return nullptr;
#else
            return new SerialPortDevice(event_queue_, device_frame_dispatcher_,
                                        port_id.c_str(), path);
#endif
    }
    return nullptr;
}

int Connections::register_port(connection_key_t key,
                               const std::string &display_name,
                               connection_port_kind_t kind,
                               connection_device_type_t default_device,
                               ConnectionAttachFn attach_fn,
                               const std::string &port_id) {
    if (ports_.count(key)) {
        std::cerr << "Connections: duplicate register for slot " << key.slot
                  << " port " << key.port << "\n";
        return -1;
    }
    if (!connection_device_allowed(kind, default_device) &&
        default_device != connection_device_type_t::NONE) {
        default_device = connection_device_type_t::FILE;
    }

    port_registration_t reg;
    reg.display_name = display_name;
    reg.kind = kind;
    reg.default_device = default_device;
    reg.current_device = connection_device_type_t::NONE;
    reg.attach_fn = std::move(attach_fn);
    reg.port_id = port_id;
    reg.device = nullptr;
    ports_[key] = std::move(reg);
    return 0;
}

bool Connections::attach(connection_key_t key, connection_device_type_t type,
                         const std::string &path) {
    auto it = ports_.find(key);
    if (it == ports_.end()) {
        std::cerr << "Connections: no port registered at slot " << key.slot
                  << " port " << key.port << "\n";
        return false;
    }
    port_registration_t &reg = it->second;
#if defined(__EMSCRIPTEN__)
    if (type == connection_device_type_t::SERIAL) {
        type = connection_device_type_t::NONE;
    }
#endif
    if (!connection_device_allowed(reg.kind, type)) {
        std::cerr << "Connections: device " << connection_device_type_name(type)
                  << " not allowed on " << reg.display_name << "\n";
        return false;
    }

    // Unwire and destroy previous device first.
    if (reg.attach_fn) {
        reg.attach_fn(nullptr);
    }
    delete reg.device;
    reg.device = nullptr;
    reg.current_device = connection_device_type_t::NONE;
    reg.path.clear();

    SerialDevice *dev = create_device(type, reg.port_id, path);
    reg.device = dev;
    reg.current_device = type;
    reg.path = (type == connection_device_type_t::SERIAL) ? path : std::string{};
    reg.attached_once = true;
    if (reg.attach_fn) {
        reg.attach_fn(dev);
    }
    return true;
}

connection_device_type_t Connections::current_device(connection_key_t key) const {
    auto it = ports_.find(key);
    if (it == ports_.end()) {
        return connection_device_type_t::NONE;
    }
    return it->second.current_device;
}

std::vector<connection_device_type_t> Connections::allowed_devices(connection_key_t key) const {
    auto it = ports_.find(key);
    if (it == ports_.end()) {
        return {};
    }
    return connection_allowed_devices(it->second.kind);
}

const std::vector<connection_port_info_t> &Connections::get_all_ports() const {
    cached_ports_.clear();
    for (const auto &kv : ports_) {
        connection_port_info_t info;
        info.key = kv.first;
        info.display_name = kv.second.display_name;
        info.kind = kv.second.kind;
        info.device = kv.second.current_device;
        info.path = kv.second.path;
        cached_ports_.push_back(std::move(info));
    }
    std::sort(cached_ports_.begin(), cached_ports_.end(),
              [](const connection_port_info_t &a, const connection_port_info_t &b) {
                  if (a.key.slot != b.key.slot) return a.key.slot < b.key.slot;
                  return a.key.port < b.key.port;
              });
    return cached_ports_;
}

void Connections::apply_defaults() {
    // Snapshot keys: attach() mutates ports_ safely by key, but avoid iterator invalidation surprises.
    std::vector<connection_key_t> keys;
    keys.reserve(ports_.size());
    for (const auto &kv : ports_) {
        if (!kv.second.attached_once) {
            keys.push_back(kv.first);
        }
    }
    for (const connection_key_t &key : keys) {
        auto it = ports_.find(key);
        if (it == ports_.end() || it->second.attached_once) {
            continue;
        }
        attach(key, it->second.default_device);
    }
}

void Connections::clear() {
    for (auto &kv : ports_) {
        port_registration_t &reg = kv.second;
        if (reg.attach_fn) {
            reg.attach_fn(nullptr);
        }
        delete reg.device;
        reg.device = nullptr;
        reg.current_device = connection_device_type_t::NONE;
        reg.path.clear();
    }
    ports_.clear();
    cached_ports_.clear();
}

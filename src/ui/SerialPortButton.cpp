/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "SerialPortButton.hpp"

#include "serial_devices/host/HostSerial.hpp"

namespace {

std::string device_short_label(connection_device_type_t d, const std::string &path) {
    switch (d) {
        case connection_device_type_t::FILE:      return "File";
        case connection_device_type_t::CLIPBOARD: return "Clip";
        case connection_device_type_t::MODEM:     return "Modem";
        case connection_device_type_t::SERIAL:    return host_serial_basename(path);
        case connection_device_type_t::NONE:
        default:                             return "—";
    }
}

} // namespace

SerialPortButton::SerialPortButton(UIContext *ctx, const Style_t &style)
    : Button_t(ctx, "Port", style) {
    size(215, 36);
}

void SerialPortButton::refresh_label() {
    text = port_name_ + "  [" + device_short_label(device_, path_) + "]";
    set_content_size_from_text();
    // Fixed tile size so a long host-port name does not grow the button or
    // skip recentering (content origin is LEFT-aligned in Button_t::render).
    size(215, 36);
    position_content(CP_CENTER, CP_CENTER);
}

void SerialPortButton::set_port(const connection_port_spec_t &spec) {
    key_ = spec.key;
    kind_ = spec.kind;
    device_ = spec.device;
    port_name_ = spec.display_name;
    path_ = spec.path;
    refresh_label();
}

void SerialPortButton::set_device(connection_device_type_t device) {
    device_ = device;
    refresh_label();
}

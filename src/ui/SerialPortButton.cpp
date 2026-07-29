/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "SerialPortButton.hpp"

namespace {

const char *device_short_label(connection_device_type_t d) {
    switch (d) {
        case connection_device_type_t::FILE:  return "File";
        case connection_device_type_t::MODEM: return "Modem";
        case connection_device_type_t::NONE:
        default:                             return "—";
    }
}

} // namespace

SerialPortButton::SerialPortButton(UIContext *ctx, const Style_t &style)
    : Button_t(ctx, "Port", style) {
    size(200, 36);
}

void SerialPortButton::refresh_label() {
    text = port_name_ + "  [" + device_short_label(device_) + "]";
    set_content_size_from_text();
    // Keep a stable button width for the panel column.
    if (cp.w < 180) {
        size(200, cp.h > 0 ? cp.h : 36);
    }
}

void SerialPortButton::set_port(const connection_port_spec_t &spec) {
    key_ = spec.key;
    kind_ = spec.kind;
    device_ = spec.device;
    port_name_ = spec.display_name;
    refresh_label();
}

void SerialPortButton::set_device(connection_device_type_t device) {
    device_ = device;
    refresh_label();
}

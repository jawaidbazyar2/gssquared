/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <string>

#include "Button.hpp"
#include "util/Connections.hpp"

/** Control-panel button for one serial/parallel port attachment. */
class SerialPortButton : public Button_t {
    connection_key_t key_{};
    connection_port_kind_t kind_ = connection_port_kind_t::SERIAL;
    connection_device_type_t device_ = connection_device_type_t::NONE;
    std::string port_name_;
    std::string path_;

    void refresh_label();

public:
    SerialPortButton(UIContext *ctx, const Style_t &style);

    void set_port(const connection_port_spec_t &spec);
    connection_key_t get_key() const { return key_; }
    connection_port_kind_t get_kind() const { return kind_; }
    connection_device_type_t get_device() const { return device_; }
    void set_device(connection_device_type_t device);
};

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <functional>
#include <vector>

#include "Container.hpp"
#include "SerialPortButton.hpp"
#include "util/Connections.hpp"

using SerialPortClickHandler =
    std::function<void(SerialPortButton *button, const SDL_Event &event)>;

/** Vertical stack of serial/parallel port buttons for the Control Panel. */
class SerialPortsOSD_t : public Container_t {
protected:
    std::vector<SerialPortButton *> buttons;
    Style_t button_style;
    SerialPortClickHandler click_handler;

public:
    SerialPortsOSD_t(UIContext *ctx, const Style_t &initial_style);
    ~SerialPortsOSD_t() override;

    void set_button_style(const Style_t &style) { button_style = style; }
    void set_click_handler(SerialPortClickHandler handler) { click_handler = std::move(handler); }

    void rebuild(const std::vector<connection_port_spec_t> &ports);
    void layout() override;

    SerialPortButton *find_button(connection_key_t key);
};

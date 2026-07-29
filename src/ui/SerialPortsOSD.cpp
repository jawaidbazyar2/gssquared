/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "SerialPortsOSD.hpp"

void SerialPortsOSD_t::layout() {
    if (!visible || tiles.empty()) return;

    float y = tp.y + static_cast<float>(style.padding);
    const float x = tp.x + static_cast<float>(style.padding);
    for (size_t i = 0; i < tiles.size(); i++) {
        if (!tiles[i] || !tiles[i]->is_visible()) continue;
        float tw = 0, th = 0;
        tiles[i]->get_tile_size(&tw, &th);
        tiles[i]->set_position(x, y);
        y += th + static_cast<float>(style.padding);
    }
}

SerialPortsOSD_t::SerialPortsOSD_t(UIContext *ctx, const Style_t &initial_style)
    : Container_t(ctx, initial_style), button_style(initial_style) {}

SerialPortsOSD_t::~SerialPortsOSD_t() {
    buttons.clear();
}

void SerialPortsOSD_t::rebuild(const std::vector<connection_port_spec_t> &ports) {
    for (SerialPortButton *button : buttons) {
        delete button;
    }
    buttons.clear();
    remove_all_tiles();

    for (const auto &port : ports) {
        SerialPortButton *button = new SerialPortButton(ctx, button_style);
        button->set_port(port);
        if (click_handler) {
            button->on_click([this, button](const SDL_Event &event) -> bool {
                click_handler(button, event);
                return true;
            });
        }
        buttons.push_back(button);
        add(button);
    }
    layout();
}

SerialPortButton *SerialPortsOSD_t::find_button(connection_key_t key) {
    for (SerialPortButton *b : buttons) {
        if (b && b->get_key() == key) return b;
    }
    return nullptr;
}

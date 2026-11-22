/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <functional>
#include <string>

#include "Tile.hpp"
#include "util/Connections.hpp"

class TextRenderer;

/**
 * Rounded-rect port sub-container card for the Connections column
 * (Control Panel and Config Editor).
 *
 * Header shows `<Port>  <Device>` with a compact Pencil icon button to change
 * what device is attached. Below a hairline divider sits the device-specific
 * body (collapsed when attachment is None), with a second Pencil icon button
 * for configurable targets (Net Printer host:port, Host Serial UART).
 */
class SerialPortButton : public Tile_t {
public:
    using ActionCallback = std::function<void()>;
    using StatusProvider = std::function<std::pair<std::string, bool>(
        connection_key_t key, connection_device_type_t device)>;

private:
    connection_port_spec_t spec_{};
    std::string short_port_name_;
    bool is_ssc_ = false;
    bool show_live_status_ = false;
    bool active_highlight_ = false;
    bool hover_header_pencil_ = false;
    bool hover_body_pencil_ = false;
    TextRenderer *small_tr_ = nullptr;
    ActionCallback edit_assignment_cb_;
    ActionCallback edit_detail_cb_;
    StatusProvider status_provider_;

    float calc_card_height() const;
    SDL_FRect header_pencil_rect() const;
    SDL_FRect body_pencil_rect() const;
    bool has_body_pencil() const;
    std::string file_pattern_label() const;

public:
    SerialPortButton(UIContext *ctx, const Style_t &style);

    void set_small_text_renderer(TextRenderer *tr) { small_tr_ = tr; }
    void set_show_live_status(bool show);
    void set_status_provider(StatusProvider provider) { status_provider_ = std::move(provider); }
    void set_active_highlight(bool active) { active_highlight_ = active; }

    void set_port(const connection_port_spec_t &spec);
    void set_device(connection_device_type_t device);

    void on_edit_assignment(ActionCallback cb) { edit_assignment_cb_ = std::move(cb); }
    void on_edit_detail(ActionCallback cb) { edit_detail_cb_ = std::move(cb); }

    const connection_port_spec_t &spec() const { return spec_; }
    connection_key_t get_key() const { return spec_.key; }
    connection_port_kind_t get_kind() const { return spec_.kind; }
    connection_device_type_t get_device() const { return spec_.device; }
    const std::string &get_host() const { return spec_.host; }
    int get_tcp_port() const { return spec_.tcp_port; }
    const std::string &short_port_name() const { return short_port_name_; }

    void render() override;
    bool handle_mouse_event(const SDL_Event &event) override;
};

/** Short display label for a connection device type (e.g. "Net Printer"). */
const char *connection_device_display_label(connection_device_type_t type);

/** Compact port name for card headers ("Serial Slot 1" -> "Port A", "SSC Slot 2" -> "Slot 2"). */
std::string connection_short_port_label(const std::string &display_name);

/** Front-crop UTF-8 text with a leading ellipsis so it fits in `max_w` pixels. */
std::string ui_crop_text_front(TextRenderer *tr, const std::string &text, float max_w);

/** End-crop UTF-8 text with a trailing ellipsis so it fits in `max_w` pixels. */
std::string ui_crop_text_end(TextRenderer *tr, const std::string &text, float max_w);

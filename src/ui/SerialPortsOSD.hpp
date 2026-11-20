/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Container.hpp"
#include "SerialPortButton.hpp"
#include "TextInput.hpp"
#include "serial_devices/host/SerialPortManager.hpp"
#include "util/Connections.hpp"

class TextRenderer;

using ConnectionApplyHandler = std::function<void(
    connection_key_t key,
    connection_device_type_t type,
    const std::string &path,
    const std::string &host,
    int tcp_port)>;

/**
 * Connections panel for the Control Panel (F4) and Config Editor.
 *
 * Groups ports into Serial Ports and Parallel Ports sections, renders each
 * port as a rounded-rect sub-container card with Pencil actions, and manages
 * all edit overlays (device assignment, Net Printer host:port, Host Serial
 * UART picker) strictly inside its own bounding rectangle so they never
 * overlap neighboring panels (e.g. Drives).
 */
class SerialPortsOSD_t : public Container_t {
    enum class OverlayMode {
        NONE,
        ASSIGN,
        NET_PRINTER,
        HOST_SERIAL,
    };

    std::vector<SerialPortButton *> cards_;
    std::vector<connection_port_spec_t> specs_;
    Style_t card_style_{};
    TextRenderer *small_tr_ = nullptr;

    ConnectionApplyHandler apply_handler_;
    SerialPortButton::StatusProvider status_provider_;
    std::function<void()> overlay_open_handler_;
    SerialPortManager *serial_port_manager_ = nullptr;
    bool show_live_status_ = false;

    bool has_serial_section_ = false;
    bool has_parallel_section_ = false;
    bool has_host_folder_section_ = false;
    float serial_header_y_ = 0.0f;
    float parallel_header_y_ = 0.0f;
    float host_folder_header_y_ = 0.0f;
    SDL_FRect host_folder_card_{0, 0, 0, 0};
    bool hover_host_folder_pencil_ = false;
    std::function<std::string()> host_folder_path_provider_;
    std::function<void()> host_folder_pick_cb_;

    // In-bounds overlay state
    OverlayMode overlay_mode_ = OverlayMode::NONE;
    connection_key_t overlay_key_{};
    bool overlay_from_assign_ = false;
    SDL_FRect overlay_sheet_{0, 0, 0, 0};

    std::vector<connection_device_type_t> assign_choices_;
    std::vector<host_serial_info_t> host_serial_ports_;
    uint64_t host_serial_gen_ = 0;

    TextInput_t *net_host_input_ = nullptr;
    TextInput_t *net_port_input_ = nullptr;
    SDL_FRect cancel_btn_rect_{0, 0, 0, 0};
    SDL_FRect apply_btn_rect_{0, 0, 0, 0};

    int hover_row_ = -1;
    bool hover_close_ = false;
    bool hover_back_ = false;
    bool hover_cancel_ = false;
    bool hover_apply_ = false;

    const connection_port_spec_t *find_spec(connection_key_t key) const;
    void open_assign_overlay(connection_key_t key);
    void open_net_printer_overlay(connection_key_t key, bool from_assign);
    void open_host_serial_overlay(connection_key_t key, bool from_assign);
    void clear_net_inputs();
    void layout_overlay();
    void confirm_net_printer();
    bool net_printer_valid() const;

    SDL_FRect overlay_close_rect() const;
    SDL_FRect overlay_back_rect() const;
    SDL_FRect overlay_row_rect(int index) const;
    SDL_FRect host_folder_pencil_rect() const;

    void render_host_folder_card();
    void render_overlay();
    void apply_and_dismiss(connection_key_t key, connection_device_type_t type,
                           const std::string &path = {}, const std::string &host = {},
                           int tcp_port = 0);

public:
    SerialPortsOSD_t(UIContext *ctx, const Style_t &initial_style);
    ~SerialPortsOSD_t() override;

    void set_button_style(const Style_t &style) { card_style_ = style; }
    void set_apply_handler(ConnectionApplyHandler handler) { apply_handler_ = std::move(handler); }
    void set_status_provider(SerialPortButton::StatusProvider provider);
    void set_serial_port_manager(SerialPortManager *mgr) { serial_port_manager_ = mgr; }
    void set_overlay_open_handler(std::function<void()> cb) { overlay_open_handler_ = std::move(cb); }
    void set_host_folder_callbacks(std::function<std::string()> path_provider,
                                   std::function<void()> on_pick);

    void rebuild(const std::vector<connection_port_spec_t> &ports);
    void layout() override;
    void render() override;
    void update() override;
    bool handle_mouse_event(const SDL_Event &event) override;

    /** Handle mouse and keyboard events while an in-bounds overlay is active. */
    bool handle_overlay_event(const SDL_Event &event);
    bool has_overlay() const { return overlay_mode_ != OverlayMode::NONE; }
    void dismiss_overlay();
    bool needs_redraw() const;

    SerialPortButton *find_button(connection_key_t key);
};

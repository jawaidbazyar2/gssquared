/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "SerialPortsOSD.hpp"

#include <algorithm>
#include <cstdio>

#include "VectorIcons.hpp"
#include "serial_devices/host/HostSerial.hpp"
#include "util/TextRenderer.hpp"

namespace {

constexpr float kOuterPad = 6.0f;
constexpr float kSectionHeaderH = 20.0f;
constexpr float kSectionHeaderGap = 4.0f;
constexpr float kSectionBreakGap = 10.0f;
constexpr float kCardGap = 6.0f;

constexpr float kSheetInset = 6.0f;
constexpr float kSheetPad = 8.0f;
constexpr float kSheetRadius = 7.0f;
constexpr float kSheetHeaderH = 22.0f;
constexpr float kOverlayIconBtn = 20.0f;
constexpr float kRowH = 27.0f;
constexpr float kRowGap = 3.0f;
constexpr float kRowRadius = 4.0f;
constexpr float kFieldH = 28.0f;
constexpr float kActionBtnH = 28.0f;

constexpr uint32_t kScrimColor        = 0x002614B8;
constexpr uint32_t kSheetBg           = 0x0D466BF8;
constexpr uint32_t kSheetBorder       = 0xF0F7FAFF;
constexpr uint32_t kSheetDivider      = 0xFFFFFF55;
constexpr uint32_t kSectionLabelColor = 0xE8FFF0FF;

constexpr uint32_t kRowBg             = 0x006A9ECC;
constexpr uint32_t kRowHoverBg        = 0xE36414FF;
constexpr uint32_t kRowSelectedBg     = 0x0084C6FF;
constexpr uint32_t kRowSelectedBorder = 0xFFFFFFFF;
constexpr uint32_t kCheckColor        = 0xFFD166FF;

constexpr uint32_t kIconBg            = 0x005B8ECC;
constexpr uint32_t kIconHoverBg       = 0xE36414FF;

constexpr uint32_t kCancelBg          = 0x005B8EFF;
constexpr uint32_t kCancelHoverBg     = 0x0072B0FF;
constexpr uint32_t kApplyBg           = 0x008A44FF;
constexpr uint32_t kApplyHoverBg      = 0x00A854FF;
constexpr uint32_t kApplyDisabledBg   = 0x26546FCC;

bool hit_rect(float x, float y, SDL_FRect r, float inflate = 0.0f) {
    return x >= (r.x - inflate) && x <= (r.x + r.w + inflate) &&
           y >= (r.y - inflate) && y <= (r.y + r.h + inflate);
}

void render_text(TextRenderer *tr, const std::string &s, float x, float y,
                 uint32_t rgba, TextAlignment align = TEXT_ALIGN_LEFT) {
    if (!tr || s.empty()) return;
    tr->set_color((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                  (rgba >> 8) & 0xFF, rgba & 0xFF);
    tr->render(s, static_cast<int>(x), static_cast<int>(y), align);
}

const char *device_short_hint(connection_device_type_t type) {
    switch (type) {
        case connection_device_type_t::NONE:        return "Off";
        case connection_device_type_t::FILE:        return "Disk";
        case connection_device_type_t::PRINTER:     return "IW II";
        case connection_device_type_t::NET_PRINTER: return "TCP";
        case connection_device_type_t::CLIPBOARD:   return "Text";
        case connection_device_type_t::MODEM:       return "Hayes";
        case connection_device_type_t::SERIAL:      return "UART";
    }
    return "";
}

} // namespace

SerialPortsOSD_t::SerialPortsOSD_t(UIContext *ctx, const Style_t &initial_style)
    : Container_t(ctx, initial_style), card_style_(initial_style) {
}

SerialPortsOSD_t::~SerialPortsOSD_t() {
    clear_net_inputs();
    cards_.clear(); // Owned by Container_t::tiles
    delete small_tr_;
    small_tr_ = nullptr;
}

void SerialPortsOSD_t::set_status_provider(SerialPortButton::StatusProvider provider) {
    status_provider_ = std::move(provider);
    show_live_status_ = static_cast<bool>(status_provider_);
    for (SerialPortButton *card : cards_) {
        if (card) {
            card->set_show_live_status(show_live_status_);
            card->set_status_provider(status_provider_);
        }
    }
    layout();
}

void SerialPortsOSD_t::set_host_folder_callbacks(std::function<std::string()> path_provider,
                                                 std::function<void()> on_pick) {
    host_folder_path_provider_ = std::move(path_provider);
    host_folder_pick_cb_ = std::move(on_pick);
    hover_host_folder_pencil_ = false;
    layout();
}

const connection_port_spec_t *SerialPortsOSD_t::find_spec(connection_key_t key) const {
    for (const auto &s : specs_) {
        if (s.key == key) return &s;
    }
    return nullptr;
}

void SerialPortsOSD_t::clear_net_inputs() {
    delete net_host_input_;
    net_host_input_ = nullptr;
    delete net_port_input_;
    net_port_input_ = nullptr;
}

void SerialPortsOSD_t::dismiss_overlay() {
    clear_net_inputs();
    overlay_mode_ = OverlayMode::NONE;
    overlay_from_assign_ = false;
    assign_choices_.clear();
    host_serial_ports_.clear();
    hover_row_ = -1;
    hover_close_ = false;
    hover_back_ = false;
    hover_cancel_ = false;
    hover_apply_ = false;
    for (SerialPortButton *c : cards_) {
        if (c) c->set_active_highlight(false);
    }
}

void SerialPortsOSD_t::rebuild(const std::vector<connection_port_spec_t> &ports) {
    for (Tile_t *tile : tiles) {
        delete tile;
    }
    remove_all_tiles();
    cards_.clear();
    specs_ = ports;

    // Partition serial ports before parallel ports while keeping slot order within each group.
    std::vector<const connection_port_spec_t *> ordered;
    ordered.reserve(specs_.size());
    for (const auto &p : specs_) {
        if (p.kind == connection_port_kind_t::SERIAL) ordered.push_back(&p);
    }
    for (const auto &p : specs_) {
        if (p.kind == connection_port_kind_t::PARALLEL) ordered.push_back(&p);
    }

    const float card_w = std::max(120.0f, tp.w - kOuterPad * 2.0f);
    for (const connection_port_spec_t *p : ordered) {
        auto *card = new SerialPortButton(ctx, card_style_);
        card->set_small_text_renderer(small_tr_);
        card->set_show_live_status(show_live_status_);
        card->set_status_provider(status_provider_);
        card->size(card_w, 28.0f);
        card->set_port(*p);
        card->set_active_highlight(has_overlay() && p->key == overlay_key_);

        const connection_key_t key = p->key;
        card->on_edit_assignment([this, key]() {
            open_assign_overlay(key);
        });
        card->on_edit_detail([this, key]() {
            const connection_port_spec_t *sp = find_spec(key);
            if (!sp) return;
            if (sp->device == connection_device_type_t::NET_PRINTER) {
                open_net_printer_overlay(key, false);
            } else if (sp->device == connection_device_type_t::SERIAL) {
                open_host_serial_overlay(key, false);
            }
        });

        cards_.push_back(card);
        add(card);
    }

    if (has_overlay() && !find_spec(overlay_key_)) {
        dismiss_overlay();
    }
    layout();
}

void SerialPortsOSD_t::layout() {
    if (!visible) return;

    has_serial_section_ = false;
    has_parallel_section_ = false;
    has_host_folder_section_ = false;

    const float card_x = tp.x + kOuterPad;
    const float card_w = std::max(120.0f, tp.w - kOuterPad * 2.0f);
    float y = tp.y + kOuterPad;

    bool placed_serial = false;
    for (SerialPortButton *card : cards_) {
        if (!card || !card->is_visible() || card->get_kind() != connection_port_kind_t::SERIAL) {
            continue;
        }
        if (!placed_serial) {
            has_serial_section_ = true;
            serial_header_y_ = y;
            y += kSectionHeaderH + kSectionHeaderGap;
            placed_serial = true;
        }
        float cw = 0.0f, ch = 0.0f;
        card->get_tile_size(&cw, &ch);
        card->size(card_w, ch);
        card->set_position(card_x, y);
        y += ch + kCardGap;
    }

    bool placed_parallel = false;
    for (SerialPortButton *card : cards_) {
        if (!card || !card->is_visible() || card->get_kind() != connection_port_kind_t::PARALLEL) {
            continue;
        }
        if (!placed_parallel) {
            if (placed_serial) {
                y += (kSectionBreakGap - kCardGap);
            }
            has_parallel_section_ = true;
            parallel_header_y_ = y;
            y += kSectionHeaderH + kSectionHeaderGap;
            placed_parallel = true;
        }
        float cw = 0.0f, ch = 0.0f;
        card->get_tile_size(&cw, &ch);
        card->size(card_w, ch);
        card->set_position(card_x, y);
        y += ch + kCardGap;
    }

    if (host_folder_path_provider_) {
        if (placed_serial || placed_parallel) {
            y += (kSectionBreakGap - kCardGap);
        }
        has_host_folder_section_ = true;
        host_folder_header_y_ = y;
        y += kSectionHeaderH + kSectionHeaderGap;
        host_folder_card_ = SDL_FRect{card_x, y, card_w, 56.0f};
    }

    if (has_overlay()) {
        layout_overlay();
    }
}

void SerialPortsOSD_t::open_assign_overlay(connection_key_t key) {
    const connection_port_spec_t *spec = find_spec(key);
    if (!spec) return;

    if (overlay_open_handler_) overlay_open_handler_();
    clear_net_inputs();

    overlay_mode_ = OverlayMode::ASSIGN;
    overlay_key_ = key;
    overlay_from_assign_ = false;
    hover_row_ = -1;
    hover_close_ = false;
    hover_back_ = false;

    // UI order matches the Connections canvas: None, File, Printer, Net Printer, Clipboard, Modem, Host Serial
    assign_choices_.clear();
    const connection_device_type_t candidates[] = {
        connection_device_type_t::NONE,
        connection_device_type_t::FILE,
        connection_device_type_t::PRINTER,
        connection_device_type_t::NET_PRINTER,
        connection_device_type_t::CLIPBOARD,
        connection_device_type_t::MODEM,
        connection_device_type_t::SERIAL,
    };
    for (connection_device_type_t type : candidates) {
        if (connection_device_allowed(spec->kind, type, spec->offers_printer)) {
            assign_choices_.push_back(type);
        }
    }

    for (SerialPortButton *c : cards_) {
        if (c) c->set_active_highlight(c->get_key() == key);
    }
    layout_overlay();
}

void SerialPortsOSD_t::open_net_printer_overlay(connection_key_t key, bool from_assign) {
    const connection_port_spec_t *spec = find_spec(key);
    if (!spec) return;

    if (overlay_open_handler_) overlay_open_handler_();
    clear_net_inputs();

    overlay_mode_ = OverlayMode::NET_PRINTER;
    overlay_key_ = key;
    overlay_from_assign_ = from_assign;
    hover_row_ = -1;
    hover_close_ = false;
    hover_back_ = false;
    hover_cancel_ = false;
    hover_apply_ = false;

    std::string cur_host;
    int cur_port = kNetPrinterDefaultPort;
    if (spec->device == connection_device_type_t::NET_PRINTER) {
        cur_host = spec->host;
        if (spec->tcp_port > 0) cur_port = spec->tcp_port;
    }

    Style_t field_style{
        .background_color = 0xFFFFFFFF,
        .border_color = 0x0084C6FF,
        .hover_color = 0xFFFFFFFF,
        .padding = 4,
        .border_width = 1,
        .corner_radius = 4.0f,
        .text_color = 0x000000FF,
    };

    TextRenderer *tr = ctx ? ctx->text_render : nullptr;
    net_host_input_ = new TextInput_t(ctx, cur_host, field_style);
    if (tr) net_host_input_->set_text_renderer(tr);
    net_host_input_->set_max_length(253);
    net_host_input_->set_cursor_position(static_cast<int>(cur_host.size()));
    net_host_input_->set_enter_handler([this](const SDL_Event &) -> bool {
        confirm_net_printer();
        return true;
    });

    const std::string port_str = std::to_string(cur_port);
    net_port_input_ = new TextInput_t(ctx, port_str, field_style);
    if (tr) net_port_input_->set_text_renderer(tr);
    net_port_input_->set_max_length(5);
    net_port_input_->set_cursor_position(static_cast<int>(port_str.size()));
    net_port_input_->set_enter_handler([this](const SDL_Event &) -> bool {
        confirm_net_printer();
        return true;
    });

    net_host_input_->set_edit_active(true);

    for (SerialPortButton *c : cards_) {
        if (c) c->set_active_highlight(c->get_key() == key);
    }
    layout_overlay();
}

void SerialPortsOSD_t::open_host_serial_overlay(connection_key_t key, bool from_assign) {
    const connection_port_spec_t *spec = find_spec(key);
    if (!spec) return;

    if (overlay_open_handler_) overlay_open_handler_();
    clear_net_inputs();

    overlay_mode_ = OverlayMode::HOST_SERIAL;
    overlay_key_ = key;
    overlay_from_assign_ = from_assign;
    hover_row_ = -1;
    hover_close_ = false;
    hover_back_ = false;

    host_serial_ports_.clear();
    if (serial_port_manager_) {
        serial_port_manager_->poll(SDL_GetTicks());
        host_serial_gen_ = serial_port_manager_->generation();
        host_serial_ports_ = serial_port_manager_->ports();
    }

    for (SerialPortButton *c : cards_) {
        if (c) c->set_active_highlight(c->get_key() == key);
    }
    layout_overlay();
}

void SerialPortsOSD_t::layout_overlay() {
    if (!has_overlay()) return;

    const float sheet_w = std::max(140.0f, tp.w - kSheetInset * 2.0f);
    const float max_h = std::max(120.0f, tp.h - kSheetInset * 2.0f);
    float sheet_h = 180.0f;

    if (overlay_mode_ == OverlayMode::ASSIGN) {
        const size_t n = assign_choices_.size();
        const float rows_h = n > 0
            ? static_cast<float>(n) * kRowH + static_cast<float>(n - 1) * kRowGap
            : 28.0f;
        sheet_h = kSheetPad + kSheetHeaderH + 6.0f + rows_h + kSheetPad;
    } else if (overlay_mode_ == OverlayMode::NET_PRINTER) {
        // Header + divider + Host label/field + Port label/field + Cancel/Apply row
        sheet_h = kSheetPad + kSheetHeaderH + 8.0f
                + 20.0f + 3.0f + kFieldH + 8.0f
                + 20.0f + 3.0f + kFieldH + 12.0f
                + kActionBtnH + kSheetPad;
    } else if (overlay_mode_ == OverlayMode::HOST_SERIAL) {
        const size_t n = std::max<size_t>(1, host_serial_ports_.size());
        const float rows_h = static_cast<float>(n) * kRowH + static_cast<float>(n - 1) * kRowGap;
        sheet_h = kSheetPad + kSheetHeaderH + 8.0f + rows_h + kSheetPad;
    }

    sheet_h = std::min(sheet_h, max_h);
    const float sheet_x = tp.x + kSheetInset;
    const float sheet_y = tp.y + (tp.h - sheet_h) * 0.5f;
    overlay_sheet_ = SDL_FRect{sheet_x, sheet_y, sheet_w, sheet_h};

    if (overlay_mode_ == OverlayMode::NET_PRINTER && net_host_input_ && net_port_input_) {
        const float inner_x = sheet_x + kSheetPad;
        const float inner_w = sheet_w - kSheetPad * 2.0f;
        float y = sheet_y + kSheetPad + kSheetHeaderH + 8.0f;

        // Host or IP field
        y += 20.0f + 3.0f;
        net_host_input_->size(inner_w, kFieldH);
        net_host_input_->set_position(inner_x, y);
        y += kFieldH + 8.0f;

        // TCP Port field
        y += 20.0f + 3.0f;
        net_port_input_->size(std::min(96.0f, inner_w), kFieldH);
        net_port_input_->set_position(inner_x, y);
        y += kFieldH + 12.0f;

        // Cancel + Apply buttons
        const float btn_gap = 8.0f;
        const float btn_w = (inner_w - btn_gap) * 0.5f;
        cancel_btn_rect_ = SDL_FRect{inner_x, y, btn_w, kActionBtnH};
        apply_btn_rect_  = SDL_FRect{inner_x + btn_w + btn_gap, y, btn_w, kActionBtnH};
    }
}

SDL_FRect SerialPortsOSD_t::overlay_close_rect() const {
    return SDL_FRect{
        overlay_sheet_.x + overlay_sheet_.w - kSheetPad - kOverlayIconBtn,
        overlay_sheet_.y + kSheetPad + (kSheetHeaderH - kOverlayIconBtn) * 0.5f,
        kOverlayIconBtn,
        kOverlayIconBtn,
    };
}

SDL_FRect SerialPortsOSD_t::overlay_back_rect() const {
    return SDL_FRect{
        overlay_sheet_.x + kSheetPad,
        overlay_sheet_.y + kSheetPad + (kSheetHeaderH - kOverlayIconBtn) * 0.5f,
        kOverlayIconBtn,
        kOverlayIconBtn,
    };
}

SDL_FRect SerialPortsOSD_t::overlay_row_rect(int index) const {
    const float list_y = overlay_sheet_.y + kSheetPad + kSheetHeaderH + 6.0f;
    return SDL_FRect{
        overlay_sheet_.x + kSheetPad,
        list_y + static_cast<float>(index) * (kRowH + kRowGap),
        overlay_sheet_.w - kSheetPad * 2.0f,
        kRowH,
    };
}

SDL_FRect SerialPortsOSD_t::host_folder_pencil_rect() const {
    return SDL_FRect{
        host_folder_card_.x + host_folder_card_.w - 8.0f - 20.0f + 2.0f,
        host_folder_card_.y + (28.0f - 20.0f) * 0.5f,
        20.0f,
        20.0f,
    };
}

bool SerialPortsOSD_t::net_printer_valid() const {
    if (!net_host_input_ || !net_port_input_) return false;
    const std::string host = trim_copy(net_host_input_->get_text());
    int port = 0;
    return !host.empty() && parse_tcp_port(net_port_input_->get_text(), port);
}

void SerialPortsOSD_t::apply_and_dismiss(connection_key_t key, connection_device_type_t type,
                                         const std::string &path, const std::string &host,
                                         int tcp_port) {
    dismiss_overlay();
    if (apply_handler_) {
        apply_handler_(key, type, path, host, tcp_port);
    }
}

void SerialPortsOSD_t::confirm_net_printer() {
    if (!net_host_input_ || !net_port_input_) return;
    const std::string host = trim_copy(net_host_input_->get_text());
    int port = 0;
    if (host.empty() || !parse_tcp_port(net_port_input_->get_text(), port)) {
        return;
    }
    const connection_key_t key = overlay_key_;
    apply_and_dismiss(key, connection_device_type_t::NET_PRINTER, {}, host, port);
}

void SerialPortsOSD_t::update() {
    if (net_host_input_) net_host_input_->update();
    if (net_port_input_) net_port_input_->update();

    if (overlay_mode_ == OverlayMode::HOST_SERIAL && serial_port_manager_) {
        serial_port_manager_->poll(SDL_GetTicks());
        if (serial_port_manager_->generation() != host_serial_gen_) {
            host_serial_gen_ = serial_port_manager_->generation();
            host_serial_ports_ = serial_port_manager_->ports();
            layout_overlay();
        }
    }
}

bool SerialPortsOSD_t::needs_redraw() const {
    return (net_host_input_ && net_host_input_->needs_redraw()) ||
           (net_port_input_ && net_port_input_->needs_redraw());
}

bool SerialPortsOSD_t::handle_overlay_event(const SDL_Event &event) {
    if (!has_overlay()) return false;

    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) {
            dismiss_overlay();
            return true;
        }
        if (overlay_mode_ == OverlayMode::NET_PRINTER && net_host_input_ && net_port_input_) {
            if (event.key.key == SDLK_TAB) {
                const bool host_active = net_host_input_->is_edit_active();
                net_host_input_->set_edit_active(!host_active);
                net_port_input_->set_edit_active(host_active);
                if (!host_active) {
                    net_host_input_->set_cursor_position(
                        static_cast<int>(net_host_input_->get_text().size()));
                } else {
                    net_port_input_->set_cursor_position(
                        static_cast<int>(net_port_input_->get_text().size()));
                }
                return true;
            }
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                confirm_net_printer();
                return true;
            }
            TextInput_t *fields[2] = {net_host_input_, net_port_input_};
            for (TextInput_t *f : fields) {
                if (f && f->handle_mouse_event(event)) {
                    return true;
                }
            }
        }
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        hover_row_ = -1;
        hover_close_ = false;
        hover_back_ = false;
        hover_cancel_ = false;
        hover_apply_ = false;
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        const float mx = event.motion.x;
        const float my = event.motion.y;
        hover_close_ = hit_rect(mx, my, overlay_close_rect(), 2.0f);
        hover_back_ = overlay_from_assign_ && hit_rect(mx, my, overlay_back_rect(), 2.0f);
        hover_row_ = -1;
        hover_cancel_ = false;
        hover_apply_ = false;

        if (overlay_mode_ == OverlayMode::ASSIGN) {
            for (size_t i = 0; i < assign_choices_.size(); ++i) {
                if (hit_rect(mx, my, overlay_row_rect(static_cast<int>(i)))) {
                    hover_row_ = static_cast<int>(i);
                    break;
                }
            }
        } else if (overlay_mode_ == OverlayMode::HOST_SERIAL) {
            for (size_t i = 0; i < host_serial_ports_.size(); ++i) {
                SDL_FRect rr = overlay_row_rect(static_cast<int>(i));
                if (rr.y + rr.h > overlay_sheet_.y + overlay_sheet_.h - kSheetPad) break;
                if (hit_rect(mx, my, rr)) {
                    hover_row_ = static_cast<int>(i);
                    break;
                }
            }
        } else if (overlay_mode_ == OverlayMode::NET_PRINTER) {
            hover_cancel_ = hit_rect(mx, my, cancel_btn_rect_);
            hover_apply_ = net_printer_valid() && hit_rect(mx, my, apply_btn_rect_);
        }
        return true;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        const float mx = event.button.x;
        const float my = event.button.y;

        if (!hit_rect(mx, my, overlay_sheet_)) {
            dismiss_overlay();
            return true;
        }
        if (event.button.button != SDL_BUTTON_LEFT) {
            return true;
        }

        if (hit_rect(mx, my, overlay_close_rect(), 2.0f)) {
            dismiss_overlay();
            return true;
        }
        if (overlay_from_assign_ && hit_rect(mx, my, overlay_back_rect(), 2.0f)) {
            open_assign_overlay(overlay_key_);
            return true;
        }

        if (overlay_mode_ == OverlayMode::ASSIGN) {
            for (size_t i = 0; i < assign_choices_.size(); ++i) {
                if (hit_rect(mx, my, overlay_row_rect(static_cast<int>(i)))) {
                    const connection_device_type_t choice = assign_choices_[i];
                    if (choice == connection_device_type_t::NET_PRINTER) {
                        open_net_printer_overlay(overlay_key_, true);
                    } else if (choice == connection_device_type_t::SERIAL) {
                        open_host_serial_overlay(overlay_key_, true);
                    } else {
                        apply_and_dismiss(overlay_key_, choice);
                    }
                    return true;
                }
            }
            return true;
        }

        if (overlay_mode_ == OverlayMode::NET_PRINTER) {
            if (net_host_input_ && net_port_input_) {
                TextInput_t *fields[2] = {net_host_input_, net_port_input_};
                for (TextInput_t *f : fields) {
                    if (f->handle_mouse_event(event)) {
                        if (f->is_edit_active()) {
                            for (TextInput_t *other : fields) {
                                if (other != f) other->set_edit_active(false);
                            }
                        }
                        return true;
                    }
                }
            }
            if (hit_rect(mx, my, cancel_btn_rect_)) {
                dismiss_overlay();
                return true;
            }
            if (hit_rect(mx, my, apply_btn_rect_) && net_printer_valid()) {
                confirm_net_printer();
                return true;
            }
            return true;
        }

        if (overlay_mode_ == OverlayMode::HOST_SERIAL) {
            for (size_t i = 0; i < host_serial_ports_.size(); ++i) {
                SDL_FRect rr = overlay_row_rect(static_cast<int>(i));
                if (rr.y + rr.h > overlay_sheet_.y + overlay_sheet_.h - kSheetPad) break;
                if (hit_rect(mx, my, rr)) {
                    const std::string path = host_serial_ports_[i].path;
                    apply_and_dismiss(overlay_key_, connection_device_type_t::SERIAL, path);
                    return true;
                }
            }
            return true;
        }

        return true;
    }

    return false;
}

bool SerialPortsOSD_t::handle_mouse_event(const SDL_Event &event) {
    if (!active || !visible) return false;
    if (has_overlay()) {
        hover_host_folder_pencil_ = false;
        return handle_overlay_event(event);
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        hover_host_folder_pencil_ = false;
        return Container_t::handle_mouse_event(event);
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        const bool inside = hit_rect(event.motion.x, event.motion.y, SDL_FRect{tp.x, tp.y, tp.w, tp.h});
        if (!inside) {
            hover_host_folder_pencil_ = false;
            SDL_Event leave{};
            leave.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
            for (SerialPortButton *card : cards_) {
                if (card) card->handle_mouse_event(leave);
            }
            return false;
        }
        hover_host_folder_pencil_ =
            has_host_folder_section_ &&
            hit_rect(event.motion.x, event.motion.y, host_folder_pencil_rect(), 2.0f);
        for (SerialPortButton *card : cards_) {
            if (card && card->is_visible()) {
                card->handle_mouse_event(event);
            }
        }
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (has_host_folder_section_ &&
            hit_rect(event.button.x, event.button.y, host_folder_pencil_rect(), 2.0f)) {
            if (host_folder_pick_cb_) {
                host_folder_pick_cb_();
            }
            return true;
        }
    }

    return Container_t::handle_mouse_event(event);
}

void SerialPortsOSD_t::render_host_folder_card() {
    if (!has_host_folder_section_ || !ctx) return;

    TextRenderer *tr = ctx->text_render;
    constexpr float kCardRadius = 6.0f;
    constexpr float kHeaderH = 28.0f;
    constexpr float kPadX = 8.0f;

    // Match SerialPortButton's blue card palette
    ctx->fill_rounded_rect(host_folder_card_, kCardRadius, 0x006399FF);
    SDL_FRect header_rect{host_folder_card_.x, host_folder_card_.y, host_folder_card_.w, kHeaderH};
    ctx->fill_rounded_rect(header_rect, kCardRadius, 0x0084C6FF, true, false);

    const SDL_FRect pencil_r = host_folder_pencil_rect();
    if (tr) {
        const float lh = static_cast<float>(tr->get_font_line_height());
        render_text(tr, "Host Folder", host_folder_card_.x + kPadX,
                    host_folder_card_.y + (kHeaderH - lh) * 0.5f, 0xFFFFFFFF);
    }

    ctx->fill_rounded_rect(pencil_r, 4.0f, hover_host_folder_pencil_ ? 0xE36414FF : 0x004E7ACC);
    ui_icons::draw_pencil(ctx, {pencil_r.x + 4.0f, pencil_r.y + 4.0f,
                                pencil_r.w - 8.0f, pencil_r.h - 8.0f},
                          hover_host_folder_pencil_ ? 0xFFFFFFFF : 0xF0F8FFFF);

    const float div_y = host_folder_card_.y + kHeaderH;
    ctx->fill_rect({host_folder_card_.x + 1.0f, div_y, host_folder_card_.w - 2.0f, 1.0f}, 0xFFFFFF66);

    if (tr) {
        const float body_y = div_y + 1.0f;
        const float body_h = host_folder_card_.y + host_folder_card_.h - body_y;
        const float lh = static_cast<float>(tr->get_font_line_height());
        std::string dir = host_folder_path_provider_ ? host_folder_path_provider_() : std::string();
        const float max_w = host_folder_card_.w - kPadX * 2.0f;
        const std::string label = dir.empty()
            ? std::string("Not configured")
            : ui_crop_text_front(tr, dir, max_w);
        render_text(tr, label, host_folder_card_.x + kPadX,
                    body_y + (body_h - lh) * 0.5f,
                    dir.empty() ? 0xCBEBFFFF : 0xFFFFFFFF);
    }

    ctx->draw_rounded_rect(host_folder_card_, kCardRadius, 0xFFFFFFFF, 1.0f);
}

void SerialPortsOSD_t::render_overlay() {
    if (!has_overlay() || !ctx) return;

    const connection_port_spec_t *spec = find_spec(overlay_key_);
    if (!spec) return;

    TextRenderer *main_tr = ctx->text_render;
    TextRenderer *sub_tr = small_tr_ ? small_tr_ : main_tr;
    const float sh = sub_tr ? static_cast<float>(sub_tr->get_font_line_height()) : 16.0f;

    // Dim the cards strictly inside the Connections container
    ctx->fill_rounded_rect(SDL_FRect{tp.x, tp.y, tp.w, tp.h}, style.corner_radius, kScrimColor);

    // Overlay sheet
    ctx->fill_rounded_rect(overlay_sheet_, kSheetRadius, kSheetBg);

    // Header row: optional Back `<`, title, Close `x`
    const SDL_FRect close_r = overlay_close_rect();
    ctx->fill_rounded_rect(close_r, 4.0f, hover_close_ ? kIconHoverBg : kIconBg);
    ui_icons::draw_close(ctx, {close_r.x + 4.0f, close_r.y + 4.0f, close_r.w - 8.0f, close_r.h - 8.0f},
                         hover_close_ ? 0xFFFFFFFF : 0xD8E6F2FF);

    float title_x = overlay_sheet_.x + kSheetPad;
    if (overlay_from_assign_) {
        const SDL_FRect back_r = overlay_back_rect();
        ctx->fill_rounded_rect(back_r, 4.0f, hover_back_ ? kIconHoverBg : kIconBg);
        ui_icons::draw_back(ctx, {back_r.x + 4.0f, back_r.y + 4.0f, back_r.w - 8.0f, back_r.h - 8.0f},
                            hover_back_ ? 0xFFFFFFFF : 0xD8E6F2FF);
        title_x = back_r.x + back_r.w + 6.0f;
    }

    const std::string port_short = connection_short_port_label(spec->display_name);
    std::string title = port_short;
    if (overlay_mode_ == OverlayMode::ASSIGN) title += " - Device";
    else if (overlay_mode_ == OverlayMode::NET_PRINTER) title += " - Net Printer";
    else if (overlay_mode_ == OverlayMode::HOST_SERIAL) title += " - Host Serial";

    const float title_max_w = close_r.x - title_x - 6.0f;
    const std::string cropped_title = ui_crop_text_end(sub_tr, title, title_max_w);
    const float title_y = overlay_sheet_.y + kSheetPad + (kSheetHeaderH - sh) * 0.5f;
    render_text(sub_tr, cropped_title, title_x, title_y, 0xFFFFFFFF);

    // Divider
    const float div_y = overlay_sheet_.y + kSheetPad + kSheetHeaderH + 2.0f;
    ctx->fill_rect({overlay_sheet_.x + 2.0f, div_y, overlay_sheet_.w - 4.0f, 1.0f}, kSheetDivider);

    if (overlay_mode_ == OverlayMode::ASSIGN) {
        for (size_t i = 0; i < assign_choices_.size(); ++i) {
            const connection_device_type_t choice = assign_choices_[i];
            const bool selected = (spec->device == choice);
            const bool hovered = (hover_row_ == static_cast<int>(i));
            const SDL_FRect rr = overlay_row_rect(static_cast<int>(i));

            const uint32_t bg = hovered ? kRowHoverBg
                                        : (selected ? kRowSelectedBg : kRowBg);
            ctx->fill_rounded_rect(rr, kRowRadius, bg);
            if (selected) {
                ctx->draw_rounded_rect(rr, kRowRadius, kRowSelectedBorder, 1.0f);
            }

            const float text_y = rr.y + (rr.h - sh) * 0.5f;
            render_text(sub_tr, connection_device_display_label(choice),
                        rr.x + 8.0f, text_y, 0xFFFFFFFF);

            float right_x = rr.x + rr.w - 8.0f;
            if (selected) {
                SDL_FRect chk{rr.x + rr.w - 18.0f, rr.y + (rr.h - 12.0f) * 0.5f, 12.0f, 12.0f};
                ui_icons::draw_check(ctx, chk, hovered ? 0xFFFFFFFF : kCheckColor);
                right_x = chk.x - 5.0f;
            }
            render_text(sub_tr, device_short_hint(choice), right_x, text_y,
                        hovered ? 0xFFF2E6FF : 0xCBEBFFFF, TEXT_ALIGN_RIGHT);
        }
    } else if (overlay_mode_ == OverlayMode::NET_PRINTER) {
        const float inner_x = overlay_sheet_.x + kSheetPad;
        if (net_host_input_) {
            float fx = 0, fy = 0;
            net_host_input_->get_tile_position(fx, fy);
            render_text(sub_tr, "Host or IP", inner_x, fy - sh - 2.0f, 0xE8F6FFFF);
            net_host_input_->render();
        }
        if (net_port_input_) {
            float fx = 0, fy = 0;
            net_port_input_->get_tile_position(fx, fy);
            render_text(sub_tr, "TCP Port", inner_x, fy - sh - 2.0f, 0xE8F6FFFF);
            net_port_input_->render();
        }

        // Cancel button
        ctx->fill_rounded_rect(cancel_btn_rect_, 5.0f, hover_cancel_ ? kCancelHoverBg : kCancelBg);
        ctx->draw_rounded_rect(cancel_btn_rect_, 5.0f, 0xD8EEFFCC, 1.0f);
        render_text(sub_tr, "Cancel",
                    cancel_btn_rect_.x + cancel_btn_rect_.w * 0.5f,
                    cancel_btn_rect_.y + (cancel_btn_rect_.h - sh) * 0.5f,
                    0xFFFFFFFF, TEXT_ALIGN_CENTER);

        // Apply button
        const bool valid = net_printer_valid();
        const uint32_t apply_bg = !valid ? kApplyDisabledBg
                                         : (hover_apply_ ? kApplyHoverBg : kApplyBg);
        ctx->fill_rounded_rect(apply_btn_rect_, 5.0f, apply_bg);
        if (valid) {
            ctx->draw_rounded_rect(apply_btn_rect_, 5.0f, 0xFFFFFFFF, 1.0f);
        }
        render_text(sub_tr, "Apply",
                    apply_btn_rect_.x + apply_btn_rect_.w * 0.5f,
                    apply_btn_rect_.y + (apply_btn_rect_.h - sh) * 0.5f,
                    valid ? 0xFFFFFFFF : 0xA6C4D8FF, TEXT_ALIGN_CENTER);
    } else if (overlay_mode_ == OverlayMode::HOST_SERIAL) {
        if (host_serial_ports_.empty()) {
            const SDL_FRect rr = overlay_row_rect(0);
            render_text(sub_tr, "No host serial ports found",
                        rr.x + rr.w * 0.5f, rr.y + (rr.h - sh) * 0.5f,
                        0xCBEBFFFF, TEXT_ALIGN_CENTER);
        } else {
            for (size_t i = 0; i < host_serial_ports_.size(); ++i) {
                const SDL_FRect rr = overlay_row_rect(static_cast<int>(i));
                if (rr.y + rr.h > overlay_sheet_.y + overlay_sheet_.h - kSheetPad) break;
                const bool selected =
                    (spec->device == connection_device_type_t::SERIAL &&
                     spec->path == host_serial_ports_[i].path);
                const bool hovered = (hover_row_ == static_cast<int>(i));

                const uint32_t bg = hovered ? kRowHoverBg
                                            : (selected ? kRowSelectedBg : kRowBg);
                ctx->fill_rounded_rect(rr, kRowRadius, bg);
                if (selected) {
                    ctx->draw_rounded_rect(rr, kRowRadius, kRowSelectedBorder, 1.0f);
                }

                const float max_w = rr.w - (selected ? 30.0f : 16.0f);
                const std::string label = ui_crop_text_front(
                    sub_tr, host_serial_basename(host_serial_ports_[i].display), max_w);
                render_text(sub_tr, label, rr.x + 8.0f, rr.y + (rr.h - sh) * 0.5f, 0xFFFFFFFF);

                if (selected) {
                    SDL_FRect chk{rr.x + rr.w - 18.0f, rr.y + (rr.h - 12.0f) * 0.5f, 12.0f, 12.0f};
                    ui_icons::draw_check(ctx, chk, hovered ? 0xFFFFFFFF : kCheckColor);
                }
            }
        }
    }

    // Overlay border drawn last
    ctx->draw_rounded_rect(overlay_sheet_, kSheetRadius, kSheetBorder, 1.5f);
}

void SerialPortsOSD_t::render() {
    if (!visible) return;

    // Draw outer Connections container background, border, and port cards
    Container_t::render();

    // Draw section headers ("SERIAL PORTS" / "PARALLEL PORTS")
    TextRenderer *hdr_tr = small_tr_ ? small_tr_ : (ctx ? ctx->text_render : nullptr);
    if (hdr_tr) {
        const float hx = tp.x + kOuterPad + 2.0f;
        if (has_serial_section_) {
            render_text(hdr_tr, "SERIAL PORTS", hx, serial_header_y_, kSectionLabelColor);
        }
        if (has_parallel_section_) {
            render_text(hdr_tr, "PARALLEL PORTS", hx, parallel_header_y_, kSectionLabelColor);
        }
        if (has_host_folder_section_) {
            render_text(hdr_tr, "HOST FST", hx, host_folder_header_y_, kSectionLabelColor);
        }
    }

    if (has_host_folder_section_) {
        render_host_folder_card();
    }

    if (has_overlay()) {
        render_overlay();
    }
}

SerialPortButton *SerialPortsOSD_t::find_button(connection_key_t key) {
    for (SerialPortButton *b : cards_) {
        if (b && b->get_key() == key) return b;
    }
    return nullptr;
}

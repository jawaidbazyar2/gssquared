/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "SerialPortButton.hpp"

#include <algorithm>
#include <cstdio>

#include "VectorIcons.hpp"
#include "serial_devices/host/HostSerial.hpp"
#include "util/TextRenderer.hpp"

namespace {

constexpr const char *kEllipsis = "\xE2\x80\xA6"; // UTF-8 "…"

constexpr float kCardWidth = 218.0f;
constexpr float kCardRadius = 6.0f;
constexpr float kHeaderH = 28.0f;
constexpr float kPadX = 8.0f;
constexpr float kIconBtnSize = 20.0f;
constexpr float kIconBtnRadius = 4.0f;

constexpr uint32_t kCardBg           = 0x006399FF;
constexpr uint32_t kHeaderBg         = 0x0084C6FF;
constexpr uint32_t kCardBorder       = 0xFFFFFFFF;
constexpr uint32_t kCardActiveBorder = 0xFFD166FF;
constexpr uint32_t kDividerColor     = 0xFFFFFF66;
constexpr uint32_t kIconBtnBg        = 0x004E7ACC;
constexpr uint32_t kIconBtnHoverBg   = 0xE36414FF;
constexpr uint32_t kIconColor        = 0xF0F8FFFF;
constexpr uint32_t kIconHoverColor   = 0xFFFFFFFF;

size_t utf8_char_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

bool point_in_rect(float x, float y, SDL_FRect r, float inflate = 0.0f) {
    return x >= (r.x - inflate) && x <= (r.x + r.w + inflate) &&
           y >= (r.y - inflate) && y <= (r.y + r.h + inflate);
}

void draw_text(TextRenderer *tr, const std::string &s, float x, float y,
               uint32_t rgba, TextAlignment align = TEXT_ALIGN_LEFT) {
    if (!tr || s.empty()) return;
    tr->set_color((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                  (rgba >> 8) & 0xFF, rgba & 0xFF);
    tr->render(s, static_cast<int>(x), static_cast<int>(y), align);
}

void draw_pencil_button(UIContext *ctx, SDL_FRect r, bool hovered) {
    ctx->fill_rounded_rect(r, kIconBtnRadius, hovered ? kIconBtnHoverBg : kIconBtnBg);
    SDL_FRect icon_box{r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, r.h - 8.0f};
    ui_icons::draw_pencil(ctx, icon_box, hovered ? kIconHoverColor : kIconColor);
}

} // namespace

const char *connection_device_display_label(connection_device_type_t type) {
    switch (type) {
        case connection_device_type_t::NONE:        return "None";
        case connection_device_type_t::FILE:        return "File";
        case connection_device_type_t::PRINTER:     return "Printer";
        case connection_device_type_t::NET_PRINTER: return "Net Printer";
        case connection_device_type_t::CLIPBOARD:   return "Clipboard";
        case connection_device_type_t::MODEM:       return "Modem";
        case connection_device_type_t::SERIAL:      return "Host Serial";
    }
    return "None";
}

std::string connection_short_port_label(const std::string &display_name) {
    if (display_name == "Serial Slot 1") return "Port A";
    if (display_name == "Serial Slot 2") return "Port B";
    if (display_name.rfind("SSC ", 0) == 0) return display_name.substr(4);
    if (display_name.rfind("Parallel ", 0) == 0) return display_name.substr(9);
    if (display_name.rfind("Serial ", 0) == 0) return display_name.substr(7);
    return display_name;
}

std::string ui_crop_text_front(TextRenderer *tr, const std::string &text, float max_w) {
    if (text.empty() || max_w <= 0.0f || tr == nullptr) return {};
    if (static_cast<float>(tr->string_width(text)) <= max_w) return text;
    if (static_cast<float>(tr->string_width(kEllipsis)) > max_w) return {};
    size_t i = 0;
    while (i < text.size()) {
        size_t len = utf8_char_len(static_cast<unsigned char>(text[i]));
        if (i + len > text.size()) len = 1;
        i += len;
        std::string tail = std::string(kEllipsis) + text.substr(i);
        if (static_cast<float>(tr->string_width(tail)) <= max_w) {
            return tail;
        }
    }
    return kEllipsis;
}

std::string ui_crop_text_end(TextRenderer *tr, const std::string &text, float max_w) {
    if (text.empty() || max_w <= 0.0f || tr == nullptr) return {};
    if (static_cast<float>(tr->string_width(text)) <= max_w) return text;
    if (static_cast<float>(tr->string_width(kEllipsis)) > max_w) return {};
    std::vector<size_t> cuts;
    size_t i = 0;
    while (i < text.size()) {
        cuts.push_back(i);
        size_t len = utf8_char_len(static_cast<unsigned char>(text[i]));
        if (i + len > text.size()) len = 1;
        i += len;
    }
    for (auto it = cuts.rbegin(); it != cuts.rend(); ++it) {
        std::string candidate = text.substr(0, *it) + kEllipsis;
        if (static_cast<float>(tr->string_width(candidate)) <= max_w) {
            return candidate;
        }
    }
    return kEllipsis;
}

SerialPortButton::SerialPortButton(UIContext *ctx, const Style_t &style)
    : Tile_t(ctx, style) {
    size(kCardWidth, kHeaderH);
}

float SerialPortButton::calc_card_height() const {
    if (spec_.device == connection_device_type_t::NONE) {
        return kHeaderH;
    }
    if (spec_.device == connection_device_type_t::NET_PRINTER) {
        return show_live_status_ ? 80.0f : 58.0f;
    }
    if (spec_.device == connection_device_type_t::SERIAL) {
        return 58.0f;
    }
    return 56.0f;
}

bool SerialPortButton::has_body_pencil() const {
    return spec_.device == connection_device_type_t::NET_PRINTER ||
           spec_.device == connection_device_type_t::SERIAL;
}

SDL_FRect SerialPortButton::header_pencil_rect() const {
    return SDL_FRect{
        tp.x + tp.w - kPadX - kIconBtnSize + 2.0f,
        tp.y + (kHeaderH - kIconBtnSize) * 0.5f,
        kIconBtnSize,
        kIconBtnSize,
    };
}

SDL_FRect SerialPortButton::body_pencil_rect() const {
    const float body_y = tp.y + kHeaderH + 1.0f;
    return SDL_FRect{
        tp.x + tp.w - kPadX - kIconBtnSize + 2.0f,
        body_y + 4.0f,
        kIconBtnSize,
        kIconBtnSize,
    };
}

std::string SerialPortButton::file_pattern_label() const {
    if (spec_.display_name == "Serial Slot 1") return "GS2.A.*";
    if (spec_.display_name == "Serial Slot 2") return "GS2.B.*";
    char buf[32];
    if (spec_.kind == connection_port_kind_t::PARALLEL) {
        std::snprintf(buf, sizeof(buf), "GS2.PARL%d.*", spec_.key.slot);
    } else {
        std::snprintf(buf, sizeof(buf), "GS2.SSC%d.*", spec_.key.slot);
    }
    return buf;
}

void SerialPortButton::set_show_live_status(bool show) {
    show_live_status_ = show;
    size(tp.w > 0.0f ? tp.w : kCardWidth, calc_card_height());
}

void SerialPortButton::set_port(const connection_port_spec_t &spec) {
    spec_ = spec;
    short_port_name_ = connection_short_port_label(spec.display_name);
    is_ssc_ = spec.display_name.rfind("SSC ", 0) == 0;
    size(tp.w > 0.0f ? tp.w : kCardWidth, calc_card_height());
}

void SerialPortButton::set_device(connection_device_type_t device) {
    spec_.device = device;
    size(tp.w > 0.0f ? tp.w : kCardWidth, calc_card_height());
}

bool SerialPortButton::handle_mouse_event(const SDL_Event &event) {
    if (!visible) return false;

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        hover_header_pencil_ = false;
        hover_body_pencil_ = false;
        is_hovering = false;
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        const float mx = event.motion.x;
        const float my = event.motion.y;
        const SDL_FRect card_rect{tp.x, tp.y, tp.w, tp.h};
        is_hovering = point_in_rect(mx, my, card_rect);
        hover_header_pencil_ = point_in_rect(mx, my, header_pencil_rect(), 2.0f);
        hover_body_pencil_ = has_body_pencil() && point_in_rect(mx, my, body_pencil_rect(), 2.0f);
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        const float mx = event.button.x;
        const float my = event.button.y;
        if (point_in_rect(mx, my, header_pencil_rect(), 2.0f)) {
            if (edit_assignment_cb_) edit_assignment_cb_();
            return true;
        }
        if (has_body_pencil() && point_in_rect(mx, my, body_pencil_rect(), 2.0f)) {
            if (edit_detail_cb_) edit_detail_cb_();
            return true;
        }
    }

    return false;
}

void SerialPortButton::render() {
    if (!visible || !ctx) return;

    TextRenderer *main_tr = ctx->text_render;
    TextRenderer *sub_tr = small_tr_ ? small_tr_ : main_tr;
    const bool has_body = (spec_.device != connection_device_type_t::NONE);

    const SDL_FRect card_rect{tp.x, tp.y, tp.w, tp.h};

    // Card background
    ctx->fill_rounded_rect(card_rect, kCardRadius, kCardBg);

    // Header strip (top-rounded when body exists, all-rounded when collapsed)
    SDL_FRect header_rect{tp.x, tp.y, tp.w, kHeaderH};
    ctx->fill_rounded_rect(header_rect, kCardRadius, kHeaderBg, true, !has_body);

    // Header text: `<Port>  <Device>`
    const SDL_FRect hdr_pencil = header_pencil_rect();
    const float text_left = tp.x + kPadX;
    const float text_right = hdr_pencil.x - 6.0f;

    if (main_tr) {
        const float mh = static_cast<float>(main_tr->get_font_line_height());
        const float port_y = tp.y + (kHeaderH - mh) * 0.5f;
        draw_text(main_tr, short_port_name_, text_left, port_y, 0xFFFFFFFF);

        const float port_w = static_cast<float>(main_tr->string_width(short_port_name_));
        const float dev_x = text_left + port_w + 7.0f;
        const float dev_max_w = text_right - dev_x;
        if (dev_max_w > 12.0f && sub_tr) {
            const float sh = static_cast<float>(sub_tr->get_font_line_height());
            const float dev_y = tp.y + (kHeaderH - sh) * 0.5f;
            const std::string dev_label = ui_crop_text_end(
                sub_tr, connection_device_display_label(spec_.device), dev_max_w);
            const uint32_t dev_col = has_body ? 0xDCF2FFFF : 0xB8DDF2FF;
            draw_text(sub_tr, dev_label, dev_x, dev_y, dev_col);
        }
    }

    // Header Pencil icon button
    draw_pencil_button(ctx, hdr_pencil, hover_header_pencil_);

    // Device-specific body below hairline divider
    if (has_body) {
        const float div_y = tp.y + kHeaderH;
        ctx->fill_rect({tp.x + 1.0f, div_y, tp.w - 2.0f, 1.0f}, kDividerColor);

        const float body_y = div_y + 1.0f;
        const float body_h = tp.y + tp.h - body_y;
        const float sh = sub_tr ? static_cast<float>(sub_tr->get_font_line_height()) : 16.0f;

        if (spec_.device == connection_device_type_t::NET_PRINTER) {
            const SDL_FRect bp = body_pencil_rect();
            std::string ep = format_net_printer_endpoint(spec_.host, spec_.tcp_port);
            if (ep.empty()) ep = "Not configured";
            const float ep_max_w = bp.x - text_left - 6.0f;
            const std::string cropped = ui_crop_text_front(sub_tr, ep, ep_max_w);
            const float row1_y = bp.y + (bp.h - sh) * 0.5f;
            draw_text(sub_tr, cropped, text_left, row1_y, 0xFFFFFFFF);
            draw_pencil_button(ctx, bp, hover_body_pencil_);

            if (show_live_status_) {
                std::string status = "Idle";
                bool is_up = false;
                if (status_provider_) {
                    auto st = status_provider_(spec_.key, spec_.device);
                    if (!st.first.empty()) status = st.first;
                    is_up = st.second;
                }
                const float row2_y = bp.y + bp.h + 2.0f;
                draw_text(sub_tr, status, text_left, row2_y, is_up ? 0x7CFF9EFF : 0xCBEBFFFF);
                draw_text(sub_tr, "60s idle", tp.x + tp.w - kPadX, row2_y, 0xB8E2FFFF, TEXT_ALIGN_RIGHT);
            }
        } else if (spec_.device == connection_device_type_t::SERIAL) {
            const SDL_FRect bp = body_pencil_rect();
            std::string name = host_serial_basename(spec_.path);
            if (name.empty()) name = "Select port";
            const float max_w = bp.x - text_left - 6.0f;
            const std::string cropped = ui_crop_text_front(sub_tr, name, max_w);
            const float row_y = bp.y + (bp.h - sh) * 0.5f;
            draw_text(sub_tr, cropped, text_left, row_y, 0xFFFFFFFF);
            draw_pencil_button(ctx, bp, hover_body_pencil_);
        } else if (spec_.device == connection_device_type_t::PRINTER) {
            const float row_y = body_y + (body_h - sh) * 0.5f;
            draw_text(sub_tr, "ImageWriter II", text_left, row_y, 0xFFFFFFFF);
            draw_text(sub_tr, is_ssc_ ? "SIC P8" : "gs2print", tp.x + tp.w - kPadX, row_y,
                      0xCBEBFFFF, TEXT_ALIGN_RIGHT);
        } else if (spec_.device == connection_device_type_t::FILE) {
            const float row_y = body_y + (body_h - sh) * 0.5f;
            const std::string pat = file_pattern_label();
            draw_text(sub_tr, pat, text_left, row_y, 0xFFFFFFFF);
            draw_text(sub_tr, "60s idle", tp.x + tp.w - kPadX, row_y, 0xCBEBFFFF, TEXT_ALIGN_RIGHT);
        } else if (spec_.device == connection_device_type_t::CLIPBOARD) {
            const float row_y = body_y + (body_h - sh) * 0.5f;
            draw_text(sub_tr, "Text capture", text_left, row_y, 0xFFFFFFFF);
            draw_text(sub_tr, "10s idle", tp.x + tp.w - kPadX, row_y, 0xCBEBFFFF, TEXT_ALIGN_RIGHT);
        } else if (spec_.device == connection_device_type_t::MODEM) {
            const float row_y = body_y + (body_h - sh) * 0.5f;
            std::string right = "Listen :6502";
            bool active_call = false;
            if (status_provider_) {
                auto st = status_provider_(spec_.key, spec_.device);
                if (!st.first.empty()) {
                    right = st.first;
                    active_call = st.second;
                }
            }
            draw_text(sub_tr, "Hayes TCP", text_left, row_y, 0xFFFFFFFF);
            draw_text(sub_tr, right, tp.x + tp.w - kPadX, row_y,
                      active_call ? 0x7CFF9EFF : 0xCBEBFFFF, TEXT_ALIGN_RIGHT);
        }
    }

    // Outer rounded card border drawn last so header/divider sit cleanly inside
    ctx->draw_rounded_rect(card_rect, kCardRadius, active_highlight_ ? kCardActiveBorder : kCardBorder, 1.0f);
}

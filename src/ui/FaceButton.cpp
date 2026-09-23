/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "FaceButton.hpp"

#include "NClock.hpp"

namespace {

constexpr uint32_t kFace       = 0x2C261EFF;
constexpr uint32_t kFaceHover  = 0x40362CFF;
constexpr uint32_t kFaceOn     = 0x343C28FF;
constexpr uint32_t kFaceOnHot  = 0x445236FF;
constexpr uint32_t kHi         = 0xA89880FF;
constexpr uint32_t kSelectBar  = 0xC6F56DFF;
constexpr uint32_t kLegend     = 0xFFF4D8FF;

}  // namespace

const char *speed_button_label(int clock_mode) {
    switch (clock_mode) {
        case CLOCK_1_024MHZ: return "1.0";
        case CLOCK_2_8MHZ:   return "2.8";
        case CLOCK_7_159MHZ: return "7.1";
        case CLOCK_14_3MHZ:  return "14.3";
        default:             return "1.0";
    }
}

FaceButton::FaceButton(UIContext *ctx, const std::string &legend, const Style_t &style, int64_t value)
    : SelectButton_t(ctx, legend, style, value) {
    set_active(false);
    this->style.background_color = 0x00000000;
    this->style.border_width = 0;
    this->style.padding = 0;
    this->style.text_color = kLegend;
    size(56, 56);
}

void FaceButton::show_text(const std::string &legend) {
    if (buttonType == BT_Text && text == legend) {
        return;
    }
    text = legend;
    buttonType = BT_Text;
    set_content_size_from_text();
    position_content(CP_CENTER, CP_CENTER);
}

void FaceButton::show_image(int asset_id) {
    if (buttonType == BT_Atlas && assetID == asset_id) {
        return;
    }
    buttonType = BT_Atlas;
    assetID = asset_id;
    aa = ctx->asset_atlas;
    if (!aa) {
        return;
    }
    const SDL_FRect rect = aa->get_rect(asset_id);
    set_content_size_only(rect.w, rect.h);
    position_content(CP_CENTER, CP_CENTER);
}

void FaceButton::calc_style() {
    estyle = style;
    if (buttonType == BT_Atlas) {
        if (is_hovering) {
            estyle.background_color = estyle.hover_color;
        }
        return;
    }
    estyle.background_color = 0x00000000;
    estyle.border_width = 0;
    estyle.text_color = kLegend;
}

void FaceButton::draw_face() {
    const float x = tp.x;
    const float y = tp.y;
    const float w = tp.w;
    const float h = tp.h;

    uint32_t face = kFace;
    if (active && is_hovering) {
        face = kFaceOnHot;
    } else if (active) {
        face = kFaceOn;
    } else if (is_hovering) {
        face = kFaceHover;
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    ctx->fill_rect({x, y, w, h}, opaque(0x000000FF));
    ctx->fill_rect({x + 1, y + 1, w - 2, h - 2}, opaque(face));
    // Top and left catch the light. Right is the same stroke drawn last so it
    // is not covered, and two pixels wide so the outer column still shows
    // when the renderer drops the far edge of a fill.
    ctx->fill_rect({x + 1, y + 1, w - 3, 1}, opaque(kHi));
    ctx->fill_rect({x + 1, y + 2, 1, h - 3}, opaque(kHi));

    const float bar_h = active ? 4.f : 3.f;
    const uint32_t bar = active ? kSelectBar : accent;
    ctx->fill_rect({x + 2, y + h - 1 - bar_h, w - 4, bar_h}, opaque(bar));
    ctx->fill_rect({x + w - 2, y, 2, h}, opaque(kHi));
    if (active) {
        ctx->draw_rect({x + 2, y + 2, w - 5, h - 4}, opaque(kSelectBar));
    }
}

void FaceButton::render() {
    if (!visible) {
        return;
    }
    if (buttonType == BT_Atlas) {
        Button_t::render();
        return;
    }
    draw_face();
    Button_t::render();
}

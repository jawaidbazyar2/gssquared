/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "SystemBadge.hpp"

SystemBadge_t::SystemBadge_t(UIContext *ctx, int image_id, const Style_t& style,
                             const std::string& name, const std::string& description)
    : ctx(ctx), name(name), description(description) {
    badge = new Button_t(ctx, image_id, style);
}

SystemBadge_t::~SystemBadge_t() {
    delete badge;
}

void SystemBadge_t::set_position(float x, float y) {
    if (badge) badge->set_position(x, y);
}

void SystemBadge_t::set_image_id(int image_id) {
    if (badge) badge->set_assetID(image_id);
}

void SystemBadge_t::set_text(const std::string& name_in, const std::string& description_in) {
    name = name_in;
    description = description_in;
}

void SystemBadge_t::render() {
    if (badge) badge->render();
    if (!ctx || !ctx->text_render) return;
    ctx->text_render->set_color(0, 0, 0, 0xFF);
    if (draw_name) {
        ctx->text_render->render(name, text_x, name_y, TEXT_ALIGN_LEFT);
    }
    if (draw_description) {
        ctx->text_render->render(description, text_x, desc_y, TEXT_ALIGN_LEFT);
    }
}

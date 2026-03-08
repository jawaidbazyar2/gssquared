/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <SDL3/SDL.h>
#include "ModalContainer.hpp"
#include "UIContext.hpp"
#include "util/StorageDevice.hpp"

ModalContainer_t::ModalContainer_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style)
    : Container_t(ctx, initial_style), msg_text(msg_text) {
}

ModalContainer_t::ModalContainer_t(UIContext *ctx, const char* msg_text)
    : Container_t(ctx, Style_t()), msg_text(msg_text) {
}

void ModalContainer_t::layout() {
    if (!visible || tiles.empty()) return;

    // TODO: actually add up the widths of the tiles (don't assume/set them);
    // and iterate based on each actual tile width

    // Center buttons in the container
    // Calculate total width of all tiles and gaps
    float buttons_width = (tiles.size() * 90.0f) + ((tiles.size() - 1) * 10.0f);
    // Center the starting position
    float buttons_x = (tp.w - buttons_width) / 2;
    float buttons_y = 100.0f;
    printf("x: %f y: %f w: %f h: %f\n", tp.x, tp.y, tp.w, tp.h);
    printf("buttons_x: %f buttons_y: %f buttons_width: %f\n", buttons_x, buttons_y, buttons_width);
    for (size_t i = 0; i < tiles.size(); i++) {
        if (tiles[i] && tiles[i]->is_visible()) {
            tiles[i]->set_position(tp.x + buttons_x, tp.y + buttons_y);
            tiles[i]->size(90.0f, 20.0f);  // Set each tile to 100 width, 20 height
            buttons_x += 100.0f;  // Add some spacing between tiles
            printf("tile %zu: %f %f\n", i, tp.x+buttons_x, tp.y+buttons_y);
        }
    }
}

void ModalContainer_t::render() {
    //if (!visible) return;
    // Call the parent class's render method to handle background, border, etc.
    Container_t::render();

    // Draw the message text
    if (!msg_text.empty()) {
        // For now, we'll just print the message to console
        // TODO: Implement proper text rendering
        //printf("Modal Message: %s\n", msg_text.c_str());
        float content_x = (tp.w - strlen(msg_text.c_str()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;
        //printf("content_x: %f X: %f Y: %f %s %08X\n", content_x, x, y, msg_text.c_str(), style.text_color);
        ctx->text_render->set_color(style.text_color >> 24, style.text_color >> 16, style.text_color >> 8, style.text_color & 0xFF);
        ctx->text_render->render(msg_text, tp.x + (tp.w / 2), tp.y + 30, TEXT_ALIGN_CENTER );
    }
}

void ModalContainer_t::set_key(storage_key_t key) {
    this->key = key;
}

void ModalContainer_t::set_data(uint64_t data) {
    this->data = data;
}

storage_key_t ModalContainer_t::get_key() const {
    return key;
}

uint64_t ModalContainer_t::get_data() const {
    return data;    
}
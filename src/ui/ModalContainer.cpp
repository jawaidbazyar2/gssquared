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

namespace {

constexpr uint32_t kAccentColor = 0x2E86ABFF;
constexpr float kAccentHeight = 4.f;
constexpr float kShadowOffset = 6.f;
constexpr uint32_t kShadowColor = 0x00000066;
constexpr uint32_t kScrimColor = 0x00000099;

void viewport_size(UIContext *ctx, int &width, int &height) {
    width = 0;
    height = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (ctx != nullptr && ctx->renderer != nullptr) {
        SDL_GetRenderLogicalPresentation(ctx->renderer, &width, &height, &mode);
    }
    if ((width <= 0 || height <= 0) && ctx != nullptr && ctx->window != nullptr) {
        SDL_GetWindowSize(ctx->window, &width, &height);
    }
    if (width <= 0) {
        width = 800;
    }
    if (height <= 0) {
        height = 600;
    }
}

void set_text_color(TextRenderer *tr, uint32_t rgba) {
    if (tr == nullptr) {
        return;
    }
    tr->set_color((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF);
}

}  // namespace

Style_t dialog_panel_style() {
    return Style_t{
        .background_color = 0x1A2332FF,
        .border_color = 0x7EB8D8FF,
        .hover_color = 0x1A2332FF,
        .padding = 3,
        .border_width = 1,
        .text_color = 0xF4F7FAFF,
    };
}

Style_t dialog_primary_button() {
    return Style_t{
        .background_color = 0x2E86ABFF,
        .border_color = 0xF0F7FAFF,
        .hover_color = 0x3D9BC4FF,
        .padding = 2,
        .border_width = 1,
        .text_color = 0xFFFFFFFF,
    };
}

Style_t dialog_secondary_button() {
    return Style_t{
        .background_color = 0x243044FF,
        .border_color = 0x5A7084FF,
        .hover_color = 0x314258FF,
        .padding = 2,
        .border_width = 1,
        .text_color = 0xE8EEF2FF,
    };
}

ModalContainer_t::ModalContainer_t(UIContext *ctx, const char* msg_text, const Style_t&, modal_stack &stack)
    : Container_t(ctx, dialog_panel_style()), stack(stack) {
        if (msg_text == nullptr) msg_text = "";
        this->msg_text = std::string(msg_text);
}

ModalContainer_t::ModalContainer_t(UIContext *ctx, const char* msg_text, modal_stack &stack)
    : Container_t(ctx, dialog_panel_style()), stack(stack) {
        if (msg_text == nullptr) msg_text = "";
        this->msg_text = std::string(msg_text);
}

void ModalContainer_t::layout() {
    if (!visible || tiles.empty()) return;

    const float gap = 16.f;
    float total = 0.f;
    int visible_count = 0;
    for (Tile_t *tile : tiles) {
        if (tile == nullptr || !tile->is_visible()) {
            continue;
        }
        float tile_w = 0.f;
        float tile_h = 0.f;
        tile->get_tile_size(&tile_w, &tile_h);
        (void)tile_h;
        total += tile_w;
        ++visible_count;
    }
    if (visible_count > 1) {
        total += gap * static_cast<float>(visible_count - 1);
    }
    float x = tp.x + (tp.w - total) / 2.f;
    const float y = tp.y + tp.h - 56.f;
    for (Tile_t *tile : tiles) {
        if (tile == nullptr || !tile->is_visible()) {
            continue;
        }
        float tile_w = 0.f;
        float tile_h = 0.f;
        tile->get_tile_size(&tile_w, &tile_h);
        (void)tile_h;
        tile->set_position(x, y);
        x += tile_w + gap;
    }
}

void ModalContainer_t::render() {
    if (!visible || ctx == nullptr || ctx->renderer == nullptr) return;

    SDL_BlendMode previous = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(ctx->renderer, &previous);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    int width = 0;
    int height = 0;
    viewport_size(ctx, width, height);
    ctx->fill_rect({0.f, 0.f, static_cast<float>(width), static_cast<float>(height)}, kScrimColor);
    ctx->fill_rect({tp.x + kShadowOffset, tp.y + kShadowOffset, tp.w, tp.h}, kShadowColor);

    Container_t::render();

    ctx->fill_rect({tp.x + 1.f, tp.y + 1.f, tp.w - 2.f, kAccentHeight}, kAccentColor);

    if (!msg_text.empty() && ctx->text_render != nullptr) {
        set_text_color(ctx->text_render, style.text_color);
        ctx->text_render->render(msg_text, tp.x + (tp.w / 2), tp.y + 30, TEXT_ALIGN_CENTER);
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, previous);
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
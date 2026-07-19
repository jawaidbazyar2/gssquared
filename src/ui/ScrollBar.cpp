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

#include "ScrollBar.hpp"

#include <algorithm>

ScrollBar_t::ScrollBar_t(UIContext *ctx, const Style_t &style) : Tile_t(ctx, style) {
    set_padding(0);
}

int ScrollBar_t::max_position() const {
    return std::max(0, content_size_ - page_size_);
}

int ScrollBar_t::clamp_position(int pos) const {
    int max_pos = max_position();
    if (pos < 0) {
        return 0;
    }
    if (pos > max_pos) {
        return max_pos;
    }
    return pos;
}

void ScrollBar_t::set_range(int content_size, int page_size) {
    content_size_ = std::max(0, content_size);
    page_size_ = std::max(1, page_size);
    apply_position(position_, false);
}

void ScrollBar_t::set_position(int pos) {
    apply_position(pos, false);
}

void ScrollBar_t::apply_position(int pos, bool notify) {
    int clamped = clamp_position(pos);
    if (clamped == position_) {
        return;
    }
    position_ = clamped;
    if (notify && change_cb_) {
        change_cb_(position_);
    }
}

void ScrollBar_t::scroll_by(int delta) {
    apply_position(position_ + delta, true);
}

void ScrollBar_t::scroll_to_home() {
    apply_position(max_position(), true);
}

void ScrollBar_t::scroll_to_end() {
    apply_position(0, true);
}

SDL_FRect ScrollBar_t::thumb_rect() const {
    float track_h = tp.h;
    if (track_h <= 0.0f) {
        return {tp.x, tp.y, tp.w, 0.0f};
    }

    int max_pos = max_position();
    float thumb_h;
    if (content_size_ <= 0 || page_size_ >= content_size_) {
        thumb_h = track_h;
    } else {
        thumb_h = track_h * (float)page_size_ / (float)content_size_;
        if (thumb_h < kMinThumbPx) {
            thumb_h = kMinThumbPx;
        }
        if (thumb_h > track_h) {
            thumb_h = track_h;
        }
    }

    float travel = track_h - thumb_h;
    float thumb_y = tp.y;
    if (max_pos > 0 && travel > 0.0f) {
        // position 0 -> bottom; position max -> top
        float t = (float)(max_pos - position_) / (float)max_pos;
        thumb_y = tp.y + travel * t;
    } else if (max_pos == 0) {
        thumb_y = tp.y + travel; // at bottom / end
    }

    return {tp.x, thumb_y, tp.w, thumb_h};
}

int ScrollBar_t::position_from_track_y(float y) const {
    int max_pos = max_position();
    if (max_pos <= 0 || tp.h <= 0.0f) {
        return 0;
    }
    float t = (y - tp.y) / tp.h; // 0 at top, 1 at bottom
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    // top -> max (oldest), bottom -> 0 (newest)
    int pos = (int)((1.0f - t) * (float)max_pos + 0.5f);
    return clamp_position(pos);
}

void ScrollBar_t::render() {
    if (!visible) {
        return;
    }

    // Track outline
    ctx->draw_rect({tp.x, tp.y, tp.w, tp.h}, opaque(style.border_color));

    // Thumb
    SDL_FRect thumb = thumb_rect();
    ctx->fill_rect(thumb, opaque(style.border_color));
}

bool ScrollBar_t::handle_mouse_event(const SDL_Event &event) {
    if (!visible) {
        return false;
    }

    auto point_in_rect = [](float x, float y, const SDL_FRect &r) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    };

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
        if (dragging_) {
            dragging_ = false;
            return true;
        }
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        float mx = event.motion.x;
        float my = event.motion.y;
        bool inside = point_in_rect(mx, my, {tp.x, tp.y, tp.w, tp.h});
        if (is_hovering != inside) {
            is_hovering = inside;
            on_hover_changed(is_hovering);
        }
        if (dragging_) {
            // Absolute: thumb/view follow mouse Y on the track
            apply_position(position_from_track_y(my), true);
            return true;
        }
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        float mx = (float)event.button.x;
        float my = (float)event.button.y;
        if (!point_in_rect(mx, my, {tp.x, tp.y, tp.w, tp.h})) {
            return false;
        }

        // Click or press anywhere on track: jump to that fraction and start drag
        apply_position(position_from_track_y(my), true);
        dragging_ = true;
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        if (is_hovering) {
            is_hovering = false;
            on_hover_changed(false);
        }
        dragging_ = false;
    }

    return false;
}

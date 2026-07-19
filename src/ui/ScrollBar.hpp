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

#pragma once

#include <functional>

#include "Tile.hpp"

/**
 * Vertical scrollbar. Position 0 = live/newest end (thumb near bottom);
 * larger position = scrolled toward older content (thumb toward top).
 */
class ScrollBar_t : public Tile_t {
public:
    using ChangeHandler = std::function<void(int pos)>;
    using Tile_t::set_position; // geometry (float x, float y)

    explicit ScrollBar_t(UIContext *ctx, const Style_t &style = Style_t());

    void set_range(int content_size, int page_size);
    void set_position(int pos); // scroll position
    int position() const { return position_; }
    int content_size() const { return content_size_; }
    int page_size() const { return page_size_; }

    void scroll_by(int delta);
    void scroll_to_home();
    void scroll_to_end();

    void on_change(ChangeHandler cb) { change_cb_ = std::move(cb); }

    void render() override;
    bool handle_mouse_event(const SDL_Event &event) override;

private:
    static constexpr float kMinThumbPx = 14.0f;

    int max_position() const;
    int clamp_position(int pos) const;
    SDL_FRect thumb_rect() const;
    /** Map a Y on the track to scroll position (top=max/oldest, bottom=0/newest). */
    int position_from_track_y(float y) const;
    void apply_position(int pos, bool notify);

    int content_size_ = 0;
    int page_size_ = 1;
    int position_ = 0;

    bool dragging_ = false;

    ChangeHandler change_cb_;
};

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

#include <algorithm>

#include "WrapContainer.hpp"

WrapContainer_t::WrapContainer_t(UIContext *ctx, const Style_t &initial_style)
    : Container_t(ctx, initial_style) {}

WrapContainer_t::WrapContainer_t(UIContext *ctx)
    : Container_t(ctx, Style_t()) {}

void WrapContainer_t::layout() {
    if (!visible || tiles.empty()) {
        return;
    }

    const float pad = static_cast<float>(style.padding);
    const float gap = pad; // space between words / between lines
    const float line_left = tp.x + pad;
    const float line_right = tp.x + tp.w - pad;

    float cursor_x = line_left;
    float cursor_y = tp.y + pad;
    float line_height = 0.0f;
    float content_bottom = cursor_y;
    bool placed_any = false;

    for (Tile_t *tile : tiles) {
        if (!tile || !tile->is_visible()) {
            continue;
        }

        float tw = 0.0f;
        float th = 0.0f;
        tile->get_tile_size(&tw, &th);

        // Wrap before placing if this word does not fit on the current line.
        // Never split a word: an oversized first word on a line may overflow.
        if (cursor_x > line_left && cursor_x + tw > line_right) {
            cursor_x = line_left;
            cursor_y += line_height + gap;
            line_height = 0.0f;
        }

        tile->set_position(cursor_x, cursor_y);
        cursor_x += tw + gap;
        line_height = std::max(line_height, th);
        content_bottom = cursor_y + line_height;
        placed_any = true;
    }

    float new_h = pad * 2.0f;
    if (placed_any) {
        new_h = (content_bottom - tp.y) + pad;
    }
    size(tp.w, new_h);
}

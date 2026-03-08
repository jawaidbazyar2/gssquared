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

#include "Container.hpp"
#include "Tile.hpp"
#include "Style.hpp"

Container_t::Container_t(UIContext *ctx, const Style_t& initial_style) : Tile_t(ctx, initial_style)
    {}

Container_t::Container_t(UIContext *ctx) : Tile_t(ctx)
    {}

Container_t::~Container_t() {
    for (auto* tile : tiles) {
        delete tile;
    }
}

void Container_t::apply_style(const Style_t& new_style) {
    style = new_style;
    layout(); // Relayout with new styling
}

void Container_t::add(Tile_t* tile) {
    tiles.push_back(tile);
}

void Container_t::replace(Tile_t* tile, size_t index) {
    if (index < tiles.size()) {
        tiles[index] = tile;
    }
}

void Container_t::set_padding(uint32_t new_padding) {
    style.padding = new_padding;
    layout();  // Relayout when padding changes
}

void Container_t::set_layout_direction(bool right_to_left, bool bottom_to_top) {
    layout_lr = right_to_left ? 1 : 0;
    layout_tb = bottom_to_top ? 1 : 0;
    layout();  // Relayout when direction changes
}

/**
 * @brief Lays out tiles in a grid pattern based on the largest tile dimensions.
 * 
 * This method:
 * 1. Finds the largest tile dimensions among visible tiles
 * 2. Calculates grid dimensions based on container size and tile sizes
 * 3. Positions each visible tile according to layout direction flags
*/
void Container_t::layout() {
    if (!visible || tiles.empty()) return;

    // First pass: find largest tile dimensions among visible tiles
    float max_tile_width = 0;
    float max_tile_height = 0;
    size_t visible_count = 0;

    for (size_t i = 0; i < tiles.size(); i++) {
        if (tiles[i] && tiles[i]->is_visible()) {
            float tile_w, tile_h;
            tiles[i]->get_tile_size(&tile_w, &tile_h);
            max_tile_width = std::max(max_tile_width, tile_w);
            max_tile_height = std::max(max_tile_height, tile_h);
            visible_count++;
        }
    }

    if (visible_count == 0) return;

    // Calculate grid dimensions
    float cell_width = max_tile_width + style.padding * 2;
    float cell_height = max_tile_height + style.padding * 2;
    
    // Calculate how many tiles can fit in each row
    size_t tiles_per_row = static_cast<size_t>(tp.w / cell_width);
    if (tiles_per_row == 0) tiles_per_row = 1;  // Ensure at least one tile per row
    
    // Calculate number of rows needed
    size_t rows = (visible_count + tiles_per_row - 1) / tiles_per_row;

    // Second pass: position the tiles
    size_t current_visible = 0;
    for (size_t i = 0; i < tiles.size(); i++) {
        if (!tiles[i] || !tiles[i]->is_visible()) continue;

        size_t row = current_visible / tiles_per_row;
        size_t col = current_visible % tiles_per_row;

        // Adjust for layout direction
        if (layout_lr) {  // right to left
            col = tiles_per_row - 1 - col;
        }
        if (layout_tb) {  // bottom to top
            row = rows - 1 - row;
        }

        // Calculate position within container
        float tile_x = tp.x + col * cell_width + style.padding;
        float tile_y = tp.y + row * cell_height + style.padding;

        // Set tile position
        tiles[i]->set_position(tile_x, tile_y);
        current_visible++;
    }
}

/**
 * @brief Handles mouse events for the container and its tiles.
 * @param event The SDL event to handle.
*/
bool Container_t::handle_mouse_event(const SDL_Event& event) {
    if (!active || !visible) return(false);

    // Handle mouse motion and button events
    if (event.type == SDL_EVENT_MOUSE_MOTION ||
        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || 
        event.type == SDL_EVENT_DROP_POSITION) {
        
        float mouse_x;
        float mouse_y;
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
        } else if (event.type == SDL_EVENT_DROP_POSITION) {
            mouse_x = event.drop.x;
            mouse_y = event.drop.y;
        }
        
        // Check if mouse is within container bounds
        bool is_inside = (mouse_x >= tp.x && mouse_x <= tp.x + tp.w &&
                        mouse_y >= tp.y && mouse_y <= tp.y + tp.h);

        // Update container hover state
        /* if (is_hovering != is_inside) {
            is_hovering = is_inside;
        } */

        // Forward events to tiles if we're inside the container
        if (is_inside) {
            for (size_t i = 0; i < tiles.size(); i++) {
                if (tiles[i] && tiles[i]->is_visible()) {
                    tiles[i]->handle_mouse_event(event);
                }
            }
        } else {
            // If mouse is outside container, ensure all tiles clear their hover state
            for (size_t i = 0; i < tiles.size(); i++) {
                if (tiles[i] && tiles[i]->is_visible() && tiles[i]->is_mouse_hovering()) {
                    SDL_Event fake_motion = event;
                    // Use current mouse position to trigger proper hover exit
                    fake_motion.motion.x = mouse_x;
                    fake_motion.motion.y = mouse_y;
                    tiles[i]->handle_mouse_event(fake_motion);
                }
            }
        }
    }
    else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        // Mouse left the window, clear all hover states
        is_hovering = false;
        for (size_t i = 0; i < tiles.size(); i++) {
            if (tiles[i] && tiles[i]->is_visible()) {
                tiles[i]->handle_mouse_event(event);
            }
        }
    }
    return(false);
}

/**
 * @brief Renders the container and all its visible tiles.
 * @param renderer The SDL renderer to use.
 */
void Container_t::render() {
    if (!visible) return;

    uint32_t bgcolor = (is_hovering) ? style.hover_color : style.background_color;

    // Draw container background
    SDL_FRect container_rect = {tp.x, tp.y, tp.w, tp.h};
    ctx->fill_rect(container_rect, bgcolor);

    // Draw border if needed
    if (style.border_width > 0) {
        for (uint32_t i = 0; i < style.border_width; i++) {
            SDL_FRect border_rect = {
                tp.x + i, tp.y + i,
                tp.w - 2 * i, tp.h - 2 * i
            };
            ctx->draw_rect(border_rect, style.border_color);
        }
    }

    // Render all visible tiles
    // TODO: won't invisible tiles ignore themselves?
    for (size_t i = 0; i < tiles.size(); i++) {
        if (tiles[i] && tiles[i]->is_visible()) {
            tiles[i]->render();
        }
    }
}

Tile_t* Container_t::get_tile(size_t index) const {
    if (index < tiles.size()) {
        return tiles[index];
    }
    return nullptr;
}

void Container_t::remove_all_tiles() {
    tiles.clear();
}

void Container_t::selected_value(int64_t v) {
    for (size_t i = 0; i < tiles.size(); i++) {
        tiles[i]->set_active(tiles[i]->value() == v);
    }
    // optionally track selected value
    _selected_value = v;
}
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

void Container_t::init_tiles(size_t max_tiles) {
    tile_max = max_tiles;
    tile_count = 0;
    tiles = new Tile_t*[max_tiles]();  // Initialize to nullptr
    for (size_t i = 0; i < max_tiles; i++) {
        tiles[i] = nullptr;
    }
}

Container_t::Container_t(SDL_Renderer *rendererp, size_t max_tiles, const Style_t& initial_style) 
    : style(initial_style), renderer(rendererp) {
    init_tiles(max_tiles);
}

Container_t::Container_t(SDL_Renderer *rendererp, size_t max_tiles) 
    : renderer(rendererp) {
    init_tiles(max_tiles);
}

Container_t::~Container_t() {
    // Clean up the buttons (since we created them with new)
    for (size_t i = 0; i < tile_count; i++) {
            if (tiles[i]) {
                delete tiles[i];
                tiles[i] = nullptr;
            }
        }
        // Clean up the tiles array
        delete[] tiles;
    }

void Container_t::apply_style(const Style_t& new_style) {
    style = new_style;
    layout(); // Relayout with new styling
}

void Container_t::add_tile(Tile_t* tile, size_t index) {
    if (index < tile_max) {
        tiles[index] = tile;
        tile_count++;
    }
}

void Container_t::set_position(float new_x, float new_y) {
    x = new_x;
    y = new_y;
    layout();  // Relayout when container position changes
}

void Container_t::set_tile_size(float new_w, float new_h) {
    w = new_w;
    h = new_h;
    layout();  // Relayout when container size changes
}

void Container_t::set_padding(uint32_t new_padding) {
    style.padding = new_padding;
    layout();  // Relayout when padding changes
}

void Container_t::set_background_color(uint32_t color) { style.background_color = color; }
void Container_t::set_border_color(uint32_t color) { style.border_color = color; }
void Container_t::set_border_width(uint32_t width) { style.border_width = width; }
void Container_t::set_hover_color(uint32_t color) { style.hover_color = color; }
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
    if (!visible || tile_count == 0) return;

    // First pass: find largest tile dimensions among visible tiles
    float max_tile_width = 0;
    float max_tile_height = 0;
    size_t visible_count = 0;

    for (size_t i = 0; i < tile_count; i++) {
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
    size_t tiles_per_row = static_cast<size_t>(w / cell_width);
    if (tiles_per_row == 0) tiles_per_row = 1;  // Ensure at least one tile per row
    
    // Calculate number of rows needed
    size_t rows = (visible_count + tiles_per_row - 1) / tiles_per_row;

    // Second pass: position the tiles
    size_t current_visible = 0;
    for (size_t i = 0; i < tile_count; i++) {
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
        float tile_x = x + col * cell_width + style.padding;
        float tile_y = y + row * cell_height + style.padding;

        // Set tile position
        tiles[i]->set_tile_position(tile_x, tile_y);
        current_visible++;
    }
}

/**
 * @brief Handles mouse events for the container and its tiles.
 * @param event The SDL event to handle.
*/
void Container_t::handle_mouse_event(const SDL_Event& event) {
    if (!active || !visible) return;

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
        bool is_inside = (mouse_x >= x && mouse_x <= x + w &&
                        mouse_y >= y && mouse_y <= y + h);

        // Update container hover state
        /* if (is_hovering != is_inside) {
            is_hovering = is_inside;
        } */

        // Forward events to tiles if we're inside the container
        if (is_inside) {
            for (size_t i = 0; i < tile_count; i++) {
                if (tiles[i] && tiles[i]->is_visible()) {
                    tiles[i]->handle_mouse_event(event);
                }
            }
        } else {
            // If mouse is outside container, ensure all tiles clear their hover state
            for (size_t i = 0; i < tile_count; i++) {
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
        for (size_t i = 0; i < tile_count; i++) {
            if (tiles[i] && tiles[i]->is_visible()) {
                tiles[i]->handle_mouse_event(event);
            }
        }
    }
}

/**
 * @brief Renders the container and all its visible tiles.
 * @param renderer The SDL renderer to use.
 */
void Container_t::render() {
    if (!visible) return;

    uint32_t bgcolor = (is_hovering) ? style.hover_color : style.background_color;

    // Draw container background
    SDL_SetRenderDrawColor(renderer, 
        (bgcolor >> 24) & 0xFF,
        (bgcolor >> 16) & 0xFF,
        (bgcolor >> 8) & 0xFF,
        bgcolor & 0xFF
    );
    
    SDL_FRect container_rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &container_rect);

    // Draw border if needed
    if (style.border_width > 0) {
        SDL_SetRenderDrawColor(renderer,
            (style.border_color >> 24) & 0xFF,
            (style.border_color >> 16) & 0xFF,
            (style.border_color >> 8) & 0xFF,
            style.border_color & 0xFF
        );
        for (uint32_t i = 0; i < style.border_width; i++) {
            SDL_FRect border_rect = {
                x + i, y + i,
                w - 2 * i, h - 2 * i
            };
            SDL_RenderRect(renderer, &border_rect);
        }
    }

    // Render all visible tiles
    for (size_t i = 0; i < tile_count; i++) {
        if (tiles[i] && tiles[i]->is_visible()) {
            tiles[i]->render(renderer);
        }
    }
}

Tile_t* Container_t::get_tile(size_t index) const {
    if (index < tile_count) {
        return tiles[index];
    }
    return nullptr;
}

void Container_t::remove_all_tiles() {
    for (size_t i = 0; i < tile_count; i++) {
        if (tiles[i]) {
            tiles[i] = nullptr;
        }
    }
    tile_count = 0;
}
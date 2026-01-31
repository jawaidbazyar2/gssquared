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

#include "MousePositionTile.hpp"
#include "Tile.hpp"

/**
 * @brief A tile that displays the current mouse position.
 * 
 * This tile shows the current mouse x,y coordinates as text.
 * It updates whenever mouse events are received.
 */

MousePositionTile_t::MousePositionTile_t() {
    // Initialize the position text
    snprintf(position_text, sizeof(position_text), "Mouse: %.0f,%.0f", mouse_x, mouse_y);
    // Set a default size that can accommodate the text
    set_tile_size(150, 30);
}

void MousePositionTile_t::render(SDL_Renderer* renderer) {
    if (!visible) return;

    // First render the base tile (background and border)
    Tile_t::render(renderer);

    // Update the position text
    snprintf(position_text, sizeof(position_text), "Mouse: %.0f,%.0f", mouse_x, mouse_y);

    // Get content area position
    //float content_x, content_y;
    //get_content_position(&content_x, &content_y);

    // Render the position text in the content area
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  // Black text
    SDL_RenderDebugText(renderer, cp.x, cp.y, position_text);
}

bool MousePositionTile_t::handle_mouse_event(const SDL_Event& event) {
    if (!active || !visible) return(false);

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        // Update stored mouse position
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
    }

    // Still call base class to handle hover detection etc.
    Tile_t::handle_mouse_event(event);
    return false;
}

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

#include <string>
#include <SDL3/SDL.h>
#include "Tile.hpp"

/**
 * @brief A tile that displays the current mouse position.
 * 
 * This tile shows the current mouse x,y coordinates as text.
 * It updates whenever mouse events are received and automatically
 * renders the position in its content area.
 */
class MousePositionTile_t : public Tile_t {
private:
    float mouse_x = 0;
    float mouse_y = 0;
    char position_text[32];  // Buffer for position text

public:
    /**
     * @brief Constructs a mouse position tile.
     * 
     * Initializes the position text and sets a default size
     * that can accommodate the coordinate display.
     */
    MousePositionTile_t();

    /**
     * @brief Renders the tile with current mouse position.
     * 
     * Displays the background, border, and current mouse coordinates
     * as text in the content area.
     * 
     * @param renderer The SDL renderer to use
     */
    void render(SDL_Renderer* renderer) override;

    /**
     * @brief Handles mouse events to update the displayed position.
     * 
     * Updates the stored mouse coordinates when mouse motion events
     * are received, and handles basic tile functionality through
     * the parent class.
     * 
     * @param event The SDL event to handle
     */
    bool handle_mouse_event(const SDL_Event& event) override;
}; 
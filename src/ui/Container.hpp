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
#include "Style.hpp"

/**
 * @brief A container class that manages and layouts multiple tiles in a grid pattern.
 * 
 * Container_t provides functionality to:
 * - Hold and manage multiple Tile_t objects
 * - Automatically layout tiles in a grid
 * - Handle mouse events for contained tiles
 * - Support different layout directions
 * - Render all contained tiles
 */
class Container_t {
protected:
    Style_t style;
    float x, y;
    float w, h;
    bool visible = true;
    bool active = true;
    bool is_hovering = false;
    int layout_lr = 0; /* 0 = left to right, 1 = right to left */
    int layout_tb = 0; /* 0 = top to bottom, 1 = bottom to top */
    size_t tile_count = 0;
    size_t tile_max = 0;
    Tile_t **tiles = nullptr;  // Array of pointers to tiles
    SDL_Renderer *renderer;

private:
    /**
     * @brief Initializes the tiles array.
     * @param max_tiles Maximum number of tiles this container can hold
     */
    void init_tiles(size_t max_tiles);

public:
    /**
     * @brief Constructs a container with style.
     * @param rendererp SDL renderer to use
     * @param max_tiles Maximum number of tiles this container can hold
     * @param initial_style Initial style settings
     */
    Container_t(SDL_Renderer *rendererp, size_t max_tiles, const Style_t& initial_style);
    
    /**
     * @brief Constructs a container with default style.
     * @param rendererp SDL renderer to use
     * @param max_tiles Maximum number of tiles this container can hold
     */
    Container_t(SDL_Renderer *rendererp, size_t max_tiles);

    /**
     * @brief Destructor that cleans up all contained tiles.
     */
    ~Container_t();

    /**
     * @brief Applies a new style to the container.
     * @param new_style The style to apply
     */
    void apply_style(const Style_t& new_style);

    /**
     * @brief Adds a tile to the container.
     * @param tile Pointer to the tile to add
     * @param index Position in the container where to add the tile
     */
    void add_tile(Tile_t* tile, size_t index);

    /**
     * @brief Sets the container's position.
     * @param new_x X coordinate
     * @param new_y Y coordinate
     */
    void set_position(float new_x, float new_y);

    /**
     * @brief Sets the container's size.
     * @param new_w Width
     * @param new_h Height
     */
    void set_tile_size(float new_w, float new_h);

    /**
     * @brief Sets the container's padding.
     * @param new_padding Padding value
     */
    void set_padding(uint32_t new_padding);

    // Style setters
    void set_background_color(uint32_t color);
    void set_border_color(uint32_t color);
    void set_border_width(uint32_t width);
    void set_hover_color(uint32_t color);

    /**
     * @brief Sets the layout direction.
     * @param right_to_left If true, layout from right to left
     * @param bottom_to_top If true, layout from bottom to top
     */
    void set_layout_direction(bool right_to_left, bool bottom_to_top);

    /**
     * @brief Lays out all tiles in the container.
     */
    virtual void layout();

    /**
     * @brief Handles mouse events for the container and its tiles.
     * @param event The SDL event to handle
     */
    void handle_mouse_event(const SDL_Event& event);

    /**
     * @brief Renders the container and all its tiles.
     */
    virtual void render();

    /**
     * @brief Gets a tile by index.
     * @param index The index of the tile to get
     * @return The tile at the specified index
     */
    Tile_t* get_tile(size_t index) const;
}; 
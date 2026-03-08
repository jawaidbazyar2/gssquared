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

#include <vector>
#include <SDL3/SDL.h>
#include "Tile.hpp"
#include "Style.hpp"
#include "UIContext.hpp"

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
class Container_t : public Tile_t {
protected:
    int64_t _selected_value = 0;
    int layout_lr = 0; /* 0 = left to right, 1 = right to left */
    int layout_tb = 0; /* 0 = top to bottom, 1 = bottom to top */
    std::vector<Tile_t*> tiles;

public:

    /**
     * @brief Constructs a container with style.
     * @param ctx Shared UI rendering context
     * @param initial_style Initial style settings
     */
    Container_t(UIContext *ctx, const Style_t& initial_style = Style_t());
    
    /**
     * @brief Constructs a container with default style.
     * @param ctx Shared UI rendering context
     */
    Container_t(UIContext *ctx);

    /**
     * @brief Destructor that cleans up all contained tiles.
     */
    ~Container_t();

    /**
     remove all tiles from the container */
    void remove_all_tiles();
    
    /**
     * @brief Removes a tile from the container.
     * @param index Position in the container where to remove the tile
     */
    void remove_tile(size_t index);

    /**
     * @brief Applies a new style to the container.
     * @param new_style The style to apply
     */
    void apply_style(const Style_t& new_style);

    /**
     * @brief Appends a tile to the container.
     * @param tile Pointer to the tile to add
     */
    void add(Tile_t* tile);

    /**
     * @brief Replaces the tile at a given index.
     * @param tile Pointer to the new tile
     * @param index Position in the container to replace
     */
    void replace(Tile_t* tile, size_t index);

    /**
     * @brief Sets the container's position.
     * @param new_x X coordinate
     * @param new_y Y coordinate
     */
/*     void set_position(float new_x, float new_y); */

    /**
     * @brief Sets the container's size.
     * @param new_w Width
     * @param new_h Height
     */
/*     void size(float new_w, float new_h); */

    /**
     * @brief Sets the container's padding.
     * @param new_padding Padding value
     */
    void set_padding(uint32_t new_padding);

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
    virtual bool handle_mouse_event(const SDL_Event& event);

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

    /**
     * @brief Gets the number of tiles in the container.
     * @return The number of tiles in the container
     */
    size_t get_tile_count() const { return tiles.size(); };

    /**
     * @brief Gets the tiles in the container.
     * @return The tiles in the container
     */
    const std::vector<Tile_t*>& get_tiles() const { return tiles; };

    /**
     * @brief Shows or hides the container.
     * @param visible If true, the container is visible, otherwise it is hidden.
     */
    inline void set_visible(bool visible) { this->visible = visible; };
    inline bool is_visible() const { return visible; };

    /**
     * @brief Selects a tile by value.
     * @param v The value to select
     */
    void selected_value(int64_t v);
    int64_t selected_value() const { return _selected_value; }
}; 
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

#include <SDL3/SDL.h>
#include <functional>

#include "UIContext.hpp"
#include "Style.hpp"
#include "util/TextRenderer.hpp"

enum ContentPosition_t {
    CP_LEFT,
    CP_CENTER,
    CP_RIGHT,
    CP_TOP,
    CP_BOTTOM
};

struct UI_Rect {
    float x, y, w, h;
};

/**
 * @brief Base class for UI elements that can be rendered and interacted with.
 * 
 * Tile_t provides core functionality for UI elements including:
 * - Positioning and sizing
 * - Background and border rendering
 * - Mouse interaction handling
 * - Click callback support
 */
class Tile_t {

public:
    using EventHandler = std::function<bool(const SDL_Event&)>;
    //typedef void (*click_callback_t)(void* data);

    /**
     * @brief Constructs a tile with the given style.
     * @param initial_style The initial style settings for the tile
     */
    Tile_t(UIContext *ctx, const Style_t& initial_style = Style_t(), int64_t initial_value = 0);
    
    /**
     * @brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~Tile_t() = default;

    /**
     * @brief Sets the size of the content area.
     * @param content_width Width of the content area
     * @param content_height Height of the content area
     */
    void size(float tile_width, float tile_height);

    /**
     * @brief Gets the content area position.
     * @param out_x Output parameter for content x position
     * @param out_y Output parameter for content y position
     */
    //void get_content_position(float* out_x, float* out_y) const;

    /**
     * @brief Gets the content area size.
     * @param out_w Output parameter for content width
     * @param out_h Output parameter for content height
     */
    void get_content_size(float* out_w, float* out_h) const;

    /**
     * @brief Sets the size of the content area.
     * @param content_width Width of the content area
     * @param content_height Height of the content area
     */
    void set_content_size(float content_width, float content_height);
    void set_content_size_only(float content_width, float content_height);

    /**
     * @brief Gets the total tile size including padding and borders.
     * @param out_w Output parameter for total width
     * @param out_h Output parameter for total height
     */
    void get_tile_size(float* out_w, float* out_h) const;
    //void size(float tile_width, float tile_height);

    /**
     * @brief Sets the padding around the content area.
     * @param new_padding The new padding value
     */
    void set_padding(uint32_t new_padding);

    /**
     * @brief Sets the border width.
     * @param width The new border width
     */
    void set_border_width(uint32_t width);

    /**
     * @brief Renders the tile.
     * @param renderer The SDL renderer to use
     */
    virtual void render();

    /**
     * @brief Handles mouse events for the tile.
     * @param event The SDL event to handle
     */
    virtual bool handle_mouse_event(const SDL_Event& event);

    // State getters
    bool is_visible() const;
    bool is_active() const;
    bool is_mouse_hovering() const;

    // State setters
    void set_visible(bool v);
    void set_active(bool a);
    virtual void set_position(float x, float y);

    void on_click(EventHandler handler);

    void set_opacity(int o);
    int calc_opacity(uint32_t color);
    uint32_t opaque(uint32_t color);
    void get_tile_position(float &x, float &y) const { x = tp.x; y = tp.y; }

    void position_content(ContentPosition_t pos_h, ContentPosition_t pos_v);
    void print();

    inline int64_t value() const { return _value; }
    inline void value(int64_t v) { _value = v; }

    /**
     * @brief Called when hover state changes.
     * @param hovering True if mouse is hovering, false otherwise
     */
     virtual void on_hover_changed(bool hovering);

     Style_t style;

protected:

    /**
     * @brief Called when tile is clicked.
     */
    virtual void do_click(const SDL_Event& event);

    void calc_content_position();
    inline virtual void calc_style() { 
        estyle = style;
        if (is_hovering) { estyle.background_color = estyle.hover_color; } 
    }

    Style_t estyle; // effective style

    UIContext *ctx = nullptr;

    UI_Rect out;                 // outer rect - rectangle that includes padding and border
    UI_Rect tp;                  // tile position - does not include padding or border
    UI_Rect cp;                  // content position - if content is smaller than tp

    ContentPosition_t pos_h = CP_LEFT, pos_v = CP_TOP;
    bool visible = true;
    bool active = true;
    bool is_hovering = false;
    int opacity = 255;

    int64_t _value = 0;

    EventHandler click_callback_h = nullptr;

}; 
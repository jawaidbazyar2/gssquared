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

#include "Tile.hpp"

Tile_t::Tile_t(const Style_t& initial_style) : style(initial_style) {
    calc_content_position();
}

void Tile_t::set_tile_size(float tile_width, float tile_height) {
    tp.w = tile_width;
    tp.h = tile_height;
    calc_content_position();
}

void Tile_t::set_content_size(float content_width, float content_height) {
    cp.w = content_width;
    cp.h = content_height;
    // Total size includes padding on both sides and border width on both sides
    tp.w = content_width + (style.padding * 2) + (style.border_width * 2);
    tp.h = content_height + (style.padding * 2) + (style.border_width * 2);
    calc_content_position();
}

void Tile_t::set_content_size_only(float content_width, float content_height) {
    cp.w = content_width;
    cp.h = content_height;
    calc_content_position();
}

void Tile_t::get_tile_size(float* out_w, float* out_h) const {
    *out_w = tp.w;
    *out_h = tp.h;
}

void Tile_t::get_content_size(float* out_w, float* out_h) const {
    *out_w = cp.w;
    *out_h = cp.h;
}

void Tile_t::set_padding(uint32_t new_padding) {
    style.padding = new_padding;
    // Recalculate total size based on new padding
    calc_content_position();
//    set_tile_size(cp.w, cp.h);  
}

void Tile_t::set_border_width(uint32_t width) {
    style.border_width = width;
    // Recalculate total size based on new border width
    //set_tile_size(cp.w, cp.h);
}

int Tile_t::calc_opacity(uint32_t color) {
    int opc = opacity < (color & 0xFF) ? opacity : (color & 0xFF);
    return(opc);
}


void Tile_t::render(SDL_Renderer* renderer) {
    if (!visible) return;

    uint32_t bcolor = (is_hovering) ? style.hover_color : style.border_color;

    // Draw background (includes padding area)
    SDL_SetRenderDrawColor(renderer, 
        (style.background_color >> 24) & 0xFF,
        (style.background_color >> 16) & 0xFF,
        (style.background_color >> 8) & 0xFF,
        calc_opacity(style.background_color)
    );
    
    SDL_FRect tile_rect = {tp.x, tp.y, tp.w, tp.h};
    SDL_RenderFillRect(renderer, &tile_rect);

    // Draw border if needed
    if (style.border_width > 0) {
        //printf("tile_t:render:hover: %d, draw border: %08X width: %d\n", (int)is_hovering,  bcolor, style.border_width);
        SDL_SetRenderDrawColor(renderer,
            (bcolor >> 24) & 0xFF,
            (bcolor >> 16) & 0xFF,
            (bcolor >> 8) & 0xFF,
            calc_opacity(bcolor)
        );
        for (uint32_t i = 0; i < style.border_width; i++) {
            SDL_FRect border_rect = {
                tp.x + i, tp.y + i,
                tp.w - 2 * i, tp.h - 2 * i
            };
            SDL_RenderRect(renderer, &border_rect);
        }
    }
}

bool Tile_t::handle_mouse_event(const SDL_Event& event) {
    if (!active || !visible) return(false);

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        float mouse_x = event.motion.x;
        float mouse_y = event.motion.y;
        
        // Check if mouse is within tile bounds
        bool is_inside = (mouse_x >= tp.x && mouse_x <= tp.x + tp.w &&
                        mouse_y >= tp.y && mouse_y <= tp.y + tp.h);
        
        // If we were hovering but mouse is now outside, or
        // we weren't hovering but mouse is now inside, trigger hover change
        if (is_hovering != is_inside) {
            is_hovering = is_inside;
            on_hover_changed(is_hovering);
        }
        // others may care about same event
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && is_hovering) {
        on_click(event);
        return(true); // we did a thing.
    }
    else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        // Mouse left the window entirely, clear hover state
        if (is_hovering) {
            is_hovering = false;
            on_hover_changed(false);
        }
    }
    return(false);
}

bool Tile_t::is_visible() const { return visible; }
bool Tile_t::is_active() const { return active; }
bool Tile_t::is_mouse_hovering() const { return is_hovering; }
void Tile_t::set_visible(bool v) { visible = v; }
void Tile_t::set_active(bool a) { active = a; }
void Tile_t::set_tile_position(float x, float y) { tp.x = x; tp.y = y; }
void Tile_t::set_background_color(uint32_t color) { style.background_color = color; }
void Tile_t::set_border_color(uint32_t color) { style.border_color = color; }
void Tile_t::set_hover_color(uint32_t color) { style.hover_color = color; }
void Tile_t::set_click_callback(click_callback_t callback, void* data) {
    click_callback = callback;
    callback_data = data;
}

// border is outside the content area.
// padding is inside the content area.
void Tile_t::calc_content_position() {
    switch (pos_h) {

        case CP_CENTER:
            cp.x = (tp.w / 2) - (cp.w / 2);
            break;
        case CP_RIGHT:
            cp.x = tp.w - cp.w - style.padding;
            break;
        case CP_LEFT:
        default:
            cp.x = style.padding;
            break;
    }
    switch (pos_v) {

        case CP_CENTER:
            cp.y = (tp.h / 2) - (cp.h / 2);
            break;
        case CP_BOTTOM:
            cp.y = tp.h - cp.h - style.padding;
            break;
        case CP_TOP:
        default:
            cp.y = style.padding;
            break;
    }
}

void Tile_t::position_content(ContentPosition_t pos_h, ContentPosition_t pos_v) {
    this->pos_h = pos_h;
    this->pos_v = pos_v;
    calc_content_position(); 
}

void Tile_t::set_click_callback(EventHandler handler) {
    click_callback_h = handler;
}

/**
 * @brief Called when the hover state changes.
 * @param hovering True if the mouse is now hovering, false otherwise.
 */
void Tile_t::on_hover_changed(bool hovering) {
    //printf("tile:on_hover_changed: %d\n", hovering);
    is_hovering = hovering;
}

/**
* @brief Called when the tile is clicked. (send the event for fullness, for text edit etc we need the exact x/y location)
*/
void Tile_t::on_click(const SDL_Event& event) {
    if (click_callback) {
        click_callback(callback_data);
    }
    else if (click_callback_h) {
        click_callback_h(event);
    }
}

void Tile_t::set_text_renderer(TextRenderer *text_render) {
    this->text_render = text_render;
}

void Tile_t::set_opacity(int o) {
    opacity = o;
}

void Tile_t::print() {
    printf("Tile_t:\n");
    // print tp x, y, w, h
    printf("tp: x=%6.1f, y=%6.1f, w=%6.1f, h=%6.1f\n", tp.x, tp.y, tp.w, tp.h);
    // print cp x, y, w, h
    printf("cp: x=%6.1f, y=%6.1f, w=%6.1f, h=%6.1f\n", cp.x, cp.y, cp.w, cp.h);
    // print pos_h and pos_v
    printf("pos_h: %6d, pos_v: %6d\n", pos_h, pos_v);
    // print visible, active, is_hovering
    printf("visible: %d, active: %d, is_hovering: %d\n", visible, active, is_hovering);
}
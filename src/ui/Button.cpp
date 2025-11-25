/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <string>

#include "AssetAtlas.hpp"
#include "Tile.hpp"
#include "Button.hpp"

/**
 * @brief A button class that can display either text or an image.
 * 
 * Buttons are interactive UI elements that can be clicked and can display
 * either text or an image (but not both). They support different states
 * (active/inactive) and can change appearance on hover.
 */
// default to setting both tile and content size.
void Button_t::set_size_from_asset() {
    SDL_FRect rect = aa->get_rect(assetID);
    set_tile_size(rect.w, rect.h);
    set_content_size(rect.w, rect.h);
}

void Button_t::set_content_size_from_tile() {
    cp.w = tp.w; cp.h = tp.h;
}

Button_t::Button_t(const std::string& button_text, const Style_t& style, int group)
    : Tile_t(style), text(button_text), group_id(group), buttonType(BT_Text) {
        //set_content_size_from_tile();
        set_content_size_from_text();
        position_content(CP_CENTER, CP_CENTER);
    }
    
Button_t::Button_t(const std::string& button_text, TextRenderer *tr, const Style_t& style, int group)
    : Tile_t(style), text(button_text), group_id(group), buttonType(BT_Text) {
        set_text_renderer(tr);
        //set_content_size_from_tile();
        set_content_size_from_text();
        position_content(CP_CENTER, CP_CENTER);
    }
   
void Button_t::set_content_size_from_text() {
    if (text_render == nullptr) {
        cp.w = (strlen(text.c_str()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        cp.h = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    } else {
        cp.w = text_render->string_width(text);
        cp.h = text_render->get_font_line_height();
    }
}

Button_t::Button_t(AssetAtlas_t* assetp, int assetID, const Style_t& style, int group)
    : Tile_t(style), aa(assetp), assetID(assetID), group_id(group), buttonType(BT_Atlas) {
        set_size_from_asset();
    }

/**
 * @brief Constructs a text button.
 * @param button_text The text to display on the button.
 * @param group The button group ID (default 0).
 */
Button_t::Button_t(const std::string& button_text, int group) 
    : text(button_text), group_id(group), buttonType(BT_Text) {}

/**
 * @brief Constructs an image button.
 * @param button_image The image asset to display on the button.
 * @param group The button group ID (default 0).
 */
Button_t::Button_t(AssetAtlas_t* assetp, int assetID, int group) 
    : aa(assetp), assetID(assetID), group_id(group), buttonType(BT_Atlas) {
        set_size_from_asset();
    }

void Button_t::set_assetID(int id) { 
    assetID = id;
    set_size_from_asset(); 
}

/**
 * @brief Sets the hover color for the button.
 * @param color The color to display when hovering over the button.
 */
void Button_t::set_hover_color(uint32_t color) { style.hover_color = color; }

/**
 * @brief Gets the button's group ID.
 * @return The group ID this button belongs to.
 */
int Button_t::get_group_id() const { return group_id; }

/**
 * @brief Renders the button to the screen.
 * @param renderer The SDL renderer to use.
 */
void Button_t::render(SDL_Renderer* renderer) {
    if (!visible) return;

    // Store current background color and temporarily set it based on hover state
    uint32_t original_bg_color = style.background_color;
    if (is_hovering) {
        style.background_color = style.hover_color;
    }

    // Call parent class render to draw background and border
    Tile_t::render(renderer);

    // Restore original background color
    style.background_color = original_bg_color;

    // Get content area position and size
    //float content_x, content_y;
    //get_content_position(&content_x, &content_y);

    // Draw button-specific content (text or image)
    if (buttonType == BT_Text) {
        if (text_render == nullptr) {
            SDL_SetRenderDrawColor(renderer,
                (style.text_color >> 24) & 0xFF,
                (style.text_color >> 16) & 0xFF,
                (style.text_color >> 8) & 0xFF,
                calc_opacity(style.text_color)
            );
            /* int wid = (strlen(text.c_str()) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
            int hei = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; */
            //SDL_RenderDebugText(renderer, tp.x + cp.x + (tp.w - wid) / 2, tp.y + cp.y + (tp.h - hei) / 2, text.c_str());
            SDL_RenderDebugText(renderer, tp.x + cp.x, tp.y + cp.y, text.c_str());
        } else {
            /* if (cp.w == -1) {
                cp.w = 
                cp.h = 
            } */

            text_render->set_color((style.text_color >> 24) & 0xFF, (style.text_color >> 16) & 0xFF, (style.text_color >> 8) & 0xFF, calc_opacity(style.text_color)); 
            //text_render->render(text, tp.x +cp.x + (cp.w /2), tp.y + cp.y, TEXT_ALIGN_CENTER);
            text_render->render(text, tp.x +cp.x, tp.y + cp.y, TEXT_ALIGN_LEFT);
        }
    } else if (buttonType == BT_Atlas) {
        aa->draw(assetID, tp.x + cp.x, tp.y + cp.y);
    }
}

/* void on_hover_changed(bool hovering) override {
    // Button-specific hover behavior could go here
    // For now, the base hover detection is sufficient
} */

void Button_t::on_click(const SDL_Event& event) {
    // Button-specific click behavior could go here
    // For now, just call the base class implementation
    Tile_t::on_click(event);
}

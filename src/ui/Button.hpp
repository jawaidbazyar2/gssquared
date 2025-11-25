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

#pragma once

#include <string>
#include "Tile.hpp"
#include "AssetAtlas.hpp"
#include "util/TextRenderer.hpp"

/**
 * @brief Enumeration of button types.
 */
enum ButtonType_t {
    BT_Text,    ///< Button displays text
    BT_Image,   ///< Button displays a single image
    BT_Atlas    ///< Button displays an image from an atlas
};

/**
 * @brief A button class that can display either text or an image.
 * 
 * Buttons are interactive UI elements that can be clicked and can display
 * either text or an image (but not both). They support different states
 * (active/inactive) and can change appearance on hover.
 */
class Button_t : public Tile_t {
protected:
    std::string text;           // Button text (if text button)
    AssetAtlas_t* aa = nullptr;
    int assetID = 0;
    int group_id = 0;          // For grouping related buttons
    ButtonType_t buttonType;    // Type of button (text or image)

    /**
     * @brief Updates button size based on the current asset.
     */
    void set_size_from_asset();

    /**
     * @brief Updates button size based on the current tile.
     */
    void set_content_size_from_tile();

    /**
     * @brief Updates button size based on the text.
     */
    void set_content_size_from_text();

public:
    /**
     * @brief Constructs a text button with style.
     * @param button_text The text to display
     * @param style The button's style settings
     * @param group The button group ID
     */
    Button_t(const std::string& button_text, const Style_t& style = Style_t(), int group = 0);
    
    /**
     * @brief Constructs a text button with style.
     * @param button_text The text to display
     * @param text_render Pointer to the text renderer
     * @param style The button's style settings
     * @param group The button group ID
     */
    Button_t(const std::string& button_text, TextRenderer *tr, const Style_t& style = Style_t(), int group = 0);

    /**
     * @brief Constructs an image button with style.
     * @param assetp Pointer to the asset atlas
     * @param assetID ID of the asset in the atlas
     * @param style The button's style settings
     * @param group The button group ID
     */
    Button_t(AssetAtlas_t* assetp, int assetID, const Style_t& style = Style_t(), int group = 0);

    /**
     * @brief Constructs a text button without style.
     * @param button_text The text to display
     * @param group The button group ID
     */
    Button_t(const std::string& button_text, int group = 0);

    /**
     * @brief Constructs an image button without style.
     * @param assetp Pointer to the asset atlas
     * @param assetID ID of the asset in the atlas
     * @param group The button group ID
     */
    Button_t(AssetAtlas_t* assetp, int assetID, int group = 0);

    /**
     * @brief Sets the asset ID for image buttons.
     * @param id The new asset ID
     */
    void set_assetID(int id);

    /**
     * @brief Sets the hover color for the button.
     * @param color The color to display when hovering
     */
    void set_hover_color(uint32_t color);

    /**
     * @brief Gets the button's group ID.
     * @return The group ID this button belongs to
     */
    int get_group_id() const;

    /**
     * @brief Renders the button.
     * @param renderer The SDL renderer to use
     */
    void render(SDL_Renderer* renderer) override;

protected:
    /**
     * @brief Called when the button is clicked.
     */
    void on_click(const SDL_Event& event) override;
}; 
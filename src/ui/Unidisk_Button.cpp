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

#include "Button.hpp"
#include "Unidisk_Button.hpp"
#include "MainAtlas.hpp"
#include "util/printf_helper.hpp"

/**
 * @brief A specialized button class for Unidisk drive interface.
 * 
 * This class extends Button_t to provide additional rendering capabilities
 * specific to Unidisk drive visualization. The base button rendering is preserved,
 * and additional visual elements can be drawn on top.
 */

/**
 * @brief Renders the Unidisk button with additional drive-specific elements.
 * @param renderer The SDL renderer to use.
 */
void Unidisk_Button_t::render(SDL_Renderer* renderer) {
    this->set_assetID(Unidisk_Face);
/*     if (status.is_mounted)    
    else this->set_assetID(Unidisk_Face);
 */
    // First, perform the base button rendering
    Button_t::render(renderer);

    // Get content area position for additional rendering
    //float content_x, content_y;
    //get_content_position(&content_x, &content_y);

    // Additional rendering can be added here
    if ((key & 0xFF) == 0) aa->draw(Unidisk_Drive1, tp.x + cp.x + 11, tp.y + cp.y + 31);
    else aa->draw(Unidisk_Drive2, tp.x + cp.x + 11, tp.y + cp.y + 31);
 
    if (status.motor_on) aa->draw(Unidisk_Light, tp.x + cp.x, tp.y + cp.y + 30);

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    char text[32];
    snprintf(text, sizeof(text), "Slot %llu", u64_t(key >> 8));
    SDL_RenderDebugText(renderer, tp.x + cp.x + 62, tp.y + cp.y + 75, text);
    
    if (is_hovering && !status.filename.empty()) {
        float text_width = (float)(status.filename.length() * 8);
        float text_x = (float)((174 - text_width) / 2);
        SDL_FRect rect = { tp.x + cp.x + text_x-5, tp.y + cp.y + 40, text_width+10, 16};
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0xFF, 0x80);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDebugText(renderer, tp.x + cp.x + text_x, tp.y + cp.y + 44, status.filename.c_str());
    }
}

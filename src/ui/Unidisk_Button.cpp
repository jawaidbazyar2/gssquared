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
void Unidisk_Button_t::render() {
    this->set_assetID(Unidisk_Face);
/*     if (status.is_mounted)    
    else this->set_assetID(Unidisk_Face);
 */
    // First, perform the base button rendering
    Button_t::render();

    // Additional rendering can be added here
    if (key.drive == 0) aa->draw(Unidisk_Drive1, tp.x + cp.x + 11, tp.y + cp.y + 31);
    else aa->draw(Unidisk_Drive2, tp.x + cp.x + 11, tp.y + cp.y + 31);
 
    if (status.motor_on) aa->draw(Unidisk_Light, tp.x + cp.x, tp.y + cp.y + 30);

    //char text[32];
    //snprintf(text, sizeof(text), "Slot %u", key.slot);
    //ctx->debug_text(text, tp.x + cp.x + 62, tp.y + cp.y + 75, 0x000000FF);
    
    if (/* is_hovering &&  */!status.filename.empty()) {
        float text_width = (float)(status.filename.length() * 8);
        float text_x = (float)((174 - text_width) / 2);
        SDL_FRect rect = { tp.x + cp.x + text_x-5, tp.y + cp.y + 40, text_width+10, 16};
        ctx->fill_rect(rect, 0x8080FF80);
        ctx->debug_text(status.filename.c_str(), tp.x + cp.x + text_x, tp.y + cp.y + 44, 0xFFFFFFFF);
    }

    char text[32];
    if (status.is_mounted && status.motor_on) {
        // if mounted and hovering, show the track number over the drive
        snprintf(text, sizeof(text), "%d/%d %04X", key.slot, key.drive+1, status.position / 2);
    } else {
        snprintf(text, sizeof(text), "%d/%d", key.slot, key.drive+1);
    }
    float text_width = (strlen(text) * 8.0);
    ctx->debug_text(text, tp.x + cp.x + 88 - (text_width/2), tp.y + cp.y + 75, 0x000000FF);
}

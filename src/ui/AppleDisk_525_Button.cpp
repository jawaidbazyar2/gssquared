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
 #include "AppleDisk_525_Button.hpp"
 #include "MainAtlas.hpp"
 #include "util/printf_helper.hpp"
 
 /**
  * @brief A specialized button class for DiskII drive interface.
  * 
  * This class extends Button_t to provide additional rendering capabilities
  * specific to DiskII drive visualization. The base button rendering is preserved,
  * and additional visual elements can be drawn on top.
  */

 /**
  * @brief Renders the DiskII button with additional drive-specific elements.
  * @param renderer The SDL renderer to use.
  */
 void AppleDisk_525_Button_t::render(SDL_Renderer* renderer) {
     if (status.is_mounted) this->set_assetID(AppleDisk_525_Closed);
     else this->set_assetID(AppleDisk_525_Open);
 
     // First, perform the base button rendering
     Button_t::render(renderer);
 
     // Get content area position for additional rendering
     //float content_x, content_y;
     //get_content_position(&content_x, &content_y);
 
     // Additional rendering can be added here
     // This space intentionally left empty for manual implementation
    aa->draw(
        (key & 0xFF) == 0 ? Unidisk_Drive1 : Unidisk_Drive2,
        tp.x + cp.x + 130, tp.y + cp.y + 24
    );

/*      if ((key & 0xFF) == 0) aa->draw(Unidisk_Drive1, tp.x + cp.x + 4, tp.y + cp.y + 4);
     else aa->draw(Unidisk_Drive2, tp.x + cp.x + 4, tp.y + cp.y + 4);
 */ 
     if (status.motor_on) aa->draw(DiskII_DriveLightOn, tp.x + cp.x + 150, tp.y + cp.y + 24);
 
     char text[32];
     snprintf(text, sizeof(text), "Slot %u", key.slot);
     SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
     SDL_RenderDebugText(renderer, tp.x + cp.x + 65, tp.y + cp.y + 78, text);
     if (is_hovering && !status.filename.empty()) {
         float text_width = (float)(status.filename.length() * 8);
         float text_x = (float)((174 - text_width) / 2);
         SDL_FRect rect = { tp.x + cp.x + text_x-5, tp.y + cp.y + 36, text_width+10, 16};
         SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0xFF, 0x80);
         SDL_RenderFillRect(renderer, &rect);
         SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
         SDL_RenderDebugText(renderer, tp.x + cp.x + text_x, tp.y + cp.y + 40, status.filename.c_str());
     }
     if (status.is_mounted && status.motor_on) {
         // if mounted and hovering, show the track number over the drive
         char text[32];
         snprintf(text, sizeof(text), "Tr %d", status.position / 2);
         SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
         SDL_RenderDebugText(renderer, tp.x + cp.x + 70, tp.y + cp.y + 18, text);
     }
 }
 
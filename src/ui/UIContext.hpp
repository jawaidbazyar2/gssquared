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
#include "util/TextRenderer.hpp"
#include "AssetAtlas.hpp"

/**
 * @brief Shared rendering context passed to all UI widgets.
 *
 * Bundles the SDL renderer, font renderers, and asset atlas so that
 * widget constructors receive a single context pointer instead of
 * individual rendering parameters.  UIContext does not own any of
 * the pointed-to objects.
 */
struct UIContext {
    SDL_Renderer *renderer      = nullptr;
    SDL_Window *window         = nullptr;
    TextRenderer *text_render   = nullptr;
    TextRenderer *title_trender = nullptr;
    AssetAtlas_t *asset_atlas   = nullptr;

    // In UIContext (or a companion DrawCtx helper)

    inline void color(uint32_t rgba) {
        SDL_SetRenderDrawColor(renderer,
            (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
            (rgba >> 8)  & 0xFF,  rgba & 0xFF);
    }

    inline void fill_rect(SDL_FRect rect, uint32_t rgba) {
        color(rgba);
        SDL_RenderFillRect(renderer, &rect);
    }

    inline void draw_rect(SDL_FRect rect, uint32_t rgba) {
        color(rgba);
        SDL_RenderRect(renderer, &rect);
    }
    inline void line(int x1, int y1, int x2, int y2, uint32_t rgba) {
        color(rgba);
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }

    inline void debug_text(const char *text, int x, int y, uint32_t rgba) {
        color(rgba);
        SDL_RenderDebugText(renderer, x, y, text);
    }
};

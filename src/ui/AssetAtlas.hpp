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

#include <SDL3/SDL.h>

/**
 * @brief A simple asset structure containing a texture and its rectangle.
 * 
 * Asset_t combines an SDL texture pointer with a rectangle defining the
 * area of the texture to use. This is typically used in conjunction with
 * AssetAtlas_t for sprite management.
 */
struct Asset_t {
    SDL_Texture *image = nullptr;
    SDL_FRect rect;
};

/**
 * @brief A texture atlas manager for efficient sprite/image handling.
 * 
 * AssetAtlas_t manages a single texture containing multiple sprites or images,
 * often called a texture atlas or sprite sheet. It provides functionality to:
 * - Load an image file as a texture
 * - Optionally scale the loaded image
 * - Define and manage multiple rectangular regions within the texture
 * - Draw specific regions of the texture
 * 
 * The class uses RAII principles to manage SDL resources and provides a safe
 * interface for texture atlas operations.
 */
class AssetAtlas_t {
private:
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *image = nullptr;
    int elementCount = 0;
    SDL_FRect *elements = nullptr;
    SDL_FRect default_element;

public:
    /**
     * @brief Constructs an asset atlas from an image file.
     * 
     * @param renderer The SDL renderer to use for texture creation
     * @param path Path to the image file to load
     * @param target_w Optional target width for scaling (0 for no scaling)
     * @param target_h Optional target height for scaling (0 for no scaling)
     * @throws std::runtime_error if image loading fails
     */
    AssetAtlas_t(SDL_Renderer *renderer, const char *path, int target_w = 0, int target_h = 0);

    /**
     * @brief Destroys the asset atlas and frees associated resources.
     */
    ~AssetAtlas_t();

    /**
     * @brief Sets the element rectangles for the atlas.
     * 
     * @param NumElements Number of elements in the Elements array
     * @param Elements Array of rectangles defining regions in the texture
     */
    void set_elements(int NumElements, SDL_FRect *Elements);

    /**
     * @brief Gets the rectangle for a specific element.
     * 
     * @param id The element ID to retrieve
     * @return SDL_FRect The rectangle defining the element's region
     */
    SDL_FRect get_rect(int id);

    /**
     * @brief Gets both the texture and rectangle for a specific element.
     * 
     * @param id The element ID to retrieve
     * @param asset Reference to an Asset_t to populate
     */
    void get_asset(int id, Asset_t &asset);

    /**
     * @brief Draws a specific element at the given position.
     * 
     * @param id The element ID to draw
     * @param dst_x X coordinate to draw at
     * @param dst_y Y coordinate to draw at
     * @throws std::runtime_error if id is invalid
     */
    void draw(int id, int dst_x, int dst_y);
}; 

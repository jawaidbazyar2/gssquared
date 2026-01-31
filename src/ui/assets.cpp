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
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <stdexcept>

#include "devices/displaypp/RGBA.hpp"

/**
 * @brief A class that manages SDL textures loaded from image files.
 * 
 * The asset_t class provides RAII-style management of SDL textures, automatically
 * handling texture loading and cleanup. It throws std::runtime_error if texture
 * loading fails.
 * 
 * Example usage:
 * @code
 * try {
 *     asset_t asset("path/to/image.png");
 *     SDL_FRect rect = asset.get_rect(100, 100);  // Get rect at position (100,100)
 *     SDL_RenderTexture(renderer, asset.image, NULL, &rect);
 * } catch (const std::runtime_error& e) {
 *     // Handle loading error
 * }
 * @endcode
 */
class asset_t {

public:
    SDL_Texture *image = nullptr;
    
    /**
     * @brief Constructs an asset by loading a texture from the specified path.
     * @param path The file path to the image to load.
     * @throws std::runtime_error if the texture cannot be loaded.
     */
    asset_t(SDL_Renderer* renderer, const char *path, int target_w = 0, int target_h = 0) {
        SDL_Surface* original = IMG_Load(path);
        if (!original) {
            throw std::runtime_error("Failed to load image");
        }
        
        if (target_w > 0 && target_h > 0) {
            SDL_Surface* scaled = SDL_CreateSurface(target_w, target_h, PIXEL_FORMAT);
            SDL_BlitSurfaceScaled(original, NULL, scaled, NULL, SDL_SCALEMODE_LINEAR);
            image = SDL_CreateTextureFromSurface(renderer, scaled);
            SDL_DestroySurface(scaled);
        } else {
            image = SDL_CreateTextureFromSurface(renderer, original);
        }
        SDL_DestroySurface(original);
    }
    
    /**
     * @brief Destroys the asset and frees the associated texture.
     */
    ~asset_t() {
        if (image) {
            SDL_DestroyTexture(image);
            image = nullptr;
        }
    }

    /**
     * @brief Gets the texture dimensions as an SDL_FRect at the specified position.
     * @param x The x-coordinate for the rectangle (defaults to 0.0f).
     * @param y The y-coordinate for the rectangle (defaults to 0.0f).
     * @return An SDL_FRect containing the texture dimensions at the specified position.
     */
    SDL_FRect get_rect(float x = 0.0f, float y = 0.0f) const {
        float w, h;
        SDL_GetTextureSize(image, &w, &h);
        return SDL_FRect{x, y, w, h};
    }
};

struct asset_info {
    int asset_id;
    const char *pathname;
    asset_t *asset;
    int target_w;
    int target_h;
};

class assets_t {
public:
    asset_info *assets = nullptr;
    size_t asset_count = 0;

    ~assets_t() {
        if (assets) {
            for (size_t i = 0; i < asset_count; i++) {
                delete assets[i].asset;
            }
            delete[] assets;
        }
    }

    asset_t* get_asset(int asset_id) {
        for (size_t i = 0; i < asset_count; i++) {
            if (assets[i].asset_id == asset_id) {
                return assets[i].asset;
            }
        }
        return nullptr;
    }

    static assets_t* load(SDL_Renderer* renderer, const asset_info *asset_list, size_t count) {
        assets_t *assets = new assets_t();
        
        try {
            assets->asset_count = count;
            assets->assets = new asset_info[count];
            
            for (size_t i = 0; i < count; i++) {
                assets->assets[i].asset_id = asset_list[i].asset_id;
                assets->assets[i].pathname = asset_list[i].pathname;
                assets->assets[i].target_w = asset_list[i].target_w;
                assets->assets[i].target_h = asset_list[i].target_h;
                assets->assets[i].asset = new asset_t(renderer, asset_list[i].pathname, asset_list[i].target_w, asset_list[i].target_h);
            }
        } catch (const std::runtime_error& e) {
            // Clean up any assets loaded so far
            for (size_t i = 0; i < count; i++) {
                if (assets->assets[i].asset) {
                    delete assets->assets[i].asset;
                }
            }
            delete[] assets->assets;
            delete assets;
            return nullptr;
        }
        
        return assets;
    }
};
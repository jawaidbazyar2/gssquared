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

#include <stdexcept>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>

#include "gs2.hpp"
#include "AssetAtlas.hpp"
#include "util/dialog.hpp"

AssetAtlas_t::AssetAtlas_t(SDL_Renderer *renderer, const char *path, int target_w, int target_h) : renderer(renderer)
{
    elementCount = 1;
    elements = &default_element;

    std::string filename_t;
    filename_t.assign(gs2_app_values.base_path);
    filename_t.append(path);

    SDL_Surface* original = IMG_Load(filename_t.c_str());
    if (!original) {
        char error_message[256];
        snprintf(error_message, sizeof(error_message), "Failed to load image: %s", path);
        system_failure(error_message);
    }
    
    if (target_w > 0 && target_h > 0) {
        SDL_Surface* scaled = SDL_CreateSurface(target_w, target_h, SDL_PIXELFORMAT_RGBA8888);
        SDL_BlitSurfaceScaled(original, NULL, scaled, NULL, SDL_SCALEMODE_LINEAR);
        image = SDL_CreateTextureFromSurface(renderer, scaled);
        SDL_DestroySurface(scaled);
        default_element = {0, 0, (float)target_w, (float)target_h};
    } else {
        image = SDL_CreateTextureFromSurface(renderer, original);
        default_element = {0, 0, (float)original->w, (float)original->h};
    }
    SDL_DestroySurface(original);
};

void AssetAtlas_t::set_elements(int NumElements, SDL_FRect *Elements)  {
    elementCount = NumElements;
    elements = Elements;
}

SDL_FRect AssetAtlas_t::get_rect(int id) {
    return elements[id];
}

void AssetAtlas_t::get_asset(int id, Asset_t &asset) {
    asset.image = image;
    asset.rect = elements[id];
}

void AssetAtlas_t::draw(int id, int dst_x, int dst_y) {
    if (id < 0 || id >= elementCount) {
        throw std::runtime_error("Invalid asset ID");
    }
    SDL_FRect srcrect = {(float)elements[id].x, (float)elements[id].y, (float)elements[id].w, (float)elements[id].h};
    SDL_FRect dstrect = {(float)dst_x, (float)dst_y, (float)elements[id].w, (float)elements[id].h};
    SDL_RenderTexture(renderer, image, &srcrect, &dstrect);
}

AssetAtlas_t::~AssetAtlas_t() {
    if (elements) {
        delete[] elements;
    }
}

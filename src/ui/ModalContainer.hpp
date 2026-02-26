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

#include <string>
#include "Container.hpp"
#include "util/TextRenderer.hpp"
#include "util/StorageDevice.hpp"

class ModalContainer_t : public Container_t {
private:
    std::string msg_text;
    storage_key_t key;
    uint64_t data;
    TextRenderer *text_render;
    
public:
    ModalContainer_t(SDL_Renderer *rendererp, TextRenderer *text_render, size_t max_tiles, const char* msg_text, const Style_t& initial_style);
    ModalContainer_t(SDL_Renderer *rendererp, TextRenderer *text_render, size_t max_tiles, const char* msg_text);
    
    void layout() override;
    void render() override;
    void set_key(storage_key_t key);
    storage_key_t get_key() const;
    void set_data(uint64_t data);
    uint64_t get_data() const;
};

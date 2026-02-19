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
#include "Button.hpp"
#include "StorageButton.hpp"
#include "util/mount.hpp"

/**
 * @brief A specialized button class for DiskII drive interface.
 * 
 * DiskII_Button_t extends Button_t to provide specific functionality for
 * disk drive controls, including disk mounting state and drive activity
 * indicators.
 */
class DiskII_Button_t : public StorageButton {
public:
    // Inherit constructors from StorageButton
    using StorageButton::StorageButton;

    // Override render to add disk-specific rendering
    void render(SDL_Renderer* renderer) override;
}; 
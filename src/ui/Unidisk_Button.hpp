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
#include "util/mount.hpp"

/**
 * @brief A specialized button class for Unidisk drive interface.
 * 
 * Unidisk_Button_t extends Button_t to provide specific functionality for
 * disk drive controls, including disk mounting state and drive activity
 * indicators.
 */
class Unidisk_Button_t : public Button_t {
protected:
    uint64_t key;
    drive_status_t status;

public:
    // Inherit constructors from Button_t
    using Button_t::Button_t;

    // Disk state setters and getters
    void set_disk_slot(int slot);
    int get_disk_slot() const;
    void set_disk_number(int num);
    int get_disk_number() const;
    void set_disk_status(drive_status_t status);

    void set_key(uint64_t k);
    uint64_t get_key() const;

    // Override render to add disk-specific rendering
    void render(SDL_Renderer* renderer) override;
}; 
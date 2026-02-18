/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "IWM_Drive.hpp"

// Default implementations for base class
bool IWM_Drive::mount(uint64_t key, media_descriptor *media) {
    // Base implementation does nothing - subclasses must override
    return false;
}

bool IWM_Drive::unmount(uint64_t key) {
    // Base implementation does nothing - subclasses must override
    return false;
}

bool IWM_Drive::writeback(uint64_t key) {
    // Base implementation does nothing - subclasses must override
    return false;
}

drive_status_t IWM_Drive::status(uint64_t key) {
    // Base implementation returns empty status
    return {false, "", false, 0, false};
}

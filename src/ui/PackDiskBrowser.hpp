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

#pragma once

#include "ModalContainer.hpp"
#include "util/StorageDevice.hpp"

class OSD;

/** Lists disk images in the active .gs2pack. Local files are a secondary action. */
class PackDiskBrowser_t : public ModalContainer_t {
public:
    PackDiskBrowser_t(UIContext *ctx, const Style_t& initial_style, modal_stack& stack,
                      OSD *osd, storage_key_t key);
    void layout() override;
};

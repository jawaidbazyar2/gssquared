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

#include "Container.hpp"

/**
 * @brief Container that lays out children like wrapping text.
 *
 * Each child tile is treated as an unsplittable "word": tiles flow left-to-right
 * on a line, and when the next tile would overflow the container width it wraps
 * to a new line. Container height is resized to fit the resulting content.
 */
class WrapContainer_t : public Container_t {
public:
    WrapContainer_t(UIContext *ctx, const Style_t &initial_style = Style_t());
    explicit WrapContainer_t(UIContext *ctx);

    void layout() override;
};

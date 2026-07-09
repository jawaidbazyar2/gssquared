/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include "StorageButton.hpp"
#include "Style.hpp"
#include "UIContext.hpp"
#include "util/mount.hpp"

/** Create a typed StorageButton for the given drive_type. Caller owns the pointer. */
StorageButton *create_storage_button(UIContext *ctx, drive_type_t drive_type, const Style_t& style);

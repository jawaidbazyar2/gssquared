/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "StorageButtonFactory.hpp"

#include <stdexcept>
#include <string>

#include "AppleDisk_35_Button.hpp"
#include "AppleDisk_525_Button.hpp"
#include "DiskII_Button.hpp"
#include "HD20SC_Button.hpp"

StorageButton *create_storage_button(UIContext *ctx, drive_type_t drive_type, const Style_t& style) {
    switch (drive_type) {
        case DRIVE_TYPE_DISKII:
            return new DiskII_Button_t(ctx, style);
        case DRIVE_TYPE_APPLEDISK_525:
            return new AppleDisk_525_Button_t(ctx, style);
        case DRIVE_TYPE_APPLEDISK_35:
            return new AppleDisk_35_Button_t(ctx, style);
        case DRIVE_TYPE_PRODOS_BLOCK:
            return new HD20SC_Button_t(ctx, style);
        default:
            throw std::runtime_error(std::string("Unknown drive type: ") + std::to_string(drive_type));
    }
}

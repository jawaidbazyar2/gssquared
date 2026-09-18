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

#include "util/BlankDisk.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

#include <SDL3/SDL.h>

#include "paths.hpp"

const char *blank_disk_resource_name(BlankDiskType type) {
    switch (type) {
        case BlankDiskType::Floppy525Unformatted: return "blank-525-unformatted.woz";
        case BlankDiskType::Floppy525Dos33:       return "blank-525-dos33.woz";
        case BlankDiskType::Floppy525Prodos:      return "blank-525-prodos.woz";
        case BlankDiskType::Floppy35Prodos:       return "blank-35-prodos.woz";
        case BlankDiskType::Hd32M:                return nullptr;
    }
    return nullptr;
}

const char *blank_disk_suggested_filename(BlankDiskType type) {
    switch (type) {
        case BlankDiskType::Floppy525Unformatted: return "Blank 5.25 Unformatted.woz";
        case BlankDiskType::Floppy525Dos33:       return "Blank 5.25 DOS 3.3.woz";
        case BlankDiskType::Floppy525Prodos:      return "Blank 5.25 ProDOS.woz";
        case BlankDiskType::Floppy35Prodos:       return "Blank 3.5 ProDOS.woz";
        case BlankDiskType::Hd32M:                return "Blank 32M HD.hdv";
    }
    return "Blank Disk";
}

const char *blank_disk_extension(BlankDiskType type) {
    return blank_disk_is_floppy(type) ? ".woz" : ".hdv";
}

bool blank_disk_is_floppy(BlankDiskType type) {
    return type != BlankDiskType::Hd32M;
}

std::string blank_disk_ensure_extension(BlankDiskType type, std::string path) {
    const char *ext = blank_disk_extension(type);
    if (!Paths::ends_with_icase(path, ext)) {
        path += ext;
    }
    return path;
}

static bool write_zero_hd(const std::string& dest, std::string& err) {
    {
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out) {
            err = "Could not create '" + dest + "'";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::resize_file(dest, static_cast<std::uintmax_t>(kBlankHd32MBytes), ec);
    if (ec) {
        err = "Could not size '" + dest + "': " + ec.message();
        std::error_code rec;
        std::filesystem::remove(dest, rec);
        return false;
    }
    return true;
}

bool write_blank_disk(BlankDiskType type, const std::string& dest_in, std::string& err) {
    err.clear();
    const std::string dest = blank_disk_ensure_extension(type, dest_in);

    if (type == BlankDiskType::Hd32M) {
        return write_zero_hd(dest, err);
    }

    const char *resource = blank_disk_resource_name(type);
    if (!resource) {
        err = "Unknown blank disk type";
        return false;
    }

    std::string src;
    Paths::calc_base(src, std::string("floppyimages/") + resource);
    if (!std::filesystem::exists(src)) {
        err = "Blank image template is missing: " + src;
        return false;
    }
    if (!SDL_CopyFile(src.c_str(), dest.c_str())) {
        err = std::string("Could not write '") + dest + "': " + SDL_GetError();
        return false;
    }
    return true;
}

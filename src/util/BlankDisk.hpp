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

#include <cstdint>
#include <string>

enum class BlankDiskType {
    Floppy525Unformatted,
    Floppy525Dos33,
    Floppy525Prodos,
    Floppy35Prodos,
    Hd32M,
    Hd32MProdos,
};

static constexpr uintmax_t kBlankHd32MBytes = 65536ull * 512ull; // 33,554,432

/* ProDOS can only address 65535 blocks, so the last block of a 32M image is
   left outside the volume. */
static constexpr uint32_t kBlankHd32MProdosBlocks = 65535;

const char *blank_disk_resource_name(BlankDiskType type);
const char *blank_disk_suggested_filename(BlankDiskType type);
const char *blank_disk_extension(BlankDiskType type); // ".woz" or ".hdv"
bool blank_disk_is_floppy(BlankDiskType type);

/** Append the type's extension if `path` does not already end with it (case-insensitive). */
std::string blank_disk_ensure_extension(BlankDiskType type, std::string path);

/**
 * Write a blank image to dest. Floppies copy a shipped WOZ template;
 * 32M HD creates a zero-filled .hdv, optionally carrying a freshly formatted
 * (empty) ProDOS volume. On failure, err is set and nothing usable is left at
 * dest (partial HD files are removed).
 */
bool write_blank_disk(BlankDiskType type, const std::string& dest, std::string& err);

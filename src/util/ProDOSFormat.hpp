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

/** ProDOS total_blocks is a 16-bit field, so 65535 blocks is the ceiling. */
static constexpr uint32_t kProDOSMaxBlocks = 65535;

/** Fallback volume name when one can't be derived from the filename. */
static constexpr const char *kProDOSDefaultVolumeName = "BLANK";

/**
 * Derive a legal ProDOS volume name from a file path: the stem, upper-cased,
 * with runs of illegal characters collapsed to a single period. Returns an
 * empty string when the result would not be a legal volume name (empty, does
 * not start with a letter, or longer than 15 characters) — callers should
 * substitute kProDOSDefaultVolumeName.
 */
std::string prodos_volume_name_from_path(const std::string& path);

/**
 * Lay a freshly formatted, empty ProDOS volume over the start of an existing
 * block image: boot block, 4-block volume directory, and volume bitmap. The
 * file must already be at least total_blocks * 512 bytes; only the first
 * 6 + bitmap blocks are written, so the rest of a sparse file stays sparse.
 *
 * volume_name must be a legal ProDOS name. On failure, err is set.
 */
bool prodos_format_image(const std::string& path, uint32_t total_blocks,
                         const std::string& volume_name, std::string& err);

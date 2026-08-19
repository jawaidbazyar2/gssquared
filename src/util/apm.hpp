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

#include <cstdint>
#include <string>
#include <vector>

#include "media.hpp"

struct apm_partition {
    std::string name;
    std::string type;
    uint32_t start_block = 0;       /* in DDM block-size units */
    uint32_t block_count = 0;       /* in DDM block-size units */
    uint64_t byte_offset = 0;       /* absolute file offset of partition data */
    uint64_t byte_length = 0;       /* bytes after EOF clip */
    uint32_t guest_block_count = 0; /* 512-byte ProDOS/HFS blocks */
    bool mountable = false;
};

struct apm_map {
    uint16_t block_size = 0;        /* DDM sbBlkSize: 512 or 2048 */
    uint32_t map_entries = 0;
    std::vector<apm_partition> partitions;
};

/** True if the image contains a recognizable Apple Partition Map at data_offset. */
bool read_apm(const media_descriptor& container, apm_map& out);

/**
 * If the container is APM, fill parts with one MEDIA_BLK descriptor per
 * mountable ProDOS/HFS slice and return true (parts may be empty).
 * Return false if the image is not APM.
 */
bool probe_apm(const media_descriptor& container, std::vector<media_descriptor>& parts);

void display_apm_map(const apm_map& map);

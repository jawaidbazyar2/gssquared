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

#include "util/ProDOSFormat.hpp"

#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "paths.hpp"

namespace {

constexpr uint32_t kBlockSize = 512;
constexpr uint32_t kVolumeDirBlock = 2;   // blocks 2-5 are the volume directory
constexpr uint32_t kVolumeDirBlocks = 4;
constexpr uint32_t kBitmapBlock = 6;      // bitmap follows the volume directory
constexpr uint32_t kBlocksPerBitmapBlock = kBlockSize * 8;
constexpr size_t kMaxVolumeNameLen = 15;

void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

struct ProDOSDateTime { uint16_t date; uint16_t time; };

ProDOSDateTime prodos_now() {
    std::time_t now = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    /* The year is 7 bits. Per Apple II Technical Note #28, 0-39 means 20xx and
       40-99 means 19xx. */
    int year = lt.tm_year + 1900;
    int year7 = (year >= 2000) ? (year - 2000) : (year - 1900);

    ProDOSDateTime dt;
    dt.date = (uint16_t)(((year7 & 0x7F) << 9) | (((lt.tm_mon + 1) & 0x0F) << 5) | (lt.tm_mday & 0x1F));
    dt.time = (uint16_t)(((lt.tm_hour & 0x1F) << 8) | (lt.tm_min & 0x3F));
    return dt;
}

/** Copy the shipped ProDOS boot loader into block 0. Missing resource just
    leaves a zeroed (unbootable) boot block; the volume is still valid. */
void write_boot_block(uint8_t *block0) {
    std::string src;
    Paths::calc_base(src, "floppyimages/prodos-boot.bin");
    std::ifstream in(src, std::ios::binary);
    if (!in || !in.read((char *)block0, kBlockSize)) {
        std::cerr << "ProDOS format: boot block resource missing (" << src
                  << "); volume will not be bootable\n";
        std::memset(block0, 0, kBlockSize);
    }
}

void mark_block_used(uint8_t *bitmap, uint32_t block) {
    bitmap[block >> 3] &= (uint8_t)~(0x80 >> (block & 7));
}

} // namespace

std::string prodos_volume_name_from_path(const std::string& path) {
    const std::string stem = std::filesystem::path(path).stem().string();

    std::string name;
    bool pending_separator = false;
    for (unsigned char c : stem) {
        if (std::isalnum(c)) {
            if (pending_separator && !name.empty()) name += '.';
            pending_separator = false;
            name += (char)std::toupper(c);
        } else {
            pending_separator = true;
        }
    }

    if (name.empty() || name.size() > kMaxVolumeNameLen) return "";
    if (!std::isalpha((unsigned char)name[0])) return "";
    return name;
}

bool prodos_format_image(const std::string& path, uint32_t total_blocks,
                         const std::string& volume_name, std::string& err) {
    const uint32_t bitmap_blocks = (total_blocks + kBlocksPerBitmapBlock - 1) / kBlocksPerBitmapBlock;
    const uint32_t reserved_blocks = kBitmapBlock + bitmap_blocks;

    if (total_blocks <= reserved_blocks || total_blocks > kProDOSMaxBlocks) {
        err = "Cannot format a ProDOS volume of " + std::to_string(total_blocks) + " blocks";
        return false;
    }
    if (volume_name.empty() || volume_name.size() > kMaxVolumeNameLen) {
        err = "Invalid ProDOS volume name '" + volume_name + "'";
        return false;
    }

    /* Everything ProDOS needs lives in the first few blocks; the rest of the
       volume is legitimately zeros, so we never touch it. */
    std::vector<uint8_t> prefix(reserved_blocks * kBlockSize, 0);
    write_boot_block(prefix.data());

    const ProDOSDateTime dt = prodos_now();

    uint8_t *vol = prefix.data() + kVolumeDirBlock * kBlockSize;
    put16(vol + 0x00, 0);                         // previous directory block
    put16(vol + 0x02, (uint16_t)(kVolumeDirBlock + 1));
    vol[0x04] = (uint8_t)(0xF0 | volume_name.size());  // storage type $F + name length
    std::memcpy(vol + 0x05, volume_name.data(), volume_name.size());
    put16(vol + 0x16, dt.date);                   // last modification (GS/OS)
    put16(vol + 0x18, dt.time);
    put16(vol + 0x1C, dt.date);                   // creation
    put16(vol + 0x1E, dt.time);
    vol[0x20] = 0x00;                             // version
    vol[0x21] = 0x00;                             // min_version
    vol[0x22] = 0xC3;                             // access: destroy/rename/write/read
    vol[0x23] = 0x27;                             // entry_length
    vol[0x24] = 0x0D;                             // entries_per_block
    put16(vol + 0x25, 0);                         // file_count
    put16(vol + 0x27, (uint16_t)kBitmapBlock);
    put16(vol + 0x29, (uint16_t)total_blocks);

    for (uint32_t i = 1; i < kVolumeDirBlocks; i++) {
        uint8_t *dir = prefix.data() + (kVolumeDirBlock + i) * kBlockSize;
        put16(dir + 0x00, (uint16_t)(kVolumeDirBlock + i - 1));
        put16(dir + 0x02, (uint16_t)(i + 1 < kVolumeDirBlocks ? kVolumeDirBlock + i + 1 : 0));
    }

    uint8_t *bitmap = prefix.data() + kBitmapBlock * kBlockSize;
    const uint32_t bitmap_bits = bitmap_blocks * kBlocksPerBitmapBlock;
    std::memset(bitmap, 0xFF, bitmap_blocks * kBlockSize);
    for (uint32_t b = 0; b < reserved_blocks; b++) mark_block_used(bitmap, b);
    for (uint32_t b = total_blocks; b < bitmap_bits; b++) mark_block_used(bitmap, b);

    std::fstream out(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!out) {
        err = "Could not open '" + path + "' to format it";
        return false;
    }
    out.seekp(0);
    out.write((const char *)prefix.data(), (std::streamsize)prefix.size());
    out.flush();
    if (!out) {
        err = "Could not write the ProDOS volume structures to '" + path + "'";
        return false;
    }
    return true;
}

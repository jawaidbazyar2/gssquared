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

#include "apm.hpp"

#include <cstdio>
#include <iostream>

static constexpr uint16_t APM_DDM_SIG = 0x4552; /* 'ER' */
static constexpr uint16_t APM_PM_SIG  = 0x504D; /* 'PM' */
static constexpr uint32_t APM_MAX_ENTRIES = 128;

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static std::string cstr32(const uint8_t *p) {
    size_t n = 0;
    while (n < 32 && p[n] != 0) n++;
    return std::string(reinterpret_cast<const char *>(p), n);
}

static bool read_at(FILE *fp, uint64_t offset, uint8_t *buf, size_t len) {
    if (fseek(fp, (long)offset, SEEK_SET) != 0) return false;
    return fread(buf, 1, len, fp) == len;
}

static bool is_mountable_type(const std::string& type) {
    return type == "Apple_PRODOS" || type == "Apple_ProDOS" || type == "Apple_HFS";
}

static bool is_known_skip_type(const std::string& type) {
    if (type == "Apple_partition_map" || type == "Apple_Free" ||
        type == "Apple_Void" || type == "Apple_Scratch" || type == "Apple_Boot") {
        return true;
    }
    return type.rfind("Apple_Driver", 0) == 0;
}

static bool pm_at(FILE *fp, uint64_t offset) {
    uint8_t sig[2];
    if (!read_at(fp, offset, sig, 2)) return false;
    return be16(sig) == APM_PM_SIG;
}

bool read_apm(const media_descriptor& container, apm_map& out) {
    out = apm_map{};
    if (container.filename.empty()) return false;

    FILE *fp = fopen(container.filename.c_str(), "rb");
    if (!fp) return false;

    const uint64_t base = container.data_offset;
    uint64_t data_end = container.file_size;
    if (container.data_size > 0) {
        uint64_t claimed = base + container.data_size;
        if (claimed < data_end) data_end = claimed;
    }

    uint8_t ddm[512];
    if (!read_at(fp, base, ddm, sizeof(ddm)) || be16(ddm) != APM_DDM_SIG) {
        fclose(fp);
        return false;
    }

    uint16_t blksize = be16(ddm + 2);
    if (blksize != 512 && blksize != 2048) {
        fclose(fp);
        return false;
    }

    /* PM usually sits at +blksize. Cooked 2048-byte CD sectors may pad the
       rest of sector 0 and put the first PM at +2048. */
    uint64_t pm0 = base + blksize;
    uint64_t pm_stride = blksize;
    if (!pm_at(fp, pm0) && blksize == 512 && pm_at(fp, base + 2048)) {
        pm0 = base + 2048;
        pm_stride = 2048;
    }
    if (!pm_at(fp, pm0)) {
        fclose(fp);
        return false;
    }

    uint8_t pmb[512];
    if (!read_at(fp, pm0, pmb, sizeof(pmb))) {
        fclose(fp);
        return false;
    }

    uint32_t map_entries = be32(pmb + 4);
    if (map_entries == 0 || map_entries > APM_MAX_ENTRIES) {
        fclose(fp);
        return false;
    }

    out.block_size = blksize;
    out.map_entries = map_entries;
    out.partitions.reserve(map_entries);

    for (uint32_t i = 0; i < map_entries; i++) {
        uint64_t off = pm0 + (uint64_t)i * pm_stride;
        if (!read_at(fp, off, pmb, sizeof(pmb)) || be16(pmb) != APM_PM_SIG) {
            break;
        }

        apm_partition p;
        p.start_block = be32(pmb + 8);
        p.block_count = be32(pmb + 12);
        p.name = cstr32(pmb + 16);
        p.type = cstr32(pmb + 48);
        p.byte_offset = base + (uint64_t)p.start_block * blksize;
        p.byte_length = (uint64_t)p.block_count * blksize;

        if (p.byte_offset >= data_end) {
            p.byte_length = 0;
        } else if (p.byte_offset + p.byte_length > data_end) {
            p.byte_length = data_end - p.byte_offset;
        }
        p.byte_length -= p.byte_length % 512;
        p.guest_block_count = (uint32_t)(p.byte_length / 512);

        if (is_mountable_type(p.type) && p.guest_block_count > 0) {
            p.mountable = true;
        } else if (!is_known_skip_type(p.type) && !p.type.empty()) {
            std::cerr << "APM: skipping partition '" << p.name
                      << "' type '" << p.type << "'\n";
        }

        out.partitions.push_back(std::move(p));
    }

    fclose(fp);
    return !out.partitions.empty();
}

bool probe_apm(const media_descriptor& container, std::vector<media_descriptor>& parts) {
    parts.clear();
    apm_map map;
    if (!read_apm(container, map)) return false;

    for (const apm_partition& p : map.partitions) {
        if (!p.mountable) continue;
        media_descriptor md = container;
        md.fp = nullptr;
        md.media_type = MEDIA_BLK;
        md.interleave = INTERLEAVE_NONE;
        md.block_size = 512;
        md.data_offset = p.byte_offset;
        md.data_size = p.byte_length;
        md.block_count = p.guest_block_count;
        md.filestub = p.name.empty() ? p.type : p.name;
        parts.push_back(std::move(md));
    }
    return true;
}

void display_apm_map(const apm_map& map) {
    std::cout << "Apple Partition Map, " << map.block_size << "-byte blocks, "
              << map.map_entries << " map entries\n";
    std::cout << "#  Start        Count  Mount  Type                     Name\n";
    int n = 1;
    for (const apm_partition& p : map.partitions) {
        std::cout << n++ << "  " << p.start_block << "  " << p.block_count
                  << "  " << (p.mountable ? "yes" : "no")
                  << "  " << p.type << "  " << p.name
                  << "  (offset " << p.byte_offset
                  << ", " << p.guest_block_count << " x 512)\n";
    }
}

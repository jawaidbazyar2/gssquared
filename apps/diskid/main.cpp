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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "util/apm.hpp"
#include "util/media.hpp"
#include "util/woz.hpp"

/* Required by gs2_devices_diskii_fmt (pulled in transitively via gs2_woz). */
uint64_t debug_level = 0;

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [-v] <filename>\n", prog);
    fprintf(stderr, "       %s --self-test\n", prog);
    fprintf(stderr, "  -v          Verbose: for WOZ files, also print TMAP and per-track stats\n");
    fprintf(stderr, "  --self-test Run built-in APM fixture tests\n");
}

static void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static void write_pm(FILE *fp, uint64_t offset, uint32_t map_cnt,
                     uint32_t start, uint32_t count,
                     const char *name, const char *type) {
    uint8_t rec[512];
    memset(rec, 0, sizeof(rec));
    put_be16(rec, 0x504D);
    put_be32(rec + 4, map_cnt);
    put_be32(rec + 8, start);
    put_be32(rec + 12, count);
    strncpy(reinterpret_cast<char *>(rec + 16), name, 31);
    strncpy(reinterpret_cast<char *>(rec + 48), type, 31);
    fseek(fp, (long)offset, SEEK_SET);
    fwrite(rec, 1, sizeof(rec), fp);
}

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            return false; \
        } \
    } while (0)

static bool test_apm_512() {
    const auto path = std::filesystem::temp_directory_path() / "gssquared_apm_512.iso";
    const uint16_t blk = 512;
    const uint32_t total = 40;
    FILE *fp = fopen(path.c_str(), "wb");
    CHECK(fp, "create 512-byte APM fixture");
    std::vector<uint8_t> zeros((size_t)total * blk, 0);
    fwrite(zeros.data(), 1, zeros.size(), fp);

    uint8_t ddm[512];
    memset(ddm, 0, sizeof(ddm));
    put_be16(ddm, 0x4552);
    put_be16(ddm + 2, blk);
    put_be32(ddm + 4, total);
    fseek(fp, 0, SEEK_SET);
    fwrite(ddm, 1, sizeof(ddm), fp);

    write_pm(fp, 1 * blk, 3, 1, 3, "Apple", "Apple_partition_map");
    write_pm(fp, 2 * blk, 3, 4, 32, "TEST.PRODOS", "Apple_PRODOS");
    write_pm(fp, 3 * blk, 3, 36, 4, "Extra", "Apple_Free");
    fclose(fp);

    media_descriptor md;
    md.filename = path.string();
    CHECK(identify_media(md) == 0, "identify 512 APM iso");
    CHECK(md.write_protected, ".iso is write-protected");
    CHECK(md.smartport_device_type == SP_DEVICE_CDROM, ".iso is SmartPort CD-ROM");
    CHECK(md.media_type == MEDIA_BLK, "MEDIA_BLK");
    CHECK(md.block_size == 512, "512-byte blocks");

    apm_map map;
    CHECK(read_apm(md, map), "read_apm 512");
    CHECK(map.block_size == 512, "map block size");
    CHECK(map.partitions.size() == 3, "3 partitions");
    CHECK(map.partitions[0].type == "Apple_partition_map" && !map.partitions[0].mountable,
          "skip partition map");
    CHECK(map.partitions[1].type == "Apple_PRODOS" && map.partitions[1].mountable,
          "ProDOS mountable");
    CHECK(map.partitions[1].name == "TEST.PRODOS", "ProDOS name");
    CHECK(map.partitions[1].start_block == 4, "ProDOS start");
    CHECK(map.partitions[1].block_count == 32, "ProDOS count");
    CHECK(map.partitions[1].byte_offset == 4ull * 512, "ProDOS byte offset");
    CHECK(map.partitions[1].guest_block_count == 32, "ProDOS guest blocks");
    CHECK(map.partitions[2].type == "Apple_Free" && !map.partitions[2].mountable,
          "skip Apple_Free");

    std::vector<media_descriptor> parts;
    CHECK(probe_apm(md, parts), "probe_apm 512");
    CHECK(parts.size() == 1, "one mountable slice");
    CHECK(parts[0].filestub == "TEST.PRODOS", "slice filestub");
    CHECK(parts[0].data_offset == 4ull * 512, "slice offset");
    CHECK(parts[0].block_count == 32, "slice blocks");
    CHECK(parts[0].write_protected, "slice inherits iso WP");
    CHECK(parts[0].smartport_device_type == SP_DEVICE_CDROM, "slice inherits CD-ROM type");

    std::filesystem::remove(path);
    return true;
}

static bool test_apm_2048() {
    const auto path = std::filesystem::temp_directory_path() / "gssquared_apm_2048.hdv";
    const uint16_t blk = 2048;
    const uint32_t total = 7;
    FILE *fp = fopen(path.c_str(), "wb");
    CHECK(fp, "create 2048-byte APM fixture");
    std::vector<uint8_t> zeros((size_t)total * blk, 0);
    fwrite(zeros.data(), 1, zeros.size(), fp);

    uint8_t ddm[512];
    memset(ddm, 0, sizeof(ddm));
    put_be16(ddm, 0x4552);
    put_be16(ddm + 2, blk);
    put_be32(ddm + 4, total);
    fseek(fp, 0, SEEK_SET);
    fwrite(ddm, 1, sizeof(ddm), fp);

    write_pm(fp, 1 * blk, 3, 1, 3, "Apple", "Apple_partition_map");
    write_pm(fp, 2 * blk, 3, 4, 2, "HFS.VOL", "Apple_HFS");
    write_pm(fp, 3 * blk, 3, 6, 1, "Extra", "Apple_Free");
    fclose(fp);

    media_descriptor md;
    md.filename = path.string();
    CHECK(identify_media(md) == 0, "identify 2048 APM hdv");
    CHECK(!md.write_protected, ".hdv is not forced WP");
    CHECK(md.smartport_device_type == SP_DEVICE_HARDDISK, ".hdv is SmartPort hard disk");

    apm_map map;
    CHECK(read_apm(md, map), "read_apm 2048");
    CHECK(map.block_size == 2048, "2048 map blocks");
    CHECK(map.partitions.size() == 3, "3 partitions");
    CHECK(map.partitions[1].mountable, "HFS mountable");
    CHECK(map.partitions[1].byte_offset == 4ull * 2048, "HFS byte offset");
    CHECK(map.partitions[1].guest_block_count == 8, "2 x 2048 = 8 x 512");

    std::vector<media_descriptor> parts;
    CHECK(probe_apm(md, parts), "probe_apm 2048");
    CHECK(parts.size() == 1, "one HFS slice");
    CHECK(parts[0].block_size == 512, "guest 512");
    CHECK(parts[0].block_count == 8, "guest block count");
    CHECK(parts[0].filestub == "HFS.VOL", "HFS name");

    std::filesystem::remove(path);
    return true;
}

static bool test_apm_cooked_cd() {
    /* DDM says 512, first PM is at offset 2048 (padded CD sector 0). */
    const auto path = std::filesystem::temp_directory_path() / "gssquared_apm_cooked.iso";
    const uint64_t file_size = 16ull * 2048;
    FILE *fp = fopen(path.c_str(), "wb");
    CHECK(fp, "create cooked APM fixture");
    std::vector<uint8_t> zeros((size_t)file_size, 0);
    fwrite(zeros.data(), 1, zeros.size(), fp);

    uint8_t ddm[512];
    memset(ddm, 0, sizeof(ddm));
    put_be16(ddm, 0x4552);
    put_be16(ddm + 2, 512);
    put_be32(ddm + 4, (uint32_t)(file_size / 512));
    fseek(fp, 0, SEEK_SET);
    fwrite(ddm, 1, sizeof(ddm), fp);

    write_pm(fp, 2048, 2, 4, 4, "Apple", "Apple_partition_map");
    write_pm(fp, 4096, 2, 8, 16, "GO.ProDOS", "Apple_PRODOS");
    fclose(fp);

    media_descriptor md;
    md.filename = path.string();
    CHECK(identify_media(md) == 0, "identify cooked iso");

    apm_map map;
    CHECK(read_apm(md, map), "read_apm cooked");
    CHECK(map.partitions.size() == 2, "2 partitions");
    CHECK(map.partitions[1].mountable, "ProDOS mountable");
    CHECK(map.partitions[1].byte_offset == 8ull * 512, "start still 512-byte units");
    CHECK(map.partitions[1].guest_block_count == 16, "16 guest blocks");

    std::vector<media_descriptor> parts;
    CHECK(probe_apm(md, parts), "probe cooked");
    CHECK(parts.size() == 1 && parts[0].filestub == "GO.ProDOS", "cooked slice");

    std::filesystem::remove(path);
    return true;
}

static bool test_not_apm() {
    const auto path = std::filesystem::temp_directory_path() / "gssquared_plain.hdv";
    FILE *fp = fopen(path.c_str(), "wb");
    CHECK(fp, "create plain hdv");
    std::vector<uint8_t> zeros(64 * 512, 0);
    fwrite(zeros.data(), 1, zeros.size(), fp);
    fclose(fp);

    media_descriptor md;
    md.filename = path.string();
    CHECK(identify_media(md) == 0, "identify plain hdv");
    std::vector<media_descriptor> parts;
    CHECK(!probe_apm(md, parts), "plain hdv is not APM");
    CHECK(parts.empty(), "no slices");

    std::filesystem::remove(path);
    return true;
}

static int run_self_test() {
    int fails = 0;
    if (!test_apm_512()) fails++;
    if (!test_apm_2048()) fails++;
    if (!test_apm_cooked_cd()) fails++;
    if (!test_not_apm()) fails++;
    if (fails) {
        std::cerr << fails << " APM self-test(s) failed\n";
        return 1;
    }
    std::cout << "APM self-test passed\n";
    return 0;
}

static int identify_file(const char *filename, bool verbose) {
    media_descriptor md;
    md.filename = filename;

    if (identify_media(md) != 0) {
        std::cerr << "Failed to identify media: " << md.filename << std::endl;
        return 1;
    }
    display_media_descriptor(md);

    if (md.media_type == MEDIA_BLK) {
        apm_map map;
        if (read_apm(md, map)) {
            std::cout << "\n--- APM ---\n";
            display_apm_map(map);
        }
    }

    if (md.media_type == MEDIA_WOZ) {
        Woz woz;
        if (woz.load(md.filename) == 0) {
            printf("\n--- WOZ detail ---\n");
            woz.dump_info();
            if (verbose) {
                printf("\n");
                woz.dump_tmap();
                printf("\n");
                woz.dump_tracks();
            }
        } else {
            std::cerr << "Warning: could not parse WOZ chunks in " << md.filename << "\n";
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--self-test") {
        return run_self_test();
    }

    bool verbose = false;
    int opt;

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v': verbose = true; break;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    return identify_file(argv[optind], verbose);
}

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

#include "util/Gs2Pack.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            return false; \
        } \
    } while (0)

static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static bool test_round_trip() {
    const auto dir = std::filesystem::temp_directory_path() / "gs2packtest-round";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto pack = (dir / "demo.gs2pack").string();
    std::string error;
    CHECK(gs2pack::write_archive(pack,
                                 {{"machine.gs2", "name = \"Demo\"\n"},
                                  {"disks/boot.woz", "BOOT"},
                                  {"disks/data.hdv", "DATA"}},
                                 error),
          error);

    gs2pack::Session session;
    CHECK(gs2pack::extract(pack, session, error), error);
    CHECK(session.members.size() == 3, "member count");
    CHECK(read_file(gs2pack::machine_gs2_path(session)) == "name = \"Demo\"\n", "machine.gs2");
    std::string member;
    CHECK(gs2pack::read_member(pack, "machine.gs2", member, error), error);
    CHECK(member == "name = \"Demo\"\n", "read_member machine.gs2");

    const auto images = gs2pack::list_disk_images(session);
    CHECK(images.size() == 2, "disk count");
    bool wrote = false;
    for (const auto& image : images) {
        if (std::filesystem::path(image).filename() == "boot.woz") {
            std::ofstream out(image, std::ios::binary | std::ios::trunc);
            out << "BOOT2";
            wrote = true;
        }
    }
    CHECK(wrote, "found boot.woz");
    CHECK(gs2pack::rewrite(session, error), error);

    gs2pack::Session again;
    CHECK(gs2pack::extract(pack, again, error), error);
    std::string boot;
    for (const auto& member : again.members) {
        if (member.name == "disks/boot.woz") {
            boot = read_file(member.path);
        }
        if (member.name == "machine.gs2") {
            CHECK(read_file(member.path) == "name = \"Demo\"\n", "config preserved");
        }
    }
    CHECK(boot == "BOOT2", "rewritten boot image");
    gs2pack::cleanup(session);
    gs2pack::cleanup(again);
    std::filesystem::remove_all(dir);
    return true;
}

static unsigned header_checksum(const unsigned char block[512]) {
    unsigned sum = 0;
    for (size_t i = 0; i < 512; ++i) {
        if (i >= 148 && i < 156) {
            sum += static_cast<unsigned char>(' ');
        } else {
            sum += block[i];
        }
    }
    return sum;
}

static void put_octal(unsigned char *field, size_t len, uint64_t value) {
    std::memset(field, '0', len);
    field[len - 1] = '\0';
    for (size_t i = len - 1; i-- > 0;) {
        field[i] = static_cast<unsigned char>('0' + (value & 7u));
        value >>= 3;
        if (i == 0) {
            break;
        }
    }
}

static void finish_header(unsigned char block[512], const char *name, uint64_t size) {
    std::memset(block, 0, 512);
    std::memcpy(block, name, std::strlen(name));
    put_octal(block + 100, 8, 0644);
    put_octal(block + 108, 8, 0);
    put_octal(block + 116, 8, 0);
    put_octal(block + 124, 12, size);
    put_octal(block + 136, 12, 0);
    std::memset(block + 148, ' ', 8);
    block[156] = '0';
    std::memcpy(block + 257, "ustar", 5);
    block[263] = '0';
    block[264] = '0';
    const unsigned sum = header_checksum(block);
    std::snprintf(reinterpret_cast<char *>(block + 148), 8, "%06o", sum);
    block[154] = '\0';
    block[155] = ' ';
}

static bool test_rejects_escape_and_missing_config() {
    const auto dir = std::filesystem::temp_directory_path() / "gs2packtest-bad";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string error;

    const auto missing = (dir / "missing.gs2pack").string();
    CHECK(gs2pack::write_archive(missing, {{"disks/boot.woz", "BOOT"}}, error), error);
    gs2pack::Session session;
    CHECK(!gs2pack::extract(missing, session, error), "missing machine.gs2 should fail");
    CHECK(error.find("machine.gs2") != std::string::npos, error);

    unsigned char block[512];
    finish_header(block, "../evil.woz", 4);
    const auto evil = dir / "evil.gs2pack";
    {
        std::ofstream out(evil, std::ios::binary);
        out.write(reinterpret_cast<const char *>(block), 512);
        out.write("EVIL", 4);
        std::vector<char> pad(512 - 4, 0);
        out.write(pad.data(), static_cast<std::streamsize>(pad.size()));
        std::vector<char> end(1024, 0);
        out.write(end.data(), static_cast<std::streamsize>(end.size()));
    }
    CHECK(!gs2pack::extract(evil.string(), session, error), "escape should fail");
    CHECK(error.find("escapes") != std::string::npos, error);
    CHECK(!std::filesystem::exists(dir / "evil.woz"), "escaped file was written");
    CHECK(!std::filesystem::exists(dir.parent_path() / "evil.woz"), "escaped file was written above");

    finish_header(block, "/abs.woz", 1);
    const auto abs_path = dir / "abs.gs2pack";
    {
        std::ofstream out(abs_path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(block), 512);
    }
    CHECK(!gs2pack::validate(abs_path.string(), error), "absolute name should fail");
    CHECK(error.find("absolute") != std::string::npos, error);

    std::filesystem::remove_all(dir);
    return true;
}

static bool test_size_cap_does_not_read_body() {
    const auto dir = std::filesystem::temp_directory_path() / "gs2packtest-cap";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    unsigned char block[512];
    finish_header(block, "machine.gs2", gs2pack::kMaxBytes + 1);
    const auto pack = dir / "huge.gs2pack";
    {
        std::ofstream out(pack, std::ios::binary);
        out.write(reinterpret_cast<const char *>(block), 512);
    }
    CHECK(std::filesystem::file_size(pack) == 512, "fixture is header only");
    std::string error;
    CHECK(!gs2pack::validate(pack.string(), error), "oversize header should fail");
    CHECK(error.find("200 MB") != std::string::npos, error);
    gs2pack::Session session;
    CHECK(!gs2pack::extract(pack.string(), session, error), "oversize extract should fail");
    CHECK(error.find("200 MB") != std::string::npos, error);
    std::filesystem::remove_all(dir);
    return true;
}

int main() {
    if (!test_round_trip()) {
        return 1;
    }
    if (!test_rejects_escape_and_missing_config()) {
        return 1;
    }
    if (!test_size_cap_does_not_read_body()) {
        return 1;
    }
    std::cout << "gs2packtest ok\n";
    return 0;
}

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
#include "util/Gs2Url.hpp"
#ifndef __EMSCRIPTEN__
#include "platform-specific/HttpsGet.hpp"
#endif

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif
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

static uint32_t fnv1a(const std::string& text) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

static std::string lower_copy(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

static bool test_pack_urls() {
    gs2url::PackUrl url;
    std::string error;
    const std::string text =
        "gssquared:https://Example.com:8443/pack/Choplifter%20boot.gs2pack?token=secret#frag";
    CHECK(gs2url::parse_pack_url(text, url, error), error);
    CHECK(url.https_url == "https://Example.com:8443/pack/Choplifter%20boot.gs2pack?token=secret",
          "query kept, fragment dropped");
    CHECK(url.host == "Example.com", "host drops the port");
    CHECK(url.filename == "Choplifter boot.gs2pack", "filename is decoded");
    CHECK(url.cache_name.find("secret") == std::string::npos, "query is not in the cache name");
    const uint32_t hash = fnv1a(lower_copy("Example.com") + "\n" + "/pack/Choplifter boot.gs2pack");
    char expect[64];
    std::snprintf(expect, sizeof(expect), "Choplifter_boot-%08x.gs2pack", hash);
    CHECK(url.cache_name == expect, url.cache_name);

    gs2url::PackUrl same_query;
    CHECK(gs2url::parse_pack_url(
              "GSSQUARED:https://example.com:8443/pack/Choplifter%20boot.gs2pack?token=other",
              same_query, error),
          error);
    CHECK(same_query.cache_name == url.cache_name, "query and host case do not change the cache name");

    gs2url::PackUrl other_host;
    CHECK(gs2url::parse_pack_url("gssquared:https://other.example/boot.gs2pack", other_host, error),
          error);
    gs2url::PackUrl other_path;
    CHECK(gs2url::parse_pack_url("gssquared:https://other.example/disks/boot.gs2pack", other_path, error),
          error);
    CHECK(other_host.cache_name != other_path.cache_name, "two boot.gs2pack paths do not collide");

    CHECK(!gs2url::parse_pack_url("gssquared:http://example.com/a.gs2pack", url, error), "http");
    CHECK(!gs2url::parse_pack_url("gssquared://example.com/a.gs2pack", url, error), "bare scheme");
    CHECK(!gs2url::parse_pack_url("gssquared:file:///tmp/a.gs2pack", url, error), "file");
    CHECK(!gs2url::parse_pack_url("gssquared:https://user:pw@example.com/a.gs2pack", url, error),
          "userinfo");
    CHECK(!gs2url::parse_pack_url("gssquared:https://example.com/machine.gs2", url, error), ".gs2");
    CHECK(!gs2url::parse_pack_url("gssquared:https://example.com/Settings.txt", url, error),
          "settings");
    CHECK(gs2url::is_gssquared_url("gssquared:https://example.com/machine.gs2"), "scheme prefix");
    CHECK(!gs2url::is_gssquared_url("/tmp/machine.gs2"), "plain path");

    gs2url::PackUrl v6;
    CHECK(gs2url::parse_pack_url("gssquared:https://[2001:db8::1]/a.gs2pack", v6, error), error);
    CHECK(v6.host == "2001:db8::1", "ipv6 host");

    std::string path;
    CHECK(gs2url::decode_file_url("file:///tmp/Foo%20bar.gs2", path), "file url");
    CHECK(path == "/tmp/Foo bar.gs2", path);
    CHECK(gs2url::decode_file_url("file://localhost/tmp/a.gs2pack", path), "localhost file url");
    CHECK(path == "/tmp/a.gs2pack", path);
    CHECK(!gs2url::decode_file_url("/tmp/a.gs2pack", path), "plain path is not a file url");

    const std::string cache = gs2url::pack_cache_file(url);
    CHECK(cache.find("Library/Caches/GSSquared/packs") != std::string::npos, cache);
    CHECK(cache.size() >= url.cache_name.size()
              && cache.compare(cache.size() - url.cache_name.size(), url.cache_name.size(),
                               url.cache_name) == 0,
          "cache file uses the cache name");

    const auto dir = std::filesystem::temp_directory_path() / "gs2packtest-token";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto pack = (dir / "demo.gs2pack").string();
    CHECK(gs2url::write_save_token_sidecar(pack, "tok-1", error), error);
    {
        std::ifstream in(pack + ".save-token");
        std::string body;
        std::getline(in, body);
        CHECK(body == "tok-1", "sidecar body");
    }
#if !defined(_WIN32)
    struct stat st {};
    CHECK(::stat((pack + ".save-token").c_str(), &st) == 0, "sidecar stat");
    CHECK((st.st_mode & 0777) == 0600, "sidecar mode 0600");
#endif
    CHECK(!gs2url::write_save_token_sidecar(pack, "bad\ntoken", error), "newline token");
    CHECK(gs2url::write_save_token_sidecar(pack, "", error), error);
    CHECK(!std::filesystem::exists(pack + ".save-token"), "empty token removes the sidecar");
    std::filesystem::remove_all(dir);
    return true;
}

static bool test_https_get() {
    const auto dest = (std::filesystem::temp_directory_path() / "gs2packtest-https-body").string();
    std::filesystem::remove(dest);
    std::filesystem::remove(dest + ".partial");
    std::atomic<bool> cancel{false};
    HttpsGetResult result;
    const bool ok = https_get_to_file("https://example.com/", dest, gs2pack::kMaxBytes, &cancel, result);
    CHECK(ok, result.error);
    CHECK(result.status == 200, "https status");
    CHECK(std::filesystem::exists(dest), "cache file");
    CHECK(!std::filesystem::exists(dest + ".partial"), "partial removed");
    CHECK(std::filesystem::file_size(dest) > 0, "body");
    std::filesystem::remove(dest);
    return true;
}

int main(int argc, char **argv) {
    if (!test_round_trip()) {
        return 1;
    }
    if (!test_rejects_escape_and_missing_config()) {
        return 1;
    }
    if (!test_size_cap_does_not_read_body()) {
        return 1;
    }
    if (!test_pack_urls()) {
        return 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--https") {
        if (!test_https_get()) {
            return 1;
        }
        std::cout << "https get ok\n";
    }
    std::cout << "gs2packtest ok\n";
    return 0;
}

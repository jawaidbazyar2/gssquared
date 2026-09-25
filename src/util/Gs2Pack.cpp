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

#include "paths.hpp"
#include "util/SystemConfig.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace gs2pack {
namespace {

constexpr size_t kBlock = 512;
constexpr size_t kName = 100;
constexpr size_t kPrefix = 155;

Session *g_active = nullptr;

bool read_block(std::istream& in, unsigned char block[kBlock], std::string& error_out) {
    in.read(reinterpret_cast<char *>(block), kBlock);
    if (in.gcount() == 0 && in.eof()) {
        error_out = "Unexpected end of archive";
        return false;
    }
    if (static_cast<size_t>(in.gcount()) != kBlock) {
        error_out = "Truncated ustar block";
        return false;
    }
    return true;
}

bool is_zero_block(const unsigned char block[kBlock]) {
    for (size_t i = 0; i < kBlock; ++i) {
        if (block[i] != 0) {
            return false;
        }
    }
    return true;
}

unsigned checksum(const unsigned char block[kBlock]) {
    unsigned sum = 0;
    for (size_t i = 0; i < kBlock; ++i) {
        if (i >= 148 && i < 156) {
            sum += static_cast<unsigned char>(' ');
        } else {
            sum += block[i];
        }
    }
    return sum;
}

bool parse_octal(const unsigned char *field, size_t len, uint64_t& out) {
    size_t i = 0;
    while (i < len && (field[i] == ' ' || field[i] == '\0')) {
        ++i;
    }
    if (i >= len) {
        out = 0;
        return true;
    }
    // GNU base-256 size marker. Not ustar.
    if (field[0] & 0x80) {
        return false;
    }
    uint64_t value = 0;
    for (; i < len; ++i) {
        const unsigned char c = field[i];
        if (c == '\0' || c == ' ') {
            break;
        }
        if (c < '0' || c > '7') {
            return false;
        }
        value = (value << 3) + static_cast<uint64_t>(c - '0');
    }
    out = value;
    return true;
}

std::string field_string(const unsigned char *field, size_t len) {
    size_t n = 0;
    while (n < len && field[n] != '\0') {
        ++n;
    }
    return std::string(reinterpret_cast<const char *>(field), n);
}

bool member_name_ok(const std::string& name, std::string& error_out) {
    if (name.empty() || name[0] == '/' || name[0] == '\\') {
        error_out = "Archive member name is absolute: " + name;
        return false;
    }
    if (name.find('\\') != std::string::npos || name.find('\0') != std::string::npos) {
        error_out = "Archive member name is not a portable path: " + name;
        return false;
    }
    std::string part;
    for (size_t i = 0; i <= name.size(); ++i) {
        if (i == name.size() || name[i] == '/') {
            if (part.empty() || part == "." || part == "..") {
                error_out = "Archive member name escapes the pack: " + name;
                return false;
            }
            part.clear();
        } else {
            part.push_back(name[i]);
        }
    }
    return true;
}

bool fits_in_workdir(const std::filesystem::path& root, const std::filesystem::path& dest) {
    const std::filesystem::path normal_root = root.lexically_normal();
    const std::filesystem::path normal_dest = dest.lexically_normal();
    auto root_it = normal_root.begin();
    auto dest_it = normal_dest.begin();
    for (; root_it != normal_root.end(); ++root_it, ++dest_it) {
        if (dest_it == normal_dest.end() || *root_it != *dest_it) {
            return false;
        }
    }
    return true;
}

void put_octal(unsigned char *field, size_t len, uint64_t value) {
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

bool fill_header(unsigned char block[kBlock], const std::string& name, uint64_t size,
                 std::string& error_out) {
    std::memset(block, 0, kBlock);
    std::string prefix;
    std::string file_name = name;
    if (file_name.size() > kName) {
        const auto slash = file_name.rfind('/');
        if (slash == std::string::npos || slash > kPrefix || file_name.size() - slash - 1 > kName) {
            error_out = "Archive member name exceeds ustar limits: " + name;
            return false;
        }
        prefix = file_name.substr(0, slash);
        file_name = file_name.substr(slash + 1);
        if (prefix.size() > kPrefix || file_name.empty() || file_name.size() > kName) {
            error_out = "Archive member name exceeds ustar limits: " + name;
            return false;
        }
    }
    std::memcpy(block + 0, file_name.data(), file_name.size());
    put_octal(block + 100, 8, 0644);
    put_octal(block + 108, 8, 0);
    put_octal(block + 116, 8, 0);
    put_octal(block + 124, 12, size);
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    put_octal(block + 136, 12, static_cast<uint64_t>(now < 0 ? 0 : now));
    std::memset(block + 148, ' ', 8);
    block[156] = '0';
    std::memcpy(block + 257, "ustar", 5);
    block[262] = '\0';
    block[263] = '0';
    block[264] = '0';
    if (!prefix.empty()) {
        std::memcpy(block + 345, prefix.data(), prefix.size());
    }
    const unsigned sum = checksum(block);
    std::snprintf(reinterpret_cast<char *>(block + 148), 8, "%06o", sum);
    block[154] = '\0';
    block[155] = ' ';
    return true;
}

bool copy_stream(std::istream& in, std::ostream& out, uint64_t size, std::string& error_out) {
    std::vector<char> buf(64 * 1024);
    uint64_t left = size;
    while (left > 0) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(left, buf.size()));
        in.read(buf.data(), static_cast<std::streamsize>(chunk));
        if (static_cast<size_t>(in.gcount()) != chunk) {
            error_out = "Truncated archive member";
            return false;
        }
        out.write(buf.data(), static_cast<std::streamsize>(chunk));
        if (!out) {
            error_out = "Failed to write pack member";
            return false;
        }
        left -= chunk;
    }
    return true;
}

bool skip_padding(std::istream& in, uint64_t size, std::string& error_out) {
    const uint64_t pad = (kBlock - (size % kBlock)) % kBlock;
    if (pad == 0) {
        return true;
    }
    in.seekg(static_cast<std::streamoff>(pad), std::ios::cur);
    if (!in) {
        error_out = "Truncated archive padding";
        return false;
    }
    return true;
}

struct HeaderInfo {
    std::string name;
    uint64_t size = 0;
    char typeflag = '0';
    bool end = false;
};

bool read_header(std::istream& in, HeaderInfo& info, std::string& error_out) {
    unsigned char block[kBlock];
    if (!read_block(in, block, error_out)) {
        return false;
    }
    if (is_zero_block(block)) {
        info.end = true;
        return true;
    }
    const std::string magic = field_string(block + 257, 6);
    if (magic != "ustar" || block[263] != '0' || block[264] != '0') {
        error_out = "Not an uncompressed ustar archive";
        return false;
    }
    uint64_t stored = 0;
    if (!parse_octal(block + 148, 8, stored) || stored != checksum(block)) {
        error_out = "Bad ustar checksum";
        return false;
    }
    if (!parse_octal(block + 124, 12, info.size)) {
        error_out = "Bad ustar member size";
        return false;
    }
    const std::string prefix = field_string(block + 345, kPrefix);
    const std::string file_name = field_string(block + 0, kName);
    if (!prefix.empty()) {
        info.name = prefix + "/" + file_name;
    } else {
        info.name = file_name;
    }
    info.typeflag = static_cast<char>(block[156]);
    info.end = false;
    return true;
}

bool check_payload_budget(uint64_t& used, uint64_t size, std::string& error_out) {
    if (size > kMaxBytes || used > kMaxBytes - size) {
        error_out = "Pack exceeds 200 MB";
        return false;
    }
    used += size;
    return true;
}

std::filesystem::path make_work_dir(std::string& error_out) {
    const auto base = std::filesystem::temp_directory_path();
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int i = 0; i < 100; ++i) {
        auto dir = base / ("gs2pack-" + std::to_string(stamp) + "-" + std::to_string(i));
        if (std::filesystem::create_directory(dir, ec)) {
            return dir;
        }
    }
    error_out = "Failed to create a pack working directory";
    return {};
}

bool replace_file(const std::filesystem::path& tmp, const std::filesystem::path& dest,
                  std::string& error_out) {
    std::error_code ec;
    std::filesystem::rename(tmp, dest, ec);
    if (!ec) {
        return true;
    }
    std::filesystem::remove(dest, ec);
    ec.clear();
    std::filesystem::rename(tmp, dest, ec);
    if (ec) {
        error_out = "Failed to replace pack: " + ec.message();
        return false;
    }
    return true;
}

}  // namespace

bool is_pack_path(const std::string& path) {
    const std::string base = std::filesystem::path(path).filename().string();
    return Paths::ends_with_icase(base, ".gs2pack");
}

bool validate(const std::string& pack_path, std::string& error_out) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(pack_path, ec);
    if (ec) {
        error_out = "Failed to open pack: " + ec.message();
        return false;
    }
    if (file_size > kMaxBytes) {
        error_out = "Pack exceeds 200 MB";
        return false;
    }
    std::ifstream in(pack_path, std::ios::binary);
    if (!in) {
        error_out = "Failed to open pack";
        return false;
    }
    uint64_t used = 0;
    bool saw_machine = false;
    for (;;) {
        HeaderInfo info;
        if (!read_header(in, info, error_out)) {
            return false;
        }
        if (info.end) {
            break;
        }
        if (!check_payload_budget(used, info.size, error_out)) {
            return false;
        }
        const char type = info.typeflag;
        if (type == '5') {
            in.seekg(static_cast<std::streamoff>(info.size), std::ios::cur);
            if (!in || !skip_padding(in, info.size, error_out)) {
                error_out = error_out.empty() ? "Truncated archive member" : error_out;
                return false;
            }
            continue;
        }
        if (type != '0' && type != '\0') {
            error_out = std::string("Unsupported ustar member type in ") + info.name;
            return false;
        }
        if (!member_name_ok(info.name, error_out)) {
            return false;
        }
        if (info.name == "machine.gs2") {
            saw_machine = true;
        }
        in.seekg(static_cast<std::streamoff>(info.size), std::ios::cur);
        if (!in) {
            error_out = "Truncated archive member";
            return false;
        }
        if (!skip_padding(in, info.size, error_out)) {
            return false;
        }
    }
    if (!saw_machine) {
        error_out = "Pack is missing machine.gs2";
        return false;
    }
    return true;
}

bool read_member(const std::string& pack_path, const std::string& name,
                 std::string& bytes, std::string& error_out) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(pack_path, ec);
    if (ec) {
        error_out = "Failed to open pack: " + ec.message();
        return false;
    }
    if (file_size > kMaxBytes) {
        error_out = "Pack exceeds 200 MB";
        return false;
    }
    std::ifstream in(pack_path, std::ios::binary);
    if (!in) {
        error_out = "Failed to open pack";
        return false;
    }
    uint64_t used = 0;
    for (;;) {
        HeaderInfo info;
        if (!read_header(in, info, error_out)) {
            return false;
        }
        if (info.end) {
            break;
        }
        if (!check_payload_budget(used, info.size, error_out)) {
            return false;
        }
        const bool wanted = info.name == name && (info.typeflag == '0' || info.typeflag == '\0');
        if (!wanted) {
            in.seekg(static_cast<std::streamoff>(info.size), std::ios::cur);
            if (!in || !skip_padding(in, info.size, error_out)) {
                if (error_out.empty()) {
                    error_out = "Truncated archive member";
                }
                return false;
            }
            continue;
        }
        bytes.resize(static_cast<size_t>(info.size));
        if (info.size > 0) {
            in.read(bytes.data(), static_cast<std::streamsize>(info.size));
            if (static_cast<uint64_t>(in.gcount()) != info.size) {
                error_out = "Truncated archive member";
                return false;
            }
        }
        return true;
    }
    error_out = "Pack is missing " + name;
    return false;
}

bool extract(const std::string& pack_path, Session& out, std::string& error_out) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(pack_path, ec);
    if (ec) {
        error_out = "Failed to open pack: " + ec.message();
        return false;
    }
    if (file_size > kMaxBytes) {
        error_out = "Pack exceeds 200 MB";
        return false;
    }
    std::ifstream in(pack_path, std::ios::binary);
    if (!in) {
        error_out = "Failed to open pack";
        return false;
    }
    const auto work = make_work_dir(error_out);
    if (work.empty()) {
        return false;
    }
    Session session;
    session.source_path = pack_path;
    session.work_dir = work.string();
    uint64_t used = 0;
    bool saw_machine = false;
    auto fail = [&](const std::string& message) {
        error_out = message;
        std::filesystem::remove_all(work, ec);
        return false;
    };
    for (;;) {
        HeaderInfo info;
        if (!read_header(in, info, error_out)) {
            return fail(error_out);
        }
        if (info.end) {
            break;
        }
        if (!check_payload_budget(used, info.size, error_out)) {
            return fail(error_out);
        }
        const char type = info.typeflag;
        if (type == '5') {
            in.seekg(static_cast<std::streamoff>(info.size), std::ios::cur);
            if (!skip_padding(in, info.size, error_out)) {
                return fail(error_out);
            }
            continue;
        }
        if (type != '0' && type != '\0') {
            return fail(std::string("Unsupported ustar member type in ") + info.name);
        }
        if (!member_name_ok(info.name, error_out)) {
            return fail(error_out);
        }
        const auto dest = work / std::filesystem::path(info.name);
        if (!fits_in_workdir(work, dest)) {
            return fail("Archive member name escapes the pack: " + info.name);
        }
        std::filesystem::create_directories(dest.parent_path(), ec);
        if (ec) {
            return fail("Failed to create pack directory: " + ec.message());
        }
        std::ofstream file(dest, std::ios::binary);
        if (!file) {
            return fail("Failed to write " + dest.string());
        }
        if (!copy_stream(in, file, info.size, error_out)) {
            return fail(error_out);
        }
        file.close();
        if (!skip_padding(in, info.size, error_out)) {
            return fail(error_out);
        }
        if (info.name == "machine.gs2") {
            saw_machine = true;
        }
        session.members.push_back(Member{info.name, dest.string()});
    }
    if (!saw_machine) {
        return fail("Pack is missing machine.gs2");
    }
    out = std::move(session);
    return true;
}

bool write_archive(const std::string& dest_path,
                   const std::vector<std::pair<std::string, std::string>>& name_and_bytes,
                   std::string& error_out) {
    std::ofstream out(dest_path, std::ios::binary);
    if (!out) {
        error_out = "Failed to open pack for writing";
        return false;
    }
    uint64_t used = 0;
    for (const auto& member : name_and_bytes) {
        if (!member_name_ok(member.first, error_out)) {
            return false;
        }
        if (!check_payload_budget(used, member.second.size(), error_out)) {
            return false;
        }
        unsigned char block[kBlock];
        if (!fill_header(block, member.first, member.second.size(), error_out)) {
            return false;
        }
        out.write(reinterpret_cast<const char *>(block), kBlock);
        out.write(member.second.data(), static_cast<std::streamsize>(member.second.size()));
        const uint64_t pad = (kBlock - (member.second.size() % kBlock)) % kBlock;
        if (pad > 0) {
            std::vector<char> zeros(static_cast<size_t>(pad), 0);
            out.write(zeros.data(), static_cast<std::streamsize>(pad));
        }
    }
    unsigned char zeros[kBlock * 2] = {};
    out.write(reinterpret_cast<const char *>(zeros), sizeof(zeros));
    if (!out) {
        error_out = "Failed to write pack";
        return false;
    }
    return true;
}

bool rewrite(const Session& session, std::string& error_out) {
    if (session.source_path.empty() || session.members.empty()) {
        error_out = "Pack session has nothing to write";
        return false;
    }
    const std::filesystem::path dest(session.source_path);
    const std::filesystem::path tmp = dest.string() + ".tmp";
    std::ofstream out(tmp, std::ios::binary);
    if (!out) {
        error_out = "Failed to open pack for writing";
        return false;
    }
    uint64_t used = 0;
    for (const Member& member : session.members) {
        std::ifstream in(member.path, std::ios::binary);
        if (!in) {
            error_out = "Missing pack member " + member.path;
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        in.seekg(0, std::ios::end);
        const auto size = static_cast<uint64_t>(in.tellg());
        in.seekg(0, std::ios::beg);
        if (!check_payload_budget(used, size, error_out)) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        unsigned char block[kBlock];
        if (!fill_header(block, member.name, size, error_out)) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        out.write(reinterpret_cast<const char *>(block), kBlock);
        if (!copy_stream(in, out, size, error_out)) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        const uint64_t pad = (kBlock - (size % kBlock)) % kBlock;
        if (pad > 0) {
            std::vector<char> zeros(static_cast<size_t>(pad), 0);
            out.write(zeros.data(), static_cast<std::streamsize>(pad));
        }
    }
    unsigned char zeros[kBlock * 2] = {};
    out.write(reinterpret_cast<const char *>(zeros), sizeof(zeros));
    out.close();
    if (!out) {
        error_out = "Failed to write pack";
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return replace_file(tmp, dest, error_out);
}

void cleanup(Session& session) {
    if (g_active == &session) {
        g_active = nullptr;
    }
    if (!session.work_dir.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(session.work_dir, ec);
    }
    session.work_dir.clear();
    session.members.clear();
    session.config = nullptr;
}

std::string machine_gs2_path(const Session& session) {
    for (const Member& member : session.members) {
        if (member.name == "machine.gs2") {
            return member.path;
        }
    }
    return (std::filesystem::path(session.work_dir) / "machine.gs2").string();
}

std::vector<std::string> list_disk_images(const Session& session) {
    std::vector<std::string> images;
    const auto dir = std::filesystem::path(session.work_dir) / "disks";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return images;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) {
            images.push_back(entry.path().string());
        }
    }
    std::sort(images.begin(), images.end());
    return images;
}

bool remember_mount(Session& session, uint16_t slot, uint16_t drive,
                    const std::string& image_path, std::string& error_out) {
    if (session.config == nullptr) {
        error_out = "Pack has no loaded config";
        return false;
    }
    const auto root = std::filesystem::path(session.work_dir).lexically_normal();
    const auto image = std::filesystem::path(image_path).lexically_normal();
    if (!fits_in_workdir(root, image)) {
        error_out = "Image is not in the pack";
        return false;
    }
    std::vector<disk_mount_t> mounts = session.config->mounts();
    bool found = false;
    for (disk_mount_t& mount : mounts) {
        if (mount.slot == slot && mount.drive == drive) {
            mount.filename = image_path;
            found = true;
            break;
        }
    }
    if (!found) {
        disk_mount_t mount;
        mount.slot = slot;
        mount.drive = drive;
        mount.filename = image_path;
        mounts.push_back(std::move(mount));
    }
    session.config->set_mounts(std::move(mounts));
    return session.config->save(session.config->path(), error_out);
}

void set_active(Session *session) {
    g_active = session;
}

Session *active() {
    return g_active;
}

}  // namespace gs2pack

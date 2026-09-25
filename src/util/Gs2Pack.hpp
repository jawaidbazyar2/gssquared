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
#include <utility>
#include <vector>

class SystemConfig;

/** Uncompressed POSIX ustar `.gs2pack`. Gzip is not a pack. */
namespace gs2pack {

constexpr uint64_t kMaxBytes = 200ull * 1024ull * 1024ull;

struct Member {
    std::string name;
    std::string path;
};

struct Session {
    std::string source_path;
    std::string work_dir;
    std::vector<Member> members;
    /** Not owned. Set by the app after the extracted machine.gs2 is loaded. */
    SystemConfig *config = nullptr;
};

bool is_pack_path(const std::string& path);

/** Parse headers only. Does not write a work tree or read member bodies. */
bool validate(const std::string& pack_path, std::string& error_out);

/** Read one member into memory. Other members are skipped, not buffered. */
bool read_member(const std::string& pack_path, const std::string& name,
                 std::string& bytes, std::string& error_out);

bool extract(const std::string& pack_path, Session& out, std::string& error_out);

/** Write `source.gs2pack.tmp` then rename over `source_path`. */
bool rewrite(const Session& session, std::string& error_out);

void cleanup(Session& session);

std::string machine_gs2_path(const Session& session);

/** Regular files in the work tree's `disks/` directory, sorted by filename. */
std::vector<std::string> list_disk_images(const Session& session);

/**
 * Replace the [[storage]] entry for this slot/drive with a pack image and
 * save machine.gs2. `image_path` must already live in the work tree.
 */
bool remember_mount(Session& session, uint16_t slot, uint16_t drive,
                    const std::string& image_path, std::string& error_out);

void set_active(Session *session);
Session *active();

/** Build an uncompressed ustar. Used by gs2packtest and rewrite. */
bool write_archive(const std::string& dest_path,
                   const std::vector<std::pair<std::string, std::string>>& name_and_bytes,
                   std::string& error_out);

}  // namespace gs2pack

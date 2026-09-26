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

#include "platform-specific/HttpsBody.hpp"

#include <filesystem>
#include <system_error>

bool HttpsBodyWriter::open(const std::string& dest, uint64_t max_bytes, std::string& error_out) {
    abort();
    const std::filesystem::path dest_path(dest);
    std::error_code ec;
    std::filesystem::create_directories(dest_path.parent_path(), ec);
    if (ec) {
        error_out = "Failed to create pack cache: " + ec.message();
        return false;
    }
    partial_ = dest + ".partial";
    out_.open(partial_, std::ios::binary | std::ios::trunc);
    if (!out_) {
        error_out = "Failed to write pack download";
        partial_.clear();
        return false;
    }
    dest_ = dest;
    max_bytes_ = max_bytes;
    written_ = 0;
    open_ = true;
    return true;
}

bool HttpsBodyWriter::write(const void *data, size_t n, const std::atomic<bool> *cancel,
                            std::string& error_out) {
    if (cancel != nullptr && cancel->load()) {
        error_out = "Download canceled";
        abort();
        return false;
    }
    if (!open_) {
        error_out = "Failed to write pack download";
        return false;
    }
    if (n > max_bytes_ || written_ > max_bytes_ - n) {
        error_out = "Pack exceeds 200 MB";
        abort();
        return false;
    }
    out_.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
    if (!out_) {
        error_out = "Failed to write pack download";
        abort();
        return false;
    }
    written_ += n;
    return true;
}

bool HttpsBodyWriter::commit(std::string& error_out) {
    if (!open_) {
        error_out = "Failed to write pack download";
        return false;
    }
    out_.close();
    open_ = false;
    if (!out_) {
        error_out = "Failed to write pack download";
        abort();
        return false;
    }
    std::error_code ec;
    std::filesystem::rename(partial_, dest_, ec);
    if (ec) {
        std::filesystem::remove(dest_, ec);
        ec.clear();
        std::filesystem::rename(partial_, dest_, ec);
    }
    if (ec) {
        error_out = "Failed to store pack download: " + ec.message();
        abort();
        return false;
    }
    partial_.clear();
    dest_.clear();
    return true;
}

void HttpsBodyWriter::abort() {
    if (out_.is_open()) {
        out_.close();
    }
    open_ = false;
    if (!partial_.empty()) {
        std::error_code ec;
        std::filesystem::remove(partial_, ec);
        partial_.clear();
    }
    dest_.clear();
    written_ = 0;
}

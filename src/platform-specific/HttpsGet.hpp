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

#include <atomic>
#include <cstdint>
#include <string>

/** Desktop HTTPS GET. macOS NSURLSession, Windows WinHTTP, Linux libcurl. */
struct HttpsGetResult {
    bool ok = false;
    int status = 0;
    /** Response header X-GS2-Save-Token, when the server sent one. */
    std::string save_token;
    std::string error;
};

/**
 * Stream `url` to `dest_path`. Writes `dest_path + ".partial"` and renames it
 * into place only after a 200. `cancel` may be null. A set cancel deletes the partial.
 */
bool https_get_to_file(const std::string& url, const std::string& dest_path, uint64_t max_bytes,
                       const std::atomic<bool> *cancel, HttpsGetResult& out);

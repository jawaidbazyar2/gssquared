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
#include <fstream>
#include <string>

/** Streams a response body to `dest.partial`, then renames over `dest`. */
class HttpsBodyWriter {
public:
    bool open(const std::string& dest, uint64_t max_bytes, std::string& error_out);
    bool write(const void *data, size_t n, const std::atomic<bool> *cancel, std::string& error_out);
    bool commit(std::string& error_out);
    void abort();

private:
    std::string dest_;
    std::string partial_;
    std::ofstream out_;
    uint64_t max_bytes_ = 0;
    uint64_t written_ = 0;
    bool open_ = false;
};

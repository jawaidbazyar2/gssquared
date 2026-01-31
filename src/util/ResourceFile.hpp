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

#pragma once

#include <stdio.h>
#include <string>
#include <cstdint>

typedef enum {
    READ_ONLY = 0,
    READ_WRITE = 1
} file_mode_t;

/**
 * A ResourceFile is a file whose contents are typically loaded read-only into memory
 * at startup time. (or, virtual computer boot time).
 * 
 * They are located in the Bundle 'Resources' directory.
 */
class ResourceFile {
private:
    std::string filename_t;
    file_mode_t mode_t;
    uint8_t *file_data;
    uintmax_t file_size;
    int error_code;

public:
    ResourceFile(const char *filename, file_mode_t mode);
    ~ResourceFile();

    /**
     * Check if the file exists
     * @return 1 if file exists, 0 otherwise
     */
    int exists();

    /**
     * Get the size of the file
     * @return size of the file in bytes
     */
    uintmax_t size();

    /**
     * Display info about the file
     */
    void dump();

    /**
     * Load the file into memory
     * @return Pointer to the loaded file data. Caller is responsible for freeing the memory.
     * @throws std::runtime_error if file cannot be loaded
     */
    uint8_t* load();

    uint8_t* get_data();
};



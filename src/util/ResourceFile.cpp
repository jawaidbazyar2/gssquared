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

/**
 * Utility functions for finding and loading resource files.
 */

#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <SDL3/SDL.h>

#include "gs2.hpp"
#include "util/ResourceFile.hpp"
#include "util/dialog.hpp"

ResourceFile::ResourceFile(const char *filename, file_mode_t mode) {
    mode_t = mode;
    error_code = 0;
    file_size = -1; // haven't read it yet.
    file_data = nullptr;

    // generate the full path based on system values
    filename_t.assign(gs2_app_values.base_path);
    filename_t.append(filename);
}

ResourceFile::~ResourceFile() {
    if (file_data) {
        // delete[] file_data; // this means we dispose of it after loading into the VM memory.
        // current use is the ResourceFile object will go away when leaving the function loading it.
    }
}

int ResourceFile::exists() {
    // check if file exists   
    if (!std::filesystem::exists(filename_t)) {
        char *debugstr = new char[512];
        snprintf(debugstr, 512, "Failed to stat %s errno: %d\n", filename_t.c_str(), errno);
        system_failure(debugstr);
        delete[] debugstr;
        return 0;
    }
    return 1;
}

void ResourceFile::dump() {
    printf("Dumping file: %s\n", filename_t.c_str());
    printf("File size: %zu\n", file_size);
    printf("File mode: %d\n", mode_t);
    if (file_data) {
        for (int i = 0; i < 256; i++) {
            printf("%02X ", file_data[i]);
        }
        printf("\n");
    }
}

uintmax_t ResourceFile::size() {
    return file_size;
}

uint8_t* ResourceFile::load() {
    // load the file into newly allocated memory.
    if (!exists()) {
        throw std::runtime_error("File does not exist: " + filename_t);            
    }

    file_size = std::filesystem::file_size(filename_t);

    // enforce reasonable size constraints
    constexpr uintmax_t MAX_FILE_SIZE = 16 * 1024 * 1024; // 16MB limit
    if (file_size == 0) {
        throw std::runtime_error("File is empty: " + filename_t);
    }
    if (file_size > MAX_FILE_SIZE) {
        throw std::runtime_error("File too large: " + filename_t);
    }

    // allocate memory for the file with error checking
    file_data = nullptr;
    try {
        file_data = new uint8_t[file_size];
    } catch (const std::bad_alloc& e) {
        throw std::runtime_error("Failed to allocate memory for file: " + filename_t);
    }

    // read the file into memory with error checking
    std::ifstream file(filename_t, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        delete[] file_data;  // Clean up allocated memory
        throw std::runtime_error("Failed to open file: " + filename_t);
    }

    file.read(reinterpret_cast<char*>(file_data), file_size);
    if (file.fail() || file.gcount() != static_cast<std::streamsize>(file_size)) {
        delete[] file_data;  // Clean up allocated memory
        file.close();
        throw std::runtime_error("Failed to read file: " + filename_t);
    }

    file.close();
    return file_data;
}

uint8_t* ResourceFile::get_data() {
    return file_data;
}

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

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "paths.hpp"
#include "build_config.hpp"

// Define static member variables
// this is a little weird, but C++ is a little weird.
std::string Paths::base_path;
std::string Paths::pref_path;
std::string Paths::home_folder;
std::string Paths::docs_folder;
std::string Paths::last_file_dialog_dir;

/**
 * On a Mac, running as a bundle, the base path is BundlePath/Resources. 
 * Even if the Mac is running it as a cli i.e. debugger 
 * For other build types, the base path is where the exe lives, but:
 *  We need to add "resources/" to it for program files builds.
 *  We need to add "../share/GSSquared/" to it for GNU install directories builds.
 * */

const std::string& get_base_path(bool console_mode) {
    static std::string base_path("");
    if (base_path != "") {
        return base_path;
    }

#if defined(__EMSCRIPTEN__)
    // Resources are preloaded into the WASM filesystem at /resources
    // (see the --preload-file flag in CMakeLists.txt).
    base_path = "/resources/";
#elif defined(GS2_GNU_INSTALL_DIRS)
    base_path = SDL_GetBasePath();
    base_path += "../share/GSSquared/";
#elif defined(__APPLE__)
    if (console_mode) {
        base_path = SDL_GetBasePath();
        const std::string suffix = "/Resources/";
        if (base_path.size() >= suffix.size() && 
            base_path.substr(base_path.size() - suffix.size()) == suffix) {
            std::cout << "hello" << std::endl;
        } else base_path += "resources/";
    } else { 
        base_path = SDL_GetBasePath();
    }
#else
    base_path = SDL_GetBasePath();
    base_path += "resources/";
#endif

    return base_path;
}

/**
 * For GNU install directories or Mac bundle builds, we can't expect to be able
 * to write files into the working directory, though that is a fair expectation
 * for program files builds.
 */

const std::string& get_pref_path(void) {
    static std::string pref_path("");
    if (pref_path != "") {
        return pref_path;
    }

#if defined(__EMSCRIPTEN__)
    // MEMFS root; not persisted across reloads (could mount IDBFS later).
    pref_path = "/";
#elif defined(GS2_PROGRAM_FILES)
    pref_path = "./";
#else
    pref_path = SDL_GetPrefPath("jawaidbazyar2", "GSSquared");
#endif

    return pref_path;
}

void Paths::initialize(bool console_mode) {
    base_path = get_base_path(console_mode);
    pref_path = get_pref_path();

    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (home && home[0]) {
        home_folder = home;
    } else {
        const char *env_home = std::getenv("HOME");
        home_folder = (env_home && env_home[0]) ? std::string(env_home) : std::string("/");
    }

    const char *docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    if (docs && docs[0]) {
        docs_folder = docs;
    } else {
        docs_folder = home_folder;
    }
}

void Paths::calc_base(std::string& return_path, std::string file) {
     return_path = base_path + file;
}

void Paths::calc_pref(std::string& return_path, std::string file) {
    return_path = pref_path + file;
}

void Paths::calc_home(std::string& return_path, std::string file) {
    return_path = home_folder + file;
}

void Paths::calc_docs(std::string& return_path, std::string file) {
    return_path = docs_folder + file;
}

const std::string& Paths::get_last_file_dialog_dir() {
    // If never set, return documents folder as a reasonable default
#if defined(__linux__)
    if (last_file_dialog_dir.empty()) {
        return docs_folder;
    }
#endif
    return last_file_dialog_dir;
}

void Paths::set_last_file_dialog_dir(const std::string& selected_path) {
    if (selected_path.empty()) {
        return;
    }
    // SDL_ShowOpenFileDialog default_location is a folder; store directory only, not the file name.
    std::filesystem::path p(selected_path);
    std::filesystem::path dir = p.parent_path();
    if (dir.empty()) {
        std::error_code ec;
        p = std::filesystem::weakly_canonical(std::filesystem::absolute(p), ec);
        dir = p.parent_path();
    }
    if (dir.empty()) {
        return;
    }
    last_file_dialog_dir = dir.lexically_normal().string();
}

bool Paths::is_directory(const std::string& filename) {
    // Platform-independent way to test if disk_mount.filename is a directory
    // We'll use std::filesystem if available (C++17); fallback to stat otherwise
    #if __has_include(<filesystem>)
    namespace fs = std::filesystem;
    return fs::is_directory(fs::u8path(filename));
#else
    struct stat path_stat;
    if (stat(filename.c_str(), &path_stat) == 0) {
        return S_ISDIR(path_stat.st_mode);
    }
#endif
    return false;
}

bool Paths::ends_with(std::string_view s, std::string_view suffix) noexcept {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), std::string_view::npos, suffix) == 0;
}

bool Paths::ends_with_icase(std::string_view s, std::string_view suffix) noexcept {
    if (s.size() < suffix.size()) {
        return false;
    }
    const size_t offset = s.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(s[offset + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

std::string Paths::get_directory(const std::string& filename) {
    std::filesystem::path p(filename);
    return p.parent_path().string();
}

bool Paths::is_absolute(const std::string& filename) {
    std::filesystem::path p(filename);
    return p.is_absolute();
}
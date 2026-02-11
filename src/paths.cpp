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
#include <iostream>

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

#if defined(GS2_GNU_INSTALL_DIRS)
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

#if defined(GS2_PROGRAM_FILES)
    pref_path = "./";
#else
    pref_path = SDL_GetPrefPath("jawaidbazyar2", "GSSquared");
#endif

    return pref_path;
}

void Paths::initialize(bool console_mode) {
    base_path = get_base_path(console_mode);
    pref_path = get_pref_path();
    // this returns a const char * and SDL_free wants char *, so ...
    home_folder = SDL_GetUserFolder(SDL_FOLDER_HOME);
    docs_folder = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
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
    if (last_file_dialog_dir.empty()) {
        return docs_folder;
    }
    return last_file_dialog_dir;
}

void Paths::set_last_file_dialog_dir(const std::string& dir) {
    last_file_dialog_dir = dir;
}
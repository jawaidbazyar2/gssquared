/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <SDL3/SDL_filesystem.h>

#include "paths.hpp"
#include "build_config.hpp"

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
        if (base_path.ends_with("/Resources/")) {
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

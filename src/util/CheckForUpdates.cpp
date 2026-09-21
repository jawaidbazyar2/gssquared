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

#include "util/CheckForUpdates.hpp"

#include <cstdio>

#include <SDL3/SDL.h>

#include "version.h"

static const char *updates_platform_token()
{
    const char *platform = SDL_GetPlatform();
    if (platform == nullptr) {
        return "other";
    }
    if (SDL_strcasecmp(platform, "macOS") == 0 ||
        SDL_strcasecmp(platform, "Mac OS X") == 0) {
        return "macos";
    }
    if (SDL_strcasecmp(platform, "Windows") == 0) {
        return "windows";
    }
    if (SDL_strcasecmp(platform, "Linux") == 0) {
        return "linux";
    }
    return "other";
}

void openCheckForUpdates()
{
    char url[256];
    snprintf(url, sizeof(url), "https://gssquared.net/updates/%s/%s/%s",
             VERSION_STRING, GIT_HASH, updates_platform_token());
    SDL_OpenURL(url);
}

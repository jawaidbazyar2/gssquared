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

#include <string>
#include <string_view>

/** HTML updates page for this build (Docs → Check For Updates). */
std::string checkForUpdatesUrl();

/** Same path with ?format=toml for the background check. */
std::string checkForUpdatesTomlUrl();

/** True only when the body parses as TOML and status is the string "available". */
bool updateTomlSaysAvailable(std::string_view body);

/** Open the gssquared.net updates page for this build in the system browser. */
void openCheckForUpdates();

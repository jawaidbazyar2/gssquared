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

#if defined(__EMSCRIPTEN__)

/**
 * Snapshot `path` from MEMFS and PUT it to `url` with `X-GS2-Save-Token`.
 * The read is synchronous. Completion is `gs2_web_put_done(generation, status, error)`
 * on the main thread. Status 0 is a CORS or network failure.
 */
void web_put_pack(const std::string& url, const std::string& path, const std::string& token,
                  int generation);

#endif

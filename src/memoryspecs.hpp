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

#ifdef APPLEIIGS
#define RAM_MB 1
#define RAM_SIZE RAM_MB * 1024 * 1024
#else
#define RAM_KB 64 * 1024
#define IO_KB 4 * 1024
#define ROM_KB 12 * 1024
#define MEMORY_SIZE 64 * 1024
#endif

#define GS2_PAGE_SIZE 256

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

#include "gs2.hpp"
#include "util/media.hpp"
#include "util/mount.hpp"
#include "computer.hpp"
#include "util/StorageDevice.hpp"

#define PDB3_MAX_UNITS 10

#define MAX_PD_BUFFER_SIZE 16
#define PD_CMD_RESET 0xC080
#define PD_CMD_PUT 0xC081
#define PD_CMD_EXECUTE 0xC082
#define PD_ERROR_GET 0xC083
#define PD_STATUS1_GET 0xC084
#define PD_STATUS2_GET 0xC085
#define PD_SP_CMD_LO 0xC086
#define PD_SP_CMD_HI 0xC087

#include "devices/pdblock2/pdb_structures.hpp"

void init_pdblock3(computer_t *computer, SlotType_t slot);

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

#pragma once

#include "gs2.hpp"
#include "cpu.hpp"
#include "util/media.hpp"
#include "util/mount.hpp"
#include "slots.hpp"
#include "computer.hpp"

#define MAX_PD_BUFFER_SIZE 16
#define PD_CMD_RESET 0xC080
#define PD_CMD_PUT 0xC081
#define PD_CMD_EXECUTE 0xC082
#define PD_ERROR_GET 0xC083
#define PD_STATUS1_GET 0xC084
#define PD_STATUS2_GET 0xC085

typedef struct media_t {
    FILE *file;
    media_descriptor *media;
    int last_block_accessed;
    uint64_t last_block_access_time;
} media_t;


struct pdblock_cmd_v1 {
    uint8_t version;
    uint8_t cmd;
    uint8_t dev;
    uint8_t addr_lo;
    uint8_t addr_hi;
    uint8_t block_lo;
    uint8_t block_hi;
    uint8_t checksum;
};

struct pdblock_cmd_buffer {
    uint8_t index;
    uint8_t cmd[MAX_PD_BUFFER_SIZE];
    uint8_t error;
    uint8_t status1;
    uint8_t status2;
};

struct pdblock2_data: public SlotData {
    uint8_t *rom;
    pdblock_cmd_buffer cmd_buffer;
    media_t prodosblockdevices[7][2];
};

enum pdblock_cmd {
    PD_STATUS = 0x00,
    PD_READ = 0x01,
    PD_WRITE = 0x02,
    PD_FORMAT = 0x03
};

#define PD_CMD        0x42
#define PD_DEV        0x43
#define PD_ADDR_LO    0x44
#define PD_ADDR_HI    0x45
#define PD_BLOCK_LO   0x46
#define PD_BLOCK_HI   0x47

#define PD_ERROR_NONE 0x00
#define PD_ERROR_IO   0x27
#define PD_ERROR_NO_DEVICE 0x28
#define PD_ERROR_WRITE_PROTECTED 0x2B
#define PD_ERROR_DEVICE_OFFLINE 0x2F

void pdblock2_execute(cpu_state *cpu, pdblock2_data *pdblock_d);
void init_pdblock2(computer_t *computer, SlotType_t slot);
bool mount_pdblock2(cpu_state *cpu, uint8_t slot, uint8_t drive, media_descriptor *media);
void unmount_pdblock2(cpu_state *cpu, uint64_t key);
drive_status_t pdblock2_osd_status(cpu_state *cpu, uint64_t key);

#pragma once

#include <cstdint>
#include <cstdio>
#include "util/media.hpp"
#include "slots.hpp"

#define MAX_PD_BUFFER_SIZE 16

struct media_t {
    FILE *file;
    media_descriptor *media;
    int last_block_accessed;
    uint64_t last_block_access_time;
};

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

struct pdblock_cmd_v2 {
    uint8_t version;
    uint8_t cmd_blk_lo;
    uint8_t cmd_blk_hi;
    uint8_t checksum;
};

struct pdblock_cmd_buffer {
    uint8_t index;
    uint8_t cmd[MAX_PD_BUFFER_SIZE];
    uint8_t error;
    uint8_t status1;
    uint8_t status2;
};

class PDBlock3; // forward declaration

struct pdblock3_data: public SlotData {
    ResourceFile *rom;
    MMU *mmu;
    MMU_II *megaii;
    PDBlock3 *pdb;
};

struct sp_cmd_standard { // CMDNUM is NOT replicated here.
    uint8_t pcount;
};

struct sp_cmd_status_st : public sp_cmd_standard {
    uint8_t unit;
    uint8_t status_p_0;
    uint8_t status_p_1;
    uint8_t status_code;
};

struct sp_cmd_rw_st : public sp_cmd_standard {
    uint8_t unit;
    uint8_t addr_lo;
    uint8_t addr_hi;
    uint8_t block_0;
    uint8_t block_1;
    uint8_t block_2;
};

struct sp_cmd_status_ex : public sp_cmd_standard {
    uint8_t unit;
    uint8_t status_p_0;
    uint8_t status_p_1;
    uint8_t status_p_2;
    uint8_t status_p_3;
    uint8_t status_code;
};

struct sp_cmd0_statcode_00 {
    uint8_t status;
    uint8_t blk_count_0;
    uint8_t blk_count_1;
    uint8_t blk_count_2;
};

struct sp_cmd0_statcode_03 {
    uint8_t status;
    uint8_t blk_count_0;
    uint8_t blk_count_1;
    uint8_t blk_count_2;
    uint8_t id_str_length;
    uint8_t id_str[16];
    uint8_t device_type;
    uint8_t device_subtype;
    uint8_t version_0;
    uint8_t version_1;
};

struct sp_cmd0_statcode_00_driver {
    uint8_t num_devices;
    uint8_t reserved[7];
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

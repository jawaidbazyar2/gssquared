#pragma once

#include <cstdint>
#include <cstdio>
#include "util/media.hpp"
#include "SlotData.hpp"
#include "util/StorageDevice.hpp"

#define MAX_PD_BUFFER_SIZE 16

// Zero-overhead little-endian multi-byte value with implicit conversion.
// sizeof(packed_uint<N>) == N, alignment == 1 — no padding added to structs.
template<int N>
struct packed_uint {
    uint8_t bytes[N] = {};

    operator uint32_t() const {
        uint32_t v = 0;
        for (int i = N - 1; i >= 0; --i) v = (v << 8) | bytes[i];
        return v;
    }
    packed_uint& operator=(uint32_t v) {
        for (int i = 0; i < N; ++i) { bytes[i] = v & 0xFF; v >>= 8; }
        return *this;
    }
};
using packed16 = packed_uint<2>;
using packed24 = packed_uint<3>;
using packed32 = packed_uint<4>;

struct media_t {
    FILE *file;
    media_descriptor *media;
    storage_key_t key;
    int last_block_accessed;
    uint64_t last_block_access_time;
};

struct pdblock_cmd_v1 {
    uint8_t version;
    uint8_t cmd;
    uint8_t dev;
    packed16 addr;
    packed16 block;
    uint8_t checksum;
};

struct pdblock_cmd_v2 {
    uint8_t version;
    packed16 cmd_blk;
    uint8_t checksum;
};

struct pdblock_cmd_buffer {
    uint8_t index;
    uint8_t cmd[MAX_PD_BUFFER_SIZE];
    uint8_t error;
    uint8_t status1;
    uint8_t status2;
};

struct sp_cmd_standard { // CMDNUM is NOT replicated here.
    uint8_t pcount;
};

/********  Status  **********/
//   Standard
struct sp_cmd_status_st : public sp_cmd_standard {
    uint8_t unit;
    packed16 status_p;
    uint8_t status_code;
};

//   Extended
struct sp_cmd_status_ex : public sp_cmd_standard {
    uint8_t unit;
    packed32 status_p;
    uint8_t status_code;
};

/********  Read-Write  **********/
//   Standard
struct sp_cmd_rw_st : public sp_cmd_standard {
    uint8_t unit;
    packed16 addr;
    packed24 block;
};

//   Extended
struct sp_cmd_rw_ex : public sp_cmd_standard {
    uint8_t unit;
    packed32 addr;
    packed32 block;
};

/**** EJECT / ("Control") ****/
//   Standard
struct sp_cmd_control_st : public sp_cmd_standard {
    uint8_t unit;
    uint8_t cl_addr_0;
    uint8_t cl_addr_1;
    uint8_t code;
};

struct sp_cmd_control_ex {
    uint8_t unit;
    uint8_t cl_addr_0;
    uint8_t cl_addr_1;
    uint8_t cl_addr_2;
    uint8_t cl_addr_3;
    uint8_t code;
};

// Status 00 response
//   Standard
struct sp_cmd0_statcode_00 {
    uint8_t status;
    packed24 blk_count;
};

//   Extended
struct sp_cmd0_statcode_00_ex {
    uint8_t status;
    packed32 blk_count;
};

// Status 03 response
//   Standard
struct sp_cmd0_statcode_03 {
    uint8_t status;
    packed24 blk_count;
    uint8_t id_str_length;
    uint8_t id_str[16];
    uint8_t device_type;
    uint8_t device_subtype;
    uint8_t version_0;
    uint8_t version_1;
};
// Extended
struct sp_cmd0_statcode_03_ex {
    uint8_t status;
    packed32 blk_count;
    uint8_t id_str_length;
    uint8_t id_str[16];
    uint8_t device_type;
    uint8_t device_subtype;
    uint8_t version_0;
    uint8_t version_1;
};

// Same for both standard and extended
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
#define PD_ERROR_BADBLOCK 0x2D
#define PD_ERROR_DEVICE_OFFLINE 0x2F

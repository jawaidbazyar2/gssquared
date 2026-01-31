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

#include <stdio.h>
#include "gs2.hpp"
//#include "devices/diskii/diskii_fmt.hpp"

/**
 * do and po only apply to 143k disks that we 'nibblize'.
 */

/**
 * 2Mg Header Format: https://gswv.apple2.org.za/a2zine/Docs/DiskImage_2MG_Info.txt
 */

typedef struct format_2mg_creator_data_t {
    const char *creator;
    const char *name;
} format_2mg_creator_data_t;



typedef struct format_2mg_raw_t {
    uint8_t id[4]; /* "2IMG" */
    uint8_t creator[4]; /* "XGS!" */
    uint8_t header_size[2]; /* header size, little endian */
    uint8_t version[2]; /* version, little endian */
    uint8_t image_format[4]; /* little endian; see below */
    uint8_t flag_byte;
    uint8_t flag_type; /* 0x01 == dos 3.3 volume number */
    uint8_t flag_byte2;
    uint8_t flag_byte3; /* 0x80 == write protected */
    uint8_t block_count[4]; /* little endian, 32-bit, number of blocks */
    uint8_t data_offset[4]; /* little endian, 32-bit, number of bytes from start of file to start of data */
    uint8_t data_length[4]; /* little endian, 32-bit, number of bytes in data */
    uint8_t comment_offset[4]; /* little endian, 32-bit, offset of optional comment */
    uint8_t comment_length[4]; /* little endian, 32-bit, length of optional comment */
    uint8_t creator_data[4]; /* little endian, 32-bit, offset of creator data */
    uint8_t creator_data_length[4]; /* little endian, 32-bit, length of creator data */
    uint8_t unused[16]; /* unused */
} format_2mg_raw_t;

#define FLAG_DOS33 0x00000100
#define FLAG_DOS33_VOL_MASK 0xFF
#define FLAG_LOCKED 0x80000000

typedef struct format_2mg_t {
    uint8_t id[5]; /* "2IMG" */
    char creator[5]; /* "XGS!" */
    uint16_t header_size;
    uint16_t version;
    uint32_t flag;
    uint32_t image_format;
    uint32_t block_count;
    uint32_t bytes_count;
    uint32_t comment_offset;
    uint32_t comment_length;
    uint32_t creator_data;
    uint32_t creator_data_length;
    char *comment_content;
    char *creator_data_content;
} format_2mg_t;

/**
 * image_format:
 * 0x00000000: DOS 3.3 sector order
 * 0x00000001: ProDOS Sector Order
 * 0x00000002: Nibblized Format
 */

/**
 * one of the nibblized formats must be used for DOS 3.3 disks, because
 * DOS and its apps all typically hit the disk controller hardware directly.
 * Also, copy-protected disks are often distributed pre-nibblized.
 */
typedef enum media_type_t {
    MEDIA_NYBBLE, /* 143K disk that needs nibblization on load */
    MEDIA_PRENYBBLE, /* 143K Diskette - pre-nibblized */
    MEDIA_BLK, /* generic block image */
} media_type_t;

typedef enum media_interleave_t {
    INTERLEAVE_NONE, /* Straight block image */
    INTERLEAVE_DO,
    INTERLEAVE_PO,
    INTERLEAVE_CPM
} media_interleave_t;

//typedef uint8_t nibblized_image_t[0x1A00 * 35];

typedef struct media_descriptor {
    //const char *filename = nullptr;
    std::string filename;
    //const char *filestub = nullptr;
    std::string filestub;
    FILE *fp = nullptr;
    media_type_t media_type = MEDIA_BLK;
    media_interleave_t interleave = INTERLEAVE_NONE;
    //nibblized_disk_t* nibblized = nullptr;
    uint64_t data_offset = 0;
    uint16_t block_size = 0;
    uint32_t block_count = 0;
    uint64_t file_size = 0;
    uint64_t data_size = 0;
    bool write_protected = false;
    uint16_t dos33_volume = 254;
} media_descriptor;

int identify_media(media_descriptor& md);
int display_media_descriptor(media_descriptor& md);
int display_2mg_header(format_2mg_t& hdr);

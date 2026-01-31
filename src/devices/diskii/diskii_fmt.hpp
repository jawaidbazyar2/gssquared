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

#include <cstdint> 
#include <string>

#include "util/media.hpp"

/**
 * Implements a Disk II disk image nibblizer.
 * 
 * Converts a raw disk image, which is just a series of sectors in logical
 * order, into a nibblized disk image, which is a series of encoded tracks.
 * 
 * Currently this is read-only.
 */

/**
 * The key stuff for this comes from Pages 3-18 and 3-19 of Beneath Apple DOS.
 */


/**
 * This is a translation table to implement the 6-and-2 nibble encoding.
 * 
 */

// AA, D5 - Reserved Bytes, not included in the 6-and-2 encoding.

extern uint8_t translate_62[64];


/**
 * Interleave orders for various OS disk formats.
 * Our raw simulated disk data input will always be in logical order,
 * e.g. track 0 sector 0 is followed by track 0 sector 1 etc.
 * When constructing our in-memory simulated nibblized disk data,
 * we can lay it out in the interleave order.
 * That said, since there is no actual disk spinning underneath the emulator,
 * if we just put them in physical order, the disk head will magically already
 * be in the same place it was the last time RWTS was called. So, these interleave
 * orders are probably not good for performance, but, they would more accurately
 * simulate disk operation.
 */
typedef uint16_t interleave_t[0x10];

/**
 * Constants for the disk image.
 * Currently hard-coded. In the future we might want to make these configurable
 * per disk_t structure.
 */

const int TRACKS_PER_DISK = 35;
const int SECTORS_PER_TRACK = 16;
const int SECTOR_SIZE = 0x0100;
const int TRACK_SIZE = 0x1A00; // https://retrocomputing.stackexchange.com/questions/27691/apple-nibble-disk-format-specification
const int TRACK_MAX_DATA = 0x18D0; // Nelson

const int GAP_A_SIZE = 64; // was 149
const int GAP_B_SIZE = 5; // This is what DOS33 uses. Seems to work for ProDOS too.
const int GAP_C_SIZE = 21;
/* const int ADDRESS_FIELD_SIZE = 15;
const int DATA_FIELD_SIZE = 350; */

const int DEFAULT_VOLUME = 0xFE;

// one sector unencoded data

typedef uint8_t sector_t[SECTOR_SIZE];

// one sector encoded (nibblized) data without checksum.
typedef uint8_t sector_62_t[342];

// one sector encoded (nibblized) data with checksum. Only used
// for debugging.
typedef uint8_t sector_62_ondisk_t[343];

// nibblized Track data structure
typedef struct {
    uint16_t size;
    uint16_t position;
    uint8_t data[TRACK_SIZE];
} track_t;

// Nibblized Disk data structure
typedef struct {
    interleave_t interleave_phys_to_logical;
    interleave_t interleave_logical_to_phys;
    track_t tracks[35]; // They are numbered from 0 to 34, track 0 being the
                        // outer most track and track 34 being the inner most.
} nibblized_disk_t;

// Disk data structure (raw, unencoded)
typedef struct {
    sector_t sectors[TRACKS_PER_DISK][SECTORS_PER_TRACK];
} disk_image_t;

extern interleave_t po_phys_to_logical;
extern interleave_t po_logical_to_phys;
extern interleave_t do_phys_to_logical;
extern interleave_t do_logical_to_phys;
extern interleave_t cpm_order;

void dump_disk_image(disk_image_t& disk_image);
int load_disk_image(media_descriptor *media, disk_image_t& disk_image);
void emit_disk(nibblized_disk_t& disk, disk_image_t& disk_image, int volume);
void write_nibblized_disk(media_descriptor *media, nibblized_disk_t& disk /* , const std::string& filename */);
void dump_disk(nibblized_disk_t& disk);
int load_nib_image(nibblized_disk_t& disk, const std::string& filename);
bool write_disk_image_po_do(media_descriptor *media, disk_image_t& disk_image);
bool write_disk_image_po_do_filename(disk_image_t& disk_image, std::string& filename);
int denibblize_disk_image(disk_image_t& disk_image, nibblized_disk_t& nib_disk, media_interleave_t interleave);
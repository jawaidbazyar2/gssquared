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

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "util/media.hpp"
#include "devices/diskii/diskii_fmt.hpp"

// ─── WOZ file magic ──────────────────────────────────────────────────────────
// First 8 bytes of a WOZ1 or WOZ2 file.
static constexpr uint8_t WOZ1_MAGIC[8] = { 'W','O','Z','1', 0xFF, 0x0A, 0x0D, 0x0A };
static constexpr uint8_t WOZ2_MAGIC[8] = { 'W','O','Z','2', 0xFF, 0x0A, 0x0D, 0x0A };

// ─── Chunk IDs (little-endian 32-bit) ────────────────────────────────────────
static constexpr uint32_t WOZ_CHUNK_INFO = 0x4F464E49u; // 'INFO'
static constexpr uint32_t WOZ_CHUNK_TMAP = 0x50414D54u; // 'TMAP'
static constexpr uint32_t WOZ_CHUNK_TRKS = 0x534B5254u; // 'TRKS'
static constexpr uint32_t WOZ_CHUNK_WRIT = 0x54495257u; // 'WRIT'
static constexpr uint32_t WOZ_CHUNK_META = 0x4154454Du; // 'META'
static constexpr uint32_t WOZ_CHUNK_FLUX = 0x58554C46u; // 'FLUX'

// ─── Fixed file-layout constants (WOZ spec §TRKS) ────────────────────────────
static constexpr size_t WOZ_HEADER_SIZE       = 12;    // magic(8) + crc(4)
static constexpr size_t WOZ_INFO_CHUNK_OFFSET = 12;    // INFO chunk starts here
static constexpr size_t WOZ_TMAP_CHUNK_OFFSET = 80;    // TMAP chunk starts here
static constexpr size_t WOZ_TRKS_CHUNK_OFFSET = 248;   // TRKS chunk starts here
static constexpr size_t WOZ_TRKS_DATA_OFFSET  = 256;   // TRK array starts here
static constexpr size_t WOZ_TRACK_DATA_OFFSET = 1536;  // block 3: first track bits
static constexpr size_t WOZ_BLOCK_SIZE        = 512;
static constexpr size_t WOZ_TRK_ARRAY_ENTRIES = 160;
static constexpr size_t WOZ_TRK_ENTRY_SIZE    = 8;     // 2+2+4 bytes per TRK entry

// WOZ1 per-track record size
static constexpr size_t WOZ1_TRACK_RECORD_SIZE = 6656;
static constexpr size_t WOZ1_BITSTREAM_SIZE    = 6646;

// INFO chunk total data size (always 60 bytes, zero-padded)
static constexpr size_t WOZ_INFO_DATA_SIZE = 60;
// TMAP chunk total data size (always 160 bytes)
static constexpr size_t WOZ_TMAP_DATA_SIZE = 160;

// ─── INFO chunk ──────────────────────────────────────────────────────────────
struct woz_info_t {
    uint8_t  version             = 2;    // INFO chunk version (1/2/3)
    uint8_t  disk_type           = 1;    // 1 = 5.25", 2 = 3.5"
    uint8_t  write_protected     = 0;
    uint8_t  synchronized        = 0;
    uint8_t  cleaned             = 1;    // fake bits replaced with 0s
    char     creator[32]         = {};   // space-padded UTF-8, no NUL per spec
    // version 2+
    uint8_t  disk_sides          = 1;
    uint8_t  boot_sector_format  = 0;    // 0=unknown, 1=16-sec, 2=13-sec, 3=both
    uint8_t  optimal_bit_timing  = 32;   // 32 × 125 ns = 4 µs (standard 5.25")
    uint16_t compatible_hardware = 0;
    uint16_t required_ram        = 0;
    uint16_t largest_track       = 0;    // max block_count across all tracks
    // version 3+
    uint16_t flux_block          = 0;
    uint16_t largest_flux_track  = 0;
};

// ─── Single track bitstream ───────────────────────────────────────────────────
// Bits are packed MSB-first within each byte (bit 7 of byte 0 is stream bit 0).
//
// A sync (self-sync) byte 0xFF is represented as 10 bits: 0b1111_1111_00
// The 8-bit nibble value comes first (MSB first), followed by two trailing
// timing zeros that allow the LSS to latch and reset before the next nibble.
//
// A regular data nibble is 8 bits (high bit first), since nibbles always have
// the high bit set and contain no more than two consecutive zero bits.
struct woz_track_t {
    std::vector<uint8_t> bits;     // bit-packed stream, MSB-first per byte
    uint32_t             bit_count = 0;
};

// ─── Full in-memory WOZ image ─────────────────────────────────────────────────
struct woz_image_t {
    int                              file_version = 2;  // from magic: 1 or 2
    woz_info_t                       info;
    uint8_t                          tmap[160];         // 0xFF = empty quarter-track
    std::vector<woz_track_t>         tracks;            // indexed by TMAP entry value
    std::map<std::string,std::string> meta;             // optional META chunk k/v pairs

    // tmap must be initialised to 0xFF (empty track sentinel per spec).
    woz_image_t() { std::fill(std::begin(tmap), std::end(tmap), 0xFF); }
};

// ─── Woz class ────────────────────────────────────────────────────────────────
class Woz {
public:
    // Constructs a blank WOZ2 5.25" image with creator string pre-filled.
    Woz();

    // Default destructor: std::vector / std::map members handle all cleanup.
    // Declared explicitly as a positive assertion that no manual resource
    // management is needed (no raw pointers or open file handles are kept).
    ~Woz() = default;

    // ── Primary operations ───────────────────────────────────────────────────

    // Load a WOZ 1.0 or 2.x file from disk into m_image.
    // Validates magic cookie and CRC32 (if non-zero).
    // Returns 0 on success, -1 on error.
    int load(const std::string& filename);

    // Write m_image to disk as WOZ 2.x with a freshly computed CRC32.
    // Returns 0 on success, -1 on error.
    int save(const std::string& filename);

    // Build m_image from a block-based or nibblized disk image described by
    // media (as returned by identify_media).  Generates a proper Apple II
    // 6&2-encoded bit stream where sync bytes occupy 10 bits (0b00_1111_1111).
    // Only MEDIA_NYBBLE (DO/PO) sources are supported.
    // Returns 0 on success, -1 on error.
    int import_from_media(media_descriptor* media);

    // ── Accessors ────────────────────────────────────────────────────────────

    woz_image_t&        image()       { return m_image; }
    const woz_image_t&  image() const { return m_image; }

    // Returns a pointer to the bit-stream for the given quarter-track index
    // (0–159).  Returns nullptr if the TMAP entry is 0xFF (empty track).
    const woz_track_t* get_track(int quarter_track) const;

    // ── Human-readable diagnostics ───────────────────────────────────────────
    void dump_info()   const;
    void dump_tmap()   const;
    void dump_tracks() const;

private:
    woz_image_t m_image;

    // ── Bit-stream primitives ────────────────────────────────────────────────
    // Append one bit (0 or 1) to trk, MSB-first within each byte.
    void emit_bit(woz_track_t& trk, int bit);
    // Emit a sync byte as 10 bits: 00 followed by 11111111.
    void emit_sync_byte(woz_track_t& trk);
    // Emit an 8-bit data nibble, high bit first.
    void emit_data_byte(woz_track_t& trk, uint8_t byte);
    // Emit N sync bytes.
    void emit_sync_bytes(woz_track_t& trk, int n);

    // ── Track-encoding helpers (mirror of diskii_fmt, bit-stream edition) ────
    void emit_encoded_44(woz_track_t& trk, uint8_t value);
    void emit_address_field(woz_track_t& trk,
                            uint8_t volume, uint8_t track_num, uint8_t sector_num);
    void emit_nibblized_sector(woz_track_t& trk, sector_t& in);
    void emit_data_field(woz_track_t& trk, sector_t& in);
    void emit_sector(woz_track_t& trk, sector_t& in,
                     uint8_t volume, uint8_t track_num, uint8_t sector_num);
    woz_track_t build_track(disk_image_t& disk_image,
                            const interleave_t& phys_to_logical,
                            int track_num, uint8_t volume);
    // Convert a raw .nib track buffer to a WOZ bitstream.
    // 0xFF bytes become 10-bit sync nibbles (1111111100); all others are
    // emitted as 8-bit data bytes (MSB first).
    woz_track_t build_track_from_nib(const uint8_t* nib_data, uint32_t nib_size);
    // Import a MEDIA_PRENYBBLE (.nib) image into the in-memory WOZ representation.
    int import_from_nib(media_descriptor* media);

    // ── Chunk parsers ────────────────────────────────────────────────────────
    int parse_info(const uint8_t* data, uint32_t size);
    int parse_tmap(const uint8_t* data, uint32_t size);
    // WOZ1: fixed 6656-byte tracks packed directly in the chunk data.
    int parse_trks_v1(const uint8_t* data, uint32_t size);
    // WOZ2: 160-entry TRK descriptor array + block-addressed bit data.
    // file_buf / file_size are needed to resolve block addresses.
    int parse_trks_v2(const std::vector<uint8_t>& file_buf,
                      size_t chunk_data_offset, uint32_t chunk_size);
    int parse_meta(const uint8_t* data, uint32_t size);

    // ── Chunk writers (always produce WOZ2) ──────────────────────────────────
    void append_u16le(std::vector<uint8_t>& v, uint16_t val);
    void append_u32le(std::vector<uint8_t>& v, uint32_t val);
    void write_chunk(std::vector<uint8_t>& out, uint32_t id,
                     const std::vector<uint8_t>& data);
    std::vector<uint8_t> build_info_chunk();
    std::vector<uint8_t> build_tmap_chunk();
    // Returns the TRKS chunk *data* (TRK array + bit blocks); also updates
    // info.largest_track as a side effect.
    std::vector<uint8_t> build_trks_chunk();
    std::vector<uint8_t> build_meta_chunk();

    // ── CRC32 (Gary S. Brown 1986, per WOZ spec Appendix A) ─────────────────
    static uint32_t crc32(const uint8_t* buf, size_t size);
    static const uint32_t crc32_tab[256];
};

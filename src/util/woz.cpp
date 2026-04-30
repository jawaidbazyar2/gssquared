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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>

#include "util/woz.hpp"

// ─── CRC32 table (Gary S. Brown 1986, verbatim from WOZ spec Appendix A) ─────

const uint32_t Woz::crc32_tab[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t Woz::crc32(const uint8_t* buf, size_t size) {
    uint32_t crc = 0u ^ ~0u;
    while (size--)
        crc = crc32_tab[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    return crc ^ ~0u;
}

// ─── Constructor ─────────────────────────────────────────────────────────────

Woz::Woz() {
    // Space-pad creator to exactly 32 bytes per spec.
    static const char CREATOR[] = "GS2 WOZ                         ";
    std::memcpy(m_image.info.creator, CREATOR, 32);
}

// ─── Bit-stream primitives ────────────────────────────────────────────────────

void Woz::emit_bit(woz_track_t& trk, int bit) {
    uint32_t byte_idx = trk.bit_count >> 3;
    uint32_t bit_idx  = 7 - (trk.bit_count & 7); // MSB first
    if (byte_idx >= trk.bits.size())
        trk.bits.push_back(0x00);
    if (bit)
        trk.bits[byte_idx] |= (1u << bit_idx);
    ++trk.bit_count;
}

// A sync byte is 10 bits: the $FF nibble (8 ones) followed by 2 trailing
// timing zeros.  Physical bit order on disk: 1111111100.  The trailing zeros
// give the Logic State Sequencer time to latch and reset before the next
// nibble's first 1-bit arrives, enabling self-synchronization.
void Woz::emit_sync_byte(woz_track_t& trk) {
    for (int i = 0; i < 8; i++) emit_bit(trk, 1);
    emit_bit(trk, 0);
    emit_bit(trk, 0);
}

void Woz::emit_data_byte(woz_track_t& trk, uint8_t byte) {
    for (int i = 7; i >= 0; i--)
        emit_bit(trk, (byte >> i) & 1);
}

void Woz::emit_sync_bytes(woz_track_t& trk, int n) {
    for (int i = 0; i < n; i++) emit_sync_byte(trk);
}

// ─── Track-encoding helpers ───────────────────────────────────────────────────

void Woz::emit_encoded_44(woz_track_t& trk, uint8_t value) {
    uint8_t xx = ((value & 0b10101010) >> 1) | 0b10101010;
    uint8_t yy =  (value & 0b01010101)       | 0b10101010;
    emit_data_byte(trk, xx);
    emit_data_byte(trk, yy);
}

void Woz::emit_address_field(woz_track_t& trk,
                              uint8_t volume, uint8_t track_num, uint8_t sector_num) {
    // Address prologue
    emit_data_byte(trk, 0xD5);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0x96);
    // Volume, track, sector, checksum – each encoded 4&4
    uint8_t checksum = volume ^ track_num ^ sector_num;
    emit_encoded_44(trk, volume);
    emit_encoded_44(trk, track_num);
    emit_encoded_44(trk, sector_num);
    emit_encoded_44(trk, checksum);
    // Address epilogue
    emit_data_byte(trk, 0xDE);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xEB);
}

// ─── Local 6&2 encoding helpers ──────────────────────────────────────────────
// These are self-contained copies of translate_62 / prenibble from
// diskii_fmt.cpp so that gs2_woz has no link-time dependency on that module.

static const uint8_t woz_translate_62[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Convert a 256-byte sector into the 342-byte GCR nibble buffer used by the
// 6&2 encoder.  nbuf[0x00..0xFF] = NBUF1, nbuf[0x100..0x155] = NBUF2.
static void woz_prenibble(sector_t& buf, sector_62_t& nbuf) {
    uint8_t *nbuf1 = nbuf;
    uint8_t *nbuf2 = nbuf + 0x100;

    int c, x, a;
    uint8_t y;

prenib16:
    x = 0;
    y = 2;

prenib1:
    y--;
    a = buf[y];

    c = a & 0b01;
    a >>= 1;
    nbuf2[x] = (nbuf2[x] << 1) | c;

    c = a & 0b01;
    a >>= 1;
    nbuf2[x] = (nbuf2[x] << 1) | c;

    nbuf1[y] = a;
    x++;
    if (x < 0x56) goto prenib1;
    x = 0;
    if (y != 0) goto prenib1;

    for (x = 0x55; x >= 0; x--)
        nbuf2[x] &= 0x3F;
}

// Emit the 343-byte 6&2-encoded data field body (no prologue/epilogue).
// Mirrors emit_nibblized_sector() in diskii_fmt.cpp exactly, using
// emit_data_byte() instead of emit_track_byte().
void Woz::emit_nibblized_sector(woz_track_t& trk, sector_t& in) {
    sector_62_t nbuf;
    woz_prenibble(in, nbuf);

    uint8_t last = 0;
    for (int i = 0x0155; i >= 0x0100; i--) {
        emit_data_byte(trk, woz_translate_62[nbuf[i] ^ last]);
        last = nbuf[i];
    }
    for (int i = 0x00; i <= 0xFF; i++) {
        emit_data_byte(trk, woz_translate_62[nbuf[i] ^ last]);
        last = nbuf[i];
    }
    emit_data_byte(trk, woz_translate_62[nbuf[0xFF]]); // checksum byte
}

void Woz::emit_data_field(woz_track_t& trk, sector_t& in) {
    // Data prologue
    emit_data_byte(trk, 0xD5);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xAD);

    emit_nibblized_sector(trk, in);

    // Data epilogue
    emit_data_byte(trk, 0xDE);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xEB);
}

void Woz::emit_sector(woz_track_t& trk, sector_t& in,
                       uint8_t volume, uint8_t track_num, uint8_t sector_num) {
    emit_address_field(trk, volume, track_num, sector_num);
    emit_sync_bytes(trk, GAP_B_SIZE);
    emit_data_field(trk, in);
    emit_sync_bytes(trk, GAP_C_SIZE);
}

woz_track_t Woz::build_track(disk_image_t& disk_image,
                               const interleave_t& phys_to_logical,
                               int track_num, uint8_t volume) {
    woz_track_t trk;
    // Gap A: self-sync bytes at the start of the track
    emit_sync_bytes(trk, GAP_A_SIZE);
    // Sectors in physical order
    for (int s = 0; s < SECTORS_PER_TRACK; s++) {
        int logical = phys_to_logical[s];
        emit_sector(trk,
                    reinterpret_cast<sector_t&>(disk_image.sectors[track_num][logical]),
                    volume,
                    static_cast<uint8_t>(track_num),
                    static_cast<uint8_t>(s));
    }
    return trk;
}

// Convert a raw .nib track buffer to a WOZ bitstream.
// The .nib format stores one track as 0x1A00 bytes of pre-encoded nibble data.
// 0xFF bytes are sync nibbles — they occupy 10 bits on disk (1111111100) rather
// than 8, giving the LSS time to latch and self-synchronise.  All other bytes
// are valid data nibbles emitted as 8 bits, MSB first.
woz_track_t Woz::build_track_from_nib(const uint8_t* nib_data, uint32_t nib_size) {
    woz_track_t trk;
    for (uint32_t i = 0; i < nib_size; i++) {
        if (nib_data[i] == 0xFF)
            emit_sync_byte(trk);            // 10 bits: 1111111100
        else
            emit_data_byte(trk, nib_data[i]);  // 8 bits, MSB first
    }
    return trk;
}

// ─── Chunk parsers ────────────────────────────────────────────────────────────

int Woz::parse_info(const uint8_t* data, uint32_t size) {
    if (size < 37) {
        std::cerr << "WOZ: INFO chunk too small (" << size << " bytes)\n";
        return -1;
    }
    woz_info_t& info = m_image.info;
    info.version         = data[0];
    info.disk_type       = data[1];
    info.write_protected = data[2];
    info.synchronized    = data[3];
    info.cleaned         = data[4];
    std::memcpy(info.creator, &data[5], 32);

    if (info.version >= 2 && size >= 50) {
        info.disk_sides         = data[37];
        info.boot_sector_format = data[38];
        info.optimal_bit_timing = data[39];
        info.compatible_hardware = static_cast<uint16_t>(data[40]) |
                                   (static_cast<uint16_t>(data[41]) << 8);
        info.required_ram        = static_cast<uint16_t>(data[42]) |
                                   (static_cast<uint16_t>(data[43]) << 8);
        info.largest_track       = static_cast<uint16_t>(data[44]) |
                                   (static_cast<uint16_t>(data[45]) << 8);
    }
    if (info.version >= 3 && size >= 54) {
        info.flux_block          = static_cast<uint16_t>(data[46]) |
                                   (static_cast<uint16_t>(data[47]) << 8);
        info.largest_flux_track  = static_cast<uint16_t>(data[48]) |
                                   (static_cast<uint16_t>(data[49]) << 8);
    }
    return 0;
}

int Woz::parse_tmap(const uint8_t* data, uint32_t size) {
    if (size < 160) {
        std::cerr << "WOZ: TMAP chunk too small (" << size << " bytes)\n";
        return -1;
    }
    std::memcpy(m_image.tmap, data, 160);
    return 0;
}

// WOZ 1.0: each track is a fixed 6656-byte record.
int Woz::parse_trks_v1(const uint8_t* data, uint32_t size) {
    uint32_t num_tracks = size / WOZ1_TRACK_RECORD_SIZE;
    m_image.tracks.resize(num_tracks);

    for (uint32_t i = 0; i < num_tracks; i++) {
        const uint8_t* rec = data + i * WOZ1_TRACK_RECORD_SIZE;
        uint16_t bytes_used = static_cast<uint16_t>(rec[WOZ1_BITSTREAM_SIZE]) |
                              (static_cast<uint16_t>(rec[WOZ1_BITSTREAM_SIZE + 1]) << 8);
        uint16_t bit_count  = static_cast<uint16_t>(rec[WOZ1_BITSTREAM_SIZE + 2]) |
                              (static_cast<uint16_t>(rec[WOZ1_BITSTREAM_SIZE + 3]) << 8);

        woz_track_t& trk = m_image.tracks[i];
        trk.bit_count = bit_count;
        uint32_t copy_bytes = bytes_used ? bytes_used : WOZ1_BITSTREAM_SIZE;
        trk.bits.assign(rec, rec + copy_bytes);
    }
    return 0;
}

// WOZ 2.0: variable-length tracks via 160-entry TRK descriptor array.
// file_buf contains the entire file; chunk_data_offset is the byte offset
// of the TRKS chunk *data* (= byte 256 in a well-formed WOZ2 file).
int Woz::parse_trks_v2(const std::vector<uint8_t>& file_buf,
                        size_t chunk_data_offset, uint32_t /*chunk_size*/) {
    // Determine how many track entries are referenced by TMAP.
    uint8_t max_trk = 0;
    bool any = false;
    for (int q = 0; q < 160; q++) {
        if (m_image.tmap[q] != 0xFF) {
            if (!any || m_image.tmap[q] > max_trk) {
                max_trk = m_image.tmap[q];
                any = true;
            }
        }
    }
    if (!any) return 0; // no tracks

    uint32_t num_tracks = static_cast<uint32_t>(max_trk) + 1;
    m_image.tracks.resize(num_tracks);

    for (uint32_t i = 0; i < num_tracks; i++) {
        size_t entry_off = chunk_data_offset + i * WOZ_TRK_ENTRY_SIZE;
        if (entry_off + WOZ_TRK_ENTRY_SIZE > file_buf.size()) break;

        const uint8_t* e = file_buf.data() + entry_off;
        uint16_t starting_block = static_cast<uint16_t>(e[0]) |
                                  (static_cast<uint16_t>(e[1]) << 8);
        uint16_t block_count    = static_cast<uint16_t>(e[2]) |
                                  (static_cast<uint16_t>(e[3]) << 8);
        uint32_t bit_count      = static_cast<uint32_t>(e[4]) |
                                  (static_cast<uint32_t>(e[5]) << 8) |
                                  (static_cast<uint32_t>(e[6]) << 16) |
                                  (static_cast<uint32_t>(e[7]) << 24);

        if (starting_block == 0 && block_count == 0) continue; // empty entry

        size_t byte_offset = static_cast<size_t>(starting_block) * WOZ_BLOCK_SIZE;
        size_t byte_count  = (bit_count + 7) / 8;
        size_t alloc_bytes = static_cast<size_t>(block_count) * WOZ_BLOCK_SIZE;

        if (byte_offset + alloc_bytes > file_buf.size()) {
            std::cerr << "WOZ: TRK[" << i << "] data out of file bounds\n";
            continue;
        }

        woz_track_t& trk = m_image.tracks[i];
        trk.bit_count = bit_count;
        trk.bits.assign(file_buf.data() + byte_offset,
                        file_buf.data() + byte_offset + byte_count);
    }
    return 0;
}

int Woz::parse_meta(const uint8_t* data, uint32_t size) {
    // Tab-delimited key\tvalue\n rows.
    std::string content(reinterpret_cast<const char*>(data), size);
    size_t pos = 0;
    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        if (eol == std::string::npos) eol = content.size();
        std::string row = content.substr(pos, eol - pos);
        pos = eol + 1;
        size_t tab = row.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = row.substr(0, tab);
        std::string val = row.substr(tab + 1);
        if (!key.empty()) m_image.meta[key] = val;
    }
    return 0;
}

// ─── Load ─────────────────────────────────────────────────────────────────────

int Woz::load(const std::string& filename) {
    current_image_filename = filename;
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "WOZ: cannot open '" << filename << "'\n";
        return -1;
    }

    // Read the entire file into memory.
    fseek(fp, 0, SEEK_END);
    long file_sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_sz < static_cast<long>(WOZ_HEADER_SIZE + 8)) {
        std::cerr << "WOZ: file too small\n";
        fclose(fp);
        return -1;
    }
    std::vector<uint8_t> buf(static_cast<size_t>(file_sz));
    if (fread(buf.data(), 1, buf.size(), fp) != buf.size()) {
        std::cerr << "WOZ: read error on '" << filename << "'\n";
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // Validate magic cookie.
    if (std::memcmp(buf.data(), WOZ1_MAGIC, 8) == 0) {
        m_image.file_version = 1;
    } else if (std::memcmp(buf.data(), WOZ2_MAGIC, 8) == 0) {
        m_image.file_version = 2;
    } else {
        std::cerr << "WOZ: unrecognised magic in '" << filename << "'\n";
        return -1;
    }

    // Validate CRC32 (skip if stored value is 0).
    uint32_t stored_crc = static_cast<uint32_t>(buf[8])  |
                         (static_cast<uint32_t>(buf[9])  << 8) |
                         (static_cast<uint32_t>(buf[10]) << 16) |
                         (static_cast<uint32_t>(buf[11]) << 24);
    if (stored_crc != 0) {
        uint32_t computed = crc32(buf.data() + WOZ_HEADER_SIZE,
                                  buf.size() - WOZ_HEADER_SIZE);
        if (computed != stored_crc) {
            std::cerr << "WOZ: CRC32 mismatch (stored 0x" << std::hex << stored_crc
                      << ", computed 0x" << computed << std::dec << ")\n";
            return -1;
        }
    }

    // Reset image to blank state before populating from file.
    m_image.tracks.clear();
    m_image.meta.clear();
    std::fill(std::begin(m_image.tmap), std::end(m_image.tmap), 0xFF);

    // Walk chunks starting at byte 12.
    size_t pos = WOZ_HEADER_SIZE;
    bool tmap_parsed = false;

    while (pos + 8 <= buf.size()) {
        uint32_t chunk_id   = static_cast<uint32_t>(buf[pos])   |
                             (static_cast<uint32_t>(buf[pos+1]) << 8) |
                             (static_cast<uint32_t>(buf[pos+2]) << 16) |
                             (static_cast<uint32_t>(buf[pos+3]) << 24);
        uint32_t chunk_size = static_cast<uint32_t>(buf[pos+4]) |
                             (static_cast<uint32_t>(buf[pos+5]) << 8) |
                             (static_cast<uint32_t>(buf[pos+6]) << 16) |
                             (static_cast<uint32_t>(buf[pos+7]) << 24);

        size_t data_off = pos + 8;
        if (data_off + chunk_size > buf.size()) {
            std::cerr << "WOZ: chunk data extends beyond file end\n";
            break;
        }
        const uint8_t* data = buf.data() + data_off;

        int rc = 0;
        switch (chunk_id) {
            case WOZ_CHUNK_INFO:
                rc = parse_info(data, chunk_size);
                break;
            case WOZ_CHUNK_TMAP:
                rc = parse_tmap(data, chunk_size);
                tmap_parsed = true;
                break;
            case WOZ_CHUNK_TRKS:
                if (!tmap_parsed) {
                    std::cerr << "WOZ: TRKS chunk appears before TMAP – skipping\n";
                } else if (m_image.file_version == 1) {
                    rc = parse_trks_v1(data, chunk_size);
                } else {
                    rc = parse_trks_v2(buf, data_off, chunk_size);
                }
                break;
            case WOZ_CHUNK_META:
                rc = parse_meta(data, chunk_size);
                break;
            default:
                // Unknown chunk – spec says to skip it.
                break;
        }
        if (rc != 0) return rc;

        pos = data_off + chunk_size;
    }
    return 0;
}

// ─── Chunk writers ────────────────────────────────────────────────────────────

void Woz::append_u16le(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(val & 0xFF);
    v.push_back((val >> 8) & 0xFF);
}

void Woz::append_u32le(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(val & 0xFF);
    v.push_back((val >> 8) & 0xFF);
    v.push_back((val >> 16) & 0xFF);
    v.push_back((val >> 24) & 0xFF);
}

void Woz::write_chunk(std::vector<uint8_t>& out, uint32_t id,
                       const std::vector<uint8_t>& data) {
    append_u32le(out, id);
    append_u32le(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
}

std::vector<uint8_t> Woz::build_info_chunk() {
    const woz_info_t& info = m_image.info;
    std::vector<uint8_t> d;
    d.reserve(WOZ_INFO_DATA_SIZE);

    // Bump version to 3 if flux fields are set, otherwise 2.
    uint8_t ver = (info.flux_block || info.largest_flux_track) ? 3 : 2;
    d.push_back(ver);
    d.push_back(info.disk_type);
    d.push_back(info.write_protected);
    d.push_back(info.synchronized);
    d.push_back(info.cleaned);
    d.insert(d.end(), info.creator, info.creator + 32); // +5..+36
    d.push_back(info.disk_sides);                        // +37
    d.push_back(info.boot_sector_format);                // +38
    d.push_back(info.optimal_bit_timing);                // +39
    append_u16le(d, info.compatible_hardware);           // +40
    append_u16le(d, info.required_ram);                  // +42
    append_u16le(d, info.largest_track);                 // +44
    append_u16le(d, info.flux_block);                    // +46
    append_u16le(d, info.largest_flux_track);            // +48
    // Pad to 60 bytes.
    while (d.size() < WOZ_INFO_DATA_SIZE) d.push_back(0x00);
    return d;
}

std::vector<uint8_t> Woz::build_tmap_chunk() {
    std::vector<uint8_t> d(m_image.tmap, m_image.tmap + WOZ_TMAP_DATA_SIZE);
    return d;
}

// Returns the TRKS chunk data (TRK array + block-aligned bit streams).
// Side effect: updates info.largest_track.
std::vector<uint8_t> Woz::build_trks_chunk() {
    const size_t num_tracks = m_image.tracks.size();
    // TRK descriptor array: always 160 × 8 bytes (unused entries zeroed).
    const size_t trk_array_bytes = WOZ_TRK_ARRAY_ENTRIES * WOZ_TRK_ENTRY_SIZE; // 1280

    std::vector<uint8_t> trk_array(trk_array_bytes, 0);
    std::vector<uint8_t> bit_data;

    // Track data starts at block 3 of the file.
    // The TRKS chunk data begins at file offset 256.
    // File offset of track data = 256 (chunk data start) + 1280 (TRK array) = 1536 = block 3.
    uint16_t current_block = 3;
    uint16_t largest_blocks = 0;

    for (size_t i = 0; i < num_tracks && i < WOZ_TRK_ARRAY_ENTRIES; i++) {
        const woz_track_t& trk = m_image.tracks[i];
        if (trk.bit_count == 0) continue;

        uint32_t byte_count  = (trk.bit_count + 7) / 8;
        uint16_t block_count = static_cast<uint16_t>((byte_count + WOZ_BLOCK_SIZE - 1) / WOZ_BLOCK_SIZE);

        // Write TRK descriptor.
        size_t entry_off = i * WOZ_TRK_ENTRY_SIZE;
        trk_array[entry_off + 0] = current_block & 0xFF;
        trk_array[entry_off + 1] = (current_block >> 8) & 0xFF;
        trk_array[entry_off + 2] = block_count & 0xFF;
        trk_array[entry_off + 3] = (block_count >> 8) & 0xFF;
        trk_array[entry_off + 4] = trk.bit_count & 0xFF;
        trk_array[entry_off + 5] = (trk.bit_count >> 8) & 0xFF;
        trk_array[entry_off + 6] = (trk.bit_count >> 16) & 0xFF;
        trk_array[entry_off + 7] = (trk.bit_count >> 24) & 0xFF;

        // Append bit data, zero-padded to block boundary.
        size_t block_bytes = static_cast<size_t>(block_count) * WOZ_BLOCK_SIZE;
        size_t old_size = bit_data.size();
        bit_data.resize(old_size + block_bytes, 0x00);
        std::memcpy(bit_data.data() + old_size, trk.bits.data(),
                    std::min(trk.bits.size(), block_bytes));

        if (block_count > largest_blocks) largest_blocks = block_count;
        current_block += block_count;
    }

    m_image.info.largest_track = largest_blocks;

    std::vector<uint8_t> chunk_data;
    chunk_data.reserve(trk_array_bytes + bit_data.size());
    chunk_data.insert(chunk_data.end(), trk_array.begin(), trk_array.end());
    chunk_data.insert(chunk_data.end(), bit_data.begin(), bit_data.end());
    return chunk_data;
}

std::vector<uint8_t> Woz::build_meta_chunk() {
    std::string s;
    for (const auto& kv : m_image.meta) {
        s += kv.first;
        s += '\t';
        s += kv.second;
        s += '\n';
    }
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ─── Save ─────────────────────────────────────────────────────────────────────

int Woz::save(const std::string& filename) {
    std::vector<uint8_t> out;
    out.reserve(64 * 1024);

    // File header: 8 bytes magic + 4 bytes CRC placeholder.
    const uint8_t* magic = WOZ2_MAGIC;
    out.insert(out.end(), magic, magic + 8);
    append_u32le(out, 0x00000000); // CRC placeholder, patched below

    // Build TRKS data first: build_trks_chunk() updates info.largest_track as a
    // side effect, and that value must be present when INFO is serialised below.
    std::vector<uint8_t> trks_data = build_trks_chunk();

    // Chunks must appear in this order to satisfy fixed file-offset constraints.
    // INFO chunk at offset 12.
    write_chunk(out, WOZ_CHUNK_INFO, build_info_chunk());
    // TMAP chunk at offset 80.
    write_chunk(out, WOZ_CHUNK_TMAP, build_tmap_chunk());
    // TRKS chunk at offset 248.
    write_chunk(out, WOZ_CHUNK_TRKS, trks_data);
    // META chunk (optional).
    if (!m_image.meta.empty()) {
        write_chunk(out, WOZ_CHUNK_META, build_meta_chunk());
    }

    // Compute and patch CRC32 (covers all bytes after the 12-byte header).
    uint32_t crc = crc32(out.data() + WOZ_HEADER_SIZE, out.size() - WOZ_HEADER_SIZE);
    out[8]  =  crc        & 0xFF;
    out[9]  = (crc >>  8) & 0xFF;
    out[10] = (crc >> 16) & 0xFF;
    out[11] = (crc >> 24) & 0xFF;

    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "WOZ: cannot create '" << filename << "'\n";
        return -1;
    }
    bool ok = (fwrite(out.data(), 1, out.size(), fp) == out.size());
    fclose(fp);
    if (!ok) {
        std::cerr << "WOZ: write error on '" << filename << "'\n";
        return -1;
    }
    return 0;
}

// ─── Import from media ────────────────────────────────────────────────────────

int Woz::import_from_nib(media_descriptor* media) {
    nibblized_disk_t nib;
    if (load_nib_image(nib, media->filename) != 0) {
        std::cerr << "WOZ: failed to load .nib image from '" << media->filename << "'\n";
        return -1;
    }

    // Reset the in-memory image.
    std::fill(std::begin(m_image.tmap), std::end(m_image.tmap), 0xFF);
    m_image.tracks.clear();
    m_image.tracks.reserve(TRACKS_PER_DISK);

    // Populate INFO fields.
    m_image.info.disk_type          = 1;   // 5.25"
    m_image.info.write_protected    = media->write_protected ? 1 : 0;
    m_image.info.synchronized       = 0;
    m_image.info.cleaned            = 1;
    m_image.info.optimal_bit_timing = 32;  // 4 µs
    m_image.info.boot_sector_format = 1;   // 16-sector
    m_image.info.disk_sides         = 1;

    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        uint8_t trk_idx = static_cast<uint8_t>(m_image.tracks.size());
        m_image.tracks.push_back(
            build_track_from_nib(nib.tracks[t].data, nib.tracks[t].size));

        // Map the whole track and the adjacent T.25 quarter-track.
        int qt = t * 4;
        if (qt     < 160) m_image.tmap[qt]     = trk_idx;  // T.00
        if (qt + 1 < 160) m_image.tmap[qt + 1] = trk_idx;  // T.25 (readable)
    }

    return 0;
}

int Woz::import_from_media(media_descriptor* media) {
    if (!media) return -1;

    if (media->media_type == MEDIA_PRENYBBLE) {
        return import_from_nib(media);
    }

    if (media->media_type != MEDIA_NYBBLE) {
        std::cerr << "WOZ: import only supports MEDIA_NYBBLE / MEDIA_PRENYBBLE sources "
                     "(DOS/ProDOS 140K floppy images)\n";
        return -1;
    }

    disk_image_t disk_image;
    if (load_disk_image(media, disk_image) != 0) {
        std::cerr << "WOZ: failed to load disk image from '" << media->filename << "'\n";
        return -1;
    }

    // Select interleave table.
    const interleave_t* phys_to_logical = nullptr;
    if (media->interleave == INTERLEAVE_PO) {
        phys_to_logical = &po_phys_to_logical;
    } else if (media->interleave == INTERLEAVE_DO) {
        phys_to_logical = &do_phys_to_logical;
    } else {
        std::cerr << "WOZ: unsupported interleave " << media->interleave << "\n";
        return -1;
    }

    // Reset the in-memory image.
    std::fill(std::begin(m_image.tmap), std::end(m_image.tmap), 0xFF);
    m_image.tracks.clear();
    m_image.tracks.reserve(TRACKS_PER_DISK);

    // Populate INFO fields appropriate for an imported disk.
    m_image.info.disk_type          = 1;   // 5.25"
    m_image.info.write_protected    = media->write_protected ? 1 : 0;
    m_image.info.synchronized       = 0;
    m_image.info.cleaned            = 1;
    m_image.info.optimal_bit_timing = 32;  // 4 µs
    m_image.info.boot_sector_format = 1;   // assume 16-sector unless told otherwise
    m_image.info.disk_sides         = 1;

    uint8_t volume = static_cast<uint8_t>(media->dos33_volume);

    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        uint8_t trk_idx = static_cast<uint8_t>(m_image.tracks.size());
        m_image.tracks.push_back(build_track(disk_image, *phys_to_logical, t, volume));

        // Map the whole track and its two adjacent quarter-tracks to this entry.
        // Quarter-track index for whole track T is T*4.
        int qt = t * 4;
        if (qt     < 160) m_image.tmap[qt]     = trk_idx;    // T.00
        if (qt + 1 < 160) m_image.tmap[qt + 1] = trk_idx;   // T.25 (readable)
    }

    return 0;
}

// ─── Accessors ────────────────────────────────────────────────────────────────

const woz_track_t* Woz::get_track_ptr(int quarter_track) const {
    if (quarter_track < 0 || quarter_track >= 160) return nullptr;
    uint8_t idx = m_image.tmap[quarter_track];
    if (idx == 0xFF) return nullptr;
    if (idx >= m_image.tracks.size()) return nullptr;
    return &m_image.tracks[idx];
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

void Woz::dump_info() const {
    const woz_info_t& i = m_image.info;
    char creator[33];
    std::memcpy(creator, i.creator, 32);
    creator[32] = '\0';
    // Trim trailing spaces for display.
    for (int k = 31; k >= 0 && creator[k] == ' '; k--) creator[k] = '\0';

    printf("WOZ file version : %d\n", m_image.file_version);
    printf("INFO version     : %d\n", i.version);
    printf("Disk type        : %s\n", i.disk_type == 1 ? "5.25\"" : "3.5\"");
    printf("Write protected  : %s\n", i.write_protected ? "yes" : "no");
    printf("Synchronized     : %s\n", i.synchronized ? "yes" : "no");
    printf("Cleaned          : %s\n", i.cleaned ? "yes" : "no");
    printf("Creator          : \"%s\"\n", creator);
    if (i.version >= 2) {
        printf("Disk sides       : %d\n", i.disk_sides);
        printf("Boot sec format  : %d\n", i.boot_sector_format);
        printf("Optimal bit time : %d (%.3f µs)\n",
               i.optimal_bit_timing, i.optimal_bit_timing * 0.125);
        printf("Compat hardware  : 0x%04X\n", i.compatible_hardware);
        printf("Required RAM     : %d K\n", i.required_ram);
        printf("Largest track    : %d blocks\n", i.largest_track);
    }
    if (i.version >= 3) {
        printf("FLUX block       : %d\n", i.flux_block);
        printf("Largest flux trk : %d\n", i.largest_flux_track);
    }
    printf("Tracks stored    : %zu\n", m_image.tracks.size());
    if (!m_image.meta.empty()) {
        printf("META entries     : %zu\n", m_image.meta.size());
    }
}

void Woz::dump_tmap() const {
    printf("TMAP (quarter-track → TRK index):\n");
    for (int q = 0; q < 160; q++) {
        uint8_t v = m_image.tmap[q];
        if (v == 0xFF) continue;
        int whole  = q / 4;
        int frac   = q % 4;
        static const char* fracs[] = { ".00", ".25", ".50", ".75" };
        printf("  [%3d] Track %2d%s → TRK[%d]\n", q, whole, fracs[frac], v);
    }
}

void Woz::dump_tracks() const {
    for (size_t i = 0; i < m_image.tracks.size(); i++) {
        const woz_track_t& trk = m_image.tracks[i];
        uint32_t bytes = (trk.bit_count + 7) / 8;
        uint32_t blocks = (bytes + (uint32_t)WOZ_BLOCK_SIZE - 1) / (uint32_t)WOZ_BLOCK_SIZE;
        printf("  TRK[%3zu]  bits=%7u  bytes=%5u  blocks=%3u\n",
               i, trk.bit_count, bytes, blocks);
    }
}

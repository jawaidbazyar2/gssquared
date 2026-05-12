#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include "woz_nibblizer_35.hpp"
#include "util/media.hpp"

/*
    6&2 for 3.5 encoding/decoding taken from CiderPress2, Andy McFadden.
    https://ciderpress2.com/formatdoc/Nibble-notes.html
*/

bool Woz_Nibblizer_35::EncodeSector62_524(uint8_t* output, const uint8_t* buffer) {
    if (output == nullptr || buffer == nullptr) {
        return false;
    }

    uint8_t part0[CHUNK_SIZE_62_524];
    uint8_t part1[CHUNK_SIZE_62_524];
    uint8_t part2[CHUNK_SIZE_62_524];

    int inIdx = 0;
    int outIdx = 0;

    uint32_t chk0 = 0, chk1 = 0, chk2 = 0;

    // Compute checksum, and split the input into 3 parts.
    int idx = 0;
    while (true) {
        chk0 = (chk0 & 0xff) << 1;
        if ((chk0 & 0x0100) != 0) {
            chk0++;
        }
        uint8_t val = buffer[inIdx++];
        chk2 += val;
        if ((chk0 & 0x0100) != 0) {
            chk2++;
            chk0 &= 0xff;
        }
        part0[idx] = (uint8_t)(val ^ chk0);

        val = buffer[inIdx++];
        chk1 += val;
        if (chk2 > 0xff) {
            chk1++;
            chk2 &= 0xff;
        }
        part1[idx] = (uint8_t)(val ^ chk2);

        if (inIdx == 524) {
            break;
        }

        val = buffer[inIdx++];
        chk0 += val;
        if (chk1 > 0xff) {
            chk0++;
            chk1 &= 0xff;
        }
        part2[idx] = (uint8_t)(val ^ chk1);
        idx++;
    }
    part2[CHUNK_SIZE_62_524 - 1] = 0x00;    // gets merged into the "twos"
    assert(idx == CHUNK_SIZE_62_524 - 1);

    // Output the nibble data.
    for (int i = 0; i < CHUNK_SIZE_62_524; i++) {
        uint8_t twos = (uint8_t)(((part0[i] & 0xc0) >> 2) |
                                 ((part1[i] & 0xc0) >> 4) |
                                 ((part2[i] & 0xc0) >> 6));
        output[outIdx++] = sDiskBytes62[twos];
        output[outIdx++] = sDiskBytes62[part0[i] & 0x3f];
        output[outIdx++] = sDiskBytes62[part1[i] & 0x3f];
        if (i != CHUNK_SIZE_62_524 - 1) {
            output[outIdx++] = sDiskBytes62[part2[i] & 0x3f];
        }
    }

    // Output the checksum.
    uint8_t chktwos =
        (uint8_t)(((chk0 & 0xc0) >> 6) | ((chk1 & 0xc0) >> 4) | ((chk2 & 0xc0) >> 2));
    output[outIdx++] = sDiskBytes62[chktwos];
    output[outIdx++] = sDiskBytes62[chk2 & 0x3f];
    output[outIdx++] = sDiskBytes62[chk1 & 0x3f];
    output[outIdx++] = sDiskBytes62[chk0 & 0x3f];

    return true;
}

/// <summary>
/// Decodes 524 bytes of 6&2-encoded sector data.
/// </summary>
/// <param name="buffer">Buffer that will receive 524 bytes of decoded data.</param>
/// <param name="input">Input buffer of encoded bytes (703 entries).</param>
/// <returns>True on success.</returns>
bool Woz_Nibblizer_35::DecodeSector62_524(uint8_t* buffer, const uint8_t *input) {
    if (buffer == nullptr || input == nullptr) {
        return false;
    }

    uint8_t part0[CHUNK_SIZE_62_524];
    uint8_t part1[CHUNK_SIZE_62_524];
    uint8_t part2[CHUNK_SIZE_62_524];
    uint8_t twos, nib0, nib1, nib2;

    int inIdx = 0;

    // Assemble 8-bit bytes from 6&2 encoded values.
    for (int i = 0; i < CHUNK_SIZE_62_524; i++) {
        twos = sInvDiskBytes62[input[inIdx++]];
        nib0 = sInvDiskBytes62[input[inIdx++]];
        nib1 = sInvDiskBytes62[input[inIdx++]];
        nib2 = 0;
        if (i != CHUNK_SIZE_62_524 - 1) {
            nib2 = sInvDiskBytes62[input[inIdx++]];
        }
        if (twos == INVALID_INV || nib0 == INVALID_INV || nib1 == INVALID_INV ||
                nib2 == INVALID_INV) {
            return false;
        }

        part0[i] = (uint8_t)(nib0 | ((twos << 2) & 0xc0));
        part1[i] = (uint8_t)(nib1 | ((twos << 4) & 0xc0));
        part2[i] = (uint8_t)(nib2 | ((twos << 6) & 0xc0));
    }

    // Grab the checksum.
    twos = sInvDiskBytes62[input[inIdx++]];
    nib2 = sInvDiskBytes62[input[inIdx++]];
    nib1 = sInvDiskBytes62[input[inIdx++]];
    nib0 = sInvDiskBytes62[input[inIdx++]];
    if (twos == INVALID_INV || nib0 == INVALID_INV || nib1 == INVALID_INV ||
            nib2 == INVALID_INV) {
        return false;
    }
    uint8_t readChk2 = (uint8_t)(nib2 | ((twos << 2) & 0xc0));
    uint8_t readChk1 = (uint8_t)(nib1 | ((twos << 4) & 0xc0));
    uint8_t readChk0 = (uint8_t)(nib0 | ((twos << 6) & 0xc0));

    // Output data and compute the checksum.  This is a little nuts.
    uint32_t chk0 = 0, chk1 = 0, chk2 = 0;
    int outCount = 0;
    int index = 0;
    while (true) {
        chk0 = (chk0 & 0xff) << 1;
        if ((chk0 & 0x0100) != 0) {
            chk0++;
        }

        uint8_t val = (uint8_t)(part0[index] ^ chk0);
        chk2 += val;
        if ((chk0 & 0x0100) != 0) {
            chk2++;
            chk0 &= 0xff;
        }
        buffer[outCount++] = val;

        val = (uint8_t)(part1[index] ^ chk2);
        chk1 += val;
        if (chk2 > 0xff) {
            chk1++;
            chk2 &= 0xff;
        }
        buffer[outCount++] = val;

        if (outCount == 524)
            break;

        val = (uint8_t)(part2[index] ^ chk1);
        chk0 += val;
        if (chk1 > 0xff) {
            chk0++;
            chk1 &= 0xff;
        }
        buffer[outCount++] = val;

        index++;
        assert(index < CHUNK_SIZE_62_524);
    }

    if (readChk0 != (uint8_t)chk0 || readChk1 != (uint8_t)chk1 || readChk2 != (uint8_t)chk2) {
        return false;
    }

    return true;
}


void Woz_Nibblizer_35::emit_nibblized_sector(woz_track_t& trk, sector_ondisk_t& in) {
    sector_62_t nbuf;
    EncodeSector62_524(nbuf, in);
    for (int i = 0; i < 703; i++) {
        emit_data_byte(trk, nbuf[i]);
    }
}

void Woz_Nibblizer_35::emit_data_field(woz_track_t& trk, sector_ondisk_t& in, int sector_num) {
    // Data prologue
    emit_data_byte(trk, 0xD5);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xAD);
    uint8_t sector_number = sector_num;
    emit_data_byte(trk, sDiskBytes62[sector_number]);

    emit_nibblized_sector(trk, in);

    // Data epilogue
    emit_data_byte(trk, 0xDE);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xFF);
}

void Woz_Nibblizer_35::emit_address_field(woz_track_t& trk, int track_num, int side, int sector_num) {

    emit_data_byte(trk, 0xD5);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0x96);

/* 
+$024 / 3: address prolog ($d5 $aa $96)
+$027 / 1: 6&2enc low part of track number: 0-79 mod 64
+$028 / 1: 6&2enc sector number (0-11)
+$029 / 1: 6&2enc side number ($00 or $20) and high part of track number ($01 for tracks >= 64)
+$02a / 1: 6&2enc format ($22 or $24)
+$02b / 1: 6&2enc address checksum: (track ^ sector ^ side ^ format) & $3f
+$02c / 2: address epilog ($de $aa) */

    uint8_t low_part_of_track_number = track_num & 0x3F;
    uint8_t sector_number = sector_num;
    uint8_t side_number = (side == 0 ? 0 : 0x20) | ((track_num & 0b0100'0000) >> 6);
    uint8_t format = 0x24;
    uint8_t address_checksum = (low_part_of_track_number ^ sector_number ^ side_number ^ format) & 0x3f;
    printf("EAF >> track: %02X side: %02X sector: %02X | low_part_of_track_number: %02X sector_number: %02X side_number: %02X format: %02X address_checksum: %02X\n", track_num, side, sector_num, low_part_of_track_number, sector_number, side_number, format, address_checksum);
    emit_data_byte(trk, sDiskBytes62[low_part_of_track_number]);
    emit_data_byte(trk, sDiskBytes62[sector_number]);
    emit_data_byte(trk, sDiskBytes62[side_number]);
    emit_data_byte(trk, sDiskBytes62[format]);
    emit_data_byte(trk, sDiskBytes62[address_checksum]);
    emit_data_byte(trk, 0xDE);
    emit_data_byte(trk, 0xAA);
    emit_data_byte(trk, 0xFF);
}

void Woz_Nibblizer_35::emit_sector(woz_track_t& trk, sector_t& in,
                    int track_num, int side, int sector_num) {
    sector_ondisk_t ondisk;
    memcpy(ondisk+12, in, 512);
    memset(ondisk, 0, 12);

    for (int i = 0; i < 524; i++) {
        printf("%02X ", ondisk[i]);
        if (i % 16 == 15) {
            printf("\n");
        }
    }
    printf("\n");
    emit_address_field(trk, track_num, side, sector_num);
    emit_sync_bytes(trk, GAP_B_SIZE);
    emit_data_field(trk, ondisk, sector_num);
    emit_sync_bytes(trk, GAP_C_SIZE);
}

woz_track_t Woz_Nibblizer_35::build_track(disk_image_t& disk_image,
                               const interleave_t& phys_to_logical,
                               int track_num, int side) {
    woz_track_t trk;

    // Gap A: self-sync bytes at the start of the track
    emit_sync_bytes(trk, GAP_A_SIZE);
    // Sectors in physical order
    int sectors = sectorsPerZone[track_num / 16];
    for (int s = 0; s < sectors; s++) {
        int logical = phys_to_logical[s];
        int index = calculateSectorOffset(track_num, logical, side);
        printf("bt >> track %d side %d sector %d blk index: %d\n", track_num, side, s, index);
        emit_sector(trk,
                    disk_image.sectors[index],
                    track_num,
                    side,
                    logical);
    }
    return trk;
}

// ─── Import from media ────────────────────────────────────────────────────────

int Woz_Nibblizer_35::load_disk_image(const media_descriptor *media, disk_image_t& disk_image) {

    FILE *fp = fopen(media->filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Could not open " << media->filename << std::endl;
        return -1;
    }

    int sect_index = 0;
    fseek(fp, media->data_offset, SEEK_SET);
    if (fread((void *)&disk_image, SECTORS_PER_DISK, SECTOR_SIZE, fp) != SECTOR_SIZE) {
        std::cerr << "Could not read " << SECTORS_PER_DISK * SECTOR_SIZE << " bytes from " << media->filename << std::endl;
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int Woz_Nibblizer_35::import_block_image(Woz& woz, const media_descriptor* media) {
    if (!media) return -1;

    if (media->media_type != MEDIA_BLK) {
        std::cerr << "WOZ: import only supports MEDIA_BLK sources "
                     "(3.5 800K floppy images)\n";
        return -1;
    }

    disk_image_t disk_image;
    if (load_disk_image(media, disk_image) != 0) {
        std::cerr << "WOZ: failed to load disk image from '" << media->filename << "'\n";
        return -1;
    }

    // Select interleave table.
    //const interleave_t* phys_to_logical = nullptr;

    // no interleave assumption for 3.5
    
    woz_image_t &m_image = woz.image();

    // Reset the in-memory image.
    std::fill(std::begin(m_image.tmap), std::end(m_image.tmap), 0xFF);
    m_image.tracks.clear();
    m_image.tracks.reserve(TRACKS_PER_DISK);

    // Populate INFO fields appropriate for an imported disk.
    m_image.info.disk_type          = 2;   // 3.5"
    m_image.info.write_protected    = media->write_protected ? 1 : 0;
    m_image.info.synchronized       = 0;
    m_image.info.cleaned            = 1;
    m_image.info.optimal_bit_timing = 16;  // 2 µs
    m_image.info.boot_sector_format = 0;   
    m_image.info.disk_sides         = 2;

    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        uint8_t trk_idx = t * 2;
        int zone = t / 16;
        // in Woz It's T0S0, T0S1, T1S0, T1S1, etc.
        m_image.tracks.push_back(build_track(disk_image, i_logical_to_phys[zone], t, 0));
        m_image.tmap[trk_idx] = trk_idx;  // track side 0
        
        m_image.tracks.push_back(build_track(disk_image, i_logical_to_phys[zone], t, 1));
        m_image.tmap[trk_idx+1] = trk_idx+1;  // track side 1
    }

    return 0;
}


// ─── Export to media (WOZ bit-stream → raw block image) ─────────────────────
#if 0
// Reverses Woz's POSTNB16 routine, mirror of decode_sector_62() in diskii_fmt.cpp.
void Woz_Nibblizer_35::decode_sector_62(sector_62_t& nbuf, sector_t& decoded) {
    uint8_t y = 0;
    int8_t  x;
    uint8_t c;
    uint8_t* nbuf1 = nbuf;
    uint8_t* nbuf2 = nbuf + 0x100;

post1:
    x = 0x56;
post2:
    x--;
    if (x < 0) goto post1;

    {
        uint8_t a = nbuf1[y];
        c = nbuf2[x] & 1; nbuf2[x] >>= 1; a = (a << 1) | c;
        c = nbuf2[x] & 1; nbuf2[x] >>= 1; a = (a << 1) | c;
        decoded[y] = a;
    }
    y++;
    if (y != 0x00) goto post2;
}


int Woz_Nibblizer_35::decode_track(const woz_track_t *trk, int track_num,
                    const interleave_t& phys_to_logical,
                    disk_image_t *out) {
    if (trk->bit_count == 0) return 0;

    BitCursor c{trk, 0, 0};

    // Bound the scan to two full revolutions so a sector that straddles the
    // wrap-point is still recoverable, and so a corrupt track can't loop us
    // forever. The factor of 2 mirrors max_iterations in denibblize_disk_image.
    const uint64_t max_bits = static_cast<uint64_t>(trk->bit_count) * 2;

    bool found[SECTORS_PER_TRACK] = {false};
    int  found_count = 0;

    uint8_t n0 = 0, n1 = 0, n2 = 0;

    while (found_count < SECTORS_PER_TRACK && c.consumed < max_bits) {
        n2 = n1; n1 = n0; n0 = read_nibble(c);
        if (!(n2 == 0xD5 && n1 == 0xAA && n0 == 0x96)) continue;

        // Address field: 8 4&4-encoded nibbles → vol, track, sector, checksum.
        uint8_t e[8];
        for (int i = 0; i < 8; i++) e[i] = read_nibble(c);
        uint8_t volume    = static_cast<uint8_t>(((e[0] & 0x55) << 1) | (e[1] & 0x55));
        uint8_t trk_no    = static_cast<uint8_t>(((e[2] & 0x55) << 1) | (e[3] & 0x55));
        uint8_t sector    = static_cast<uint8_t>(((e[4] & 0x55) << 1) | (e[5] & 0x55));
        uint8_t checksum  = static_cast<uint8_t>(((e[6] & 0x55) << 1) | (e[7] & 0x55));

        if (checksum != static_cast<uint8_t>(volume ^ trk_no ^ sector)) {
            n0 = n1 = n2 = 0;
            continue;
        }
        if (sector >= SECTORS_PER_TRACK) {
            n0 = n1 = n2 = 0;
            continue;
        }

        // Skip the 3-nibble address epilogue (DE AA EB) — values not checked.
        (void)read_nibble(c);
        (void)read_nibble(c);
        (void)read_nibble(c);

        // Hunt for the data prologue (D5 AA AD), bounded so we don't run away.
        uint8_t d0 = 0, d1 = 0, d2 = 0;
        const uint64_t hunt_limit = c.consumed + 64 * 8; // ~64 nibbles
        bool got_prologue = false;
        while (c.consumed < hunt_limit) {
            d2 = d1; d1 = d0; d0 = read_nibble(c);
            if (d2 == 0xD5 && d1 == 0xAA && d0 == 0xAD) { got_prologue = true; break; }
        }
        if (!got_prologue) {
            n0 = n1 = n2 = 0;
            continue;
        }

        // Read the 342 nibbles of 6&2-encoded payload (XOR-chain).
        sector_62_t nbuf;
        uint8_t csum = 0;
        for (int i = 0x155; i >= 0x100; i--) {
            nbuf[i] = static_cast<uint8_t>(csum ^ woz_denibble_table[read_nibble(c)]);
            csum = nbuf[i];
        }
        for (int i = 0; i < 0x100; i++) {
            nbuf[i] = static_cast<uint8_t>(csum ^ woz_denibble_table[read_nibble(c)]);
            csum = nbuf[i];
        }
        // Trailing checksum nibble — verifies that the running csum is zero
        // after XORing with the table-decoded checksum nibble.
        uint8_t tail = woz_denibble_table[read_nibble(c)];
        if (static_cast<uint8_t>(csum ^ tail) != 0) {
            // Bad checksum: drop this sector, try the next prologue.
            n0 = n1 = n2 = 0;
            continue;
        }

        sector_t decoded;

        decode_sector_62(nbuf, decoded);
        int logical = phys_to_logical[sector];
        std::memcpy(out->sectors[track_num][logical], decoded, SECTOR_SIZE);

        if (!found[sector]) { found[sector] = true; ++found_count; }
        n0 = n1 = n2 = 0;
    }

    return found_count;
}


/**
 * when handling writes, we will need to decode the nibbles back to bytes.
 * particularly, the address field, so then we know which disk_image track and sector
 * to write the data back to.
 */

int Woz_Nibblizer_35::write_disk_image_po_do(const media_descriptor *media, const disk_image_t *disk_image) {
    FILE *out_fp = fopen(media->filename.c_str(), "r+b");
    if (!out_fp) {
        std::cerr << "Could not open " << media->filename << " for writing" << std::endl;
        return false;
    }
    fseek(out_fp, media->data_offset, SEEK_SET);
    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        fwrite(disk_image->sectors[t], sizeof(sector_t), SECTORS_PER_TRACK, out_fp);
    }

    fclose(out_fp);
    return true;
}
#endif

int Woz_Nibblizer_35::export_block_image(const Woz& woz, const media_descriptor* media) {
#if 0
    disk_image_t *out = new disk_image_t;

    const interleave_t* phys_to_logical = nullptr;
    if (media->interleave == INTERLEAVE_PO) {
        phys_to_logical = &po_phys_to_logical;
    } else if (media->interleave == INTERLEAVE_DO) {
        phys_to_logical = &do_phys_to_logical;
    } else {
        std::cerr << "WOZ: export_to_disk_image: unsupported interleave "
                  << media->interleave << "\n";
        return -1;
    }

    // Start from a clean slate so any sector we fail to decode is left zeroed
    // rather than carrying whatever the caller's stack had.
    std::memset(out, 0, sizeof(disk_image_t));

    int total_decoded = 0;
    int tracks_with_full_data = 0;

    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        /* uint8_t idx = m_image.tmap[t * 4];
        if (idx == 0xFF || idx >= m_image.tracks.size()) {
            std::cerr << "WOZ: export_to_disk_image: track " << t
                      << " missing from TMAP\n";
            continue;
        } */
        
        const woz_track_t *trk_ptr = woz.get_track_ptr(t*4);
        if (trk_ptr == nullptr) {
            std::cerr << "WOZ: export_to_disk_image: track " << t
                      << " missing from TMAP\n";
            continue;
        }
        //const woz_track_t& trk = m_image.tracks[idx];
        int got = decode_track(trk_ptr, t, *phys_to_logical, out);
        total_decoded += got;
        if (got == SECTORS_PER_TRACK) {
            ++tracks_with_full_data;
        } else {
            std::cerr << "WOZ: export_to_disk_image: track " << t
                      << " decoded " << got << "/" << SECTORS_PER_TRACK
                      << " sectors\n";
        }
    }

    int status = (tracks_with_full_data == TRACKS_PER_DISK) ? 0 : -1;

    if (status < 0) {
        fprintf(stderr,
                "Floppy_woz: WOZ->block export had decode errors for '%s'; "
                "writing partial result\n",
                media->filename.c_str());
        // Fall through and write whatever was recovered, matching
        // Floppy525::writeback()'s always-write behaviour.
    }
    if (write_disk_image_po_do(media, out) != 0) {
        fprintf(stderr, "Floppy_woz: block writeback failed for '%s'\n",
                media->filename.c_str());
        return -1;
    }

    return status;
#endif
}
#include <cstdio>
#include <iostream>
#include "woz_nibblizer_525.hpp"


// Convert a 256-byte sector into the 342-byte GCR nibble buffer used by the
// 6&2 encoder.  nbuf[0x00..0xFF] = NBUF1, nbuf[0x100..0x155] = NBUF2.
void Woz_Nibblizer_525::prenibble(sector_t& buf, sector_62_t& nbuf) {
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
void Woz_Nibblizer_525::emit_nibblized_sector(woz_track_t& trk, sector_t& in) {
    sector_62_t nbuf;
    prenibble(in, nbuf);

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

void Woz_Nibblizer_525::emit_data_field(woz_track_t& trk, sector_t& in) {
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

void Woz_Nibblizer_525::emit_sector(woz_track_t& trk, sector_t& in,
                       uint8_t volume, uint8_t track_num, uint8_t sector_num) {
    emit_address_field(trk, volume, track_num, sector_num);
    emit_sync_bytes(trk, GAP_B_SIZE);
    emit_data_field(trk, in);
    emit_sync_bytes(trk, GAP_C_SIZE);
}

woz_track_t Woz_Nibblizer_525::build_track(disk_image_t& disk_image,
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
woz_track_t Woz_Nibblizer_525::build_track_from_nib(const uint8_t* nib_data, uint32_t nib_size) {
    woz_track_t trk;
    for (uint32_t i = 0; i < nib_size; i++) {
        if (nib_data[i] == 0xFF)
            emit_sync_byte(trk);            // 10 bits: 1111111100
        else
            emit_data_byte(trk, nib_data[i]);  // 8 bits, MSB first
    }
    return trk;
}

// ─── Import from media ────────────────────────────────────────────────────────


int Woz_Nibblizer_525::load_nib_image(nibblized_disk_t& disk, const std::string& filename) {
    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Could not open " << filename << std::endl;
        return -1;
    }

    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        fread(disk.tracks[t].data, 1, TRACK_SIZE, fp);
        disk.tracks[t].position = 0;
        disk.tracks[t].size = TRACK_SIZE; // TODO: maybe determine how many bytes actually used in .nib.
    }

    fclose(fp);
    return 0;
}

int Woz_Nibblizer_525::import_from_nib(Woz& woz, const media_descriptor* media) {
    nibblized_disk_t nib;
    if (load_nib_image(nib, media->filename) != 0) {
        std::cerr << "WOZ: failed to load .nib image from '" << media->filename << "'\n";
        return -1;
    }
    auto m_image = woz.image();
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


int Woz_Nibblizer_525::load_disk_image(const media_descriptor *media, disk_image_t& disk_image) {

    FILE *fp = fopen(media->filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Could not open " << media->filename << std::endl;
        return -1;
    }

    int sect_index = 0;
    fseek(fp, media->data_offset, SEEK_SET);
    for (int t = 0; t < TRACKS_PER_DISK; t++) {
        for (int s = 0; s < SECTORS_PER_TRACK; s++) {
            if (fread(disk_image.sectors[t][s], 1, SECTOR_SIZE, fp) != SECTOR_SIZE) {
                std::cerr << "Could not read " << SECTOR_SIZE << " bytes from " << media->filename << std::endl;
                fclose(fp);
                return -1;
            }
        }
    }

    fclose(fp);
    return 0;
}

int Woz_Nibblizer_525::import_block_image(Woz& woz, const media_descriptor* media) {
    if (!media) return -1;

    if (media->media_type == MEDIA_PRENYBBLE) {
        return import_from_nib(woz, media);
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
    woz_image_t &m_image = woz.image();

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


// ─── Export to media (WOZ bit-stream → raw block image) ─────────────────────

// Reverses Woz's POSTNB16 routine, mirror of decode_sector_62() in diskii_fmt.cpp.
void Woz_Nibblizer_525::decode_sector_62(sector_62_t& nbuf, sector_t& decoded) {
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


int Woz_Nibblizer_525::decode_track(const woz_track_t *trk, int track_num,
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

int Woz_Nibblizer_525::write_disk_image_po_do(const media_descriptor *media, const disk_image_t *disk_image) {
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


int Woz_Nibblizer_525::export_block_image(const Woz& woz, const media_descriptor* media) {
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
}
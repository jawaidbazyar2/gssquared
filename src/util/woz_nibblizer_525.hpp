#pragma once

#include "woz.hpp"
#include "woz_nibblizer.hpp"



class Woz_Nibblizer_525 : public Woz_Nibblizer {

// 5.25 structs
static constexpr int TRACKS_PER_DISK = 35;
static constexpr int SECTORS_PER_TRACK = 16;
static constexpr int SECTOR_SIZE = 0x0100;
static constexpr int TRACK_SIZE = 0x1A00; // https://retrocomputing.stackexchange.com/questions/27691/apple-nibble-disk-format-specification
static constexpr int TRACK_MAX_DATA = 0x18D0; // Nelson

static constexpr int GAP_A_SIZE = 64; // was 149
static constexpr int GAP_B_SIZE = 5; // This is what DOS33 uses. Seems to work for ProDOS too.
static constexpr int GAP_C_SIZE = 21;

// one sector encoded (nibblized) data without checksum.
typedef uint8_t sector_62_t[342];

// one sector encoded (nibblized) data with checksum. Only used
// for debugging.
typedef uint8_t sector_62_ondisk_t[343];

// nibblized Track data structure
typedef struct track_s {
    uint16_t size = 0;
    uint16_t position = 0;
    uint8_t data[TRACK_SIZE];
} track_t;

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

// Nibblized Disk data structure
typedef struct {
    interleave_t interleave_phys_to_logical;
    interleave_t interleave_logical_to_phys;
    track_t tracks[35]; // They are numbered from 0 to 34, track 0 being the
                        // outer most track and track 34 being the inner most.
} nibblized_disk_t;

// one sector unencoded data

typedef uint8_t sector_t[SECTOR_SIZE];

typedef struct {
    sector_t sectors[TRACKS_PER_DISK][SECTORS_PER_TRACK];
} disk_image_t;

protected:

    // helper tables for 5.25 sector interleave orders
    static constexpr interleave_t po_phys_to_logical = {   // also Pascal Order.
        0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
    };

    static constexpr interleave_t po_logical_to_phys = {   // also Pascal Order.
        0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
    };

    static constexpr interleave_t do_phys_to_logical = {
        0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
    };

    static constexpr interleave_t do_logical_to_phys = {
        0x0, 0xD, 0xB, 0x9, 0x7, 0x5, 0x3, 0x1, 0xE, 0xC, 0xA, 0x8, 0x6, 0x4, 0x2, 0xF
    };

    static constexpr interleave_t cpm_order = {  // to be complete.
        0, 10, 6, 1, 12, 7, 2, 13, 8, 3, 14, 9, 4, 15, 10, 5
    };


// ─── Local 6&2 encoding helpers ──────────────────────────────────────────────
// These are self-contained copies of translate_62 / prenibble from
// diskii_fmt.cpp so that gs2_woz has no link-time dependency on that module.

    static constexpr uint8_t woz_translate_62[64] = {
        0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
        0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
        0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    };

// Self-contained mirrors of denibble_table[] and decode_sector_62() from
// diskii_fmt.cpp, kept here so gs2_woz has no link-time dependency on that
// module (matching the comment near woz_translate_62 above).

    static constexpr uint8_t woz_denibble_table[256] = {
        //   0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x00
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x10
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x20
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x30
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x40
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x50
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x60
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x70
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x80
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x03, 0x00, 0x04, 0x05, 0x06, // 0x90
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x08, 0x00, 0x00, 0x00, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, // 0xA0
        0x00, 0x00, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x00, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, // 0xB0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x1C, 0x1D, 0x1E, // 0xC0
        0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x20, 0x21, 0x00, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 0xD0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x2A, 0x2B, 0x00, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, // 0xE0
        0x00, 0x00, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x00, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, // 0xF0
    };
    

    void prenibble(sector_t& buf, sector_62_t& nbuf);
    void emit_nibblized_sector(woz_track_t& trk, sector_t& in);
    void emit_address_field(woz_track_t& trk, uint8_t volume, uint8_t track_num, uint8_t sector_num);
    void emit_data_field(woz_track_t& trk, sector_t& in);
    void emit_sector(woz_track_t& trk, sector_t& in,
                     uint8_t volume, uint8_t track_num, uint8_t sector_num);
    woz_track_t build_track(disk_image_t& disk_image,
                            const interleave_t& phys_to_logical,
                            int track_num, uint8_t volume);
    woz_track_t build_track_from_nib(const uint8_t* nib_data, uint32_t nib_size);
    void decode_sector_62(sector_62_t& nbuf, sector_t& decoded);
    int decode_track(const woz_track_t *trk, int track_num,
                      const interleave_t& phys_to_logical,
                      disk_image_t *out);
    int load_nib_image(nibblized_disk_t& disk, const std::string& filename);
    int import_from_nib(Woz& woz, const media_descriptor* media);

    int load_disk_image(const media_descriptor *media, disk_image_t& disk_image);
    int write_disk_image_po_do(const media_descriptor *media, const disk_image_t *disk_image);

public:
    /* Woz_Nibblizer_525()  {};
    ~Woz_Nibblizer_525() {} ; */
    virtual int import_block_image(Woz& woz, const media_descriptor* media) override;
    virtual int export_block_image(const Woz& woz, const media_descriptor* media) override;
};


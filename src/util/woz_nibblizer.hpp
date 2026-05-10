#pragma once

#include "woz.hpp"
#include "media.hpp"

class Woz_Nibblizer {
  
    public:
        virtual ~Woz_Nibblizer() = default;
        // Populate `woz` from a block/nib image described by `media`.
        // `woz` is reset to a clean state before importing.
        // Returns 0 on success, -1 on error.
        virtual int import_block_image(Woz& woz, const media_descriptor* media) = 0;
        // Decode `woz` back to a raw block image on disk.
        // `media` supplies filename, interleave, and target format.
        // Returns 0 on success, -1 on error.
        virtual int export_block_image(const Woz& woz, const media_descriptor* media) = 0;
    protected:
        static void emit_bit(woz_track_t& trk, int bit);
        static void emit_sync_byte(woz_track_t& trk);
        static void emit_data_byte(woz_track_t& trk, uint8_t byte);
        static void emit_sync_bytes(woz_track_t& trk, int n);
        static void emit_encoded_44(woz_track_t& trk, uint8_t value);
        static void emit_address_field(woz_track_t& trk, uint8_t volume, uint8_t track_num, uint8_t sector_num);

        // LSS-style bit cursor over a WOZ track. The bit position is monotonically
        // incrementing and wrapped modulo trk->bit_count on read, so callers can
        // safely walk past the revolution boundary; `consumed` is the total number
        // of bits read since the cursor was created (used to bound scans).
        struct BitCursor {
            const woz_track_t* trk;
            uint32_t           pos;
            uint64_t           consumed;

            int read_bit() {
                uint32_t i = pos++ % trk->bit_count;
                consumed++;
                return (trk->bits[i >> 3] >> (7 - (i & 7))) & 1;
            }
        };
        // Latches one complete nibble using the bit-7-set technique from the Disk II
        // LSS: shift bits into a register; once the high bit is set we have a full
        // nibble. Naturally absorbs 0xFF sync bytes (8 ones latch a 0xFF; the two
        // trailing zeros leave the shifter empty, ready for the next nibble's
        // leading 1) so 8-bit data and 10-bit sync are both handled transparently.
        static uint8_t read_nibble(BitCursor& c) {
            uint8_t shifter = 0;
            while (true) {
                shifter = static_cast<uint8_t>((shifter << 1) | c.read_bit());
                if (shifter & 0x80) return shifter;
            }
        }
    };
    
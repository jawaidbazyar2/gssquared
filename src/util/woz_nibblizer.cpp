#include "woz_nibblizer.hpp"

// ─── Bit-stream primitives ────────────────────────────────────────────────────

void Woz_Nibblizer::emit_bit(woz_track_t& trk, int bit) {
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
void Woz_Nibblizer::emit_sync_byte(woz_track_t& trk) {
    for (int i = 0; i < 8; i++) emit_bit(trk, 1);
    emit_bit(trk, 0);
    emit_bit(trk, 0);
}

void Woz_Nibblizer::emit_data_byte(woz_track_t& trk, uint8_t byte) {
    for (int i = 7; i >= 0; i--)
        emit_bit(trk, (byte >> i) & 1);
}

void Woz_Nibblizer::emit_sync_bytes(woz_track_t& trk, int n) {
    for (int i = 0; i < n; i++) emit_sync_byte(trk);
}


// ─── Track-encoding helpers ───────────────────────────────────────────────────

void Woz_Nibblizer::emit_encoded_44(woz_track_t& trk, uint8_t value) {
    uint8_t xx = ((value & 0b10101010) >> 1) | 0b10101010;
    uint8_t yy =  (value & 0b01010101)       | 0b10101010;
    emit_data_byte(trk, xx);
    emit_data_byte(trk, yy);
}
/* 
void Woz_Nibblizer::emit_address_field(woz_track_t& trk,
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
 */
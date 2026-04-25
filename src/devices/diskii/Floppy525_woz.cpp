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

#include <iostream>
#include <cstdint>
#include <cstdio>
#include "util/printf_helper.hpp"
#include "Floppy525_woz.hpp"
#include "devices/diskii/diskii.hpp"
#include "util/SoundEffectKeys.hpp"
#include "debug.hpp"

// ─── Head position helpers ────────────────────────────────────────────────────

void Floppy525_woz::set_track(int track_num) {
    track = track_num;
}

void Floppy525_woz::move_head(int direction) {
    track += direction;
}

// ─── Bit-stream helpers ───────────────────────────────────────────────────────

void Floppy525_woz::update_track_ptr() {
    // `track` is a half-track index (same as Floppy525).  WOZ TMAP is indexed
    // by quarter-track, so multiply by 2: half-track N → quarter-track N*2.
    // (Full track T = half-track T*2 = quarter-track T*4.)
    cur_track_ptr = woz.get_track(track * 2);

    // Clear LSS state on track change so bits from the old track don't
    // contaminate the first nibble on the new track.
    lss_shift = 0;
    read_shift_register = 0;

    // Re-wrap bit_fp for the new track's revolution length so start_bit
    // is always in-range.  The disk keeps spinning so the angular position
    // is preserved; only the modulus changes.
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        bit_fp = bit_fp % (cur_track_ptr->bit_count * 8);
    }

}

void Floppy525_woz::fast_forward() {
    uint64_t now     = clock->get_cycles();
    uint64_t elapsed = now - last_cycle;
    last_cycle = now;

    if (!enable || !cur_track_ptr || cur_track_ptr->bit_count == 0) {
        // Motor off or no track: still update bit_fp so position is consistent
        // when the track becomes available, but don't shift any bits.
        return;
    }

    uint64_t track_bits = cur_track_ptr->bit_count;

    // Compute new physical position BEFORE updating bit_fp so we can derive
    // the exact number of whole bit-cells to simulate from the fractional
    // position already accumulated in bit_fp.
    //
    // Using (new_fp >> 3) - (bit_fp >> 3) instead of elapsed/4 is critical:
    // bit_fp may be mid-cell (e.g. bit_fp=30 = bit 3 + 6/8 of a cell elapsed).
    // 7 elapsed cycles (14 units) from that position completes cells 3 AND 4,
    // but elapsed/4=1 would only simulate 1, permanently skipping bits.
    uint64_t new_bit_fp    = bit_fp + elapsed * 2;
    uint64_t bits_to_sim   = ((new_bit_fp >> 3) - (bit_fp >> 3)) % track_bits;
    uint64_t start_bit     = (bit_fp >> 3) % track_bits;

    for (uint64_t i = 0; i < bits_to_sim; i++) {
        uint64_t bi  = (start_bit + i) % track_bits;
        uint8_t  bit = (cur_track_ptr->bits[bi / 8] >> (7 - (bi % 8))) & 1;
        lss_shift = (lss_shift << 1) | bit;
        if (lss_shift & 0x80) {
            // LSS latch fires: bit 7 went high.  Capture nibble and reset accumulator
            // so the next nibble always accumulates from 0 — this is what makes every
            // valid nibble produce its exact byte value (D5→0xD5, AA→0xAA, 96→0x96).
            read_shift_register = lss_shift;
            lss_shift = 0;
        }
    }

    // Advance and wrap physical position (1/8-bit-cell units).
    bit_fp = new_bit_fp % (track_bits * 8);
}

// ─── Nybble I/O ───────────────────────────────────────────────────────────────

uint8_t Floppy525_woz::read_nybble() {
    if (!enable) {
        // Motor off: return the last latched value without advancing.
        return read_shift_register;
    }

    fast_forward();

    if (!is_mounted || !cur_track_ptr || cur_track_ptr->bit_count == 0) {
        // No track data: shift a 1-bit through the LSS accumulator so the
        // controller sees noise (eventually latches 0xFF as a sync-like byte).
        lss_shift = (lss_shift << 1) | 1;
        if (lss_shift & 0x80) {
            read_shift_register = lss_shift;
            lss_shift = 0;
        }
    }

    // Consume the latch: the Apple II LSS data register is cleared after each
    // CPU read of Q6L.  This ensures the BPL loop for each prologue byte
    // (AA, 96) actually waits for the next nibble to accumulate rather than
    // immediately exiting on the previously latched value.
    uint8_t val = read_shift_register;
    read_shift_register = 0;
    return val;
}

void Floppy525_woz::write_nybble(uint8_t /* nybble */) {
    // WOZ write-back requires bit-level splicing into the bitstream.
    // Deferred: stub only.
    modified = true;
}

// ─── Mount / unmount / writeback ─────────────────────────────────────────────

bool Floppy525_woz::mount(uint64_t key, media_descriptor *media_in) {
    if (is_mounted) {
        unmount(key);
    }

    int rc = (media_in->media_type == MEDIA_WOZ)
                 ? woz.load(media_in->filename)
                 : woz.import_from_media(media_in);

    if (rc != 0) {
        fprintf(stderr, "Floppy525_woz: failed to load/import '%s'\n",
                media_in->filename.c_str());
        return false;
    }

    bit_fp = 0;
    last_cycle = clock->get_cycles();
    lss_shift = 0;
    read_shift_register = 0;
    write_shift_register = 0;
    //track               = 0; // don't reset track. however, we need point to the current track.
    cur_track_ptr = woz.get_track(track*2);

    update_track_ptr();

    write_protect = media_in->write_protected;
    is_mounted    = true;
    media_d       = media_in;
    modified      = false;

    play_sound(SE_SHUGART_CLOSE);

    std::cout << "Floppy525_woz: mounted " << media_in->filestub << std::endl;
    return true;
}

bool Floppy525_woz::unmount(uint64_t key) {
    // Reset Woz image to a clean blank state
    woz = Woz{};
    cur_track_ptr = nullptr;
    bit_fp        = 0;
    last_cycle    = 0;
    lss_shift            = 0;
    read_shift_register  = 0;
    write_shift_register = 0;

    is_mounted = false;
    media_d    = nullptr;
    modified   = false;

    play_sound(SE_SHUGART_OPEN);
    return true;
}

bool Floppy525_woz::writeback() {
    if (!media_d) return false;

    // For WOZ files save in-place; for imported block images write back as WOZ, but change the file extension to .woz.
    std::string filename = media_d->filename;
    std::cout << "Floppy525_woz: writing back disk image" << media_d->filename << "  " << media_d->media_type << std::endl;
    if (media_d->media_type == MEDIA_NYBBLE) { // poorly named
        std::cout << "Floppy525_woz: writing back WOZ disk image" << std::endl;
        filename = filename.substr(0, filename.find_last_of('.')) + ".woz";
        // woz.export
    } else {
        std::cout << "Floppy525_woz: writing back block disk image" << std::endl;
        // TODO: write back the block disk image.
        int rc = woz.save(filename);
        if (rc != 0) {
            fprintf(stderr, "Floppy525_woz: writeback failed for '%s'\n",
                    media_d->filename.c_str());
            return false;
        }   
    }

    modified = false;
    return true;
}

// ─── Status / reset ───────────────────────────────────────────────────────────

drive_status_t Floppy525_woz::status() {
    if (is_mounted)
        return {is_mounted, media_d->filestub, enable, track<<1, modified,
                media_d->write_protected};
    return {is_mounted, "", enable, track, modified, false};
}

void Floppy525_woz::reset() {
    enable = false;
}

// ─── Controller register dispatch ────────────────────────────────────────────

uint8_t Floppy525_woz::read_cmd(uint16_t address) {
    uint16_t reg = address & 0x0F;

    if (enable && mark_cycles_turnoff != 0 &&
        (clock->get_c14m() > mark_cycles_turnoff)) {
        if (DEBUG(DEBUG_DISKII))
            printf("motor off: %llu %llu cycles\n",
                   u64_t(clock->get_c14m()), u64_t(mark_cycles_turnoff));
        enable = false;
        mark_cycles_turnoff = 0;
    }

    int8_t cur_track = track;
    int8_t cur_phase = cur_track % 4;

    switch (reg) {
        case DiskII_Ph0_Off:
            phase0 = 0;
            break;
        case DiskII_Ph0_On:
            if (cur_phase == 1) track--;
            else if (cur_phase == 3) track++;
            phase0 = 1;
            last_phase_on = 0;
            break;
        case DiskII_Ph1_Off:
            phase1 = 0;
            break;
        case DiskII_Ph1_On:
            if (cur_phase == 2) track--;
            else if (cur_phase == 0) track++;
            phase1 = 1;
            last_phase_on = 1;
            break;
        case DiskII_Ph2_Off:
            phase2 = 0;
            break;
        case DiskII_Ph2_On:
            if (cur_phase == 3) track--;
            else if (cur_phase == 1) track++;
            phase2 = 1;
            last_phase_on = 2;
            break;
        case DiskII_Ph3_Off:
            phase3 = 0;
            break;
        case DiskII_Ph3_On:
            if (cur_phase == 0) track--;
            else if (cur_phase == 2) track++;
            phase3 = 1;
            last_phase_on = 3;
            break;

        case DiskII_Q6L:
            /**
            * when Q6=0 and Q7=0, then cycle another bit read of a nybble from the disk
            */
            /**
            * when Q6L is read, and Q7H was previously set (written) then we need to write the byte to the disk.
            */    
            Q6 = 0;
            if (Q7 == 1 || Q6 == 1) {
                write_nybble(write_shift_register);
            }
            break;
        case DiskII_Q6H:
            Q6 = 1;
            break;
        case DiskII_Q7L:
            Q7 = 0;
            if (Q6 == 1) { // Q6H then Q7L is a write protect sense.
                uint8_t xwp = write_protect << 7;
                //printf("wp: Q7: %d, Q6: %d, wp: %d %02X\n", seldrive.Q7, seldrive.Q6, seldrive.write_protect, xwp);
                return xwp; // write protect sense. Return hi bit set (write protected)
            }
            break;
        case DiskII_Q7H:
            Q7 = 1;
            break;
    }

    if (track < 0) track = 0;
    if (track > 68) track = 68;

    if (track != cur_track) {
        update_track_ptr();
    }

    return 0;
}

void Floppy525_woz::write_cmd(uint16_t address, uint8_t data) {
    uint16_t reg = address & 0x0F;

    // store the value being written into the write_shift_register. It will be stored in the disk image when Q6L is tweaked in read.
    switch (reg) {
        case DiskII_Q6H:
            write_shift_register = data;
            Q6 = 1;
            break;
        case DiskII_Q7H:
            write_shift_register = data;
            Q7 = 1;
            break;
    }
}

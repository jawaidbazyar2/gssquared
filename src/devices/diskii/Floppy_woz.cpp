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

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>

#include "Floppy_woz.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "util/SoundEffectKeys.hpp"

// ───────────────────────────── mount/unmount/writeback ─────────────────────────

bool Floppy_woz::mount(uint64_t key, media_descriptor *media_in) {
    if (is_mounted) {
        unmount(key);
    }

    int rc = (media_in->media_type == MEDIA_WOZ)
                 ? woz.load(media_in->filename)
                 : woz.import_from_media(media_in);

    if (rc != 0) {
        fprintf(stderr, "Floppy_woz: failed to load/import '%s'\n",
                media_in->filename.c_str());
        return false;
    }

    read_position = 0;
    head_position = 0;
    last_cycle    = get_current_time();
    // Force update_track_ptr() to treat this as a fresh track (old_bits == 0
    // skips the angular-rescale path so we start at position 0).
    cur_track_ptr = nullptr;

    update_track_ptr();

    write_protect = media_in->write_protected;
    is_mounted    = true;
    media_d       = media_in;
    modified      = false;

    play_sound(SE_SHUGART_CLOSE);

    std::cout << "Floppy_woz: mounted " << media_in->filestub << std::endl;
    return true;
}

bool Floppy_woz::unmount(uint64_t key) {
    (void)key;
    // Reset WOZ image to a clean blank state.
    woz = Woz{};
    cur_track_ptr = nullptr;

    read_position = 0;
    head_position = 0;
    last_cycle    = 0;

    is_mounted = false;
    media_d    = nullptr;
    modified   = false;

    play_sound(SE_SHUGART_OPEN);
    return true;
}

bool Floppy_woz::writeback() {
    if (!media_d) return false;

    // MEDIA_NYBBLE means the mount source was a 143K block image
    // (.do/.po/.dsk): decode the in-memory WOZ bit stream back into a raw
    // disk_image_t and rewrite the file in place. For native WOZ images
    // just save as WOZ2.
    std::cout << "Floppy_woz: writing back disk image " << media_d->filename
              << " (media_type=" << media_d->media_type << ")" << std::endl;

    if (media_d->media_type == MEDIA_NYBBLE) {
        std::cout << "Floppy_woz: writing back block disk image" << std::endl;
        disk_image_t out{};
        if (woz.export_to_disk_image(out, media_d->interleave) != 0) {
            fprintf(stderr,
                    "Floppy_woz: WOZ->block export had decode errors for '%s'; "
                    "writing partial result\n",
                    media_d->filename.c_str());
            // Fall through and write whatever was recovered, matching
            // Floppy525::writeback()'s always-write behaviour.
        }
        if (!write_disk_image_po_do(media_d, out)) {
            fprintf(stderr, "Floppy_woz: block writeback failed for '%s'\n",
                    media_d->filename.c_str());
            return false;
        }
    } else {
        std::cout << "Floppy_woz: writing back WOZ disk image" << std::endl;
        int rc = woz.save(media_d->filename);
        if (rc != 0) {
            fprintf(stderr, "Floppy_woz: writeback failed for '%s'\n",
                    media_d->filename.c_str());
            return false;
        }
    }

    modified = false;
    return true;
}

// ───────────────────────────── status / reset ─────────────────────────────────

drive_status_t Floppy_woz::status() {
    if (is_mounted) {
        return {is_mounted, media_d->filestub, enable, get_track(), modified,
                media_d->write_protected};
    }
    return {is_mounted, "", enable, get_track(), modified, false};
}

void Floppy_woz::reset() {
    enable = false;
}

// ───────────────────────────── track / bit stream helpers ────────────────────

void Floppy_woz::update_track_ptr() {
    const uint64_t old_bits = cur_track_ptr ? cur_track_ptr->bit_count : 0;
    cur_track_ptr = woz.get_track_ptr(current_tmap_index());
    const uint64_t new_bits = cur_track_ptr ? cur_track_ptr->bit_count : 0;

    // Preserve angular position across a track change. 5.25 tracks are all
    // near-equal length so the old/new ratio is ~1 and this degenerates to
    // a plain modulus; 3.5 tracks vary by zone and the scale is essential.
    if (new_bits && old_bits) {
        // 128-bit intermediate to avoid overflow on the multiply.
        head_position = static_cast<uint64_t>(
            ((__uint128_t)head_position * new_bits) / old_bits);
    }
    if (new_bits) {
        head_position %= new_bits * 8;
    }
    read_position = head_position;
}

uint64_t Floppy_woz::fast_forward(/* uint64_t now */) {
    uint64_t now = get_current_time(); // use our own clock.
    uint64_t elapsed = now - last_cycle;
    last_cycle = now;

    if (!lss_disk_spinning()) {
        return 0;  // disk is not spinning
    }

    const uint32_t adv = head_advance_per_cycle();

    if (!cur_track_ptr || cur_track_ptr->bit_count == 0) {
        // No track under the head: caller will feed the LSS random bits.
        // Return bits_to_sim proportional to elapsed time so read_position
        // stays consistent when a track later becomes available.
        return elapsed * adv;
    }

    uint64_t track_bits = cur_track_ptr->bit_count;

    // Compute new angular position BEFORE updating read_position so we can
    // derive the number of whole bit cells to simulate from the fractional
    // position already accumulated. Using (new_fp >> 3) - (read_fp >> 3)
    // is critical: head_position may be mid-cell, and naive elapsed/4 would
    // permanently skip bits on a straddling boundary.
    head_position += (elapsed * adv);
    uint64_t bits_to_sim =
        ((head_position >> 3) - (read_position >> 3)) % track_bits;
    return bits_to_sim;
}

uint8_t Floppy_woz::get_random_bit() {
    random_bits = (random_bits << 1) | (random_bits >> 63);
    return random_bits & 1;
}

uint8_t Floppy_woz::read_pulse() {
    uint8_t bit;
    if (!lss_disk_spinning() || !cur_track_ptr || cur_track_ptr->bit_count == 0) {
        bit = 0;  // Randomized below via the 4-zero-bit window check.
    } else {
        uint64_t track_bits  = cur_track_ptr->bit_count;
        uint64_t bi          = (read_position >> 3) % track_bits;
        uint64_t byte_idx    = bi >> 3;
        uint64_t bit_in_byte = bi & 7;
        size_t   need_bytes  = (static_cast<size_t>(track_bits) + 7) / 8;
        //uint16_t track_size = cur_track_ptr->bits.size();
        /* if (byte_idx < cur_track_ptr->bits.size() &&
            cur_track_ptr->bits.size() >= need_bytes) { */
        if (bi < cur_track_ptr->bit_count) {
            bit = (cur_track_ptr->bits[byte_idx] >> (7 - static_cast<int>(bit_in_byte))) & 1;
        } else {
            bit = 0;  // WOZ metadata vs. buffer inconsistency: synthesize.
        }
    }

    windowBits = (windowBits << 1) | bit;
    if ((windowBits & 0x0F) == 0) {
        // Four consecutive zeros: hand the LSS a random fake bit.
        bit = get_random_bit();
    }
    read_position += 8;
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        read_position %= cur_track_ptr->bit_count * 8;
    }
    return bit;
}

void Floppy_woz::write_pulse(uint8_t bit) {
    if (lss_disk_spinning() && cur_track_ptr && cur_track_ptr->bit_count > 0) {
        uint64_t track_bits  = cur_track_ptr->bit_count;
        uint64_t bi          = (read_position >> 3) % track_bits;
        uint64_t byte_idx    = bi >> 3;
        uint64_t bit_in_byte = bi & 7;
        size_t   need_bytes  = (static_cast<size_t>(track_bits) + 7) / 8;
        if (byte_idx < cur_track_ptr->bits.size() &&
            cur_track_ptr->bits.size() >= need_bytes) {
            uint8_t mask = static_cast<uint8_t>(1u << (7 - static_cast<int>(bit_in_byte)));
            if (bit & 1) cur_track_ptr->bits[byte_idx] |= mask;
            else         cur_track_ptr->bits[byte_idx] &= static_cast<uint8_t>(~mask);
            modified = true;
        }
    }
    read_position += 8;
    if (cur_track_ptr && cur_track_ptr->bit_count > 0) {
        read_position %= cur_track_ptr->bit_count * 8;
    }
}

// ───────────────────────────── unused cmd shims ──────────────────────────────

uint8_t Floppy_woz::read_cmd(uint16_t address) {
    (void)address;
    assert(false && "Floppy_woz::read_cmd is not used in the IWM/LSS path");
    return 0;
}

void Floppy_woz::write_cmd(uint16_t address, uint8_t data) {
    (void)address;
    (void)data;
    assert(false && "Floppy_woz::write_cmd is not used in the IWM/LSS path");
}

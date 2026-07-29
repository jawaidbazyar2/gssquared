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

#include <algorithm>
#include <cstddef>
#include <cstdint>

/**
 * IIgs contiguous fast RAM sizing (banks $00+), matching KEGS / real hardware.
 *
 * Motherboard (ROM-dependent) + expansion card size. Mega II ($E0/$E1) is
 * separate and not included here.
 *
 *   ROM01 (128KB ROM): 128KB mobo ($00–$01) + card
 *   ROM03 (256KB ROM): 1MB mobo ($00–$0F) + card
 *
 * Default card size is 8MB. Totals:
 *   ROM01 + 8MB card → 8.125MB (banks $00–$81)
 *   ROM03 + 8MB card → 9MB (banks $00–$8F)
 *
 * `exp_bytes` is expansion-card size, not total system RAM.
 */
namespace iigs_memory {

constexpr size_t kBankBytes = 65536;
constexpr size_t kDefaultExpBytes = 8u * 1024u * 1024u;
/** KEGS clamp: contiguous RAM must not extend into the $E0+ Mega II / ROM region. */
constexpr size_t kMaxFastRamBytes = 0xDF0000;

inline bool is_rom03(size_t rom_size_bytes) {
    return rom_size_bytes >= 4 * kBankBytes;
}

/** Built-in fast RAM on the motherboard for this ROM revision. */
inline size_t mobo_ram_bytes(size_t rom_size_bytes) {
    return is_rom03(rom_size_bytes) ? (1024u * 1024u) : (128u * 1024u);
}

/**
 * Contiguous FPI RAM size from bank $00: motherboard + expansion card,
 * capped at kMaxFastRamBytes.
 */
inline size_t fast_ram_bytes(size_t rom_size_bytes, size_t exp_bytes = kDefaultExpBytes) {
    const size_t total = mobo_ram_bytes(rom_size_bytes) + exp_bytes;
    return std::min(total, kMaxFastRamBytes);
}

inline uint32_t last_ram_bank(size_t fast_ram_bytes_value) {
    if (fast_ram_bytes_value < kBankBytes) {
        return 0;
    }
    return static_cast<uint32_t>((fast_ram_bytes_value / kBankBytes) - 1);
}

} // namespace iigs_memory

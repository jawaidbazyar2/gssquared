#pragma once

#include <cstdint>

/**
 * C029 bit 6 aux linearization: map CPU linear address in $2000–$9FFF
 * to physical Mega II aux interleave ($2000/$6000 halves).
 *
 *   x = linear - $2000
 *   phys = $2000 + ((x & 1) << 14) | (x >> 1)
 *
 * Covers the entire window (SHR pixels, SCBs $9Dxx, palettes $9Exx).
 * Outside $2000–$9FFF the address is unchanged.
 */
inline uint16_t iigs_aux_linear_to_phys(uint16_t linear) {
    if (linear < 0x2000 || linear > 0x9FFF) {
        return linear;
    }
    uint16_t x = linear - 0x2000;
    return 0x2000 + (((x & 1) << 14) | (x >> 1));
}

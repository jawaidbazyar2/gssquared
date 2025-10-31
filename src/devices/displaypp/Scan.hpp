#pragma once

#include <cstdint>

struct alignas(4) Scan_t {
    uint8_t mode;
    uint8_t auxbyte;
    uint8_t mainbyte;
    uint8_t flags;
    uint32_t shr_bytes;
};

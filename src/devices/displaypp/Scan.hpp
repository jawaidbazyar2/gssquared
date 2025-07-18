#pragma once

#include <cstdint>

struct Scan_t {
    uint8_t mode;
    uint8_t auxbyte;
    uint8_t mainbyte;
    uint8_t unused;
};

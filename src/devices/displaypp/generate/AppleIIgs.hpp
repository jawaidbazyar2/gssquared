#pragma once

#include <cstdint>

#include "devices/displaypp/RGBA.hpp"

struct SHRColor {
    union {
        struct {
            uint8_t b:4; 
            uint8_t g:4;
            uint8_t r:4;
            uint8_t _x:4;
        };
        uint16_t v;
    };
};

struct SHRMode {
    union {
        struct {
            uint8_t p:4;
            uint8_t _unused:1;
            uint8_t fill:1;
            uint8_t interrupt:1;
            uint8_t mode640:1;
        };
        uint8_t v;
    };
};

struct Palette {
    SHRColor colors[16];
};

struct SHR {
    uint8_t pixels[32000];
    SHRMode modes[200];
    uint8_t unused1[56];
    Palette palettes[16];
};

// Convert 12-bit color value to 24-bit color value
inline RGBA_t convert12bitTo24bit(SHRColor c12) {
    RGBA_t c24;

    // Expand each 4-bit component to 8 bits
    // By duplicating the 4 bits to fill the 8-bit space
    c24.r = (c12.r << 4) | c12.r;
    c24.g = (c12.g << 4) | c12.g;
    c24.b = (c12.b << 4) | c12.b;
    c24.a = 0xFF;
    // Combine into 24-bit color value
    return c24;
}

template<int pixelno>
inline uint8_t pixel640(uint8_t pval) {
    return (pval & (0b11 << (pixelno*2)))>>(pixelno*2);
}

template<int pixelno>
inline uint8_t pixel320(uint8_t pval) {
    return (pval & (0b1111 << (pixelno*4)))>>(pixelno*4);
}

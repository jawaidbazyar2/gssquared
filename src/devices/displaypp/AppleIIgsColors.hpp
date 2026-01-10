#pragma once

#include "RGBA.hpp"

namespace AppleIIgs {
    // 16-entry RGB color table for Apple IIgs colors
    // Each color is 4-bit R, G, B scaled to 8-bit RGBA
    // Defined in AppleIIgsColors.cpp
    extern const RGBA_t RGB_COLORS[16];
    extern const RGBA_t TEXT_COLORS[16];

    // Color indices for common colors
    enum ColorIndex : uint8_t {
        BLACK = 0,
        DEEP_RED = 1,
        BROWN = 2,
        ORANGE = 3,
        DARK_GREEN = 4,
        DARK_GRAY = 5,
        LIGHT_GREEN = 6,
        YELLOW = 7,
        DARK_BLUE = 8,
        PURPLE = 9,
        LIGHT_GRAY = 10,
        PINK = 11,
        MEDIUM_BLUE = 12,
        LIGHT_BLUE = 13,
        AQUAMARINE = 14,
        WHITE = 15
    };
}

#pragma once

#include <cstdint>

typedef enum PlatformId_t {
    PLATFORM_APPLE_II = 0,
    PLATFORM_APPLE_II_PLUS,
    PLATFORM_APPLE_IIE,
    PLATFORM_APPLE_IIE_ENHANCED,
    PLATFORM_APPLE_IIE_65816,
    PLATFORM_APPLE_IIGS,
    PLATFORM_END
} PlatformId_t;

using PlatformFlags_t = uint32_t;
const PlatformFlags_t PLATFLAG_NONE = 0;
const PlatformFlags_t PLATFLAG_APPLE_II = 1 << 0;
const PlatformFlags_t PLATFLAG_APPLE_II_PLUS = 1 << 1;
const PlatformFlags_t PLATFLAG_APPLE_IIE = 1 << 2;
const PlatformFlags_t PLATFLAG_APPLE_IIE_ENHANCED = 1 << 3;
const PlatformFlags_t PLATFLAG_APPLE_IIE_65816 = 1 << 4;
const PlatformFlags_t PLATFLAG_APPLE_IIGS = 1 << 5;
const PlatformFlags_t PLATFLAG_ALL = 0xFFFFFFFF;
#pragma once

#include <cstdint>

typedef enum PlatformId_t {
    PLATFORM_APPLE_II = 0,
    PLATFORM_APPLE_II_PLUS,
    PLATFORM_APPLE_IIE,
    PLATFORM_APPLE_IIE_ENHANCED,
    PLATFORM_APPLE_IIE_65816,
    PLATFORM_APPLE_IIGS,
    PLATFORM_APPLE_IIGS_ROM3,
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
const PlatformFlags_t PLATFLAG_APPLE_IIGS_ROM3 = 1 << 6;
const PlatformFlags_t PLATFLAG_ALL = 0xFFFFFFFF;
const PlatformFlags_t PLATFLAG_ANY_IIGS = PLATFLAG_APPLE_IIGS | PLATFLAG_APPLE_IIGS_ROM3;

inline bool platform_is_iigs(PlatformId_t id) {
    return id == PLATFORM_APPLE_IIGS || id == PLATFORM_APPLE_IIGS_ROM3;
}
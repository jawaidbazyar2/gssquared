#pragma once

#include <cstdint>

#pragma pack(push, 1)

/**
 * different platforms have different 'preferred' pixel formats.
 * we will select at compile time the best data structure.
 * This likely needs to be more like 'Mac' etc.
 */

#ifdef __APPLE__
// Apple - ABGR 
struct RGBA_t {
    union {
        struct {
            uint8_t r, g, b, a;
        };
        uint32_t rgba;
    };

    bool operator==(const RGBA_t& other) const {
        return rgba == other.rgba;
    }
    
    bool operator!=(const RGBA_t& other) const {
        return rgba != other.rgba;
    }

    static constexpr RGBA_t make(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
        return {r, g, b, a};  // Apple order: r, g, b, a
    }
};
#define PIXEL_FORMAT SDL_PIXELFORMAT_ABGR8888
#elif defined(__linux__)
// Linux - RGBA
struct RGBA_t {
    union {
        struct {
            uint8_t a, b, g, r;
        };
        uint32_t rgba;
    };
    bool operator==(const RGBA_t& other) const {
        return rgba == other.rgba;
    }
    
    bool operator!=(const RGBA_t& other) const {
        return rgba != other.rgba;
    }

    static constexpr RGBA_t make(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
        return {a, b, g, r};  // Linux order: a, b, g, r
    }
};
#define PIXEL_FORMAT SDL_PIXELFORMAT_RGBA8888
#elif defined(_WIN64)
// Windows - BGRA
struct RGBA_t {
    union {
        struct {
            uint8_t a, r, g, b;
        };   
        uint32_t rgba;
    };
    bool operator==(const RGBA_t& other) const {
        return rgba == other.rgba;
    }
    
    bool operator!=(const RGBA_t& other) const {
        return rgba != other.rgba;
    }

    static constexpr RGBA_t make(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) {
        return {a, r, g, b};  // Windows order: a, r, g, b
    }
};
#define PIXEL_FORMAT SDL_PIXELFORMAT_BGRA8888
#else
#error "Unsupported platform"
#endif

#pragma pack(pop)

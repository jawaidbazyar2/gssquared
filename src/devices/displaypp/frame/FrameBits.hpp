#pragma once

#include <cstdint>

/**
 * Packed 1-bit-per-dot legacy video frame (see Docs/NTSCShader.md, "Packed line layout").
 *
 * Words 0..17 hold dots in display coordinates: dot c is bit (c & 31) of word (c >> 5).
 * Word 18 holds the line mode: byte 0 colorburst, byte 1 phase_offset, byte 2 line_start.
 * The layout is little-endian word order and doubles as the GPU upload format.
 */
struct FrameBits {
    static constexpr uint32_t LINES = 192;
    static constexpr uint32_t DOT_WORDS = 18;
    static constexpr uint32_t WORDS = 19;

    alignas(64) uint32_t words[LINES][WORDS];

    inline uint32_t *line(uint32_t y) noexcept { return words[y]; }
    inline const uint32_t *line(uint32_t y) const noexcept { return words[y]; }

    inline void clear_line(uint32_t y) noexcept {
        for (uint32_t i = 0; i < WORDS; i++) words[y][i] = 0;
    }

    inline void clear() noexcept {
        for (uint32_t y = 0; y < LINES; y++) clear_line(y);
    }

    static inline uint32_t make_mode(uint32_t colorburst, uint32_t phase_offset, uint32_t line_start) noexcept {
        return (colorburst & 0xFF) | ((phase_offset & 0xFF) << 8) | ((line_start & 0xFF) << 16);
    }

    inline uint32_t colorburst(uint32_t y) const noexcept { return words[y][DOT_WORDS] & 0xFF; }
    inline uint32_t phase_offset(uint32_t y) const noexcept { return (words[y][DOT_WORDS] >> 8) & 0xFF; }
    inline uint32_t line_start(uint32_t y) const noexcept { return (words[y][DOT_WORDS] >> 16) & 0xFF; }
};

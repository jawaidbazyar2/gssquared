#pragma once

#include <cstdint>
#include <cstring>

/**
 * Apple II text-page layout helpers (debug / video snapshot).
 * Row offsets match the classic nonlinear $0400/$0800 map used by video generation.
 */
namespace text_page {

inline constexpr uint16_t kTextRowOff[24] = {
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
    0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
    0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0,
};

inline constexpr uint32_t kRows = 24;
inline constexpr uint32_t kCols40 = 40;
inline constexpr uint32_t kCols80 = 80;
inline constexpr uint32_t kAuxBankOffset = 0x10000;

/** Resolved page 1 or 2 → base offset in main (or aux) bank. */
inline constexpr uint16_t page_base(uint32_t page) {
    return (page == 2) ? 0x0800 : 0x0400;
}

/** Linearize 40-column text into row-major out[24*40]. */
inline void linearize_text40(const uint8_t *main_ram, uint32_t page, uint8_t *out) {
    const uint16_t base = page_base(page);
    for (uint32_t row = 0; row < kRows; ++row) {
        const uint8_t *src = main_ram + base + kTextRowOff[row];
        std::memcpy(out + row * kCols40, src, kCols40);
    }
}

/**
 * Linearize 80-column text: display order is aux then main per column pair
 * (matches generate_text80 / VM_TEXT80).
 * main_ram is the physical 128 KB buffer; aux lives at +0x10000.
 */
inline void linearize_text80(const uint8_t *main_ram, uint32_t page, uint8_t *out) {
    const uint16_t base = page_base(page);
    const uint8_t *aux_ram = main_ram + kAuxBankOffset;
    for (uint32_t row = 0; row < kRows; ++row) {
        const uint16_t off = static_cast<uint16_t>(base + kTextRowOff[row]);
        uint8_t *dst = out + row * kCols80;
        for (uint32_t x = 0; x < kCols40; ++x) {
            dst[2 * x] = aux_ram[off + x];
            dst[2 * x + 1] = main_ram[off + x];
        }
    }
}

} // namespace text_page

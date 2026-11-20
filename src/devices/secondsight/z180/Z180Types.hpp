/*
 *   Fixed-width storage used by Second Sight's Zilog Z8S180 core.
 *   Instruction handlers need both a register pair and its individual bytes.
 *   These unions provide those views without changing the emulated byte order.
 *   They are fields of a CPU or serial device and use that owner's execution
 *   thread. The helpers perform no bus access and advance no emulated time.
 *
 *   Source for the register layout: Zilog UM005004-0918 pp. 173-179.
 *   The host storage types and bit helper are software implementation choices.
 */

#pragma once

#include <cstdint>

// GSSquared's supported targets are little endian.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The Z180 register byte views require a little endian target.
#endif

// CPU-owned register/address slot. w.l is the 16-bit CPU pair; b.l/b.h
// are its low/high bytes. The wider d view also holds 20-bit direct memory access (DMA) addresses.
union Z180Register {
    uint32_t d = 0;
    struct {
        uint16_t l, h;
    } w;
    struct {
        uint8_t l, h, h2, h3;
    } b;
};

// Device-owned 16-bit counter or constant, accessible as one word or two bytes.
union Z180Word {
    uint16_t w = 0;
    struct {
        uint8_t l, h;
    } b;
};

// Return bit 0-31 of value as 0 or 1 for pin and register-field decoding.
constexpr unsigned z180_bit(uint32_t value, unsigned bit) {
    return (value >> bit) & 1;
}

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstdint>

/** Matches SecondSight::vga_mode_rec (84 bytes used by SetUserMode). */
struct ss_vga_mode_rec {
    uint8_t misc;
    uint8_t clockMode;
    uint8_t featureCtl;
    uint8_t seq02;
    uint8_t seq03;
    uint8_t seq04;
    uint8_t crt_regs[0x19];
    uint8_t graph_regs[0x09];
    uint8_t attr_regs[0x15];
    uint8_t ext_regs[20];
    uint8_t unused[30];
};

/**
 * SecondSight ROM vga_text80x25 (mode 0x03) — Oak OTI-087 / IBM VGA 80x25 text.
 * Disassembly label: vga_text80x25 (ROM:0D03).
 */
inline constexpr ss_vga_mode_rec SS_ROM_VGA_TEXT_80X25 = {
    0x67, // misc
    0x00, // sequencer clocking mode
    0x00, // feature / PEL mask
    0x03, // seq02 map mask
    0x00, // seq03 character map select
    0x02, // seq04 memory mode
    { // crt
        0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, // 00-05
        0xBF, 0x1F, 0x00, 0x4F, 0x2D, 0x0E, // 06-0B
        0x00, 0x00, 0x00, 0x00,             // 0C-0F start addr, cursor
        0x9C, 0x8E, 0x8F, 0x28, 0x1F,       // 10-14
        0x96, 0xB9, 0xA3, 0xFF,             // 15-18
    },
    { // graphics
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x10, 0x0E, 0x00, 0xFF,
    },
    { // attribute
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x0C, 0x10, 0x0F, 0x08, 0x00,
    },
    { // ext (OTI chipset; not linearly mapped on real hardware)
        0x05, 0x10, 0x07, 0x00, 0x01,
        0xC8, 0x00, 0x00, 0x00, 0x0F,
        0x01, 0x00, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {}, // unused
};

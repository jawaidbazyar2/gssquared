/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <cstdint>

static constexpr uint8_t SS_HT_CTRL_BYTES = 32;
static constexpr uint8_t SS_HT_MODE_80X25 = 0x03;
static constexpr uint8_t SS_HT_MODE_80X43 = 0x43;

struct ss_host_text_raster_t {
    uint8_t mode = SS_HT_MODE_80X25;
    uint8_t cols = 80;
    uint8_t vis_rows = 25;
    uint8_t cell_w = 9;
    uint8_t cell_h = 16;
    uint16_t pix_w = 720;
    uint16_t pix_h = 400;
};

inline bool ss_host_text_lookup_raster(uint8_t mode_num, ss_host_text_raster_t *out) {
    if (!out) {
        return false;
    }
    switch (mode_num) {
        case SS_HT_MODE_80X25:
            *out = {SS_HT_MODE_80X25, 80, 25, 9, 16, 720, 400};
            return true;
        case SS_HT_MODE_80X43:
            *out = {SS_HT_MODE_80X43, 80, 43, 8, 8, 640, 344};
            return true;
        default:
            return false;
    }
}

static constexpr uint8_t SS_HT_PLANAR = 0x01;
static constexpr uint8_t SS_HT_WRAP = 0x02;
static constexpr uint8_t SS_HT_PAL_FROM_BLOCK = 0x04;
static constexpr uint8_t SS_HT_BLINK = 0x08;
static constexpr uint8_t SS_HT_CURSOR = 0x10; /* v0.1–v0.2; superseded by cursor_style */
static constexpr uint8_t SS_HT_BUFFER_AUX = 0x20;
static constexpr uint8_t SS_HT_ATTR_AUX = 0x40;
static constexpr uint8_t SS_HT_PAL_AUX = 0x80;

static constexpr uint16_t SS_HT_CS_ENABLE = 0x0001;
static constexpr uint16_t SS_HT_CS_BLINK = 0x0002;
static constexpr uint16_t SS_HT_CS_SHAPE_MASK = 0x000C;
static constexpr uint16_t SS_HT_CS_SHAPE_SHIFT = 2;
static constexpr uint16_t SS_HT_CS_REPLACE = 0x0010;
static constexpr uint16_t SS_HT_CS_START_SHIFT = 8;
static constexpr uint16_t SS_HT_CS_END_SHIFT = 12;
static constexpr uint16_t SS_HT_CS_NIBBLE = 0x0F;
static constexpr uint16_t SS_HT_CS_DEFAULT_LINES = 0x0F;
static constexpr uint16_t SS_HT_CS_LEGACY = 0x0003; /* enable + blink, block, invert */

enum ss_ht_cursor_shape_t : uint8_t {
    SS_HT_CS_SHAPE_BLOCK = 0,
    SS_HT_CS_SHAPE_UNDERLINE = 1,
    SS_HT_CS_SHAPE_BAR = 2,
};

#pragma pack(push, 1)
struct ss_host_text_ctrl_t {
    uint8_t flags;
    uint8_t cols;
    uint8_t vis_rows;
    uint8_t virt_rows;
    uint8_t start_line;
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t frozen_top;
    uint8_t frozen_bottom;
    uint8_t reserved;
    uint16_t buffer_addr;
    uint16_t attr_addr;
    uint16_t pal_addr;
    uint16_t cursor_style;
    uint8_t pad[14];
};
#pragma pack(pop)

static_assert(sizeof(ss_host_text_ctrl_t) == SS_HT_CTRL_BYTES, "host text ctrl is 32 bytes");
static_assert(offsetof(ss_host_text_ctrl_t, cursor_style) == 0x10, "cursor_style at $10");

inline uint16_t ss_host_text_effective_cursor_style(const ss_host_text_ctrl_t &c) {
    if (c.cursor_style != 0) {
        return c.cursor_style;
    }
    if (c.flags & SS_HT_CURSOR) {
        return SS_HT_CS_LEGACY;
    }
    return 0;
}

/** Mega II / IIe: main at +0, aux at +64K. */
const uint8_t *ss_host_text_bank(const uint8_t *a2_ram, uint32_t ram_size, bool aux);

bool ss_host_text_ctrl_valid(const ss_host_text_ctrl_t &c);

/**
 * Compose vis_rows of interleaved cells into dst (pitch 160 for 80 cols).
 * Remaining rows up to max_rows (the raster) are filled with spaces / attr 0.
 * Returns false if ctrl is invalid or vis_rows does not fit the raster.
 */
bool ss_host_text_compose(uint8_t *dst, int dst_pitch,
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c,
    int max_rows);

/** Overlay hardware cursor onto an ARGB8888 raster (after vga_raster_text_*). */
void ss_host_text_overlay_cursor(uint32_t *pixels, int pixel_pitch,
    const uint8_t *cells, int cell_pitch, const ss_host_text_ctrl_t &c,
    bool blink_phase, int cell_w, int cell_h, int max_rows);

/** 48-byte RGB888 at pal_addr; false if the window is missing. */
bool ss_host_text_read_palette(uint8_t rgb48[48],
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c);

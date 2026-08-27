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

static constexpr uint8_t SS_HT_CTRL_BYTES = 32;

static constexpr uint8_t SS_HT_PLANAR = 0x01;
static constexpr uint8_t SS_HT_WRAP = 0x02;
static constexpr uint8_t SS_HT_PAL_FROM_BLOCK = 0x04;
static constexpr uint8_t SS_HT_BLINK = 0x08;
static constexpr uint8_t SS_HT_CURSOR = 0x10;
static constexpr uint8_t SS_HT_BUFFER_AUX = 0x20;
static constexpr uint8_t SS_HT_ATTR_AUX = 0x40;
static constexpr uint8_t SS_HT_PAL_AUX = 0x80;

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
    uint8_t pad[16];
};
#pragma pack(pop)

static_assert(sizeof(ss_host_text_ctrl_t) == SS_HT_CTRL_BYTES, "host text ctrl is 32 bytes");

/** Mega II / IIe: main at +0, aux at +64K. */
const uint8_t *ss_host_text_bank(const uint8_t *a2_ram, uint32_t ram_size, bool aux);

bool ss_host_text_ctrl_valid(const ss_host_text_ctrl_t &c);

/**
 * Compose vis_rows of interleaved cells into dst (pitch 160 for 80 cols).
 * Remaining rows up to VGA_TEXT_ROWS are filled with spaces / attr 0.
 * Returns false if ctrl is invalid (caller should blank).
 */
bool ss_host_text_compose(uint8_t *dst, int dst_pitch,
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c);

void ss_host_text_apply_cursor(uint8_t *dst, int dst_pitch, const ss_host_text_ctrl_t &c,
    bool blink_on);

/** 48-byte RGB888 at pal_addr; false if the window is missing. */
bool ss_host_text_read_palette(uint8_t rgb48[48],
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c);

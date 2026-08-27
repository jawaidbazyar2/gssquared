/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "ss_host_text.hpp"
#include "vga_render_text_9x16.hpp"

#include <cstring>

const uint8_t *ss_host_text_bank(const uint8_t *a2_ram, uint32_t ram_size, bool aux) {
    if (a2_ram == nullptr || ram_size == 0) {
        return nullptr;
    }
    if (aux) {
        if (ram_size < 0x20000u) {
            return nullptr;
        }
        return a2_ram + 0x10000;
    }
    return a2_ram;
}

bool ss_host_text_ctrl_valid(const ss_host_text_ctrl_t &c) {
    if (c.cols != 40 && c.cols != 80) {
        return false;
    }
    if (c.vis_rows == 0 || c.virt_rows < c.vis_rows) {
        return false;
    }
    if ((uint16_t)c.frozen_top + (uint16_t)c.frozen_bottom >= c.vis_rows) {
        return false;
    }
    if ((c.flags & SS_HT_PAL_FROM_BLOCK) && c.pal_addr == 0) {
        return false;
    }
    return true;
}

static int map_src_row(const ss_host_text_ctrl_t &c, int y) {
    const int vis = (int)c.vis_rows;
    const int virt = (int)c.virt_rows;
    const int top = (int)c.frozen_top;
    const int bot = (int)c.frozen_bottom;
    if (y < top) {
        return y;
    }
    if (y >= vis - bot) {
        return virt - bot + (y - (vis - bot));
    }
    /* Scroll band is the virtual rows that are not frozen. Wrap/clamp stay
     * inside that band so a status line is not pulled into the scrolling
     * region when start_line is 0 (or wraps past virt-1). */
    const int vis_in_scroll = y - top;
    const int band0 = top;
    const int band = virt - top - bot;
    if (band <= 0) {
        return top;
    }
    int start = (int)c.start_line;
    if (c.flags & SS_HT_WRAP) {
        start %= band;
        if (start < 0) {
            start += band;
        }
        return band0 + (start + vis_in_scroll) % band;
    }
    int src = start + vis_in_scroll;
    if (src < band0) {
        src = band0;
    }
    if (src >= band0 + band) {
        src = band0 + band - 1;
    }
    return src;
}

static uint8_t host_at(const uint8_t *bank, uint16_t addr) {
    return bank[addr];
}

bool ss_host_text_compose(uint8_t *dst, int dst_pitch,
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c)
{
    if (!dst || !ss_host_text_ctrl_valid(c)) {
        return false;
    }
    const int cols = (int)c.cols;
    if (dst_pitch < cols * 2) {
        return false;
    }

    const bool planar = (c.flags & SS_HT_PLANAR) != 0;
    const uint8_t *buf_bank = ss_host_text_bank(a2_ram, ram_size, (c.flags & SS_HT_BUFFER_AUX) != 0);
    if (buf_bank == nullptr) {
        return false;
    }
    bool attr_aux = (c.flags & SS_HT_ATTR_AUX) != 0;
    uint16_t attr_base = c.attr_addr;
    if (!planar) {
        attr_base = 0;
    } else if (c.attr_addr == 0) {
        attr_base = (uint16_t)(c.buffer_addr + (uint16_t)cols * (uint16_t)c.virt_rows);
        attr_aux = (c.flags & SS_HT_BUFFER_AUX) != 0;
    }
    const uint8_t *attr_bank = planar
        ? ss_host_text_bank(a2_ram, ram_size, attr_aux)
        : buf_bank;
    if (planar && attr_bank == nullptr) {
        return false;
    }

    const int vis = (int)c.vis_rows > VGA_TEXT_ROWS ? VGA_TEXT_ROWS : (int)c.vis_rows;
    const uint16_t char_pitch = planar ? (uint16_t)cols : (uint16_t)(cols * 2);

    for (int y = 0; y < VGA_TEXT_ROWS; y++) {
        uint8_t *row = dst + y * dst_pitch;
        if (y >= vis) {
            for (int x = 0; x < cols; x++) {
                row[x * 2] = 0x20;
                row[x * 2 + 1] = 0x00;
            }
            continue;
        }
        const int src_row = map_src_row(c, y);
        const uint16_t row_off = (uint16_t)((uint32_t)src_row * char_pitch);
        if (planar) {
            const uint16_t ch_base = (uint16_t)(c.buffer_addr + row_off);
            const uint16_t at_base = (uint16_t)(attr_base + (uint16_t)((uint32_t)src_row * (uint32_t)cols));
            for (int x = 0; x < cols; x++) {
                row[x * 2] = host_at(buf_bank, (uint16_t)(ch_base + (uint16_t)x));
                row[x * 2 + 1] = host_at(attr_bank, (uint16_t)(at_base + (uint16_t)x));
            }
        } else {
            const uint16_t cell_base = (uint16_t)(c.buffer_addr + row_off);
            for (int x = 0; x < cols; x++) {
                const uint16_t cell = (uint16_t)(cell_base + (uint16_t)(x * 2));
                row[x * 2] = host_at(buf_bank, cell);
                row[x * 2 + 1] = host_at(buf_bank, (uint16_t)(cell + 1));
            }
        }
    }
    return true;
}

void ss_host_text_apply_cursor(uint8_t *dst, int dst_pitch, const ss_host_text_ctrl_t &c,
    bool blink_on)
{
    if (!dst || (c.flags & SS_HT_CURSOR) == 0 || !blink_on) {
        return;
    }
    const int cols = (int)c.cols;
    const int vis = (int)c.vis_rows;
    if (c.cursor_x >= cols || c.cursor_y >= vis || c.cursor_y >= VGA_TEXT_ROWS) {
        return;
    }
    uint8_t *cell = dst + (int)c.cursor_y * dst_pitch + (int)c.cursor_x * 2;
    const uint8_t attr = cell[1];
    cell[1] = (uint8_t)(((attr & 0x0F) << 4) | ((attr >> 4) & 0x0F));
}

bool ss_host_text_read_palette(uint8_t rgb48[48],
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c)
{
    if (!rgb48) {
        return false;
    }
    const uint8_t *bank = ss_host_text_bank(a2_ram, ram_size, (c.flags & SS_HT_PAL_AUX) != 0);
    if (bank == nullptr) {
        return false;
    }
    for (int i = 0; i < 48; i++) {
        rgb48[i] = host_at(bank, (uint16_t)(c.pal_addr + (uint16_t)i));
    }
    return true;
}

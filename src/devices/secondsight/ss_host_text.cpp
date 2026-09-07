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
    const uint8_t *a2_ram, uint32_t ram_size, const ss_host_text_ctrl_t &c,
    int max_rows)
{
    if (!dst || !ss_host_text_ctrl_valid(c) || max_rows <= 0) {
        return false;
    }
    if ((int)c.vis_rows > max_rows) {
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

    const int vis = (int)c.vis_rows;
    const uint16_t char_pitch = planar ? (uint16_t)cols : (uint16_t)(cols * 2);

    for (int y = 0; y < max_rows; y++) {
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

static void cursor_scanlines(uint16_t style, int cell_h, int *sl0, int *sl1) {
    const int shape = (int)((style & SS_HT_CS_SHAPE_MASK) >> SS_HT_CS_SHAPE_SHIFT);
    const int start = (int)((style >> SS_HT_CS_START_SHIFT) & SS_HT_CS_NIBBLE);
    const int end = (int)((style >> SS_HT_CS_END_SHIFT) & SS_HT_CS_NIBBLE);
    if (start == (int)SS_HT_CS_DEFAULT_LINES || end == (int)SS_HT_CS_DEFAULT_LINES
        || start > end) {
        if (shape == SS_HT_CS_SHAPE_UNDERLINE) {
            *sl0 = (cell_h >= 2) ? cell_h - 2 : 0;
            *sl1 = cell_h - 1;
        } else {
            *sl0 = 0;
            *sl1 = cell_h - 1;
        }
        return;
    }
    *sl0 = (start >= cell_h) ? cell_h - 1 : start;
    *sl1 = (end >= cell_h) ? cell_h - 1 : end;
    if (*sl0 > *sl1) {
        *sl0 = 0;
        *sl1 = cell_h - 1;
    }
}

void ss_host_text_overlay_cursor(uint32_t *pixels, int pixel_pitch,
    const uint8_t *cells, int cell_pitch, const ss_host_text_ctrl_t &c,
    bool blink_phase, int cell_w, int cell_h, int max_rows)
{
    if (!pixels || !cells || cell_w <= 0 || cell_h <= 0 || pixel_pitch <= 0 || max_rows <= 0) {
        return;
    }
    const uint16_t style = ss_host_text_effective_cursor_style(c);
    if ((style & SS_HT_CS_ENABLE) == 0) {
        return;
    }
    if ((style & SS_HT_CS_BLINK) && !blink_phase) {
        return;
    }
    const int cols = (int)c.cols;
    const int vis = (int)c.vis_rows;
    if (c.cursor_x >= cols || c.cursor_y >= vis || (int)c.cursor_y >= max_rows) {
        return;
    }
    if (cell_pitch < cols * 2) {
        return;
    }

    const int cx = (int)c.cursor_x;
    const int cy = (int)c.cursor_y;
    const uint8_t attr = cells[cy * cell_pitch + cx * 2 + 1];
    const uint32_t *palette = vga_text_palette();
    const uint32_t fg = palette[attr & 0x0F];
    const uint32_t bg = palette[(attr >> 4) & 0x0F];
    const bool replace = (style & SS_HT_CS_REPLACE) != 0;
    int shape = (int)((style & SS_HT_CS_SHAPE_MASK) >> SS_HT_CS_SHAPE_SHIFT);
    if (shape == 3) {
        shape = SS_HT_CS_SHAPE_BLOCK;
    }

    int sl0 = 0;
    int sl1 = cell_h - 1;
    cursor_scanlines(style, cell_h, &sl0, &sl1);

    const int x0 = cx * cell_w;
    const int x1 = (shape == SS_HT_CS_SHAPE_BAR)
        ? x0 + ((cell_w >= 9) ? 2 : 1)
        : x0 + cell_w;

    for (int gy = sl0; gy <= sl1; gy++) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (cy * cell_h + gy) * pixel_pitch);
        for (int px = x0; px < x1; px++) {
            if (replace) {
                row[px] = fg;
            } else {
                row[px] = (row[px] == fg) ? bg : fg;
            }
        }
    }
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

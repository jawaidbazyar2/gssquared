/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "ss_gpu_text.hpp"

#include <cstdio>
#include <cstring>

bool SsGpuText::enter(uint8_t mode_num) {
    ss_host_text_raster_t raster{};
    if (!ss_host_text_lookup_raster(mode_num, &raster)) {
        return false;
    }
    if (active && raster_.cols == raster.cols && raster_.vis_rows == raster.vis_rows
        && raster_.cell_w == raster.cell_w && raster_.cell_h == raster.cell_h) {
        /* Same raster: keep VRAM / pen / sync / yield. SetMode is not Erase. */
        return true;
    }
    leave();
    raster_ = raster;
    const size_t nbytes = (size_t)raster.cols * (size_t)raster.vis_rows * 2u;
    cells_.resize(nbytes); /* new buffer; not Reset — host Erase/$92 if it wants a page */
    reset_pen_and_margins();
    sync = Sync::Immediate;
    yielded_ = false;
    active = true;
    return true;
}

void SsGpuText::leave() {
    active = false;
    yielded_ = false;
    have_lo = false;
    lo_byte = 0;
    pending = Pending::None;
    pending_count = 1;
    ring_head = 0;
    ring_tail = 0;
    ring_count = 0;
    cells_.clear();
    raster_ = {};
    sync = Sync::Immediate;
}

void SsGpuText::reset_pen_and_margins() {
    x = 0;
    y = 0;
    left = 0;
    right = raster_.cols ? (uint8_t)(raster_.cols - 1) : 0;
    top = 0;
    bottom = raster_.vis_rows ? (uint8_t)(raster_.vis_rows - 1) : 0;
    attr = ATTR_RESET;
    style = CURSOR_RESET;
}

void SsGpuText::erase_full_raster() {
    const int cols = (int)raster_.cols;
    const int rows = (int)raster_.vis_rows;
    fill_rect(0, 0, cols - 1, rows - 1);
}

void SsGpuText::push_byte(uint8_t b) {
    if (!active) {
        return;
    }
    if (!have_lo) {
        lo_byte = b;
        have_lo = true;
        return;
    }
    const uint16_t w = (uint16_t)lo_byte | ((uint16_t)b << 8);
    have_lo = false;
    enqueue_word(w);
    if (sync_immediate()) {
        drain();
    }
}

void SsGpuText::enqueue_word(uint16_t w) {
    if (!active) {
        return;
    }
    if (ring_count >= RING_WORDS) {
        return; /* drop newest */
    }
    ring[ring_head] = w;
    ring_head = (ring_head + 1) % RING_WORDS;
    ring_count++;
}

void SsGpuText::drain() {
    while (ring_count > 0) {
        const uint16_t w = ring[ring_tail];
        ring_tail = (ring_tail + 1) % RING_WORDS;
        ring_count--;
        execute_word(w);
    }
}

const uint8_t *SsGpuText::cell_at(int col, int row) const {
    const int cols = (int)raster_.cols;
    const int rows = (int)raster_.vis_rows;
    if (col < 0 || row < 0 || col >= cols || row >= rows) {
        return nullptr;
    }
    return cells_.data() + (row * cols + col) * 2;
}

uint8_t *SsGpuText::cell_mut(int col, int row) {
    return const_cast<uint8_t *>(cell_at(col, row));
}

void SsGpuText::write_cell(int col, int row, uint8_t ch, uint8_t at) {
    uint8_t *p = cell_mut(col, row);
    if (!p) {
        return;
    }
    p[0] = ch;
    p[1] = at;
}

void SsGpuText::fill_cell(int col, int row) {
    write_cell(col, row, 0x20, attr);
}

void SsGpuText::fill_rect(int x0, int y0, int x1, int y1) {
    if (x0 > x1 || y0 > y1) {
        return;
    }
    for (int row = y0; row <= y1; row++) {
        for (int col = x0; col <= x1; col++) {
            fill_cell(col, row);
        }
    }
}

void SsGpuText::copy_span(int dst_x, int dst_y, int src_x, int src_y, int ncells) {
    if (ncells <= 0) {
        return;
    }
    uint8_t *dst = cell_mut(dst_x, dst_y);
    uint8_t *src = cell_mut(src_x, src_y);
    if (!dst || !src) {
        return;
    }
    memmove(dst, src, (size_t)ncells * 2u);
}

void SsGpuText::putc_cell(uint8_t ch, uint8_t at) {
    write_cell((int)x, (int)y, ch, at);
    if (x < right) {
        x++;
    }
}

void SsGpuText::set_x(uint8_t col) {
    const int cols = (int)raster_.cols;
    if (cols <= 0) {
        return;
    }
    if (col >= (uint8_t)cols) {
        col = (uint8_t)(cols - 1);
    }
    if (col > right) {
        col = right;
    }
    x = col;
}

void SsGpuText::set_y(uint8_t row) {
    const int rows = (int)raster_.vis_rows;
    if (rows <= 0) {
        return;
    }
    if (row >= (uint8_t)rows) {
        row = (uint8_t)(rows - 1);
    }
    y = row;
}

void SsGpuText::try_set_left(uint8_t col) {
    if (col >= raster_.cols || col > right) {
        return;
    }
    left = col;
}

void SsGpuText::try_set_right(uint8_t col) {
    if (col >= raster_.cols || left > col) {
        return;
    }
    right = col;
}

void SsGpuText::try_set_top(uint8_t row) {
    if (row >= raster_.vis_rows || row > bottom) {
        return;
    }
    top = row;
}

void SsGpuText::try_set_bottom(uint8_t row) {
    if (row >= raster_.vis_rows || top > row) {
        return;
    }
    bottom = row;
}

void SsGpuText::scroll_up(uint8_t n) {
    n = count_arg(n);
    const int h = (int)bottom - (int)top + 1;
    const int w = (int)right - (int)left + 1;
    if (h <= 0 || w <= 0) {
        return;
    }
    if (n > (uint8_t)h) {
        n = (uint8_t)h;
    }
    for (uint8_t i = 0; i < n; i++) {
        for (int row = (int)top; row < (int)bottom; row++) {
            copy_span((int)left, row, (int)left, row + 1, w);
        }
        for (int col = (int)left; col <= (int)right; col++) {
            fill_cell(col, (int)bottom);
        }
    }
}

void SsGpuText::scroll_down(uint8_t n) {
    n = count_arg(n);
    const int h = (int)bottom - (int)top + 1;
    const int w = (int)right - (int)left + 1;
    if (h <= 0 || w <= 0) {
        return;
    }
    if (n > (uint8_t)h) {
        n = (uint8_t)h;
    }
    for (uint8_t i = 0; i < n; i++) {
        for (int row = (int)bottom; row > (int)top; row--) {
            copy_span((int)left, row, (int)left, row - 1, w);
        }
        for (int col = (int)left; col <= (int)right; col++) {
            fill_cell(col, (int)top);
        }
    }
}

void SsGpuText::insert_line(uint8_t n) {
    n = count_arg(n);
    if (y < top || y > bottom) {
        return;
    }
    const int w = (int)right - (int)left + 1;
    const int avail = (int)bottom - (int)y + 1;
    if (w <= 0 || avail <= 0) {
        return;
    }
    if ((int)n >= avail) {
        fill_rect((int)left, (int)y, (int)right, (int)bottom);
        return;
    }
    for (int row = (int)bottom; row >= (int)y + (int)n; row--) {
        copy_span((int)left, row, (int)left, row - (int)n, w);
    }
    fill_rect((int)left, (int)y, (int)right, (int)y + (int)n - 1);
}

void SsGpuText::delete_line(uint8_t n) {
    n = count_arg(n);
    if (y < top || y > bottom) {
        return;
    }
    const int w = (int)right - (int)left + 1;
    const int avail = (int)bottom - (int)y + 1;
    if (w <= 0 || avail <= 0) {
        return;
    }
    if ((int)n >= avail) {
        fill_rect((int)left, (int)y, (int)right, (int)bottom);
        return;
    }
    for (int row = (int)y; row <= (int)bottom - (int)n; row++) {
        copy_span((int)left, row, (int)left, row + (int)n, w);
    }
    fill_rect((int)left, (int)bottom - (int)n + 1, (int)right, (int)bottom);
}

void SsGpuText::insert_char(uint8_t n) {
    n = count_arg(n);
    if (y < top || y > bottom || x < left || x > right) {
        return;
    }
    const int avail = (int)right - (int)x + 1;
    if (avail <= 0) {
        return;
    }
    if ((int)n >= avail) {
        fill_rect((int)x, (int)y, (int)right, (int)y);
        return;
    }
    copy_span((int)x + (int)n, (int)y, (int)x, (int)y, avail - (int)n);
    fill_rect((int)x, (int)y, (int)x + (int)n - 1, (int)y);
}

void SsGpuText::delete_char(uint8_t n) {
    n = count_arg(n);
    if (y < top || y > bottom || x < left || x > right) {
        return;
    }
    const int avail = (int)right - (int)x + 1;
    if (avail <= 0) {
        return;
    }
    if ((int)n >= avail) {
        fill_rect((int)x, (int)y, (int)right, (int)y);
        return;
    }
    copy_span((int)x, (int)y, (int)x + (int)n, (int)y, avail - (int)n);
    fill_rect((int)right - (int)n + 1, (int)y, (int)right, (int)y);
}

void SsGpuText::erase(uint8_t sub) {
    const int cols = (int)raster_.cols;
    const int rows = (int)raster_.vis_rows;
    int L = (int)left;
    int R = (int)right;
    int T = (int)top;
    int B = (int)bottom;
    if (sub == 0x06) {
        L = 0;
        R = cols - 1;
        T = 0;
        B = rows - 1;
    }
    if (L > R || T > B) {
        return;
    }

    const int cx = (int)x;
    const int cy = (int)y;

    switch (sub) {
        case 0x00: /* EL 0 / CLREOL */
            if (cy < T || cy > B) {
                return;
            }
            fill_rect(cx < L ? L : cx, cy, R, cy);
            break;
        case 0x01: /* EL 1 */
            if (cy < T || cy > B) {
                return;
            }
            fill_rect(L, cy, cx > R ? R : cx, cy);
            break;
        case 0x02: /* EL 2 */
            if (cy < T || cy > B) {
                return;
            }
            fill_rect(L, cy, R, cy);
            break;
        case 0x03: /* ED 0 / CLREOP */
            if (cy < T) {
                fill_rect(L, T, R, B);
                return;
            }
            if (cy > B) {
                return;
            }
            fill_rect(cx < L ? L : cx, cy, R, cy);
            if (cy < B) {
                fill_rect(L, cy + 1, R, B);
            }
            break;
        case 0x04: /* ED 1 */
            if (cy > B) {
                fill_rect(L, T, R, B);
                return;
            }
            if (cy < T) {
                return;
            }
            if (cy > T) {
                fill_rect(L, T, R, cy - 1);
            }
            fill_rect(L, cy, cx > R ? R : cx, cy);
            break;
        case 0x05: /* ED 2 margin */
            fill_rect(L, T, R, B);
            break;
        case 0x06: /* full raster */
            fill_rect(L, T, R, B);
            break;
        case 0x07: /* ECH 1, no advance */
            if (cx >= L && cx <= R && cy >= T && cy <= B) {
                fill_cell(cx, cy);
            }
            break;
        default:
            break;
    }
}

void SsGpuText::execute_pending(uint16_t extra) {
    switch (pending) {
        case Pending::FullCell:
            putc_cell((uint8_t)(extra & 0xFF), (uint8_t)(extra >> 8));
            break;
        case Pending::Repeat: {
            const uint8_t ch = (uint8_t)(extra & 0xFF);
            const uint8_t at = (uint8_t)(extra >> 8);
            const int n = (int)pending_count;
            for (int i = 0; i < n; i++) {
                write_cell((int)x, (int)y, ch, at);
                if (x < right) {
                    x++;
                }
            }
            break;
        }
        case Pending::SetAttrFull:
            attr = (uint8_t)(extra >> 8);
            break;
        case Pending::CursorStyle:
            style = extra;
            break;
        case Pending::None:
            break;
    }
    pending = Pending::None;
}

void SsGpuText::execute_command(uint8_t op, uint8_t arg) {
    switch (op) {
        case 0x80:
            pending = Pending::FullCell;
            break;
        case 0x81:
            pending = Pending::Repeat;
            pending_count = count_arg(arg);
            break;
        case 0x82:
            set_x(arg);
            break;
        case 0x83:
            set_y(arg);
            break;
        case 0x84:
            scroll_up(arg);
            break;
        case 0x85:
            scroll_down(arg);
            break;
        case 0x86:
            insert_line(arg);
            break;
        case 0x87:
            delete_line(arg);
            break;
        case 0x88:
            insert_char(arg);
            break;
        case 0x89:
            delete_char(arg);
            break;
        case 0x8A:
            erase(arg);
            break;
        case 0x8B:
            attr = (uint8_t)(arg & 0x7F);
            break;
        case 0x8C:
            pending = Pending::SetAttrFull;
            break;
        case 0x8D:
            try_set_left(arg);
            break;
        case 0x8E:
            try_set_right(arg);
            break;
        case 0x8F:
            try_set_top(arg);
            break;
        case 0x90:
            try_set_bottom(arg);
            break;
        case 0x91:
            pending = Pending::CursorStyle;
            break;
        case 0x92: {
            const Sync keep = sync;
            reset_pen_and_margins();
            erase_full_raster();
            sync = keep;
            break;
        }
        case 0x93:
            if (arg == 0x00) {
                sync = Sync::Immediate;
            } else if (arg == 0x01) {
                sync = Sync::Vbl;
            }
            break;
        case 0x95: {
            const bool was = yielded_;
            if (arg == 0x00) {
                yielded_ = true;
            } else if (arg == 0x01) {
                yielded_ = false;
            }
            std::printf("SecondSight: GPUText $95 arg=%02X yielded %d->%d\n",
                arg, was ? 1 : 0, yielded_ ? 1 : 0);
            std::fflush(stdout);
            break;
        }
        default:
            /* $94 ReadCells and unused $96–$FF: ignore this word */
            break;
    }
}

void SsGpuText::execute_word(uint16_t w) {
    if (pending != Pending::None) {
        execute_pending(w);
        return;
    }
    if ((w & 0x8000) == 0) {
        putc_cell((uint8_t)(w & 0xFF), (uint8_t)(w >> 8));
        return;
    }
    execute_command((uint8_t)(w >> 8), (uint8_t)(w & 0xFF));
}

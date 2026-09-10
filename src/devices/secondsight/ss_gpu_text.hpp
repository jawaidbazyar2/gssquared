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
#include <vector>

#include "ss_host_text.hpp"

/** Second Sight GPU Text (SetMode flag $05). Spec: Docs/SecondSight_GPUText.md v0.1. */
class SsGpuText {
public:
    static constexpr int RING_WORDS = 8192;
    static constexpr uint16_t CURSOR_RESET = 0x0003;
    static constexpr uint8_t ATTR_RESET = 0x07;

    enum class Sync : uint8_t {
        Immediate = 0,
        Vbl = 1,
    };

    bool enter(uint8_t mode_num);
    void leave();
    bool is_active() const { return active; }

    /** One FIFO byte (C0B1 or C0B2). Completes a little-endian word every two bytes. */
    void push_byte(uint8_t b);
    /** Test helper: enqueue a complete word (does not drain). */
    void enqueue_word(uint16_t w);
    /** Execute every queued word. */
    void drain();

    bool ring_empty() const { return ring_count == 0; }
    int queued_words() const { return ring_count; }
    bool pending_extra() const { return pending != Pending::None; }
    bool half_word_pending() const { return have_lo; }

    bool sync_immediate() const { return sync == Sync::Immediate; }
    Sync sync_mode() const { return sync; }
    /** $95 Yield: true while scanout is Mega II; VRAM / ring stay armed. */
    bool yielded() const { return yielded_; }

    const uint8_t *cells() const { return cells_.empty() ? nullptr : cells_.data(); }
    uint8_t *cells() { return cells_.empty() ? nullptr : cells_.data(); }
    int cell_pitch() const { return (int)raster_.cols * 2; }
    const ss_host_text_raster_t &raster() const { return raster_; }

    uint8_t cursor_x() const { return x; }
    uint8_t cursor_y() const { return y; }
    uint16_t cursor_style() const { return style; }
    uint8_t current_attr() const { return attr; }
    uint8_t margin_left() const { return left; }
    uint8_t margin_right() const { return right; }
    uint8_t margin_top() const { return top; }
    uint8_t margin_bottom() const { return bottom; }

    /** Cell at (col,row): char in [0], attr in [1]. nullptr if OOB. */
    const uint8_t *cell_at(int col, int row) const;

private:
    enum class Pending : uint8_t {
        None = 0,
        FullCell,
        Repeat,
        SetAttrFull,
        CursorStyle,
    };

    void reset_pen_and_margins();
    void erase_full_raster();
    void execute_word(uint16_t w);
    void execute_command(uint8_t op, uint8_t arg);
    void execute_pending(uint16_t extra);
    void putc_cell(uint8_t ch, uint8_t at);
    void write_cell(int col, int row, uint8_t ch, uint8_t at);
    void fill_cell(int col, int row);
    void fill_rect(int x0, int y0, int x1, int y1);
    void set_x(uint8_t col);
    void set_y(uint8_t row);
    void try_set_left(uint8_t col);
    void try_set_right(uint8_t col);
    void try_set_top(uint8_t row);
    void try_set_bottom(uint8_t row);
    void scroll_up(uint8_t n);
    void scroll_down(uint8_t n);
    void insert_line(uint8_t n);
    void delete_line(uint8_t n);
    void insert_char(uint8_t n);
    void delete_char(uint8_t n);
    void erase(uint8_t sub);
    void copy_span(int dst_x, int dst_y, int src_x, int src_y, int ncells);
    uint8_t *cell_mut(int col, int row);
    static uint8_t count_arg(uint8_t a) { return a == 0 ? 1 : a; }

    bool active = false;
    bool yielded_ = false;
    ss_host_text_raster_t raster_{};
    std::vector<uint8_t> cells_;
    uint16_t ring[RING_WORDS] = {};
    int ring_head = 0;
    int ring_tail = 0;
    int ring_count = 0;
    bool have_lo = false;
    uint8_t lo_byte = 0;
    Pending pending = Pending::None;
    uint8_t pending_count = 1;
    Sync sync = Sync::Immediate;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t left = 0;
    uint8_t right = 79;
    uint8_t top = 0;
    uint8_t bottom = 24;
    uint8_t attr = ATTR_RESET;
    uint16_t style = CURSOR_RESET;
};

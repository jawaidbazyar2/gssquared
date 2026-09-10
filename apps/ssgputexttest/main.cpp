/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   SsGpuText ISA self-test (SecondSight_GPUText.md v0.1).
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "devices/secondsight/ss_gpu_text.hpp"

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL: %s\n", msg); \
            return false; \
        } \
    } while (0)

static uint16_t putc_word(uint8_t ch, uint8_t at) {
    return (uint16_t)ch | ((uint16_t)(at & 0x7F) << 8);
}

static uint16_t cmd(uint8_t op, uint8_t arg) {
    return (uint16_t)arg | ((uint16_t)op << 8);
}

static void feed(SsGpuText &t, uint16_t w) {
    t.enqueue_word(w);
    t.drain();
}

static bool cell_is(const SsGpuText &t, int x, int y, uint8_t ch, uint8_t at) {
    const uint8_t *c = t.cell_at(x, y);
    return c && c[0] == ch && c[1] == at;
}

static bool test_enter_rasters() {
    SsGpuText t;
    CHECK(!t.enter(0x01), "40x25 must be rejected");
    CHECK(t.enter(0x03), "80x25 enter");
    CHECK(t.raster().cols == 80 && t.raster().vis_rows == 25, "80x25 size");
    CHECK(t.cell_at(0, 0) != nullptr, "buffer allocated");
    CHECK(!(t.cell_at(0, 0)[0] == 0x20 && t.cell_at(0, 0)[1] == 0x07),
        "SetMode is not Reset");
    CHECK(t.current_attr() == 0x07, "default attr");
    CHECK(t.cursor_style() == 0x0003, "default cursor");
    CHECK(t.sync_immediate(), "default immediate");
    CHECK(t.enter(0x43), "80x43 enter");
    CHECK(t.raster().cols == 80 && t.raster().vis_rows == 43, "80x43 size");
    CHECK(t.cell_at(0, 42) != nullptr, "80x43 last row");
    CHECK(t.cell_at(0, 43) == nullptr, "80x43 OOB row");
    t.leave();
    CHECK(!t.is_active(), "leave");
    return true;
}

static bool test_putc_and_clamp() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, putc_word('A', 0x07));
    CHECK(cell_is(t, 0, 0, 'A', 0x07), "putc A");
    CHECK(t.cursor_x() == 1 && t.cursor_y() == 0, "advance");
    feed(t, cmd(0x82, 79));
    feed(t, putc_word('Z', 0x1F));
    CHECK(cell_is(t, 79, 0, 'Z', 0x1F), "rightmost");
    CHECK(t.cursor_x() == 79, "clamp at right");
    feed(t, putc_word('Y', 0x1F));
    CHECK(cell_is(t, 79, 0, 'Y', 0x1F), "stay clamped");
    CHECK(t.cursor_x() == 79, "still clamped");
    return true;
}

static bool test_reset_keeps_sync() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x93, 0x01));
    CHECK(!t.sync_immediate(), "VBL");
    feed(t, putc_word('Q', 0x70));
    feed(t, cmd(0x92, 0));
    CHECK(cell_is(t, 0, 0, 0x20, 0x07), "Reset erase");
    CHECK(t.cursor_x() == 0 && t.cursor_y() == 0, "Reset home");
    CHECK(t.current_attr() == 0x07, "Reset attr");
    CHECK(t.cursor_style() == 0x0003, "Reset cursor");
    CHECK(t.margin_left() == 0 && t.margin_right() == 79, "Reset margins x");
    CHECK(t.margin_top() == 0 && t.margin_bottom() == 24, "Reset margins y");
    CHECK(!t.sync_immediate(), "Reset does not change SetSync");
    return true;
}

static bool test_scroll_and_erase() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x92, 0));
    feed(t, cmd(0x8B, 0x1F));
    feed(t, putc_word('A', 0x07));
    feed(t, cmd(0x83, 1));
    feed(t, cmd(0x82, 0));
    feed(t, putc_word('B', 0x07));
    feed(t, cmd(0x84, 0)); /* count 0 => 1 */
    CHECK(cell_is(t, 0, 0, 'B', 0x07), "ScrollUp moved B");
    CHECK(cell_is(t, 0, 24, 0x20, 0x1F), "ScrollUp fill current attr");
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x83, 0));
    feed(t, cmd(0x8A, 0x00));
    CHECK(cell_is(t, 0, 0, 0x20, 0x1F), "CLREOL");
    feed(t, cmd(0x8A, 0x05));
    CHECK(cell_is(t, 40, 12, 0x20, 0x1F), "Erase margin");
    return true;
}

static bool test_fullcell_and_reserved() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x80, 0));
    feed(t, (uint16_t)'X' | (uint16_t)0x8700); /* blink + bright bg */
    CHECK(cell_is(t, 0, 0, 'X', 0x87), "FullCell quoted attr");
    CHECK(t.cursor_x() == 1, "FullCell advance");
    const uint8_t before = t.cursor_x();
    feed(t, cmd(0x94, 5)); /* ReadCells: NOP one word */
    CHECK(t.cursor_x() == before, "ReadCells ignored");
    CHECK(t.cell_at(1, 0) && t.cell_at(1, 0)[0] == 0, "no extra putc");
    feed(t, cmd(0xFF, 0x55));
    CHECK(t.cursor_x() == before, "reserved ignore one word");
    return true;
}

static bool test_il_dl_ich() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x92, 0));
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x83, 0));
    feed(t, putc_word('A', 0x07));
    feed(t, cmd(0x83, 1));
    feed(t, cmd(0x82, 0));
    feed(t, putc_word('B', 0x07));
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x83, 0));
    feed(t, cmd(0x86, 1)); /* IL */
    CHECK(cell_is(t, 0, 0, 0x20, 0x07), "IL fill at cursor row");
    CHECK(cell_is(t, 0, 1, 'A', 0x07), "IL shifted A down");
    CHECK(cell_is(t, 0, 2, 'B', 0x07), "IL shifted B down");
    feed(t, cmd(0x87, 1)); /* DL */
    CHECK(cell_is(t, 0, 0, 'A', 0x07), "DL pulled A up");
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x83, 0));
    feed(t, putc_word('1', 0x07));
    feed(t, putc_word('2', 0x07));
    feed(t, putc_word('3', 0x07));
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x88, 1)); /* ICH */
    CHECK(cell_is(t, 0, 0, 0x20, 0x07), "ICH space");
    CHECK(cell_is(t, 1, 0, '1', 0x07), "ICH shift");
    return true;
}

static bool test_invalid_margins() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x8D, 10));
    feed(t, cmd(0x8E, 20));
    CHECK(t.margin_left() == 10 && t.margin_right() == 20, "set margins");
    feed(t, cmd(0x8D, 25)); /* left > right */
    CHECK(t.margin_left() == 10, "invalid left ignored");
    feed(t, cmd(0x8E, 5)); /* right < left */
    CHECK(t.margin_right() == 20, "invalid right ignored");
    return true;
}

static bool test_byte_fifo_and_vbl() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    t.push_byte('H');
    t.push_byte(0x07);
    CHECK(cell_is(t, 0, 0, 'H', 0x07), "immediate drain after word");
    CHECK(t.ring_empty(), "ring empty immediate");
    feed(t, cmd(0x93, 1));
    t.push_byte('V');
    t.push_byte(0x07);
    CHECK(t.queued_words() == 1, "VBL queues");
    CHECK(!cell_is(t, 1, 0, 'V', 0x07), "not applied yet");
    t.drain();
    CHECK(cell_is(t, 1, 0, 'V', 0x07), "VBL drain");
    CHECK(t.ring_empty(), "empty after drain");
    return true;
}

static bool test_setxy() {
    SsGpuText t;
    CHECK(t.enter(0x43), "43");
    feed(t, cmd(0x82, 200));
    CHECK(t.cursor_x() == 79, "SetX clip raster then right");
    feed(t, cmd(0x83, 50));
    CHECK(t.cursor_y() == 42, "SetY clip raster");
    return true;
}

static bool test_setmode_keeps_page() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    feed(t, cmd(0x92, 0));
    feed(t, putc_word('K', 0x1F));
    feed(t, cmd(0x82, 3));
    feed(t, cmd(0x83, 2));
    feed(t, cmd(0x8B, 0x70));
    CHECK(t.enter(0x03), "re-SetMode same raster");
    CHECK(cell_is(t, 0, 0, 'K', 0x1F), "VRAM kept");
    CHECK(t.cursor_x() == 3 && t.cursor_y() == 2, "cursor kept");
    CHECK(t.current_attr() == 0x70, "attr kept");
    CHECK(t.enter(0x43), "raster change");
    CHECK(t.raster().vis_rows == 43, "80x43");
    CHECK(t.cell_at(0, 0) != nullptr, "new buffer");
    CHECK(t.cursor_x() == 0 && t.cursor_y() == 0, "new raster resets pen");
    return true;
}

static bool test_yield_keeps_page() {
    SsGpuText t;
    CHECK(t.enter(0x03), "enter");
    CHECK(!t.yielded(), "claim on enter");
    feed(t, cmd(0x92, 0));
    feed(t, putc_word('A', 0x1F));
    feed(t, cmd(0x82, 0));
    feed(t, cmd(0x83, 1));
    feed(t, putc_word('B', 0x70));
    feed(t, cmd(0x95, 0)); /* Yield */
    CHECK(t.yielded(), "yield");
    CHECK(cell_is(t, 0, 0, 'A', 0x1F), "yield keeps row0");
    CHECK(cell_is(t, 0, 1, 'B', 0x70), "yield keeps row1");
    CHECK(t.cursor_x() == 1 && t.cursor_y() == 1, "yield keeps cursor");
    feed(t, cmd(0x95, 2)); /* reserved arg */
    CHECK(t.yielded(), "bad $95 arg ignored");
    feed(t, cmd(0x95, 1)); /* Claim */
    CHECK(!t.yielded(), "claim");
    CHECK(cell_is(t, 0, 0, 'A', 0x1F), "claim same page");
    CHECK(cell_is(t, 0, 1, 'B', 0x70), "claim no replay needed");
    return true;
}

int main() {
    struct {
        const char *name;
        bool (*fn)();
    } tests[] = {
        {"enter_rasters", test_enter_rasters},
        {"putc_and_clamp", test_putc_and_clamp},
        {"reset_keeps_sync", test_reset_keeps_sync},
        {"scroll_and_erase", test_scroll_and_erase},
        {"fullcell_and_reserved", test_fullcell_and_reserved},
        {"il_dl_ich", test_il_dl_ich},
        {"invalid_margins", test_invalid_margins},
        {"byte_fifo_and_vbl", test_byte_fifo_and_vbl},
        {"setxy_43", test_setxy},
        {"setmode_keeps_page", test_setmode_keeps_page},
        {"yield_keeps_page", test_yield_keeps_page},
    };
    int failed = 0;
    for (auto &tc : tests) {
        if (!tc.fn()) {
            std::fprintf(stderr, "  in %s\n", tc.name);
            failed++;
        }
    }
    if (failed) {
        std::fprintf(stderr, "%d test(s) failed\n", failed);
        return EXIT_FAILURE;
    }
    std::printf("ssgputexttest: %zu tests ok\n", sizeof(tests) / sizeof(tests[0]));
    return EXIT_SUCCESS;
}

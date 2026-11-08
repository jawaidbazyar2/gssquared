/*
 *   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright holders: Nathan Woods, Peter Trauner, Angelo Salese.
 *   See secondsight-video.LICENSE for the BSD-3-Clause terms.
 *
 *   Turn Second Sight's Oak OTI-087X video RAM (VRAM) into a host pixel image.
 *   The CRTC (cathode-ray-tube controller) supplies the starting address, row
 *   stride and screen dimensions. Text fetches characters and font rows; planar
 *   modes combine bits from four separate memory planes; packed modes fetch
 *   consecutive pixel bytes. The Brooktree Bt481 palette/converter supplies
 *   RGB (red, green, blue) colors. Scanout means these display-memory reads.
 *
 *   SecondSightDisplay calls the renderer on the main thread while SSCard
 *   has stopped card execution. OTI087 owns memory, registers and cached rows.
 *   Callers own the destination surface. The output is opaque ARGB8888: eight bits each for
 *   alpha, red, green and blue. Rendering never advances the processor clock.
 *   A display-address latch retains the address until its scheduled load;
 *   recorded changes split the image into rows using the appropriate address.
 *
 *   Sources: Oak OTI-087 February 1993 databook pp. 21, 39-46 and OTI-087X addendum;
 *   IBM VGA/XGA Technical Reference May 1992 pp. 2-72 to 2-95 (address generation
 *   and palettes); Brooktree Bt481A L481A_C pp. 7-8 (direct-color bit positions).
 *   Video analysis: https://github.com/Webifi/second-sight-reference,
 *   docs/06-video.md, docs/15-oak-drawing.md and
 *   docs/reference/oti087-registers.md. The analyzed Oak Windows cursor driver
 *   is STD8_HC.DRV, SHA-256
 *   523f60c849db7f865fce075e80a890aa1f5dbc3fb243d5b0c7e950525f6d5fa9.
 *   SIMD means one host instruction handles several pixels; scalar fallbacks
 *   apply the same conversion one pixel at a time.
 */

#include "oti087.hpp"
#include "vga_render_text_9x16.hpp"
#include <cstring>
#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace {
// Extract bit n as zero or one when decoding a documented register field.
constexpr unsigned bit(uint32_t value, unsigned n) {
    return (value >> n) & 1;
}
// Two cursor bits choose color 0, color 1, leave the screen alone, or invert
// its palette index. Oak's Windows cursor driver copies AND/XOR masks into
// these planes (second-sight-reference docs/06-video.md, "Hardware cursor").
enum class CursorOp { color0, color1, transparent, invert };
constexpr CursorOp cursor_codes[] = {CursorOp::color0, CursorOp::color1, CursorOp::transparent, CursorOp::invert};

// VRAM retains four 64K planes per 256K block. Each pan alignment selects
// four constant plane offsets, including the next column after lane three.
// Thus a one-pixel pan uses the same vector loop as an aligned scanline.
// Split at plane-storage boundaries; callers bound the populated memory.
template<unsigned Lane>
void packed_bytes_lanes(uint8_t *dest, uint8_t const *memory, uint32_t address, unsigned count) {
    auto byte = [&](uint32_t a) { return memory[(a & 0xc0000) | ((a & 3) << 16) | ((a >> 2) & 0xffff)]; };
    constexpr unsigned a0 = (Lane & 3) * 0x10000;
    constexpr unsigned a1 = ((Lane + 1) & 3) * 0x10000 + ((Lane + 1) >> 2);
    constexpr unsigned a2 = ((Lane + 2) & 3) * 0x10000 + ((Lane + 2) >> 2);
    constexpr unsigned a3 = ((Lane + 3) & 3) * 0x10000 + ((Lane + 3) >> 2);
    // Limit each group to the current 256 KiB block before forming plane
    // pointers; the next iteration recalculates them across the wrap.
    while (count >= 4) {
        unsigned groups = std::min(count / 4, (0x40000 - (address & 0x3ffff)) / 4);
        if constexpr (Lane != 0) {
            if (!groups) {
                // Finish this storage block, then continue at aligned lane 0.
                unsigned const tail = 0x40000 - (address & 0x3ffff);
                for (unsigned i = 0; i < tail; ++i) *dest++ = byte(address++);
                packed_bytes_lanes<0>(dest, memory, address, count - tail);
                return;
            }
        }
        uint8_t const *p = memory + (address & 0xc0000) + ((address >> 2) & 0xffff);
        unsigned n = groups;
#if defined(__wasm_simd128__)
        while (n >= 16) {
            auto a = wasm_v128_load(p + a0), b = wasm_v128_load(p + a1);
            auto c = wasm_v128_load(p + a2), d = wasm_v128_load(p + a3);
            auto ab0 = wasm_i8x16_shuffle(a,b,0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23);
            auto ab1 = wasm_i8x16_shuffle(a,b,8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31);
            auto cd0 = wasm_i8x16_shuffle(c,d,0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23);
            auto cd1 = wasm_i8x16_shuffle(c,d,8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31);
            wasm_v128_store(dest, wasm_i16x8_shuffle(ab0,cd0,0,8,1,9,2,10,3,11));
            wasm_v128_store(dest+16, wasm_i16x8_shuffle(ab0,cd0,4,12,5,13,6,14,7,15));
            wasm_v128_store(dest+32, wasm_i16x8_shuffle(ab1,cd1,0,8,1,9,2,10,3,11));
            wasm_v128_store(dest+48, wasm_i16x8_shuffle(ab1,cd1,4,12,5,13,6,14,7,15));
            p += 16; dest += 64; n -= 16;
        }
#elif defined(__ARM_NEON)
        while (n >= 16) {
            uint8x16x4_t lanes = {{vld1q_u8(p + a0), vld1q_u8(p + a1), vld1q_u8(p + a2), vld1q_u8(p + a3)}};
            vst4q_u8(dest, lanes);
            p += 16;
            dest += 64;
            n -= 16;
        }
#endif
        while (n--) {
            dest[0] = p[a0];
            dest[1] = p[a1];
            dest[2] = p[a2];
            dest[3] = p[a3];
            ++p;
            dest += 4;
        }
        count -= groups * 4;
        address += groups * 4;
    }
    while (count--)
        *dest++ = byte(address++);
}

void packed_bytes(uint8_t *dest, uint8_t const *memory, uint32_t address, unsigned count) {
    switch (address & 3) {
        case 0: packed_bytes_lanes<0>(dest, memory, address, count); break;
        case 1: packed_bytes_lanes<1>(dest, memory, address, count); break;
        case 2: packed_bytes_lanes<2>(dest, memory, address, count); break;
        case 3: packed_bytes_lanes<3>(dest, memory, address, count); break;
    }
}

// RASHn selects an unpopulated megabyte on this card. Use fixed $FF bytes
// there, including a row that crosses the boundary; never mirror RASLn.
// See second-sight-reference docs/15-oak-drawing.md, "Hardware scrolling".
void packed_bytes_bounded(uint8_t *dest, uint8_t const *memory, uint32_t address, unsigned count) {
    unsigned const valid = address < OTI087::VRAM_SIZE ? std::min(count, OTI087::VRAM_SIZE - address) : 0;
    packed_bytes(dest, memory, address, valid);
    std::fill(dest + valid, dest + count, 0xff);
}

// Expand packed pixels with exactly the Bt481 channel bit placement; vector
// and scalar paths must leave unused low color bits zero (L481A_C pp. 7-8).
// Convert count pixels from source bytes to dest ARGB words. palette has
// 256 resolved Bt481 colors for indexed mode; direct modes use command bits.
void packed_colors(uint32_t *dest, uint8_t const *source, unsigned count, Bt481 const &dac, uint32_t const *palette) {
    // Sleep outputs black. Indexed bytes use the already masked palette;
    // direct color decodes two/three/four input bytes for each output pixel.
    unsigned const bytes = dac.bytes_per_pixel();
    uint8_t const command = dac.command_r();
    if (dac.asleep()) {
        std::fill_n(dest, count, 0xff000000);
        return;
    }
    if (bytes == 1) {
        while (count >= 4) {
            dest[0] = palette[source[0]];
            dest[1] = palette[source[1]];
            dest[2] = palette[source[2]];
            dest[3] = palette[source[3]];
            source += 4;
            dest += 4;
            count -= 4;
        }
        while (count--)
            *dest++ = palette[*source++];
        return;
    }
#if defined(__wasm_simd128__)
    if (bytes == 2) {
        auto colors = [command](v128_t pixels) {
            auto blue = wasm_v128_and(wasm_i32x4_shl(pixels,3), wasm_i32x4_splat(0xf8));
            auto green = wasm_v128_and(wasm_i32x4_shl(pixels, command & 0x40 ? 5 : 6),
                                      wasm_i32x4_splat(command & 0x40 ? 0xfc00 : 0xf800));
            auto red = wasm_v128_and(wasm_i32x4_shl(pixels, command & 0x40 ? 8 : 9), wasm_i32x4_splat(0xf80000));
            return wasm_v128_or(wasm_i32x4_splat(0xff000000), wasm_v128_or(red,wasm_v128_or(green,blue)));
        };
        while (count >= 8) {
            auto pixels = wasm_v128_load(source);
            wasm_v128_store(dest, colors(wasm_u32x4_extend_low_u16x8(pixels)));
            wasm_v128_store(dest+4, colors(wasm_u32x4_extend_high_u16x8(pixels)));
            source += 16; dest += 8; count -= 8;
        }
    } else if (bytes == 3) {
        // A load covers 16 bytes; keep at least eight pixels before loading
        // twelve bytes of output, so the final source row is never overread.
        const auto order = command & 2 ? wasm_i8x16_make(0,1,2,16,3,4,5,16,6,7,8,16,9,10,11,16)
                                      : wasm_i8x16_make(2,1,0,16,5,4,3,16,8,7,6,16,11,10,9,16);
        while (count >= 8) {
            auto pixels = wasm_i8x16_swizzle(wasm_v128_load(source),order);
            wasm_v128_store(dest, wasm_v128_or(pixels,wasm_i32x4_splat(0xff000000)));
            source += 12; dest += 4; count -= 4;
        }
    }
#elif defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (bytes == 2) {
        while (count >= 8) {
            uint16x8_t pixels = vreinterpretq_u16_u8(vld1q_u8(source));
            uint8x8_t red = command & 0x40 ? vshrn_n_u16(pixels, 8) : vshrn_n_u16(pixels, 7);
            uint8x8_t green = command & 0x40 ? vshrn_n_u16(pixels, 3) : vshrn_n_u16(pixels, 2);
            uint8x8x4_t rgb = {{vand_u8(vshl_n_u8(vmovn_u16(pixels), 3), vdup_n_u8(0xf8)),
                                vand_u8(green, vdup_n_u8(command & 0x40 ? 0xfc : 0xf8)), vand_u8(red, vdup_n_u8(0xf8)),
                                vdup_n_u8(0xff)}};
            vst4_u8(reinterpret_cast<uint8_t *>(dest), rgb);
            source += 16;
            dest += 8;
            count -= 8;
        }
    } else if (bytes == 3) {
        while (count >= 16) {
            uint8x16x3_t pixels = vld3q_u8(source);
            uint8x16x4_t rgb = {
                {pixels.val[command & 2 ? 0 : 2], pixels.val[1], pixels.val[command & 2 ? 2 : 0], vdupq_n_u8(0xff)}};
            vst4q_u8(reinterpret_cast<uint8_t *>(dest), rgb);
            source += 48;
            dest += 16;
            count -= 16;
        }
    }
#endif
    while (count--) {
        if (bytes == 2) {
            uint16_t pixel = source[0] | (uint16_t(source[1]) << 8);
            *dest++ = command & 0x40 ? VideoColor((pixel >> 8) & 0xf8, (pixel >> 3) & 0xfc, (pixel << 3) & 0xf8)
                                     : VideoColor((pixel >> 7) & 0xf8, (pixel >> 2) & 0xf8, (pixel << 3) & 0xf8);
        } else if (bytes == 3) {
            *dest++ =
                command & 2 ? VideoColor(source[2], source[1], source[0]) : VideoColor(source[0], source[1], source[2]);
        } else {
            uint32_t pixel =
                source[0] | (uint32_t(source[1]) << 8) | (uint32_t(source[2]) << 16) | (uint32_t(source[3]) << 24);
            *dest++ = dac.direct_color(pixel);
        }
        source += bytes;
    }
}
} // namespace

// Freeze display controls and the test-input byte for later raster replay.
// Memory and beam timing remain owned by the live controller.
OTI087::display_registers_t OTI087::display_registers() const {
    display_registers_t state{sequencer, crtc, gc, attribute, {}, ramdac, setup, enable,
                              uint8_t(scan_test() && system_data ? *system_data : 0xff)};
    std::copy(std::begin(ext), std::end(ext), state.ext.begin());
    return state;
}

// Select controls for a saved span; render_changes restores the live copy.
// Rebuild derived colors because the saved palette may differ from the last span.
void OTI087::display_registers(const display_registers_t &state) {
    sequencer = state.sequencer;
    crtc = state.crtc;
    gc = state.gc;
    attribute = state.attribute;
    std::copy(state.ext.begin(), state.ext.end(), std::begin(ext));
    ramdac = state.dac;
    setup = state.setup;
    enable = state.enable;
    test_data = state.test_data;
    rendering_snapshot = true;
    packed_scanout = packed() && !scan_test();
    render_colors_valid = false;
}

// Preserve the color/control state up to this pixel, not just this line.
// No memory write logging or scanline timer is needed. Palette staging
// writes do not reach here until the complete RGB triplet is committed.
void OTI087::record_display_change() {
    int const position = vpos() * timing.total_width + hpos();
    // At frame origin there are no earlier pixels to preserve. Several writes
    // at one position need only the first pre-write state; the next saved
    // state, or the final live state, includes all those writes.
    if (position <= 0 || (!display_changes.empty() && position <= display_changes.back().position))
        return;
    display_changes.push_back({position, display_registers()});
}

// IBM p2-75, Figure 2-65. Address-bit selection is common to text and all VGA shift
// modes. Oak packed pixels use their own eight-byte start-address units.
uint32_t OTI087::display_address(uint32_t counter, unsigned row_scan) const {
    uint32_t address = counter;
    if (crtc.dw)
        address = (counter & ~0xffffU) | ((counter << 2) & 0xfffc) | ((counter >> 12) & 3);
    else if (!crtc.word_mode)
        address = (counter & ~0xffffU) | ((counter << 1) & 0xfffe) | bit(counter, crtc.aw ? 15 : 13);
    if (!crtc.map13)
        address = (address & ~0x2000U) | (bit(row_scan, 0) << 13);
    if (!crtc.map14)
        address = (address & ~0x4000U) | (bit(row_scan, 1) << 14);
    return address & sequencer.address_mask;
}

unsigned OTI087::serializer_load_shift() const {
    // IBM Type 1 convention (pp2-11, 2-49/50): load every 1/2/4 character
    // clocks, SH4 taking priority. Oak's planar/word-address register model
    // fits this better than XGA Type 2's Extended Graphics exception.
    // No Oak source identifies its type or suppressed-load output. Model a
    // group of consecutive bytes per map, consumed before the next reload.
    // Oak packed scanout is separate.
    return bit(sequencer.data[1], 4) ? 2 : bit(sequencer.data[1], 2) ? 1 : 0;
}

// Select one byte from the group loaded at this character-clock position.
// count_shift divides the address count; load_shift sets the group size.
uint32_t OTI087::serializer_address(const scan_row_t &row, unsigned character,
                                  unsigned count_shift, unsigned load_shift) const {
    unsigned const lane_mask = (1U << load_shift) - 1;
    uint32_t const counter = row.address + crtc.byte_panning + ((character & ~lane_mask) >> count_shift);
    // Select CRTC address bits when loading a group, then select its byte.
    // Apply the Extended Memory (EM) plane-size mask after both steps.
    // With EM clear, the model clears bits 14/15 in each of four planes
    // to provide 64 KiB total; higher Oak segment bits are retained.
    return (display_address(counter, row.scan) + (character & lane_mask)) & sequencer.address_mask;
}

// Convert palette indices once for all rows using the current register span.
void OTI087::prepare_render_colors() {
    if (render_colors_valid)
        return;
    for (unsigned i = 0; i < render_dac_colors.size(); ++i)
        render_dac_colors[i] = ramdac.pixel_select(i);
    for (unsigned i = 0; i < render_attribute_colors.size(); ++i)
        render_attribute_colors[i] = attribute_color(i);
    render_colors_valid = true;
}

// IBM p2-95 lists different pan units for 9-dot text and 256-color pixels.
unsigned OTI087::horizontal_pan(const scan_row_t &row) const {
    if (row.split && bit(attribute.data[0x10], 5))
        return 0;
    // Packed modes use IBM's eight-dot rule: zero means no shift, then
    // one pixel per step. Oak's BIOS sets pan zero in packed modes
    // (second-sight-reference docs/15-oak-drawing.md, "Horizontal pixel panning").
    if (packed())
        return row.pan & 7;
    if (!gc.alpha_dis && !bit(sequencer.data[1], 0))
        return row.pan == 8 ? 0 : (row.pan & 7) + 1;
    if (gc.shift256 && bit(attribute.data[0x10], 6))
        return (row.pan & 6) >> 1;
    return row.pan & 7;
}

// Constant-register frames can calculate a row directly. During raster
// changes the caller supplies the accumulated start-of-line latch instead.
OTI087::scan_row_t OTI087::scan_row(unsigned y) const {
    if (render_row)
        return *render_row;
    unsigned const fields = get_interlace_mode() ? 2 : 1;
    unsigned const line = y / fields;
    unsigned const split_line = (crtc.line_compare + 1) * vertical_divisor();
    bool const split = line >= split_line;
    unsigned const scan_repeat = crtc.scan_doubling + 1;
    unsigned const scan = (split ? line - split_line : line) / scan_repeat +
                          (split ? 0 : crtc.preset_row_scan);
    unsigned const rows = scan / crtc.maximum_scan_line;
    // Line compare clears the address counter in either field. The odd
    // field's initial offset belongs only to the region before that reset.
    return {(split ? 0 : crtc.start_addr) + (rows * fields + (split ? 0 : y % fields)) * crtc.offset * 2,
            scan % crtc.maximum_scan_line, (split ? line - split_line : line) % scan_repeat, split,
            attribute.pel_shift};
}

// IBM pp2-71/103: add the *current* offset only when the row rolls over.
// A mid-frame write therefore changes following rows without moving old ones.
void OTI087::next_scan_row(scan_row_t &row, unsigned field_line) const {
    if ((field_line + 1) % vertical_divisor() == 0 && field_line / vertical_divisor() == crtc.line_compare) {
        row = {0, 0, 0, true, row.pan};
        return;
    }
    if (++row.repeat <= crtc.scan_doubling)
        return;
    row.repeat = 0;
    if (++row.scan < crtc.maximum_scan_line)
        return;
    row.scan = 0;
    row.address += crtc.offset * (get_interlace_mode() ? 4 : 2);
}

// Replay only register boundaries, while retaining the address accumulator
// across them. A palette split and a pitch split on the same row coexist.
void OTI087::render_changes(ss_video_buffer_t surface) {
    auto live = display_registers();
    unsigned change = 0;
    display_registers(display_changes.front().registers);
    unsigned const fields = get_interlace_mode() ? 2 : 1;
    unsigned const total = timing.total_height / fields;
    scan_row_t row = scan_row(0);
    for (unsigned beam_y = 0; beam_y < unsigned(timing.total_height); ++beam_y) {
        unsigned const field_line = beam_y % total;
        unsigned const y = field_line * fields + beam_y / total;
        int x = 0;
        // The preceding row has already advanced its address. A write at
        // column zero changes this row's controls, not that earlier increment.
        while (change < display_changes.size() &&
               display_changes[change].position <= int(beam_y) * timing.total_width) {
            ++change;
            display_registers(change < display_changes.size() ? display_changes[change].registers : live);
        }
        if (!field_line)
            row = {crtc.start_addr + (beam_y / total) * crtc.offset * 2, crtc.preset_row_scan, 0, false};
        row.pan = attribute.pel_shift;
        render_row = &row;
        while (x < timing.total_width) {
            int end = timing.total_width;
            if (change < display_changes.size())
                end = std::min(end, display_changes[change].position - int(beam_y) * timing.total_width);
            if (end > x && y < unsigned(surface.height) && x < surface.width)
                render(surface, {x, std::min(end, surface.width) - 1, int(y), int(y)});
            x = std::max(x, end);
            if (x == timing.total_width)
                break;
            if (change < display_changes.size() &&
                display_changes[change].position <= int(beam_y) * timing.total_width + x) {
                ++change;
                display_registers(change < display_changes.size() ? display_changes[change].registers : live);
            }
        }
        next_scan_row(row, field_line);
    }
    render_row = nullptr;
    display_registers(live);
    rendering_snapshot = false;
}

// Scan packed pixels using Oak start/offset units and the active Bt481 depth.
void OTI087::draw_packed(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    unsigned const bytes = ramdac.bytes_per_pixel();
    // Start = CRTC start * 8; pitch = CRTC offset * 16, bypassing the VGA
    // word/dword mux. These units are inferred from Oak BIOS settings and
    // XFree86 OAKAdjust (second-sight-reference docs/06-video.md,
    // "Packed-pixel mode and the CRTC", and docs/15-oak-drawing.md,
    // "Display features that cost no drawing").
    packed_row.resize(clip.width() * bytes);
    for (int y = clip.top; y <= clip.bottom; ++y) {
        auto const row = scan_row(y);
        uint32_t const address = row.address * 8 + horizontal_pan(row) * bytes;
        packed_bytes_bounded(packed_row.data(), memory.data(), address + clip.left * bytes, packed_row.size());
        packed_colors(&surface.pixel(y, clip.left), packed_row.data(), clip.width(), ramdac, render_dac_colors.data());
    }
}

// Resolve the VGA attribute palette and color-select bits before DAC lookup.
VideoColor OTI087::attribute_color(uint8_t index) const {
    index &= attribute.data[0x12] & 15;
    uint8_t palette = attribute.data[index];
    if (bit(attribute.data[0x10], 7))
        palette = (palette & 15) | ((attribute.data[0x14] & 3) << 4);
    palette |= (attribute.data[0x14] & 12) << 4;
    return ramdac.pixel_select(palette);
}

// Fetch character, attribute and font-plane bytes at CRTC addresses; apply
// blink and cursor rules before converting the palette index to color.
template<bool Test>
void OTI087::draw_text(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    // Standard VGA character/attribute planes 0/1 and font plane2. Use the
    // same physical VRAM translation as CPU accesses, including odd/even mode.
    unsigned const dots = char_dots();
    unsigned const count_shift = crtc.div4 ? 2 : crtc.div2 ? 1 : 0;
    unsigned const load_shift = serializer_load_shift();
    bool const fonts = bit(sequencer.data[4], 1);
    bool const font_select = fonts && sequencer.char_sel.A != sequencer.char_sel.B;
    bool const blink = bit(attribute.data[0x10], 3);
    bool const blink_off = bit(frames, 5);
    for (int y = clip.top; y <= clip.bottom; ++y) {
        auto const row = scan_row(y);
        unsigned const glyph_row = row.scan;
        unsigned const pan = horizontal_pan(row);
        for (int x = clip.left; x <= clip.right;) {
            unsigned dx = x + pan;
            unsigned const character = dx / dots;
            uint32_t counter = row.address + crtc.byte_panning + (character >> count_shift);
            uint32_t cell = serializer_address(row, character, count_shift, load_shift);
            uint8_t ch = linear_byte(cell * 4), attr = Test ? test_data : linear_byte(cell * 4 + 1);
            unsigned bank = fonts ? (bit(attr, 3) ? sequencer.char_sel.A : sequencer.char_sel.B) : 0;
            uint32_t font = bank * 0x2000 + ch * 32 + (glyph_row & 31);
            uint8_t glyph = Test ? 0xff : linear_byte(font * 4 + 2);
            // IBM pp2-72/99: underline is a glyph-row attribute, so it
            // follows font selection, blink and the line-graphics ninth dot.
            // SUL is the desired (one-based) line minus one; glyph_row is
            // already zero-based, just like the cursor row comparators.
            if ((attr & 0x77) == 1 && glyph_row == crtc.underline_loc)
                glyph = 0xff;
            bool cursor = crtc.cursor_enable && bit(frames, 4) && counter == crtc.cursor_addr + crtc.cursor_skew &&
                          glyph_row >= crtc.cursor_scan_start && glyph_row <= crtc.cursor_scan_end;
            uint32_t row_pixels[9];
            vga_raster_text_glyph_row(row_pixels, dots, glyph, ch, bit(attribute.data[0x10], 2),
                                      blink && blink_off && bit(attr, 7), cursor, render_attribute_colors[attr & (font_select ? 7 : 15)],
                                      render_attribute_colors[(attr >> 4) & (blink ? 7 : 15)]);
            for (unsigned column = dx % dots; column < dots && x <= clip.right; column++, x++)
                surface.pixel(y, x) = row_pixels[column];
        }
    }
}

// OTI-087 p33, attribute-controller scan test. The card leaves SD15:8 open;
// use $FF there and the last Z180 bus byte on SD7:0 by convention (Second
// Sight reference, docs/reference/oti087-registers.md, ext $03). Like VRAM,
// the retained byte is sampled for rendering, with register changes keeping
// the sampled value for the preceding span. No writes or latches are changed.
void OTI087::draw_scan_test(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    if (!rendering_snapshot)
        test_data = system_data ? *system_data : 0xff;
    if (!packed() && !gc.alpha_dis) {
        draw_text<true>(surface, clip);
        return;
    }
    uint32_t color;
    if (packed()) {
        const uint8_t pixels[4] = {0xff, 0xff, 0xff, 0xff};
        packed_colors(&color, pixels, 1, ramdac, render_dac_colors.data());
    } else if (gc.shift256 && bit(attribute.data[0x10], 6))
        color = render_dac_colors[(attribute.data[0x12] & 15) * 17];
    else
        color = render_attribute_colors[15];
    surface.fill(color, clip);
}

// Combine one bit from each VGA plane into the attribute-controller index.
// A nine-dot character clock still loads eight bits per map, so the ninth dot
// samples index zero (model choice: IBM p2-93 defines the ninth dot for text only).
template<bool Nine>
void OTI087::draw_planar(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    constexpr unsigned dots = Nine ? 9 : 8;
    unsigned const count_shift = crtc.div4 ? 2 : crtc.div2 ? 1 : 0;
    unsigned const load_shift = serializer_load_shift();
    auto colors = render_attribute_colors;
    // Monochrome attribute logic changes the palette index, not VRAM.
    // Resolve it once per span rather than branching for every pixel.
    if (bit(attribute.data[0x10], 1))
        for (unsigned i = 0; i < colors.size(); ++i) {
            unsigned color = i & attribute.data[0x12] & 5;
            if (color == 4 && bit(attribute.data[0x10], 3) && bit(frames, 5))
                color = 0;
            colors[i] = render_attribute_colors[color];
        }
    for (int y = clip.top; y <= clip.bottom; ++y) {
        auto const row = scan_row(y);
        unsigned const pan = horizontal_pan(row);
        for (int x = clip.left; x <= clip.right;) {
            unsigned const dx = x + pan;
            uint32_t const address = serializer_address(row, dx / dots, count_shift, load_shift) * 4;
            uint8_t planes[4];
            for (unsigned p = 0; p != 4; ++p)
                planes[p] = linear_byte(address + p);
            unsigned column = dx % dots;
            for (; column < 8 && x <= clip.right; ++column, ++x) {
                unsigned const shift = 7 - column;
                unsigned const color = bit(planes[0], shift) | (bit(planes[1], shift) << 1) |
                                       (bit(planes[2], shift) << 2) | (bit(planes[3], shift) << 3);
                surface.pixel(y, x) = colors[color];
            }
            if (Nine && column == 8 && x <= clip.right)
                surface.pixel(y, x++) = colors[0];
        }
    }
}

// Interpret VGA 256-color shift mode using CRTC address generation; this
// layout differs from Oak packed-pixel scanout.
template<bool Nine>
void OTI087::draw_vga256(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    // Four consecutive PELs come from the four maps at one CRTC address.
    // Use the same plane storage as CPU accesses, independently of CH4.
    // The output geometry already accounts for attribute PW (pixel width),
    // so only the remaining shift-mode repetition is applied here.
    bool const eight_bit = bit(attribute.data[0x10], 6);
    unsigned const count_shift = crtc.div4 ? 2 : crtc.div2 ? 1 : 0;
    unsigned const load_shift = serializer_load_shift();
    unsigned const pixel_shift = eight_bit ? 0 : 1;
    unsigned const group_width = eight_bit ? 4 : 8;
    unsigned const plane_mask = (attribute.data[0x12] & 15) * 17;
    for (int y = clip.top; y <= clip.bottom; ++y) {
        auto const row = scan_row(y);
        unsigned const pan = horizontal_pan(row);
        if (Nine && eight_bit) {
            // PW pairs consecutive dots, so some pairs include the zero ninth dot.
            unsigned const dot = (clip.left + pan) * 2;
            unsigned character = dot / 9, column = dot % 9;
            uint8_t samples[9] = {};
            auto load = [&] {
                uint32_t const address = serializer_address(row, character, count_shift, load_shift) * 4;
                for (unsigned p = 0; p < 4; ++p) {
                    uint8_t const pixel = linear_byte(address + p);
                    samples[p * 2] = pixel >> 4;
                    samples[p * 2 + 1] = pixel & 15;
                }
            };
            auto next = [&] {
                unsigned const sample = samples[column];
                if (++column == 9) {
                    column = 0;
                    ++character;
                    load();
                }
                return sample;
            };
            load();
            for (int x = clip.left; x <= clip.right; ++x) {
                unsigned const high = next();
                surface.pixel(y, x) = render_dac_colors[((high << 4) | next()) & plane_mask];
            }
            continue;
        }
        for (int x = clip.left; x <= clip.right;) {
            unsigned const dot = x + pan;
            unsigned const character = Nine ? dot / 9 : dot >> (2 + pixel_shift);
            uint32_t const address = serializer_address(row, character, count_shift, load_shift) * 4;
            uint8_t pixels[4];
            for (unsigned p = 0; p < 4; ++p)
                pixels[p] = linear_byte(address + p) & plane_mask;
            // Four attribute inputs are sampled twice in 256-color mode
            // (IBM p2-22). Plane enables gate those inputs on both samples.
            // PW=0 uses low-then-high nibbles as a model convention; IBM's
            // supported mode13 programming sets PW=1 (p2-92).
            unsigned column = Nine ? dot % 9 : dot & (group_width - 1);
            if (eight_bit)
                for (; column < 4 && x <= clip.right; ++column, ++x)
                    surface.pixel(y, x) = render_dac_colors[pixels[column]];
            else {
                for (; column < 8 && x <= clip.right; ++column, ++x)
                    surface.pixel(y, x) = render_attribute_colors[(pixels[column >> 1] >> ((column & 1) * 4)) & 15];
                if (Nine && column == 8 && x <= clip.right)
                    surface.pixel(y, x++) = render_attribute_colors[0];
            }
        }
    }
}

// Overlay the Oak two-plane hardware cursor after the underlying raster.
// Cursor storage and position come from the OTI-087X extended registers.
void OTI087::draw_cursor(ss_video_buffer_t &surface, ss_video_rect_t const &clip) {
    uint8_t const control = ext[0x4c];
    if (!bit(control, 0) || ramdac.asleep())
        return;
    // WHATVGA tested the cursor in 256-color modes only (second-sight-reference
    // docs/06-video.md, "Hardware cursor"). Its colors pass through the
    // Bt481 palette and pixel mask, including unchained VGA 256-color modes.
    if (ramdac.bytes_per_pixel() != 1 ||
        !(packed() || (gc.alpha_dis && gc.shift256 && bit(attribute.data[0x10], 6))))
        return;
    if (bit(control, 3) && bit(frames, 2 + ((control >> 4) & 3)))
        return;
    unsigned const px = ext[0x44], py = ext[0x45];
    if (px > 63 || py > 63)
        return; // p45: presets end at 63, never wrap
    unsigned const skew = crtc.disp_enable_skew * char_dots() * horizontal_multiplier() / pixel_divisor();
    // Oak p44 measures cursor position from the start of the display area.
    // Oak's Windows driver uses signed corner coordinates (STD8_HC.DRV file
    // offsets $56AD-$56C6 and $5537-$557A). It treats Y as the row before
    // the cursor. This model applies Y+1 at every preset, including negative Y.
    // WHATVGA rounds X to a multiple of four and reports imperfect positions;
    // driver avoids X=-14, but the faulty positions are undocumented. Draw
    // each signed X exactly as programmed (second-sight-reference
    // docs/06-video.md, "How Oak's driver positions it").
    int const x0 = int(int16_t((ext[0x40] << 8) | ext[0x41])) + int(skew);
    int const yreg = int16_t((ext[0x42] << 8) | ext[0x43]);
    // Probably the comparator uses field lines: each pattern row appears
    // in both fields (second-sight-reference docs/06-video.md, "Probably:
    // twice as tall in interlace"). This scales position and height
    // independently of presets.
    unsigned const field_shift = get_interlace_mode() ? 1 : 0;
    unsigned const fields = 1U << field_shift;
    int const y0 = (yreg + 1) * int(fields);
    // The cursor base uses eight-byte units. Each 64-pixel row occupies
    // eight bytes in each of two planes (Oak databook pp. 44-46).
    uint32_t const address = ((ext[0x47] << 16) | (ext[0x48] << 8) | ext[0x49]) << 3;
    ss_video_rect_t area{x0, x0 + 63 - int(px), y0, y0 + (64 - int(py)) * int(fields) - 1};
    area = area.intersect(clip);
    area = area.intersect(surface.bounds());
    // p46 bit1 permits the cursor over overscan, but never sync/blanking.
    if (!bit(control, 1))
        area = area.intersect({int(skew), int(skew) + timing.width - 1, 0, timing.height - 1});
    uint32_t const color0 = render_dac_colors[ext[0x4a]];
    uint32_t const color1 = render_dac_colors[ext[0x4b]];
    scan_row_t row;
    unsigned pan = 0;
    int inverted_row = -1;
    for (int y = area.top; y <= area.bottom; ++y) {
        for (int x = area.left; x <= area.right; ++x) {
            if (blanked(x, y))
                continue;
            unsigned const sx = x - x0 + px, sy = (unsigned(y - y0) >> field_shift) + py;
            // Oak's Windows driver (STD8_HC.DRV offsets $53D2-$541B and
            // $87A9-$87E7) uses the first eight bytes of each row for
            // plane A and the next eight for B (second-sight-reference
            // docs/06-video.md, "Hardware cursor"). Its per-byte nibble swap
            // reorders pixels, never planes. Probably bit2 changes only
            // the order within each byte, as described
            // (Oak databook p. 46). The code table states this model's
            // interpretation of the two plane bits.
            uint8_t const a = linear_byte(address + sy * 16 + sx / 8);
            uint8_t const b = linear_byte(address + sy * 16 + 8 + sx / 8);
            unsigned const lane = bit(control, 2) ? 7 - (sx & 7) : (sx & 7);
            switch (cursor_codes[(bit(a, lane) << 1) | bit(b, lane)]) {
                case CursorOp::color0:
                    surface.pixel(y, x) = color0;
                    break;
                case CursorOp::color1:
                    surface.pixel(y, x) = color1;
                    break;
                case CursorOp::transparent:
                    break;
                case CursorOp::invert: {
                    // Oak p21: P[7:0] carries the index to the external palette
                    // chip. The cursor's invert operation (second-sight-reference
                    // docs/06-video.md, "Hardware cursor") therefore complements
                    // that index before the Bt481 mask and palette lookup.
                    // Recover it from scanout, not RGB: palette entries can
                    // have equal colors and need not form complementary pairs.
                    unsigned index = attribute.data[0x11];
                    if (x >= int(skew) && x < int(skew) + timing.width && y < timing.height) {
                        // Solid and transparent cursors need no background
                        // fetch. Resolve a row only at its first inverted pixel.
                        if (inverted_row != y) {
                            row = scan_row(y);
                            pan = horizontal_pan(row);
                            inverted_row = y;
                        }
                        unsigned const dot = x - skew + pan;
                        if (scan_test())
                            index = packed() ? 255 : (attribute.data[0x12] & 15) * 17;
                        else if (packed())
                            index = linear_byte(row.address * 8 + dot);
                        else {
                            unsigned const count_shift = crtc.div4 ? 2 : crtc.div2 ? 1 : 0;
                            unsigned const load_shift = serializer_load_shift();
                            unsigned const plane_mask = (attribute.data[0x12] & 15) * 17;
                            index = linear_byte(serializer_address(row, dot >> 2, count_shift, load_shift) * 4 +
                                                (dot & 3)) & plane_mask;
                        }
                    }
                    surface.pixel(y, x) = render_dac_colors[index ^ 255];
                    break;
                }
            }
        }
    }
}

// Clip the requested surface and dispatch the active VGA mode; blanking and
// the hardware cursor are applied to the same visible region.
void OTI087::render(ss_video_buffer_t surface, ss_video_rect_t clip) {
    auto bounds = clip.intersect(surface.bounds());
    if (bounds.empty())
        return;
    if (!enabled() || (sequencer.data[0] & 3) != 3 || (sequencer.data[1] & 0x20) || !(attribute.index & 0x20)) {
        surface.fill(0xff000000, bounds);
        return;
    }
    if (!render_row)
        render_colors_valid = false;
    prepare_render_colors();
    unsigned const dots = char_dots();
    int const skew = crtc.disp_enable_skew * dots * horizontal_multiplier() / pixel_divisor();
    auto active = bounds.intersect({skew, timing.width + skew - 1, 0, timing.height - 1});
    bool const overscan = active.width() != bounds.width() || active.height() != bounds.height();
    if (overscan)
        surface.fill(0xff000000, bounds);
    if (!active.empty()) {
        ss_video_buffer_t target{surface.pixels + skew, surface.width - skew, surface.height, surface.pitch};
        ss_video_rect_t area{active.left - skew, active.right - skew, active.top, active.bottom};
        if (scan_test())
            draw_scan_test(target, area);
        else if (packed())
            draw_packed(target, area);
        else if (!gc.alpha_dis)
            draw_text<false>(target, area);
        else if (gc.shift256)
            dots == 9 ? draw_vga256<true>(target, area) : draw_vga256<false>(target, area);
        else if (!gc.shift_reg)
            dots == 9 ? draw_planar<true>(target, area) : draw_planar<false>(target, area);
        else
            dots == 9 ? draw_cga<true>(target, area) : draw_cga<false>(target, area);
    }
    if (overscan)
        for (int y = bounds.top; y <= bounds.bottom; y++)
            for (int x = bounds.left; x <= bounds.right; x++)
                if (!active.contains(x, y) && !blanked(x, y))
                    surface.pixel(y, x) = ramdac.pixel_select(attribute.data[0x11]);
    // Blanking can begin inside the displayed rectangle. Fill intervals,
    // not individual pixels, so the common raster has no per-pixel test.
    int const hb_start = horizontal_blank_first() * dots * horizontal_multiplier() / pixel_divisor();
    int const hb_end = hb_start + horizontal_blank_length() * dots * horizontal_multiplier() / pixel_divisor();
    for (int y = bounds.top; y <= bounds.bottom; ++y) {
        // Probe vertical blank separately from horizontal blank at x=0.
        unsigned const line = y / ((get_interlace_mode() ? 2 : 1) * vertical_divisor());
        unsigned const vt = crtc.vert_total + 2;
        if ((line + vt - vertical_blank_first() % vt) % vt < vertical_blank_length())
            surface.fill(0xff000000, {bounds.left, bounds.right, y, y});
        else {
            surface.fill(0xff000000, ss_video_rect_t{hb_start, hb_end - 1, y, y}.intersect(bounds));
            if (hb_end > timing.total_width)
                surface.fill(0xff000000, ss_video_rect_t{0, hb_end - timing.total_width - 1, y, y}.intersect(bounds));
        }
    }
    draw_cursor(surface, bounds);
}

// Render only packed rows whose VRAM or palette dependency changed.
// Start-address latch changes split the frame into independently addressed spans.
bool OTI087::render_packed_frame(ss_video_buffer_t surface) {
    unsigned const bytes = ramdac.bytes_per_pixel();
    bool const active_changes = !display_changes.empty() &&
        (get_interlace_mode() || display_changes.front().position < timing.height * timing.total_width);
    if (active_changes || !packed_scanout || bytes == 4 || !enabled() || (sequencer.data[0] & 3) != 3 || (sequencer.data[1] & 0x20) ||
        !(attribute.index & 0x20) || crtc.disp_enable_skew || (ext[0x4c] & 1) || surface.width != timing.width ||
        surface.height != timing.height || surface.width <= 0 || surface.height <= 0)
        return false;
    // The row-reuse path assumes the active rectangle is outside blanking.
    // Nonstandard blank windows use the same interval renderer as other modes.
    if (horizontal_blank_first() <= crtc.horz_disp_end || vertical_blank_first() <= crtc.vert_disp_end ||
        horizontal_blank_first() + horizontal_blank_length() > crtc.horz_total + 5u ||
        vertical_blank_first() + vertical_blank_length() > crtc.vert_total + 2u)
        return false;
    // Row reuse needs ordered start-address changes beginning at line zero.
    // Reject ambiguous spans so the general renderer handles them instead.
    if (!starts.empty()) {
        if (starts.front().line != 0)
            return false;
        for (unsigned i = 1; i < starts.size(); ++i)
            if (starts[i].line <= starts[i - 1].line)
                return false;
    }
    std::array<uint32_t, 256> palette{};
    if (bytes == 1)
        for (unsigned i = 0; i < palette.size(); ++i)
            palette[i] = ramdac.pixel_select(i);
    // Equal VRAM bytes are not enough: geometry, packing, sleep and palette
    // can change their visible colors. Any such change invalidates every row.
    bool const width_changed = packed_width != surface.width;
    bool const redraw = !packed_valid || width_changed || packed_height != surface.height ||
                        packed_command != ramdac.command_r() || packed_asleep != ramdac.asleep() ||
                        packed_palette != palette;
    packed_width = surface.width;
    packed_height = surface.height;
    packed_command = ramdac.command_r();
    packed_asleep = ramdac.asleep();
    packed_palette = palette;
    unsigned const row_bytes = surface.width * bytes;
    packed_row.resize(row_bytes);
    packed_source.resize(row_bytes * surface.height);
    frame_pixels.resize(surface.width * surface.height);
    std::array<uint32_t, 10> const address_state = {
        unsigned(surface.height), starts.empty() ? crtc.start_addr : starts.front().address,
        crtc.offset, crtc.maximum_scan_line, crtc.scan_doubling, crtc.preset_row_scan,
        crtc.line_compare, unsigned(get_interlace_mode()), vertical_divisor(),
        attribute.pel_shift | (unsigned(attribute.data[0x10] & 0x20) << 8) | (bytes << 16)};
    bool const address_changes = starts.size() > 1 && starts[1].line < surface.height;
    // Width changes the populated-memory bound even when row addresses do not.
    if (!packed_addresses_valid || width_changed || address_changes || packed_address_state != address_state) {
        packed_addresses.resize(surface.height);
        packed_rows_in_memory = true;
        unsigned const fields = get_interlace_mode() ? 2 : 1;
        for (unsigned field = 0; field < fields; ++field) {
            unsigned change = 0, field_line = 0;
            auto row = scan_row(field);
            for (unsigned y = field; y < unsigned(surface.height); y += fields) {
                while (change + 1 < starts.size() && starts[change + 1].line <= y)
                    ++change;
                uint32_t address = row.address;
                if (!row.split && !starts.empty())
                    address += starts[change].address - crtc.start_addr;
                packed_addresses[y] = address * 8 + horizontal_pan(row) * bytes;
                if (packed_addresses[y] >= VRAM_SIZE || row_bytes > VRAM_SIZE - packed_addresses[y])
                    packed_rows_in_memory = false;
                next_scan_row(row, field_line++);
            }
        }
        packed_address_state = address_state;
        // A frame containing address changes cannot seed the constant cache.
        packed_addresses_valid = !address_changes;
    }
    // Bound checks belong to address preparation, not the common pixel copy.
    // The general path supplies fixed undriven bytes for an unpopulated row.
    if (!packed_rows_in_memory)
        return false;
    for (int y = 0; y < surface.height; ++y) {
        packed_bytes(packed_row.data(), memory.data(), packed_addresses[y], row_bytes);
        uint8_t *previous = packed_source.data() + y * row_bytes;
        uint32_t *pixels = frame_pixels.data() + y * surface.width;
    // Compare the displayed source bytes, after current address translation,
    // so a page flip or stride change is detected without per-write dirty flags.
        if (redraw || std::memcmp(previous, packed_row.data(), row_bytes)) {
            std::memcpy(previous, packed_row.data(), row_bytes);
            packed_colors(pixels, packed_row.data(), surface.width, ramdac, palette.data());
            if (present_bottom < present_top)
                present_top = y;
            else
                present_top = std::min(present_top, y);
            present_bottom = std::max(present_bottom, y);
        }
        if (&surface.pixel(y, 0) != pixels)
            std::memcpy(&surface.pixel(y, 0), pixels, surface.width * sizeof(uint32_t));
    }
    packed_valid = true;
    return true;
}

// Draw a full frame into surface, replaying retained display-address/pan
// boundaries. Restore the live register values after rendering those spans;
// presenting a frame must not change what the emulated CPU reads.
void OTI087::render_frame(ss_video_buffer_t surface) {
    // The usual page flip latches after the active picture. Keep the packed
    // SIMD/row-reuse path for that complete picture under its previous state;
    // only changes during active scanout need the row accumulator replay.
    if (!display_changes.empty() && !get_interlace_mode() && surface.width == timing.width &&
        surface.height == timing.height && display_changes.front().position >= timing.height * timing.total_width) {
        auto live = display_registers();
        display_registers(display_changes.front().registers);
        if (!render_packed_frame(surface)) {
            packed_valid = false;
            present_top = 0;
            present_bottom = surface.height - 1;
            render(surface, surface.bounds());
        }
        display_registers(live);
        rendering_snapshot = false;
        return;
    }
    if (render_packed_frame(surface))
        return;
    packed_valid = false;
    present_top = 0;
    present_bottom = surface.height - 1;
    if (!display_changes.empty()) {
        render_changes(surface);
        return;
    }
    uint32_t address = crtc.start_addr;
    if (starts.empty())
        render(surface, surface.bounds());
    for (unsigned i = 0; i < starts.size(); i++) {
        int end = i + 1 < starts.size() ? starts[i + 1].line - 1 : surface.height - 1;
        crtc.start_addr = starts[i].address;
        if (end >= starts[i].line)
            render(surface, {0, surface.width - 1, starts[i].line, end});
    }
    crtc.start_addr = address;
}

// Expose scanout geometry and palette to presentation without changing registers.
ss_video_frame_t OTI087::display_state() const {
    ss_video_frame_t state{};
    state.mode = packed()        ? ss_video_mode_t::Packed
                 : !gc.alpha_dis ? ss_video_mode_t::Text
                 : gc.shift256   ? ss_video_mode_t::VGA256
                 : gc.shift_reg  ? ss_video_mode_t::CGA
                                 : ss_video_mode_t::Planar;
    state.width = timing.width;
    state.height = timing.height;
    state.pitch = packed() ? crtc.offset * 16 : offset();
    state.start = crtc.start_addr * (packed() ? 8 : 1);
    state.depth = packed() ? ramdac.bytes_per_pixel() * 8 : gc.shift256 ? 8 : 4;
    if (packed() && (ramdac.command_r() & 0xd0) == 0x80)
        state.depth = 15;
    for (unsigned i = 0; i < 256; i++) {
        VideoColor color = ramdac.pixel_select(i);
        state.palette[i][0] = color.r();
        state.palette[i][1] = color.g();
        state.palette[i][2] = color.b();
    }
    return state;
}

// Interpret CGA-compatible shift/address modes through the VGA palette path.
template<bool Nine>
void OTI087::draw_cga(ss_video_buffer_t &surface, const ss_video_rect_t &clip) {
    constexpr unsigned dots = Nine ? 9 : 8;
    // IBM p2-84: even/odd bits from maps 0/1 become C0/C1; maps 2/3
    // supply C2/C3. All four maps share the regular CRTC address mux.
    unsigned const count_shift = crtc.div4 ? 2 : crtc.div2 ? 1 : 0;
    unsigned const load_shift = serializer_load_shift();
    for (int y = clip.top; y <= clip.bottom; y++) {
        auto const row = scan_row(y);
        unsigned const pan = horizontal_pan(row);
        for (int x = clip.left; x <= clip.right;) {
            unsigned const dx = x + pan;
            uint32_t const address = serializer_address(row, dx / dots, count_shift, load_shift) * 4;
            uint8_t planes[4];
            for (unsigned p = 0; p < 4; ++p)
                planes[p] = linear_byte(address + p);
            unsigned column = dx % dots;
            for (; column < 8 && x <= clip.right; ++column, ++x) {
                unsigned const plane = column >> 2;
                unsigned const shift = 6 - (column & 3) * 2;
                unsigned const color = ((planes[plane] >> shift) & 3) | (((planes[plane + 2] >> shift) & 3) << 2);
                surface.pixel(y, x) = render_attribute_colors[color];
            }
            if (Nine && column == 8 && x <= clip.right)
                surface.pixel(y, x++) = render_attribute_colors[0];
        }
    }
}

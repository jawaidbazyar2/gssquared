/*
 *   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright holders: Nathan Woods, Peter Trauner, Angelo Salese.
 *   See secondsight-video.LICENSE for the BSD-3-Clause terms.
 *
 *   Decode the Video Graphics Array (VGA) registers in the Oak OTI-087X
 *   controller on Second Sight.
 *   OTI087's I/O decoder calls these functions after software selects a register
 *   index. Writes split packed register bits into fields; the memory and display
 *   paths consume those fields. Reads reconstruct the software-visible byte.
 *
 *   CRTC means cathode-ray-tube controller: the counters that choose the display
 *   address, pixel/line totals, blank intervals and text cursor. The graphics
 *   controller chooses video-memory planes (separate arrays of pixel bits) and
 *   masked-write rules. Its latches retain bytes from a previous memory read.
 *   OTI087 owns all fields; only the card execution thread writes them. Host
 *   inspection waits for that thread. Source: IBM VGA/XGA Technical Reference,
 *   May 1992, pp. 2-64 to 2-95 (CRTC, graphics and attribute registers). Oak
 *   OTI-087 databook pp. 39-44 adds upper address bits and wide-latch controls.
 *   Register analysis: https://github.com/Webifi/second-sight-reference,
 *   docs/14-custom-code.md.
 */

#include "oti087.hpp"

namespace {
// Extract bit n as zero or one when decoding a documented register field.
constexpr unsigned bit(uint32_t value, unsigned n) {
    return (value >> n) & 1;
}
} // namespace

// Pack split CRTC timing and address fields back into their indexed register
// bytes (IBM VGA/XGA Technical Reference, CRTC register map).
// CRTC indices (hex; IBM VGA/XGA TRM pp. 2-64 to 2-81):
// 00 horizontal total; 01 display end; 02 blank start; 03 blank end/skew;
// 04 retrace start; 05 retrace end/skew; 06 vertical total; 07 overflow bits;
// 08 preset row/byte pan; 09 character height, line doubling and overflow;
// 0A/0B text cursor first/last row; 0C/0D display start high/low;
// 0E/0F cursor address high/low; 10 vertical retrace start; 11 retrace end,
// interrupt control and write protection; 12 vertical display end;
// 13 row stride; 14 underline/address units; 15/16 vertical blank start/end;
// 17 sync/address controls; 18 split-screen line; 24 attribute access phase.
// Retrace is the interval used to return the monitor beam. Pan shifts the
// displayed origin within a row. Overflow registers hold upper counter bits.
uint8_t OTI087::crtc_read(unsigned offset) const {
    switch (offset) {
        case 0x00: {
            return crtc.horz_total & 0xff;
        }
        case 0x01: {
            return crtc.horz_disp_end & 0xff;
        }
        case 0x02: {
            return crtc.horz_blank_start & 0xff;
        }
        case 0x03: {
            uint8_t res = crtc.horz_blank_end & 0x1f;
            res |= (crtc.disp_enable_skew & 3) << 5;
            res |= (crtc.evra & 1) << 7;
            return res;
        }
        case 0x04: {
            return crtc.horz_retrace_start & 0xff;
        }
        case 0x05: {
            uint8_t res = (crtc.horz_blank_end & 0x20) << 2;
            res |= (crtc.horz_retrace_skew & 3) << 5;
            res |= (crtc.horz_retrace_end & 0x1f);
            return res;
        }
        case 0x06: {
            return crtc.vert_total & 0xff;
        }
        case 0x07: {
            uint8_t res = (crtc.line_compare & 0x100) >> 4;
            res |= (crtc.vert_retrace_start & 0x200) >> 2;
            res |= (crtc.vert_disp_end & 0x200) >> 3;
            res |= (crtc.vert_total & 0x200) >> 4;
            res |= (crtc.vert_blank_start & 0x100) >> 5;
            res |= (crtc.vert_retrace_start & 0x100) >> 6;
            res |= (crtc.vert_disp_end & 0x100) >> 7;
            res |= (crtc.vert_total & 0x100) >> 8;
            return res;
        }
        case 0x08: {
            uint8_t res = (crtc.byte_panning & 3) << 5;
            res |= (crtc.preset_row_scan & 0x1f);
            return res;
        }
        case 0x09: {
            uint8_t res = (crtc.maximum_scan_line - 1) & 0x1f;
            res |= (crtc.scan_doubling & 1) << 7;
            res |= (crtc.line_compare & 0x200) >> 3;
            res |= (crtc.vert_blank_start & 0x200) >> 4;
            return res;
        }
        case 0x0a: {
            uint8_t res = (crtc.cursor_scan_start & 0x1f);
            res |= ((crtc.cursor_enable & 1) ^ 1) << 5;
            return res;
        }
        case 0x0b: {
            uint8_t res = (crtc.cursor_skew & 3) << 5;
            res |= (crtc.cursor_scan_end & 0x1f);
            return res;
        }
        case 0x0c:
        case 0x0d: {
            return (crtc.start_addr_latch >> ((offset & 1) ^ 1) * 8) & 0xff;
        }
        case 0x0e:
        case 0x0f: {
            return (crtc.cursor_addr >> ((offset & 1) ^ 1) * 8) & 0xff;
        }
        case 0x10: {
            // Convention: no EGA light-pen alias when CRTC03 bit7 is clear.
            // Return programmed 10/11 at every beam position (second-sight-
            // reference docs/14-custom-code.md, "The current line").
            return crtc.vert_retrace_start & 0xff;
        }
        case 0x11: {
            uint8_t res = (crtc.protect_enable & 1) << 7;
            res |= (crtc.bandwidth & 1) << 6;
            res |= (crtc.vert_retrace_end & 0xf);
            res |= (crtc.irq_clear & 1) << 4;
            res |= (crtc.irq_disable & 1) << 5;
            return res;
        }
        case 0x12: {
            return crtc.vert_disp_end & 0xff;
        }
        case 0x13: {
            return crtc.offset & 0xff;
        }
        case 0x14: {
            uint8_t res = (crtc.dw & 1) << 6;
            res |= (crtc.div4 & 1) << 5;
            res |= (crtc.underline_loc & 0x1f);
            return res;
        }
        case 0x15: {
            return crtc.vert_blank_start & 0xff;
        }
        case 0x16: {
            return crtc.vert_blank_end & 0xff;
        }
        case 0x17: {
            uint8_t res = (crtc.sync_en & 1) << 7;
            res |= (crtc.word_mode & 1) << 6;
            res |= (crtc.aw & 1) << 5;
            res |= (crtc.div2 & 1) << 3;
            res |= (crtc.sldiv & 1) << 2;
            res |= (crtc.map14 & 1) << 1;
            res |= (crtc.map13 & 1) << 0;
            return res;
        }
        case 0x18: {
            return crtc.line_compare & 0xff;
        }
        case 0x24:
            return attribute.state << 7;
        default:
            return 0xff;
    }
}

// Store data at the selected CRTC index while honoring write protection.
// The caller recomputes timing afterwards. Overflow bits belong to several
// counters, so each write preserves the bits stored in other registers.
void OTI087::crtc_write(uint8_t data) {
    crtc.data[crtc.index] = data;
    unsigned offset = crtc.index;
    switch (offset) {
        case 0x00: {
            if (crtc.protect_enable)
                return;
            crtc.horz_total = (crtc.horz_total & ~0xff) | (data & 0xff);

            break;
        }
        case 0x01: {
            if (crtc.protect_enable)
                return;
            crtc.horz_disp_end = (crtc.horz_disp_end & ~0xff) | (data & 0xff);

            break;
        }
        case 0x02: {
            if (crtc.protect_enable)
                return;
            crtc.horz_blank_start = (crtc.horz_blank_start & ~0xff) | (data & 0xff);

            break;
        }
        case 0x03: {
            if (crtc.protect_enable)
                return;
            crtc.horz_blank_end &= ~0x1f;
            crtc.horz_blank_end |= data & 0x1f;
            crtc.disp_enable_skew = (data & 0x60) >> 5;
            crtc.evra = bit(data, 7);

            break;
        }
        case 0x04: {
            if (crtc.protect_enable)
                return;
            crtc.horz_retrace_start = (crtc.horz_retrace_start & ~0xff) | (data & 0xff);

            break;
        }
        case 0x05: {
            if (crtc.protect_enable)
                return;
            crtc.horz_blank_end &= ~0x20;
            crtc.horz_blank_end |= ((data & 0x80) >> 2);
            crtc.horz_retrace_skew = ((data & 0x60) >> 5);
            crtc.horz_retrace_end = (crtc.horz_retrace_end & ~0x1f) | (data & 0x1f);

            break;
        }
        case 0x06: {
            if (crtc.protect_enable)
                return;
            crtc.vert_total &= ~0xff;
            crtc.vert_total |= data & 0xff;

            break;
        }
        case 0x07: {
            // Overflow bit 4 belongs to the split-screen line and remains
            // writable even when protection locks the other timing fields.
            crtc.line_compare &= ~0x100;
            crtc.line_compare |= ((data & 0x10) << (8 - 4));

            if (crtc.protect_enable)
                return;
            crtc.vert_total &= ~0x300;
            crtc.vert_retrace_start &= ~0x300;
            crtc.vert_disp_end &= ~0x300;
            crtc.vert_blank_start &= ~0x100;
            crtc.vert_retrace_start |= ((data & 0x80) << (9 - 7));
            crtc.vert_disp_end |= ((data & 0x40) << (9 - 6));
            crtc.vert_total |= ((data & 0x20) << (9 - 5));
            crtc.vert_blank_start |= ((data & 0x08) << (8 - 3));
            crtc.vert_retrace_start |= ((data & 0x04) << (8 - 2));
            crtc.vert_disp_end |= ((data & 0x02) << (8 - 1));
            crtc.vert_total |= ((data & 0x01) << (8 - 0));

            break;
        }
        case 0x08: {
            crtc.byte_panning = (data & 0x60) >> 5;
            crtc.preset_row_scan = (data & 0x1f);

            break;
        }
        case 0x09: {
            crtc.line_compare &= ~0x200;
            crtc.vert_blank_start &= ~0x200;
            crtc.scan_doubling = ((data & 0x80) >> 7);
            crtc.line_compare |= ((data & 0x40) << (9 - 6));
            crtc.vert_blank_start |= ((data & 0x20) << (9 - 5));
            crtc.maximum_scan_line = (data & 0x1f) + 1;

            break;
        }
        case 0x0a: {
            crtc.cursor_enable = ((data & 0x20) ^ 0x20) >> 5;
            crtc.cursor_scan_start = data & 0x1f;
            break;
        }
        case 0x0b: {
            crtc.cursor_skew = (data & 0x60) >> 5;
            crtc.cursor_scan_end = data & 0x1f;
            break;
        }
        case 0x0c:
        case 0x0d: {
            crtc.start_addr_latch &= ~(0xff << (((offset & 1) ^ 1) * 8));
            crtc.start_addr_latch |= (data << (((offset & 1) ^ 1) * 8));
            break;
        }
        case 0x0e:
        case 0x0f: {
            crtc.cursor_addr &= ~(0xff << (((offset & 1) ^ 1) * 8));
            crtc.cursor_addr |= (data << (((offset & 1) ^ 1) * 8));
            break;
        }
        case 0x10: {
            crtc.vert_retrace_start &= ~0xff;
            crtc.vert_retrace_start |= data & 0xff;

            break;
        }
        case 0x11: {
            crtc.protect_enable = bit(data, 7);
            crtc.bandwidth = bit(data, 6);

            crtc.irq_disable = bit(data, 5);
            crtc.irq_clear = bit(data, 4);
            crtc.vert_retrace_end = (crtc.vert_retrace_end & ~0xf) | (data & 0x0f);

            // Interrupt clear is active low. Writing a zero acknowledges
            // the latched vertical interrupt; the caller updates its output pin.
            if (crtc.irq_clear == 0) {
                crtc.irq_latch = 0;
            }

            break;
        }
        case 0x12: {
            crtc.vert_disp_end &= ~0xff;
            crtc.vert_disp_end |= data & 0xff;

            break;
        }
        case 0x13: {
            crtc.offset &= ~0xff;
            crtc.offset |= data & 0xff;
            break;
        }
        case 0x14: {
            crtc.dw = (data & 0x40) >> 6;
            crtc.div4 = (data & 0x20) >> 5;
            crtc.underline_loc = (data & 0x1f);
            break;
        }
        case 0x15: {
            crtc.vert_blank_start &= ~0xff;
            crtc.vert_blank_start |= data & 0xff;

            break;
        }
        case 0x16: {
            crtc.vert_blank_end = data;

            break;
        }
        case 0x17: {
            crtc.sync_en = bit(data, 7);
            crtc.word_mode = bit(data, 6);
            crtc.aw = bit(data, 5);
            crtc.div2 = bit(data, 3);
            crtc.sldiv = bit(data, 2);
            crtc.map14 = bit(data, 1);
            crtc.map13 = bit(data, 0);

            break;
        }
        case 0x18: {
            crtc.line_compare &= ~0xff;
            crtc.line_compare |= data & 0xff;

            break;
        }
        default:
            break;
    }
}

// Return the guest-visible graphics-controller register bit layout.
// Graphics-controller indices (IBM VGA/XGA TRM pp. 2-82 to 2-89):
// 00 set/reset color (bits 3:0); 01 enables that color per plane (3:0);
// 02 compare color (3:0); 03 rotate count (2:0), logic operation (4:3);
// 04 read-plane number (1:0); 05 write mode (1:0), compare-read mode (3),
// odd/even reads (4), CGA shift (5), 256-color shift (6); 06 graphics/text
// (0), odd/even chaining (1), CPU window (3:2); 07 compare-plane enables
// (3:0); 08 per-pixel bit mask (7:0). Unimplemented indices read as $FF.
uint8_t OTI087::gc_read(unsigned offset) const {
    switch (offset) {
        case 0x00: {
            return gc.set_reset & 0xf;
        }
        case 0x01: {
            return gc.enable_set_reset & 0xf;
        }
        case 0x02: {
            return gc.color_compare & 0xf;
        }
        case 0x03: {
            return ((gc.logical_op & 3) << 3) | (gc.rotate_count & 7);
        }
        case 0x04: {
            return gc.read_map_sel & 3;
        }
        case 0x05: {
            uint8_t res = (gc.shift256 & 1) << 6;
            res |= (gc.shift_reg & 1) << 5;
            res |= (gc.host_oe & 1) << 4;
            res |= (gc.read_mode & 1) << 3;
            res |= (gc.write_mode & 3);
            return res;
        }
        case 0x06: {
            uint8_t res = (gc.memory_map_sel & 3) << 2;
            res |= (gc.chain_oe & 1) << 1;
            res |= (gc.alpha_dis & 1);
            return res;
        }
        case 0x07: {
            return gc.color_dont_care & 0xf;
        }
        case 0x08: {
            return gc.bit_mask & 0xff;
        }
        default:
            return 0xff;
    }
}

// Decode VGA rotate, read/write mode, map and mask controls for the VRAM path
// (IBM VGA/XGA Technical Reference, Graphics Controller registers).
void OTI087::gc_write(uint8_t data) {
    unsigned offset = gc.index;
    switch (offset) {
        case 0x00: {
            gc.set_reset = data & 0xf;
            break;
        }
        case 0x01: {
            gc.enable_set_reset = data & 0xf;
            break;
        }
        case 0x02: {
            gc.color_compare = data & 0xf;
            break;
        }
        case 0x03: {
            gc.logical_op = (data & 0x18) >> 3;
            gc.rotate_count = data & 7;
            break;
        }
        case 0x04: {
            gc.read_map_sel = data & 3;
            break;
        }
        case 0x05: {
            gc.shift256 = bit(data, 6);
            gc.shift_reg = bit(data, 5);
            gc.host_oe = bit(data, 4);
            gc.read_mode = bit(data, 3);
            gc.write_mode = data & 3;

            break;
        }
        case 0x06: {
            gc.memory_map_sel = (data & 0xc) >> 2;
            gc.chain_oe = bit(data, 1);
            gc.alpha_dis = bit(data, 0);

            break;
        }
        case 0x07: {
            gc.color_dont_care = data & 0xf;
            break;
        }
        case 0x08: {
            gc.bit_mask = data & 0xff;
            break;
        }
        default:
            break;
    }
    gc.plain_write = gc.write_mode == 0 && !gc.enable_set_reset && !gc.rotate_count &&
                     !gc.logical_op && gc.bit_mask == 0xff;
}

// Convert CRTC Offset to the active byte/word/dword address stride.
// IBM VGA/XGA Technical Reference, CRTC Offset and Underline Location.
uint16_t OTI087::offset() const {
    if (crtc.dw)
        return crtc.offset << 3;
    if (crtc.word_mode)
        return crtc.offset << 1;
    else
        return crtc.offset << 2;
}

// Compute a plane write from the retained read latch and current VGA write
// mode; a masked bit retains the latch value, not the incoming CPU byte.
uint8_t OTI087::vga_latch_write(int offs, uint8_t data) {
    // The IBM and Oak wide-latch paths use the same equations; only the
    // saved byte differs (IBM VGA/XGA TRM pp. 2-83 to 2-89).
    return vga_write_byte(offs, data, gc.latch[offs]);
}

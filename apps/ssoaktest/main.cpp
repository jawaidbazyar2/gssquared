/*
 *   Second Sight's Oak OTI-087X, Brooktree Bt481 and Z180 memory-DMA checks.
 *   DMA (direct memory access) transfers bytes without CPU instructions.
 *   VGA means Video Graphics Array; its CRTC (cathode-ray-tube controller)
 *   generates scan addresses and timing. CGA is the older Color Graphics
 *   Adapter pixel format retained by VGA. Each single-threaded fixture
 *   owns its card and inspects registers, memory or rendered pixels.
 *   The checks run without firmware or an SDL window. Main takes no arguments
 *   and returns zero after all assertions pass.
 *
 *   Sources: IBM VGA/XGA Technical Reference, May 1992, chapter 2 (pages noted
 *   per fixture); Oak OTI-087 databook, February 1993, pp. 29-46 and OTI-087X
 *   addendum; Brooktree Bt481A L481A_C, pp. 6-8 and 14-20. Software-derived
 *   lane, pitch and cursor rules are described in the Second Sight reference:
 *   https://github.com/Webifi/second-sight-reference
 *   docs/06-video.md, "The 64-bit latch lane rule", "Packed-pixel mode and
 *   the CRTC" and "Hardware cursor"; docs/15-oak-drawing.md, "Display features
 *   that cost no drawing"; docs/14-custom-code.md, "The current line";
 *   docs/reference/oti087-registers.md; firmware/v1.5/src/text.asm and
 *   firmware/v1.5/src/textsetup.asm.
 *   Missing hardware definitions are identified at the corresponding fixture
 *   and implementation. Passing those checks validates the stated model, not
 *   undocumented silicon behavior.
 *
 *   VGA writes use an independent bit-by-bit truth table for all four write
 *   modes, four logical functions, eight rotations, four bit masks and sixteen
 *   map masks in planar and packed modes. Other checks cover read comparison,
 *   banks, odd/even and chain-4 addressing, apertures, expansion, wide latches,
 *   CRTC timing/addressing, simultaneous READY/raster events, register changes
 *   within a frame, text/CGA serialization, palettes, cursor clipping and DMA0.
 *   Bt481 checks cover every 15/16-bit input word, every pixel mask/index pair,
 *   6/8-bit palette sequencing, 24/32-bit formats and command-register access.
 *
 *   Included in the normal developer-tools build; run with CTest.
 */
#include "devices/secondsight/ss_card.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static unsigned checks = 0;
static void equal(uint64_t actual, uint64_t expected, int line) {
    ++checks;
    if (actual != expected) {
        std::fprintf(stderr, "line %d: got %llx, expected %llx\n", line,
                     (unsigned long long)actual, (unsigned long long)expected);
        std::abort();
    }
}
#define EQ(a,b) equal((a),(b),__LINE__)
static void reg(OTI087 &v, unsigned port, unsigned index, unsigned value) {
    v.io_w(port, index); v.io_w(port + 1, value);
}
// Register blocks: sequencer, graphics controller, CRTC and Oak extensions.
static void seq(OTI087 &v, unsigned i, unsigned d) { reg(v, 0x3c4, i, d); }
static void gc(OTI087 &v, unsigned i, unsigned d) { reg(v, 0x3ce, i, d); }
static void cr(OTI087 &v, unsigned i, unsigned d) { reg(v, 0x3d4, i, d); }
static void oak(OTI087 &v, unsigned i, unsigned d) { reg(v, 0x3de, i, d); }
static unsigned read(OTI087 &v, unsigned port, unsigned i) {
    v.io_w(port, i); return v.io_r(port + 1);
}
// Reset the attribute index/data toggle, write register i, then enable display.
static void attr(OTI087 &v, unsigned i, unsigned d) {
    v.io_r(0x3da); v.io_w(0x3c0, i); v.io_w(0x3c0, d); v.io_w(0x3c0, 0x20);
}
// Enable Oak I/O and the A0000 memory window with all four VGA planes writable.
// Leave bank and format changes to the fixture that needs them (Oak pp. 29-31).
static void enable(OTI087 &v) {
    v.io_w(0x46e8, 0x18); v.io_w(0x102, 1); v.io_w(0x46e8, 8);
    v.io_w(0x3c2, 3); seq(v, 0, 3); seq(v, 1, 1); seq(v, 2, 15); seq(v, 4, 6);
    gc(v, 6, 5); gc(v, 8, 255); oak(v, 0x19, 15);
}
static void beam(OTI087 &v, unsigned y, unsigned x = 0) {
    v.advance(v.time() + v.time_until_pos(y, x));
    EQ(v.vpos(), y); EQ(v.hpos(), x);
}
// Make a small scan: 64 dots, 8 visible lines; 160 dots and 16 lines total.
// packed selects Oak byte-per-pixel mode. Otherwise c256 selects VGA
// 256-color output, which combines two dots per pixel (32 visible pixels).
static void mode(OTI087 &v, bool packed = false, bool c256 = true) {
    v.reset();
    enable(v);
    v.io_w(0x3c6, 255);
    cr(v, 0x11, 0); cr(v, 0, 15); cr(v, 1, 7); cr(v, 2, 8); cr(v, 3, 20);
    cr(v, 4, 12); cr(v, 5, 16); cr(v, 6, 14); cr(v, 7, 0x10);
    cr(v, 8, 0); cr(v, 9, 0x40); cr(v, 0x10, 12); cr(v, 0x11, 0x1e);
    cr(v, 0x12, 7); cr(v, 0x13, 4); cr(v, 0x14, 31); cr(v, 0x15, 9);
    cr(v, 0x16, 14); cr(v, 0x17, 0xc3); cr(v, 0x18, 255);
    gc(v, 5, c256 ? 0x40 : 0); oak(v, 0x21, packed ? 4 : 0);
    attr(v, 0x10, c256 && !packed ? 0x41 : 1); attr(v, 0x12, 15);
    attr(v, 0x13, 0);
    for (unsigned i = 0; i < 16; ++i) attr(v, i, i);
    for (unsigned i = 0; i < 256; ++i) v.dac().set_color(i, VideoColor(0, 0, i));
    beam(v, 0);
}
// The sentinel exposes pixels left unwritten by the renderer.
static std::vector<uint32_t> frame(OTI087 &v) {
    const auto &t = v.display_timing();
    std::vector<uint32_t> pixels(t.width * t.height, 0xdeadbeef);
    v.render_frame({pixels.data(), t.width, t.height, t.width});
    return pixels;
}
// Expected opaque color for the fixture's palette: blue equals index i.
static uint32_t color(unsigned i) { return 0xff000000 | i; }

// A per-bit truth table rather than a copy of the byte-wide implementation.
// IBM pp2-79--88: mode1 ignores every control except map/address selection.
// cpu is the written byte; plane chooses a map; sr/esr are set/reset and its
// enable bits; mask selects changed bits; latch holds that plane's prior read.
// Return the expected byte after rotation and the selected logical function.
static uint8_t written(unsigned mode, unsigned function, unsigned rotate, unsigned cpu,
                       unsigned plane, unsigned sr, unsigned esr, unsigned mask, unsigned latch) {
    if (mode == 1) return latch;
    unsigned result = 0;
    for (unsigned b = 0; b < 8; ++b) {
        bool old = (latch >> b) & 1;
        bool rotated = (cpu >> ((b + rotate) % 8)) & 1;
        bool source = mode == 0 ? (((esr >> plane) & 1) ? ((sr >> plane) & 1) : rotated)
                    : mode == 2 ? ((cpu >> plane) & 1) : ((sr >> plane) & 1);
        bool selected = ((mask >> b) & 1) && (mode != 3 || rotated);
        bool value = function == 0 ? source : function == 1 ? source && old
                   : function == 2 ? source || old : source != old;
        result |= unsigned(selected ? value : old) << b;
    }
    return result;
}
static void memory_checks() {
    OTI087 v; enable(v);
    const uint8_t latched[] = {0x13, 0x5c, 0xa9, 0xe6};
    for (bool packed : {false, true}) {
        oak(v, 0x21, packed ? 4 : 0); seq(v, 4, packed ? 14 : 6);
        for (unsigned wm = 0; wm < 4; ++wm)
        for (unsigned fn = 0; fn < 4; ++fn)
        for (unsigned rotate = 0; rotate < 8; ++rotate)
        for (unsigned mask : {0U, 0x3cU, 0x81U, 255U})
        for (unsigned maps = 0; maps < 16; ++maps) {
            gc(v, 0, 5); gc(v, 1, 9); gc(v, 3, fn * 8 + rotate); gc(v, 5, wm); gc(v, 8, mask);
            seq(v, 2, maps);
            for (unsigned p = 0; p < 4; ++p) {
                v.mem_linear_w(0x80 + p, latched[p]); v.mem_linear_w(0x100 + p, 0x77);
            }
            v.memory_r(0xa0000 + (packed ? 0x80 : 0x20));
            if (packed) {
                for (unsigned p = 0; p < 4; ++p) v.memory_w(0xa0100 + p, 0x96);
            } else v.memory_w(0xa0040, 0x96);
            for (unsigned p = 0; p < 4; ++p)
                EQ(v.mem_linear_r(0x100 + p), (maps & (1 << p))
                    ? written(wm, fn, rotate, 0x96, p, 5, 9, mask, latched[p]) : 0x77);
        }
        gc(v, 5, 8);
        for (unsigned compare = 0; compare < 16; ++compare)
        for (unsigned care = 0; care < 16; ++care) {
            gc(v, 2, compare); gc(v, 7, care);
            unsigned expected = 0;
            for (unsigned b = 0; b < 8; ++b) {
                bool matches = true;
                for (unsigned p = 0; p < 4; ++p)
                    if ((care & (1 << p)) && (((latched[p] >> b) ^ (compare >> p)) & 1)) matches = false;
                expected |= unsigned(matches) << b;
            }
            EQ(v.memory_r(0xa0000 + (packed ? 0x80 : 0x20)), expected);
        }
    }
    // Oak p42: independent read/write segments; planar bank steps are 256KB.
    gc(v, 5, 0); gc(v, 0, 0); gc(v, 1, 0); gc(v, 3, 0); gc(v, 8, 255); seq(v, 2, 15);
    for (bool packed : {false, true}) {
        oak(v, 0x21, packed ? 4 : 0); seq(v, 4, packed ? 14 : 6);
        for (unsigned bank = 0; bank < (packed ? 16U : 4U); ++bank) {
            oak(v, 0x24, bank); v.memory_w(0xa0010, 0x30 + bank);
            unsigned base = bank * (packed ? 0x10000 : 0x40000) + (packed ? 0x10 : 0x40);
            EQ(v.mem_linear_r(base), 0x30 + bank);
        }
        for (unsigned bank = 0; bank < (packed ? 16U : 4U); ++bank) {
            oak(v, 0x23, bank); EQ(v.memory_r(0xa0010), 0x30 + bank);
        }
    }
    oak(v, 0x25, 0); oak(v, 0x21, 0); seq(v, 4, 2); seq(v, 2, 15);
    // IBM pp2-54, 2-84, 2-86: odd/even access selects the plane pair;
    // GC chaining replaces A0 within that mode, without shifting the address.
    gc(v, 6, 7); gc(v, 5, 0x10);
    v.memory_w(0xa0002, 0x61); v.memory_w(0xa0003, 0x72);
    EQ(v.mem_linear_r(8), 0x61); EQ(v.mem_linear_r(9), 0x72);
    EQ(v.memory_r(0xa0002), 0x61); EQ(v.memory_r(0xa0003), 0x72);
    gc(v, 6, 5); v.memory_w(0xa0003, 0x93); EQ(v.mem_linear_r(13), 0x93);
    // IBM memory window boundaries (p2-86).
    for (unsigned map = 0; map < 4; ++map) {
        const unsigned base[] = {0xa0000, 0xa0000, 0xb0000, 0xb8000};
        const unsigned size[] = {0x20000, 0x10000, 0x8000, 0x8000};
        gc(v, 6, (map << 2) | 1);
        EQ(v.memory_read_selected(base[map]), true); EQ(v.memory_read_selected(base[map]+size[map]-1), true);
        EQ(v.memory_read_selected(base[map]-1), false); EQ(v.memory_read_selected(base[map]+size[map]), false);
    }
    std::puts("PASS IBM VGA memory truth tables, read compare, segments, odd/even and apertures");
}

static void font_address_checks() {
    // IBM VGA/XGA Technical Reference pp2-54, 2-84, 2-86: SEQ4 bit2
    // disables odd/even writes; GC5 bit4 selects odd/even reads. GC6 bit1
    // does not turn sequential map access into odd/even addressing.
    // Second Sight reference, firmware/v1.5/src/text.asm copy_user_font and
    // textsetup.asm copy_font_8x16/copy_font_8x8: consecutive source bytes
    // occupy the first 16 or 8 rows of each 32-byte plane-2 character cell.
    std::array<uint8_t, 4096> font;
    for (unsigned glyph = 0; glyph < 256; ++glyph)
        for (unsigned row = 0; row < 16; ++row)
            font[glyph * 16 + row] = row == 0 || row == 15 ? 0x81
                : row == 2 ? glyph : row == 3 ? glyph ^ 255
                : ((glyph << (row % 8) | glyph >> (8 - row % 8)) ^ (row * 29));
    const uint8_t glyph0[] = {0x81,0x1d,0x00,0xff,0x74,0x91,0xae,0xcb,
                             0xe8,0x05,0x22,0x3f,0x5c,0x79,0x96,0x81};
    for (unsigned row = 0; row < 16; ++row) EQ(font[row], glyph0[row]);
    for (unsigned height : {8U, 16U}) for (unsigned map : {0U, 3U}) {
        OTI087 v; enable(v);
        seq(v, 2, 4); seq(v, 4, 6); gc(v, 5, 0); gc(v, 6, (map << 2) | 2);
        gc(v, 4, 2);
        unsigned const base = map == 0 ? 0xa0000 : 0xb8000;
        for (unsigned a = 0; a < 8192 * 4; ++a) v.mem_linear_w(a, 3);
        for (unsigned glyph = 0; glyph < 256; ++glyph)
            for (unsigned row = 0; row < height; ++row)
                v.memory_w(base + glyph * 32 + row, font[glyph * height + row]);
        for (unsigned glyph = 0; glyph < 256; ++glyph)
            for (unsigned row = 0; row < 32; ++row) {
                unsigned const address = glyph * 32 + row;
                unsigned const expected = row < height ? font[glyph * height + row] : 3;
                for (unsigned plane = 0; plane < 4; ++plane)
                    EQ(v.mem_linear_r(address * 4 + plane), plane == 2 ? expected : 3);
                EQ(v.memory_r(base + address), expected);
            }
    }
    // Independently vary the read/write odd/even controls and address mux.
    for (unsigned write_oe = 0; write_oe < 2; ++write_oe)
    for (unsigned read_oe = 0; read_oe < 2; ++read_oe)
    for (unsigned chain = 0; chain < 2; ++chain) {
        OTI087 v; enable(v);
        seq(v, 4, write_oe ? 2 : 6); gc(v, 5, read_oe << 4); gc(v, 6, chain << 1);
        for (unsigned a = 0; a < 8; ++a) v.memory_w(0xa0000 + a, 0x80 + a);
        for (unsigned a = 0; a < 8; ++a) for (unsigned plane = 0; plane < 4; ++plane) {
            unsigned expected = 0;
            if (!write_oe) expected = 0x80 + a;
            else if (chain) {
                if (!(a & 1)) expected = 0x80 + a + (plane & 1);
            } else if ((a & 1) == (plane & 1)) expected = 0x80 + a;
            EQ(v.mem_linear_r(a * 4 + plane), expected);
        }
        for (unsigned a = 0; a < 8; ++a) for (unsigned plane = 0; plane < 4; ++plane)
            v.mem_linear_w(a * 4 + plane, 0x40 + a * 4 + plane);
        for (unsigned selected = 0; selected < 4; ++selected) {
            gc(v, 4, selected);
            for (unsigned a = 0; a < 8; ++a) {
                unsigned const offset = read_oe && chain ? a & ~1U : a;
                unsigned const plane = read_oe ? (selected & 2) | (a & 1) : selected;
                EQ(v.memory_r(0xa0000 + a), 0x40 + offset * 4 + plane);
            }
        }
    }
    std::puts("PASS sequential user-font rows and independent odd/even read/write controls");
}

static void oak_checks() {
    OTI087 v; enable(v); oak(v, 0x21, 4); seq(v, 4, 14);
    // Oak wide-latch enable must not widen ordinary mode-0 byte uploads.
    for(unsigned wide : {0U,16U}) for(unsigned maps=0;maps<16;++maps) {
        oak(v,0x22,wide); seq(v,2,maps);
        for(unsigned p=0;p<4;++p) v.mem_linear_w(p,0xee);
        for(unsigned p=0;p<4;++p) v.memory_w(0xa0000+p,0x31+p);
        for(unsigned p=0;p<4;++p) EQ(v.mem_linear_r(p),(maps&(1<<p)) ? 0x31+p : 0xee);
    }
    // Oak databook p44: color expansion. Second Sight reference,
    // docs/15-oak-drawing.md, "Color expansion: 4 pixels from 4 bits": each
    // 8-bit host write fills four pixels, selected by A0, with the wide
    // latch either off or on.
    for (unsigned wide : {0U, 16U})
    for (unsigned order : {0U, 16U}) {
        oak(v, 0x22, wide); oak(v, 0x30, 13 | order); oak(v, 0x31, 0xa5); oak(v, 0x32, 0x5a);
        oak(v, 0x33, 0xb2); oak(v, 0x34, 0x96); seq(v, 2, 0);
        for (unsigned i = 0; i < 8; ++i) v.mem_linear_w(i, 0xee);
        v.memory_w(0xa0000, 0); v.memory_w(0xa0001, 0);
        for (unsigned i = 0; i < 8; ++i) {
            bool mask = (0x96 >> (order ? 7-i : i)) & 1;
            EQ(v.mem_linear_r(i), mask ? ((0xb2 >> (7-i)) & 1 ? 0xa5 : 0x5a) : 0xee);
        }
    }
    // Explicit planar exception in Oak p44: high/low nibble plane enables.
    oak(v, 0x21, 0); seq(v, 4, 6); oak(v, 0x30, 15); oak(v, 0x34, 0x69);
    oak(v, 0x33, 0x96); oak(v, 0x31, 5); oak(v, 0x32, 10);
    for (unsigned a = 0; a < 2; ++a) {
        for (unsigned p = 0; p < 4; ++p) v.mem_linear_w(4*a+p, 0xee);
        v.memory_w(0xa0000+a, 0);
        for (unsigned p = 0; p < 4; ++p) EQ(v.mem_linear_r(4*a+p), ((a ? 9 : 6) & (1<<p))
            ? ((5 & (1<<p)) ? 0x96 : 0x69) : 0xee);
    }
    oak(v, 0x21, 4); seq(v, 4, 14); oak(v, 0x30, 8); oak(v, 0x34, 255); gc(v, 5, 1);
    for (unsigned swap : {0U, 32U}) {
        oak(v, 0x22, 16 | swap); oak(v, 0x35, 0);
        oak(v, 0x35, 0); v.io_w(0x3de, 0x36);
        for (unsigned i = 0; i < 8; ++i) v.io_w(0x3df, 0x40+i);
        EQ(read(v, 0x3de, 0x35), 0);
        for (unsigned address = 0; address < 8; ++address) {
            for (unsigned i = 0; i < 8; ++i) v.mem_linear_w(0x100+i, 0xee);
            v.memory_w(0xa0100+address, 0);
            for (unsigned i = 0; i < 8; ++i) EQ(v.mem_linear_r(0x100+i), (i/4 == (address&1))
                ? 0x40 + (i ^ (swap ? 4 : 0)) : 0xee);
        }
    }
    // Exercise every mask on both four-byte lane selections, with mask
    // reversal and source-half swap independent (Oak pp42--44; Second Sight
    // reference, docs/06-video.md, "The 64-bit latch lane rule"). Disabled
    // lanes retain memory.
    for (unsigned swap : {0U,32U}) for (unsigned order : {0U,16U}) {
        oak(v,0x22,16|swap); oak(v,0x30,8|order);
        for (unsigned mask=0;mask<256;++mask) {
            oak(v,0x34,mask);
            for (unsigned address=0;address<8;++address) {
                for (unsigned i=0;i<8;++i) v.mem_linear_w(0x100+i,0xee);
                v.memory_w(0xa0100+address,0);
                for (unsigned i=0;i<8;++i) {
                    bool selected = i/4==(address&1) && ((mask>>(order ? 7-i : i))&1);
                    EQ(v.mem_linear_r(0x100+i),selected ? 0x40+(i^(swap ? 4 : 0)) : 0xee);
                }
            }
        }
    }
    // Content-addressable byte selection, auto-increment, non-destructive peek.
    oak(v, 0x22, 1); oak(v, 0x30, 0); seq(v, 2, 15);
    for (unsigned i = 0; i < 8; ++i) { v.memory_w(0xa0200+i, 0); EQ(v.mem_linear_r(0x200+i), 0x40+i); }
    oak(v, 0x35, 7); v.io_w(0x3de, 0x36); EQ(v.io_r(0x3df, false), 0x47); EQ(v.io_r(0x3df), 0x47);
    EQ(read(v, 0x3de, 0x35), 0);
    // Swap is orthogonal to content-addressable selection (Oak p42).
    // Second Sight reference, docs/15-oak-drawing.md, "Filling from the latch
    // registers": content addressing stores one byte at its own pixel.
    oak(v,0x22,0x21); oak(v,0x30,8); oak(v,0x34,255);
    for(unsigned i=0;i<8;++i) v.mem_linear_w(0x300+i,0xee);
    v.memory_w(0xa0301,0);
    for(unsigned i=0;i<8;++i) EQ(v.mem_linear_r(0x300+i),i==1 ? 0x45 : 0xee);
    oak(v,0x30,0);
    // Linear aperture and board half flag address the same physical memory.
    gc(v, 5, 0); oak(v, 5, 0x19); oak(v, 0x22, 0);
    v.memory_w(0x180010, 0x7b); EQ(v.mem_linear_r(0x80010), 0x7b);
    EQ(v.memory_read_selected(0xfffff), false); EQ(v.memory_read_selected(0x200000), false);
    // Oak register aliases and all OTI-068 clock selects (p35, generator p7).
    const unsigned clocks[] = {25175000,28322000,65000000,44900000,28322000,36000000,40000000,36000000,
                               25175000,28322000,78000000,65000000,63000000,72000000,40000000,50000000};
    for (unsigned c = 0; c < 16; ++c) { oak(v, 6, c); EQ(v.vclk_hz(), clocks[c]); EQ((v.io_r(0x3cc)>>2)&3, c&3); }
    v.io_w(0x3c2, 3|8); EQ(read(v,0x3de,6)&3,2);
    oak(v, 0x0d, 0); EQ(read(v,0x3de,6)&4,0);
    oak(v, 0x14, 8); EQ(read(v,0x3de,0x17)&1,1);
    oak(v, 0x17, 6); EQ(read(v,0x3de,0x14)&8,0);
    oak(v, 0x25, 31); EQ(read(v,0x3de,0x23),31); EQ(read(v,0x3de,0x24),31);
    for (unsigned size=0; size<4; ++size) { oak(v,2,size*2+1); EQ(read(v,0x3de,2),size*2); }
    oak(v,0x19,4); v.io_w(0x342,0x7f); EQ(v.io_r(0x3c6),0x7f);
    oak(v,0x19,15); EQ(v.io_read_selected(0x342),false);
    std::puts("PASS Oak expansion, packed lane conventions, planar mask, latch registers, aperture and aliases");
}

static void timing_checks() {
    OTI087 v; mode(v);
    EQ(v.display_timing().width,32); EQ(v.display_timing().height,8);
    beam(v,2,1); cr(v,0x0d,0x20);
    beam(v,8); EQ(v.io_r(0x3c2)&0x80,0x80); EQ(v.cint_r(),true);
    EQ(v.display_state().start,0); EQ(v.io_r(0x3da)&8,0);
    beam(v,10); EQ(v.display_state().start,0); EQ(v.io_r(0x3da)&8,0);
    beam(v,12); EQ(v.display_state().start,0x20); EQ(v.io_r(0x3da)&8,8);
    cr(v,0x11,0x2e); EQ(v.cint_r(),false); EQ(v.io_r(0x3c2)&0x80,0);
    cr(v,0x11,0x3e); beam(v,8); EQ(v.cint_r(),false); EQ(v.io_r(0x3c2)&0x80,0x80);
    cr(v,0x11,0x1e); EQ(v.cint_r(),true);
    cr(v,0x11,0); cr(v,0x16,0xc7); EQ(read(v,0x3d4,0x16),0xc7);
    // Register protection leaves the line-compare overflow bit writable.
    cr(v,0x11,0x80); cr(v,0,0x77); EQ(read(v,0x3d4,0),15);
    cr(v,7,0); EQ(read(v,0x3d4,7)&0x10,0);
    cr(v,0x11,0); cr(v,0x17,0xc7); EQ(v.display_timing().total_height,32); EQ(v.display_timing().height,16);
    cr(v,0x17,0xc3); auto period=v.display_timing().frame_period(); seq(v,1,9);
    EQ(std::abs(v.display_timing().frame_period()/period-2)<1e-9,true);
    seq(v,1,1); oak(v,0x14,0x80); oak(v,0x15,3); EQ(v.display_timing().total_height,32);
    beam(v,28,0); EQ(v.io_r(0x3da)&8,0); beam(v,28,12); EQ(v.io_r(0x3da)&8,8);
    // Interlaced beam lines 0..15 and 16..31 contain the even/odd fields.
    // Their 16 visible image rows end at beam line24, not beam line16.
    beam(v,0); auto frames=v.frame_number(); beam(v,24); EQ(v.frame_number(),frames);
    beam(v,0); EQ(v.frame_number(),frames+1);
    // Oak p40 specifies the odd-field VSYNC position in characters. The
    // model infers that the start-address latch follows that same edge.
    // Seq1 bit0 alone selects eight or nine dots, in alpha and graphics alike.
    // The fixture selects a two-dot pixel divisor and tests both sides of the
    // inferred edge without taking its expected position from the model.
    for (unsigned graphics : {0U, 1U}) for (unsigned clock8 : {0U, 1U})
        for (unsigned position : {0U, 1U, 3U, 7U}) {
            OTI087 scan; mode(scan);
            gc(scan, 6, 4 | graphics); seq(scan, 1, clock8);
            oak(scan, 0x14, 0x80); oak(scan, 0x15, position);
            unsigned phase = position * (clock8 ? 8 : 9) / 2;
            beam(scan, 27); cr(scan, 0x0d, 0x20);
            if (phase) {
                beam(scan, 28, phase - 1);
                EQ(scan.display_state().start, 0);
                EQ(scan.io_r(0x3da) & 8, 0);
            }
            beam(scan, 28, phase);
            EQ(scan.display_state().start, 0x20);
            EQ(scan.io_r(0x3da) & 8, 8);
        }
    // Sequential Systems' disk mode files SECOND/MISC/XX and SECOND/MISC/XXX
    // describe two planar modes with 40 displayed characters. Switching from
    // eight to nine dots per character and changing CRTC $00/$02/$04 shortens
    // the line from 68 to 62 characters: 360 instead of 320 pixels at a
    // similar line rate.
    for (unsigned xxx : {0U, 1U}) {
        OTI087 v; mode(v, false, false);
        seq(v, 1, xxx ? 0 : 1); cr(v, 0, xxx ? 0x39 : 0x3f); cr(v, 1, 0x27);
        cr(v, 3, 0x9a); cr(v, 4, xxx ? 0x31 : 0x33); cr(v, 5, 0x9a);
        EQ(v.display_timing().width, xxx ? 360 : 320);
        EQ(v.display_timing().total_width, xxx ? 558 : 544);
        unsigned const sync = xxx ? 49 * 9 : 51 * 8;
        beam(v, 0, sync - 1); EQ(v.hsync_r(), 0);
        beam(v, 0, sync); EQ(v.hsync_r(), 1);
    }
    // A nine-dot graphics character shows its eight serializer bits, then
    // index zero. Bytes are maps 0-3 of characters 0 and 1; the lists are
    // line 0 palette indices with eight-dot and nine-dot characters.
    struct NineDots { unsigned gc5, attr10; uint8_t maps[8]; std::vector<unsigned> eight, nine; };
    NineDots const nine_dots[] = {
        {0x00, 0x01, {0xf1, 0x3c, 0, 0, 0, 0, 0x81, 0xff}, // planar
         {1,1,3,3,2,2,0,1, 12,8,8,8,8,8,8,12}, {1,1,3,3,2,2,0,1,0, 12,8,8,8,8,8,8,12,0}},
        {0x20, 0x01, {0x1b, 0xe4, 0, 0x03, 0x55, 0xaa, 0, 0}, // CGA shift
         {0,1,2,3,3,2,1,12, 1,1,1,1,2,2,2,2}, {0,1,2,3,3,2,1,12,0, 1,1,1,1,2,2,2,2,0}},
        {0x40, 0x01, {0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0xed, 0x1f}, // 256-color, PW=0
         {1,2,3,4,5,6,7,8, 9,10,11,12,13,14,15,1}, {1,2,3,4,5,6,7,8,0, 9,10,11,12,13,14,15,1,0}},
        {0x40, 0x41, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1}, // 256-color, PW=1
         {0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf1}, {0x12,0x34,0x56,0x78,0x09,0xab,0xcd,0xef,0x10}},
    };
    for (auto const &c : nine_dots) for (unsigned nine : {0U, 1U}) {
        OTI087 v; mode(v, false, false);
        gc(v, 5, c.gc5); attr(v, 0x10, c.attr10); seq(v, 1, nine ? 0 : 1);
        for (unsigned i = 0; i < 8; ++i) v.mem_linear_w(i, c.maps[i]);
        beam(v, 12); beam(v, 0);
        auto const pixels = frame(v);
        auto const &expected = nine ? c.nine : c.eight;
        for (unsigned x = 0; x < expected.size(); ++x) EQ(pixels[x], color(expected[x]));
    }
    std::puts("PASS independent interrupt/retrace/blank edges, protection, overflow, divider and interlace phase");
}

static void deadline_checks() {
    OTI087 v; mode(v,true);
    double edge = v.time() + v.time_until_pos(8);
    EQ(std::abs(v.next_event()-edge)<1e-12,true);
    // READY may precede a raster edge without moving it. A timing-register
    // write must immediately replace the pending raster deadline.
    v.memory_r(0xa0000);
    EQ(std::abs(v.next_event()-v.time()-v.access_wait())<1e-12,true);
    v.advance(v.next_event());
    EQ(std::abs(v.next_event()-edge)<1e-12,true);
    seq(v,1,9);
    beam(v,0);
    edge = v.time() + v.time_until_pos(8);
    EQ(std::abs(v.next_event()-edge)<1e-12,true);
    // Give READY the same deadline as display end. Both signals must fire,
    // followed by retrace's independent address latch (IBM pp2-67/70).
    v.set_access_wait(edge-v.time());
    unsigned ready_edges=0;
    v.ready_callback=[&](bool ready) { if(ready) ++ready_edges; };
    v.memory_r(0xa0000); cr(v,0x0d,0x20);
    v.advance(edge);
    EQ(ready_edges,1); EQ(v.cint_r(),true); EQ(v.display_state().start,0);
    EQ(std::abs(v.next_event()-v.time()-v.time_until_pos(12))<1e-12,true);
    v.advance(v.next_event()); EQ(v.display_state().start,0x100);
    v.reset(); EQ(v.next_event()>v.time(),true);
    v.advance(v.next_event()); EQ(v.next_event()>v.time(),true);
    std::puts("PASS raster deadline retiming, READY overlap, retrace latch and reset");
}

static void raster_checks() {
    OTI087 v; mode(v);
    for (unsigned a=0; a<512; ++a) for(unsigned p=0;p<4;++p) v.mem_linear_w(a*4+p,a&255);
    beam(v,2,3); cr(v,0x13,1); // row 2 retains address 16; subsequent rows add 2.
    beam(v,8);
    auto pixels=frame(v);
    const unsigned rows[]={0,8,16,18,20,22,24,26};
    for(unsigned y=0;y<8;++y) EQ(pixels[y*32],color(rows[y]));
    EQ(read(v,0x3d4,0x13),1); // rendering does not alter live registers.
    // Split at line 3 clears the address regardless of the earlier offset.
    mode(v); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,1);
    beam(v,3,8);
    v.io_w(0x3c8,1); v.io_w(0x3c9,0x44); v.io_w(0x3c9,0x55);
    EQ(v.dac().pixel_select(1),color(1)); v.io_w(0x3c9,0x66);
    beam(v,8); pixels=frame(v);
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<32;++x)
        EQ(pixels[y*32+x],y<3||(y==3&&x<8)?color(1):0xff445566);
    mode(v); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,0x21);
    beam(v,2,5); v.io_w(0x3c6,0x0f); beam(v,8); pixels=frame(v);
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<32;++x)
        EQ(pixels[y*32+x],y<2||(y==2&&x<5)?color(0x21):color(1));
    // Byte-mode unchained C256: pixel p from plane p, plus IBM pan table.
    mode(v); v.io_w(0x3c6,255);
    for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,a&255);
    for(unsigned pan=0;pan<4;++pan) {
        attr(v,0x13,pan*2); beam(v,12); beam(v,0); pixels=frame(v);
        for(unsigned x=0;x<32;++x) EQ(pixels[x],color(x+pan));
    }
    cr(v,0x18,2); cr(v,7,0); cr(v,9,0); attr(v,0x10,0x61);
    beam(v,12); beam(v,0); pixels=frame(v);
    EQ(pixels[3*32],color(0)); EQ(pixels[2*32],color(67));
    // IBM p2-75: DWORD wraps MA12/13 to address bits0/1.
    mode(v); cr(v,0x14,0x40); cr(v,0x0c,0x10); cr(v,0x0d,0);
    v.mem_linear_w(0x10004,0x7d); beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(0x7d));
    std::puts("PASS cumulative offset, pixel-level palette/mask changes, unchained scanout, panning and split");
}

static void text_and_cga_checks() {
    OTI087 v; mode(v,false,false); gc(v,6,4); attr(v,0x10,0); seq(v,3,4);
    v.mem_linear_w(0,1); v.mem_linear_w(1,9);
    v.mem_linear_w((2*8192+32)*4+2,0x80); v.mem_linear_w(32*4+2,0);
    auto pixels=frame(v); EQ(pixels[0],color(1)); EQ(pixels[1],color(0));
    seq(v,3,0); v.mem_linear_w(32*4+2,0x80); pixels=frame(v); EQ(pixels[0],color(9));
    // Underline across eight dots, ninth dot only for line graphics (IBM p2-99).
    cr(v,9,7); cr(v,0x14,2); v.mem_linear_w(1,1); pixels=frame(v);
    for(unsigned x=0;x<8;++x) EQ(pixels[2*64+x],color(1));
    EQ(pixels[3*64+1],color(0));
    // CGA data comes from maps0/1, with maps2/3 supplying the upper pair.
    mode(v,false,false); gc(v,5,0x20);
    const uint8_t bytes[]={0x1b,0xe4,0x6c,0x93};
    for(unsigned p=0;p<4;++p) v.mem_linear_w(p,bytes[p]);
    pixels=frame(v);
    const unsigned expected[]={4,9,14,3,11,6,1,12};
    for(unsigned x=0;x<8;++x) EQ(pixels[x],color(expected[x]));
    attr(v,0x12,3); pixels=frame(v);
    for(unsigned x=0;x<8;++x) EQ(pixels[x],color(expected[x]&3));
    std::puts("PASS text font/intensity/underline and four-map CGA serialization");
}

// Brooktree L481A_C pp6-8,14-20. These test the chip directly, then the
// raster fixtures above check that its committed output changes reach Oak.
static void dac_checks() {
    Bt481 d;
    for (unsigned depth : {6U,8U}) {
        d.command_w(1); d.address_w(2); d.mask_w(depth==8 ? 0x1e : 0x1c); d.command_w(0);
        for (unsigned index=0;index<256;++index) {
            d.address_w(index); d.palette_w(index); d.palette_w(index^0x55); d.palette_w(index^0xaa);
            EQ(d.address_r(),(index+1)&255);
            d.address_read_w(index); EQ(d.address_r(),(index+1)&255);
            EQ(d.palette_r(false),index & (depth==8 ? 255 : 63));
            EQ(d.palette_r(),index & (depth==8 ? 255 : 63));
            EQ(d.palette_r(),(index^0x55) & (depth==8 ? 255 : 63));
            EQ(d.palette_r(),(index^0xaa) & (depth==8 ? 255 : 63));
            EQ(d.address_r(),(index+2)&255);
        }
    }
    for (unsigned i=0;i<256;++i) d.set_color(i,VideoColor(i,i^0x55,i^0xaa));
    for (unsigned mask=0;mask<256;++mask) {
        d.mask_w(mask);
        for (unsigned i=0;i<256;++i) EQ(d.pixel_select(i),VideoColor(i&mask,(i&mask)^0x55,(i&mask)^0xaa));
    }
    d.mask_w(255);
    // All 15/16-bit input words, including discarded top/low component bits.
    for (unsigned format : {0x80U,0xc0U}) {
        d.command_w(format); EQ(d.bytes_per_pixel(),2);
        for (unsigned word=0;word<65536;++word) {
            unsigned b=(word%32)*8;
            unsigned g=(word/32%(format==0xc0 ? 64 : 32))*(format==0xc0 ? 4 : 8);
            unsigned r=(word/(format==0xc0 ? 2048 : 1024)%32)*8;
            EQ(d.direct_color(word),VideoColor(r,g,b));
        }
    }
    for (unsigned format : {0x90U,0x92U,0xb0U,0xb2U}) {
        d.command_w(format); EQ(d.bytes_per_pixel(),format&32 ? 3 : 4);
        EQ(d.direct_color(0x00123456),format&2 ? 0xff123456 : 0xff563412);
        EQ(d.direct_color(0x80123456),format&32 ? (format&2 ? 0xff123456 : 0xff563412) : uint32_t(d.get_color(128)));
    }
    // Hidden command access consumes four reads, not debugger peeks.
    d.command_w(0);
    for (unsigned i=0;i<8;++i) d.vga_r(2,false);
    d.vga_w(2,0x7f); EQ(d.command_r(),0); EQ(d.pixel_mask(),0x7f);
    for (unsigned i=0;i<4;++i) d.vga_r(2);
    d.vga_w(2,0x84); EQ(d.command_r(),0x84); EQ(d.vga_r(2),0x84);
    d.vga_w(2,0); EQ(d.command_r(),0);
    // Overlay-mask writes stage until the next pixel-mask write (p14).
    d.command_w(1); d.address_w(1); d.mask_w(5); EQ(d.overlay_pixel_mask(),15);
    d.address_w(0); d.mask_w(255); EQ(d.overlay_pixel_mask(),5);
    d.set_color(261,VideoColor(0x12,0x34,0x56)); EQ(d.pixel_select(0,15),0xff123456);
    // Passive snapshots cannot consume palette components or hidden reads.
    d.command_w(0); d.address_read_w(37);
    auto state=d.access_state(); const Bt481 &view=d;
    for(unsigned i=0;i<8;++i) { EQ(view.access_state().component,state.component); EQ(view.pixel_mask(),255); }
    EQ(d.palette_r(),37);
    std::puts("PASS Bt481 palette sequencing/precision, all masks, direct formats, command access and passive snapshots");
}

static void address_checks() {
    // IBM p2-75 byte/word/dword address mux, p2-64 byte panning and
    // pp2-73/77 row-scan substitutions. Each tuple gives an independently
    // specified counter and expected map address, not the renderer's formula.
    struct Example { unsigned counter, mode, underline, address; };
    const Example examples[] = {
        {0x1234,0xc3,0,0x1234}, {0x2000,0x83,0,0x4001},
        {0x8000,0xa3,0,1}, {0x1000,0xc3,0x40,0x4001},
        {0x2000,0xc3,0x40,0x8002}, {0x3000,0xc3,0x40,0xc003}
    };
    for (auto e : examples) {
        OTI087 v; mode(v); cr(v,0x17,e.mode); cr(v,0x14,e.underline);
        cr(v,0xc,e.counter>>8); cr(v,0xd,e.counter); v.mem_linear_w(e.address*4,0x79);
        beam(v,12); beam(v,0); auto pixels=frame(v); EQ(pixels[0],color(0x79));
    }
    OTI087 v; mode(v);
    for(unsigned a=0;a<1024;++a) for(unsigned p=0;p<4;++p) v.mem_linear_w(4*a+p,a&255);
    cr(v,8,0x20); auto pixels=frame(v); EQ(pixels[0],color(1)); // one byte-pan address
    cr(v,8,2); cr(v,9,0x43); pixels=frame(v);
    const unsigned preset[]={0,0,8,8,8,8,16,16};
    for(unsigned y=0;y<8;++y) EQ(pixels[32*y],color(preset[y]));
    cr(v,8,0); cr(v,9,0xc0); pixels=frame(v);
    for(unsigned y=0;y<8;++y) EQ(pixels[32*y],color((y/2)*8));
    cr(v,9,0x43); cr(v,0x17,0xc0);
    v.mem_linear_w(0,1); v.mem_linear_w(0x8000,2); v.mem_linear_w(0x10000,3); v.mem_linear_w(0x18000,4);
    pixels=frame(v); for(unsigned y=0;y<4;++y) EQ(pixels[32*y],color(y+1));
    // Compare bits 8 and 9 must not be discarded by either register write.
    cr(v,0x18,0x9a); cr(v,7,0x10); cr(v,9,0x40);
    const OTI087 &view=v; EQ(view.crtc_state().line_compare,0x39a);
    cr(v,7,0); EQ(view.crtc_state().line_compare,0x29a);
    cr(v,9,0); EQ(view.crtc_state().line_compare,0x9a);
    // A write exactly at row start applies to the next addition, not the
    // addition which has already delivered this row (IBM p2-103).
    mode(v); for(unsigned a=0;a<256;++a) for(unsigned p=0;p<4;++p) v.mem_linear_w(4*a+p,a);
    beam(v,2); cr(v,0x13,1); beam(v,8); pixels=frame(v);
    const unsigned offsets[]={0,8,16,18,20,22,24,26};
    for(unsigned y=0;y<8;++y) EQ(pixels[32*y],color(offsets[y]));
    std::puts("PASS byte/word/dword mux, byte pan, preset, double scan, row substitutions and offset edge");
}

static void display_control_checks() {
    OTI087 v; mode(v);
    for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,1);
    // Blanking is independent of display end (IBM pp2-58/73). Vertical
    // blank begins at its start register and stops before its end register.
    cr(v,2,2); cr(v,3,4); cr(v,0x15,2); cr(v,0x16,4);
    auto pixels=frame(v);
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<32;++x)
        EQ(pixels[y*32+x],(x>=8&&x<16)||(y>=2&&y<=3) ? color(0) : color(1));
    beam(v,3,0); EQ(v.io_r(0x3da)&1,1); beam(v,6,0); EQ(v.io_r(0x3da)&1,0);
    // Sync skew changes the pulse edges; screen off keeps those pulses.
    mode(v); cr(v,5,0x30); beam(v,0,48); EQ(v.hsync_r(),0);
    beam(v,0,52); EQ(v.hsync_r(),1); seq(v,1,0x21);
    EQ(v.hsync_r(),1); pixels=frame(v);
    // The mid-line screen-off preserves the part already scanned.
    EQ(pixels[0],color(1)); EQ(pixels[32],color(0));
    // Restore at frame start so the following checks use a constant mode.
    mode(v); cr(v,3,0x34); attr(v,0x11,0x2a); pixels=frame(v);
    EQ(pixels[0],color(0x2a)); EQ(pixels[4],color(1));
    // PS and Color Select replace P5:P4 and supply P7:P6 (IBM pp2-92/96).
    mode(v,false,false); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,0);
    v.mem_linear_w(0,0xff); attr(v,1,0x25); attr(v,0x14,0x0a);
    pixels=frame(v); EQ(pixels[0],color(0xa5));
    attr(v,0x10,0x81); pixels=frame(v); EQ(pixels[0],color(0xa5));
    attr(v,0x14,0x09); pixels=frame(v); EQ(pixels[0],color(0x95));
    // C256 has a four-bit stream when attribute PW is clear (IBM p2-22).
    mode(v); attr(v,0x10,1); v.mem_linear_w(0,0xa3); pixels=frame(v);
    EQ(pixels[0],color(3)); EQ(pixels[1],color(10));
    attr(v,0x12,5); pixels=frame(v); EQ(pixels[0],color(1)); EQ(pixels[1],color(0));
    // Nine-dot panning table: 8 means no shift; 0 means one dot (p2-95).
    mode(v,false,false); gc(v,6,4); attr(v,0x10,0); seq(v,1,0);
    v.mem_linear_w(0,1); v.mem_linear_w(1,1); v.mem_linear_w(32*4+2,0x80);
    attr(v,0x13,8); beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(1));
    attr(v,0x13,0); beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(0));
    // Mode F's four logical colors pass through its programmable palette.
    mode(v,false,false); attr(v,0x10,0x0b); attr(v,1,7); attr(v,4,7); attr(v,5,15);
    v.mem_linear_w(0,0x50); v.mem_linear_w(1,0); v.mem_linear_w(2,0x30); v.mem_linear_w(3,0);
    pixels=frame(v); EQ(pixels[0],color(0)); EQ(pixels[1],color(7));
    EQ(pixels[2],color((v.frame_number()&32) ? 0 : 7)); EQ(pixels[3],color(15));
    std::puts("PASS blanking, sync skew, screen off, display skew, color select, PW, nine-dot pan and monochrome palette");
}

static void sequencer_checks() {
    // IBM pp2-49/50 reload cadence, using the labeled Type 1 consecutive
    // map-byte convention. Word mode makes suppressed reloads observable.
    const unsigned addresses[3][8] = {
        {0,2,4,6,8,10,12,14}, {0,1,4,5,8,9,12,13}, {0,1,2,3,8,9,10,11}
    };
    OTI087 v;
    for(unsigned kind=0;kind<4;++kind) {
        mode(v,false,kind==1); cr(v,0x17,0x83);
        if(kind==2) gc(v,5,0x20);
        if(kind==3) { gc(v,6,4); attr(v,0x10,0); cr(v,0xa,0x20); }
        for(unsigned a=0;a<32;++a) for(unsigned p=0;p<4;++p) {
            unsigned value=kind==0 ? ((a>>p)&1)*255 : kind==1 ? a*4+p
                : kind==2 ? ((a>>(p<2 ? 0 : 2))&3)*0x55 : p==0 ? 1 : p==1 ? a&15 : 0;
            v.mem_linear_w(a*4+p,value);
        }
        if(kind==3) v.mem_linear_w(32*4+2,255);
        for(unsigned bits : {0U,4U,16U,20U}) {
            seq(v,1,1|bits); auto pixels=frame(v);
            unsigned group=bits&16 ? 2 : bits&4 ? 1 : 0;
            for(unsigned c=0;c<8;++c) for(unsigned pixel=0;pixel<(kind==1 ? 4U : 8U);++pixel)
                EQ(pixels[c*(kind==1 ? 4 : 8)+pixel],color(kind==1 ? addresses[group][c]*4+pixel : addresses[group][c]));
        }
    }
    mode(v,false,false); cr(v,0x17,0x8b); seq(v,1,5);
    for(unsigned a=0;a<32;++a) for(unsigned p=0;p<4;++p) v.mem_linear_w(a*4+p,((a>>p)&1)*255);
    auto pixels=frame(v);
    for(unsigned c=0;c<8;++c) EQ(pixels[c*8],color(c)); // count-by-two is independent
    mode(v,true); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,a&255);
    auto ordinary=frame(v); seq(v,1,0x15); EQ(frame(v)==ordinary,true);

    // IBM p2-54 specifies capacity only. Check the model's four 16KB maps,
    // retaining Oak segment bits; these alias equations are a convention.
    v.reset(); enable(v); seq(v,4,4);
    for(unsigned p=0;p<4;++p) v.mem_linear_w(0x1048c+p,0xee);
    for(unsigned quarter=0;quarter<4;++quarter) {
        v.memory_w(0xa0123+quarter*0x4000,0x30+quarter);
        for(unsigned p=0;p<4;++p) EQ(v.mem_linear_r(0x48c+p),0x30+quarter);
        EQ(v.memory_r(0xa0123),0x30+quarter);
    }
    for(unsigned p=0;p<4;++p) EQ(v.mem_linear_r(0x1048c+p),0xee);
    oak(v,0x25,1); v.memory_w(0xa4123,0x78);
    EQ(v.mem_linear_r(0x4048c),0x78); EQ(v.memory_r(0xac123),0x78);
    oak(v,0x25,0); EQ(v.memory_r(0xa8123),0x33);
    seq(v,4,12); v.memory_w(0xa4004,0x91); EQ(v.mem_linear_r(20),0x91);
    seq(v,4,14); v.memory_w(0xa4004,0x92); EQ(v.mem_linear_r(0x10014),0x92);
    EQ(v.mem_linear_r(20),0x91);
    mode(v); cr(v,0xc,0x40); beam(v,12); beam(v,0);
    v.mem_linear_w(0,0x31); v.mem_linear_w(0x10000,0x79);
    seq(v,4,4); pixels=frame(v); EQ(pixels[0],color(0x31));
    seq(v,4,6); pixels=frame(v); EQ(pixels[0],color(0x79));
    // The EM limit does not take memory from Oak's native packed extension.
    mode(v,true); seq(v,4,12); oak(v,0x25,1); v.memory_w(0xa4004,0x93);
    EQ(v.mem_linear_r(0x14004),0x93); EQ(v.memory_r(0xa4004),0x93);
    std::puts("PASS Type 1 SL/SH4 convention in four VGA paths, independent counting and EM capacity convention");
}

static void cursor_checks() {
    OTI087 v; mode(v,true); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,1);
    oak(v,0x48,0x20); // cursor base 0x2000 * 8 = 0x10000
    oak(v,0x4a,2); oak(v,0x4b,3);
    v.mem_linear_w(0x10000,0xcc); v.mem_linear_w(0x10008,0xaa);
    const uint32_t expected[]={color(2),color(3),color(1),color(254),
                               color(2),color(3),color(1),color(254)};
    for(unsigned reverse : {0U,4U}) {
        oak(v,0x4c,1|reverse); auto pixels=frame(v);
        for(unsigned x=0;x<8;++x) EQ(pixels[64+x],expected[reverse?7-x:x]);
        EQ(pixels[0],color(1)); // nonnegative Y=0 starts on output row 1
    }
    oak(v,0x4c,1); oak(v,0x44,61); auto pixels=frame(v);
    EQ(pixels[64+3],color(1)); // preset 61 leaves precisely three cursor columns
    oak(v,0x44,64); pixels=frame(v); EQ(pixels[64],color(1));
    oak(v,0x44,0); oak(v,0x45,64); pixels=frame(v); EQ(pixels[64],color(1));
    oak(v,0x45,0);
    // A mid-line cursor move preserves pixels already scanned. Palette and
    // pitch changes on the same line remain active alongside the cursor.
    beam(v,2,8); oak(v,0x41,16); cr(v,0x13,1); v.io_w(0x3c6,3); beam(v,8);
    pixels=frame(v); EQ(pixels[64],color(2)); EQ(pixels[2*64],color(2)); EQ(pixels[2*64+16],color(2));
    for(unsigned rate=0;rate<4;++rate) {
        mode(v,true); oak(v,0x48,0x20); oak(v,0x4a,2); oak(v,0x4b,3); oak(v,0x4c,9|(rate<<4));
        unsigned on=0,off=0,period=4<<rate;
        for(unsigned f=0;f<period*2;++f) {
            pixels=frame(v);
            if ((v.frame_number()/period)&1) { EQ(pixels[64],color(1)); ++off; }
            else { EQ(pixels[64],color(2)); ++on; }
            beam(v,0);
        }
        EQ(on,period); EQ(off,period);
    }
    std::puts("PASS Oak cursor planes/order/presets/blink and simultaneous raster controls");
}

static void cursor_position_checks() {
    // Oak's Windows cursor driver uses signed coordinates and treats Y as
    // the preceding row (STD8_HC.DRV offsets $56AD-$56C6 and $5537-$557A).
    // The model applies Y+1 even to negative Y. Test clipping at negative X,
    // including positions where
    // the driver applies an unexplained workaround (second-sight-reference
    // docs/06-video.md, "Hardware cursor").
    OTI087 v;
    for (int x : {-65,-64,-15,-14,-2,0,3})
    for (int y : {-65,-64,-2,-1,0,2,7}) {
        mode(v,true);
        for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,1);
        for(unsigned a=0;a<1024;++a) v.mem_linear_w(0x10000+a,0);
        // Pattern source row 2, column 2 selects color1; other pixels color0.
        v.mem_linear_w(0x10000+2*16+8,0x20);
        oak(v,0x48,0x20); oak(v,0x4a,2); oak(v,0x4b,3); oak(v,0x4c,5);
        oak(v,0x40,uint16_t(x)>>8); oak(v,0x41,x);
        oak(v,0x42,uint16_t(y)>>8); oak(v,0x43,y);
        auto pixels=frame(v);
        int top=y+1;
        for(int row=0;row<8;++row) for(int col=0;col<64;++col) {
            bool inside=col>=x && col<x+64 && row>=top && row<top+64;
            EQ(pixels[row*64+col],color(!inside ? 1 : col==x+2 && row==top+2 ? 3 : 2));
        }
    }
    mode(v,true);
    for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,1);
    for(unsigned a=0;a<1024;++a) v.mem_linear_w(0x10000+a,0);
    oak(v,0x48,0x20); oak(v,0x4a,2); oak(v,0x4c,5);
    oak(v,0x40,255); oak(v,0x41,254); oak(v,0x42,255); oak(v,0x43,254);
    beam(v,2,8);
    oak(v,0x40,0); oak(v,0x41,16); oak(v,0x42,0); oak(v,0x43,2);
    beam(v,8);
    auto pixels=frame(v);
    for(unsigned row=0;row<8;++row) for(unsigned col=0;col<64;++col) {
        bool old=row<2 || (row==2 && col<8);
        bool visible=old ? col<62 : row>=3 && col>=16;
        EQ(pixels[row*64+col],color(visible ? 2 : 1));
    }
    std::puts("PASS signed Oak cursor clipping, Y+1 offset and mid-frame X/Y changes");
}

static void cursor_index_checks() {
    // Oak p21: the cursor precedes the P[7:0] bus to the external RAMDAC.
    // Inversion complements the index, then Bt481 L481A_C p14's pixel mask
    // selects a palette entry. Equal source colors with different inverted
    // colors rule out both RGB inversion and reverse lookup of the framebuffer.
    auto palette = [](unsigned i) -> uint32_t {
        return i < 2 ? 0xff123456 : uint32_t(VideoColor(i ^ 0x71, i * 37, i * 13));
    };
    auto source = [](unsigned a) { return a % 4 < 2 ? a % 4 : a & 255; };
    OTI087 v;
    for (unsigned kind = 0; kind < 3; ++kind)
    for (unsigned precision : {6U, 8U})
    for (unsigned mask : {255U, 127U, 0U}) {
        bool const packed = kind == 0;
        mode(v, packed);
        if (kind == 2) seq(v, 4, 14);
        v.dac().command_w(1); v.dac().address_w(2);
        v.dac().mask_w(precision == 8 ? 0x1e : 0x1c); v.dac().command_w(0);
        for (unsigned i = 0; i < 256; ++i) v.dac().set_color(i, palette(i));
        for (unsigned a = 0; a < 2048; ++a) v.mem_linear_w(a, source(a));
        for (unsigned a = 0; a < 1024; ++a) v.mem_linear_w(0x10000 + a, 255);
        oak(v, 0x48, 0x20); oak(v, 0x4c, 5); v.io_w(0x3c6, mask);
        // IBM pp2-64, 2-77, 2-95: byte pan, line-compare reset and fine pan.
        attr(v, 0x13, packed ? 3 : 6); attr(v, 0x10, packed ? 0x21 : 0x61);
        cr(v, 8, packed ? 0 : 0x20); cr(v, 7, 0); cr(v, 9, 0); cr(v, 0x18, 2);
        beam(v, 0);
        auto pixels = frame(v);
        unsigned const width = packed ? 64 : 32;
        for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < width; ++x) {
            unsigned const row = y < 3 ? y : y - 3;
            unsigned const address = row * width + (packed ? 0 : 4) + (y < 3 ? 3 : 0) + x;
            unsigned const index = (source(address) ^ (y ? 255 : 0)) & mask;
            EQ(pixels[y * width + x], palette(index) & (precision == 8 ? 0xffffffff : 0xfffcfcfc));
        }
    }
    // Changes on the same raster line must affect both the underlying index
    // and the cursor lookup at that pixel, without changing the scanned part.
    mode(v, true);
    for (unsigned a = 0; a < 2048; ++a) v.mem_linear_w(a, 1);
    for (unsigned a = 0; a < 1024; ++a) v.mem_linear_w(0x10000 + a, 255);
    oak(v, 0x48, 0x20); oak(v, 0x4c, 5);
    beam(v, 2, 16); v.io_w(0x3c6, 127);
    v.io_w(0x3c8, 126); v.io_w(0x3c9, 0x31); v.io_w(0x3c9, 0x57); v.io_w(0x3c9, 0x93);
    cr(v, 0x13, 1); beam(v, 8);
    auto pixels = frame(v);
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 64; ++x)
        EQ(pixels[y * 64 + x], !y ? color(1) : y < 2 || (y == 2 && x < 16) ? color(254) : 0xff315793);
    // p46 permits cursor pixels in overscan only when bit1 is set. Its
    // inversion source is the full overscan index (IBM p2-94), not black.
    mode(v, true); attr(v, 0x11, 0x35); oak(v, 0x48, 0x20);
    oak(v, 0x41, 60); oak(v, 0x4c, 7); cr(v, 2, 12);
    std::vector<uint32_t> border(96 * 8);
    v.render({border.data(), 96, 8, 96}, {0, 95, 0, 7});
    for (unsigned x = 64; x < 96; ++x) EQ(border[96 + x], color(0xca));
    std::puts("PASS cursor index inversion before Bt481 precision/mask/palette, panning, split, history and overscan");
}

static void cursor_size_checks() {
    // Oak pp44-46 measures this 64x64 cursor in pixels. IBM p2-92 PW takes
    // two dot clocks per pixel; it changes the output width, not cursor size.
    // The sequencer dot-clock divide (p2-50) changes time, not pixel count.
    OTI087 v;
    for (unsigned kind = 0; kind < 4; ++kind)
    for (unsigned clock : {1U, 9U})
    for (unsigned preset : {0U, 31U, 63U}) {
        mode(v, kind < 2); seq(v, 1, clock);
        if (kind == 1) attr(v, 0x10, 0x41);
        if (kind == 3) seq(v, 4, 14);
        cr(v, 0, 47); cr(v, 1, 23); cr(v, 2, 24); cr(v, 3, 30); cr(v, 4, 32); cr(v, 5, 36);
        cr(v, 6, 98); cr(v, 0x10, 90); cr(v, 0x11, 0x1f); cr(v, 0x12, 79);
        cr(v, 0x13, 12); cr(v, 0x15, 80); cr(v, 0x16, 95);
        for (unsigned a = 0; a < 32768; ++a) v.mem_linear_w(a, 1);
        for (unsigned row = 0; row < 64; ++row) for (unsigned a = 0; a < 16; ++a)
            v.mem_linear_w(0x10000 + row * 16 + a, a < 8 ? 0 : row & 1 ? 0xaa : 0x55);
        oak(v, 0x48, 0x20); oak(v, 0x4a, 2); oak(v, 0x4b, 3); oak(v, 0x4c, 5);
        oak(v, 0x41, 7); oak(v, 0x43, 2); oak(v, 0x44, preset); oak(v, 0x45, preset);
        beam(v, 0); auto pixels = frame(v);
        unsigned const width = kind == 0 ? 192 : 96;
        EQ(v.display_timing().width, width); EQ(v.display_timing().height, 80);
        for (unsigned y = 0; y < 80; ++y) for (unsigned x = 0; x < width; ++x) {
            bool const inside = x >= 7 && x < 7 + 64 - preset && y >= 3 && y < 3 + 64 - preset;
            EQ(pixels[y * width + x], color(inside ? 2 + ((x - 7 + y - 3) & 1) : 1));
        }
    }
    std::puts("PASS 64x64 cursor size and presets with one/two dot clocks per pixel and divided dot clock");
}

static void packed_address_checks() {
    // Second Sight reference, docs/06-video.md, "Packed-pixel mode and the
    // CRTC": pitch = offset * 16. docs/15-oak-drawing.md, "Display features
    // that cost no drawing" cites XFree86 OAKAdjust for start = byte address / 8.
    // IBM pp2-65--77 define the preset, repeated-row and line-compare counters.
    OTI087 v; mode(v,true);
    for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
    auto pixels=frame(v); EQ(pixels[64],color(64));
    pixels=frame(v); EQ(pixels[64],color(64));
    cr(v,0x13,1); pixels=frame(v); EQ(pixels[64],color(16));
    cr(v,0x0d,1); beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(8));
    cr(v,0x18,2); cr(v,7,0); cr(v,9,0); pixels=frame(v);
    EQ(pixels[2*64],color(40)); EQ(pixels[3*64],color(0));
    cr(v,0x18,255); cr(v,7,0x10); cr(v,9,0x41); pixels=frame(v);
    EQ(pixels[64],color(8)); EQ(pixels[2*64],color(24));
    cr(v,8,1); pixels=frame(v); EQ(pixels[64],color(24));
    cr(v,8,0); cr(v,9,0xc0); pixels=frame(v);
    EQ(pixels[64],color(8)); EQ(pixels[2*64],color(24));
    cr(v,9,0x40); oak(v,0x14,0x80); beam(v,0); pixels=frame(v);
    EQ(pixels[0],color(8)); EQ(pixels[64],color(24)); EQ(pixels[2*64],color(40));
    cr(v,0x18,2); cr(v,7,0); cr(v,9,0); pixels=frame(v);
    EQ(pixels[6*64],color(0)); EQ(pixels[7*64],color(0));
    // Force the interval renderer as well: both paths must clear the odd
    // field's starting offset when line compare resets the address counter.
    oak(v,0x4c,1); oak(v,0x44,64); pixels=frame(v);
    EQ(pixels[6*64],color(0)); EQ(pixels[7*64],color(0));
    std::puts("PASS packed row reuse across pitch, start, split, preset, double scan and interlace");
}

static void dma_checks() {
    std::array<uint8_t,SSCard::ROM_SIZE> rom{}; rom[0]=0x76;
    SSCard card(rom.data(),18'000'000); auto &v=card.video(); enable(v);
    card.memory_read(0x50000); // Board half-select supplies Oak A19 for A0000.
    oak(v,0x23,1); oak(v,0x24,2); gc(v,5,1); seq(v,2,11);
    for(unsigned a=0;a<16;++a) {
        v.mem_linear_w(0x40000+4*0x100+a,0x40+a);
        v.mem_linear_w(0x80000+4*0x200+a,0xee);
    }
    auto &z=card.processor(); z.write_register(0x32,0); z.write_register(0x36,0);
    const uint8_t regs[]={0,1,10,0,2,10,4,0};
    for(unsigned i=0;i<8;++i) z.write_register(0x20+i,regs[i]);
    z.write_register(0x31,2); z.write_register(0x30,0x41); card.catch_up(1800);
    EQ(z.read_register(0x26),0); EQ(z.read_register(0x27),0);
    for(unsigned a=0;a<16;++a) EQ(v.mem_linear_r(0x80000+4*0x200+a),(a%4==2)?0xee:0x40+a);
    std::puts("PASS Z180 DMA0 independently banked unchained VGA latch copy and map mask");
}
static void revision_e_checks() {
    // Second Sight reference, docs/06-video.md, "Which Oak chip", and
    // docs/reference/oti087-registers.md, "Identification, configuration and
    // bus fields": revision E IDs, writable size, AT straps, no connector.
    OTI087 v; enable(v);
    EQ(read(v,0x3de,0),1); EQ(read(v,0x3de,1),0x20); EQ(read(v,0x3de,2),0);
    for(unsigned value=0;value<256;++value) {
        v.io_w(0x3de,value); EQ(v.io_r(0x3de),value);
        oak(v,0,value); oak(v,1,value);
        EQ(read(v,0x3de,0),1); EQ(read(v,0x3de,1),0x20);
        oak(v,2,value); EQ(read(v,0x3de,2),value&6);
        oak(v,4,value); EQ(read(v,0x3de,4),value&31);
        oak(v,8,value); EQ(read(v,0x3de,8),1);
    }
    v.reset(); enable(v); EQ(read(v,0x3de,2),0);
    // Oak's true-color driver 8ETRU640.DRV stores R,G,B at rising addresses
    // and sets Bt481 $F0 (second-sight-reference docs/06-video.md,
    // "24-bit (true-color) pixels").
    // Exercise vector groups and the scalar tail, with primary colors and
    // unequal components. Expected RGB comes from those bytes, not the DAC.
    mode(v,true); oak(v,0x21,6); v.dac().command_w(0xf0);
    const unsigned rgb[][3]={{255,0,0},{0,255,0},{0,0,255},{0x12,0x67,0xab}};
    for(unsigned x=0;x<64;++x) for(unsigned c=0;c<3;++c) v.mem_linear_w(x*3+c,rgb[x%4][c]);
    auto pixels=frame(v);
    for(unsigned x=0;x<64;++x) EQ(pixels[x],0xff000000|(rgb[x%4][0]<<16)|(rgb[x%4][1]<<8)|rgb[x%4][2]);
    std::vector<uint32_t> clipped(pixels.size());
    v.render({clipped.data(),64,int(pixels.size()/64),64},{1,62,0,0});
    for(unsigned x=1;x<=62;++x) EQ(clipped[x],pixels[x]);
    std::puts("PASS OTI-087X revision E IDs, index, size, local-bus/strap fields and RGB byte order");
}

static void wide_function_checks() {
    // Second Sight reference, docs/15-oak-drawing.md, "The standard VGA graphics
    // controller": IBM function/mask/set-reset on each wide-latch byte
    // and four-pixel mode-0 fills. The oracle above evaluates individual bits.
    OTI087 v; enable(v); oak(v,0x21,4); seq(v,4,14); seq(v,2,0);
    const unsigned saved[]={0x12,0x34,0x56,0x78,0x87,0x65,0x43,0x21};
    for(unsigned wm=0;wm<4;++wm) for(unsigned fn=0;fn<4;++fn)
    for(unsigned rotate=0;rotate<8;++rotate) for(unsigned mask : {0U,0x39U,0xc6U,255U})
    for(unsigned sr : {0U,5U,10U,15U}) for(unsigned esr : {0U,9U,15U})
    for(unsigned swap : {0U,32U}) for(unsigned order : {0U,16U}) {
        oak(v,0x22,16|swap); oak(v,0x30,8|order); oak(v,0x34,0xa5);
        gc(v,0,sr); gc(v,1,esr); gc(v,3,fn*8+rotate); gc(v,5,wm); gc(v,8,mask);
        oak(v,0x35,0);
        v.io_w(0x3de,0x36); for(unsigned value:saved) v.io_w(0x3df,value);
        for(unsigned address=0;address<8;++address) {
            for(unsigned p=0;p<8;++p) v.mem_linear_w(0x100+p,0xee);
            v.memory_w(0xa0100+address,0x96);
            for(unsigned p=0;p<8;++p) {
                bool selected=(p/4==(address&1)) && ((0xa5>>(order?7-p:p))&1);
                EQ(v.mem_linear_r(0x100+p),selected ? written(wm,fn,rotate,0x96,p&3,sr,esr,mask,saved[p^(swap?4:0)]) : 0xee);
            }
        }
    }
    // The probable mode-0 half fill does not require the wide-latch enable.
    gc(v,0,0); gc(v,1,0); gc(v,3,0); gc(v,5,0); gc(v,8,255);
    for(unsigned wide : {0U,16U}) for(unsigned address=0;address<8;++address) {
        oak(v,0x22,wide); oak(v,0x30,8); oak(v,0x34,255);
        for(unsigned p=0;p<8;++p) v.mem_linear_w(0x100+p,0xee);
        v.memory_w(0xa0100+address,0x5a);
        for(unsigned p=0;p<8;++p) EQ(v.mem_linear_r(0x100+p),p/4==(address&1) ? 0x5a : 0xee);
    }
    // Without pixel masking, the same function writes one addressed pixel.
    oak(v,0x30,0); seq(v,2,15); gc(v,3,0x18); gc(v,8,0x3c);
    oak(v,0x22,0x30); oak(v,0x35,0); v.io_w(0x3de,0x36);
    for(unsigned value:saved) v.io_w(0x3df,value);
    for(unsigned p=0;p<8;++p) {
        v.memory_w(0xa0100+p,0x96);
        EQ(v.mem_linear_r(0x100+p),written(0,3,0,0x96,p&3,0,0,0x3c,saved[p^4]));
    }
    std::puts("PASS probable per-byte wide-latch functions, masks, set/reset, swap and mode-0 half fills");
}

static void memory_end_and_pan_checks() {
    // Second Sight reference, docs/15-oak-drawing.md, "Hardware scrolling":
    // RASHn is open; fixed FF is this model's undriven-memory value.
    OTI087 v; mode(v,true); seq(v,4,14);
    v.mem_linear_w(0,0x12); v.mem_linear_w(0xfffff,0x34);
    for(unsigned address : {0x100000U,0x1fffffU,0x200000U}) {
        v.mem_linear_w(address,0x91); EQ(v.mem_linear_r(address),255);
    }
    EQ(v.mem_linear_r(0),0x12); EQ(v.mem_linear_r(0xfffff),0x34);
    oak(v,0x22,16); oak(v,0x23,16); EQ(v.memory_r(0xa0000),255);
    oak(v,0x24,0); oak(v,0x30,8); oak(v,0x34,255); gc(v,5,1);
    v.memory_w(0xa0100,0); for(unsigned p=0;p<4;++p) EQ(v.mem_linear_r(0x100+p),255);
    gc(v,5,0); oak(v,0x30,0); oak(v,0x24,16); v.memory_w(0xa0000,0x87); EQ(v.mem_linear_r(0),0x12);
    oak(v,5,0x2d); // 2MB linear aperture at 2MB; its upper half is unpopulated.
    v.memory_w(0x200000,0x56); v.memory_w(0x300000,0x78);
    EQ(v.memory_r(0x200000),0x56); EQ(v.memory_r(0x300000),255);
    // Each unchained segment selects 256KB; segment four selects RASHn.
    oak(v,5,0); oak(v,0x21,0); seq(v,4,6); oak(v,0x25,4);
    EQ(v.memory_r(0xa0000),255); v.memory_w(0xa0000,0x91); EQ(v.mem_linear_r(0),0x56);
    // Second Sight reference, docs/15-oak-drawing.md, "Horizontal pixel
    // panning": packed pixels use IBM's eight-dot rule, zero for no pan.
    // Cached, general and repeated frames must retain the same pixel origin.
    for(unsigned x=0;x<16;++x) {
        mode(v,true); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,a&255);
        cr(v,0xd,x/8); attr(v,0x13,x%8); beam(v,12); beam(v,0);
        auto pixels=frame(v); auto repeated=frame(v); EQ(pixels==repeated,true);
        for(unsigned p=0;p<64;++p) EQ(pixels[p],color(x+p));
        std::vector<uint32_t> general(pixels.size());
        v.render({general.data(),64,8,64},{0,63,0,7}); EQ(pixels==general,true);
        attr(v,0x10,0x21); cr(v,7,0); cr(v,9,0); cr(v,0x18,2); beam(v,0); pixels=frame(v);
        EQ(pixels[3*64],color(0));
    }
    mode(v,true); for(unsigned a=0;a<64;++a) v.mem_linear_w(a,0x33);
    for(unsigned a=0;a<8;++a) v.mem_linear_w(0xffff8+a,0x20+a);
    cr(v,0xc,255); cr(v,0xd,255); oak(v,0x17,1); beam(v,12); beam(v,0);
    auto pixels=frame(v);
    for(unsigned x=0;x<64;++x) EQ(pixels[x],color(x<8?0x20+x:255));
    for(unsigned x=0;x<64;++x) EQ(pixels[64+x],color(255));
    oak(v,0x17,2); cr(v,0xc,0); cr(v,0xd,0); beam(v,12); beam(v,0);
    pixels=frame(v); for(uint32_t pixel:pixels) EQ(pixel,color(255));
    // All pan alignments crossing a four-plane storage block, including
    // short clipped spans, must keep the same logical pixel order.
    for(unsigned pan=0;pan<8;++pan) {
        mode(v,true);
        for(unsigned a=0x3fff8;a<0x40050;++a) v.mem_linear_w(a,a&255);
        cr(v,0xc,0x7f); cr(v,0xd,255); attr(v,0x13,pan); beam(v,12); beam(v,0);
        pixels=frame(v);
        for(unsigned x=0;x<64;++x) EQ(pixels[x],color((0x3fff8+pan+x)&255));
        for(unsigned start=0;start<4;++start) for(unsigned width=1;width<=20;++width) {
            std::vector<uint32_t> clipped(pixels.size());
            v.render({clipped.data(),64,8,64},{int(start),int(start+width-1),0,0});
            for(unsigned x=start;x<start+width;++x) EQ(clipped[x],pixels[x]);
        }
    }
    std::puts("PASS unpopulated DRAM without mirroring, latch reads, linear/planar banks and fine packed scrolling");
}

// Second Sight reference, docs/15-oak-drawing.md, "Hardware scrolling" to
// "Bands that start anywhere"; IBM pp2-67, 2-77, 2-95 and 2-103.
static void display_address_and_pan_checks() {
    // A 16-color byte-mode mode with 1280 pixels and CRTC $13 = $50, as Oak
    // mode 58h: 160 addresses per row, so row 409 starts at 65,440.
    OTI087 v; mode(v,false,false);
    cr(v,0,195); cr(v,1,159); cr(v,2,160); cr(v,3,0x80); cr(v,4,170); cr(v,5,15);
    cr(v,6,0xae); cr(v,7,0x1f); cr(v,9,0x40); cr(v,0x10,0xa4); cr(v,0x12,0x9f);
    cr(v,0x13,0x50); cr(v,0x15,0xa0); cr(v,0x16,0xaa);
    EQ(v.display_timing().width,1280); EQ(v.display_timing().height,416);
    // Each address shows its own value: pixel k of the cell is nibble k,
    // high nibble first. Undriven planes give $FFFFFFFF.
    auto put=[&](uint32_t a) {
        for(unsigned p=0;p<4;++p) {
            uint8_t byte=0;
            for(unsigned k=0;k<8;++k) byte|=((a>>(4*(7-k)+p))&1)<<(7-k);
            v.mem_linear_w(a*4+p,byte);
        }
    };
    auto shown=[](const std::vector<uint32_t> &pixels, unsigned y, unsigned cell) {
        uint32_t a=0;
        for(unsigned k=0;k<8;++k) a=a<<4|(pixels[y*1280+cell*8+k]&15);
        return a;
    };
    for(uint32_t a=0;a<66000;++a) put(a);
    for(uint32_t a=0x3fff0;a<0x40000;++a) put(a);
    beam(v,420); beam(v,0);
    auto pixels=frame(v);
    for(unsigned y=408;y<=410;++y) for(unsigned cell=0;cell<160;++cell) EQ(shown(pixels,y,cell),y*160+cell);
    EQ(shown(pixels,409,95),65535); EQ(shown(pixels,409,96),65536);
    // The 1 MB end: address $40000 is linear $100000. It shows undriven
    // DRAM, never address 0.
    cr(v,0xc,0xff); cr(v,0xd,0xf0); oak(v,0x17,3); beam(v,420); beam(v,0);
    pixels=frame(v);
    for(unsigned cell=0;cell<160;++cell) EQ(shown(pixels,0,cell),cell<16?0x3fff0+cell:0xffffffff);
    for(unsigned cell=0;cell<160;++cell) EQ(shown(pixels,1,cell),0xffffffff);

    // The start address, ext $17 included, latches once at the start of
    // vertical retrace (CRTC $10 = 12 here).
    mode(v,true); for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
    v.mem_linear_w(0x80010,0x5a);
    beam(v,2); cr(v,0xd,1); beam(v,11); pixels=frame(v);
    EQ(pixels[0],color(0)); EQ(pixels[7*64],color(0xc0));
    beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(8));
    beam(v,13); cr(v,0xd,2); beam(v,0); pixels=frame(v); EQ(pixels[0],color(8));
    beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(16));
    beam(v,3); oak(v,0x17,1); beam(v,11); pixels=frame(v); EQ(pixels[0],color(16));
    beam(v,12); beam(v,0); pixels=frame(v); EQ(pixels[0],color(0x5a));

    // Attribute $13 has no retrace latch. A start written during display and
    // a pan written just after vertical sync starts show on the same frame.
    mode(v,true); for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
    beam(v,2); cr(v,0xd,1); beam(v,12); attr(v,0x13,3);
    pixels=frame(v); EQ(pixels[0],color(0));
    beam(v,0); pixels=frame(v);
    for(unsigned y=0;y<8;++y) EQ(pixels[y*64],color((8+3+y*64)&255));
    // A pan write mid-line shows from the next line, in packed and in
    // unchained 256-color modes (pan 4 = 2 pixels there).
    mode(v,true); for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
    beam(v,3,8); attr(v,0x13,2); beam(v,8); pixels=frame(v);
    for(unsigned y=0;y<8;++y) for(unsigned x:{0U,8U,63U})
        EQ(pixels[y*64+x],color((y*64+x+(y>3?2:0))&255));
    mode(v); for(unsigned a=0;a<2048;++a) v.mem_linear_w(a,a&255);
    beam(v,3,8); attr(v,0x13,4); beam(v,8); pixels=frame(v);
    for(unsigned y=0;y<8;++y) for(unsigned x:{0U,8U,31U})
        EQ(pixels[y*32+x],color((y*32+x+(y>3?2:0))&255));

    // Rows below line compare restart at address 0 and add the offset per
    // row. Attribute $10 bit 5 forces pan 0 there until vertical sync.
    for(unsigned compat:{0U,1U}) {
        mode(v,true); for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
        cr(v,0xd,1); attr(v,0x13,3); attr(v,0x10,compat?0x21:1);
        cr(v,0x18,2); cr(v,7,0); cr(v,9,0); beam(v,12); beam(v,0);
        for(unsigned frames=0;frames<2;++frames) {
            pixels=frame(v);
            for(unsigned y=0;y<8;++y) for(unsigned x:{0U,63U})
                EQ(pixels[y*64+x],color(((y<3?8+y*64+3:(y-3)*64+(compat?0:3))+x)&255));
            beam(v,12); beam(v,0);
        }
    }

    // CRTC $13 written mid-frame moves the start of every following row.
    mode(v,true); for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,a&255);
    beam(v,2,3); cr(v,0x13,1); beam(v,8); pixels=frame(v);
    const unsigned rows[]={0,64,128,144,160,176,192,208};
    for(unsigned y=0;y<8;++y) EQ(pixels[y*64],color(rows[y]));
    std::puts("PASS display address past 64K, 1 MB end, start latch, unlatched pan, split restart and mid-frame offset");
}

static void cursor_087x_checks() {
    // Second Sight reference, docs/06-video.md, "Hardware cursor": WHATVGA's
    // 256-color restriction includes VGA chained/unchained and Oak packed.
    OTI087 v;
    for(unsigned kind=0;kind<10;++kind) {
        mode(v,kind>=4 && kind<=8,kind>=2 && kind<=8);
        if(kind==0) { gc(v,6,4); attr(v,0x10,0); }
        if(kind==3) seq(v,4,14);
        if(kind>=5 && kind<=8) { oak(v,0x21,kind>=7?6:5); v.dac().command_w(kind==5?0xa0:kind==6?0xe0:kind==7?0xf0:0xd0); }
        if(kind==9) gc(v,5,0x20);
        for(unsigned a=0;a<4096;++a) v.mem_linear_w(a,1);
        for(unsigned a=0;a<1024;++a) v.mem_linear_w(0x10000+a,0);
        oak(v,0x48,0x20); oak(v,0x4a,0x82); v.io_w(0x3c6,0x7f);
        auto without=frame(v); oak(v,0x4c,1); auto with=frame(v);
        if(kind>=2 && kind<=4) EQ(with[v.display_timing().width],color(2));
        else EQ(with==without,true);
    }
    // Y+1 holds for every preset; probable field-line comparison doubles
    // the pattern height, with each source row in both neighboring frame lines.
    for(unsigned interlace : {0U,128U}) for(unsigned preset=0;preset<64;++preset)
    for(int y : {-2,-1,0,1}) {
        mode(v,true); cr(v,6,148); cr(v,0x12,127); cr(v,0x15,128); cr(v,0x16,145);
        cr(v,0x10,140); cr(v,0x11,0x1f); oak(v,0x14,interlace); beam(v,0);
        for(unsigned a=0;a<16384;++a) v.mem_linear_w(a,1);
        for(unsigned row=0;row<64;++row) for(unsigned a=0;a<16;++a)
            v.mem_linear_w(0x10000+row*16+a,a<8?0:row&1?255:0);
        oak(v,0x48,0x20); oak(v,0x4a,2); oak(v,0x4b,3); oak(v,0x4c,5);
        oak(v,0x45,preset); oak(v,0x42,uint16_t(y)>>8); oak(v,0x43,y);
        auto pixels=frame(v); unsigned scale=interlace?2:1;
        int top=(y+1)*int(scale),height=(64-preset)*scale;
        for(int row=0;row<v.display_timing().height;++row) {
            bool inside=row>=top && row<top+height;
            EQ(pixels[row*64],color(inside ? 2+((unsigned(row-top)/scale+preset)&1) : 1));
        }
    }
    std::puts("PASS cursor mode gating, Bt481 indexes/mask, every preset Y+1 and probable interlace height");
}

static void test_register_checks() {
    // Extended register $03 is readable and writable (Oak p. 33). CRTC $10/$11
    // return their programmed values even when CRTC $03 bit 7 is clear; the
    // card has no known current-line register (second-sight-reference
    // docs/14-custom-code.md, "The current line").
    OTI087 v;
    mode(v, true);
    EQ(read(v, 0x3de, 3), 0);
    for (unsigned data = 0; data < 256; ++data) {
        oak(v, 3, data);
        EQ(read(v, 0x3de, 3), data);
        EQ(v.extended_read(3), data);
    }
    v.reset(); enable(v); EQ(read(v, 0x3de, 3), 0);

    // Bit4 flush is independent of bit7. Each CPU write is already visible
    // to the following read, with the Oak write-cache bit both off and on.
    for (unsigned cache : {0U, 8U}) for (unsigned control : {0U, 0x10U, 0x80U, 0x90U}) {
        mode(v, true); oak(v, 0x22, cache); oak(v, 3, control);
        for (unsigned a = 0; a < 32; ++a) {
            v.memory_w(0xa0000 + a, a ^ 0x93);
            EQ(v.memory_r(0xa0000 + a), a ^ 0x93);
            EQ(v.mem_linear_r(a), a ^ 0x93);
        }
    }
    // Reserved/global tests other than 101 preserve normal output. Bit6
    // alone changes neither pixels nor transaction-level memory behavior.
    for (unsigned kind = 0; kind < 5; ++kind) {
        mode(v, kind == 4, kind == 2 || kind == 4);
        if (kind == 0) { gc(v, 6, 4); attr(v, 0x10, 0); }
        if (kind == 3) gc(v, 5, 0x20);
        for (unsigned a = 0; a < 4096; ++a) v.mem_linear_w(a, a * 17 + 39);
        auto normal = frame(v);
        for (unsigned data : {0x08U, 0x10U, 0x40U, 0x80U, 0x81U, 0x82U, 0x83U, 0x84U, 0x86U, 0x87U}) {
            oak(v, 3, data);
            EQ(frame(v) == normal, true);
        }
    }

    // Count blanking pulses and observe retrace at every position of this
    // 160-dot, 16-line raster. Bit5 toggles only the electrical output;
    // repeated writes do not toggle it again. There is no light-pen alias.
    mode(v, true);
    v.io_w(0x3c2, 0xc3); // Negative sync polarity at both connector pins.
    for (unsigned cr03 : {20U, 148U}) {
        cr(v, 3, cr03);
        unsigned blank_edges = 0;
        for (int y = 0; y < 16; ++y) {
            bool was_blank = true;
            for (int x = 0; x < 160; ++x) {
                beam(v, y, x);
                unsigned status = v.io_r(0x3da);
                EQ(status & 1, x >= 64 || y >= 8);
                EQ(status & 8, y >= 12 && y < 14 ? 8 : 0);
                if (!(status & 1) && was_blank) ++blank_edges;
                was_blank = status & 1;
                EQ(read(v, 0x3d4, 0x10), 12);
                EQ(read(v, 0x3d4, 0x11), 0x1e);
                bool irq = v.cint_r();
                auto number = v.frame_number();
                int sync = v.vsync_r();
                unsigned before = read(v, 0x3de, 3);
                oak(v, 3, before ^ 0x20);
                EQ(v.vsync_r(), sync ^ 1);
                oak(v, 3, before ^ 0x20);
                EQ(v.vsync_r(), sync ^ 1);
                EQ(v.io_r(0x3da), status);
                EQ(v.cint_r(), irq);
                EQ(v.frame_number(), number);
                EQ(v.hpos(), x); EQ(v.vpos(), y);
                oak(v, 3, before);
                EQ(v.vsync_r(), sync);
            }
        }
        EQ(blank_edges, 8);
    }

    // $FF system pixel data still passes through mode/plane enables and the
    // Bt481. Exercise packed 8/15/16/24/32-bit, VGA256, planar and CGA.
    for (unsigned kind = 0; kind < 8; ++kind) {
        mode(v, kind >= 3, kind == 0 || kind >= 3);
        if (kind == 2) gc(v, 5, 0x20);
        if (kind >= 4) {
            v.dac().command_w(kind == 4 ? 0xa0 : kind == 5 ? 0xe0 : kind == 6 ? 0xf0 : 0xd0);
            oak(v, 0x21, 4); // Commit the new DAC pixel width to raster timing.
            beam(v, 0);
        }
        attr(v, 0x12, 5); v.io_w(0x3c6, 0x7f);
        for (unsigned a = 0; a < 4096; ++a) v.mem_linear_w(a, 0);
        oak(v, 3, 0x85);
        auto pixels = frame(v);
        // L481A_C pp7-8: the fourth $FF byte in 32-bit mode selects the
        // masked palette entry; 15/16-bit formats leave unused DAC bits zero.
        uint32_t expected = kind == 0 ? color(0x55) : kind < 3 ? color(5) : kind == 3 || kind == 7 ? color(0x7f)
                          : kind == 4 ? 0xfff8f8f8 : kind == 5 ? 0xfff8fcf8 : kind == 6 ? 0xffffffff
                          : 0;
        for (auto pixel : pixels) EQ(pixel, expected);
        oak(v, 3, 0); pixels = frame(v);
        EQ(pixels[0], color(0));
    }
    mode(v, true);
    for (unsigned flags = 0; flags < 16; ++flags) {
        oak(v, 3, 0x85 | (flags << 3));
        EQ(frame(v)[0], color(255));
    }
    // The compatibility alias must also update packed-row eligibility.
    mode(v, false, false); gc(v, 6, 4); attr(v, 0x10, 0);
    v.mem_linear_w(0, 1); v.mem_linear_w(1, 2); v.mem_linear_w(32 * 4 + 2, 0x80);
    EQ(frame(v)[0], color(2)); oak(v, 0x0d, 8); EQ(frame(v)[0], color(1));
    oak(v, 0x0d, 0); EQ(frame(v)[0], color(2));

    // Text takes attributes from the board's retained bus byte and glyphs
    // from open SD15:8, while character codes and ninth-dot rules still apply.
    std::array<uint8_t, SSCard::ROM_SIZE> rom{};
    rom[0] = 0x76; rom[1] = 0x2b;
    SSCard card(rom.data(), 18'000'000);
    auto &text = card.video(); mode(text, false, false);
    gc(text, 6, 4); attr(text, 0x10, 0);
    text.mem_linear_w(0, 1); text.mem_linear_w(1, 0x14);
    text.mem_linear_w(32 * 4 + 2, 0);
    card.io_write(0x3de, 3); card.io_write(0x3df, 0x85);
    for (unsigned data : {0x17U, 0x2aU, 0xf3U}) {
        card.memory_write(0x60000, data); // ROM discards the store, bus retains it.
        auto pixels = frame(text);
        for (unsigned x = 0; x < 8; ++x) EQ(pixels[x], color(data & 15));
        EQ(text.mem_linear_r(1), 0x14);
        EQ(text.mem_linear_r(32 * 4 + 2), 0);
    }
    EQ(card.memory_read(0x60001), 0x2b); EQ(frame(text)[0], color(11));
    card.io_write(0x1234, 0x39); EQ(frame(text)[0], color(9));
    seq(text, 1, 0); attr(text, 0x13, 8); beam(text, 12); beam(text, 0);
    card.memory_write(0x60000, 0x2a);
    auto pixels = frame(text); EQ(pixels[0], color(10)); EQ(pixels[8], color(2));
    text.mem_linear_w(0, 0xc0); attr(text, 0x10, 4);
    EQ(frame(text)[8], color(10));
    card.catch_up(100);
    EQ(card.interrupt_bus_byte(), 0x2b); // Direct ROM fetches retain the byte after HALT.
    EQ(frame(text)[0], color(11));
    // The U12 RGB timing logic combines the post-test VSYNC pin, not the
    // CRTC retrace status.
    mode(text, true); text.io_w(0x3c2, 0xc3); beam(text, 2, 0);
    EQ(card.composite_sync(), 1); oak(text, 3, 0x20); EQ(card.composite_sync(), 0);
    beam(text, 2, 96); EQ(text.hsync_r(), 0); oak(text, 3, 0); EQ(card.composite_sync(), 0);
    // Register history retains normal pixels before scan-test enable.
    mode(v, true); for (unsigned a = 0; a < 4096; ++a) v.mem_linear_w(a, 3);
    beam(v, 2, 16); oak(v, 3, 0x85);
    auto split = frame(v);
    EQ(split[2 * 64 + 15], color(3)); EQ(split[2 * 64 + 16], color(255));
    // Attribute scan testing does not remove the independently fetched cursor.
    mode(v, true); oak(v, 3, 0x85);
    for (unsigned a = 0; a < 1024; ++a) v.mem_linear_w(0x10000 + a, 255);
    oak(v, 0x48, 0x20); oak(v, 0x4c, 1);
    auto cursor = frame(v); EQ(cursor[0], color(255)); EQ(cursor[64], color(0));
    // Direct clipped rendering uses the same bus sample as frame rendering.
    card.memory_write(0x60000, 0x29);
    pixels.assign(text.display_timing().width * text.display_timing().height, 0);
    oak(text, 0x21, 0); gc(text, 6, 4); attr(text, 0x10, 0); oak(text, 3, 0x85);
    text.render({pixels.data(), text.display_timing().width, text.display_timing().height,
                 text.display_timing().width}, {0, 0, 0, 0});
    EQ(pixels[0], color(9));
    std::puts("PASS Oak test register readback/cache/sync/scan modes and per-line status without light-pen alias");
}

int main() {
    memory_checks(); font_address_checks(); oak_checks(); timing_checks(); deadline_checks(); raster_checks(); text_and_cga_checks();
    dac_checks(); address_checks(); display_control_checks(); sequencer_checks();
    cursor_checks(); cursor_position_checks(); cursor_index_checks(); cursor_size_checks(); packed_address_checks(); dma_checks();
    revision_e_checks(); wide_function_checks(); memory_end_and_pan_checks(); display_address_and_pan_checks();
    cursor_087x_checks();
    test_register_checks();
    std::printf("PASS %u document-derived checks\n",checks);
}

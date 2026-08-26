#include "devices/displaypp/generate/AppleII.hpp"

#include "devices/displaypp/AppleIIgsColors.hpp"
#include "devices/displaypp/generate/AppleIIgs.hpp"
#include "devices/displaypp/render/GSRGB_LUT.hpp"
#include "display/filters.hpp"
#include "display/ntsc.hpp"
#include "mmus/iigs_aux_linear.hpp"

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

static constexpr int kCharNum = 256;
static constexpr int kCharWidth = 16;
static constexpr int kCellWidth = 14;
static constexpr int kDots = 560;
static constexpr int kLines = 192;

static constexpr RGBA_t kBlack = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
static constexpr RGBA_t kGreen = RGBA_t::make(0x00, 0xFF, 0x00, 0xFF);
static constexpr RGBA_t kWhite = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF);
static constexpr RGBA_t kPadRgb = RGBA_t::make(0x00, 0x00, 0x00, 0x00);

alignas(64) static constexpr uint32_t kTextMap[24] = {
    0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380,
    0x0028, 0x00A8, 0x0128, 0x01A8, 0x0228, 0x02A8, 0x0328, 0x03A8,
    0x0050, 0x00D0, 0x0150, 0x01D0, 0x0250, 0x02D0, 0x0350, 0x03D0,
};

alignas(64) static constexpr uint32_t kHiresMap[24] = {
    0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380,
    0x0028, 0x00A8, 0x0128, 0x01A8, 0x0228, 0x02A8, 0x0328, 0x03A8,
    0x0050, 0x00D0, 0x0150, 0x01D0, 0x0250, 0x02D0, 0x0350, 0x03D0,
};

static bool mode_needs_aux(video_decode_mode_t mode) {
    return mode == video_decode_mode_t::TEXT80 || mode == video_decode_mode_t::LORES80 ||
           mode == video_decode_mode_t::DHGR;
}

static uint8_t phase_offset_for(video_decode_mode_t mode) {
    switch (mode) {
        case video_decode_mode_t::TEXT80:
        case video_decode_mode_t::LORES80:
        case video_decode_mode_t::DHGR:
            return 1;
        default:
            return 0;
    }
}

static bool ntsc_colorburst(video_decode_mode_t mode) {
    return mode != video_decode_mode_t::TEXT40 && mode != video_decode_mode_t::TEXT80;
}

static bool gsrgb_hires(video_decode_mode_t mode) {
    return mode == video_decode_mode_t::HIRES || mode == video_decode_mode_t::HIRES_NOSHIFT ||
           mode == video_decode_mode_t::DHGR;
}

struct AppleII_Context {
    CharRom *char_rom = nullptr;
    bool flash_state = false;
    uint16_t char_set = 0;
    bool normal_alt = false;
    uint8_t text_fg = 0x0F;
    uint8_t text_bg = 0x00;
    uint8_t hires40Font[2 * kCharNum * kCharWidth];

    explicit AppleII_Context(CharRom *rom) : char_rom(rom) { buildHires40Font(true); }

    void buildHires40Font(bool delayEnabled) {
        for (int i = 0; i < 2 * kCharNum; i++) {
            uint8_t value = (i & 0x7f) << 1 | (i >> 8);
            bool delay = delayEnabled && (i & 0x80);
            for (int x = 0; x < kCharWidth; x++) {
                bool bit = (value >> ((x + 2 - delay) >> 1)) & 0x1;
                hires40Font[i * kCharWidth + x] = bit ? 0xff : 0x00;
            }
        }
    }

    void set_char_set(uint16_t set) {
        char_set = set;
        if (char_rom) {
            char_rom->set_char_set(char_set, normal_alt);
        }
    }

    void set_normal_alt(bool alt) {
        normal_alt = alt;
        if (char_rom) {
            char_rom->set_char_set(char_set, normal_alt);
        }
    }

    uint8_t char_scanline(uint8_t tchar, uint16_t y) const {
        uint8_t cdata = char_rom->get_char_scanline(tchar, y);
        if (char_rom->is_flash(tchar) && flash_state) {
            cdata ^= 0xFF;
        }
        return cdata;
    }

    void expand_text40(const uint8_t *page, uint16_t linegroup, uint16_t y, uint8_t *dots) const {
        const uint8_t tc = static_cast<uint8_t>((text_fg << 4) | 1);
        const uint8_t td = static_cast<uint8_t>(text_bg << 4);
        uint32_t char_addr = kTextMap[linegroup];
        uint8_t *p = dots;
        for (uint32_t x = 0; x < 40; x++) {
            uint8_t cdata = char_scanline(page[char_addr], y);
            for (uint32_t n = 0; n < 7; n++) {
                const uint8_t packed = (cdata & 1) ? tc : td;
                *p++ = packed;
                *p++ = packed;
                cdata >>= 1;
            }
            char_addr++;
        }
    }

    void expand_text80(const uint8_t *main, const uint8_t *aux, uint16_t linegroup, uint16_t y,
                       uint8_t *dots) const {
        const uint8_t tc = static_cast<uint8_t>((text_fg << 4) | 1);
        const uint8_t td = static_cast<uint8_t>(text_bg << 4);
        uint32_t char_addr = kTextMap[linegroup];
        uint8_t *p = dots;
        for (uint32_t x = 0; x < 40; x++) {
            uint8_t cdata = char_scanline(aux[char_addr], y);
            for (int n = 0; n < 7; n++) {
                *p++ = (cdata & 1) ? tc : td;
                cdata >>= 1;
            }
            cdata = char_scanline(main[char_addr], y);
            for (int n = 0; n < 7; n++) {
                *p++ = (cdata & 1) ? tc : td;
                cdata >>= 1;
            }
            char_addr++;
        }
    }

    void expand_hires40(const uint8_t *hgrpage, uint16_t linegroup, uint16_t y, bool noshift,
                        uint8_t *dots) const {
        const uint8_t *d = hgrpage + kHiresMap[linegroup] + y * 0x400;
        uint8_t lastByte = 0x00;
        uint8_t *p = dots;
        for (int x = 0; x < 40; x++) {
            uint8_t byte = noshift ? (d[x] & 0x7F) : d[x];
            size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * kCharWidth;
            for (int i = 0; i < kCellWidth; i++) {
                *p++ = hires40Font[fontIndex + i];
            }
            lastByte = byte;
        }
    }

    void expand_hires80(const uint8_t *hgrpage, const uint8_t *althgrpage, uint16_t linegroup,
                        uint16_t y, uint8_t *dots) const {
        const uint8_t *m = hgrpage + kHiresMap[linegroup] + y * 0x400;
        const uint8_t *a = althgrpage + kHiresMap[linegroup] + y * 0x400;
        uint8_t *p = dots;
        for (int x = 0; x < 40; x++) {
            uint8_t byteA = a[x];
            uint8_t byteM = m[x];
            for (int i = 0; i < 7; i++) {
                *p++ = (byteA & 0x01) ? 1 : 0;
                byteA >>= 1;
            }
            for (int i = 0; i < 7; i++) {
                *p++ = (byteM & 0x01) ? 1 : 0;
                byteM >>= 1;
            }
        }
    }

    void expand_lores40(const uint8_t *textpage, uint16_t linegroup, uint16_t y, uint8_t *dots) const {
        uint16_t char_addr = kTextMap[linegroup];
        uint8_t *p = dots;
        for (uint16_t x = 0; x < 40; x++) {
            uint8_t tchar = textpage[char_addr];
            if (y & 4) {
                tchar = tchar >> 4;
            }
            uint8_t color = tchar & 0x0F;
            uint16_t pixeloff = (x * 14) % 4;
            for (int bits = 0; bits < 14; bits++) {
                uint8_t bit = (color >> pixeloff) & 0x01;
                *p++ = static_cast<uint8_t>((color << 4) | bit);
                pixeloff = (pixeloff + 1) % 4;
            }
            char_addr++;
        }
    }

    void expand_lores80(const uint8_t *textpage, const uint8_t *alttextpage, uint16_t linegroup,
                        uint16_t y, uint8_t *dots) const {
        uint16_t char_addr = kTextMap[linegroup];
        uint8_t *p = dots;
        for (uint16_t x = 0; x < 40; x++) {
            uint8_t tchar = alttextpage[char_addr];
            if (y & 4) {
                tchar = tchar >> 4;
            }
            tchar &= 0x0F;
            // Aux stores the color rotated right one bit; rotate left to recover the
            // motherboard color for RGB. Composite decodes the raw dots as-is. (Sather IIe 8-29)
            uint8_t color = static_cast<uint8_t>(((tchar << 1) | ((tchar & 0x08) >> 3)) & 0x0F);
            uint16_t pixeloff = (x * 14) % 4;
            for (uint16_t bits = 0; bits < 7; bits++) {
                uint8_t bit = (tchar >> pixeloff) & 0x01;
                *p++ = static_cast<uint8_t>((color << 4) | bit);
                pixeloff = (pixeloff + 1) % 4;
            }

            tchar = textpage[char_addr];
            if (y & 4) {
                tchar = tchar >> 4;
            }
            tchar &= 0x0F;
            pixeloff = (x * 14) % 4;
            for (uint16_t bits = 0; bits < 7; bits++) {
                uint8_t bit = (tchar >> pixeloff) & 0x01;
                *p++ = static_cast<uint8_t>((tchar << 4) | bit);
                pixeloff = (pixeloff + 1) % 4;
            }
            char_addr++;
        }
    }

    void expand_line(video_decode_mode_t mode, const uint8_t *main, const uint8_t *aux,
                     uint16_t linegroup, uint16_t y, uint8_t *dots) const {
        switch (mode) {
            case video_decode_mode_t::TEXT40:
                expand_text40(main, linegroup, y, dots);
                break;
            case video_decode_mode_t::TEXT80:
                expand_text80(main, aux, linegroup, y, dots);
                break;
            case video_decode_mode_t::LORES40:
                expand_lores40(main, linegroup, y, dots);
                break;
            case video_decode_mode_t::LORES80:
                expand_lores80(main, aux, linegroup, y, dots);
                break;
            case video_decode_mode_t::HIRES:
                expand_hires40(main, linegroup, y, false, dots);
                break;
            case video_decode_mode_t::HIRES_NOSHIFT:
                expand_hires40(main, linegroup, y, true, dots);
                break;
            case video_decode_mode_t::DHGR:
                expand_hires80(main, aux, linegroup, y, dots);
                break;
            case video_decode_mode_t::SHR:
                break;
        }
    }
};

static void pad_line(Frame560RGBA *out, RGBA_t color, int n) { out->push_n(color, n); }

static void fill_black(Frame560RGBA *out) {
    for (uint16_t y = 0; y < kLines; y++) {
        out->set_line(y);
        out->push_n(kBlack, 567);
    }
}

class AppleII_Composite {
    AppleII_Context &ctx_;

    static void ensure_ntsc() {
        static bool ready = false;
        if (!ready) {
            setupConfig();
            generate_filters(NUM_TAPS);
            init_hgr_LUT();
            ready = true;
        }
    }

    void emit_mono(Frame560RGBA *out, const uint8_t *dots, uint8_t phase_offset, RGBA_t on) {
        if (phase_offset == 0) {
            pad_line(out, kBlack, 7);
        }
        for (int i = 0; i < kDots; i++) {
            out->push((dots[i] & 1) ? on : kBlack);
        }
        if (phase_offset == 1) {
            pad_line(out, kBlack, 7);
        }
    }

    void emit_ntsc_color(Frame560RGBA *out, const uint8_t *dots, uint8_t phase_offset) {
        if (phase_offset == 0) {
            pad_line(out, kBlack, 7);
        }
        uint32_t bits = 0;
        for (uint16_t i = 0; i < NUM_TAPS; i++) {
            bits >>= 1;
            if (dots[i] & 1) {
                bits |= (1u << (NUM_TAPS * 2));
            }
        }
        for (uint16_t x = 0; x < kDots; x++) {
            bits >>= 1;
            if ((x < kDots - NUM_TAPS) && (dots[x + NUM_TAPS] & 1)) {
                bits |= (1u << (NUM_TAPS * 2));
            }
            uint32_t phase = (phase_offset + x) % 4;
            out->push(g_hgr_LUT[phase][bits]);
        }
        if (phase_offset == 1) {
            pad_line(out, kBlack, 7);
        }
    }

public:
    explicit AppleII_Composite(AppleII_Context &ctx) : ctx_(ctx) { ensure_ntsc(); }

    void generate(video_decode_mode_t mode, bool ntsc, const uint8_t *main, const uint8_t *aux,
                  Frame560RGBA *out) {
        if (!main || !out) {
            return;
        }
        if (mode_needs_aux(mode) && !aux) {
            fill_black(out);
            return;
        }

        const uint8_t phase = phase_offset_for(mode);
        const bool color = ntsc && ntsc_colorburst(mode);
        const RGBA_t on = ntsc ? kWhite : kGreen;

        uint8_t dots[kDots];
        for (uint16_t lg = 0; lg < 24; lg++) {
            for (uint16_t y = 0; y < 8; y++) {
                ctx_.expand_line(mode, main, aux, lg, y, dots);
                out->set_line(lg * 8 + y);
                if (color) {
                    emit_ntsc_color(out, dots, phase);
                } else {
                    emit_mono(out, dots, phase, on);
                }
            }
        }
    }
};

class AppleII_GSRGB {
    AppleII_Context &ctx_;
    const uint16_t *lut_ = HiresColorTable;
    RGBA_t hgr_lut_[16];
    RGBA_t txt_lut_[16];

    void emit_hires_pixels(uint32_t shiftreg, Frame560RGBA *out) {
        uint16_t pixels = lut_[shiftreg & 0x7FF];
        out->push(hgr_lut_[(pixels >> 12) & 0xF]);
        out->push(hgr_lut_[(pixels >> 8) & 0xF]);
        out->push(hgr_lut_[(pixels >> 4) & 0xF]);
        out->push(hgr_lut_[pixels & 0xF]);
    }

    void emit_hires_line(Frame560RGBA *out, const uint8_t *dots, uint8_t phase_offset) {
        if (phase_offset == 0) {
            pad_line(out, kPadRgb, 7);
        }
        uint32_t shiftreg = 0;
        uint16_t remaining = kDots;
        uint16_t idx = 0;
        for (int i = 0; i < 3 - phase_offset; i++) {
            shiftreg = (shiftreg << 1) | (dots[idx++] & 1);
            remaining--;
        }
        while (remaining >= 4) {
            for (int i = 0; i < 4; i++) {
                shiftreg = (shiftreg << 1) | (dots[idx++] & 1);
            }
            remaining -= 4;
            emit_hires_pixels(shiftreg, out);
        }
        int cnt = remaining;
        for (int i = 0; i < cnt; i++) {
            shiftreg = (shiftreg << 1) | (dots[idx++] & 1);
        }
        for (int i = 0; i < 4 - cnt; i++) {
            shiftreg = (shiftreg << 1) | 0;
        }
        emit_hires_pixels(shiftreg, out);
        if (phase_offset == 1) {
            pad_line(out, kPadRgb, 7);
        }
    }

    void emit_text_line(Frame560RGBA *out, const uint8_t *dots, uint8_t phase_offset) {
        if (phase_offset == 0) {
            pad_line(out, kPadRgb, 7);
        }
        for (int i = 0; i < kDots; i++) {
            out->push(txt_lut_[dots[i] >> 4]);
        }
        if (phase_offset == 1) {
            pad_line(out, kPadRgb, 7);
        }
    }

public:
    explicit AppleII_GSRGB(AppleII_Context &ctx) : ctx_(ctx) {
        for (int i = 0; i < 16; i++) {
            hgr_lut_[i] = RGBA_t::make(GSHGRColors[i].r >> 8, GSHGRColors[i].g >> 8,
                                       GSHGRColors[i].b >> 8, 0xFF);
            txt_lut_[i] = AppleIIgs::TEXT_COLORS[i];
        }
    }

    void generate(video_decode_mode_t mode, const uint8_t *main, const uint8_t *aux,
                  Frame560RGBA *out) {
        if (!main || !out) {
            return;
        }
        if (mode_needs_aux(mode) && !aux) {
            fill_black(out);
            return;
        }

        const uint8_t phase = phase_offset_for(mode);
        const bool hires = gsrgb_hires(mode);
        uint8_t dots[kDots];
        for (uint16_t lg = 0; lg < 24; lg++) {
            for (uint16_t y = 0; y < 8; y++) {
                ctx_.expand_line(mode, main, aux, lg, y, dots);
                out->set_line(lg * 8 + y);
                if (hires) {
                    emit_hires_line(out, dots, phase);
                } else {
                    emit_text_line(out, dots, phase);
                }
            }
        }
    }
};

static uint8_t shr_at(const uint8_t *base2000, uint16_t linear_off, bool interleave) {
    uint16_t cpu = static_cast<uint16_t>(0x2000 + linear_off);
    uint16_t phys = interleave ? iigs_aux_linear_to_phys(cpu) : cpu;
    return base2000[phys - 0x2000];
}

static SHRColor shr_palette_color(const uint8_t *base2000, uint8_t pal, uint8_t idx, bool interleave) {
    // Hardware: 16 palettes × 16 colors × 2 bytes at $9E00. Do not overlay
    // struct Palette (it carries a runtime RGBA cache that is not in RAM).
    uint16_t off = static_cast<uint16_t>(0x7E00 + pal * 32 + idx * 2);
    uint8_t lo = shr_at(base2000, off, interleave);
    uint8_t hi = shr_at(base2000, off + 1, interleave);
    SHRColor c;
    c.v = static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
    return c;
}

template<typename FrameT>
static void generate_shr_line(const uint8_t *base2000, bool interleave, FrameT *f,
                              uint16_t src_line, uint16_t dst_line) {
    SHRMode mode;
    mode.v = shr_at(base2000, static_cast<uint16_t>(0x7D00 + src_line), interleave);
    uint8_t p_num = mode.p;
    f->set_line(dst_line);
    if (mode.mode640) {
        for (int x = 0; x < 160; x++) {
            uint8_t pval = shr_at(base2000, static_cast<uint16_t>(src_line * 160 + x), interleave);
            f->push(convert12bitTo24bit(shr_palette_color(base2000, p_num, pixel640<3>(pval) + 0x8, interleave)));
            f->push(convert12bitTo24bit(shr_palette_color(base2000, p_num, pixel640<2>(pval) + 0x0C, interleave)));
            f->push(convert12bitTo24bit(shr_palette_color(base2000, p_num, pixel640<1>(pval) + 0x00, interleave)));
            f->push(convert12bitTo24bit(shr_palette_color(base2000, p_num, pixel640<0>(pval) + 0x04, interleave)));
        }
    } else {
        // Same scanline fill-seed as VideoScanGenerator_RGB (MAME / John Brooks).
        static const uint32_t fillmode_init[32] = {
            2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
        };
        RGBA_t lastpixel = convert12bitTo24bit(
            shr_palette_color(base2000, p_num, static_cast<uint8_t>(fillmode_init[src_line & 0x1F]), interleave));
        for (int x = 0; x < 160; x++) {
            uint8_t pval = shr_at(base2000, static_cast<uint16_t>(src_line * 160 + x), interleave);
            uint8_t pix = pixel320<1>(pval);
            if (!mode.fill || pix != 0) {
                lastpixel = convert12bitTo24bit(shr_palette_color(base2000, p_num, pix, interleave));
            }
            f->push(lastpixel);
            f->push(lastpixel);
            pix = pixel320<0>(pval);
            if (!mode.fill || pix != 0) {
                lastpixel = convert12bitTo24bit(shr_palette_color(base2000, p_num, pix, interleave));
            }
            f->push(lastpixel);
            f->push(lastpixel);
        }
    }
}

static void generate_shr(const uint8_t *base2000, bool interleave, Frame640 *f) {
    if (!base2000 || !f) {
        return;
    }
    for (uint16_t line = 0; line < 200; line++) {
        generate_shr_line(base2000, interleave, f, line, line);
    }
}

static void generate_shr_voc400(const uint8_t *e1, bool e1_interleave,
                                const uint8_t *e0, bool e0_interleave,
                                Frame640x400 *f) {
    if (!e1 || !e0 || !f) {
        return;
    }
    for (uint16_t dst = 0; dst < 400; dst++) {
        uint16_t src = dst >> 1;
        if (dst & 1) {
            generate_shr_line(e0, e0_interleave, f, src, dst);
        } else {
            generate_shr_line(e1, e1_interleave, f, src, dst);
        }
    }
}

struct AppleII_View::Impl {
    AppleII_Context ctx;
    AppleII_Composite composite;
    AppleII_GSRGB gsrgb;

    explicit Impl(CharRom *rom) : ctx(rom), composite(ctx), gsrgb(ctx) {}
};

AppleII_View::AppleII_View(CharRom *char_rom) : impl_(std::make_unique<Impl>(char_rom)) {}

AppleII_View::~AppleII_View() = default;

void AppleII_View::set_char_set(uint16_t char_set) { impl_->ctx.set_char_set(char_set); }

void AppleII_View::set_normal_alt(bool normal_alt) { impl_->ctx.set_normal_alt(normal_alt); }

void AppleII_View::set_flash_state(bool flash_state) { impl_->ctx.flash_state = flash_state; }

void AppleII_View::set_text_fg(uint8_t fg) { impl_->ctx.text_fg = fg & 0x0F; }

void AppleII_View::set_text_bg(uint8_t bg) { impl_->ctx.text_bg = bg & 0x0F; }

void AppleII_View::generate(video_decode_mode_t decode, video_render_mode_t render,
                            const uint8_t *main, const uint8_t *aux, Frame560RGBA *out560,
                            Frame640 *out640, bool shr_phys_interleave) {
    if (decode == video_decode_mode_t::SHR) {
        generate_shr(main, shr_phys_interleave, out640);
        return;
    }
    if (!main || !out560) {
        return;
    }
    if (mode_needs_aux(decode) && !aux) {
        return;
    }
    if (render == video_render_mode_t::RGB) {
        impl_->gsrgb.generate(decode, main, aux, out560);
    } else {
        impl_->composite.generate(decode, render == video_render_mode_t::NTSC, main, aux, out560);
    }
}

void AppleII_View::generate_voc400(const uint8_t *e1_2000, const uint8_t *e0_2000,
                                   Frame640x400 *out,
                                   bool e1_phys_interleave, bool e0_phys_interleave) {
    generate_shr_voc400(e1_2000, e1_phys_interleave, e0_2000, e0_phys_interleave, out);
}

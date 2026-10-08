#pragma once

/*
 * Frozen reference copy of the byte-per-dot composite pipeline, as it was before
 * the packed bitstream (Docs/NTSCShader.md, phase 1): VideoScanGenerator_Comp writing
 * a Frame560, decoded by NTSC560 / Monochrome560. Only the output plumbing differs:
 * renderers write into a plain 910-wide RGBA buffer instead of a locked texture,
 * and mono dots are white.
 *
 * Do not "fix" or optimize this file; it is the oracle for vsgtest.
 */

#include <cstdint>

#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/ScanBuffer.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "display/ntsc.hpp"

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

namespace ref {

constexpr RGBA_t black = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
constexpr RGBA_t white = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF);

class RefRender {
public:
    bool shift_enabled = true;
    virtual ~RefRender() {}
    virtual void render(Frame560 *frame_byte, RGBA_t *base, uint32_t pitch) = 0;
};

class RefNTSC560 : public RefRender {
public:
    void render(Frame560 *frame_byte, RGBA_t *base, uint32_t pitch) override {
        uint16_t framewidth = frame_byte->width();

        for (uint16_t y = 0; y < 192; y++)
        {
            color_mode_t color_mode = frame_byte->get_color_mode(y);
            uint16_t phase_offset = color_mode.phase_offset;
            uint32_t bits = 0;
            frame_byte->set_line(y);
            RGBA_t *out = base + (size_t)(y + 35) * pitch + (168 - 7 * shift_enabled);

            if (phase_offset == 0 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }
            if (color_mode.colorburst == 0) {
                for (uint16_t x = 0; x < framewidth; x++) {
                    uint8_t bit = frame_byte->pull();
                    *out++ = bit ? white : black;
                }
            } else {
                for (uint16_t i = 0; i < NUM_TAPS; i++)
                {
                    bits = bits >> 1;
                    if (frame_byte->pull())
                        bits = bits | (1 << ((NUM_TAPS*2)));
                }

                for (uint16_t x = 0; x < framewidth; x++)
                {
                    bits = bits >> 1;
                    if ((x < framewidth-NUM_TAPS) && (frame_byte->pull()))
                        bits = bits | (1 << ((NUM_TAPS*2)));

                    uint32_t phase = (phase_offset + x) % 4;
                    *out++ = g_hgr_LUT[phase][bits];
                }
            }
            if (phase_offset == 1 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }
        }
    }
};

class RefMono560 : public RefRender {
public:
    void render(Frame560 *frame_byte, RGBA_t *base, uint32_t pitch) override {
        for (size_t l = 0; l < 192; l++) {
            frame_byte->set_line(l);
            RGBA_t *out = base + (size_t)(l + 35) * pitch + (168 - 7 * shift_enabled);

            color_mode_t color_mode = frame_byte->get_color_mode(l);
            uint8_t phase_offset = color_mode.phase_offset;
            if (phase_offset == 0 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }

            uint32_t fw = frame_byte->width();
            for (size_t i = 0; i < fw; i++) {
                uint8_t bit = frame_byte->pull();
                *out++ = bit ? white : black;
            }

            if (phase_offset == 1 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }
        }
    }
};

class RefComp {
    static constexpr int R_CHAR_NUM = 256;
    static constexpr int R_CHAR_WIDTH = 16;
    static constexpr int R_CELL_WIDTH = 14;

    uint8_t hires40Font[2 * R_CHAR_NUM * R_CHAR_WIDTH];
    CharRom *char_rom = nullptr;
    uint16_t char_set = 0;
    bool flash_state = false;
    uint32_t flash_counter = 0;
    bool dhgr_mono_mode = false;

    uint32_t hcount = 0;
    uint32_t vcount = 0;
    bool sawdata = false;
    uint8_t lastByte = 0x00;
    color_mode_t color_mode = {};
    uint8_t color_delay_mask = 0xFF;
    bool modeChecks = true;

    Frame560 *frame_byte = nullptr;
    RefRender *render = nullptr;

    void build_hires40Font(bool delayEnabled)
    {
        for (int i = 0; i < 2 * R_CHAR_NUM; i++)
        {
            uint8_t value = (i & 0x7f) << 1 | (i >> 8);
            bool delay = delayEnabled && (i & 0x80);

            for (int x = 0; x < R_CHAR_WIDTH; x++)
            {
                bool bit = (value >> ((x + 2 - delay) >> 1)) & 0x1;
                hires40Font[i * R_CHAR_WIDTH + x] = bit ? 1 : 0;
            }
        }
    }

public:
    RefComp(CharRom *charrom) : char_rom(charrom) {
        build_hires40Font(true);
        frame_byte = new Frame560(560, 263);
        frame_byte->clear(0);
        frame_byte->set_line(0);
    }
    ~RefComp() { delete frame_byte; }

    void set_render(RefRender *r) { render = r; }
    Frame560 *get_frame_byte() { return frame_byte; }
    void set_dhgr_mono_mode(bool m) { dhgr_mono_mode = m; }
    void set_char_set(uint16_t cs) { char_set = cs; }

    void generate_frame(ScanBuffer *frame_scan, RGBA_t *base, uint32_t pitch)
    {
        uint64_t fcnt = frame_scan->get_count();

        flash_counter++;
        if (flash_counter > 14) {
            flash_state = !flash_state;
            flash_counter = 0;
        }

        while (fcnt--) {
            Scan_t scan = frame_scan->pull();
            if (modeChecks && scan.mode <= VM_DHIRES) {
                color_mode.colorburst = (scan.mode == VM_TEXT40 || scan.mode == VM_TEXT80) ? 0 : 1;
                color_mode.mixed_mode = scan.flags & VS_FL_MIXED ? 1 : 0;
                modeChecks = false;
            }

            uint8_t eff_mode = scan.mode;
            if ((eff_mode <= VM_DHIRES) && (scan.flags & VS_FL_MIXED) && (vcount >= 160)) {
                eff_mode = (scan.flags & VS_FL_80COL) ? VM_TEXT80 : VM_TEXT40;
            }

            switch (eff_mode) {
                case VM_VSYNC:
                    vcount = 0;
                    frame_byte->set_line_v(vcount);
                    sawdata = false;
                    fcnt = 0;
                    break;

                case VM_HSYNC:
                    lastByte = 0x00;
                    hcount = 0; if (sawdata) vcount++;
                    frame_byte->set_line(vcount);
                    modeChecks = true;
                    break;

                case VM_TEXT40: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = color_mode;
                            cmode.phase_offset = 0;
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);

                        uint8_t tchar = scan.mainbyte;
                        uint8_t invert = char_rom->is_flash(tchar) ? (flash_state ? 0xFF : 0x00) : 0x00;
                        uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                        cdata ^= invert;

                        for (int n = 0; n < 7; n++) {
                            frame_byte->push_n((cdata & 1), 2);
                            cdata >>= 1;
                        }
                    }
                    hcount++;
                    break;
                case VM_TEXT80: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = color_mode;
                            cmode.phase_offset = 1;
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t tchar = scan.auxbyte;
                        char_rom->set_char_set(char_set, scan.flags & VS_FL_ALTCHARSET);
                        uint8_t cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                        uint8_t invert = char_rom->is_flash(tchar) ? (flash_state ? 0xFF : 0x00) : 0x00;
                        cdata ^= invert;
                        for (int n = 0; n < 7; n++) {
                            frame_byte->push((cdata & 1)); cdata >>= 1;
                        }

                        tchar = scan.mainbyte;
                        cdata = char_rom->get_char_scanline(tchar, vcount & 0b111);
                        invert = char_rom->is_flash(tchar) ? (flash_state ? 0xFF : 0x00) : 0x00;
                        cdata ^= invert;
                        for (int n = 0; n < 7; n++) {
                            frame_byte->push((cdata & 1)); cdata >>= 1;
                        }
                    }
                    hcount++;
                    break;
                case VM_LORES_7M: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = {1, 0, 0};
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t tchar = scan.mainbyte;
                        if (vcount & 4) tchar = tchar >> 4;
                        uint32_t pixeloff = (hcount * 14) % 4;
                        for (size_t bits = 0; bits < R_CELL_WIDTH / 2; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push_n(bit, 2);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                    }
                    hcount++;
                    break;
                case VM_LORES: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = {1, 0, 0};
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t tchar = scan.mainbyte;
                        if (vcount & 4) tchar = tchar >> 4;
                        uint32_t pixeloff = (hcount * 14) % 4;
                        for (size_t bits = 0; bits < R_CELL_WIDTH; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push(bit);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                    }
                    hcount++;
                    break;
                case VM_DLORES: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = {1, 0, 1};
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t tchar = scan.auxbyte;
                        if (vcount & 4) tchar = tchar >> 4;
                        uint32_t pixeloff = (hcount * 14) % 4;
                        for (size_t bits = 0; bits < 7; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push(bit);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                        tchar = scan.mainbyte;
                        if (vcount & 4) tchar = tchar >> 4;
                        pixeloff = (hcount * 14) % 4;
                        for (size_t bits = 0; bits < 7; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push(bit);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                    }
                    hcount++;
                    break;

                case VM_HIRES_NOSHIFT:
                    color_delay_mask = 0x7F;
                    [[fallthrough]];
                case VM_HIRES: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = {1, 0, 0};
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t byte = scan.mainbyte & color_delay_mask;
                        size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * R_CHAR_WIDTH;
                        for (size_t i = 0; i < 14; i++) {
                            frame_byte->push(hires40Font[fontIndex + i]);
                        }
                        lastByte = byte;
                    }
                    color_delay_mask = 0xFF;
                    hcount++;
                    break;
                case VM_DHIRES: {
                        sawdata = true;
                        if (hcount == 0) {
                            color_mode_t cmode = {1, 0, 1};
                            if (dhgr_mono_mode) cmode.colorburst = 0;
                            frame_byte->set_color_mode(vcount, cmode);
                        }
                        uint8_t byteM = scan.mainbyte;
                        uint8_t byteA = scan.auxbyte;
                        for (size_t i = 0; i < 7; i++) {
                            frame_byte->push((byteA & 0x01));
                            byteA >>= 1;
                        }
                        for (size_t i = 0; i < 7; i++) {
                            frame_byte->push((byteM & 0x01));
                            byteM >>= 1;
                        }
                    }
                    hcount++;
                    break;
                default:
                    break;
            }
        }

        render->render(frame_byte, base, pitch);
    }
};

} // namespace ref

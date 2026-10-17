#pragma once

#include "frame/Frames.hpp"
#include "frame/FrameBits.hpp"
#include "CharRom.hpp"
#include "ScanBuffer.hpp"
#include "generate/AppleIIgs.hpp"
#include "render/Render.hpp"
#include "VideoScanGenerator_Intf.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class VideoScanGenerator_Comp : public VideoScanGeneratorIntf
{
private:
    // 14-dot patterns, bit 0 = leftmost dot.
    uint16_t dbl7[128];          // 7 bits, each doubled
    uint16_t lores14[16][2];     // [nibble][hcount & 1]
    uint16_t lores7m[16][2];     // 7 rotated bits, each doubled
    uint16_t lo7[16][2];         // 7 rotated bits (DLORES half)
    uint16_t hires14[2 * CHAR_NUM]; // [byte | (lastByte & 0x40) << 2]
    void build_patterns(bool delayEnabled);
    CharRom *char_rom = nullptr;
    uint16_t char_set = 0;
    bool flash_state = false;
    uint32_t flash_counter = 0;

    bool display_shift_enabled = true;

    bool dhgr_mono_mode = false;
    bool mono_mode = false;
    bool dump_next_frame = false;


    SHRMode mode = { .p = 0 }; 
    Palette palette = { .colors = {0} };
    RGBA_t lastpixel = {0};

    uint32_t hcount = 0;
    uint32_t vcount = 0;
    uint32_t beam_h = 0;
    uint32_t beam_v = 0;
    bool sawdata = false;
    uint16_t scanner_freq = 14;

    uint8_t lastByte = 0x00; // for hires
    color_mode_t color_mode = {};
    
    uint8_t color_delay_mask = 0xFF;

    int palette_index = 0; // reset to 0 each scanline.
    bool modeChecks = true;

    FrameVSG *frame_vsg = nullptr;
    FrameBits *frame_bits = nullptr;
    Render *render = nullptr;

    // Packed line writer. cur is null when vcount is past the visible lines.
    uint32_t *cur = nullptr;
    uint64_t acc = 0;
    uint32_t nbits = 0;
    uint32_t word = 0;

    inline void select_line(uint32_t y) noexcept {
        cur = (y < FrameBits::LINES) ? frame_bits->line(y) : nullptr;
        acc = 0; nbits = 0; word = FrameBits::DOT_WORDS;
    }

    inline void start_line(color_mode_t cmode) noexcept {
        if (!cur) return;
        uint32_t line_start = (display_shift_enabled && cmode.phase_offset == 0) ? 7 : 0;
        cur[FrameBits::DOT_WORDS] = FrameBits::make_mode(cmode.colorburst, cmode.phase_offset, line_start);
        acc = 0; nbits = line_start; word = 0;
    }

    inline void emit14(uint32_t pattern) noexcept {
        acc |= (uint64_t)pattern << nbits;
        nbits += 14;
        if (nbits >= 32) {
            if (word < FrameBits::DOT_WORDS) cur[word++] = (uint32_t)acc;
            acc >>= 32;
            nbits -= 32;
        }
    }

    inline void flush_line() noexcept {
        if (nbits > 0 && word < FrameBits::DOT_WORDS) cur[word] = (uint32_t)acc;
        acc = 0; nbits = 0; word = FrameBits::DOT_WORDS;
    }

public:
    VideoScanGenerator_Comp(CharRom *charrom, bool border_enabled = false, FrameVSG *frame_vsg = nullptr);
    ~VideoScanGenerator_Comp() { delete frame_bits; }

    virtual void generate_frame(ScanBuffer *frame_scan);
    virtual void set_display_shift(bool enable) { display_shift_enabled = enable; }
    virtual void set_dhgr_mono_mode(bool mono) { dhgr_mono_mode = mono; }
    virtual bool get_dhgr_mono_mode() const { return dhgr_mono_mode; }
    virtual void set_mono_mode(bool mono) { mono_mode = mono; }
    virtual bool get_mono_mode() const { return mono_mode; }
    virtual void set_char_set(uint16_t char_set) { this->char_set = char_set; }
    virtual void set_render(Render *render) { this->render = render; }
    const FrameBits *get_frame_bits() const { return frame_bits; }
    virtual void setDumpNextFrame(bool dump) { dump_next_frame = dump; }
    virtual uint32_t get_h() const { return beam_h; }
    virtual uint32_t get_v() const { return beam_v; }
};
#pragma once

#include <cassert>
#include <cstdint>

#include "util/RingBuffer.hpp"
#include "AppleIIgsColors.hpp"
#include "frame/Frames.hpp"
#include "CharRom.hpp"
#include "ScanBuffer.hpp"
#include "generate/AppleIIgs.hpp"
#include "render/GSRGB_LUT.hpp"
#include "render/Render.hpp"
#include "VideoScanGenerator_Intf.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class VideoScanGenerator_RGB : public VideoScanGeneratorIntf
{
private:

    CharRom *char_rom = nullptr;
    uint16_t char_set = 0;
    bool flash_state = false;
    uint32_t flash_counter = 0;

    bool display_shift_enabled = true;

    bool dhgr_mono_mode = false;
    bool mono_mode = false;
    /* bool dump_next_frame = false; */

    SHRMode mode = { .p = 0 }; 
    Palette palette = { .colors = {0} };
    RGBA_t lastpixel = {0};
    uint64_t shiftreg = 0;
    RingBuffer<uint8_t, 32> bit_stream;
    int phase_offset = 0;

    uint32_t hcount = 0; // count of pixel data cycles processed on this scanline (excludes border). Counts from 0 to 39.
    uint32_t vcount = 0; // where we're at in content area of frame. Counts from 0 to 199.
    uint32_t beam_h = 0;
    uint32_t beam_v = 0;
    bool sawdata = false;
    uint16_t scanner_freq = 14;

    uint8_t lastByte = 0x00; // for hires
    color_mode_t color_mode = {};
    bool dump_next_frame = false;
    
    uint8_t color_delay_mask = 0xFF;

    int palette_index = 0; // reset to 0 each scanline.
    bool modeChecks = true;

    //ScanBuffer *frame_scan = nullptr;
    FrameVSG *frame_vsg = nullptr;
    Render *render = nullptr;
    
    // hires mapping (process data in from GuS)
    // the hgr table ("16 colors") are not the same layout/assignment as the text/lores table. So maintain it separately.
    uint16_t *lut = nullptr; // TODO: change this code to access this table directly, avoid pointer indirection.

    RGBA_t hgr_lut[16]; // active color set
    RGBA_t hgr_color_lut[16]; // precalculated color set
    RGBA_t hgr_mono_lut[16]; // precalculated mono set

    RGBA_t txt_lut[16]; // active color set
    RGBA_t txt_color_lut[16]; // precalculated color set
    RGBA_t txt_mono_lut[16]; // precalculated mono set

    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];

    void build_hires40Font(bool delayEnabled);
    void add_hires_bits(uint8_t hires_byte);
    void add_dhires_bits(uint8_t main_byte, uint8_t aux_byte);
    //void render_hires(bool start_of_line, bool end_of_line);
    void render_hires();
    void hires_start();
    void emit_hires_pixels(uint16_t shift);
    void render_hires_mono();
    void build_mono_lut(RGBA_t *ct, RGBA_t *mt);
    inline void update_mono_lut() { 
        if (mono_mode) {
            for (int i = 0; i < 16; i++) {
                txt_lut[i] = txt_mono_lut[i];
                hgr_lut[i] = hgr_mono_lut[i];
            } 
        } else {
            for (int i = 0; i < 16; i++) {
                txt_lut[i] = txt_color_lut[i];
                hgr_lut[i] = hgr_color_lut[i];
            }
        }
    }

public:
    VideoScanGenerator_RGB(CharRom *charrom, bool border_enabled = false, FrameVSG *frame_vsg = nullptr);

    virtual void generate_frame(ScanBuffer *frame_scan);
    virtual void set_display_shift(bool enable) { display_shift_enabled = enable; }
    virtual void set_dhgr_mono_mode(bool mono) { dhgr_mono_mode = mono; }
    virtual bool get_dhgr_mono_mode() const { return dhgr_mono_mode; }
    virtual void set_mono_mode(bool mono) { mono_mode = mono; update_mono_lut();}
    virtual bool get_mono_mode() const { return mono_mode; }
    virtual void set_char_set(uint16_t char_set) { this->char_set = char_set; }
    /* void saveScanBufferToFile(ScanBuffer *frame_scan, const char *filename);*/
    virtual void setDumpNextFrame(bool dump) { dump_next_frame = dump; }
    virtual void set_render(Render *render) { this->render = render; }
    virtual uint32_t get_h() const { return beam_h; }
    virtual uint32_t get_v() const { return beam_v; }
};
#pragma once

#include "frame/Frames.hpp"
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
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];
    void build_hires40Font(bool delayEnabled);
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
    Frame560 *frame_byte = nullptr;
    Render *render = nullptr;

public:
    VideoScanGenerator_Comp(CharRom *charrom, bool border_enabled = false, FrameVSG *frame_vsg = nullptr);

    virtual void generate_frame(ScanBuffer *frame_scan);
    virtual void set_display_shift(bool enable) { display_shift_enabled = enable; }
    virtual void set_dhgr_mono_mode(bool mono) { dhgr_mono_mode = mono; }
    virtual bool get_dhgr_mono_mode() const { return dhgr_mono_mode; }
    virtual void set_mono_mode(bool mono) { mono_mode = mono; }
    virtual bool get_mono_mode() const { return mono_mode; }
    virtual void set_char_set(uint16_t char_set) { this->char_set = char_set; }
    virtual void set_render(Render *render) { this->render = render; }
    virtual void setDumpNextFrame(bool dump) { dump_next_frame = dump; }
    virtual uint32_t get_h() const { return beam_h; }
    virtual uint32_t get_v() const { return beam_v; }
};
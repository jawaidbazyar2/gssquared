#pragma once

#include "frame/Frames.hpp"
#include "CharRom.hpp"
#include "ScanBuffer.hpp"
#include "generate/AppleIIgs.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class VideoScanGenerator
{
private:
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];
    void build_hires40Font(bool delayEnabled);
    CharRom *char_rom = nullptr;
    uint16_t char_set = 0;
    bool flash_state = false;
    uint32_t flash_counter = 0;

    bool display_shift_enabled = true;

    bool mono_mode = false;
    bool dump_next_frame = false;


    SHRMode mode = { .p = 0 }; 
    Palette palette = { .colors = {0} };
    RGBA_t lastpixel = {0};

    uint32_t hcount = 0;
    uint32_t vcount = 0;
    uint32_t vcount_real = 0;
    
    uint8_t lastByte = 0x00; // for hires
    color_mode_t color_mode = {};
    
    uint8_t color_delay_mask = 0xFF;

    int palette_index = 0; // reset to 0 each scanline.
    bool modeChecks = true;

public:
    VideoScanGenerator(CharRom *charrom, bool border_enabled = false);

    void generate_frame(ScanBuffer *frame_scan, Frame560 *frame_byte, FrameBorder *border = nullptr, Frame640 *frame_shr = nullptr);
    void set_display_shift(bool enable) { display_shift_enabled = enable; }
    void set_mono_mode(bool mono) { mono_mode = mono; }
    void set_char_set(uint16_t char_set) { this->char_set = char_set; }
    void saveScanBufferToFile(ScanBuffer *frame_scan, const char *filename);
    void setDumpNextFrame(bool dump) { dump_next_frame = dump; }
};
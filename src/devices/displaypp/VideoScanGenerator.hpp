#pragma once

#include "frame/Frames.hpp"
#include "CharRom.hpp"
#include "ScanBuffer.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class VideoScanGenerator
{
private:
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];
    void build_hires40Font(bool delayEnabled);
    CharRom *char_rom = nullptr;
    bool flash_state = false;
    uint16_t flash_counter = 0;
    //bool border_enabled = false;

public:
    VideoScanGenerator(CharRom *charrom, bool border_enabled = false);

    void generate_frame(ScanBuffer *frame_scan, Frame560 *frame_byte, FrameBorder *border = nullptr, Frame640 *frame_shr = nullptr);
};
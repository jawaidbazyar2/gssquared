#pragma once

#include "frame/Frames.hpp"
#include "CharRom.hpp"

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

public:
    VideoScanGenerator(CharRom *charrom);

    void generate_frame(FrameScan560 *frame_scan, Frame560 *frame_byte);
};
#pragma once

#include "frame/Frames.hpp"

#define CHAR_NUM 256
#define CHAR_WIDTH 16
#define CELL_WIDTH 14

class VideoScanGenerator
{
private:
    uint8_t hires40Font[2 * CHAR_NUM * CHAR_WIDTH];
    void build_hires40Font(bool delayEnabled);

public:
    VideoScanGenerator();

    void generate_frame(FrameScan560 *frame_scan, Frame560 *frame_byte);
};
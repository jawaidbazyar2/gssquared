#include <cstdint>
#include <cstdio>
#include <cstring>

#include "frame_bit.hpp"
#include "util/printf_helper.hpp"

Frame_Bitstream::Frame_Bitstream(uint16_t width, uint16_t height) { // pixels
    f_width = width;
    f_height = height;
    scanline = 0;
    hpos = 0;
    hloc = 0;
    working = 0;
}

Frame_Bitstream::~Frame_Bitstream() {
  // nothing to do
}

void Frame_Bitstream::print() {
    for (int i = 0; i < FB_BITSTREAM_HEIGHT; i++) {
        for (int j = 0; j < FB_BITSTREAM_WIDTH_WORDS; j++) {
            printf("%016llx ", u64_t(display_bitstream[i][j]));
        }
        printf("\n");
    }
}

void Frame_Bitstream::clear() {
    memset(display_bitstream, 0, sizeof(display_bitstream));
}
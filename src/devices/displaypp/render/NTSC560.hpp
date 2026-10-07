#pragma once

#include "Render.hpp"
#include "display/ntsc.hpp"
#include "display/filters.hpp"

/** Generate a 'frame' (i.e., a group of 8 scanlines) of video output data using the lookup table.  */

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

class NTSC560 : public Render {

public:
    NTSC560(bool shift_enabled = true) : Render(shift_enabled) {
        setupConfig();
        generate_filters(NUM_TAPS);
        init_hgr_LUT();
    };
    ~NTSC560() {};

    // Reads and writes rows through local __restrict pointers instead of Frame::push/pull:
    // uint8_t and the RGBA_t union may alias anything, which otherwise forces the compiler
    // to reload and store Frame::row / Frame::hloc on every pixel.
    virtual void render(Frame560 *frame_byte, FrameVSG *frame_rgba) override {
        constexpr uint32_t TOP_BIT = 1u << (NUM_TAPS * 2);
        const uint32_t framewidth = frame_byte->width();
        const uint32_t body_end = framewidth - NUM_TAPS; // last x with a lookahead bit available
        const RGBA_t mono = mono_color;

        for (uint32_t y = 0; y < 192; y++)
        {
            color_mode_t color_mode = frame_byte->get_color_mode(y);
            uint32_t phase_offset = color_mode.phase_offset;
            const uint8_t * __restrict in = frame_byte->line(y);
            RGBA_t * __restrict out = frame_rgba->line(y + 35) + (168 - 7 * shift_enabled);

            if (phase_offset == 0 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }
            if (color_mode.colorburst == 0) {
                for (uint32_t x = 0; x < framewidth; x++) {
                    out[x] = in[x] ? mono : black;
                }
            } else {
                // for x = 0, we need bits preloaded with the first NUM_TAPS+1 bits in 
                // 16-8, and 0's in 0-7.
                // if num_taps = 6, 
                // 11111 1X000000
                uint32_t bits = 0;
                for (uint32_t i = 0; i < NUM_TAPS; i++) {
                    bits = (bits >> 1) | (in[i] ? TOP_BIT : 0);
                }

                const RGBA_t *lut0 = g_hgr_LUT[(phase_offset + 0) & 3];
                const RGBA_t *lut1 = g_hgr_LUT[(phase_offset + 1) & 3];
                const RGBA_t *lut2 = g_hgr_LUT[(phase_offset + 2) & 3];
                const RGBA_t *lut3 = g_hgr_LUT[(phase_offset + 3) & 3];
                const uint8_t * __restrict ahead = in + NUM_TAPS;

                uint32_t x = 0;
                for (; x + 4 <= body_end; x += 4) {
                    bits = (bits >> 1) | (ahead[x + 0] ? TOP_BIT : 0); out[x + 0] = lut0[bits];
                    bits = (bits >> 1) | (ahead[x + 1] ? TOP_BIT : 0); out[x + 1] = lut1[bits];
                    bits = (bits >> 1) | (ahead[x + 2] ? TOP_BIT : 0); out[x + 2] = lut2[bits];
                    bits = (bits >> 1) | (ahead[x + 3] ? TOP_BIT : 0); out[x + 3] = lut3[bits];
                }
                const RGBA_t *luts[4] = { lut0, lut1, lut2, lut3 };
                for (; x < framewidth; x++) {
                    // at end of line insert 0s
                    bits = (bits >> 1) | ((x < body_end && ahead[x]) ? TOP_BIT : 0);
                    out[x] = luts[x & 3][bits];
                }
            }
            out += framewidth;
            if (phase_offset == 1 && shift_enabled) {
                for (int i = 0; i < 7; i++) *out++ = black;
            }
        }
    }
};

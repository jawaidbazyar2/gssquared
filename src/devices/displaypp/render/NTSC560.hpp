#pragma once

#include <cstring>

#include "Render.hpp"
#include "display/ntsc.hpp"
#include "display/filters.hpp"

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

class NTSC560 : public Render {

public:
    NTSC560(bool shift_enabled = true) : Render(shift_enabled) {
        setupConfig();
        generate_filters(NUM_TAPS);
        init_hgr_LUT();
    };
    ~NTSC560() {};

    virtual void render_lines(const FrameBits &bits, RGBA_t *base, uint32_t pitch_pixels) override {
        constexpr uint32_t WINDOW_MASK = (1u << (NUM_TAPS * 2 + 1)) - 1;
        constexpr uint32_t DATA_DOTS = 560;
        const uint32_t width = region_width();
        uint32_t buf[22];
        ensure_hgr_LUT();

        for (uint32_t y = 0; y < FrameBits::LINES; y++) {
            load_line(bits, y, buf);
            RGBA_t * __restrict out = base + (size_t)(y + 35) * pitch_pixels + region_x();

            if (bits.colorburst(y) == 0) {
                expand_mono(buf, out, width);
                continue;
            }

            const uint32_t line_start = bits.line_start(y);
            const uint32_t phase_offset = bits.phase_offset(y);
            const uint32_t data_end = line_start + DATA_DOTS;

            for (uint32_t c = 0; c < line_start; c++) out[c] = black;
            for (uint32_t c = data_end; c < width; c++) out[c] = black;

            const RGBA_t * __restrict lut0 = g_hgr_LUT[(phase_offset + 0) & 3];
            const RGBA_t * __restrict lut1 = g_hgr_LUT[(phase_offset + 1) & 3];
            const RGBA_t * __restrict lut2 = g_hgr_LUT[(phase_offset + 2) & 3];
            const RGBA_t * __restrict lut3 = g_hgr_LUT[(phase_offset + 3) & 3];

            // Window bit k is dot c - NUM_TAPS + k. buf[1] holds dot 0, so dot d is bit d + 32 of buf.
            const uint8_t *bytes = reinterpret_cast<const uint8_t *>(buf);
            auto window = [bytes](uint32_t c) -> uint32_t {
                uint32_t pos = c + 32 - NUM_TAPS;
                uint64_t v;
                std::memcpy(&v, bytes + (pos >> 3), sizeof(v));
                return (uint32_t)(v >> (pos & 7)) & WINDOW_MASK;
            };

            for (uint32_t c = line_start; c < data_end; c += 4) {
                out[c + 0] = lut0[window(c + 0)];
                out[c + 1] = lut1[window(c + 1)];
                out[c + 2] = lut2[window(c + 2)];
                out[c + 3] = lut3[window(c + 3)];
            }
        }
    }
};

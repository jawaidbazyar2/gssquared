#pragma once

#include "Render.hpp"

/** Every line is decoded as colorburst off. Dots are written white; the monitor tint is applied to the texture. */
class Monochrome560 : public Render {

public:
    Monochrome560(bool shift_enabled = true) : Render(shift_enabled) {};
    ~Monochrome560() {};

    virtual void render_lines(const FrameBits &bits, RGBA_t *base, uint32_t pitch_pixels) override {
        const uint32_t width = region_width();
        uint32_t buf[22];
        for (uint32_t y = 0; y < FrameBits::LINES; y++) {
            load_line(bits, y, buf);
            expand_mono(buf, base + (size_t)(y + 35) * pitch_pixels + region_x(), width);
        }
    };
};

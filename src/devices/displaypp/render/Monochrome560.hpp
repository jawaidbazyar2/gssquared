#pragma once

#include "Render.hpp"

class Monochrome560 : public Render {

public:
    Monochrome560(bool shift_enabled = true) : Render(shift_enabled) {};
    ~Monochrome560() {};

    virtual void render(Frame560 *frame_byte, FrameVSG *frame_rgba) override {
        for (size_t l = 0; l < 192; l++) {
            frame_byte->set_line(l);
            frame_rgba->set_line(l+35);
            frame_rgba->advance(168-7*shift_enabled);

            color_mode_t color_mode = frame_byte->get_color_mode(l);
            uint8_t phase_offset = color_mode.phase_offset;
            if (phase_offset == 0 && shift_enabled) {
                frame_rgba->push_n(black, 7);
            }

            uint32_t fw = frame_byte->width();
            for (size_t i = 0; i < fw; i++) {
                uint8_t bit = frame_byte->pull();
                frame_rgba->push((bit /* & 1 */) ? mono_color : black);
            }

            if (phase_offset == 1 && shift_enabled) {
                frame_rgba->push_n(black, 7);
            }
        }
    };
};

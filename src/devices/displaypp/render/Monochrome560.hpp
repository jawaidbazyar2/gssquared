#include "Render.hpp"

class Monochrome560 : public Render {

public:
    Monochrome560(bool shift_enabled = true) : Render(shift_enabled) {};
    ~Monochrome560() {};

    void render(Frame560 *frame_byte, Frame560RGBA *frame_rgba, RGBA_t color) {
        for (size_t l = 0; l < 192; l++) {
            frame_rgba->set_line(l);
            frame_byte->set_line(l);

            color_mode_t color_mode = frame_byte->get_color_mode(l);
            uint8_t phase_offset = color_mode.phase_offset;
            if (phase_offset == 0 && shift_enabled) {
                for (size_t i = 0; i < 7; i++) {
                    frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0x00));
                }
            }

            uint32_t fw = frame_byte->width();
            for (size_t i = 0; i < fw; i++) {
                uint8_t bit = frame_byte->pull();
                frame_rgba->push((bit & 1) ? color :  RGBA_t::make(0x00, 0x00, 0x00, 0xFF));
            }

            if (phase_offset == 1 && shift_enabled) {
                for (size_t i = 0; i < 7; i++) {
                    frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0x00));
                }
            }
        }
    };
};

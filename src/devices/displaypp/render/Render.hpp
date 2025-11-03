#pragma once

#include "devices/displaypp/frame/frame.hpp"
#include "devices/displaypp/frame/Frames.hpp"

class Render {

    public:
        Render(bool shift_enabled = true) : shift_enabled(shift_enabled) {};
        ~Render() {};

        void set_shift_enabled(bool shift_enabled) { this->shift_enabled = shift_enabled; }

        void render(Frame560 *frame_byte, Frame560RGBA *frame_rgba);

    protected:
        bool shift_enabled = false;
};
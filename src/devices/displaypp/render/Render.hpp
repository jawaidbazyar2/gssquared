#pragma once

#include <cstring>

#include "devices/displaypp/frame/frame.hpp"
#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/frame/FrameBits.hpp"

class Render {

    public:
        Render(bool shift_enabled = true) : shift_enabled(shift_enabled) {};
        virtual ~Render() {};

        void set_shift_enabled(bool shift_enabled) { this->shift_enabled = shift_enabled; }
        bool get_shift_enabled() const { return shift_enabled; }

        // frame_vsg must be open (locked); its rows are 910 pixels.
        void render(const FrameBits &bits, FrameVSG *frame_vsg) {
            render_lines(bits, frame_vsg ? frame_vsg->data() : nullptr, FrameVSG::max_width());
        }

        // Writes rows 35..226 of a 910-wide RGBA buffer at base. base is null only for
        // headless generators (tests), whose renderers capture bits instead of drawing.
        virtual void render_lines(const FrameBits &bits, RGBA_t *base, uint32_t pitch_pixels) = 0;

    protected:
        bool shift_enabled = false;
        static constexpr RGBA_t black = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);
        static constexpr RGBA_t white = RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF);

        // Display region: 567 columns from 161 when shifted (IIe), else 560 from 168.
        inline uint32_t region_x() const { return 168 - 7 * shift_enabled; }
        inline uint32_t region_width() const { return 560 + 7 * shift_enabled; }

        // Copies a line's 18 dot words into buf[1..18] with zero guard words on both sides.
        static inline void load_line(const FrameBits &bits, uint32_t y, uint32_t (&buf)[22]) {
            buf[0] = 0;
            std::memcpy(&buf[1], bits.line(y), FrameBits::DOT_WORDS * sizeof(uint32_t));
            buf[19] = buf[20] = buf[21] = 0;
        }

        struct Mono8 { RGBA_t px[256][8]; };

        static const Mono8 &mono8() {
            static const Mono8 table = [] {
                Mono8 t;
                for (uint32_t b = 0; b < 256; b++)
                    for (uint32_t k = 0; k < 8; k++) t.px[b][k] = ((b >> k) & 1) ? white : black;
                return t;
            }();
            return table;
        }

        // Writes white/black per dot for columns [0, width).
        static inline void expand_mono(const uint32_t (&buf)[22], RGBA_t * __restrict out, uint32_t width) {
            const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&buf[1]);
            const Mono8 &m = mono8();
            uint32_t c = 0;
            for (; c + 8 <= width; c += 8) {
                std::memcpy(out + c, m.px[bytes[c >> 3]], sizeof(m.px[0]));
            }
            for (; c < width; c++) {
                out[c] = ((bytes[c >> 3] >> (c & 7)) & 1) ? white : black;
            }
        }
};

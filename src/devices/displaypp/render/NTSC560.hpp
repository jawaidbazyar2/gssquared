#include "Render.hpp"
#include "display/ntsc.hpp"
#include "display/filters.hpp"

/** Generate a 'frame' (i.e., a group of 8 scanlines) of video output data using the lookup table.  */

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

class NTSC560 : public Render {

public:
    NTSC560() {
        setupConfig();
        generate_filters(NUM_TAPS);
        init_hgr_LUT();
    };
    ~NTSC560() {};

    void render(Frame560 *frame_byte, Frame560RGBA *frame_rgba, RGBA_t color, uint16_t phaseoffset) {
        // Process each scanline
        uint16_t framewidth = frame_byte->width();
        color_mode_t color_mode = frame_byte->get_color_mode(0); // get color mode for this frame (based on scanline 0)

        for (uint16_t y = 0; y < 192; y++)
        {
            uint32_t bits = 0;
            frame_byte->set_line(y);
            frame_rgba->set_line(y);

            if (color_mode == COLORBURST_OFF) {
                // do nothing
                for (uint16_t x = 0; x < framewidth; x++) {
                    if (frame_byte->pull() != 0) {
                        frame_rgba->push(color);
                    } else {
                        frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0xFF));
                    }
                }
            } else {
                // do color burst

                // for x = 0, we need bits preloaded with the first NUM_TAPS+1 bits in 
                // 16-8, and 0's in 0-7.
                // if num_taps = 6, 
                // 11111 1X000000
                for (uint16_t i = 0; i < NUM_TAPS; i++)
                {
                    bits = bits >> 1;
                    if (frame_byte->pull() != 0) // TODO: if we assume values of 0 and 1 this might work a hair faster?
                        bits = bits | (1 << ((NUM_TAPS*2)));
                }
                
                // Process the scanline
                for (uint16_t x = 0; x < framewidth; x++)
                {
                    bits = bits >> 1;
                    if ((x < framewidth-NUM_TAPS) && (frame_byte->pull() != 0)) // at end of line insert 0s
                        bits = bits | (1 << ((NUM_TAPS*2)));

                    uint32_t phase = (phaseoffset + x) % 4;

                    //  Use the phase and the bits as the index
                    frame_rgba->push( g_hgr_LUT[phase][bits] );
                }
            }
        }
    }
};

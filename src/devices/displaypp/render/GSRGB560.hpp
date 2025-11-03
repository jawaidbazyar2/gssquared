#include "Render.hpp"

/** Generate a 'frame' (i.e., a group of 8 scanlines) of video output data using the lookup table.  */

// 16-entry RGB color table for Apple IIgs colors
// Each color is 4-bit R, G, B scaled to 8-bit RGBA
static const RGBA_t gs_rgb_colors_wrong[16] = {
    RGBA_t::make(0x00, 0x00, 0x00, 0xFF), // 0x0000 - Black        0b0000 - ok
    RGBA_t::make(0xDD, 0x00, 0x33, 0xFF), // 0x0D03 - Deep Red     0b0001 - ok
    RGBA_t::make(0x00, 0x00, 0x99, 0xFF), // 0x0009 - Dark Blue    0b0010 - brown
    RGBA_t::make(0xDD, 0x22, 0xDD, 0xFF), // 0x0D2D - Purple       0b0011 - orange
    RGBA_t::make(0x00, 0x77, 0x22, 0xFF), // 0x0072 - Dark Green   0b0100 - ok
    RGBA_t::make(0x55, 0x55, 0x55, 0xFF), // 0x0555 - Dark Gray    0b0101 - ok
    RGBA_t::make(0x22, 0x22, 0xFF, 0xFF), // 0x022F - Medium Blue  0b0110 - light green
    RGBA_t::make(0x66, 0xAA, 0xFF, 0xFF), // 0x06AF - Light Blue   0b0111 - yellow
    RGBA_t::make(0x88, 0x55, 0x00, 0xFF), // 0x0850 - Brown        0b1000 - dk blue
    RGBA_t::make(0xFF, 0x66, 0x00, 0xFF), // 0x0F60 - Orange       0b1001 - purple
    RGBA_t::make(0xAA, 0xAA, 0xAA, 0xFF), // 0x0AAA - Light Gray  0b1010 - ok
    RGBA_t::make(0xFF, 0x99, 0x88, 0xFF), // 0x0F98 - Pink        0b1011 - ok
    RGBA_t::make(0x11, 0xDD, 0x00, 0xFF), // 0x01D0 - Light Green 0b1100 - med blu
    RGBA_t::make(0xFF, 0xFF, 0x00, 0xFF), // 0x0FF0 - Yellow      0b1101 - lt blue
    RGBA_t::make(0x44, 0xFF, 0x99, 0xFF), // 0x04F9 - Aquamarine  0b1110 - ok
    RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)  // 0x0FFF - White       0b1111 - ok
};

static const RGBA_t gs_rgb_colors[16] = {
    RGBA_t::make(0x00, 0x00, 0x00, 0xFF), // 0x0000 - Black        0b0000 - ok
    RGBA_t::make(0xDD, 0x00, 0x33, 0xFF), // 0x0D03 - Deep Red     0b0001 - ok
    RGBA_t::make(0x88, 0x55, 0x00, 0xFF), // 0x0850 - Brown        0b1000 - dk blue
    RGBA_t::make(0xFF, 0x66, 0x00, 0xFF), // 0x0F60 - Orange       0b1001 - purple
    RGBA_t::make(0x00, 0x77, 0x22, 0xFF), // 0x0072 - Dark Green   0b0100 - ok
    RGBA_t::make(0x55, 0x55, 0x55, 0xFF), // 0x0555 - Dark Gray    0b0101 - ok
    RGBA_t::make(0x11, 0xDD, 0x00, 0xFF), // 0x01D0 - Light Green 0b1100 - med blu
    RGBA_t::make(0xFF, 0xFF, 0x00, 0xFF), // 0x0FF0 - Yellow      0b1101 - lt blue

    RGBA_t::make(0x00, 0x00, 0x99, 0xFF), // 0x0009 - Dark Blue    0b0010 - brown
    RGBA_t::make(0xDD, 0x22, 0xDD, 0xFF), // 0x0D2D - Purple       0b0011 - orange
    RGBA_t::make(0xAA, 0xAA, 0xAA, 0xFF), // 0x0AAA - Light Gray  0b1010 - ok
    RGBA_t::make(0xFF, 0x99, 0x88, 0xFF), // 0x0F98 - Pink        0b1011 - ok
    RGBA_t::make(0x22, 0x22, 0xFF, 0xFF), // 0x022F - Medium Blue  0b0110 - light green
    RGBA_t::make(0x66, 0xAA, 0xFF, 0xFF), // 0x06AF - Light Blue   0b0111 - yellow
    RGBA_t::make(0x44, 0xFF, 0x99, 0xFF), // 0x04F9 - Aquamarine  0b1110 - ok
    RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)  // 0x0FFF - White       0b1111 - ok
};


class GSRGB560 : public Render {

public:
    uint8_t barrel_shifter[4][16];
    GSRGB560(bool shift_enabled = true) : Render(shift_enabled) {
        // Set up the lookup table.
        for (uint8_t phase = 0; phase < 4; phase++) {
            for (uint8_t val = 0; val < 16; val++) {
                barrel_shifter[phase][val] = barrel_shift_right(phase, val);
            }
        }
        print_barrel();
    };

    ~GSRGB560() {};

    void print_barrel() {
        for (uint8_t i = 0; i < 16; i++) {
            for (uint8_t j = 0; j < 4; j++) {
                uint8_t v = barrel_shifter[j][i];
                switch (v) {
                    case 0: printf("0000 "); break;
                    case 1: printf("0001 "); break;
                    case 2: printf("0010 "); break;
                    case 3: printf("0011 "); break;
                    case 4: printf("0100 "); break;
                    case 5: printf("0101 "); break;
                    case 6: printf("0110 "); break;
                    case 7: printf("0111 "); break;
                    case 8: printf("1000 "); break;
                    case 9: printf("1001 "); break;
                    case 10: printf("1010 "); break;
                    case 11: printf("1011 "); break;
                    case 12: printf("1100 "); break;
                    case 13: printf("1101 "); break;
                    case 14: printf("1110 "); break;
                    case 15: printf("1111 "); break;
                    default: printf("?? "); break;
                }
            }
            printf("\n");
        }
    }


    uint8_t barrel_shift_right(uint8_t phase, uint8_t val) {
        uint8_t inval = val;
        for (uint8_t i = 0; i < phase; i++) {
            uint8_t nval = val >> 1;
            nval |= ((val & 0x1) << 3);
            val = nval;
        }
        printf("barrel phase: %d in: %d out: %d\n", phase, inval, val);
        return val & 0x0F;
    }

    uint8_t barrel_shift_left(uint8_t phase, uint8_t val) {
        uint8_t inval = val;
        for (uint8_t i = 0; i < phase; i++) {
            uint8_t nval = val << 1;
            nval |= ((val & 0x8) >> 3);
            val = nval;
        }
        printf("barrel phase: %d in: %d out: %d\n", phase, inval, val);
        return val & 0x0F;
    }

    void render_old(Frame560 *frame_byte, Frame560RGBA *frame_rgba, RGBA_t color, uint16_t phaseoffset) {

        for (uint16_t y = 0; y < 192; y++)
        {
            // Process each scanline
            uint8_t phase = 0;
            uint8_t ShiftReg = 0;
            uint8_t BSOut = 0;
            uint8_t LatchOut = 0;
            uint8_t MuxOut = 0;

            bool JK43 = 0;
            bool JK43_J = 0;
            bool JK43_K = 0;
            bool JK44 = 0;
            bool JK44_J = 0;
            bool JK44_K = 0;

            frame_byte->set_line(y);
            frame_rgba->set_line(y);
            uint16_t framewidth = frame_byte->width();
            for (uint16_t x = 0; x < framewidth; x++) {
                // PRE-Clock

                bool INP = frame_byte->pull();
                bool SR3 = (ShiftReg & 0x8) >> 3;
                bool SR2 = (ShiftReg & 0x4) >> 2;

                // the barrel shifter needs to rotate to the RIGHT based on phase number.
                BSOut = barrel_shifter[phase][ShiftReg];

                JK43_J = (!SR3 & INP);
                JK43_K = SR2;
                JK44_J = SR3 & !INP;
                JK44_K = !SR2;

                if (x >= 4) frame_rgba->push(gs_rgb_colors[LatchOut]);

                // TODO: this could be the other way..
                // MUX 34
                /* if (JK43 | JK44) {
                    MuxOut = BSOut;
                } else {
                    MuxOut = LatchOut;
                } */
                if (JK43 | JK44) {
                    MuxOut = LatchOut; // on a change, keep the old value for a time.
                } else {
                    MuxOut = BSOut;
                }

                // POST-Clock
                // set state for next iteration.

                // new values are shifted in from the right (lsb).
                ShiftReg = ((ShiftReg << 1) | INP) & 0xF;

                // cycle FF43                
                if (JK43_J == 0 && JK43_K == 0) {
                    // no change
                } else if (JK43_J == 0 && JK43_K == 1) {
                    JK43 = 0;
                } else if (JK43_J == 1 && JK43_K == 0) {
                    JK43 = 1;
                } else if (JK43_J == 1 && JK43_K == 1) {
                    JK43 = !JK43;
                }

                // cycle FF44
                if (JK44_J == 0 && JK44_K == 0) {
                    // no change
                } else if (JK44_J == 0 && JK44_K == 1) {
                    JK44 = 0;
                } else if (JK44_J == 1 && JK44_K == 0) {
                    JK44 = 1;
                } else if (JK44_J == 1 && JK44_K == 1) {
                    JK44 = !JK44;
                }

                // cycle color reference and 7m inputs to the barrel shifter.
                phase = (phase + 1) % 4;

                // cycle the latch.
                LatchOut = MuxOut;
            }
        }
    }

/** counter > 3, compare all 4 bits.: This provides crisp lo-res with no artifacts except something is wrong at the 
end of the line.. dlgr is passable. I need to see what dlgr looks like on the GS.
need to test against hi-res.
text looks like a** in it. */

    void render(Frame560 *frame_byte, Frame560RGBA *frame_rgba, RGBA_t color /* , uint16_t phaseoffset */)
    {
        uint16_t framewidth = frame_byte->width();

        for (uint16_t y = 0; y < 192; y++)
        {
            frame_byte->set_line(y);
            frame_rgba->set_line(y);

            color_mode_t color_mode = frame_byte->get_color_mode(y); // TODO: also need to know if we're in split mode so we can do lines > 160 correctly.

            uint8_t phase_offset = color_mode.phase_offset;

            if (phase_offset == 0 && shift_enabled) {
                for (int i = 0; i < 7; i++) {
                    frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0x00));
                }
            }

            if (color_mode.colorburst == 1 && color_mode.mixed_mode == 0) {
                // do color burst

                // Process each scanline
                uint8_t phase = phase_offset;
                uint8_t ShiftReg = 0;
                uint8_t BSOut = 0;
                uint8_t LatchOut = 0;
                uint8_t MuxOut = 0;
                uint8_t Counter = 0;

                bool JK43 = 0;
                bool JK43_J = 0;
                bool JK43_K = 0;
                bool JK44 = 0;
                bool JK44_J = 0;
                bool JK44_K = 0;
                bool INP;

                uint8_t bit = frame_byte->peek();

                INP = frame_byte->pull() & 1; // preload the shift register for lookahead
                phase = (phase + 1) % 4;
                ShiftReg = ((ShiftReg << 1) | INP) & 0xF;
                INP = frame_byte->pull() & 1;
                phase = (phase + 1) % 4;
                ShiftReg = ((ShiftReg << 1) | INP) & 0xF;
                INP = frame_byte->pull() & 1;
                phase = (phase + 1) % 4;
                ShiftReg = ((ShiftReg << 1) | INP) & 0xF;
                INP = frame_byte->pull() & 1;
                phase = (phase + 1) % 4;
                ShiftReg = ((ShiftReg << 1) | INP) & 0xF;
                LatchOut = barrel_shifter[3][ShiftReg];

                for (uint16_t x = 4; x < framewidth; x++) {
                    // PRE-Clock
                    //phase = x % 4;
                    uint8_t bit = frame_byte->pull();
                    bool INP = bit & 1;
                    ShiftReg = ((ShiftReg << 1) | INP) & 0xF;

                    // the barrel shifter needs to rotate to the RIGHT based on phase number.
                    BSOut = barrel_shifter[phase][ShiftReg];

                    RGBA_t pixel = gs_rgb_colors[LatchOut];
                    frame_rgba->push(pixel);

                    /* if we only check hi bit, we don't switch between colors when hi bit changes. that's wrong.*/
                    if ((BSOut & 0b1111) != (LatchOut & 0b1111)) {
                        Counter++;
                    }
                    if (Counter > 3) { // this 
                        LatchOut = BSOut;
                        Counter = 0;
                    }

                    // cycle color reference and 7m inputs to the barrel shifter.
                    phase = (phase + 1) % 4;

                    // cycle the latch.
                    //LatchOut = MuxOut;
                }
                // need to run out the rest of the line of pixels
                // pretend 561 to 564
                for (uint16_t x = 1; x <= 4; x++) {
                    RGBA_t pixel = gs_rgb_colors[LatchOut];
                    frame_rgba->push(pixel);
                }
            } else {
                // do mono (white) rendering (or colored with GS colors)
                for (uint16_t x = 0; x < framewidth; x++) {
                    uint8_t bit = frame_byte->pull();
                    RGBA_t pixel = gs_rgb_colors[bit >> 4];
                    frame_rgba->push(pixel);
                }
            }
            if (phase_offset == 1 && shift_enabled) {
                for (int i = 0; i < 7; i++) {
                    frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0x00));
                }
            }
        } 
    }
};

/**
flip:

brown / dark blue
orange and purple
 */
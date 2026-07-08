#pragma once

#include "frame.hpp"
#include "devices/displaypp/RGBA.hpp"


// legacy apple ii mode bitstream. Height is the full scanline count (like
// FrameVSG/FrameBorder below), not just the 192 visible lines: the scan
// generators index this by vcount (0..262) on every HSYNC before a VSYNC
// resets it. Sizing it to 192 let a misaligned scan stream write past the
// end (heap overflow in VideoScanGenerator_Comp::generate_frame).
using Frame560 = Frame<uint8_t, 263, 560>;
// legacy apple II mode texture
using Frame560RGBA = Frame<RGBA_t, 192, 567, SDLTextureStorage>;
// border texture
using FrameBorder = Frame<RGBA_t, 263, 53, SDLTextureStorage>;
// shr texture
using Frame640 = Frame<RGBA_t, 200, 640, SDLTextureStorage>;

// new omnibus buffer
using FrameVSG = Frame<RGBA_t, 263, 910, SDLTextureStorage>;

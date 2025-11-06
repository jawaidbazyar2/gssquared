#pragma once

#include "frame.hpp"
#include "devices/displaypp/RGBA.hpp"


// legacy apple ii mode bitstream
using Frame560 = Frame<uint8_t, 192, 560>;
// legacy apple II mode texture
using Frame560RGBA = Frame<RGBA_t, 192, 567, SDLTextureStorage>;
// border texture
using FrameBorder = Frame<RGBA_t, 263, 53, SDLTextureStorage>;
// shr texture
using Frame640 = Frame<RGBA_t, 200, 640, SDLTextureStorage>;
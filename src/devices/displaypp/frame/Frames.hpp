#pragma once

#include "frame.hpp"
#include "devices/displaypp/RGBA.hpp"


// legacy apple ii mode bitstream
using Frame560 = Frame<uint8_t, 262, 560>;
// legacy apple II mode texture
using Frame560RGBA = Frame<RGBA_t, 262, 580>;
// border texture
using FrameBorder = Frame<RGBA_t, 263, 53>;
// shr texture
using Frame640 = Frame<RGBA_t, 200, 640>;
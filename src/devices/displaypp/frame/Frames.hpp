#pragma once

#include "frame.hpp"
#include "devices/displaypp/RGBA.hpp"


using Frame560 = Frame<uint8_t, 262, 580>;
using Frame560RGBA = Frame<RGBA_t, 262, 580>;
using FrameBorder = Frame<RGBA_t, 262, 13>;
using Frame640 = Frame<RGBA_t, 200, 640>;
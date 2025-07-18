#pragma once

#include "frame.hpp"
#include "devices/displaypp/RGBA.hpp"
#include "devices/displaypp/Scan.hpp"

//typedef uint32_t RGBA_t;

using Frame560 = Frame<uint8_t, 192, 580>;
using Frame560RGBA = Frame<RGBA_t, 192, 580>;
using FrameScan560 = Frame<Scan_t, 192, 40>; // 40 cycles per line.

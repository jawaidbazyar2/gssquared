#pragma once

#include "Render.hpp"

// Leaves the generator's FrameBits for a GPU decoder; draws nothing on the CPU.
class PackedOnly : public Render {
public:
    PackedOnly() : Render(false) {}
    virtual void render_lines(const FrameBits &, RGBA_t *, uint32_t) override {}
};

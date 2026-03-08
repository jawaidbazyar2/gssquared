#pragma once

#include "Container.hpp"

class FadeContainer_t : public Container_t {
protected:
    int fade_frames = 512;
    int fade_steps = 3;
    int frameCount = 0;

public:
    FadeContainer_t(UIContext *ctx, const Style_t& initial_style = Style_t(), int fade_frames = 512);
    FadeContainer_t(UIContext *ctx) : Container_t(ctx, Style_t()), fade_frames(512), fade_steps(3) {}
    virtual bool handle_mouse_event(const SDL_Event& event) override;
    virtual void render() override;
    virtual void reset() { frameCount = 0; }
};
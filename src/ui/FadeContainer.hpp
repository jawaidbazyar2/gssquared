#pragma once

#include "Container.hpp"

class FadeContainer_t : public Container_t {
protected:
    int fade_frames = 0;
    int fade_steps = 3;
    int frameCount = 0;

public:
    FadeContainer_t(SDL_Renderer *renderer, size_t max_tiles, const Style_t& initial_style, int fade_frames);
    FadeContainer_t(SDL_Renderer *renderer, size_t max_tiles);
    virtual void handle_mouse_event(const SDL_Event& event) override;
    void render() override;
    void reset() { frameCount = 0; }
};
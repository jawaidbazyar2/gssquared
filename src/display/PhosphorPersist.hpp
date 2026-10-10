#pragma once

#include <SDL3/SDL.h>

// Monochrome phosphor persistence: a 910x263 history texture that is faded by
// k each frame, then max-blended with the newly decoded frame. All work is
// render-target draws; nothing is read back to the CPU.
class PhosphorPersist {
public:
    static constexpr int WIDTH = 910;
    static constexpr int HEIGHT = 263;

    bool init(SDL_Renderer *renderer);
    void shutdown();

    // frame is a WIDTH x HEIGHT texture; k is the fraction of history brightness
    // kept. Restores the render target and draw state; returns the history texture.
    SDL_Texture *apply(SDL_Texture *frame, float k);

    // The next apply() starts from black.
    void invalidate() { valid = false; }

private:
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *history = nullptr;
    SDL_BlendMode max_blend = SDL_BLENDMODE_INVALID;
    SDL_BlendMode sub_blend = SDL_BLENDMODE_INVALID;
    bool valid = false;
};

#include "FadeContainer.hpp"

FadeContainer_t::FadeContainer_t(SDL_Renderer *renderer, size_t max_tiles, const Style_t& initial_style, int fade_frames) : Container_t(renderer, max_tiles, initial_style) {
    this->fade_frames = fade_frames;
    fade_steps = 3;
}

FadeContainer_t::FadeContainer_t(SDL_Renderer *renderer, size_t max_tiles) : Container_t(renderer, max_tiles) {
    fade_frames = 512;
    fade_steps = 3;
}

void FadeContainer_t::handle_mouse_event(const SDL_Event& event) {
    if (fade_frames == 0) return;
    Container_t::handle_mouse_event(event);
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        frameCount = fade_frames;
    }
}

void FadeContainer_t::render() {
    if (frameCount > 0) {
        frameCount -= fade_steps;
        if (frameCount < 0) frameCount = 0;
        int opc = frameCount > 255 ? 255 : frameCount;
        // for each button in the container, set the opacity to opc.
        for (size_t i = 0; i < tile_count; i++) {
            if (tiles[i]) {
                tiles[i]->set_opacity(opc);
            }
        }
        Container_t::render();
    }
}
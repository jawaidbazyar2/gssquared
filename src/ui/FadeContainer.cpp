#include "FadeContainer.hpp"

FadeContainer_t::FadeContainer_t(UIContext *ctx, const Style_t& initial_style, int fade_frames) : Container_t(ctx, initial_style) {
    this->fade_frames = fade_frames;
    fade_steps = 3;
}

bool FadeContainer_t::handle_mouse_event(const SDL_Event& event) {
    if (fade_frames == 0) return(false);
    Container_t::handle_mouse_event(event);
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        // if mouse is inside container.. 
        if (event.motion.x >= tp.x && event.motion.x <= tp.x + tp.w && event.motion.y >= tp.y && event.motion.y <= tp.y + tp.h) {
            frameCount = fade_frames;
            fade_steps = 0;
        } else {
            if (frameCount > 255) { // we were inside container and moved outside..
                fade_steps = 3;
            }
        }
    }
    return(false);
}

void FadeContainer_t::render() {
    if (frameCount > 0) {
        frameCount -= fade_steps;
        if (frameCount < 0) frameCount = 0;
        int opc = frameCount > 255 ? 255 : frameCount;
        // for each button in the container, set the opacity to opc.
        for (size_t i = 0; i < tiles.size(); i++) {
            if (tiles[i]) {
                tiles[i]->set_opacity(opc);
            }
        }
        Container_t::render();
    }
}
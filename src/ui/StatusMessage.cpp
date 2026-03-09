#include "Container.hpp"
#include "StatusMessage.hpp"

StatusMessage_t::StatusMessage_t(UIContext *ctx) : Container_t(ctx, Style_t()) {
    headsUpMessageCount = 0;
}

void StatusMessage_t::trigger(const std::string& message) {
    headsUpMessageText = message;
    headsUpMessageCount = 512;
}

void StatusMessage_t::layout() {
    int window_width, window_height;
    SDL_GetWindowSize(ctx->window, &window_width, &window_height);
    msg_x = window_width / 2;
}

void StatusMessage_t::update() {
    if (!headsUpMessageCount) return;
    headsUpMessageCount -= 3;
    if (headsUpMessageCount < 0) headsUpMessageCount = 0;
}

void StatusMessage_t::render() {
    if (!visible) return;
    if (!headsUpMessageCount) return;

    layout();

    // should probably assume 1:1 scale in all the things, only save/restore on the whole OSD entry / exit.
    float ox,oy;
    SDL_GetRenderScale(ctx->renderer, &ox, &oy);

    // Display this regardless of OSD state.
    SDL_SetRenderScale(ctx->renderer, 1,1); // TODO: calculate these based on window size
    if (headsUpMessageCount) { // set it to 512 for instance to sit at full opacity for 4 seconds then fade out over 4ish seconds.    
        SDL_SetRenderTarget(ctx->renderer, nullptr);
        int opacity = headsUpMessageCount < 255 ? headsUpMessageCount : 255;
        ctx->text_render->set_color(0xFF, 0xFF, 0xFF, opacity);
        ctx->text_render->render(headsUpMessageText, msg_x, 30, TEXT_ALIGN_CENTER);

    }
    SDL_SetRenderScale(ctx->renderer, ox,oy);
    
}

bool StatusMessage_t::handle_mouse_event(const SDL_Event& event) {
    return false;
}
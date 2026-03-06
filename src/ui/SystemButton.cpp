#include <SDL3/SDL.h>

#include "SystemButton.hpp"

void SystemButton::render() {
    Button_t::render();

    // draw the system description
    if (is_hovering) {
        ctx->text_render->set_color(0xFF, 0xFF, 0xFF, 0xFF);
        ctx->text_render->render(system_config->description, (1120.0f+60.0f)/2.0f, 800, TEXT_ALIGN_CENTER);
    }
}
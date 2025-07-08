#include <SDL3/SDL.h>

#include "SystemButton.hpp"

void SystemButton::render(SDL_Renderer *renderer) {
    Button_t::render(renderer);

    // draw the system description
    if (is_hovering) {
        text_render->set_color(0xFF, 0xFF, 0xFF, 0xFF);
        text_render->render(system_config->description, (1120.0f+60.0f)/2.0f, 800, TEXT_ALIGN_CENTER);
    }
}
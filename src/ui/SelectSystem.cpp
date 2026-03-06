#include "SelectSystem.hpp"
#include "Container.hpp"
#include "systemconfig.hpp"
#include "Button.hpp"
#include "Style.hpp"
#include "util/TextRenderer.hpp"
#include "MainAtlas.hpp"
#include "SystemButton.hpp"

SelectSystem::SelectSystem(video_system_t *vs, AssetAtlas_t *aa)
    : vs(vs), aa(aa) {

// create container with tiles for each system.
// be cheap right now and do a text button.
Style_t CS;
    CS.background_color = 0x000000FF;
    CS.border_width = 0;
    CS.padding = 25;

    Style_t BS;
    BS.background_color = 0xBFBB98FF;
    BS.border_color = 0xBFBB9840;
    BS.hover_color = 0x008C4AFF;
    BS.padding = 15;
    BS.border_width = 1;
    BS.text_color = 0xFFFFFFFF;

    int num_configs = NUM_SYSTEM_CONFIGS;

    text_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 24);
    ui_ctx = { vs->renderer, text_renderer, nullptr, aa };

    container = new Container_t(&ui_ctx, num_configs, CS); 

    container->set_tile_size(1024, 768);
    SDL_GetWindowSize(vs->window, &window_width, &window_height);
    container->set_position((window_width - 1024) / 2, (window_height - 768) / 2);

    selected_system = SELECT_PENDING;

    // add a text button for each system.
    for (int i = 0; i < num_configs; i++) {
        SystemButton *button = new SystemButton(&ui_ctx, &BuiltinSystemConfigs[i], BS);
        button->set_tile_size(200, 200);
        button->position_content(CP_CENTER, CP_CENTER);

        button->on_click([this,i](const SDL_Event& event) -> bool {
            selected_system = i;
            return true;
        });
        container->add_tile(button, i);
    }

    container->layout();

    updated = true;
}

SelectSystem::~SelectSystem() {
    delete container;
    delete text_renderer;
}

bool SelectSystem::event(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT) {
        selected_system = SELECT_QUIT;
        return true;
    }
    container->handle_mouse_event(event);
    return (selected_system >= 0);
}

int SelectSystem::get_selected_system() {
    return selected_system;
}

bool SelectSystem::update() {
    return updated;
}

void SelectSystem::render() {
    if (updated) {
        float scale_x, scale_y;
        SDL_GetRenderScale(vs->renderer, &scale_x, &scale_y);
        SDL_SetRenderScale(vs->renderer, 1, 1);
        text_renderer->set_color(255,255,255,255);
        text_renderer->render("Choose your retro experience", (window_width / 2), 50, TEXT_ALIGN_CENTER);

        container->render();
        SDL_SetRenderScale(vs->renderer, scale_x, scale_y);
        //updated = false;
    }
}

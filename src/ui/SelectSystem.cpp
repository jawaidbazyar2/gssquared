#include "SelectSystem.hpp"
#include "Container.hpp"
#include "systemconfig.hpp"
#include "Style.hpp"
#include "util/TextRenderer.hpp"
#include "AssetAtlas.hpp"
#include "SystemButton.hpp"

SelectSystem::SelectSystem(video_system_t *vs, AssetAtlas_t *aa)
    : vs(vs), aa(aa) {

// create container with tiles for each system.
// be cheap right now and do a text button.
    Style_t CS = {
        .background_color = 0x000000FF,
        .padding = 25,
        .border_width = 0,
    };

    Style_t BS = {
        .background_color = 0xBFBB98FF,
        .border_color = 0xBFBB9840,
        .hover_color = 0x008C4AFF,
        .padding = 15,
        .border_width = 1,
        .text_color = 0xFFFFFFFF
    };

    int num_configs = NUM_SYSTEM_CONFIGS;

    text_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 24);
    ui_ctx = { vs->renderer, vs->window, text_renderer, nullptr, aa };

    container = new Container_t(&ui_ctx, CS);

    container->size(1024, 768);
    SDL_GetWindowSize(vs->window, &window_width, &window_height);
    container->set_position((window_width - 1024) / 2, (window_height - 768) / 2);

    selected_system = SELECT_PENDING;

    // add a text button for each system.
    for (int i = 0; i < num_configs; i++) {
        platform_info* platform = get_platform(BuiltinSystemConfigs[i].platform_id);
        SystemButton *button = new SystemButton(&ui_ctx, &BuiltinSystemConfigs[i], platform->image_id, BS);
        button->size(200, 200);
        button->position_content(CP_CENTER, CP_CENTER);
        button->style.background_color = platform->case_color;

        button->on_click([this,i](const SDL_Event& event) -> bool {
            selected_system = i;
            return true;
        });
        container->add(button);
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
        ui_ctx.color(0x000000FF); // set back to 0. Someone isn't correctly setting color elsewhere..
    }
}

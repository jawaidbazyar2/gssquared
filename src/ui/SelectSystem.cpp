#include "SelectSystem.hpp"
#include "Container.hpp"
#include "systemconfig.hpp"
#include "Button.hpp"
#include "Style.hpp"
#include "util/TextRenderer.hpp"
#include "MainAtlas.hpp"

SelectSystem::SelectSystem(video_system_t *vs, AssetAtlas_t *aa)
    : vs(vs), aa(aa) {

// create container with tiles for each system.
// be cheap right now and do a text button.
Style_t CS;
    CS.background_color = 0x000000FF;
    CS.border_width = 0;
    CS.padding = 50;

    Style_t BS;
    BS.background_color = 0xBFBB98FF;
    BS.border_color = 0xBFBB9840;
    BS.hover_color = 0x008C4AFF;
    BS.padding = 15;
    BS.border_width = 1;
    BS.text_color = 0xFFFFFFFF;

    container = new Container_t(vs->renderer, 6, CS); 

    container->set_tile_size(1024, 768);
    SDL_GetWindowSize(vs->window, &window_width, &window_height);
    container->set_position((window_width - 1024) / 2, (window_height - 768) / 2);

    text_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 24);
    selected_system = -1;

    // add a text button for each system.
    for (int i = 0; i < 4; i++) {
        Button_t *button = new Button_t(aa, BuiltinSystemConfigs[i].image_id, BS, 0);
        button->set_tile_size(200, 200);
        button->position_content(CP_CENTER, CP_CENTER);
        button->set_text_renderer(text_renderer);
       /*  if (i == 2) {
            button->set_opacity(0.5);
            button->set_background_color(0xC0C0C0FF);
            button->set_hover_color(0xC0C0C0FF);
        } else { */
            button->set_click_callback([this,i](const SDL_Event& event) -> bool {
                selected_system = i;
                return true;
            });
     /*    } */
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
        selected_system = -1;
        return true;
    }
    container->handle_mouse_event(event);
    return (selected_system != -1);
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

int SelectSystem::select() {

    bool selected = false;
    while (!selected) {
        SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
        vs->clear();

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if (this->event(event)) {
                selected = true;
            }
        }
        if (update()) {
            render();
            vs->present();
        }
        SDL_Delay(16);
    }
    return selected_system;
}
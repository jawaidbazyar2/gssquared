#include "gs2.hpp"

#include "ui/Style.hpp"
#include "ui/Tile.hpp"
#include "ui/Button.hpp"

gs2_app_t gs2_app_values;

int main(int argc, char **argv) {
    printf("Starting UI test...\n");

    /* gs2_app_values.base_path = "./";
    gs2_app_values.pref_path = gs2_app_values.base_path;
    gs2_app_values.console_mode = false; */

    // Init SDL with 640x480 window.
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("UI Test", 640, 480, SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return 1;
    }

    Style_t style;
    style.background_color = 0xFFFFFFFF;
    style.border_color     = 0x0000FFFF;
    style.hover_color      = 0xFFFF00FF;
    style.text_color       = 0x000000FF;
    style.padding = 10;
    style.border_width = 3;

    // load tt font
    TextRenderer *tr = new TextRenderer(renderer, "assets/fonts/OpenSans-Regular.ttf", 20);
    if (!tr) {
        printf("Failed to load font\n");
        return 1;
    }

    // TODO: feel like changing these names.
    // set_position; set_size
    // Create a Tile and render it.
    Tile_t tile(style);
    tile.set_tile_position(100, 100);
    tile.set_tile_size(100, 50);

    Button_t button("Button1", tr, style);
    button.set_tile_position(220, 100);
    button.set_tile_size(100, 50);
    //button.set_text_renderer(tr);
    //button.position_content(CP_CENTER, CP_CENTER);
    button.print();

    Button_t button2("Button2", nullptr, style);
    button2.set_tile_position(340, 100);
    button2.set_tile_size(100, 50);
    button2.print();


    // Main loop.
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {   
                return 0;
            } else {
                tile.handle_mouse_event(event);
                button.handle_mouse_event(event);
                button2.handle_mouse_event(event);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        tile.render(renderer);
        button.render(renderer);
        button2.render(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return 0;
}
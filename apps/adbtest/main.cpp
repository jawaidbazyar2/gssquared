#include <SDL3/SDL.h>
#include "devices/adb/KeyGloo.hpp"

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("DisplayPP Test Harness", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return 1;
    }

    KeyGloo *kg = new KeyGloo();

    while (1) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exit(0);
            }
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                printf("Key event: [%d] %08X %08X\n", event.type, event.key.key, event.key.mod);

                kg->process_event(event);
                printf("--------------------------------\n");
                printf("Key latch: %08X\n", kg->read_key_latch());
                printf("Mod latch: %08X\n", kg->read_mod_latch());
    
            }
    
        }
    }
    return 0;
}
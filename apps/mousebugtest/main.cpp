#include <SDL3/SDL.h>


int main(int argc, char **argv) {
 bool capture = false;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Mouse Bug Test", 640, 480, SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return 1;
    }

    while (1) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return 0;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                printf("Key down: %d\n", event.key.key);
                if (event.key.key == SDLK_ESCAPE) {
                    capture = !capture;
                    if (!SDL_SetWindowRelativeMouseMode(window, capture)) {
                        printf("Failed to set relative mouse mode: %s\n", SDL_GetError());
                    }
                    if (!SDL_SetWindowKeyboardGrab(window, capture)) {
                        printf("Failed to set keyboard grab: %s\n", SDL_GetError());
                    }
                }
            }
        }

        SDL_Delay(16);
    }
}
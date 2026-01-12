#include "SDL3/SDL_events.h"
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "menu.h"

#include "devices/adb/ADB_Micro.hpp"

struct MyAppState {
    KeyGloo *kg;
    SDL_Window *window;
    SDL_Renderer *renderer;

    int mouse_x_loc = 0, mouse_y_loc = 0;
};

const char *cmd_names[] = {
    "ReadKbd",
    "Flush",
    "Reset",
};
const int num_cmds = sizeof(cmd_names)/sizeof(cmd_names[0]);

void draw_clear(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(renderer);
}

void draw_controls(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    for (int i = 0; i < num_cmds; i++) {
        SDL_RenderDebugText(renderer, 10 + i * 8 * 8, 10, cmd_names[i]);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("DisplayPP Test Harness", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window\n");
        return SDL_APP_FAILURE;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowKeyboardGrab(window, true);

    KeyGloo *kg = new KeyGloo();
    MyAppState *as = new MyAppState();
    as->kg = kg;
    as->window = window;
    as->renderer = renderer;

    draw_clear(renderer);
    draw_controls(renderer);
    SDL_RenderPresent(renderer);

    initMenu(window);

    *appstate = as;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    MyAppState *as = (MyAppState *)appstate;
    KeyGloo *kg = as->kg;
    SDL_Event event_copy = *event;

    kg->process_event(event_copy);

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
        //printf("ignoring quit event\n");
    }
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        printf("< Key event: [%d] %08X %08X\n", event->type, event->key.key, event->key.mod);

        printf("--------------------------------\n");
        printf("< Key latch: %08X\n", kg->read_key_latch());
        printf("< Mod latch: %08X\n", kg->read_mod_latch());
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        float x = event->motion.x;
        float y = event->motion.y;
         if (y < 20.0) {
            return SDL_APP_CONTINUE;
         }
        uint8_t mousex = kg->read_mouse_data();
        uint8_t mousey = kg->read_mouse_data();
        printf("Mouse: x: %02X, y: %02X\n", mousex, mousey);
        char buf[256];

        int8_t xdelta = (mousex & 0x3F) * ((mousex & 0x40) ? 1 : -1);
        int8_t ydelta = (mousey & 0x3F) * ((mousey & 0x40) ? 1 : -1);

        as->mouse_x_loc += xdelta;
        as->mouse_y_loc += ydelta;

        snprintf(buf, sizeof(buf), "Mouse: x: %05d, y: %05d\n", as->mouse_x_loc, as->mouse_y_loc);
        SDL_SetRenderDrawColor(as->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDebugText(as->renderer, 10, 40, buf);
        snprintf(buf, sizeof(buf), "Last Mtn: x: %05d, y: %05d\n", xdelta, ydelta    );
        SDL_RenderDebugText(as->renderer, 10, 60, buf);
        draw_controls(as->renderer);
        SDL_RenderPresent(as->renderer);
        printf("Mouse Delta: x: %05d, y: %05d\n", xdelta, ydelta);
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        float x = event->button.x;
        float y = event->button.y;
        if (y  < 20) { // in command/controls area
            if (x < 10 + num_cmds * 8 * 8) {
                int cmd_index = (x - 10.0) / (8 * 8);
                printf("Command: %s\n", cmd_names[cmd_index]);
                switch (cmd_index) {
                    case 0:
                        {
                            printf("< Mouse button down: %d\n", event->button.button);
                            uint8_t k = kg->read_key_latch(); // lda c000
                            uint8_t m = kg->read_mod_latch(); // lda c0?? - get modifiers
                            kg->write_key_strobe(0); // sta c010 - clear key latch
                            printf("< Key read: %02X, Mod read: %02X\n", k, m);
                            printf("--------------------------------\n");
                        }
                        break;
                    case 1: 
                        kg->flush();
                        break;
                    case 2:
                        kg->reset();
                        break;
                }

            }
        } else { // in display area, so act like normal mouse event

        }
        return SDL_APP_CONTINUE;

    }
    
    
    /* else {
        printf("Unknown event: [%d] %08X %08X\n", event->type, event->key.key, event->key.mod);
        return SDL_APP_CONTINUE;
    } */

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    // Update game logic, clear screen, draw, and present
    // Access app state via the appstate pointer
    // Returning SDL_APP_FAILURE will terminate the program
    // Returning SDL_APP_CONTINUE continues the app loop
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result; // unused
    // Clean up resources (destroy textures, renderer, window, free memory, etc.)
    // Access app state via the appstate pointer
    MyAppState *as = (MyAppState *)appstate;
    KeyGloo *kg = as->kg;
    //delete kg;
    delete as;
}


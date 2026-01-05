#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include "menu.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window *window = SDL_CreateWindow("SDL3 menu example", 1024, 768,
					      SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		SDL_Log("Window could not be created: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	initMenu(window);

	*appstate = window;

	return SDL_APP_CONTINUE;
	(void)argc;
	(void)argv;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	return SDL_APP_CONTINUE;
	(void)appstate;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}

	return SDL_APP_CONTINUE;
	(void)appstate;
}

void SDL_AppQuit(void *appstate) {
	SDL_DestroyWindow(static_cast<SDL_Window *>(appstate));
	SDL_Quit();
}


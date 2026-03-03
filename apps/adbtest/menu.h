#ifndef MENU_H
#define MENU_H
#pragma once

#include <SDL3/SDL.h>

typedef SDL_AppResult (*MenuIterateCallback)(void *appstate);

void initMenu(SDL_Window* window);
void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate);

#endif // MENU_H

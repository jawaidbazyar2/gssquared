#ifdef _WIN32

#include <SDL3/SDL.h>
#include "platform-specific/menu.h"

void initMenu(SDL_Window *window) {
    (void)window;
}

void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate) {
    (void)callback;
    (void)appstate;
}
#endif

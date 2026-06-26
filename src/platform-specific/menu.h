#pragma once

#include <SDL3/SDL.h>

enum MenuEventCode {
	MENU_MACHINE_RESET = 1,
	MENU_MACHINE_RESTART,
	MENU_MACHINE_PAUSE_RESUME,
	MENU_MACHINE_CAPTURE_MOUSE,

	MENU_SPEED_1_0 = 100,
	MENU_SPEED_2_8,
	MENU_SPEED_7_1,
	MENU_SPEED_14_3,

	MENU_MONITOR_COMPOSITE = 200,
	MENU_MONITOR_GS_RGB,
	MENU_MONITOR_MONO_GREEN,
	MENU_MONITOR_MONO_AMBER,
	MENU_MONITOR_MONO_WHITE,

	MENU_DISPLAY_FULLSCREEN = 300,
	
	MENU_EDIT_COPY_SCREEN = 400,
	MENU_EDIT_PASTE_TEXT,

	MENU_OPEN_DEBUG_WINDOW = 501,

	MENU_DISK_TOGGLE = 600,  // user.data1 = storage_key_t cast to void*

	MENU_CONTROLLER_GAMEPAD = 700,
	MENU_CONTROLLER_MOUSE,
	MENU_CONTROLLER_JOYPORT,

};

typedef SDL_AppResult (*MenuIterateCallback)(void *appstate);

void initMenu(SDL_Window* window);
void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate);

// Called from SDL_AppEvent; returns true if the event was consumed by the menu.
// Linux and Emscripten use the Dear ImGui menu (real implementations below);
// other platforms get no-ops.
#if defined(__linux__) || defined(__EMSCRIPTEN__)
bool handleMenuEvent(const SDL_Event *event);
void renderMenuOverlay(SDL_Renderer *renderer, int win_w, int win_h);
void pumpMenuEvents();
#else
inline bool handleMenuEvent(const SDL_Event * /*event*/) { return false; }
inline void renderMenuOverlay(SDL_Renderer * /*renderer*/, int /*win_w*/, int /*win_h*/) {}
inline void pumpMenuEvents() {}
#endif

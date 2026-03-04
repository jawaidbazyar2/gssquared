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
};

typedef SDL_AppResult (*MenuIterateCallback)(void *appstate);

void initMenu(SDL_Window* window);
void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate);

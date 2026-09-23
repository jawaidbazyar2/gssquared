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
	MENU_HUD_STATS,
	MENU_HUD_DRIVES,
	MENU_DISPLAY_SS_TEXT,
	MENU_DISPLAY_CRT_SHADER,

	MENU_EDIT_COPY_SCREEN = 400,
	MENU_EDIT_PASTE_TEXT,

	MENU_OPEN_DEBUG_WINDOW = 501,

	MENU_DISK_TOGGLE = 600,  // user.data1 = storage_key_t cast to void*

	MENU_OPEN_CONFIG = 650,
	MENU_FILE_SAVE_SCREENSHOT,
	MENU_FILE_MOUNT_DRIVERS,
	MENU_FILE_NEW_DISK_525_UNFMT,
	MENU_FILE_NEW_DISK_525_DOS33,
	MENU_FILE_NEW_DISK_525_PRODOS,
	MENU_FILE_NEW_DISK_35_PRODOS,
	MENU_FILE_NEW_DISK_32M_HD,
	MENU_FILE_NEW_DISK_32M_PRODOS,

	MENU_CONTROLLER_GAMEPAD = 700,
	MENU_CONTROLLER_MOUSE,
	MENU_CONTROLLER_JOYPORT,
	MENU_CONTROLLER_ABSENT_DISCONNECTED,
	MENU_CONTROLLER_JOYPORT_LEFT,
	MENU_CONTROLLER_JOYPORT_CENTER,
	MENU_CONTROLLER_JOYPORT_RIGHT,

};

typedef SDL_AppResult (*MenuIterateCallback)(void *appstate);

void initMenu(SDL_Window* window);
void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate);

// Called from SDL_AppEvent; returns true if the event was consumed by the menu.
// Linux and Emscripten use the Dear ImGui menu (real implementations below);
// other platforms get no-ops.
#if defined(__linux__) || defined(__EMSCRIPTEN__)
bool handleMenuEvent(const SDL_Event *event);
// Draw the ImGui menu immediately above `content` (window points) when there
// is room, otherwise flush with the dest top. `content` is the letterboxed
// display dest; passing NULL uses the full window. SelectSystem / EditSystem
// must leave `menuBarHeight()` of design-space room under that strip.
void renderMenuOverlay(SDL_Renderer *renderer, const SDL_FRect *content);
// One ImGui menu frame, no draw. The web build uses this from the DOM click
// handler so File → Drives can open a picker before Safari's user gesture ends.
void advanceMenuFrameForGesture(SDL_Renderer *renderer, const SDL_FRect *content);
void pumpMenuEvents();
/** True when ImGui is capturing input (menu bar hover / open menu) and needs another frame. */
bool menuNeedsFrame();
// ImGui bar height in window/design points (font + frame padding). 0 on
// platforms that use a native menu.
inline float menuBarHeight() { return 28.0f; }
#else
inline bool handleMenuEvent(const SDL_Event * /*event*/) { return false; }
inline void renderMenuOverlay(SDL_Renderer * /*renderer*/, const SDL_FRect * /*content*/) {}
inline void pumpMenuEvents() {}
inline bool menuNeedsFrame() { return false; }
inline float menuBarHeight() { return 0.0f; }
#endif

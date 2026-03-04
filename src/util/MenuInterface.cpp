#include "util/MenuInterface.h"
#include "gs2.hpp"
#include "NClock.hpp"
#include "videosystem.hpp"
#include "platform-specific/menu.h"

static void pushMenuEvent(Sint32 code) {
	SDL_Event event = {};
	event.type = gs2_app_values.menu_event_type;
	event.user.code = code;
	SDL_PushEvent(&event);
}

void MenuInterface::machineReset()       { pushMenuEvent(MENU_MACHINE_RESET); }
void MenuInterface::machineRestart()     { pushMenuEvent(MENU_MACHINE_RESTART); }
void MenuInterface::machinePauseResume() { pushMenuEvent(MENU_MACHINE_PAUSE_RESUME); }
void MenuInterface::machineCaptureMouse(){ pushMenuEvent(MENU_MACHINE_CAPTURE_MOUSE); }

void MenuInterface::setSpeed(int speed_id) {
	switch (speed_id) {
		case SPEED_1_0:  pushMenuEvent(MENU_SPEED_1_0); break;
		case SPEED_2_8:  pushMenuEvent(MENU_SPEED_2_8); break;
		case SPEED_7_1:  pushMenuEvent(MENU_SPEED_7_1); break;
		case SPEED_14_3: pushMenuEvent(MENU_SPEED_14_3); break;
	}
}

void MenuInterface::setMonitor(int monitor_id) {
	pushMenuEvent(monitor_id);
}

void MenuInterface::toggleSleepMode() {
	gs2_app_values.sleep_mode = !gs2_app_values.sleep_mode;
}

int MenuInterface::getCurrentSpeed() {
	if (!gs2_app_values.clock) return -1;
	return (int)gs2_app_values.clock->get_clock_mode();
}

int MenuInterface::getCurrentMonitor() {
	video_system_t *vs = gs2_app_values.video_system;
	if (!vs) return -1;

	if (vs->display_color_engine == DM_ENGINE_NTSC) return MONITOR_COMPOSITE;
	if (vs->display_color_engine == DM_ENGINE_RGB)  return MONITOR_GS_RGB;

	switch (vs->display_mono_color) {
		case DM_MONO_GREEN: return MONITOR_MONO_GREEN;
		case DM_MONO_AMBER: return MONITOR_MONO_AMBER;
		default:            return MONITOR_MONO_WHITE;
	}
}

bool MenuInterface::getSleepMode() {
	return gs2_app_values.sleep_mode;
}

static MenuInterface sInstance;

MenuInterface *getMenuInterface() {
	return &sInstance;
}

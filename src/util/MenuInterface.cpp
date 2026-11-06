#include "util/MenuInterface.h"
#include "util/Gs2Pack.hpp"
#include "gs2.hpp"
#include "NClock.hpp"
#include "videosystem.hpp"
#include "platform-specific/menu.h"
#include "debugger/debugwindow.hpp"
#include "computer.hpp"
#include "util/AudioSystem.hpp"
#include "util/mount.hpp"
#include "util/SystemSettings.hpp"
#include "util/applekeys.hpp"
#include "devices/game/gamecontroller.hpp"
#include "Module_ID.hpp"

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
	if (monitorAvailable(monitor_id)) pushMenuEvent(monitor_id);
}

// Second Sight shares the monitor menu with the built-in display. Disable
// only choices whose required renderer is unavailable; monitor_id is a menu ID.
bool MenuInterface::monitorAvailable(int monitor_id) {
    return computer_ && computer_->video_system &&
        (monitor_id != MONITOR_GS_COMPOSITE || computer_->video_system->gscomposite_allowed);
}

// Second Sight firmware senses the selected cable: true means AppleColor RGB.
// Store the choice for the next frame hook; firmware applies it at mode setup.
void MenuInterface::setSecondSightMonitor(bool rgb) {
	SystemSettings::instance().set_ss_rgb_monitor(rgb);
}

// Select the visible Second Sight/IIgs monitor arrangement without resetting chips.
void MenuInterface::setSecondSightView(int view) {
	SystemSettings::instance().set_ss_view(view);
}

// Store the next power-on firmware choice through the shared settings path.
void MenuInterface::setSecondSightFirmware(int firmware) {
	SystemSettings::instance().set_ss_firmware(firmware);
}

// Store the next power-on Lattice choice independently of firmware.
void MenuInterface::setSecondSightLattice(bool v2) {
	SystemSettings::instance().set_ss_lattice_v2(v2);
}

// Return the persisted option for menu checkmarks, which may await Restart.
int MenuInterface::getSecondSightFirmware() {
	return SystemSettings::instance().ss_firmware();
}

// Return the persisted chip option for menu checkmarks, which may await Restart.
bool MenuInterface::getSecondSightLattice() {
	return SystemSettings::instance().ss_lattice_v2();
}

// Store the next power-on INT0 pull-up choice.
void MenuInterface::setSecondSightInt0Pullup(bool fitted) {
	SystemSettings::instance().set_ss_int0_pullup(fitted);
}

// Return the persisted INT0 pull-up option, which may await Restart.
bool MenuInterface::getSecondSightInt0Pullup() {
	return SystemSettings::instance().ss_int0_pullup();
}

// Enable Second Sight controls only while its independent display is installed.
bool MenuInterface::hasSecondSightZ180() {
	return computer_ && computer_->video_system && bool(computer_->video_system->additional_display);
}

// Supply the Second Sight cable checkmark from the saved monitor preference.
bool MenuInterface::getSecondSightRGBMonitor() {
	return SystemSettings::instance().ss_rgb_monitor();
}

// Supply the Second Sight view checkmark: IIgs=0, card=1, both=2.
int MenuInterface::getSecondSightView() {
	return SystemSettings::instance().ss_view();
}

void MenuInterface::openDebugWindow() { pushMenuEvent(MENU_OPEN_DEBUG_WINDOW); }

void MenuInterface::diskToggle(storage_key_t key) {
	SDL_Event event = {};
	event.type = gs2_app_values.menu_event_type;
	event.user.code = MENU_DISK_TOGGLE;
	event.user.data1 = (void*)(uintptr_t)key.key;
	SDL_PushEvent(&event);
}

void MenuInterface::openSystemConfig() { pushMenuEvent(MENU_OPEN_CONFIG); }

void MenuInterface::displayFullScreen() { pushMenuEvent(MENU_DISPLAY_FULLSCREEN); }
void MenuInterface::editCopyScreen()   { pushMenuEvent(MENU_EDIT_COPY_SCREEN); }
void MenuInterface::editPasteText()    { pushMenuEvent(MENU_EDIT_PASTE_TEXT); }
void MenuInterface::fileSaveScreenshot() { pushMenuEvent(MENU_FILE_SAVE_SCREENSHOT); }
void MenuInterface::fileSaveCollection() { pushMenuEvent(MENU_FILE_SAVE_COLLECTION); }

bool MenuInterface::canSaveCollection() const {
	const gs2pack::Session *session = gs2pack::active();
	return session != nullptr && !session->save_token.empty() && !session->save_url.empty();
}
void MenuInterface::toggleMountDrivers() { pushMenuEvent(MENU_FILE_MOUNT_DRIVERS); }
void MenuInterface::newDiskImage(int menu_code) { pushMenuEvent(menu_code); }

void MenuInterface::toggleSleepMode() {
	gs2_app_values.sleep_mode = !gs2_app_values.sleep_mode;
}

void MenuInterface::togglePauseWhenMinimized() {
	SystemSettings::instance().toggle_pause_when_minimized();
}

void MenuInterface::toggleAudioDecorrelation() {
	if (computer_ && computer_->audio_system) computer_->audio_system->toggle_decorrelation();
}

void MenuInterface::toggleRightMouseAccel() {
	gs2_app_values.right_mouse_accelerate = !gs2_app_values.right_mouse_accelerate;
}

void MenuInterface::toggleCrtShader() {
	if (computer_ && computer_->video_system) computer_->video_system->toggle_crt_shader();
}

void MenuInterface::toggleMonoPersistence() {
	if (computer_ && computer_->video_system) {
		video_system_t *vs = computer_->video_system;
		vs->set_mono_persistence(!vs->mono_persistence_enabled);
	}
}

void MenuInterface::setScaling(bool sharp) {
	if (computer_ && computer_->video_system)
		computer_->video_system->set_display_pixel_mode(sharp ? DM_PIXEL_SQUARE : DM_PIXEL_FUZZ);
}

void MenuInterface::toggleHudStats() {
	SystemSettings::instance().toggle_hud_stats();
}

void MenuInterface::toggleHudDrives() {
	SystemSettings::instance().toggle_hud_drives();
}

void MenuInterface::toggleDisconnectedWhenNoGamepad() {
	SystemSettings::instance().toggle_disconnected_when_no_gamepad();
}

void MenuInterface::toggleSsTextMode() {
	SystemSettings::instance().toggle_ss_text_mode();
}

int MenuInterface::getCurrentSpeed() {
	if (!computer_ || !computer_->clock) return -1;
	return (int)computer_->clock->get_clock_mode();
}

int MenuInterface::getCurrentMonitor() {
	if (!computer_) return -1;
	video_system_t *vs = computer_->video_system;
	if (!vs) return -1;

	if (vs->display_color_engine == DM_ENGINE_NTSC) return MONITOR_COMPOSITE;
	if (vs->display_color_engine == DM_ENGINE_RGB)  return MONITOR_GS_RGB;
	if (vs->display_color_engine == DM_ENGINE_GSCOMPOSITE) return MONITOR_GS_COMPOSITE;

	switch (vs->display_mono_color) {
		case DM_MONO_GREEN: return MONITOR_MONO_GREEN;
		case DM_MONO_AMBER: return MONITOR_MONO_AMBER;
		default:            return MONITOR_MONO_WHITE;
	}
}

bool MenuInterface::getSleepMode() {
	return gs2_app_values.sleep_mode;
}

bool MenuInterface::getPauseWhenMinimized() {
	return SystemSettings::instance().pause_when_minimized();
}

bool MenuInterface::getAudioDecorrelation() {
	return computer_ && computer_->audio_system ? computer_->audio_system->get_decorrelation() : false;
}

bool MenuInterface::getRightMouseAccel() {
	return gs2_app_values.right_mouse_accelerate;
}

bool MenuInterface::getCrtShader() {
	return computer_ && computer_->video_system ? computer_->video_system->get_crt_shader_enabled() : false;
}

bool MenuInterface::getCrtShaderAvailable() {
	return computer_ && computer_->video_system ? computer_->video_system->crt_shader_available() : false;
}

bool MenuInterface::getMonoPersistence() {
	return computer_ && computer_->video_system ? computer_->video_system->mono_persistence_enabled
	                                            : SystemSettings::instance().mono_persistence();
}

bool MenuInterface::getMonoPersistenceAvailable() {
	return computer_ && computer_->video_system ? computer_->video_system->mono_persistence_available() : false;
}

bool MenuInterface::getScalingSharp() {
	return computer_ && computer_->video_system ? computer_->video_system->display_pixel_mode == DM_PIXEL_SQUARE
	                                            : SystemSettings::instance().scaling_sharp();
}

bool MenuInterface::getHudStats() {
	return SystemSettings::instance().hud_stats();
}

bool MenuInterface::getHudDrives() {
	return SystemSettings::instance().hud_drives();
}

bool MenuInterface::getDisconnectedWhenNoGamepad() {
	return SystemSettings::instance().disconnected_when_no_gamepad();
}

bool MenuInterface::getSsTextMode() {
	return SystemSettings::instance().ss_text_mode();
}

bool MenuInterface::isEmulationRunning() {
	return computer_ != nullptr;
}

bool MenuInterface::hasBazFast() {
	return computer_ && computer_->has_bazfast();
}

bool MenuInterface::hasSecondSight() {
	return computer_ && computer_->has_second_sight();
}

bool MenuInterface::getMountDrivers() {
	return computer_ && computer_->is_drivers_mounted();
}

bool MenuInterface::isPaused() {
	return computer_ && computer_->execution_mode == EXEC_PAUSED;
}

bool MenuInterface::isMouseCaptured() {
	return SDL_GetWindowRelativeMouseMode(computer_->video_system->window);
}

void MenuInterface::setControllerMode(int mode) {
	switch (mode) {
		case JOYSTICK_APPLE_GAMEPAD: pushMenuEvent(MENU_CONTROLLER_GAMEPAD); break;
		case JOYSTICK_APPLE_MOUSE:   pushMenuEvent(MENU_CONTROLLER_MOUSE);   break;
		case JOYSTICK_ATARI_DPAD:    pushMenuEvent(MENU_CONTROLLER_JOYPORT); break;
	}
}

int MenuInterface::getCurrentControllerMode() {
	if (!computer_) return JOYSTICK_APPLE_GAMEPAD;
	gamec_state_t *gc = (gamec_state_t *)computer_->get_module_state(MODULE_GAMECONTROLLER);
	if (!gc) return JOYSTICK_APPLE_GAMEPAD;
	return (int)get_joystick_mode(gc);
}

void MenuInterface::setJoyportSelect(int select) {
	switch (select) {
		case JOYPORT_SELECT_LEFT:   pushMenuEvent(MENU_CONTROLLER_JOYPORT_LEFT);   break;
		case JOYPORT_SELECT_CENTER: pushMenuEvent(MENU_CONTROLLER_JOYPORT_CENTER); break;
		case JOYPORT_SELECT_RIGHT:  pushMenuEvent(MENU_CONTROLLER_JOYPORT_RIGHT);  break;
	}
}

int MenuInterface::getJoyportSelect() {
	return (int)get_joyport_select();
}

void MenuInterface::setAppleKeyLayout(int layout) {
	if (layout < 0 || layout > static_cast<int>(AppleKeyLayout::LeftOptionOpenApple)) {
		return;
	}
	SystemSettings::instance().set_apple_keys(
		apple_key_layout_id(static_cast<AppleKeyLayout>(layout)));
}

int MenuInterface::getAppleKeyLayout() {
	return static_cast<int>(current_apple_key_layout());
}

std::vector<MenuDriveInfo> MenuInterface::getDriveList() {
	std::vector<MenuDriveInfo> result;
	if (!computer_ || !computer_->mounts) return result;

	for (const drive_info_t &info : computer_->mounts->get_all_drives()) {
		MenuDriveInfo mdi;
		mdi.key               = info.key;
		mdi.is_mounted        = info.status.is_mounted;
		mdi.is_modified       = info.status.is_modified;
		mdi.is_write_protected = info.status.is_write_protected;
		mdi.filename          = info.status.filename;
		result.push_back(mdi);
	}
	return result;
}

static MenuInterface sInstance;

MenuInterface *getMenuInterface() {
	return &sInstance;
}

#pragma once

#include <string>
#include <vector>
#include "util/StorageDevice.hpp"

struct MenuDriveInfo {
    storage_key_t key;
    bool          is_mounted;
    bool          is_modified;
    bool          is_write_protected;
    std::string   filename;
};

enum MenuSpeedID {
	SPEED_FREE_RUN = 0,
	SPEED_1_0  = 1,
	SPEED_2_8  = 2,
	SPEED_7_1  = 3,
	SPEED_14_3 = 4,
};

enum MenuMonitorID {
	MONITOR_COMPOSITE  = 200,
	MONITOR_GS_RGB     = 201,
	MONITOR_MONO_GREEN = 202,
	MONITOR_MONO_AMBER = 203,
	MONITOR_MONO_WHITE = 204,
};

struct computer_t;

class MenuInterface {
	computer_t *computer_ = nullptr;
public:
	void setComputer(computer_t *computer) { computer_ = computer; }

	void machineReset();
	void machineRestart();
	void machinePauseResume();
	void machineCaptureMouse();
	// Cycle the mouse-input mode (FOLLOW_HOST → CAPTURE → DISABLED).
	// Round-robins; same path as F1 / middle-click. The current label
	// for the menu UI comes from getCurrentMouseModeLabel() — that
	// string includes the mode name, so callers can render an item
	// whose title updates dynamically.
	void machineCycleMouseMode();
	void setSpeed(int speed_id);
	void setMonitor(int monitor_id);
	void toggleSleepMode();
	void toggleAudioDecorrelation();
	void toggleRightMouseAccel();
	void toggleCrtShader();
	void displayFullScreen();
	void editCopyScreen();
	void editPasteText();
	void openDebugWindow();
	void diskToggle(storage_key_t key);

	void setControllerMode(int mode);

	int  getCurrentSpeed();
	int  getCurrentMonitor();
	bool getSleepMode();
	bool getAudioDecorrelation();
	bool getRightMouseAccel();
	bool getCrtShader();
	bool getCrtShaderAvailable();
	bool isEmulationRunning();
	bool isPaused();
	bool isMouseCaptured();
	// Short human label for the current mouse mode, e.g. "follow host".
	// Used to build dynamic menu item titles like "Mouse Mode: capture".
	const char *getCurrentMouseModeLabel();
	int  getCurrentControllerMode();
	std::vector<MenuDriveInfo> getDriveList();
};

MenuInterface *getMenuInterface();

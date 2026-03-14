#pragma once

#include <string>
#include <vector>
#include "util/StorageDevice.hpp"

struct MenuDriveInfo {
    storage_key_t key;
    bool          is_mounted;
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
	void setSpeed(int speed_id);
	void setMonitor(int monitor_id);
	void toggleSleepMode();
	void displayFullScreen();
	void editCopyScreen();
	void editPasteText();
	void openDebugWindow();
	void diskToggle(storage_key_t key);

	void setControllerMode(int mode);

	int  getCurrentSpeed();
	int  getCurrentMonitor();
	bool getSleepMode();
	bool isEmulationRunning();
	bool isPaused();
	int  getCurrentControllerMode();
	std::vector<MenuDriveInfo> getDriveList();
};

MenuInterface *getMenuInterface();

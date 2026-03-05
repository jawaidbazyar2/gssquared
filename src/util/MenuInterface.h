#pragma once

enum MenuSpeedID {
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

class MenuInterface {
public:
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

	int  getCurrentSpeed();
	int  getCurrentMonitor();
	bool getSleepMode();
};

MenuInterface *getMenuInterface();

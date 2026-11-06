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
	MONITOR_GS_COMPOSITE = 205,
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
	// CPU composite remains available when the optional GS encoder fails to load.
	bool monitorAvailable(int monitor_id);
	// Second Sight cable sense: false=VGA, true=AppleColor RGB; sampled at mode setup.
	void setSecondSightMonitor(bool rgb);
	// Second Sight visibility: IIgs=0, card=1, both=2; no chip reset.
	void setSecondSightView(int view);
	// Save Second Sight firmware index (1.4=0, 1.5=1, 2.0=2) for Restart.
	void setSecondSightFirmware(int firmware);
	// Save the Second Sight U7 Lattice chip equations: shipped=false, v2.0=true.
	void setSecondSightLattice(bool v2);
	// Saved Second Sight firmware selection for menu checkmarks, possibly pending Restart.
	int getSecondSightFirmware();
	// Saved Second Sight capture-logic selection, independent of firmware.
	bool getSecondSightLattice();
	// Save the Second Sight INT0 pull-up modification: stock=false, added=true.
	void setSecondSightInt0Pullup(bool fitted);
	// Saved INT0 pull-up selection for menu checkmarks, possibly pending Restart.
	bool getSecondSightInt0Pullup();
	void toggleSleepMode();
	void togglePauseWhenMinimized();
	void toggleAudioDecorrelation();
	void toggleRightMouseAccel();
	void toggleCrtShader();
	void toggleMonoPersistence();
	void setScaling(bool sharp);
	void toggleHudStats();
	void toggleHudDrives();
	void toggleDisconnectedWhenNoGamepad();
	void toggleSsTextMode();
	void displayFullScreen();
	void editCopyScreen();
	void editPasteText();
	void fileSaveScreenshot();
	void fileSaveCollection();
	bool canSaveCollection() const;
	void toggleMountDrivers();
	void newDiskImage(int menu_code);
	void openDebugWindow();
	void diskToggle(storage_key_t key);
	void openSystemConfig();

	void setControllerMode(int mode);
	void setJoyportSelect(int select);
	void setAppleKeyLayout(int layout);

	int  getCurrentSpeed();
	int  getCurrentMonitor();
	bool getSleepMode();
	bool getPauseWhenMinimized();
	bool getAudioDecorrelation();
	bool getRightMouseAccel();
	bool getCrtShader();
	bool getCrtShaderAvailable();
	bool getMonoPersistence();
	bool getMonoPersistenceAvailable();
	bool getScalingSharp();
	bool getHudStats();
	bool getHudDrives();
	bool getDisconnectedWhenNoGamepad();
	bool getSsTextMode();
	bool isEmulationRunning();
	bool isPaused();
	bool isMouseCaptured();
	bool hasBazFast();
	bool hasSecondSight();
	// Whether the firmware-executing Second Sight display is attached to this machine.
	bool hasSecondSightZ180();
	// Saved Second Sight cable choice; true identifies AppleColor RGB.
	bool getSecondSightRGBMonitor();
	// Saved Second Sight view index: IIgs=0, card=1, both=2.
	int getSecondSightView();
	bool getMountDrivers();
	int  getCurrentControllerMode();
	int  getJoyportSelect();
	int  getAppleKeyLayout();
	std::vector<MenuDriveInfo> getDriveList();
};

MenuInterface *getMenuInterface();

# Menus

GSSquared provides a top menu bar appropriate to your platform: a top menu bar on macOS, an in-window menu bar on Windows and Linux.

### File
  * Launch Config… (only when the machine is off)
  * New Disk Image...
    * 5.25 DOS 3.3
    * 5.25 ProDOS
    * 800K ProDOS
    * 32M ProDOS
  * Open Disk Image...
    * Slot 6, Drive 1 - is xxxx
    * Slot 6, Drive 2 - is xxxx
    * etc.
  * Open System (only when machine is off)
  * Save System
  * Save As System
  * Mount Drivers
  * Save Screenshot
  * Close Emulation (when a machine is running)
  * Quit

**Launch Config…** picks a `.gs2` or `… Settings.txt` and boots it immediately (System Select only).

New Disk Image creates a new disk image of the specified size and type. You will be prompted for a folder and filename to save the new image to.

Open Disk Image lets you select the storage device, then you will select the disk image file to mount to that device.

Open System can be exercised when you're at the System Select screen ("Choose your retro experience"). This lets you load a system configuration you have previously defined and saved.

Save System / Save As System let you save the current system configuration. This does not include any machine state except what disk images are mounted.

**Mount Drivers** is a checkable item that mounts or unmounts the built-in `/GS2.DRIVERS` volume on BazFast (write-protected). Grayed out when BazFast is not in the current machine. The installer on that disk has options to install **Host FST**, **Marinetti**, and **Uthernet II**. See [Host FST](HostFST.md) and [Uthernet II](Cards_UthernetII.md).

**Quit** exits the application. On Windows it is enabled at System Select; while a machine is running, use **Close Emulation** first (Quit is grayed out). On macOS and Linux, Quit is available in both states.

**Save Screenshot** writes the current display (with borders) to a PNG on your Desktop, named like `GS2 Screenshot YYYY-MM-DD HH.MM.SS.png`. Shortcut: Shift+PrintScreen. Only one screenshot write can be in progress at a time.

### Edit
  * Copy Screen
  * Paste Text

Copy Screen copies the current display - with borders - into your computer's copy/paste buffer, where you can easily paste it into documents, Slack, Facebook, Twitter, etc. Shortcut: PrintScreen.

Paste Text pastes text from your computer’s clipboard into the emulated Apple, as if you were typing it. Works on II / IIe and on IIgs ADB: characters are injected one per frame so the guest can keep up. Shortcut: Shift+Insert. Reset or a keyboard flush aborts an in-progress paste.

### Machine
  * Reset
  * Restart
  * Pause / Resume
  * ---
  * Capture Mouse

Reset issues a Ctrl-RESET to the Apple.
Restart issues a Ctrl-OA-RESET to the Apple.
Pause / Resume will pause the emulator, and then resume it.
Capture Mouse - see [Using a Mouse](Mouse.md)

### Settings
  * System Speed
    * 1.0
    * 2.8
    * 7.1
    * 14.3
    * Unlimited
  * Sleep/Busy Wait
  * Game Controller
    * Gamepad (Normal Joystick)
    * Mouse (Normal Joystick)
    * Sirius / Atari Joyport
    * Disconnected When No Gamepad
  * Modifier Keys
    * OA/Cmd = ALT; CA/Opt = Win
    * OA/Cmd = WIN; CA/Opt = ALT

**Disconnected When No Gamepad** — when checked, paddle/button lines float as if no joystick were plugged in. When unchecked (default), an absent gamepad still reports a centered stick so software like Total Replay keeps joystick titles visible. See [Joysticks](Joysticks.md).

### Display
  * Monitor
    * Composite
    * GS RGB
    * Monochrome - Green
    * Monochrome - Amber
    * Monochrome - White
  * HUD
    * Stats
    * Drives
  * Full Screen
  * Second Sight Text
  * CRT Shader
  * Hover Enable / Disable

**HUD → Stats** toggles the small performance/stats overlay (off by default).  
**HUD → Drives** toggles the drive-activity strip at the bottom of the screen (on by default).  
Both are useful when recording video and you want a clean picture. Preferences are remembered in app settings.

**Second Sight Text** (IIgs with a Second Sight card) renders Apple II 40/80-column text through the card’s VGA path. Grayed out if the current machine has no Second Sight. See [Displays](Displays.md).  
**CRT Shader** applies a GPU CRT effect. Shortcut: **F7**. Available on macOS, Windows, and Linux. See [Displays](Displays.md).

### Sound
  * Sound Effects On/Off
  * Volume
  * Audio Decorrelation (Mockingboard)

Drive seek/activity sounds are stereo (drive 1 toward the left, drive 2 toward the right) when sound effects are on. On the IIgs, Ensoniq DOC output is stereo when the guest software uses stereo; mono titles are played on both speakers.

### Debug

See [Using the Debugger](UsingTheDebugger.md) for the full guide (panes, monitor commands, breakpoints, video views, and workflows).

  * **F10** or the OSD **Debug** button — show/hide the debugger window
  * On exit, the instruction trace is saved automatically as `gssquared-trace.bin` in your documents folder

Menu items for Save Trace / Load Symbols / Save Symbols are planned; use monitor commands (`sload`, `sclear`, `slookup`) and the automatic trace save on quit for now.

### Docs
  * Online Documentation
  * Donate

**Online Documentation** opens the GSSquared user docs in your browser. **Donate** opens the project donation page.

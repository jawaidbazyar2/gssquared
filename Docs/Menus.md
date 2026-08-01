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

**Launch Config…** picks a `.gs2` or `… Settings.txt` and boots it immediately (System Select only).

New Disk Image creates a new disk image of the specified size and type. You will be prompted for a folder and filename to save the new image to.

Open Disk Image lets you select the storage device, then you will select the disk image file to mount to that device.

Open System can be exercised when you're at the System Select screen ("Choose your retro experience"). This lets you load a system configuration you have previously defined and saved.

Save System / Save As System let you save the current system configuration. This does not include any machine state except what disk images are mounted.

**Mount Drivers** is a checkable item that mounts or unmounts the built-in `/GS2.DRIVERS` volume on BazFast (write-protected). Grayed out when BazFast is not in the current machine. Used to install Host FST into GS/OS — see [Host FST](HostFST.md).

**Save Screenshot** writes the current display (with borders) to a PNG on your Desktop, named like `GS2 Screenshot YYYY-MM-DD HH.MM.SS.png`. Shortcut: Shift+PrintScreen. Only one screenshot write can be in progress at a time.

### Edit
  * Copy Screen
  * Paste Text

Copy Screen copies the current display - with borders - into your computer's copy/paste buffer, where you can easily paste it into documents, Slack, Facebook, Twitter, etc. Shortcut: PrintScreen.

Paste Text pastes text in your computer's copy/paste buffer into the emulated Apple, as if you were typing it.

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
  * CRT Shader
  * Full Screen
  * Hover Enable / Disable

**HUD → Stats** toggles the small performance/stats overlay (off by default).  
**HUD → Drives** toggles the drive-activity strip at the bottom of the screen (on by default).  
Both are useful when recording video and you want a clean picture. Preferences are remembered in app settings.

### Sound
  * Sound Effects On/Off
  * Volume
  * Audio Decorrelation (Mockingboard)

Drive seek/activity sounds are stereo (drive 1 toward the left, drive 2 toward the right) when sound effects are on. On the IIgs, Ensoniq DOC output is stereo when the guest software uses stereo; mono titles are played on both speakers.

### Debug
  * Show/Hide Debugger
  * Save Trace
  * Load Symbols
  * Save Symbols

### Docs
  * Online Documentation
  * Donate

**Online Documentation** opens the GSSquared user docs in your browser. **Donate** opens the project donation page.

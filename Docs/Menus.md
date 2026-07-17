# Menus

GSSquared provides a top menu bar appropriate to your platform. A top menu bar for MacOS, an in-window menu bar for Windows and Linux.

### File
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
  * Save Screenshot

New Disk Image creates a new disk image of the specified size and type. You will be prompted for a folder and filename to save the new image to.

Open Disk Image lets you select the storage device, then you will select the disk image file to mount to that device.

Open System can be exercised when you're at the System Select screen ("Choose your retro experience"). This lets you load a system configuration you have previously defined and saved.

Save System / Save As System let you save the current system configuration. This does not include any machine state except what disk images are mounted.

Save Screenshot writes the current display (with borders) to a PNG on your Desktop, named like `GS2 Screenshot YYYY-MM-DD HH.MM.SS.png`. Shortcut: Shift+PrintScreen. Only one screenshot write can be in progress at a time.

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
  * Modifier Keys
    * OA/Cmd = ALT; CA/Opt = Win
    * OA/Cmd = WIN; CA/Opt = ALT

### Display
  * Monitor
    * Composite
    * GS RGB
    * Monochrome - Green
    * Monochrome - Amber
    * Monochrome - White
  * CRT Effects
    * Scanlines
    * etc
  * Full Screen
  * Hover Enable / Disable
* Sound
  * Sound Effects On/Off
  * Volume
* Debug
  * Show/Hide Debugger
  * Save Trace
  * Load Symbols
  * Save Symbols

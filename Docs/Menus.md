# Menus

GSSquared provides a top menu bar appropriate to your platform: a native menu bar on macOS, an in-window menu bar on Windows, Linux and Web.

Saving a machine configuration is done from the [config editor](ConfigEditor.md) (**Save** / **Save As**), not from the File menu. To launch a saved `.gs2` or `… Settings.txt` without editing it, use **File → Launch Config…** or a [System Select](Select.md) tile.

On Linux, several File items appear only when they apply: **Launch Config…** and **Quit** at System Select; **Drives**, **Mount Drivers**, **Save Screenshot**, and **Close Emulation** while a machine is running.

### File
  * Launch Config… (only when the machine is off)
  * New Disk Image
    * 5.25 Unformatted
    * 5.25 Formatted DOS 3.3
    * 5.25 Formatted ProDOS
    * 3.5 Formatted ProDOS
    * 32M HD Unformatted
    * 32M HD Formatted ProDOS
  * Drives (only when a machine is running)
    * Slot 6, Drive 1 — …
    * Slot 6, Drive 2 — …
    * (one item per mounted or empty drive)
  * Mount Drivers
  * Save Screenshot
  * Close Emulation (when a machine is running)
  * Quit

**Launch Config…** picks a `.gs2` or `… Settings.txt` and boots it immediately (System Select only). See [Selecting a System](Select.md).

**New Disk Image** opens a save dialog and writes a blank image to the chosen path. It does not mount the file — use **File → Drives**, the Control Panel, or drag-and-drop afterwards. Floppy types copy a shipped `.woz` template; **32M HD Unformatted** creates a 32M file of zeros (`.hdv`), and **32M HD Formatted ProDOS** creates the same file with an empty ProDOS volume already on it. Available whether or not a machine is running. See [Blank Disk Images](BlankFloppy.md).

**Drives** is a dynamic per-drive list. Choose a drive to mount or unmount an image the same way as the Control Panel. See [Storage](Storage.md).

**Mount Drivers** is a checkable item that mounts or unmounts the built-in `/GS2.DRIVERS` volume on BazFast (write-protected). Grayed out when BazFast is not in the current machine. The installer on that disk has options to install **Host FST**, **Marinetti**, and **Uthernet II**. See [Host FST](HostFST.md) and [Uthernet II](Cards_UthernetII.md).

**Save Screenshot** writes the current display (with borders) to a PNG on your Desktop, named like `GS2 Screenshot YYYY-MM-DD HH.MM.SS.png`. Shortcut: Shift+PrintScreen. Only one screenshot write can be in progress at a time.

**Close Emulation** powers off the virtual machine and returns to System Select. On Windows, Quit is grayed out while a machine is running — use Close Emulation first. On macOS and Linux, Quit is available in both states. Dirty floppy images prompt to save or discard unless you launched with [`--no-quit-confirm`](CommandLine.md).

**Quit** exits the application.

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
Restart issues a Ctrl-OA-RESET to the Apple (also works on II+, even though a real II+ has no Open-Apple on the game port).
Pause / Resume will pause the emulator, and then resume it.
Capture Mouse - see [Using a Mouse](Mouse.md)

### Settings
  * Speed
    * 1.0 MHz
    * 2.8 MHz
    * 7.1 MHz
    * 14.3 MHz
  * Game Controller
    * Joystick - Gamepad
    * Joystick - Mouse
    * Sirius / Atari Joyport
    * Joyport Controller Select
      * Left
      * Center
      * Right
    * Disconnected When No Gamepad
  * Apple Keys
    * Command = Open Apple
    * Alt = Open Apple
    * Left Option = Open Apple
  * Sleep / Busy Wait
  * Mono Helper
  * Right Mouse Button Accelerate

**Speed** sets the host CPU throttle. **Ludicrous / Unlimited** (video still runs at about 1 MHz so speaker, Ensoniq, and disks keep working) is not in this submenu — use **F9**, the Control Panel speed row, or the hover speed button. See [On-Screen Display](OSD.md) and [Command Line](CommandLine.md).

**Joyport Controller Select** emulates the physical Left / Center / Right switch on a Sirius Joyport. Center (default) lets software pick joystick 1 vs 2 with Annunciator 0. The submenu is available only in Joyport mode. See [Joysticks](Joysticks.md).

**Disconnected When No Gamepad** — when checked, paddle/button lines float as if no joystick were plugged in. When unchecked (default), an absent gamepad still reports a centered stick so software like Total Replay keeps joystick titles visible. See [Joysticks](Joysticks.md).

**Apple Keys** chooses the host keys for Open Apple and Closed Apple. Available whether or not a machine is running. See [Using a Keyboard](KeyboardShortcuts.md).

**Sleep / Busy Wait** — when checked, the emulator sleeps between frames instead of busy-waiting (lower host CPU). Same as the [`-s`](CommandLine.md) flag.

**Mono Helper** widens Mockingboard stereo (decorrelates the right channel) so it is easier to hear on a mono speaker or mixed-down recording. Available whether or not a machine is running. See [Mockingboard](Cards_Mockingboard.md).

**Right Mouse Button Accelerate** — hold the right mouse button to run at a faster CPU speed (same idea as the INS accelerate path). See [Using a Mouse](Mouse.md).

Drive seek/activity sounds play in stereo (drive 1 toward the left, drive 2 toward the right). On the IIgs, Ensoniq DOC output is stereo when the guest software uses stereo; mono titles play on both speakers. There is no Sound menu and no user master-volume or FX on/off toggle.

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

**HUD → Stats** toggles the small performance/stats overlay (off by default).  
**HUD → Drives** toggles the drive-activity strip at the bottom of the screen (on by default).  
Both are useful when recording video and you want a clean picture. Preferences are remembered in app settings.

**Second Sight Text** (IIgs with a Second Sight card) renders Apple II 40/80-column text through the card’s VGA path. Grayed out if the current machine has no Second Sight. See [Second Sight](Cards_SecondSight.md) and [Displays](Displays.md).  
**CRT Shader** applies a GPU CRT effect. Shortcut: **F7**. Available on macOS, Windows, and Linux. See [Displays](Displays.md).

Hover speed and display widgets appear automatically when the mouse is over the window and is not captured. There is no Hover Enable menu item. See [On-Screen Display](OSD.md).

### Debug

There is no Debug menu on the native menu bar. Open the debugger with **F10** or the hover **Debug** button.

See [Using the Debugger](UsingTheDebugger.md) for the full guide (panes, monitor commands, breakpoints, video views, and workflows).

  * **F10** or the hover **Debug** button — show/hide the debugger window
  * On exit, the instruction trace is saved automatically as `gssquared-trace.bin` in your documents folder

Menu items for Save Trace / Load Symbols / Save Symbols are planned; use monitor commands (`sload`, `sclear`, `slookup`) and the automatic trace save on quit for now.

### Docs
  * Check For Updates
  * Online Documentation
  * Donate

**Check For Updates** opens the GitHub Releases page so you can download a newer package. **Online Documentation** opens the GSSquared user docs in your browser. **Donate** opens the project donation page.

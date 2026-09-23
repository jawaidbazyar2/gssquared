# OSD - On-Screen Display

Aside from Menus, you can interact with the emulated system through a few On-Screen Display (OSD) elements:

* [Control Panel](#control-panel)
* [Hover controls](#hover-controls)
* [Disk Drive Status](#disk-drive-status)


## Control Panel

This is the "Control Panel" of the system.

To open, press **F4** or click the triangle tab that appears near the upper left corner of the display when the mouse is moving.

Inside the Control Panel, you can select:

### Display mode

NTSC, RGB, and three colors of monochrome

### Speed

1 MHz, 2.8 MHz, 7.1 MHz, 14.3 MHz, and Ludicrous speed. Ludicrous speed still ties into the normal 1 MHz video system so speaker, Ensoniq, and disk devices keep running (if ludicrously).

### Slots

Displays what virtual cards are present in slots.

If you are seeing the Control Panel from the System Select screen, it's because you asked to create or edit a System Config, and you will be able to change what is in the slots. Otherwise, it is display-only.

### Serial / Parallel

Buttons for each serial or parallel port on the machine (IIgs built-in SCC, Super Serial, Parallel, etc.). Click a button to attach **None**, **File**, **Clipboard**, **Modem**, or a listed host serial port. See [Serial & Parallel Connections](SerialConnections.md).

### Disk Drives / Storage

Drive icons show slot, drive, and the mounted filename (if any).

* Click an **empty** drive to open a file picker and mount an image.
* Click a **mounted** floppy to unmount. If the image has buffered writes, you are asked to Save, Discard, or Cancel.
* Drag a disk image onto the window: the Control Panel opens so you can drop the file on the drive you want.

See [Storage](Storage.md) for formats, write-back, BazFast, and errors.

On Apple IIgs systems, **Host Folder…** chooses which real-computer directory is shared through Host FST (volume `:Host`). Not available in the [browser build](Web.md). See [Host FST](HostFST.md).

## Hover controls

When the mouse is over the main window and is **not** captured, a strip of face buttons fades in on the left:

| Button | Action |
|--------|--------|
| **RESET** | Same as Machine → Reset (Ctrl-Reset) |
| **Capture** | Capture the mouse for guest pointer software |
| **Debug** | Open the debugger window (same as **F10**) |
| **Speed** | Shows the current throttle (`1.0`, `2.8`, `7.1`, `14.3`, or an infinity glyph for Ludicrous). Click to open a picker. |
| **Display** | Opens a picker for Composite / RGB / mono. The button accent follows the current monitor. |

The strip hides automatically when **Capture Mouse** is on, so you do not click buttons while using mouse-aware software. There is no Display → Hover Enable menu; the strip appears whenever hover is possible.

The triangle **Control Panel** tab (upper left) is separate from this strip.

## Disk Drive Status

When the Control Panel is not open, if disk drives are active, they will be displayed at the bottom of the screen, indicating slot, drive, track number, and disk image filename if an image is present.

You can hide this strip with **Display → HUD → Drives**. The stats overlay (if enabled) is toggled with **Display → HUD → Stats**. See [Menus](Menus.md) and [Displays](Displays.md).

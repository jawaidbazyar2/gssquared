# Selecting a System

When you start GSSquared, you see **Choose your retro experience** — a row of system tiles you can click to boot.

**File → Close Emulation** (or the window close button on a running machine, depending on platform) powers the virtual machine off and returns here. That is how you switch machines without quitting the app. A CLI launch that passed a config path or `-p` quits instead of returning to this screen — see [Command Line](CommandLine.md).

## Built-in systems

Tiles typically include:

* Apple ][ / ][+
* Apple //e and Enhanced //e
* Apple //e Enhanced with **65816** (optional [VIDHD](Cards_VIDHD.md) in a custom config)
* Apple IIgs **ROM 01**
* Apple IIgs **ROM 03**

ROM 01 and ROM 03 are separate platforms. Pick the one that matches the software or GS/OS setup you want to run.

GSSquared is one app for both 8-bit Apple II machines and the 16-bit IIgs. ROMs and assets ship in the package; you do not download firmware separately.

## Custom and recent configs

Besides the built-in tiles:

* **+** — create a new custom machine in the [config editor](ConfigEditor.md).
* **Edit…** / folder — open an existing `.gs2` (or A2Fusion `… Settings.txt`) to edit or launch.
* **Recent custom configs** — tiles for configs you have opened before (from `Documents/GSSquared/` and recent history). Click one to boot it immediately.

Shipped example `.gs2` files are copied into `Documents/GSSquared/` on first run (including a PAL //e example). **File → Launch Config…** defaults there when you browse for a config. Older installs that used the prefs `SystemConfigs` folder are migrated into Documents on startup.

## Launching without the tile row

* **File → Launch Config…** — pick a `.gs2` or `… Settings.txt` and boot it.
* Double-click a `.gs2` (macOS, single instance) or use Open With. On **Windows**, a second double-click starts a **second process**. See [File types and URL protocols](ProtocolHandlers.md).
* Drag a `.gs2` onto the System Select window.
* Pass a config path or `-p N` on the [command line](CommandLine.md) (skips this screen; closing then quits).

See [Creating Custom System Configs](ConfigEditor.md) for editing slots, disks, and serial attachments.

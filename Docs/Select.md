# Selecting a System

When you start GSSquared, you see **Choose your retro experience** — a row of system tiles you can click to boot.

## Built-in systems

Tiles typically include:

* Apple ][ / ][+
* Apple //e and Enhanced //e (and related variants)
* Apple IIgs **ROM 01**
* Apple IIgs **ROM 03**

ROM 01 and ROM 03 are separate platforms. Pick the one that matches the software or GS/OS setup you want to run.

## Custom and recent configs

Besides the built-in tiles:

* **+** — create a new custom machine in the [config editor](ConfigEditor.md).
* **Edit…** / folder — open an existing `.gs2` (or A2Fusion `… Settings.txt`) to edit or launch.
* **Recent custom configs** — tiles for configs you have opened before (from your prefs `SystemConfigs` folder and recent history). Click one to boot it immediately.

Shipped example `.gs2` files are copied into your application preferences folder on first run (`…/GSSquared/SystemConfigs/` under your OS prefs path). **File → Launch Config…** defaults there when you browse for a config.

## Launching without the tile row

* **File → Launch Config…** — pick a `.gs2` or `… Settings.txt` and boot it.
* Double-click a `.gs2` (macOS) or use Open With.
* Drag a `.gs2` onto the System Select window.
* Pass a config path on the command line (boots that machine and skips System Select).

See [Creating Custom System Configs](ConfigEditor.md) for editing slots, disks, and serial attachments.

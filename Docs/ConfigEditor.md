# Creating Custom System Configs

GSSquared lets you build and save your own system configurations — which Apple II model, which cards are in which slots, which disk images to mount at launch, and what is attached to serial/parallel ports.

Saved configs are `.gs2` files. You can also open community profile packs that use `… Settings.txt` files. For hand-editing the file format itself, see [Writing Config Files Manually](ConfigFiles.md).

## From the System Select screen

When you start GSSquared, you see **Choose your retro experience** with the built-in system tiles (including IIgs ROM 01 and ROM 03), recent custom configs, plus:

* **+** — create a new custom config from scratch. Opens the config editor with a default Enhanced //e setup you can change.
* **Edit...** — open an existing config file (`.gs2` or `… Settings.txt`) and load it into the editor so you can change it and save.

Hover the **+** or **Edit…** tile to see a short description at the bottom of the screen.

## Launch Config… (menu)

From the System Select screen, use **File → Launch Config…**.

Pick a `.gs2` or `… Settings.txt` file. Unlike **Edit...**, this loads the config and boots it immediately — it does not open the editor. The file dialog starts in your prefs `SystemConfigs` folder (where shipped examples are copied on first run).

Launch Config is only available when the machine is off (at the System Select screen).

## Double-click a `.gs2` file

On macOS, `.gs2` files are associated with GSSquared. Double-click one in Finder (or use Open With → GSSquared).

* If the System Select screen is showing, GSSquared loads that config and launches it.
* If emulation is already running, you'll see a short message asking you to quit emulation first.

You can also drag a `.gs2` (or `… Settings.txt`) onto the GSSquared window while at System Select — same result as launching it.

## Open With… for `Settings.txt` files

Community profile packs (arqyv / A2Fusion) ship as folders with disk images and one or more files named like `Something Settings.txt`.

These are not double-click associated the way `.gs2` is (because we don't want to overload .txt files on your system). To open one:

1. Right-click the `… Settings.txt` file.
2. Choose **Open With → GSSquared**.

GSSquared loads the profile and boots it, the same as Launch Config. You can also use Launch Config or **Edit...** and pick the Settings file from the file dialog.

## Using the config editor

The editor title is **Edit System Configuration**. You can set:

* **Name** and **description** — shown on the badge / for your own reference.
* **Platform** — Apple II, II+, //e, Enhanced //e, IIgs ROM 01, IIgs ROM 03, etc.
* **Slots** — click a slot to pick which card goes there (or None). Card choices include Disk II, BazFast, Mockingboard, [Uthernet II](Cards_UthernetII.md), [Super Serial](Cards_SuperSerial.md), [Apple Mouse](Cards_AppleMouse.md), [Parallel](Cards_Parallel.md), [Video Overlay Card](Cards_VOC.md) (IIgs slot 3), and others.
* **Storage (pre-mount)** — click a drive to choose a disk image that will be mounted when you later launch this config. Click again to clear it. Any valid disk image including .pmap (BazFast multi-image) can be "pre-mounted" this way.
* **Serial / Parallel** — click a port button and choose **None**, **File**, **Clipboard**, **Modem**, or a listed host serial port (serial jacks only). See [Serial & Parallel Connections](SerialConnections.md).

Speed and Display controls on this screen are for preview only; they are **not saved** into the config file.

**Save** writes a `.gs2` file (you'll get a save dialog). **Cancel** returns to System Select without saving.

After saving, use **Launch Config…**, double-click the `.gs2`, or Open With to run it. Recent configs also appear as tiles on System Select.

## Machine identity (BRAM)

Each `.gs2` file gets a stable **`id`** (UUID). On Apple IIgs configs, battery RAM / Control Panel NVRAM is stored per `id` under your prefs `bram/` folder — so two different configs do not overwrite each other’s BRAM. You normally do not need to edit `id` by hand; GSSquared assigns one when missing. See [Writing Config Files Manually](ConfigFiles.md#machine-identity-id).

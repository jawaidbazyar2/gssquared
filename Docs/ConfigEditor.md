# Creating Custom System Configs

GSSquared lets you build and save your own system configurations — which Apple II model, which cards are in which slots, and which disk images to mount at launch.

Saved configs are `.gs2` files. You can also open community profile packs that use `… Settings.txt` files. For hand-editing the file format itself, see [Writing Config Files Manually](ConfigFiles.md).

## From the System Select screen

When you start GSSquared, you see **Choose your retro experience** with the built-in system tiles, plus two extra tiles:

* **+** — create a new custom config from scratch. Opens the config editor with a default Enhanced //e setup you can change.
* **Edit...** — open an existing config file (`.gs2` or `… Settings.txt`) and load it into the editor so you can change it and save.

## Launch Config… (menu)

From the System Select screen, use **File → Launch Config…**.

Pick a `.gs2` or `… Settings.txt` file. Unlike **Edit...**, this loads the config and boots it immediately — it does not open the editor.

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
* **Platform** — Apple II, II+, //e, Enhanced //e, IIgs, etc.
* **Slots** — click a slot to pick which card goes there (or None).
* **Storage (pre-mount)** — click a drive to choose a disk image that will be mounted when you later launch this config. Click again to clear it. Any valid disk image including .pmap (BazFast multi-image) can be "pre-mounted" this way.

Speed and Display controls on this screen are for preview only; they are **not saved** into the config file.

**Save** writes a `.gs2` file (you'll get a save dialog). **Cancel** returns to System Select without saving.

After saving, use **Launch Config…**, double-click the `.gs2`, or Open With to run it.

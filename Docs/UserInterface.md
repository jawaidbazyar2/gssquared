# User Interface

The primary goal of first phase of UI, is the following:

* Focus on "Normal" user interaction (people running apple ii software, not necessarily developers)
* Clean, futuristic design
* Video Game Approach - on screen display, hover controls, dynamic presentation

When we're "in emulator", I don't want a bunch of "modern" user interface cluttering up the "1980s computing" experience.

Implementing these will be a primary target of the 0.8 release.

## System Selector

* Create new profile (opens full control panel) (tile with a big + in the middle)
* Load Other profile.

## Assets

* Have a professional clean up the artwork

## Control Panel

* Show Platform Type and Logo / Icon
* Show/Manage slots (Manage only when system off)
* Show / Manage devices connected to serial ports
* Manage Hard Disk Partitions on SmartPort
* Drag and drop mount disks - if CP not open, slide it open as drag enters our window, and the reverse
* When system is off will need info about what drives, just for display. They can mount disks and re-save after starting.
* Create, Save, Load system configs
* UI doesn't need to redraw the various UI textures unless it's been updated (incl. system select)
* Control panel is used to configure system - full control when system off, more limited control when system on (e.g. can make config with pre-mounted disks)
* Save, Save As.. settings to file. (Settings load/save to json code)
* Slots: use image of the slot connector with text over it. Slot number outside it to the left. "Slots" label.
* Scale with window size? (Coordinate system scaling will bend my mind)
* Consider using higher-resolution assets and scaling down as necessary (will look better on high-res screens)

we're going to need a "register serial port" concept for OSD to talk to ports.

## Hover Controls

The Hover Controls serve two purposes. 
1. to provide controls that are accessible even when in fullscreen
1. to support the "video game" look'n'feel I am going for.

The hover controls live in the top border area, just above the content (and below the area for the fading notification messages)

* Rethink where have things like reset, system speed, etc. currently in OSD.
* when click on a setting like speed, monitor to change it, accordion out to a container (could have them pop down..).
* small text label underneath each setting
* Don't mouse capture if we're clicking in active hover controls area
* don't show controls if we're not the active window.
* require minimum motion for mouse to activate Hover

* Reset (control-reset)
* Restart (3-finger, or literal power off/power on)
* Power Off
* Pause / Play
* System Speed
* Monitor Type
* CRT Effects
* Game Controller selections
* Full Screen
* Open Debugger
* Sleep / Busy (CP)
* Sound effects on / off (CP)
* Modifier Keys Mapping (alt/win/cmd/option/etc.) (CP)
* Mouse Capture auto / manual (if we make it always a button)

This is way more things than will fit in the window, esp if they make it smaller. Marked with CP vs Hover above. So that is let's say 8 to 10 items. Should be okay, esp with the accordion/pulldown.

Styling - my icons suck. What about doing some icons that (for the most part) are line art. And draw them xor with the background so no matter what the border is they'll be clear.
Hm, they could match the text fg/bg color. interesting.. I feel like this also would benefit from being drawn to larger scale, then scaling down as needed. Hm, what if we did them as SVGs? Re-generate them on a window resize.

Can be lined up in line with where the word "control panel" is, and that is also in line with the CP open widget, which leaves room for the text fade above.

Slow down the OSD open/close a bit, to make it whooshier.

## System Menus

* File
  * New
  * Open System (only when machine is off)
  * Save System
  * Save As System
  * Storage
    * Slot 6, Drive 1 - is xxxx
    * Slot 6, Drive 2 - is xxxx
* Edit
  * Copy Text
  * Copy Screen
  * Paste Text
* Machine
  * Reset
  * Restart
  * Power Off/On
  * Pause / Resume
* Settings
  * System Speed
    * 1.0
    * 2.8
    * 7.1
    * 14.3
    * Unlimited
  * Sleep/Busy Wait
  * Game Controller
    * Gamepad (Normal Joystick)
    * Atari Joyport
    * the other options..
  * Modifier Keys
    * Alt=OA (or, Command=OA)
    * Win=OA (or, Option=OA)
* Display
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

## Tray Icons

SDL3 has support for system tray icons with attached menus. Could put a limited subset of the menu items there. However, this is likely somewhat redundant to regular menus.

This might be the easiest way to get menus into Linux (versus requiring gtk).

## Debugger

this could use a lot of work. it is here that using something like ImGui might be suitable. Give that some thought.
The h/v location, frame video cycle, idle should only appear when debugger is open. Maybe when Hover controls are active. Maybe both.
Avoid KEGS "Wall of dense technical information".

## Mouse Capture

Maybe we can have something interesting like Control-Click Capture/Uncapture, in addition to F1?

SDL_WarpMouseInWindow(window, x, y): Warps the mouse pointer to a specific coordinate. This is essential when switching from relative mode back to absolute mode to place the mouse cursor exactly where the user expects it. <- worth a try here. IFF we're in GS/OS and we can read the GS/OS mouse cursor position, make this call. That's getting a little hinky!

## Key Shortcuts Limitations per System

### Linux

Linux preempts Ctrl-Alt-Fx, causes switch to different virtual desktop.
So Ctrl-Alt-Reset could be: Ctrl-Alt-BRK

### MacOS

MacOS preempts F11. But only F11. Modifiers-F11 seems ok.
F12 seems to have been adopted by several platforms as RESET. (KEGS; Crossrunner;)

### Windows

Not sure of any right now.

## System-Specific Optimizations

### MacBook - Touch Bar

The MacBooks produced from 2016-2022 have a "touch bar" which is a app-programmable touchable interface. You can put your own "keys", indicators, status, etc. there. While Apple deprecated this, I think there is probably a big installed base that has it. We can offer an option to enable/disable custom touchbar, to put things like reset, etc there. Probably a lot of our Fx key mappings but with snazz icons for usability.

# Classes

## Config

Have a header file only class whose purpose is to:
* select / program key shortcuts
* make them available to various code.

Static, Global class. Values are dynamic, though, in case we end up having multiple layout options per platform and want to enable user to switch between them dynamically.

GS2 becomes a container for things like this (e.g., paths, etc.)

## Menu Definition and Callbacks

Have abstraction to define the menus we want and callbacks; modifying menu items (e.g., gray, currently selected, ...)

## Key Shortcut Callbacks

instead of directly requesting an SDL event, devices videosystem etc will request to be hooked into a dispatcher that is savvy to the keypresses but also to the menu bar items, which will then issue the closure callbacks.

There are the following potential sources of these callbacks: menus, hover, touchbar, etc..

This can be an "event bus", that accepts events from multiple sources, and dispatches them as appropriate / requested.

So we need one thing to tie them all in together.

# Drag and Drop

ok, here are some interesting Factoids.

First, you can have multiple files in a Drop. You get one Drop event per file. So, I think the DROP_FILE event needs to be handled by something that knows about its structure a little?
Right now I am iterating the Drives container looking for the highlighted one. But instead, just have each button class handle the drop event itself. Then the SmartPort hard drive can handle multiple file drops, and the floppies can disregard subsequent mounts? Maybe the timestamp on subsequent DROP_FILE would be the same and can filter that way.
Except there's a bunch of injection we'd have to do, even in StorageButton. For the mount call.. well, the device will either accept multiple mounts or not.

I'd like to support drop onto a drive with a disk already mounted - that is going to require unmounting, maybe doing the "unsaved changes" dialog, etc before actually doing the mount. Yeah, we could send a frame_appevent and have that handle the mount/unmount/blah state machine instead of trying to do it inside the OSD code.

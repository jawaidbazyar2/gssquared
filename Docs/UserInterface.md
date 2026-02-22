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

* Rethink where have things like reset, system speed, etc. currently in OSD.
* when click on a setting like speed, monitor to change it, accordion out to a container (could have them pop down..).
* small text label underneath each setting
* Don't mouse capture if we're clicking in active hover controls area
* don't show controls if we're not the active window.
* require minimum motion for mouse to activate Hover

* Reset (control-reset)
* Restart (3-finger, or literal power off)
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

* Copy / Paste / Screenshot (Copy for copy text that's on screen)
* Save / Save As (Also, but should be button in the control panel)
* Can put in most of the Hover controls above, for people who really really want that stuff
* Debugger menu - save trace, load symbols, etc.
* Disable/enable hover controls 

## Debugger

this could use a lot of work. it is here that using something like ImGui might be suitable. Give that some thought.
The h/v location, frame video cycle, idle should only appear when debugger is open. Maybe when Hover controls are active. Maybe both.
Avoid KEGS "Wall of dense technical information".

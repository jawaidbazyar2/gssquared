# User Interface

The primary goal of first phase of UI, is the following:

* Focus on "Normal" user interaction (people running apple ii software, not necessarily developers)
* Clean, futuristic design
* Video Game Approach - on screen display, hover controls, dynamic presentation

When we're "in emulator", I don't want a bunch of "modern" user interface cluttering up the "1980s computing" experience.

Implementing these will be a primary target of the 0.8 release.

## OSD

OSD is On Screen Display, which refers generally to the way we composit controls, displays on top of the main display. The OSD module is the repository for all that code.

## System Selector

* Create new profile (opens full control panel) (tile with a big + in the middle)
* Load Other profile.

## Assets

* Have a professional clean up the artwork

## Control Panel

* [X] Show Platform Type and Logo / Icon (done)
* [X] Show slots
* [ ] Manage slots (only when system off)
* [ ] Show / Manage devices connected to serial ports
* [X] Manage Hard Disk Partitions on SmartPort
* [X] Drag and drop mount disks - if CP not open, slide it open as drag enters our window, and the reverse
* When system is off will need info about what drives, just for display. They can mount disks and re-save after starting.
* [ ] Create, Save, Load system configs
* UI doesn't need to redraw the various UI textures unless it's been updated (incl. system select)
* Control panel is used to configure system - full control when system off, more limited control when system on (e.g. can make config with pre-mounted disks)
* Save, Save As.. settings to file. (Settings load/save to json code)
* Slots: use image of the slot connector with text over it. Slot number outside it to the left. "Slots" label.
* Scale with window size? (Coordinate system scaling will bend my mind)
* Consider using higher-resolution assets and scaling down as necessary (will look better on high-res screens)
* OSD background color chosen to match system case color. 

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
* Edit
  * Copy Text
  * Copy Screen
  * Paste Text
* Machine
  * Reset
  * Restart
  * Power Off/On
  * Pause / Resume
  * ---
  * Capture Mouse
* Settings
  * System Speed
    * 1.0
    * 2.8
    * 7.1
    * 14.3
    * Unlimited
  * Game Controller
    * Joystick - Gamepad
    * Joystick - Mouse
    * Atari Joyport
  * Modifier Keys
    * OA/Cmd = ALT; CA/Opt = Win
    * OA/Cmd = WIN; CA/Opt = ALT
  * Sleep/Busy Wait
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

Getting confused when capture was on and we F4... need to restore the capture state after F4. Done (1-layer stack).
Also, Alt-TAB takes us out of our window focus, releases capture, but SDL re-captures when focus returns to our window. So that all seems to work well.

There is some edge case where the system stops respecting MouseCapture mode. it hides the mouse, but it doesn't protect against moving outside the window. I don't think it's my code. Ah, it might be: if the window is moved, mouse capture stops working. Yep.

## Modifier Keys

II+: has only control and shift, shift only used for special symbols.
IIe: control, shift, open-apple, closed-apple. open apple is left of space, closed apple is right of space.
IIgs: OA (cmd) and CA (option) are next to each other.

Right now, applekeys.hpp assumes split (different behavior left/right). Which is a bug, since GS has them next to each other.

There are three places that have this:
gamecontroller (because OA/CA are also pushbuttons)
ADB
keyboard (iie)

Ah we're back to my very original issue: PC keyboard on Mac, maps windows->cmd and alt->option by default. I have them reversed. So on my Mac, I'm just gonna have to get used to them being reversed. Which they -are-. They are then correct on every other platform, including laptop keyboards. Suck it up.

So I'll be using the same keys I always have, except on the MacBook. Let's test that.

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

The Bus messages will be a function ID: e.g., "set speed", "toggle speed", "open control panel", etc.

# Drag and Drop

ok, here are some interesting Factoids.

First, you can have multiple files in a Drop. You get one Drop event per file. So, I think the DROP_FILE event needs to be handled by something that knows about its structure a little?
Right now I am iterating the Drives container looking for the highlighted one. But instead, just have each button class handle the drop event itself. Then the SmartPort hard drive can handle multiple file drops, and the floppies can disregard subsequent mounts? Maybe the timestamp on subsequent DROP_FILE would be the same and can filter that way.
Except there's a bunch of injection we'd have to do, even in StorageButton. For the mount call.. well, the device will either accept multiple mounts or not.

I'd like to support drop onto a drive with a disk already mounted - that is going to require unmounting, maybe doing the "unsaved changes" dialog, etc before actually doing the mount. Yeah, we could send a frame_appevent and have that handle the mount/unmount/blah state machine instead of trying to do it inside the OSD code.

Maybe we can flash the hover on and off and pause for a half second after DROP_FILE ? Just to give a visual indication something happened? Of course they'll get the message up top..

# Mouse Location

If the mouse is outside the visible screen area (i.e., if it's in window, but also over borders) then make sure OSD eats mouse events, so clicking on things outside the screen area will never cause clicks to effect the VM.

# Other Design Ideas

there is also the possibility to use a windows-like Hamburger menu.
Three lines. this pops down a menu, then you can navigate that. progressive (click and release, instead of click and drag). it wouldn't interfere with emulator operations at all. it would be a fair bit like the feel of the control panel. it's a metaphor many are familiar with. and it would not require platform-specific code..

# UI Sections

* Slide-out Control Panel
* StatusMessage: Fade-out message text.
* Drive HUD
* Side Menu

Right now these are all just piled into big chonky funktions. Split these out into specialized Containers and trim OSD down into just one more container, that composes and loops through the others.

Start with Fade-out text then Drive HUD since these are relatively simple (don't have any interactivity).

StatusMessage: it's a container with just one element.

# UI Element API

## Rects

* tile position
* content position
* something else

## Style

## Attributes

* Visible

Is the element visible and displayed?

* Hover

Is the mouse hovering over the element?

* Active

For 'toggle' type elements, is the element active or inactive? (e.g., the current speed selection)

Specific subclasses can define and render visual behavior based on these flags (or others they may choose to implement).

We may want to separate the "style" (which is a definition partly of what colors to use given certain states) and the effective colors, which is a more delimited palette. 

Making some progress now with these abstraction changes.

## Drives HUD

There are sort of TWO of these. One that displays inside the Control Panel. One that displays at bottom of screen (and which we probably will allow for a user to optionally turn off).

They are currently intertwined, sharing setup, configuration, etc. I don't want to do them twice, but we're not exactly doing them twice. And they don't appear on screen at the same time - it's one or the other. If invisible, all of the logic is bypassed. So, ok then. Let's go ahead and write them independently.

## "Radio" Multi-selectors

ok, we have a few of these. Speed, monitor type; there will be more.

RadioSelect
contains a number of buttons
when we click on one of the buttons, we want to signal the parent to update state.
The parent will have a "set_selection" method for this. It matches the key on each element to find the correct one.
A subclass then will be the thing that composes a specific "menu". And it sets the onclick for each button, to call the set_selection method.
And it will override set_selection, to call the parent one but then also call into the system to change the parameter.

Instead of having a FadeContainer specialization, perhaps have an Animator we can pass into Container; Container then instances the Animator we want.
Do that after I've got the RadioSelect working.

ok, I had to add an ncontainers loop to osd::event, but now it's working! to set the speed. Problem is, we fade out, and, the menu never goes away. What we want is:
click speed
speed menu opens
click speed again to close it
or, click new speed which sets speed and then closes it.
So, I can just hide myself, right? oops, menu doesn't open again after that.
So something is eating events or otherwise not iterating handle_event for children..
ncontainers
  hover_controls_con
    hov_speed_con

Working pretty well now, but, want to minimize the submenus whenever we hide the main menu.

Windows menus implemented. works well. It's still displayed in fullscreen though, so we probably want to hide it when in fullscreen. Otherwise it's kind of awkward.
OH, also on Windows the "Choose your retro experience" is partly erased at the bottom. The buttons container backround was black and there must have been some overlap. Not sure why it didn't do the same thing on Mac? resolution / placement difference I guess.

Linux - ahh, linux, you are a PITA. Trying to implement using Gtk. It's not going well. there are usability issues, particularly:
1) there is a permanent hamburger in the upper left corner of the emulator window. dislike.
2) sometimes it works as expected; sometimes you click the burger, move over into the menu, and it disappears in 1 second.
3) the wiring into Gtk etc is fairly involved and likely fragile.

SO. What if we implement the menu as containers of just text buttons. It would be exactly like we have now with the graphical buttons, except just text.

And at that point, if that works well enough, do we just use THAT as the menu for all 3 platforms?
Humhmmmm.

OK, tried the system tray but the thing isn't even enabled in GNOME/Ubuntu 22 by default. Oy vey, does linux actually hate application authors? (the answer is: yes, and this is clearly a key reason for Linux desktop adoption failure).

OK tried imgui. it's not awful. it lets you pick a custom font, so I am using Oxygen. Two observations:
1. The menu text is still a little small, at 17px.
1. since -we- draw the menus based on -our- frame rate, it is fully asynchronous (this would be a benefit on the Mac)
1. it is obscuring part of the top border area. So we should either expand increase the window size slightly, or, have the menu display when we're a) not mouse captured and b) mouse has been moving.
1. we should see how much we can tweak the styling. Can we include cool icons in the menu items? Can we have slider menu items for hue / saturation?
1. if I go cross platform with this, we're going to want to bring the imgui source into the project.

So I would say at this point we have a -glut- of user interface choices. 
The linux and Windows menu placement is basically the same. Mac could offer an option?

I wonder if we should convert mouse wheel to up and down arrows. (and, mouse wheel while control or shift is down, to left and right arrows). There will be no Apple II software that understands what a mouse wheel is.

[ ] test Mouse Wheel -> arrow key insertion.  

so, mouse wheel motion should probably be capped at 2-3 up-arrow insertions. Currently, mouse wheel is set up as a paddle. I think that is probably pretty useless.

[ ] Remove "mouse wheel is paddle" function.  

In terms of packaging, the imgui library is a .a - so, static? And basically always linked into my exe? Ah, yep. well ok then so it's good to go.

## Create / Save / Load System Configs

on the SelectSystem screen, have the following:

A + icon; A Folder icon.
The first lets the user create and define a new system config.
The second lets them open an existing system config file.

Also have this under File menu:
* Open System Config
* Save System Config

Push some of our current system configs into files to clean up the display.

Can have recently opened user configs show up as buttons on the main screen. e.g. maybe just have six (II, II+, IIe, Enh IIe, GS ROM01, GS ROM03). Then other / recent ones show up (up to four), then the + and Folder. Yeah that's nice.

[ ] Get rid of the IIgs with 5.25 only. that was really just for testing.

So these should open/save by default in the user directory. 

We want to support both TOML and Project X files. The TOML .gs2 files are really more for locally-user-created files.
Project X will be ones we 
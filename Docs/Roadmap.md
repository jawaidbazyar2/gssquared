
# Current Release: 0.7.1

## Release 0.3

[X] refactor all the other slot cards (like mb) to use the slot_store instead of device_store.  
[X] vector the RGB stuff as discussed in DisplayNG correctly.  
[X] make OSD fully match DisplayNG.  
[X] Fix the joystick.  
[X] video setup code should be put into its own area.  
[X] Implement general filter on speaker.cpp. (Ended up doing big rewrite to improve audio)  

## Release 0.3.5:

[X] refactor the hinky code we have in bus for handling mockingboard, I/O space memory switching, etc.  
[X] Tracing  
[X] Debugger  
[X] Bring in a decent readable font for the OSD elements  
[X] GS2 starts in powered-off mode. have snazzy tiles showing each platform that can be booted.  
[X] Can select a 'platform' each of which has a default configuration.  
[X] Can power on and power off vm  

## Release 0.4

[X] Apple IIe support  
[X] 80-column / double lo-res / double hi-res  

## Release 0.5

[X] Drag/drop disk images onto window to load into first drive  

[X] Implement new optimized audio code  

[X] Implement cycle-accurate video display to support apps that switch video mode by counting cycles  
[X] implement floating-bus read based on video data  

[X] provide a mode for Atari Joyport - use the dpad. https://lukazi.blogspot.com/2009/04/game-controller-atari-joysticks.html. Can also use gamepad.  

## Release 0.6

[X] Refactor CPU to be more cycle-accurate including false/phantom reads/writes  
[X] Support PAL video timing  
[X] Implement even more optimized audio code  

## Release 0.7

[X] First Apple IIgs release  

## Release 0.7.1

[X] Maintenance release: bug fixes
[X] More visual/sound effects for broader class of disks
[X] Display AppleDisk drives on GS  
[X] IWM 5.25

## Release 0.7.2

[X] put common controls in hover-over on main page - reset, restart, power-off, open debugger  
[X] put "modified" and "write protected" indicator of some kind on the disk icons.  
[X] Add Menus on Mac/Windows
[X] Add Help menu  

## Release 0.7.3

[X] 5.25 Floppy Disk Support for .WOZ files  
[X] Mockingboard passes all mb-audit tests  

## Release 0.8

[X] IWM 5.25/3.5 + WOZ  
[X] True "hard drive" Control Panel for editing mounts on the SmartPort device.
[X] when we go to power off (from inside OSD), check to see if disks need writing, and throw up appropriate dialogs.  

## Release 0.9

[X] Edit, Save, Load custom hw config. (Click "+ Custom"). (load, edit, save from inside editor is complete)
[X] Mouse tracking for GS/OS  
[X] CRT Effects

## Release 0.10

[X] Host FST
[X] Built-in RO Disk with drivers (e.g. Host FST driver) mount/unmount with menu item; PPU mode demo; various other things.
[ ] Provide flexible AI-drivable binary protocol debugger interface  

## Release 1.0

[ ] implement shaders on windows/linux
[ ] Fix all known / pending bugs  


## Post-1.0

[ ] Refactor MB to use new fixed-point synth  
[ ] Optimize / cache UI elements  

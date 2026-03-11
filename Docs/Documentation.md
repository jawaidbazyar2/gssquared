# GSSquared - Ultimate Apple II Series Emulator

The ambition for this project is large - and it will take a while to get it done. So at any given point in time, consider it a work-in-progress complete with bugs, missing features, and experiments. Of course there will be plenty that "just works".

Looking for how to use GSSquared?
[Click here for User Documentation](Index.md)

Keep reading for Project Status, Design, and development journals.

* [Project Status](#project-status-as-of-2025-06-12)
* [Code Organization](#code-organization)

# Project Status (as of 2026-03-10)

## Supported Host Platforms

| Platform | Status |
|----------|--------|
| MacOS    | Builds a MacOS App Bundle with all assets |
| Linux    | Builds a linux AppImage with all assets |
| Windows  | Builds a ZIP file with all assets (no 'installation' required) |

## Modeled Virtual Platforms

| Platform | Status |
|----------|--------|
| Apple II Rev 0. | ❌ |
| Apple II  | ✅ |
| Apple II+ | ✅ |
| Apple II j-plus | ❌ |
| Apple IIe | ✅ |
| Apple IIe (enh) | ✅ |
| Apple IIc | ❌ |
| Apple IIc+ | ❌ |
| Apple IIgs | ✅ |


## Supported Video Modes

The following video modes are currently implemented:

| Apple II Mode | Composite | RGB | Monochrome |
|---------------|-----------|-----|------------|
| Text (40-col) | ✅        | ✅  | ✅         |
| Text (80-col) | ✅        | ✅  | ✅         |
| Videx VideoTerm |         |     | ✅         |
| LGR           | ✅        | ✅  | ✅         |
| HGR           | ✅        | ✅  | ✅         |
| DLGR          | ✅        |     | ✅         |
| DHGR          | ✅        | ✅  | ✅         |
| SHR 320       |           | ✅  |            |
| SHR 640       |           | ✅  |            |

The Composite mode is an accurate simulation of a real NTSC monitor.
RGB is an accurate (exact) Apple IIgs RGB presentation.

## CPUs

| CPU Type | Status |
|----------|--------|
| NMOS 6502 | ✅ |
| CMOS 6502 | ✅ |
| 65c816 | ✅ |

## Keyboard

| Model | Status |
|-------|--------|
| Apple II+ | ✅ |
| Apple IIe | ✅ |
| Apple IIc | ❌ |
| Apple IIgs | ✅ |


## Memory Expansion

| Type | Status |
|------|--------|
| Language Card | ✅ |
| Apple II Memory Expansion Card ("Slinky") | ✅ | 

## Storage Devices

| Type | Status |
|------|--------|
| Cassette | No support planned. |
| Disk II Controller Card | ✅ |
| ProDOS Block-Device Interface | ✅ |
| SmartPort / ProDOS Block-Device Interface | ✅ |
| RAMfast SCSI Interface | Not started |
| IWM - Integrated Woz Machine | 5.25 Support Complete, 3.5 Pending |

Additional notes: 5.25 Floppy does not support quarter or half tracks.

## Disk Image Formats

| Format | Status | Size | Notes |
|--------|--------|------|-------|
| .do, .dsk | ✅ Read/write | 143K | |
| .po | ✅ Read/write | 143K | |
| .hdv | ✅ Read/write | Any size | Block device, complete |
| .2mg | ✅ Read/write | Various | For block devices, complete |
| .nib | ✅ Read/write | 143K | |
| .woz | Not started | | |


| Component | Status | Notes |
|-----------|--------|-------|
| Basic Speaker | ✅ Complete |  |
| Ensoniq / IIGS Sound | In progress |

## I/O Devices

| Device | Status | Notes |
|--------|--------|-------|
| Printer / parallel port | Work in progress | Right now just dumps binary data to file, no processsing or printer emulation |
| Printer / serial port | Partial | GS-only, support SCC chip and "print to file" |
| Modem / serial port | Not started | GS-only, support SCC chip and Hayes-compatible modem with telnet |
| ImageWriter printer emulation | Not started | |
| Joystick / paddles | ✅ Complete | Mouse emulation of Joystick; Gamepads;  |
| Sirius JoyPort (Atari) | ✅ | Implemented Single Joystick - enable mode with F6 |
| Shift-key mod and Lowercase Character Generator | Not started | |

## Clocks

| Type | Status | Notes |
|------|--------|-------|
| Thunderclock | Partial | Read of time implemented. Interrupts, writing clock - not implemented. Needs testing. |
| Generic ProDOS-compatible Clock | Complete | Read-only. |
| IIgs Realtime Clock | ✅ | syncs to host clock using local time zone |

# More Detailed Notes on Status

## Keyboard

### Apple II+

For specific platform key mappings, see: [Keyboard Shortcuts](KeyboardShortcuts.md)

On the Mac, you will want to disable "Pressing Option 5 Times enables Mouse Keys", and Dictation. Otherwise pressing CTRL repeatedly will interfere with the emulator's use of the option / control keys.

BTW, Control-OpenApple-Reset will also "force reboot" a II+ mode, because it's super-useful, even though OpenApple and ClosedApple are NOT mapped to the game controller buttons on the II+.

1. control keys: YES
1. shift keys: YES
1. arrow keys: YES. II+ only has left and right arrow keys, so only those work on II+.
1. Escape key: YES
1. RESET key: YES.
1. REPT key: no. But we don't need this, autorepeat driven by real keyboard.
1. Backspace - YES.

### Apple IIe

1. control keys: YES
1. shift keys: YES
1. arrow keys: YES. All 4 arrow keys.
1. Escape key: YES
1. RESET key: YES.
1. REPT key: no. But we don't need this, autorepeat driven by real keyboard.
1. Backspace - YES. Mapped to 0x7F 'Delete' on IIe keyboard.
1. OA and CA - YES.

## ROMs

For now, ROMs are included in the source tree and distribution.


### More Character ROM stuff:

https://github.com/dschmenk/Apple2CharGen

Info on the character set:

https://en.wikipedia.org/wiki/Apple_II_character_set

## Display

The display is managed by the cross-platform SDL3 library, which supports graphics
but also sound, user input, lots of other stuff. It is a library intended for
video games, and in thinking about all the things that make up a good emulator, 
emulators fit the video game category quite closely.

GSSquared supports three video display engines:
* NTSC - a highly accurate rendering of Apple II video as it would have worked on a composite monitor of the time.
* RGB - the exact Apple IIgs mapping of Apple II video to RGB
* Monochrome - green, amber, or white 'high bandwidth' monochrome display.

At this time all video modes below work with all display engines.

### Text Mode

Supports 40-column normal, inverse, and flash;
80-column with standard and alternate character sets, and on GS, all 8 international character sets.
Also, GS2 supports Videx VideoTerm in the II+ model.

### Low-Resolution Graphics

Wwork with screen 1, 2, split-screen, and full-screen.

### High-Resolution Graphics

Works with page 1, 2, split-screen, and full-screen.

### Double Modes

Double lo-res and Double hi-res supported fully, and with all video engines.

## Disk Drive Interfaces

### Disk II Controller Card Design and Operation

https://www.bigmessowires.com/2021/11/12/the-amazing-disk-ii-controller-card/

We simulate time passage by counting cycles, not by counting realtime. This way floppy disks can be simulated at any speed. If you are running in one of the faster modes, the simulated disk drive "speeds up".

We support reading and writing to Disk II images per the table above. Writes are buffered in memory until you unmount the disk, at which time if there have been changes you are given the opportunity to save them back to the original image file.

### RAMfast SCSI Interface

I'm not sure if there is any benefit to emulating the RAMfast SCSI interface. An Apple II forum
user suggested it, but, I don't think there was any software that required it specifically?
(There was also a suggestion to emulate a SecondSight, and, that would be way easier and
make more sense).

### Pascal Storage Device

There is a standardized device interface that ProDOS supports, called Pascal. This may be what
became known as the SmartPort / ProDOS block-device interface. See above.

# Code Organization

## Frames

The system is organized around Frames, and an event loop. You can think of a frame as a video update cycle, occuring 60 times per second.

This is basically modern video game development organization.

The main system loop does the following:

* execute 1/60th of a second's worth of CPU instructions, based on current "speed" setting.
* process events (keyboard, mouse inputs, etc)
* generate audio frames
* generate video update
* sleep precisely until start of next frame

CPU instructions are emulated at the equivalent of 500 times faster than the original Apple II, which leaves plenty of time in that 1/60th second to do all the other work above. This allows the overall cycle rate to be very accurately timed as far as human perception goes.

And that's the core of this design - organized around frames like this, the code is executing very differently to how it was in a real II, but the way we perceive it is indistinguishable.

This organization also makes the system very portable, and, very accommodating to host systems of different speeds.

## Platforms

When a virtual Apple II is created and 'powered on', there are several stages.

1. Platform is initialized.
1. CPU structure is created.
1. MMU is created
1. Memory allocated and initial memory map assigned.
1. Devices are initialized and 'registered'.
1. The CPU is 'powered on'.

A "Platform" is a specific selection and combination of: CPU type, devices, and memory map.
Because so much of the Apple II series share significant systems and subsystems,
we define and implement those with granularity and "compose" them together when a 
particular virtual machine is set up.

As one example, the Apple II, II+, IIe, IIc, and IIgs all share the same $C030 primitive
speaker toggle. So, we write that only once. When we 'boot' a IIgs OR a IIe, we call
"init_mb_speaker" against the Computer structure, and that hooks in this functionality using memory mapping routines in the MMU.

Similarly with CPUs. You want a IIe with nmos 6502? Great. Select those components.
You want a IIe with original firmware but 65c02 (it might not run very well but try it anyway!) You can define a platform to do exactly that.

Want to create your own platform that never existed in reality and experiment?
Go for it! Design the next generation of Apple II! Then maybe someone can take your
idea and implement in hardware.

Contribute new devices back to the project for fun and profit! Well, for fun anyway.

## Computer

The "Computer" serves as a nexus and container for the various components of the system - the CPU, MMU, Devices, VideoSystem, and AudioSystem.

## CPU

The CPU is the central processing unit, executing 6502/816 instructions.

## MMU

The MMU manages memory for the system, including allocating main system memory, and performing memory mapping at the request of devices.

Memory is mapped in units of Pages. In the Apple II/II+/IIe world, a page is 256 bytes as this is typically the smallest unit of memory mapping.

A page has 5 map controls:
* Read memory - maps reads for the page to a certain memory block inside the emulator
* Write memory - maps writes to a memory block
* Read Handler Callback - maps reads to a callback handler
* Write Handler Callback - maps writes to a callback handler
* Shadow Callback - maps writes to a SECOND callback handler if desired.

As an example, the Shadow callback function is used by the display system to be notified whenever a byte in video memory (text, or hires) is modified - this sets flags to update that part of the display. (An optimization, we don't redraw the entire screen every frame, only those areas changed).

The page C000-C0FF is further broken down into 256 Softswitch locations. When a device initializes, it registers a read and/or write handler on I/O locations it wants to control.

There is also a special callback handler for managing C800-C8FF memory space by a device.

Whenever the MMU is asked to read or write a memory address, it looks up the page map entry for the page in question and handles according to programmed map controls.

## VideoSystem

The VideoSystem mediates video updates between display modules (e.g. motherboard video or Videx) and the underlying SDL. Typically, a display system will maintain a texture in the host video RAM. When frame update is complete, VideoSystem completes the frame by drawing the texture into the display window.

## Devices

Devices are components of an emulated system aside from the above ones. There are two categories: motherboard devices, and slot devices. A slot device is one which would go into an expansion slot in a real Apple II (Disk II, Videx, Memory Expansion Card, Language Card). A motherboard device is one that is typically embedded on the motherboard (e.g., keyboard, game port).

Users will be able to edit the Slot Cards in a system, and you can have multiple instances of a slot device in a system. For example: you can put two Mockingboards into a system though I am unaware of any software besides Ultima V that acutally uses it. Or two Disk II controllers for four Disk II drives.

You can only have one instance of each type of motherboard device in a system, and these are not user-changeable. These are defined in code as part of the "Platform" data structures.

When a device initializes it hooks into the system by setting callbacks for its I/O locations in the MMU. It may also hook in callbacks for C800, shadow memory, etc. Whatever it needs.

Devices may also register a "frame handler" which is called once per video frame.
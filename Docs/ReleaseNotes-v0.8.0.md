# GSSquared v0.8.0

I am pleased to announce the release of **GSSquared v0.8.0**.

GSSquared is an Apple II series computer emulator.

## Supported platforms

- Apple ][
- Apple ][ Plus
- Apple //e
- Apple //e Enhanced
- Apple //e Enhanced with 65816 CPU and Super Hires Video
- Apple IIgs ROM 01

## Pre-built binaries

- **macOS** (Intel and Apple Silicon)
- **Windows 10+**
- **Linux AppImage** (should run on many Linux distros; tested on Ubuntu 22)

---

This release brings full **3.5" floppy support** on the Apple IIgs, a much-improved **Control Panel** for managing disks, and better handling when you quit with unsaved disk changes.

## Features

- **Apple IIgs 3.5" floppy drives** now support **WOZ disk images** for reading and writing, including many copy-protected titles.

- **5.25" and 3.5" floppy emulation** has been significantly improved. Copy-protected WOZ images that previously failed should now work, and changes to mounted disks can be saved back to the original image file when you unmount.

- The **Control Panel** has new, clearer drive icons for 3.5" AppleDisk drives and hard drives. Icons match the machine you are emulating — Disk II on early Apple II models, AppleDisk on the //e and IIgs.

- **BazFast**, the built-in SmartPort hard drive, can now mount **multiple disk images at once**. Drag a `.pmap` file onto a BazFast icon to load a whole set of volumes in one step — handy for multi-disk setups like a hard-drive collection with separate ProDOS volumes.

- When you **quit or power off**, GSSquared now properly asks whether to save any disks that have unsaved changes.

- You can launch GSSquared directly into a specific machine from the command line: `-p PLATFORM` (for example, `-p iigs`). Closing the window will then exit the emulator automatically.

## Accuracy

- Floppy disk timing and head movement are more accurate, which helps games and copy-protection schemes that depend on precise disk behavior.

- Mounting a disk image on the wrong kind of drive (for example, a 140K floppy image on a hard drive) now shows a clear error instead of failing silently.

## Bug Fixes

- The debugger open/close key is now **F10**, so it no longer conflicts with keys used inside Apple II software.

- Fixed a Windows build issue.

## Internals

- The floppy and IWM disk code has been reorganized and rewritten as a foundation for future storage improvements.

- Documentation for disks, WOZ images, and BazFast has been updated.

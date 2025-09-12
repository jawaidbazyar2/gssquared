# Apple IIgs

## Key Differences

New Hardware

## New Hardware

The GS features a variety of new hardware.

### Ensoniq 5503 DOC

See Ensoniq.md. Accessed via new $C0xx registers.

### Super Hi-Res Video Modes

Pretty straightforward. Linear video buffer, pixel values look up 4096 colors (12-bit color value) in a palette table.

### Standard Apple II text with Color

Text modes have a foreground and background color applied from a choice of 16 colors. Configured with new $C0xx registers.

### RGB output even for 8-bit II modes

RGB output. (Partially implemented now, I think).

### ADB (Apple Desktop Bus)

A shared serial bus (precursor of the USB concept) for keyboard, mouse, potentially other devices.

### 2 built-in serial ports via Zilog SCC chip 

Zilog SCC chip supports 2 built-in serial ports.

### AppleTalk / LocalTalk networking (via Zilog chip)

This is likely just driven by firmware. If we wanted to support AppleTalk, we'd need to decode packets and convert to ethernet.

### Built-in 5.25/3.5 Controller

Has the IWM chip - Integrated Woz Machine. Looks like a Disk II in Slot 6, but how is the 3.5 drive handled? What is that protocol?

Depends on the drive. UniDisk has a built-in CPU controller operating at 2mhz. AppleDisk 3.5 has no built-in controller.

You can 'download' custom code to run on the UniDisk. With an AppleDisk this "runs in host memory" according to UniDisk 3.5 TechPub #5.

UniDisk details are discussed in Apple IIgs Firmware Reference.

This discusses the registers in detail:

https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/storage/disks/IWM-Controlling%20the%203.5%20Drive%20Hardware%20on%20the%20Apple%20IIGS.pdf


### Built-in Mouse via ADB port

Likely just handled with firmware.

### Keyboard Type-ahead buffer (16 chars)

The Keyboard is ADB but there is a simulation of the $C000/$C010 scheme. Likely built on top of the ADB interface.

## 65816

Test suite for the 65816.

https://forums.nesdev.org/viewtopic.php?t=24940


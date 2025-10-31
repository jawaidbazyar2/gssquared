# Apple IIgs

## Key Differences

New Hardware

## New Hardware

The GS features a variety of new hardware.

### 65816 Processor

This looks like an excellent resource:

http://www.6502.org/tutorials/65c816opcodes.html

Test suite for the 65816.

https://forums.nesdev.org/viewtopic.php?t=24940

First one to start using:
https://github.com/gilyon/snes-tests



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


The ADB Protocol is fairly straightforward. The 65816-side registers are documented in Apple IIGS Hardware Reference Chapter 6.  The microcontroller side is discussed here:
https://llx.com/Neil/a2/adb.html

however, I don't think there is any reason to emulate anything other than the keyboard and mouse. We do need to handle generic / general-purpose ADB commands/responses, but again the protocol is very simple. There are several types of interrupts the ADB can generate.

The C000/C010 keyboard logic should be fed from the ADB module.

Have an ADBGLU class, that acts as a middleman between the devices and the "motherboard". Need to be able to queue stuff. I am guessing the GLU handles the 16-character typeahead buffer? Or is it in the keyboard. this is unclear, but unlikely to be relevant for our purposes.

The Keyboard is ADB but there is a simulation of the $C000/$C010 scheme. The ADB GLU is where those switches live in a GS.


### Memory Map

The IIgs has a 16MB (24-bit) address space. All I/O is done in the legacy $C000 space.

Use 4K pages in MMU_GS.

Apple IIe emulation is run in banks $E0 and $E1. This is "slow ram", always runs @ 1MHz.
So have page maps that map these banks to the Apple II MMU.
Banks $00 - $01 are 128K built-in in ROM01 (plus E0/E1 = 256K). Banks $00 - $0F for ROM03 (1.125M total)
Banks $00 and $01 are "shadowed" to banks $E0 and $E1 if shadowing is enabled - this is exact use of shadow handler for those pages.

Fast ROM is banks $F0 to $FF. (1MB total).


Shadowing: not all the regions in $00/$01 are shadowed:
https://retrocomputing.stackexchange.com/questions/5555/apple-iigs-hardware-implementation-of-ram-shadowing

```
0 - 0400...07FF - Text1
1 - 2000...3FFF - HGR1
2 - 4000...5FFF - HGR2
3 - 2000...9FFF - Super High-res
4 - 2000...3FFF - Double High-res ($01/2000...)
5 - Unused
6 - C000...DFFF - Switches/Slot Memory
7 - Speed indicator
```

### Bus Timing

CPU normally runs at 1MHz (speed set to 1MHz hard), 2.8MHz, (or 7MHz / 14MHz in our accelerated modes). When an access is done to slow memory, we need to wait an appropriate amount of time to synchronize with the 1MHz clock, then execute the 1MHz cycle access at 1MHz speed.

So we'll need to know in incr_cycles if it's a 1MHz access or 2.8MHz access. (Also, the GS can control its own cpu speed.) We will want to generalize that mechanism to work at 2.8 but also 7.1MHz.

This will be done by tracking a 1MHz reference clock all the time.  

"remember to allow an additional 10% in total cycle time to account for RAM refresh delays". More about this on Page 24 hardware reference. "FPI can execute ROM cycles while RAM refresh cycles are occurring". Occur approximately every 3.5 microseconds and reduce the 2.8MHz processing speed by about 8% for programs that run in RAM. What is 3.5 microseconds.. about 50 14Ms we need to delay cpu access to RAM by 4 14M's. This applies only to the Fast Ram. The slow ram is refreshed by the video circuitry during phase low just like an Apple II.

There are certain registers inside the fPI (fast processor interface) that can be read at high speed: dma register; speed register; shadow register; interrupt ROM ($C071-$C07F) does not slow the system.
State Register and Slot ROM select register are written at 1MHz and read at 2.8MHz.


Great Discussion on the Apple2Inifinitum slack #kegs-emulator channel:
Personally, I don't think there's much point in implementing the refresh timing without implementing the other stuff (i.e. stretch cycles and sync between fast and slow). You'll end up something theoretically closer, but it will still look off. (edited) 
I can paste code here, but it'll probably add to the confusion.
Fundamentally, you need to implement 3 clocks - a fast clock, a slow clock with stretched cycles and a refresh clock. The refresh clock is every 10 fast clocks. The refresh clock gets delayed if it overlaps between a slow and a fast. Refresh clock is "free" if it happens on a slow cycle or a FPI register cycle.

There's even some code of clocking implementation from CrossRunner.


### IWM (SmartPort)

IWM/SmartPort can control both 5.25" disk II drives as well as 3.5" drives. 

The UniDisk is a "smart" drive that has built in cpu and is accessed via a block interface protocol.
The Apple Disk 3.5 is a dumb drive that looks a lot like the Disk II and is managed low-level like the Disk II.

We will implement the 5.25" and the Apple 3.5" drive - i.e. the dumb drives. (That's what I've got for real hardware).
Then don't have to bother trying to support downloadable code into UniDisk.

This means we will have to implement something like the current Disk II code, but to work with the 3.5" drive low level format.

Sectors are 512 bytes. And of course the disks are double-sided. 

```
The standard layout for a single side is as follows: 
Zone 	Tracks	Sectors/Track	Bytes/Track
1	0–15	12	6,144
2	16–31	11	5,632
3	32–47	10	5,120
4	48–63	9	4,608
5	64–79	8	4,096
```

Now, we -could- cheat and act like we have slot 5 mapped to our own card which is our dummy pdblock drive. That would be the fastest way to get an 800k drive working here. But we'd need the above to eventually deal with copy-protected .WOZ images of 3.5's.

The IWM registers look just like Disk II registers and sit in $C0E0-C0EF; plus a couple other control registers elsewhere.

Start with pdblock2 + disk ii, and then implement the iwm fully as a Phase 2. This might be where we implement .WOZ support fully also.


### Game I/O

The same as the Apple II. Can use the gamecontroller module as-is.

### Realtime Clock

$C033 and $C034 work together to provide access to the IIgs Realtime clock.

Bits 7-4 of $C034 manage a protocol to communicate with the clock; bits 3-0 control the Border Color on the IIgs display.
Page 169 of the hardware reference.

The RTC also holds the more general purpose "battery ram". There are a total of 256 bytes of information present in this. These values will need to be stored to a data file on host disk whenever they're modified. This is a detailed description of the battery ram and its contents:

https://groups.google.com/g/comp.sys.apple2/c/FmncxrjEVlw

- Excellent deep dive into IWM
https://llx.com/Neil/a2/disk


### 2 built-in serial ports via Zilog SCC chip 

Zilog SCC chip supports 2 built-in serial ports. The registers for this chip are: $C038 -9 (scc command channel B and A), and $C03A-B (scc data channel b and a).


### AppleTalk / LocalTalk networking (via Zilog chip)

This is likely just driven by firmware. If we wanted to support AppleTalk, we'd need to decode packets and convert to ethernet or something like that.

### Built-in 5.25/3.5 Controller

Has the IWM chip - Integrated Woz Machine. Looks like a Disk II in Slot 6, but how is the 3.5 drive handled? What is that protocol?

Depends on the drive. UniDisk has a built-in CPU controller operating at 2mhz. AppleDisk 3.5 has no built-in controller.

You can 'download' custom code to run on the UniDisk. With an AppleDisk this "runs in host memory" according to UniDisk 3.5 TechPub #5.

UniDisk details are discussed in Apple IIgs Firmware Reference.

This discusses the registers in detail:

https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/storage/disks/IWM-Controlling%20the%203.5%20Drive%20Hardware%20on%20the%20Apple%20IIGS.pdf

### Debugger

Technically not hardware - but, the various debugger modules must be updated to support 24-bit address and 16-bit data values.
This should involve cleanup/refactor of the trace, disasm, and monitor module more specifically. 

1. "Address" should be able to handle any size value up to 32 bits.
1. allow "bp" and "watch" on single address
1. monitor commands must be enhanced to support 32-bit addresses.
1. convert iiememory info section to be a callback like the others.

### Trace

1. all this to make room for decoding addresses -> symbols, as well as 16-bit widths for: A, X, Y, SP, PC.  That's two more characters for each of those.
1. have debugger BP -before- instruction execute. i.e. if the PC is going to be XXXX, then BP before it executes. And highlight that instruction in the display.
1. [ ] instead of showing whole cycle, what if we just display last say 6 digits of cycle. That would be enough to show differences while not taking up too much screen.
1. [x] make generation of the lines much easier by having a line class that tracks a horizontal 'cursor' inside the line.
1. pack opcode and operand into a single 32-bit value, instead of 32-bit plus 8-bit.
1. the other fields in the trace record are otherwise the right size.
1. have a trace decoder base class plus two (maybe 3?) derived classes: 6502, 65c02, 65816. 
1. Same for disassembler.
1. in 6502/65c02 mode, show SP as just a byte, to free up room.
1. [ ] use p flags e/m/x to determine trace formatting in 65816 class.

## Display Parameters

on my GS RGB SHR is 7.75 inches across and regular hires is 7.75 inches across. So they are the same horizontal beam scan path, but the SHR just clocks the pixels out a bit faster. We'll have to hope the scaling doesn't look too bad. I think the Videx is ok (640 pixels). But, this may be where the aspect ratio stuff comes in. This is an interesting question actually.. 

# Development Roadmap

We can implement in this order.

1. 65816 [initial version done]
1. text mode enhancements (colored text / border)
1. mmu
1. ADB
1. SHR
1. IWM
1. Ensoniq

65816 emulation mode - the key element here is going to be to switch all the functions to use auto to allow us to switch from 8-bit to 16-bit more easily. By using auto, the compiler will .. uh, automatically.. pick the right data types based on calling parameters, stuff like that. CPU done.

So I jumped ahead and did SHR stuff. Got it in the dpp framework, now need to implement it in vpp (VideoScanner). Then go back to text/borders.

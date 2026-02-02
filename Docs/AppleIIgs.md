# Apple IIgs

## Key Differences

New Hardware

## New Hardware

The GS features a variety of new hardware.

Test Suite for Emulators generally:

http://krue.net/truegs/


# Development Roadmap

We can implement in this order.

[x] 65816 [initial version done]  
[ ] Accurate Cycle Timing (FPI / Mega II / Fast-Slow)  
[x] mmu (done-ish but might still be bugs)  
[x] RTC / BRAM  
[x] ADB  
  [x] Keyboard  
  [x] Mouse  
[x] New Display Modes
  [x] SHR  
  [x] text mode enhancements (colored text / border)  
[x] Game controller   
[x] Interrupts  
[ ] IWM  
[ ] Ensoniq  
[ ] Zilog SCC 8530  
[ ] ROM03    

65816 emulation mode - the key element here is going to be to switch all the functions to use auto to allow us to switch from 8-bit to 16-bit more easily. By using auto, the compiler will .. uh, automatically.. pick the right data types based on calling parameters, stuff like that. CPU done.

So I jumped ahead and did SHR stuff. Got it in the dpp framework, now need to implement it in vpp (VideoScanner). Then go back to text/borders.


## 65816 Processor

This looks like an excellent resource:

http://www.6502.org/tutorials/65c816opcodes.html

Test suite for the 65816.

https://forums.nesdev.org/viewtopic.php?t=24940

First one to start using:
https://github.com/gilyon/snes-tests

## Accurate Cycle Timing

The CPU code will need to distinguish, for incr_cycles() purposes, between:
* internal cycles
* bus cycles, with a speed returned by the mmu

Hm, ARE there any purely "internal" cycles? Every CPU cycle hits the memory bus. 


## Ensoniq 5503 DOC

See Ensoniq.md. Accessed via new $C0xx registers.

## New Display Modes

### SHR - Super Hi-Res Video Modes

Pretty straightforward. Linear video buffer, pixel values look up 4096 colors (12-bit color value) in a palette table.

[ ] Implement Fill Mode  

### Standard Apple II text with Color

Text modes have a foreground and background color applied from a choice of 16 colors. Configured with new $C0xx registers.

### RGB output even for 8-bit II modes

Exact Apple IIgs style RGB display for the Apple II video modes is done for hires, double hires.

[ ] Implement special case for crisp Lo-Res and Double Lo-Res in RGB mode  


## ADB (Apple Desktop Bus)

A shared serial bus (precursor of the USB concept) for keyboard, mouse, potentially other devices.


The ADB Protocol is fairly straightforward. The 65816-side registers are documented in Apple IIGS Hardware Reference Chapter 6.  The microcontroller side is discussed here:
https://llx.com/Neil/a2/adb.html

however, I don't think there is any reason to emulate anything other than the keyboard and mouse. We do need to handle generic / general-purpose ADB commands/responses, but again the protocol is very simple. There are several types of interrupts the ADB can generate.

The C000/C010 keyboard logic should be fed from the ADB module.

Have an ADBGLU class, that acts as a middleman between the devices and the "motherboard". Need to be able to queue stuff. I am guessing the GLU handles the 16-character typeahead buffer? Or is it in the keyboard. this is unclear, but unlikely to be relevant for our purposes.

The Keyboard is ADB but there is a simulation of the $C000/$C010 scheme. The ADB GLU is where those switches live in a GS.


## Memory Map

The IIgs has a 16MB (24-bit) address space. All I/O is done in the legacy $C000 space.

Shadowing: normally, only banks 00/01 shadow - and that's writes, and that's to banks E0/E1, and then, only portions of those banks, controlled by shadowing register.

00/01 runs at full speed, except shadowed writes, which slow down and sync to 1MHz. In native mode programs might only have I/O shadowing enabled.
E0/E1 runs at 1MHz, period. This is "slow ram", always runs @ 1MHz.

Apple IIe emulation is run in banks $00 and $01, but with shadowing to E0/E1 for I/O and video.  
This allows reads by 8-bit software to occur at full speed, but shadowed writes are synced to 1MHz.
There is various stuff in banks E0/E1 used by GS/OS, interrupt handlers, etc.

I'll need something to return the speed and sync of a r/w. Maybe just a simple flag "1mhz read/write", which is:
* any shadowed write in 00/01
* any read or write in E0/E1

So computer has a MegaII MMU, but also MMU_IIGS - the FPI. MMU_IIGS allocates its own Mega II MMU internally.

The CPU calls MMU_IIGS. Everything else is injected with RegMMU.
Unlike the IIe implementation, MMU_IIGS is a class that manages all its own softswitches etc., which exposes registers including:

* Shadow register
* New-Video register
* State register

MMU_IIgs also replicates the language card functionality, and have those registers, because that bank switching exists in banks 00/01.

New-Video, controls linearity or interleave of some memory in bank 1. Also, controls whether ALL even/odd banks are shadowed?

So have page maps that map these banks to the Apple II MMU.
Banks $00 - $01 are 128K built-in in ROM01 (plus E0/E1 = 256K). Banks $00 - $0F for ROM03 (1.125M total)
Banks $00 and $01 are "shadowed" to banks $E0 and $E1 if shadowing is enabled - this is exact use of shadow handler for those pages.

Fast ROM is banks $F0 to $FF. (1MB total).

The $C000 - $FFFF space in E0/E1 is controlled by the LC. The Shadow register bit 6 just controls whether a WINDOW in those locations in banks0/1 is present or not.
LC memory writes are NOT shadowed to E0/E1.

The ROM01 file is 128KB exactly, and maps to banks $FE $FF. ROM03 is 256KB and maps to banks FC-FF. It looks like $FF/C000 is various slot card firmware, including at $C800-$CFFF (I forget, did the IIe have stuff here too?)
$FF/D000 is AppleSoft etc. $F800 should be monitor ROM.. yep. All largely unmodified. So mapping this should be straightforward. Of course the reset/etc vectors are different.

Weirdly, there is ROM data at $C071-$C07F. The IRQ/BRK vector is 74 C0 ($C074). At that location in the ROM is this:

B8 5C 10 00 E1

which is CLV then JML $E1/0010. What? E1/0010 etc are interrupt vectors. What about apple II software that uses page 0? Ah, that is not shadowed.

JBrooks: "$C07x ROM is active in bank 0 & 1 when I/O shadowing is enabled, or after the CPU does a vector pull (interrupt) via $FFEx/FFFx"
We determined that we think C07x ROM is controlled purely by the I/O shadowing flag. 


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

ok. There is Fast RAM and Slow RAM. Slow RAM has the I/O stuff.

#### Fast RAM - banks $00 - $7F

Banks $02 - $7F are pure RAM (well, you -can- shadow them but this is rarely used in practice).
Banks $00 - $01 are half of legacy Apple II emulation. 

Use a 64KByte page size. Banks $00 and $01 get special read/write handler controlled by iigsmemory, that 
The other banks are basically always mapped directly to RAM and that's it. So the MMU function for these will
be the fastest performance. These handlers must handle Apple II LC bank switching; but also be able to set
these banks to pure-RAM operation just like $02 - $7F. iigsmemory will likely have the custom handlers for these
banks: native (what we'll call the "it's just ram" handler); and emulation (what we'll call what handles all the IIe memory mangling)

Runs at average of 2.5MHz speed and is affected by RAM refresh timing.

#### Fast ROM - banks $F0 - $FF

Runs at full 2.8MHz and is unaffected by RAM refresh timing.

#### Slow RAM - banks $E0 - $E1

These two banks are comprised of: display pages; I/O space. The I/O space mapping and ROM mapping for it are here and
handled the same way they are in a IIe.
All accesses in this space are clocked at 1MHz to share access with video scanner.

#### State Register - C068

this is a condensed version of all the bank0/1 memory map features, in one place. And these likely coincide with the ones iigsmemory will deal with.


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



## Game I/O

The same as the Apple II. Can use the gamecontroller module as-is.

## Realtime Clock

$C033 and $C034 work together to provide access to the IIgs Realtime clock.

Bits 7-4 of $C034 manage a protocol to communicate with the clock; bits 3-0 control the Border Color on the IIgs display.
Page 169 of the hardware reference.

The RTC also holds the more general purpose "battery ram". There are a total of 256 bytes of information present in this. These values will need to be stored to a data file on host disk whenever they're modified. This is a detailed description of the battery ram and its contents:

https://groups.google.com/g/comp.sys.apple2/c/FmncxrjEVlw

- Excellent deep dive into IWM
https://llx.com/Neil/a2/disk


## 2 built-in serial ports via Zilog SCC chip 

Zilog SCC chip supports 2 built-in serial ports. The registers for this chip are: $C038 -9 (scc command channel B and A), and $C03A-B (scc data channel b and a).


### AppleTalk / LocalTalk networking (via Zilog chip)

This is likely just driven by firmware. If we wanted to support AppleTalk, we'd need to decode packets and convert to ethernet or something like that.

## IWM (SmartPort) - Built-in 5.25/3.5 Controller

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

Now, we -could- cheat and act like we have slot 5 mapped to our own card which is our dummy pdblock drive. That would be the fastest way to get an 800k drive working here. But we'd need the above to eventually deal with copy-protected .WOZ images of 3.5's. (Turns out there are a fair number of titles that see GS floppy and just assume slot 6 and crash booting)

The IWM registers look just like Disk II registers and sit in $C0E0-C0EF; plus a couple other control registers elsewhere.

Start with pdblock2 + disk ii, and then implement the iwm fully as a Phase 2. This might be where we implement .WOZ support fully also.

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

1. [x] all this to make room for decoding addresses -> symbols, as well as 16-bit widths for: A, X, Y, SP, PC.  That's two more characters for each of those.
1. have debugger BP -before- instruction execute. i.e. if the PC is going to be XXXX, then BP before it executes. And highlight that instruction in the display.
1. [x] instead of showing whole cycle, what if we just display last say 6 digits of cycle. That would be enough to show differences while not taking up too much screen.
1. [x] make generation of the lines much easier by having a line class that tracks a horizontal 'cursor' inside the line.
1. pack opcode and operand into a single 32-bit value, instead of 32-bit plus 8-bit.
1. the other fields in the trace record are otherwise the right size.
1. have a trace decoder base class plus two (maybe 3?) derived classes: 6502, 65c02, 65816. 
1. Same for disassembler.
1. in 6502/65c02 mode, show SP as just a byte, to free up room.
1. [x] use p flags e/m/x to determine trace formatting in 65816 class.

## Display Parameters

on my GS RGB SHR is 7.75 inches across and regular hires is 7.75 inches across. So they are the same horizontal beam scan path, but the SHR just clocks the pixels out a bit faster. We'll have to hope the scaling doesn't look too bad. I think the Videx is ok (640 pixels). But, this may be where the aspect ratio stuff comes in. This is an interesting question actually.. 

## Interrupts

### VGC

| Register | Description |
|-|-|
| C023[7] | VGC Interrupt Status |
| C023[6] | One-second interrupt status |
| C023[5] | scanline interrupt status |
| C023[4] | external interrupt status |
| C023[3] | unused set to 0 |
| C023[2] | one-second interrupt enable |
| C023[1] | scanline interrupt enable |
| C023[0] | external interrupt enable |

7 is the OR of 4-6.

This is one of the clearer interrupt setups. 

| Register | Description |
|-|-|
| C032[7] | not used, set to 0 |
| C032[6] | Clear bit for one-second interrupt |
| C032[5] | clear bit for scanline interrupt |
| C032[4-0] | unused set to 0 |

Write a 0 into bit 6 or bit 5 to clear that interrupt. Writing a 1 has no effect.
Reading the video counters will also clear the ScanLine interrupt status bit!

To implement the one-second interrupt, we need a timer that works sort of like the Scheduler except is based on the 14M clock. (or is this based on 60 frames? i.e. is it ACTUALLY 1 second?).

To implement the scanline interrupt, VideoScannerIIgs will have to set the interrupt when it reads the scanline control byte. If the bit in SCB is 1, throw an interrupt. That seems easy enough.

* Other ADB

| Register | Description |
|-|-|
| C027[5] | data interrupt full |
| C027[4] | data interrupt enable |

This was unusually difficult because it's hard to find test examples that don't immediately crash and/or require tons of code tracing. but, it might be working?

### MegaII Mouse

* C041 - Int Enable - INTEN

| Register | Description |
|-|-|
| C041[7] | 0 |
| C041[6] | 0 |
| C041[5] | 0 |
| C041[4] | Enable 1/4 sec ints |
| C041[3] | Enable VBL ints |
| C041[2] | Enable Mega II mouse switch ints |
| C041[1] | enable Mega II mouse move ints |
| C041[0] | enable Mega II mouse operation |

So I don't think the GS uses anything here except bits 3-4: vbl and quarter-second.

Again, the ideal place to insert the VBL interrupt is in VideoScannerIIgs. (done)


## ROM03

There are a few relatively minor differences between ROM01 and ROM03.

1. ROM03 supports shadowing Text Page 2.

This is easily supported by checking the ROM type in shadow_bank_read/write.

1. It has more memory by default. This won't really matter for us.

This is irrelevant given our design (all fast RAM is one contiguous stretch)

1. It has an updated ADB Micro, that supports stickey keys


## Unique Peripheral Cards to Consider Implementing

There were a variety of unique/useful cards that really took advantage of the Apple IIgs capabilities. With any of these give consideration to utility - truly want to enable a broader software ecosystem that would really use this stuff.

### VoC - Video Overlay Card

Offered video overlay of IIgs graphics on an external video source, but also allowed a 320x400 and 640x400 mode using interlacing on the IIgs monitor. 

### SecondSight

Well, we know all about this. It added VGA graphics to the Apple IIgs, and had some cool JPEG, TIF etc picture viewer written by a guy named Jawaid. 

### VidHD and/or other upcoming cards

VidHD had some interesting and useful features. and the long-awaited AppleTini card promises a variety of very interesting video modes.

What if I implemented my thoughts for a video system, with a frame buffer living in fast ram and having video acceleration features like a "gpu" that implements quickdraw calls in paravirtualization.

### Virtual Memory - Memory Protection

Ahhh the holy grail. This deserves its own long writeup. But short version: memory protection integrated with GS/OS and GNO, and perhaps memory virtualization to allow combinations of apps > 16MB address space. (Could have much more physical ram behind the virtual window). But memory protection would be a huge win. 

### RAMfast SCSI

this was the fastest disk interface then (or now for that matter) since it supported DMA to any IIgs memory bank. Since we don't have a real bus, trying to implement this would be really for nostalgia sake. However, having a Smartport-compliant "Hard Disk" card option that makes it very easy to manage lots of large disk partitions is something I need. The current pdblock2 has been fine but the 2-drive limit and no removability support makes it awkward to use on a GS.

This would need a nice clean UI. Drag and drop for sure. More generally, the ability to save disk mounts in config so they persist across sessions. 


## Source code that reads the character rom out of a IIgs

from Ian Brumby:

For those following the thread on comp.sys.apple2, we now have some documentation on four previously unknown softswitches: $C02C, $C06D, $C06E, $C06F. And it turns out it was on my hard drive the whole time. Doh!
	
```
  lda #0
buildloop	anop
	sta thechar
	tax
	lda packbuffer,x
	and #$00FF
	bne isachar
	brl nextchar

isachar	anop
	lda thechar
	xba
	ora thechar
	ldx #$76
fillscreen anop
	sta $E00400,x
	sta $E00480,x
	sta $E00500,x
	sta $E00580,x
	sta $E00600,x
	sta $E00680,x
	sta $E00700,x
	sta $E00780,x
	dex
	dex
	bpl fillscreen

	lda thechar
	asl a
	asl a
	asl a
	ora #$8000
	tay		;offset into buffptr+$8000

	shortm
	sta $E0C00C	;40COL
	sta $E0C00F	;ALTCHARSET

offset	equ $A1D5
	ldx #offset
	lda #0
	sta thevline
	phb
	lda #$E0
	pha
	plb
	bra readROM

tryagain	anop 	;missed our data: wait and retry
	lda #0	;turn off test mode while waiting to...
	sta $C06D-offset,x	;...minimize chance of a reset...
	sta $C06E-offset,x	;...happening while it's enabled.
	plp		;give interrupts a chance
	phx		;waste time
	plx
	phx
	plx
	phx
	plx
	phx
	plx
readROM	anop
	php		;disable interrupts while in test mode
	sei
	lda #$DA	;password byte 1
	sta $C06F-offset,x
	lda #$61	;password byte 2
	sta $C06F-offset,x
	lda #$40	;enable test mode $40
	sta $C06D-offset,x
tryonce	anop
	lda $C02F	;horizontal count
	asl a	;low bit of vcount now in carry
	and #%11100000
	eor #%10100000	;want hcounts +/-8usec from left edge
	bne tryonce
	lda $C02E	;vcount
	rol a	;roll in low bit from hcount
	bcc tryagain	;want top 192 visible lines only
	bmi tryagain
	eor >thevline	;compare to desired v position
	and #$07	;...only care about low 3 bits
	bne tryagain
	lda $C02C-offset,x          ;read CHARROM finally
	sta [<buffptr],y
	lda #0	;turn off test mode
	sta $C06D-offset,x
	sta $C06E-offset,x
	plp		;reenable interrupts
	iny

	lda >thevline
	inc a
	sta >thevline
	cmp #8
	blt readROM
	plb
	longm
nextchar	anop
	lda thechar
	inc a
	cmp #$100
	bge lastchar
	brl buildloop

lastchar	anop
```

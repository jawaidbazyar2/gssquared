# AppleMouse

The AppleMouse card is fairly complex considering what it does. It's got an onboard microprocessor (6805), a 6520 chip (according to one thing I read). 6520 is like 6522 without timers.

The programming references for it specify use of a bunch of standardized firmware vectors. It's got a 2K ROM for C800-CFFF.

So there are two options: ParaVirtual, or emulate the hardware.

https://github.com/freitz85/AppleIIMouse/blob/master/AppleMouse.txt

```
ClampMouseX ->  W 4 Bytes   LLX, LHX, HLX, HHX
ClampMouseY ->  W 4 Bytes   LLY, LHY, HLY, HHY
SetMouse    ->  W 1 Byte    MODE
ServeMouse  ->  R 1 Byte    STATUS
ReadMouse   ->  R 6 Bytes   LX, LY, HX, HY, STATUS, MODE
ClearMouse  ->  W 4 Bytes   LX, LY, HX, HY
PosMouse    ->  W 4 Bytes   LX, LY, HX, HY
HomeMouse   ->  R 4 Bytes   LLX, HLX, LLY, HLY
                W 4 Bytes   LX, LY, HX, HY

A0-A3:

Write:
0x00    Position:   LX, HX
0x01                LY, HY
0x02    BoundaryX:  LLX, LHX
0x03                HLX, HHX
0x04    BoundaryY:  LLY, LHY
0x05                HLY, HHY
0x06    State:      MODE
0x07    Set to Input

Read:
0x08    Position:   LX, HX
0x09                LY, HY
0x0a    BoundaryX:  LLX, LHX
0x0b                HLX, HHX
0x0c    BoundaryY:  LLY, LHY
0x0d                HLY, HHY
0x0e    State:      STATUS
0x0f    Reset to default
```

These are 6805 microcontroller register indexes. 

* PA: 

PA0-7 on the 6520 is connected to the microcontroller data lines D0-7.

* PB: likely set for output direction

PB4-7 are connected to the microcontroller PC0-3 lines - Port C I/O lines.
I suspect these are the register selects.
PB1-3 are connected to PB1-3 but also pulldowns, and, also A8-A10 on the EPROM (hmm). So this is apparently used to bankswitch the ROM.

PB0 is connected to "sync latch" from some clock related chip.

CB1 is tied to 6805:T to +5 with a pull-up. T is timer, unused, so in short this is just always '1'. But also tied to 6805:/INT pin 2

CB2, CA1, CA2 are NC/unused.

A0-1 are connected to 6520 RS0-1. 

IRQ connects to PB6 on 6805.

R/W to the 6805 is determined by the address. I.e. address 0-7 is reads, 8-F is write.

On a read, put the value from the MC into PIB. On a write, we use the value in ORB.

Here's a disassembly of the ROM:

https://github.com/freitz85/AppleIIMouse/blob/master/mouse%20rom.s

This is a project which emulates the Apple Mouse II card pretty closely:

https://www.applefritter.com/content/a2usb-apple-ii-usb-mouse-interface-card-emulation


ok I'm going to guess the following:

BoundaryX provides a range from L to H - low value to high value. And a similar range for Y low to high.
This clamps the mouse Position to be inside that range. Simple enough.
What we need is the State: MODE and STATUS values.

The position X and Y can range from -32768 to 32767 (signed 16 bit integer). "Are normally clamped to range $0 to $3FF (0 to 1023 decimal).

ah ok these are in the manual. The Mode byte:

| Bit | Function |
|-----|----------|
| 0 | Turn mouse on |
| 1 | Enable interrupts with mouse movement |
| 2 | Enable interrupts when button pushed |
| 3 | Enable interrupts every screen refresh |
| 4-7 | Reserved |

if you write value of $01 to mode, it's "passive mode" - i.e., no interrupts.

Status

| Bit | Function |
|-----|----------|
| 7 | Button is down |
| 6 | Button was down at last reading |
| 5 | x or Y changed since last reading |
| 4 | Reserved |
| 3 | interrupt caused by screen refresh |
| 2 | interrupt caused by button press |
| 1 | interrupt caused by mouse movement |
| 0 | Reserved |

Oh, somewhere we need a register for a mouse click?
What is SERVEMOUSE?

if you use an interrupt mode, the interrupt handler must call SERVEMOUSE as part of that - it determines whether interrupt was caused by mouse so we can call READMOUSE if it was.

So what clears the interrupts? Just reading the STATUS byte perhaps?

## 6805

/RESET - system bus reset
/INT - tied to pullup along with 6520:CB1 (which seems to be an input only). Not likely this is used anywhere. But make available to read.


## 6520

6520 has these registers:
| Register | Function |
|-----|----------|
| CRA | Control Register A |
| DIR | Data Input Register |
| DDRA | Data Direction Register A |
| DDRB | Data Direction Register B |
| PIBA | Peripheral Interface Buffer A |
| PIBB | Peripheral Interface Buffer B |
| DDRB | Data Direction Register B |
| ISCB | Interrupt Status Control B |
| CRB | Control Register B |
| ORA | Peripheral Output Register A |
| ORB | Peripheral Output Register B |

The two RS lines (RS0/1) and CRA/CRB select various 6520 registers to be accessed.
RS0 = A0, RS1 = A1. 

There are pins RS0/1 on the 6520 - Register Select. There are only two of these, so there are only four register locations.

The 6520 IRQ lines are not used. IRQ is connected to the MC6805.

### CRA and CRB

| | 7 | 6 | 5  4  3 | 2 | 1 0 |
|--|--|--|--|--|--|
| CRA | IRQA1 | IRQA2 | CA2 Control | DDRA Access | CA1 Control |
| CRB | IRQB1 | IRQB2 | CB2 Control | DDRB Access | CB1 Control |

When Bit 2 = 0 (see below), access DDR. When Bit 2 = 1, access PIB.

CA1/CB1 are interrupt input pins only.
CA1 is NC.
CB1 is tied to pullup so it just always high. Should always read as 1.
CA2 is NC, CB2 is NC.

Cx2 control: bit 5 = 1, output mode "ca2 can operate independently to generate a simple pulse each time the microprocessor reads the data on the peripheral port". Since Cx2 is NC we can disregard the values of the CA2 and CB2 control bits.
CR bit 1 (CA1 high bit) is 0 for negative transition, 1 for positive transition. These values are unused.

This is covered on Page 2-29 of the 6520 data sheet.

### Register Access

| A2 Bus Addr | Register Address | RS1 | RS0 | CRA (bit 2) | CRB (Bit 2) R/W=H | R/W = L | |
|--|--|--|--|--|--|--|--|
| C080 | 0 | L | L | 1 | - | Read PIBA | Write ORA |
| C080 | 0 | L | L | 0 | - | Read DDRA | Write DDRA |
| C081 | 1 | L | H | - | - | Read CRA | Write CRA |
| C082 | 2 | H | L | - | 1 | Read PIBB | Write ORB |
| C082 | 2 | H | L | - | 0 | Read DDRB | Write DDRB |
| C083 | 3 | H | H | - | - | Read CRB | Write CRB |

(But also addresses C084-7, C088-B, C08C-F)


# Paravirtualization Approach

Apple2TS uses a paravirtual approach for its mouse implementation. Which is interesting, however, the device is still fairly complex, with interrupts, multiple registers, etc etc.

There probably should not be any software that directly accesses the hardware as Apple started pushing devs hard on this at that time.

# Hardware Emulation Approach

Practically speaking we only need to emulate the 6520 interface.

# Screen Holes

```
$0478 + slot Low byte of absolute X position
$04F8 + slot Low byte of absolute Y position
$0578 + slot High byte of absolute X position
$05F8 + slot High byte of absolute Y position
$0678 + slot Reserved and used by the firmware
$06F8 + slot Reserved and used by the firmware
$0778 + slot Button 0/1 interrupt status byte
$07F8 + slot Mode byte
```

# ROM Mapping

the first 256 bytes of the ROM seem to be $Cn00-$CnFF. And perhaps then the whole 2K ROM is also at $C800-$C8FF. However it is unclear how the $C800 space is mapped in. The IOSTROBE line is connected into a small PAL chip, and this may have an output on pin 17 that goes to PB0. 
Alright, so PB0 is mapped as an input (in the source code, for "sync latch" whatever that is).
Putting $3E into PB should then result in PB1-3 being E, and setting address bits of the ROM A8-A10 to 111. The low byte of PB is what ROM page to map in.

So, I don't think that it does. I think ORB is used to set the firmware bank that appears in $Cn00. There are no absolute references JSR/JMP inside $C800 in the firmware. And, in every bank, at $7B there is a "PLA" - i.e, here is the code in $Cn00:

```
C85B   B9 83 C0   LC85B     LDA $C083,Y
C85E   29 FB                AND #$FB
C860   99 83 C0             STA $C083,Y
C863   A9 3E                LDA #$3E
C865   99 82 C0             STA $C082,Y
C868   B9 83 C0             LDA $C083,Y
C86B   09 04                ORA #$04
C86D   99 83 C0             STA $C083,Y
C870   B9 82 C0   LC870     LDA $C082,Y
C873   29 C1                AND #$C1
C875   1D B8 05             ORA $05B8,X
C878   99 82 C0             STA $C082,Y
C87B   68                   PLA
```

There is that same PLA in almost every bank. This is because $C878 is setting the bank number to 111 (7) which means ROM $700-$7FF. So this falls through to this:

```
CF7B   68                   PLA
CF7C   D0 82                BNE LCF00
CF7E   A9 00      LCF7E     LDA #$00
CF80   9D B8 05             STA $05B8,X
CF83   48                   PHA
```

This is really freakin crazy. They do all this just to save some address decode logic? Or was there some other purpose?

# BazMouse

ok, I think it may be significantly simpler to write my own mouse card setup.

| Screen Hole | Description |
|--|--|
| $478 + n | Low byte of X position |
| $4F8 + n | Low byte of Y position |
| $578 + n | High byte of X position |
| $5F8 + n | High byte of Y position |
| $678 + n | Reserved |
| $6F8 + n | Reserved |
| $778 + n | Button and interrupt status |
| $7F8 + n | Current mode |

| Address | Name |
|--|--|
| C0s0 | RW | Position XL |
| C0s1 | RW | Position XH |
| C0s2 | RW | Position YL |
| C0s3 | RW | Position YH |
| C0s4 | RW | Clamp X Low Bound Low Byte |
| C0s5 | RW | Clamp X Low Bound High Byte |
| C0s6 | RW | Clamp X High Bound Low Byte |
| C0s7 | RW | Clamp X High Bound High Byte |
| C0s8 | RW | Clamp Y Low Bound Low Byte |
| C0s9 | RW | Clamp Y Low Bound High Byte |
| C0sA | RW | Clamp Y High Bound Low Byte |
| C0sB | RW | Clamp Y High Bound Hi Byte |
| C0sE | R | Status |
| C0sF | RW | Mode | 

Mode

| Bit | Function |
|--|--|
| 0 | Turn mouse on |
| 1 | Enable interrupts on mouse movement |
| 2 | Enable interrupts when button pressed |
| 3 | enable interrupts every screen refresh |
| 4-7 | Reserved |

Status

| Bit | Function |
|--|--|
| Bit 7 | Button is down  |
| 6 | Button was down at last reading  |
| 5 | X or Y changed since last reading  |
| 4 | Reserved |
| 3 | Interrupt caused by screen refresh |
| 2 | Interrupt caused by button press |
| 1 | Interrupt caused by mouse movement |
| 0 | Reserved |

The application program is responsible for setting up the interrupt vector and handler routines.

## Mouse API Routines

### SETMOUSE

Input:
A = mode value
X = $Cn
Y = $n0

Return:

C = 1: if mode byte is illegal (greater than $0F)
C = 0: everything fine.
No change to registers or screen holes.

### SERVEMOUSE

If an interrupt was caused by mouse, SERVEMOUSE updates the status byte ($778+n) to show which event caused the interrupt.

Return:
C = 0: interrupt caused by mouse
C = 1: interrupt not caused by mouse.

### READMOUSE

Transfers current mouse data to screen holes. Sets bits 1,2,3 in status byte to 0.

### CLEARMOUSE

Sets X and Y position values to 0, both in screen holes and on card. Button and interrupt status remains unchanged.

### POSMOUSE

sets position registers on peripheral card to the values it finds in the screen holes.

### CLAMPMOUSE

Input:
A = 0: change X coordinate limits
A = 1: change Y coordinate limits

New boundaries are read from Apple main memory:
$478 - low byte of lower bound
$4F8 - low byte of higher bound
$578 - high byte of lower bound
$5F8 - high byte of higher bound

destroys contents of mouse's X and Y position screen holes. Program must follow CLAMP with a READMOUSE.

### HOMEMOUSE

Sets mouse position registers on the peripheral card to the lower boundaries.
Does not update the screen holes. To do that, you should follow with READMOUSE.

### INITMOUSE

sets internal default values for mouse subsystem and synchronizes with monitor's vertical blanking cycle.

## Call jump table

| Offset | Function |
|--|--|
| $Cn12 | Low byte of SETMOUSE entry point address |
| $Cn13 | Low byte of SERVEMOUSE entry point address | 
| $Cn14 | Low byte of READMOUSE entry point address | 
| $Cn15 | Low byte of CLEARMOUSE entry point address | 
| $Cn16 | Low byte of POSMOUSE entry point address | 
| $Cn17 | Low byte of CLAMPMOUSE entry point address | 
| $Cn18 | Low byte of HOMEMOUSE entry point address | 
| $Cn19 | Low byte of INITMOUSE entry point address | 

# Translation of Mouse Motion

The virtual Apple will have its own clamping. This presents two options:
1) SetWindowRelativeMouseMode will constrain the mouse to the interior of the window; when we're using the mouse in the gameport code, we translate and scale mouse-in-window coordinates to 0-255. This means there is a different scale vertically and horizontally. I don't think that's what we want.
2) Have the above mode, BUT, use Mouse movement events to update the virtual mouse x and y positions.

I think the latter approach is better - x and y will have the same sensitivity this way, and, we're not having to deal with translating and scaling. It also deals better with setting the mouse position, and we won't have to do a SDLSetMousePos call.

SDL_MouseMotionEvent is the ticket.

I have an odd conflict: A2Desktop can use both mouse and joystick and gets confused. So when the Mouse is active (turned on), disable Mouse-Joystick emulation.

# Interrupts

there are three interrupts: vbl, on mouse motion, on button click.
"upon detecting an interrupt event, the mouse subsystem sends an IRQ to the 6502 at the end of the current monitor screen writing cycle".
So, it only sends IRQ when we're in VBL. 

OK, whenever we initmouse, this synchronizes with the vbl. So we can calculate where the vbl should be and set and event timer for that cycle.
The handler for that event will check whether the mouse moved, button pressed, or .. i.e. we should set flags "this event occurred".
when the handler is called, we (may) push these into the IRQ flags then OR them and set (or clear) the slot IRQ. then clear the underlying flags?

ok. So we can grab the cycle counter whenever the mouse is enabled. grab this from the video routines. That's the simplest way, and will work on ii plus mode or //e mode.

This is what Shufflepuck author learned writing it:

https://www.colino.net/wordpress/en/archives/2025/05/08/yes-the-apple-ii-mousecard-irq-is-synced-to-the-vbl/

(for initial test, we don't care, just pick current cycle plus 17,030)

# Compatibility

## A2Desktop

working w/o interrupts. However, when the gamepad is not connected, and gamecontroller uses mouse to emulate joystick, there is a conflict in that some mouse clicks and motion cause joystick which interferes with reading from the mouse card. We should disable mouse-based gamecontroller when mouse card mode is Active.

## Shufflepuck

[x] starts to blank screen?  
[ ] The mouse is overly sensitive  

plays a short bit of music
hangs on blank screen in a tight loop waiting for $B003 to become non-zero.

likely waiting on mouse interrupts. maybe even vbl.

the blank screen issue was unrelated to mouse, it was not handling the //e quasi video mode with 80col off but hires on doing no-shift mode.

## DazzleDraw

also likely waiting on mouse interrupts. specifically, DD wants VBL interrupt. So that's easy to implement.

status byte toggles between 0x60 -> 0xA0 when I click mouse. If I spastically click mouse many times it may get one click every so often through.


# Bugs

[ ] When I click and drag (like choosing a menu), only the X or the Y coordinate can change, not both. That is weird. not reproducing this today with debugging off  

[ ] when using apple-W to close, well it actually closes the emulator window.  

Cmd-W is being acted on by the MacOS and closing the window before I ever have a chance to do anything with it.

[ ] in DD, clicking in-window does not cause mouse to be captured  

videosystem was returning true on the mouse capture section - changed to -false- because we also still want these clicks passed through to whatever there is underneath looking for these events.


in DD about 20% to 40% of vertical area, the cursor gets weird. So VBL is in the wrong place.

# Other Implementations

## AppleWin

Emulates the 6521 interface (labels it a 6821), so it's likely simulating the AppleMouseII fairly directly and using its firmware.

## 
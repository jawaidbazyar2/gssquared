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

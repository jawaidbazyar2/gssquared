# Apple IIgs Clock and Battery RAM Device

The Apple IIgs clock device also has "battery-backed parameter RAM" which is I believe 256 bytes of memory kept live by battery. (As opposed to what a modern computer might do with a small flash memory chip).

## Control Register - $C034

| Bit | Name | Description |
|-|-|-|
| 7 | start/finished | 1 = read or write begins by setting this to 1. RTC sets back to 0 when data exchange is complete. Program detects end of data exchange by polling this bit to become 0 |
| 6 | read/write | set this bit to 1 prior to a read; set to 0 prior to a write |
| 5 | last byte | set this bit to 1 after the last byte has been read or written. Typically 2 or 3 bytes |
| 4 | Reserved | Reserved; do not modify |
| 3 - 0 | Border Color | Border Color Register |

## Data Register - $C033

8 bits

## Examples

Using toolbox:

https://groups.google.com/g/comp.sys.apple2/c/FmncxrjEVlw


```
Using the IIGS Battery-Backed-Up RAM and Clock From Assembly Language

Neil Parker

npa...@cie.uoregon.edu
par...@corona.uoregon.edu
Version 1.00
January 1994

COPYRIGHT by 1993 Neil Parker
All Rights Reserved

========
Abstract
========
This technical note discusses how a machine-language programmer can access
the Apple IIGS battery-backed-up RAM and clock hardware directly, bypassing
the firmware and system software. The BRAM and clock are normally accessed
through a set of subroutines in the Miscellaneous Toolset, which is the only
Apple-supported method accessing them. Direct manipulation of the hardware
provides access to several features that the Miscelaneous Toolset does not,
such as the "raw" time and the BRAM write-protect register.

===========
Terminology
===========

Terminology:
Bit 0 The least significant bit in a byte
Bit 7 The most significant bit in a byte
BRAM Battery backed-up RAM. Information stored in the
BRAM is not lost when the IIGS is turned off.
CLOCKCTL Clock/BRAM protocol control register
CLOCKDATA Clock/BRAM data register

========================
Accessing the IIGS Clock
========================
The IIGS BRAM/Clock chip seems to be identical to the one documented in
_The Macintosh Family Hardware Reference_. The BRAM/Clock chip uses a
simple protocol to transfer data in and out using two memory locations:

 Register Address    Function
--------- ---------- ------------------------------------
CLOCKCTL  0xC034     Clock/BRAM control register
CLOCKDATA 0xC033     Clock/BRAM command and data register

The protocol for a "write" is:
CLOCKDATA <-- command specifying register to be written
CLOCKCTL <-- bits to initiate transfer
CLOCKDATA <-- data to be written
CLOCKCTL <-- bits to continue transfer

The protocol for a "read" is:
CLOCKDATA <-- command specifying register to be read
CLOCKCTL <-- bits to initiate transfer
CLOCKDATA --> data from BRAM/Clock
at the conclusion of this sequence, CLOCKDATA contains the desired data.

The available commands are shown in the table:

Comamnd  Access Purpose
-------- ------ --------------------------------------
z0000001 R/W    Clock seconds register lo
z0000101 R/W    Clock seconds register next-to-lo
z0001001 R/W    Clock seconds register next-to-hi
z0001101 R/W    Clock seconds register hi
00110001 W-Only Write to test register Always set bits 6 & 7 to "0".
00110101 W-Only Write to write-protect register Setting bit 7 to "1" locks out further writes to the BRAM/Clock
z010ab01 R/W    Access BRAM address 100ab
z1abcd01 R/W    Access BRAM address 0abcd
z0111abc R/W    Followed by 0defgh00. Access BRAM address abcdefgh

The "Command" column contains entries beginning with an "z". This letter
is replaced by a "0" or "1" indicating the desired access type. A value
of "0" for this bit specifies a write, while a value of "1" for this bit
specifies a read.

For example, the command to access the low byte of the clock's seconds
register is "z0000001". To write a value to this register, write the
byte "00000001" followed by the new value for the low byte of the seconds
register. To read the value of the low byte of the clock seconds register,
write the byte "10000001", and then read the value from CLOCKDATA.

----------------
Seconds Register
----------------
The four bytes of the seconds register form a 32-bit count of seconds since
midnight, January 1, 1904. They should always be accessed in lo-to-hi
order. When reading the clock, the ROM reads all four registers several
times, until it gets the same value twice in a row. When writing, the ROM
immediately tests the written value by reading it back -- if the values
don't match, it tries again.

----------------------------------------
Test Register and Write-Protect Register
----------------------------------------
The test register and the write-protect register are write-only registers.
Setting the high bit of the write-protect register prevents any of the
other registers (or BRAM locations) from being written to, and clearing it
enables writes to the registers and BRAM locations. The two high-order
bits of the test register are used as control bits during testing, and
should normally be set to 0 (setting them to anything else interferes with
normal clock functioning).

I can't find any code in the IIGS ROM that accesses these registers.

---------------------
Battery Ram Locations
---------------------
The z010ab01 and z1abcd01 commands are holdovers from the early Macintosh
days when there were only 20 bytes of BRAM. The IIGS doesn't seem to use
these functions--it uses the more general z0111abc 0defgh00 command
instead.

Note that the z0111abc 0defgh00 command is two bytes long. For reading,
read the data byte after writing the second byte. For writing, write the
data byte after writing the second byte.

========================
Reading/Writing the BRAM
========================
The CLOCKCTL byte is used to transfer data to and from the BRAM and the
clock. Only the upper three bits of the CLOCKCTL byte at $C034 are used
by the BRAM/Clock; the lower five bits contain the current screen border
and are thus not used by the BRAM/Clock. The bits are organized as:
7 6 5 4 3 2 1 0
+---+---+---+---+---+---+---+---+
| S | T | A | Z | B | B | B | B |
+---+---+---+---+---+---+---+---+
where:
B Screen Border Color (Unused By BRAM/Clock)
S Start Transaction
T Transaction Type (Read/Write)
A Clock Enable Assert
Z This bit should always be set to 0

------------------
Start Transfer Bit
------------------
Bit 7 is the "Start Transaction" bit. Writing a "1" to this bit begins a
read or write transaction. All this means is that the BRAM/Clock begins
processing the value in CLOCKDATA. When the operation has completed, Bit
7 will be reset to "0". Thus, testing this bit allows the programmer to
determine when an operation has completed.

--------------------
Transaction Type Bit
--------------------
Bit 6 specifies the "Transaction Type" bit, either a read or a write. The
values and meanings for this bit are shown below:
0 = Write
1 = Read

-----------------------
Clock Enable Assert Bit
-----------------------
Bit 5 is the "Clock Enable Assert" bit. All the documentation says this
bit should be set to "0" before beginning a transfer, and should be set
back to "1" after the transfer is complete. However, the ROM routines
do the opposite -- they set the bit to "1" at the beginning of a transfer,
and "0" when the transfer completes.

--------------
Remaining Bits
--------------
Bit 4 should always be 0. Bits 0-3 have nothing to do with the BRAM/Clock
-- they control the screen border color.

--------------------
Writing Data To BRAM
--------------------
To write a byte, write the command indicating which location is desired to
CLOCKDATA. Then set bits 7 and 5 of CLOCKCTL, and clear bit 6 of CLOCKCTL.
When bit 7 is cleared by the BRAM, CLOCKDATA can be loaded with the data
value to be written. If you have more data to write from the BRAM, bits 6
and 5 in CLOCKCTL need not be changed between transfers. After each write,
clear bit 5.

A write uses the following protocol:
CLOCKDATA = "write" cmd for desired register or BRAM location
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 0
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] == 0
if this is a two-byte command ("00111abc 0defgh00")
CLOCKDATA = second byte of command
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 0
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] = 0
end if
CLOCKDATA = data to be written
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 0
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] == 0
CLOCKCTL[bit 5] = 0

----------------------
Reading Data From BRAM
----------------------
To read a byte, write the command indicating which location is desired to
CLOCKDATA. Then set bits 7, 6, and 5 of CLOCKCTL. When bit 7 is cleared
by the BRAM, CLOCKDATA contains the data value. If you have more data to
read from the BRAM, bits 6 and 5 in CLOCKCTL need not be changed between
transfers. After each read, clear bit 5.

The algorithm looks like this:
CLOCKDATA = "read" cmd for desired register or BRAM location
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 0
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] == 0
If this is a two-byte command ("10111abc 0defgh00")
CLOCKDATA = second byte of "read" command
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 0
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] == 0
end if
CLOCKCTL[bit 7] = 1
CLOCKCTL[bit 6] = 1
CLOCKCTL[bit 5] = 1
wait until CLOCKCTL[bit 7] = 0
result = CLOCKDATA
CLOCKCTL[bit 5] = 0

------------------
Computing Checksum
------------------
Any change to the BRAM invalidates the checksum unless the programmer
explicitly recomputes the checksum and stores it in the BRAM. An invalid
checksum causes the IIGS to reload the BRAM with default values. The
checksum is a 16-bit number computed by the following algorithm:
initialize checksum to 0
start at end of BRAM buffer (BRAM locations $FA and $FB)
repeat
rotate checksum left 1 bit
checksum = checksum + word from buffer
buffer position = buffer position - 1 byte
until we reach the beginning of the buffer

The checksum is stored in BRAM bytes $FC (low byte) and $FD (high byte).
The checksum exclusive-ORed with the constant $AAAA is stored in BRAM bytes
$FE (low byte) and $FF (high byte).

The source code for computing the BRAM checksum is included in the ROM
disassembly below.

-------
Caveats
-------
The Macintosh documentation recommends that the last access to the
BRAM/Clock
be a write operation, in order to avoid running down the battery. Since the
IIGS ROM code does not seem to heed this advice it is possible that the IIGS
hardware makes the final write unnecessary.

Beware of writing directly to the BRAM. The last few bytes of BRAM are
reserved for a checksum -- if the checksum doesn't match the remaining
contents of the BRAM, then the IIGS will ignore the entire BRAM contents
and reset everything to the defaults. The BRAM will also be ignored and
reset to defaults if any of its values are outside their legal ranges.

======================================
Appendix A: IIGS ROM Clock Access Code
======================================

The following code was disassembled from the IIGS ROM code accessing the
clock and BRAM. This code is for illustrative purposes only, and may
contain transcription errors.

; Copyright by Apple Computer, Inc.
; Disassembly by Neil Parker
; (Warning: There may be some transcription errors below)
;
ClockData equ $C033
ClockCtl equ $C034
BatteryRAM equ $E102C0 ;Battery RAM buffer
ClkErr equ $E103E0 ;Clock read error count
ClkRData equ $E103E1 ;4 bytes--clock read buffer
ClkWData equ $E103E5 ;4 bytes--clock write buffer
DBRE1 equ $FFF882 ;Set DBR to $E1
;
; Subroutine READTIME, at $FF/B5A0 (ROM 1) or $FF/B45A (ROM 0)
; Read the time into CLKRDATA
; Enter in 8-bit native mode
; Returns carry clear for success, carry set for failure
;
; The TOREADTIME vector at $E1/008C jumps to this routine.
;
ReadTime php        ;Save interrupt state
        sei         ;No interrupts allowed
        phb
        jsr DBRE1   ;Switch to bank $E1
        stz |ClkErr ;Start error count at 0
RTTry   ldy #0      ;Start read count at 0
RTTry2  inc |ClkErr
        beq RTFail ;If error count wraps, fail
        ldx #0      ;Start byte count at 0
        lda #$FD    ;Command--turns into 10000001 below
RTByteLp clc
        adc #4      ;Form next sec reg read command
        pha         ;Save partial cmd on stack
        ora #$80    ;Set "read" bit
        sep #$40    ;Set overflow bit (indicates read command)
        jsr BatRWAY ;Access register
        cpy #0      ;First read of regs?
        bne RTComp  ;If not, compare with previous read
        sta |ClkRData,X ;1st read, so store it
        bra RTMatch ;Go prepare for reading next byte
RTComp cmp |ClkRData,X ;Does this read match last read?
        beq RTMatch ;If so, go prepare for next byte
        pla         ;Otherwise discard partial command
        bra RTTry   ;...and try again
RTMatch pla ;Get partial cmd
        inx
        cpx #4 ;Got all 4 bytes yet?
        bcc RTByteLp ;If not, go get more
        tyx ;First read?
        bne RTGood ;If not, success!
        iny ;Otherwise indicate 2nd read
        bra RTTry2 ;...and go read again
RTFail  plb ;Failure exit--restore old DBR
SECRTL  plp ;Restore int state
        sec ;Return failure
        rtl
RTGood  plb
CLCRTL  plp
        clc ;Return success
        rtl
;
; Subroutine WRITETIME, at $FF/B5E4 (ROM 1) or $FF/B49E (ROM 0)
; Writes raw time in CLKWDATA to clock chip
; Enter in 8-bit native mode
; Returns carry clear for success, set for failure
;
; The TOWRITETIME vector at $E1/0088 jumps to this routine.
;
WriteTime php ;Save int state
        sei ;No interrupts allowed
        lda #0 ;Start error count at 0
        pha ;Keep error count on stack
WTTry ldx #0 ;Start byte count at 0
        lda #$FD ;Cmd--turns into 00000001 below
WTByteLp clc
        adc #4 ;Form next write cmd
        pha ;Save it
        lda >ClkWData,X ;Get byte to write
        tay
        lda 1,S ;Get write cmd
        jsr BatWAY ;Write the byte
        pla ;Get write cmd
        inx
        cpx #4 ;Done 4 bytes yet?
        bcc WTByteLp ;If not, go do the next
        jsl ReadTime ;Read back what we just wrote
        ldx #3
WTCmpLp lda >ClkRData,X ;Compare data just read
        cmp >ClkWData,X ;...to data we wrote
        bne WTNotSame ;If not the same, go read again
        dex ;Compared all 4 bytes yet?
        bpl WTCmpLp ;If not, compare more
        pla ;Success--discard error count
        bra CLCRTL ;...and exit with success
WTNotSame pla ;Get error count
        dec A
        bne WTTry ;If not too many errors, try again
        bra SECRTL ;Otherwise exit with failure
;
; Subroutine BCHECKSUM, at $FF/B61D (ROM 1) or $FF/B4D7 (ROM 0)
; Calculate the checksum of the BRAM
; Enter with data to be checksummed in BATTERYRAM buffer
; 8-bit native mode
; DBR pointing to bank $E1
; Returns with X=checksum
; A=checksum EORed with constant $AAAA
; 16-bit native mode
; Don't call this routine yourself--it resides in bank $FF, and ends with
; an RTS, not an RTL. If you need to compute a new BRAM checksum yourself,
; you should write your own routine that duplicates the functionality of
; this code.
;
BCheckSum ldx #$FA ;Start at end of buffer
        rep #$20 ;16-bit A-reg, since checksum is 16 bits
        longa on
        lda #0 ;Initialize checksum to 0
BCheck1 rol A
        adc |BatteryRAM,X ;Add in current word of buffer
        dex
        cpx #$FF ;At beginning of buffer yet?
        bne BCheck1 ;If not, go do more
        rep #$30
        longi on
        tax ;Else save checksum in X
        eor #$AAAA ;Get complement of checksum in A
        rts
        longa off
        longi off
;
; Subroutine BATIO, at $FF/B635 (ROM 1) or $FF/B4EF (ROM 0)
; Read or write a byte from a BRAM location
; Enter with A=value to read (if reading)
; Y=address to read or write
; Overflow flag= 1 to read or 0 to write
; 8-bit native mode
; DBR pointing to bank 0, 1, $E0, or $E1
; If reading, returns byte read in A reg
;
; The READBRAM and WRITEBRAM routines just call this in a loop. Don't
; try to call it yourself--it resides in bank $FF, and ends with an RTS,
; not an RTL.
;
BatIO pha ;Save byte for writing
        tya ;Save address to read/write
        pha
        and #$E0 ;Work on 1st byte: abcdefgh -> v0111abc
        lsr A
        lsr A
        lsr A
        lsr A
        lsr A
        ora #$38
        bvc BNoOv
        ora #$80
BNoOv xba ;Save 1st byte
        pla ;Work on 2nd byte: abcdefgh -> 0defgh00
        and #$1F
        asl A
        asl A
        xba ;Get 1st byte back, save 2nd byte
        php ;Save overflow bit
        jsr BatSend ;Send 1st byte to BRAM/Clock
        xba ;Get 2nd byte
BDoTrans jsr BatSend ;Send it
        plp ;Restore overflow bit
        pla ;Get data to be sent
        jsr BatSR ;Read or write byte (depending on V bit)
        pha ;Save data read, if any
        lda |ClockCtl ;Turn off "clock enable assert" bit
        and #$DF
        sta |ClockCtl
        pla
        rts
;
; BATWAY: Write A-reg, then Y-reg to BRAM/Clock chip
;
BatWAY clv ;Force write mode, then fall into...
;
; BATRWAY: Write A-reg, then either write Y-reg or read into A-reg
;
BatRWAY phy
        php ;Save overflow bit
        bra BDoTrans ;Go do the transaction
;
; BATSEND: Send A-reg to BRAM/Clock chip
;
BatSend clv ;Force write mode, then fall into...
;
; BATSR: Send or receive data to/from BRAM/Clock chip
;
BatSR sta |ClockData ;Put send data (if any) in data reg
        lda |ClockCtl ;Get control bits
        and #$3F ;Done-status=0, R/W=W
        bvs SRVSet ;Overflow set (read mode)?
        ora #$A0 ;No: Done-status=1, "assert"=1
        bra SRDoIt
SRVSet ora #$E0 ;Yes: Done-status=1, R/W=R, "assert"=1
SRDoIt sta |ClockCtl ;Perform action
SRLoop lda |ClockCtl
        bmi SRLoop ;Wait for action to finish
        lda |ClockData ;Get data to be read (if any)
        rts ;Done

==================================
Appendix B: Battery RAM Memory Map
==================================

Apple Computer's official position is that NONE of the BRAM is free for
programmer use; all 256 bytes are used. The table below describes the
value in each location in the BRAM for ROM 1. Any locations marked
"Reserved" are reserved by Apple for future use; some of these locations
may in fact be used by ROM 3 or by GS/OS. Note that the Port 2 values
at locations $0C through $17 function identically to the Port 1 values
at locations $00 through $0B and have been omitted for brevity.

Location Contents
-------- --------------------------------------------------
$00 Port 1 peripheral
0 = Printer
1 = Modem
2 = Appletalk (ROM 3 only)
$01 Port 1 line length
0 = Unlimited
1 = 40 characters
2 = 72 characters
3 = 80 characters
4 = 132 characters
$02 Port 1 Delete Line Feed After Carriage Return
0 = No
1 = Yes
$03 Port 1 Add Line Feed After Carriage Return
0 = No
1 = Yes
$04 Port 1 Echo
0 = Off
1 = On
$05 Port 1 Buffering
0 = Off
1 = On
$06 Port 1 Baud Rate
0 = 50 Baud
1 = 75 Baud
2 = 110 Baud
$D = 9600 Baud
$E = 19200 Baud
$07 Port 1 Data/Stop Bits
0 = 5 Data Bits, 1 Stop Bit
1 = 5 Data Bits, 2 Stop Bit
2 = 6 Data Bits, 1 Stop Bit
3 = 6 Data Bits, 2 Stop Bit
4 = 7 Data Bits, 1 Stop Bit
5 = 7 Data Bits, 2 Stop Bit
6 = 8 Data Bits, 1 Stop Bit
7 = 8 Data Bits, 2 Stop Bit
$08 Port 1 Parity
0 = Odd
1 = Even
2 = None
$09 Port 1 DCD Handshake
0 = Off
1 = On
$0A Port 1 DSR Handshake
0 = Off
1 = On
$0B Port 1 XON/XOFF Handshake
0 = Off
1 = On

$0C Port 2 Printer/Modem
$0D Port 2 Line Length
$0E Port 2 Delete Line Feed After Carriage Return
$0F Port 2 Add Line Feed After Carriage Return
$10 Port 2 Echo
$11 Port 2 Buffering
$12 Port 2 Baud rate
$13 Port 2 Data/Stop bits
$14 Port 2 Parity
$15 Port 2 DCD Handshake
$16 Port 2 DSR Handshake
$17 Port 2 XON/XOFF Handshake

$18 Display Color/Monochrome
0 = Color
1 = Monochrome
$19 Display 40/80 Columns
0 = 40 columns
1 = 80 columns
$1A Display Text Color
Color = $0 to $F
$1B Display Background Color
Color = $0 to $F
$1C Display Border Color
Color = $0 to $F
$1D 50/60 Hertz
0 = 60 Hertz
1 = 50 Hertz
$1E User Volume
Volume = $0 to $F ($0 is quietest)
$1F Bell Pitch
Pitch = $0 to $F ($0 is lowest)
$20 System Speed
0 = Slow
1 = Fast

$21 Slot 1 Internal/External
0 = Printer
1 = Your Card
$22 Slot 2 Internal/External
0 = Modem
1 = Your Card
$23 Slot 3 Internal/External
0 = 80-Column Card Firmware
1 = Your Card
$24 Slot 4 Internal/External
0 = Mouse
1 = Your Card
$25 Slot 5 Internal/External
0 = Smartport
1 = Your Card
$26 Slot 6 Internal/External
0 = 5.25 Drive
1 = Your Card
$27 Slot 7 Internal/External
0 = AppleTalk
1 = Your Card
$28 Startup Slot
0 = Scan
1-7 = Slot Number
8 = RAM Disk
9 = ROM Disk

$29 Text Display Language
$2A Keyboard Language
$2B Keyboard Buffering
0 = off
1 = on
$2C Keyboard Repeat Speed
Speed = 0 to 7 (0 is slowest)
$2D Keyboard Repeat Delay
0 = shortest delay
...
3 = longest delay
4 = no delay
Speed = 0 to 4
$2E Double-Click Time
0 = longest delay
...
4 = shortest delay
$2F Cursor Flash Rate
0 = fastest flash rate
...
4 = slowest flash rate
$30 Shift Caps/Lowercase
0 = No
1 = Yes
$31 Fast space/delete Keys
0 = No
1 = Yes
$32 Dual Speed Keys
0 = No
1 = Yes
$33 High Speed Mouse
0 = No
1 = Yes

$34 Month/day/year format
0 = MM/DD/YY
1 = DD/MM/YY
2 = YY/MM/DD
$35 24-hr/AM-PM format
0 = 12 Hour Format with AM/PM
1 = 24-Hour format
$36 Minimum RAM for RAM disk
0 = None
$20 = Largest Selectable Size
$37 Maximum RAM for RAM disk
0 = None
$20 = Largest Selectable Size

$38-$40 List of available display languages
$41-$51 List of available keyboard layouts

$52-$58 Reserved (Maybe Used On ROM 3???)
$59 Memory Peeker and Visit Monitor CDA settings
Bit Meaning
0-6 ???
7 Install CDAs at boot time
$5A Keyboard translation setting
0 = none
1-$FE = user-defined
$FF = standard
$5B CloseView settings
Bit Meaning
0-3 magnification
4 cvUseKeys
5 cvMagnify
6 cvInvert
7 cvEnabled
$5E Miscellaneous System 6 settings
Bit Meaning
0 disable close captioning
("visual indication of sounds")
1 0 = daylight savings time
1 = standard time
2 disable automatic daylight savings time
3-4 number of menu blinks
$5F Miscellaneous System 6 settings
Bit Meaning
0 Alphabetize DA Menus
1 Disable Init Icons On Boot
2 Disable QuickDraw Scanline Interrupts
3-5 Reserved
6-7 Set to %10 If This Byte Is Valid
$60 Scaling For WaitUntil Toolbox Call
$61 Reserved For Network Medium Selection
$62 OS For Network Boot
1 = GS/OS
2 = ProDOS 8
$63-$7F Reserved
$80 AppleTalk Node Number
$81 GS/OS Cache Size
0 = Minimum
1 = 32K
2 = 64K
...
$FE = 8128K
$82 Reserved for operating system variables
...
$A1 Reserved for operating system variables
$A2 Reserved
...
$FB Reserved

$FC Checksum (low byte)
$FD Checksum (high byte)
$FE Complement of checksum (low byte)
$FF Complement of checksum (high byte)

============
Bibliography
============

_Inside Macintosh, Volume III_
Apple Computer, Inc.
Addison-Wesley
1985
Contains a description of the old Mac BRAM/Clock chip (the one with only
20 bytes of RAM).

_The Macintosh Family Hardware Reference_
Apple Computer, Inc.
Addison-Wesley.
Contains a description of the newer Mac BRAM/Clock chip and the commands
that it accepts.

_Apple IIGS Hardware Reference_
Apple Computer, Inc.
Addison-Wesley.
1987
Describes the CLOCKDATA and CLOCKCTL registers.

_Apple IIGS Toolbox Reference: Volume 1_
Apple Computer, Inc.
Addison-Wesley.
1988
Describes the contents of most of the Battery RAM locations.

_Apple IIGS Technical Reference_
Michael Fischer.
McGraw-Hill, Inc.
1986
This seems to be the only source that lists the legal values for most of
the BRAM locations.
```

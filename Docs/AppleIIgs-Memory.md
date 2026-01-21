# iigsmemory

Memory Control Switches

This is a great discussion directly addressing several questions I have:
https://comp.sys.apple2.narkive.com/o1R6slX2/apple-iigs-bank-latching

Also this:
http://umich.edu/~archive/apple2/technotes/tn/iigs/TN.IIGS.032


## Shadow Register - $C035

* Default on Power-up:
* Default on RESET:

| Bit | Description | Notes |
|-|-|-|
| 7 | Reserved - do not modify | |
| 6 | Inhibit I/O and language-card Operation | |
| 5 | Inhibit Shadowing text page 2 | ROM03 only |
| 4 | Inhibit shadowing, aux bank hi-res pages (both) | |
| 3 | Inhibit shadowing - Super Hires Buffer | |
| 2 | Inhibit shadowing - hires page 2 | |
| 1 | Inhibit shadowing - hires page 1 | |
| 0 | inhibit shadowing - text page 1 | |

### 6: I/O and LC inhibit:
0. enable I/O at $C000, and perform LC operations in $D000-$FFFF
1. flat RAM memory space from $0000 - $FFFF

There is no shadowing from $D000-$FFFF. the LC that can appear in banks $00/$01 is distinct from the LC that can appear in banks $E0/$E1.

### 5: Inhibit shadowing text page 2
Text Page 2 is $0800 - $0BFF.

0. shadow main text page 2 and auxiliary text page 2 to E0/E1
1. no shadowing

ROM01 boards don't have this and so have that classic desk accessory to do it in software.

### 4: Inhibit Shadowing Aux Hi-Res graphics pages
The hires pages are $2000 - $5FFF

0. shadowing is enabled subject to the setting of bit 1.
1. shadowing is disabled in the auxiliary bank. Main bank hires shadowing is unaffected.

### 3: Inhibit shadowing: Super Hires buffer
This is Aux bank, $2000 - $9FFF

0. Shadowing enabled for super hires graphics buffer.
1. Shadowing is disabled for entire 32K video buffer.

### 2: Inhibit Shadowing Hires Graphics Page 2
Address Range: $4000 - $5FFF

0. Shadowing is enabled for hires page2, both Main and Aux, subject also to Bit 4
1. Shadowing disabled for hires page 2, both Main and Aux.

### 1: Inhibit Shadowing Hires Graphics Page 1
Address Range: $2000 - $3FFF

0. Shadowing is enabled for hires page 1, both Main and Aux, subject also to Bit 4
1. Shadowing disabled for hires page 1, both Main and Aux.

### 0: Inhibit shadowing text page 1
Text Page 1 is $0400 - $07FF.

0. shadow main text page 1 and auxiliary text page 1 to E0/E1
1. shadow disabled for text page 1.

## $C036 - Speed Register

### 7: CPU Speed

### 6: Power-on status

### 5: reserved - do not modify

### 4: Shadowing enable for ALL FAST RAM banks

GuS only changes pagetable entries for the SHR page; banks $02 - $MAXFASTBANK.
But both even and odd banks, which doesn't seem right. And it -only- considers the SHR area for shadowing, not any other video area. This seems wrong. let's look at KEGS.

/* Assume Ninja Force Megademo */
/* only do banks 3 - num_banks by 2, shadowing into e1 */
At least this only does the odd banks. But clearly is a hack for one piece of software.

### 3: Slot 7 disk motor-on detect

### 2: Slot 6 disk motor-on detect

### 1: Slot 5 disk motor-on detect

### 0: Slot 4 disk motor-on detect


The CYA (Speed) reg is cleared to zeros on reset or power up

## $C029 - newvideo register

### 7: Disable Video

when 0, all existing Apple II video modes are supported.
When 1, memory address bus is tri-stated during PHI1 except for refresh. Also changed memory map to linear per bit 6.

### 6: Change Memory Map

When 0, memory map is same as normal Apple II.
When 1, memory map is linear and starts at $2000 and ends at $9FFF in Aux memory.

When linearization is enabled, e1/2000..9fff is remapped per below, to allow the SHR stuff to read two bytes of SHR data per cycle.

```
x = linear_address - $2000
y = x[15]:x[0]:x[14..1]
physical_address = y + $2000
```
in other words:

linear $2000 maps to physical $2000
linear $2001 maps to physical $6000

$2002 -> $2001
$2003 -> $6001

the pattern continues...

It just so happens that the mapping between main/aux bank and physical
chips on the board is swapped for addresses $6000..$9fff. These two
hacks together let the VGC access two contiguous bytes of SHR data
simultaneously.

Experiment:
turn on linear
E1/2000:1 2 3 4
turn off linear
these values should now be in:
e1/2000.2001 e1/6000.6001
E1/2000
E1/6000
monitor is changing this.
so have to write a test program.


### 5: B&W / Color

When 0, double hires displayed in color.
When 1, double hires display is black and white.

### 4-1: Reserved, must set to 0.

### 0: Enable Bank Latch

When 1, data on alternate-data-bus bit 1 is latched as bank address bit 0.
When the bit is 0, data is not latched.

When the bank address is latched, the Main/Aux hierarchy is as follows: if bank address is 1, Aux memory is always addressed. If bank address is 0, soft switches determine which bank is accessed. (RAMRD/RAMWRT).

* When the bank latch is enabled, A17 is passed to the Mega II, so you can
access E1/xxxx directly. If the bank latch is disabled, all accesses to
E1/xxxx go to E0/xxxx (where you can still access auxiliary memory through
Apple IIe soft switches)

Enable bank latch. If this bit is 1, the 17th address bit is
used to select either the main or auxiliary memory bank.
If the address bit is 1, then the auxiliary bank is
enabled. (Actually data bit 0 is used as the 17th address
bit). If the address bit is 0, the state of the memory
configuration soft switches determines which memory
bank is enabled. See Chapter 3 for descriptions of the
memory configuration soft switches. Table 4-18 shows
how to use this bit to select a memory bank.
1 The 17th address bit is ignored.

So, "data bit" is just "we're going to hack another address bit by using the data bus" because the MegaII has only a 16-bit input. So that really can be treated as address bit 17.

ugh this is still not clear which banks this works against.

## $C068 - State Register

The State Register duplicates in the FPI certain memory management bits that can be read all in one byte, instead of across many bytes as in the Apple IIe.

| Bit | Softswitch | Description |
|-|-|-|
| 7 | ALTZP | 1 = ZP, Stack, LC are in Main; 0 = in Aux |
| 6 | PAGE2 | 1 = Text Page 2 Selected |
| 5 | RAMRD | 1 = Aux RAM is read-enabled |
| 4 | RAMWRT | 1 = aux RAM is write-enabled |
| 3 | RDROM | 1 = ROM is read enabled in LC area; 0 = RAM read enabled in LC area |
| 2 | LCBNK2 | 1 = LC Bank 2 Selected |
| 1 | ROMBANK | Must always be 0 |
| 0 | INTCXROM | 1 = internal ROM at $Cx00 is selected; 0 = peripheral-card ROM |

This is a read-write register - intention being to dramatically speed up interrupt handling.

Here is an interesting question - is this -all- the softswitches that control IIe memory mapping in Main / Even Banks? NO.
It's missing: 80STORE; but that's the only one I think.

LCBNK2 here: the GS HW Reference has this bit's description backwards. HW Ref says "1 means Bank 1 selected". In fact, 1 means Bank 2 selected, just like the name says.

## Memory Translation

The Mega II swaps $6000-$9FFF (half the superhires page) in the *physical* RAM.
I.e.
logical address $5FFF is in Memory chips marked Main
logical address $6000 is in Memory chips marked Aux

I think there is a softswitch that controls this.

Based on the schematic, the MegaII has following inputs:

MDBUS (same as DBUS via a 245 bus transceiver). Main data bus (//e main bank)
ADBUS (Aux data bus, shared only with video)
RABUS - ram address on the two banks of 64K.
CASA, CASM, RAS: column address for mega ii memory (aux and main) and row strobe. So it has its own address / dram decoder.
So when there is an access from CPU to "aux RAM" it must be mediated by MegaII, and MegaII must internally push incoming data on MDBUS to ADBUS as required.

ONLY 16 bits of address (A0-A15) are connected to MegaII. Plus we know, one more bit of address when multiplexed by 816 on the data bus D0.

What is SBUS ? It's 6 bits (S0-S5) going into MegaII.

### Implementation

Basis: iigs MMU page tables are 64k.

iigsmem_shadow_config

Do it like iiememory bsr_config?

Compose a map to be used to rapidly determine what memory addresses shall be shadowed.
If there is no shadowing enabled (as will often be the case for IIgs applications under GS/OS), remove the shadow handler from banks $00/$01.
Otherwise set the handler and set the handler's map of 128 pages according to the map calculated.

In Speed Register, bit 4 = 1 means shadowing in all banks $00 - $7F; otherwise shadowing only banks $00-$01.

So we can either shadow (and make I/O & LC appear) in banks 0/1; OR, in all RAM banks.

For purposes of the LC, if shadowing is enabled for a particular bank, and the IOLC is enabled, then transform the address. for LC ROM, it's always going to point to the same real ROM. for LC RAM, we're mapping the addresses potentially like so:
D000-DFFF: either C000-CFFF (bank 1), or D000-DFFF (bank 2)
E000-FFFF: E000-FFFF.
0000 - 01FF: 0000 - 01FF either in main bank, or in aux bank.
Then re-add to base address of bank.

The mapping could also be:
17-bit address in
lookup in 256-byte page table (just like //e)
get new 17-bit address out (but not full 64-bit address, but just a relative address)
There can be flag bits in the LUT: I/O; ROM; so we can then transform addresses the correct chunk of malloc'd memory. Because I think the memory 

RAMRD/RAMWRT

Any access to an even shadowed bank must act like an Apple IIe access to main (the only) memory address space. Is this only bank 0, or, any shadowed even bank? (It seems likely THIS only operates in banks 00/01).

On a direct access to an odd shadowed bank, it goes directly to that bank. (Does this bypass LC machinations?) Controlled by the Bit 17 thing. (of course this implies an effective MMU address > $FFFF).
Otherwise, an access to an even shadowed bank, gets transformed and might result in an access to its corresponding odd bank.


## Vector Pulls

Need to add these MMU tests. Not sure about this!
    // Test: when IOLC not inhibited, interrupt vector pull reads from ROM.
    // Test: when IOLC inhibited, interrupt vector pull reads from RAM.

1. Set up vector for BRK?
1. make sure IOLC enabled and ROM enabled
1. BRK
1. our vector handler will store a value into memory
1. Test it.

WriteOp(0x2000, 0x00)
WriteOp(0x1000, )
AssertOp(0x2000, 0xAE)

```
PHA
LDA #$AE
STA $00/2000
PLA
RTI

PHA
LDA #$EF
STA $00/2000
PLA
RTI

```
Somehow, I'm going to have to signal a vector pull to the MMU module. I could use a different read() routine for those.
Add a vector_read() routine.


## C1-CF Mapping

This works a little differently on the GS than IIe.

There is (perhaps) the IIe behavior of INTCXROM SLOTC3ROM.
There is of course the ability to set the slot roms between internal and slot.


## Notes


Apple IIe emulation is run in banks $00 and $01, but with shadowing to E0/E1 for I/O and video.  
This allows reads by 8-bit software to occur at full speed, but shadowed writes are synced to 1MHz.
There is various stuff in banks E0/E1 used by GS/OS, interrupt handlers, etc.

I'll need something to return the speed and sync of a r/w. Maybe just a simple flag "1mhz read/write", which is:
* any shadowed write in 00/01
* any read or write in E0/E1

So computer will have the RegMMU, but also FastMMU. RegMMU is injected into FastMMU.
The CPU calls FastMMU. Everything else is injected with RegMMU.
FastMMU is controlled by device iigsmemory, which exposes registers:
* shadow register
* New-Video register.

iigsmemory will also replicate the language card functionality, and have those registers, because that bank switching exists in banks 00/01.

New-Video, controls linearity or interleave of some memory in bank 1. Also, controls whether ALL even/odd banks are shadowed?

So have page maps that map these banks to the Apple II MMU.
Banks $00 - $01 are 128K built-in in ROM01 (plus E0/E1 = 256K). Banks $00 - $0F for ROM03 (1.125M total)
Banks $00 and $01 are "shadowed" to banks $E0 and $E1 if shadowing is enabled - this is exact use of shadow handler for those pages.

Fast ROM is banks $F0 to $FF. (1MB total).

The $C000 - $FFFF space in E0/E1 is controlled by the LC. The Shadow register bit 6 just controls whether a WINDOW in those locations in banks0/1 is present or not.
LC memory writes are NOT shadowed to E0/E1.

The ROM01 file is 256KB exactly, and would map to banks $FC $FD $FE $FF. It looks like $FF/C000 is various slot card firmware, including at $C800-$CFFF (I forget, did the IIe have stuff here too?)
$FF/D000 is AppleSoft etc. $F800 should be monitor ROM.. yep. All largely unmodified. So mapping this should be straightforward. Of course the reset/etc vectors are different.

Weirdly, there is ROM data at $C06E-$C0FF. For example, $C06E is a JMP 9D36. what? 

The IRQ/BRK vector is 74 C0 ($C074). At that location in the ROM is this:
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

## Tests

Develop a test harness that can easily do two things:

1. run tests in an emulator test harness
1. generate 65816 code to run tests on another emulator, or on real hardware.

Define test data.
iterate the test data
generate C++ code for the tests
also
generate 65816 assembly code for the tests

### Test Data

Test data is a series of the following:

W: ADDR24, VAL
V: ADDR24, VAL
VA: PADDR24, VAL

W means "write", as in write VAL into ADDR24. ADDR24 is an address that is passed through the MMU.
PADDR24 is a raw physical address read directly from the memory array, NOT via the MMU.
V means "validate" - read the value from ADDR24, compare to VAL; continue if a match, error if mismatch.
VA means "Validate absolute", or compare a value at physical address 24.


# Boiled Down

"shadow map"

We could compress "shadow / not shadow" into a single bit, and an entire 64k bank into 256 bits at 1 bit per 256 bytes.
Shadowing may even be fine on 1KB boundaries. So, 64 bits per bank. A single word.
So then, we can trivially have a lookup table for any variation of the shadow register bits as the LUT index. This then means a bit test and function call down to MMU_IIe.

Have an init func create a LUT based on every possible shadow bit (256 indices) and the 64-bit shadow mask above.

"shadow enabled / disabled"

Either banks $00/$01, or, ALL ram banks, may be set to shadow to $E0 and $E1, based on the shadow map above. (There are 3 or 4 basic methods each for read and write, for these options).

# Test behavior of INTCXROM and SLOTC3ROM in GS  

the HW Ref shows 3 variations on I/O memory map. Peripheral expansion ROM (c800), internal rom and peripheral rom, and "internal rom". 

in KEGS:

intcxromoff
c800:floating bus

intcxromon
c800:"internal" memory

ok, and I tested that this is the same in GS2.

Speaking generally, since there is unlimited ROM in the IIgs there would be no reason for the GS builtin peripherals to each have their own C800 space. But, that space should behave like on IIe if you have a slot card in and the slot register set to "your card".


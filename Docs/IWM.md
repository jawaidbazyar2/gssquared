# IWM - Integrated Woz Machine

The IWM was developed as a more flexible and compact form of Woz's Disk II disk controller.

it supports two different main timing inputs, 7MHz (used in apple IIs) and 8MHz (used in Macs).
And it supports two different speeds (densities), a 4uS cell (disk ii) and a 2uS cell (3.5 drive).

It has 4 internal registers and also the ability to communicate status and commands to/from the (dumb) 3.5 drive. And to talk to the smartport. (This last bit, I am unclear how it specifically works).

And it does all this using the same 16-byte I/O locations as a standard Disk II, while having a default mode that is exactly Disk II compatible. Crazy piece of work, this!

Original Apple Spec
https://www.brutaldeluxe.fr/documentation/iwm/apple2_IWM_Spec_Rev19_1982.pdf


Nick Parker's 3.5-focused treatment - but should give a good idea of how both operate (since 5.25 is just 'default mode')
https://www.applefritter.com/files/2025/03/02/IWM-Controlling%20the%203.5%20Drive%20Hardware%20on%20the%20Apple%20IIGS_alt.pdf

## Reset State

thinking about reset states.
the 3.5 motor on is separate from enable=1; but what happens when iwm is reset, enable=0, does that cause drive to turn off motor? that would be sensible thing..

## Refactoring Disk II

see DiskII.md for details.

Initially, just implement 5.25 support on IWM. Then we'll start to add in 3.5 and see if we need a different abstraction layer.

ok, I found the splice point - we need to pass the register read/writes down to the Floppy layer also. I pass them all, it can ignore the controller-level ones.

This is quite the layer upon layer of classes, but these are all header implementations and they are honestly not that complex, so the compiler should be able to optimize pretty well.

I'm now at the point where I need something to wire in the hooks to OSD/mount. I guess I can manually mount a floppy image for testing before I do that.

We need to reference the DISKREG register to control whether we're accessing 5.25, or 3.5. This will control which drives we call in the code. Drives 0-1 are 5.25; 2-3 are 3.5. We generate the index by: C031[6]. 0=5.25, 1=3.5. So it's that bit, shifted left one, plus the drive select switch.

ok, that's done. Now on to ...


## Refactoring Disk II Again

Well just got done implementing .woz support and redoing the internals. Now have a diskii_controller class, new floppy525_woz class.

Investigating how to tie that into IWM, so we'll then hopefully be in a position to do the 3.5 support.




## AppleDrive 3.5 Support

Key things here. Probably first thing to start with is, implement the interface to the 3.5 drive registers. There are 16 of them. They are selected by using CA0-CA2, plus SEL. You can read whether head is at track 0 or not; choose the disk head; read number of sides; detect if motor is on; detect motor speed; if a drive is even connected; if a diskette is inserted; if write protected; etc.

There are also 7 control functions. Those registers are selected the same way, but then triggered by toggling LSTRB. The control functions move the disk head, set head direction, turn the motor on and off, eject the disk.

The low level disk read/write stuff is mostly the same, though the 3.5 has variable sized tracks we need to account for. When we move the head to a different sized track we need to interpolate the head position (i.e. the index into the buffer) appropriately.




## Notes on simulating delayed head movement

We can use a frame time handler to simulate head movement latency. In the 5.25 world, the driver code is responsible for moving the head, then doing a busy wait for an appropriate length of time to let the head settle down. On the 3.5, that is handled in the drive and the OS is looking for a "head movement is done".

So we can tick that state machine reasonably accurately during frame time. Basically, we want to wait for a whole frame to have elapsed. This will probably be ok. If it ends up being too slow, we can use a 14M cycle checkpoint and also nudge it forward during register accesses.

Internet says 30ms track settling time for 3.5, 40ms for 5.25. In practice, it's been 25ms in most code I've seen for 5.25, I bet 3.5 is a shorter time, and this setting will impact.

Since the floppy525 code now has some delay in here to simulate taking some time to move the head, this won't be that different.

## IWM Phase 2

So the 3.5 physical layer basically works the same as the 5.25.however the interface has some enhancements.
the IWM has a number of mode switches to deal with 5.25. the big one is:

```
diskreg[6]
E	Enables 3.5-inch drive support:			0 = 5.25-inch drive and smartport devices available
			1 = 3.5-inch drive available"
```

this likely toggles the line on the 19pin connector labeled "3.5". there is also a "HDSEL" which is likely to be the smartport  (ok, it is)
the second tier of mode switches for using 3.5 vs 5.25 is the IWM Mode Register:
The bits of the mode register are laid out as follows:
```
	  7   6   5   4   3   2   1   0  
	+---+---+---+---+---+---+---+---+
	| R | R | R | S | C | M | H | L |
	+---+---+---+---+---+---+---+---+

With the various bit meanings described below:
	Bit	Function
	---	--------
	 R	Reserved
	 S	Clock speed:
			0 = 7 MHz.    // this is for Apple II
			1 = 8 MHz     // this is for Macs.
		Should always be 0.
	 C	Bit cell time:
			0 = 4 usec/bit (for 5.25 drives)
			1 = 2 usec/bit (for 3.5 drives). // twice as fast on a 3.5, double bit density
	 M	Motor-off timer:
			0 = leave drive on for 1 sec after program turns
			    it off
			1 = no delay.  // 3.5 drive off turns motor off immediately
		Should be 0 for 5.25 and 1 for 3.5.
	 H	Handshake protocol:
			0 = synchronous (software must supply proper
			    timing for writing data)
			1 = asynchronous (IWM supplies timing)
		Should be 0 for 5.25 and 1 for 3.5.
	 L	Latch mode:
			0 = read-data stays valid for about 7 usec
			1 = read-data stays valid for full byte time
		Should be 0 for 5.25 and 1 for 3.5.
```

So basically, the whole thing, $00 for 5.25, and $0F for 3.5. the big one here we're concerned with is instead of 
the wp sense bit shifting into the data register, has been replaced with the status register.(Q6=1, Q7=0)

The 3.5 also has various status and controls that are signaled with the phase lines (renamed CA0-2 + diskreg[sel]and LSTRB). So set the four bits and strobe. the result is in the status[7].

So, I am feeling like we could have iwm_controller. It will be very similar to diskii_controller. in fact we could instance a diskii_controller and vector between it and iwm_controller based on the 5.25/3.5 switch. 

the things that will be different between 5.25 and 3.5 in controller are:fast_forwardthe way the register loads are bytewise for reading and writing (i.e. check for completion byte at a time instead of bit at a time?)

IWM now using Floppy525_woz. All is good except ProDOS8 2.4.3 does -something- that causes the emulator to drag and blow frames. Must be something wrong with fast_forward?

## Commonalities between disk_controller and IWM

disk::decode is similar to iwm::access. the read_cmd/write_cmd are now fairly similar. 

There is actually just not that much core code in the diskii_controller here. it's fast_forward on down. 

Things to vector in 5.25/3.5 in IWM:

fast_forward
decode(): motor-on/motor-off differs based on that mode flag; 

Make the switches implementation the same between these (array union with individual switch names).

ok with some fussing and munging the apis are very similar now.


## Floppy Differences between 5.25 and 3.5

the floppy525_woz->floppy35_woz changes are:

the fast_forward speed per cycle. currently head_position += cycles * 2. head position is bit with a , so this advances by 1/4 or 1 bit cell.however, for 3.5, it runs twice as fast, so we would do head_positoin += cycles * 4 (2 cpu cycles per bit cell). this will return more bits_to_sim per invocation.

This could be a parameter from disk_controller. 

OR, maybe I can template the 3.5 and 5.25 off the (mostly) same code base. 

I think the read pulse, etc. are all the same. 

other differences: the 3.5 set_phase
this . the SEL bit from DISKREG needs to go to the 3.5 port? it must be on one of the pins. Research this more. We can just push this into floppy. Ah, it could be the HDSEL pin. I assumed this was "hard drive select" but Head Select makes more sense, given the firmware reference suggests this controls which head is used. (Probably mistake).

the way phase, SEL behave is of course radically different from the 5.25. but the controller interface is still those pins.
basically depending on how those are setup, the selected status is read on the write protect sense line. which is -always- this status bit on a 3.5.

## Debugging

no media in the drive:
so if I turn the motor on 35 on manually, there is a big long wait - read_position comes out in the billions, and head_position hasn't moved at all. And everything freezes for a while.

I monkeyed with other stuff for a while, there were issues with the motor on flag being inaccurately tied in to enable. I've dealt with that..

and then we were seeing partial bytes being read. Fixed that. I am only seeing whole nybbles come in. However, the read sequence I see right now:

D5 AA 96    A7    DB    DE
is actually on disk
D5 AA 96 96 A7 96 DB E9 DE

After the D5 AA 96 signature we are missing every other byte. 

I wonder if this is a cycle timing issue.

We are calling Floppy_woz::fast_forward with cycles from clock->get_cycles() 
@src/devices/iwm/IWM2.hpp:237 
this is cpu cycles.
I wonder if the floppy should be clocked with vid_cycles (1mhz), independently of the cpu speed. 
gemini changes all of them to vid_cycle (too eager to do what I was asking about) and it broke 5.25.
But let's think about this. the bits from the disk are coming in twice as fast as before. The bit cells are 2uS. So one (regular) nybble is 16uS (2 * 8), instead of the old 32uS.
The Neil 3.5 primer says "set fast speed". If we have cpu at 2.8 but the drive also clocked at 2.8.. the bits are coming in too fast. 5.25 code at 1mhz can keep up with a 5.25 disk. A 3.5 disk we need to be faster.

we know the 5.25 code can match any cpu clock speed. So, hmm...

I should eliminate direct accesses to ->get_cycles in the floppy code. 
it does use nclock for a timer for disk head movement. this use is unrelated to the other use.

floppy::fast_forward could take the incremental number of cycles. or it can treat 0 as special case "this is first access".

ok so I am calling floppy35:fast_forward with nclock now, and I am seeing all the bytes! but I'm still getting I/O error trying to "cat,s5".

ahhh. My test800.woz image was invalid! I created it with cp2 and it can access/validate it just fine but it doesn't work in emulator. ba ha ha ha. Once I started passing 3.5 the vid_clock, and used a non-broken image, it works FINE.

Writes very much confuse the system. I think the issue is write code expects to see an underrun flag when the write is complete; and iwm2 never sets underrun. So underrun flag is important.

ok!

“IWM Device Specification” says “in asynchronous mode the write shift register is buffered and when the buffer is empty the iwm sets the msb of the handshake register to 1 to indicate the next nibble can be written to the buffer. the buffer may be written at any time during the write state. only the data last written into the buffer register, before the contents of the buffer register is transferred to the write shift register, is used”

ok, so that clearly implies a one-nibble queue. there is “shift register actively writing bits”, and “one more nibble waiting to be loaded into shift register automatically” if it’s there.

An underrun occurs when data has not been written to the buffer register between the time the write-handshake bit indicates an empty buffer, and the time the buffer is transferred to the write shift register. If an underrun occurs in asynch mode /WRREQ will be disabled and /underrun will be set to 0.

there are TWO registers involved in writing in async mode:
write shift register
buffer register

the write-handshake bit is set to 0 when buffer register is written;
it is set to 1 when the shift register is empty, and the contents of the buffer are transferred into the shift register.
And if write-handshake was already *1* at that time, we flag underrun (set underrun bit to 0) and transfer nothing.
You can write the buffer at any time (writing multiple different values there) before it's shifted into the shift reg.

I AM WINNING SO MUCH I'M SICK OF IT!

OK, P8 seems to be able to write to the 3.5's just fine. 

[ ] GS/OS gets multiple disk insert sounds when I try to insert the disk in GS/OS. I wonder if it's continually reading a "disk changed" sense from it.

if you insert after Finder is already up, it works. If you have inserted at boot time, you get the multiple insert sounds and it never works.

[ ] The track and side are not being displayed correctly. (side tends to flutter back and forth)

[ ] attempting to Initialize a 3.5 in GS/OS results in no drive activity (no spinning) and the system hanging until the disk is ejected

[ ] Copy2Plus - attempt to format 5,1 and get "no disk in drive" even though it's in there.  (This was after the GS/OS debacle above)
eject and reinsert, copy2plus sees it, but failed format at block $BA5 (but I was at accelerated speed..).  ok, also did not work at 2.8. Hm!  

Going through the IWM spec, it says "if an underrun occurs in asynchronous mode /WRREQ will be disabled (set to ttl high) and /underrun will be set to 0." So this means no writing when underrun is 1. Currently we are forcing 0 writes. I'm not sure how you format a disk w/o writing during overrun.. (could it shift to synchronous mode?)

"In asynchronous mode the data register will latch the shift register when a one is shifted into the MSB. And will be cleared 14 FCLK periods (about 2uS) after a valid data read takes place. (valid defined as /DEV low and D7 outputting 1 from data register for at least one FCLK period)." <- this provides the cpu time to read. 
in synchronous mode "the shift register will appear to the data bus to be stalled for a period of 2 bit times plus four CLK periods".
FCLK is 7MHZ. So it will clear the data register about 2 bits after a valid data read takes place. This is probably not an issue, but it wouldn't be hard to model.
At some point, Floppy Emu could not format 3.5 disk images. Interesting.

https://www.applefritter.com/content/how-important-are-iwm-features-apple-iic

this has discussion about how the IWM produces the 10-bit sync bytes!

ah, ok! That's what I suspected - in async mode you can only send 8-bit latched values. So they write these 8bit values in sequence: 3F, CF, F3, FC, FF
which is an 8-bit sequence that has the 2 0 bits baked in. That's 40 bits, so that generates 4 FF's.
I'm wondering about that first one, the 3F, make sure our code doesn't gate on bit[7]=1. (Doesn't seem to).
So, that should work?

Hmmm, when we switch select the phase signals should carry over to the new drive. we are not currently doing that.

John Brooks utility to read a track from a 3.5.
https://comp.sys.apple2.narkive.com/ACBSoKvL/new-gs-3-5-diskimage-nibble-utility
it's failing - in his notes he mentions "fails due to not properly support 3.5 drive RDY timing"

I wonder if he means diskReady per below. 

So let's say the software is expecting that to be 1 (not ready) and then to switch to 0 (ready). 

3.5 sense_out: 8 = 0 lower
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: C = 1 numberSides
3.5 sense_out: C = 1
3.5 sense_out: 4 = 1 motorOn
3.5 sense_out: C = 1 numberSides
3.5 sense_out: D = 0 diskReady
3.5 sense_out: 1 = 1
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: D = 0
3.5 sense_out: D = 0
3.5 sense_out: 5 = 0 atTrack0
3.5 sense_out: 4 = 1 motorOn
3.5 sense_out: 0 = 0
3.5 sense_out: 6 = 1 diskSwitched
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 5 = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 1 = 0
3.5 sense_out: 1 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 7 = 0 tachometer
3.5 sense_out: 3 = 1
3.5 sense_out: 1 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: D = 0

3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0

3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: C = 1
3.5 sense_out: C = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 6 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 5 = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 1 = 0
3.5 sense_out: 1 = 0
3.5 sense_out: 5 = 0 atTrack0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 7 = 0 tachometer
3.5 sense_out: 3 = 1 diskLocked
3.5 sense_out: 2 = 1 diskIsStepping
3.5 sense_out: 0 = 0
3.5 sense_out: 2 = 1 diskIsStepping
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 6 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 5 = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 1 = 0
3.5 sense_out: 1 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 4 = 1
3.5 sense_out: 0 = 0
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: C = 1
3.5 sense_out: D = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 0 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: C = 1
3.5 sense_out: C = 1
3.5 sense_out: 4 = 1
3.5 sense_out: C = 1
3.5 sense_out: D = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: D = 0
3.5 sense_out: D = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 4 = 1
3.5 sense_out: 0 = 0
3.5 sense_out: 6 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 4 = 1
3.5 sense_out: 5 = 0
3.5 sense_out: 1 = 1
3.5 sense_out: 1 = 0

3.5 sense_out: 1 = 0 disk in place
3.5 sense_out: 5 = 0 attrack0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: 7 = 0 tach pulses
3.5 sense_out: 3 = 1 write protect
3.5 sense_out: 1 = 0 disk in place
3.5 sense_out: 5 = 0 attrack0
3.5 sense_out: 5 = 0
3.5 sense_out: 5 = 0
3.5 sense_out: D = 0 diskReady

3.5 sense_out: 9 = 0 upper head
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0
3.5 sense_out: 9 = 0

3.5 sense_out: 8 = 0 lower head
3.5 sense_out: 0 = 0 stepDirection
3.5 sense_out: 8 = 0 lower
3.5 sense_out: 0 = 0 stepDirection
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0
3.5 sense_out: 8 = 0


## JBrooks test

This IWM raw nibble viewer is derived from a single-block GS IWM bootloader which runs a compact state machine with 11 commands. The high nibble of each byte contains the command. The low nibble contains a parameter for the cmd.

```
*-------------------------------
* Set 3.5 ctrl regs            *
*  bit0 = CA2                  *
*  bit1 = SEL                  *
*  bit2 = CA0                  *
*  bit3 = CA1                  *
*-------------------------------
* Status                       *
* $1 = Read head 0             *
* $2 = Is drive empty?         *
* $3 = Read Head 1             *
* $4 = Is head done stepping?  *
* $6 = Is disk locked?         *
* $8 = Is motor off?           *
* $9 = Is drive dbl-sided?     *
* $A = Head at track > zero?   *
* $B = Disk up to speed?       *
* $C = Is disk switched?       *
* $E = Tachometer: 60 per rev  *
*-------------------------------
* Control                      *
* $0*= Step head inward        *
* $1*= Step head outward       *
* $3*= Reset disk switched     *
* $4*= Step head one track     *
* $8*= Motor on                *
* $9*= Motor off               *
* $D*= Eject disk              *
*-------------------------------

CMDTBL
         DW    CMDRDREG,CMDWRREG,CMDSELIWM,CMDSEL35
         DW    CMDSELTRIG,CMDTESTC0,CMDTESTC1,CMDJMP
         DW    CMDLOOPCTR,CMDREADTRK,CMDDONE
```
or:
| 0 | CMDRDREG | read C0Ex |
| 1 | CMDWRREG | write C0Ex |
| 2 | CMDSELIWM | set IWM mode register |
| 3 | CMDSEL35 | select sense source |
| 4 | CMDSELTRIG | do command |
| 5 | CMDTESTC0 | branch if sense is 0 |
| 6 | CMDTESTC1 | branch if sense is 1 |
| 7 | CMDJMP | jump to offset |
| 8 | CMDLOOPCTR | 
| 9 | CMDREADTRK | read track data (into buffer?) |
| A | CMDDONE | exit test with response code |

Below are the state-command comments so you can see what is happening logically with the IWM state and registers.

IWM register init:
INITCMD
         HEX   08         ;enable=0 ;drive off

         HEX   00         ;CA0 ;clear all phases
         HEX   02         ;CA1
         HEX   04         ;CA2
         HEX   06         ;LSTRB

Set IWM to 3.5" mode and exit if no disk is present
DRIVECMD
         HEX   0A         ;SELECT ;Activate drive #1

         HEX   0C         ;Q6 ;IWM in safe state
         HEX   0E         ;Q7

         HEX   0D         ;Q6+1 ;IWM set mode
         HEX   2F         ;SelIWM $F ;Set IWM to 3.5" mode
         HEX   0C         ;Q6 ;IWM back to safe state

         HEX   11         ;Wr DISKREG | $40 ;enable 3.5 drive
         HEX   09         ;ENABLE+1 ;Drive on
         HEX   32         ;Sel35 #2 ;chk for disk in drive
         HEX   51         ;Test, BCC *+1 ;Skip exit if disk is present
         HEX   AF         ;exit with error

Recalibrate the head to track 0 prior to reading:
RECALCMD
         HEX   48         ;SelTrig #8 ;Motor on
         HEX   41         ;SelTrig #1 ;Step dir=outward

         HEX   72         ;JMP *+2
:RECALSTEP
         HEX   44         ;SelTrig #4 ;Step head
         HEX   5F         ;Test, BCC *-1 ;Loop until head step completes
:TESTTRACK0
         HEX   3A         ;Sel35 #A ;chk for head at track 0
         HEX   6C         ;Test, BCS *-4 ;Loop head step if not trk 0

:ATTRACK0
         HEX   40         ;SelTrig #0 ;Step dir=inward
:CHKDISKRDY
         HEX   3B         ;Sel35 #B ;chk if disk is up to speed
         HEX   61         ;if disk status is not ready, then skip 1st wait
         HEX   5F         ;Test, BCC *-1 ;Loop until disk is not ready
         HEX   6F         ;Test, BCS *-1 ;Loop while disk is not ready

         HEX   A0         ;Exit no-err
```

So this last bit is it. IF the disk is ready, then it loops waiting until the disk is NOT ready. Then it loops until the disk is ready.
And I imagine it's timing that out.
This isn't EVERY track. It's every time the track changes zones. What are the disk zones?

| 0-15 | Zone 1 |
| 16 - 31 | Zone 2 |
| 32 - 47 | Zone 3 |
| 48 - 63 | Zone 4 |
| 64 - 79 | Zone 5 |

Convenient, zone is track / 16.

let's say track to track movement is 5ms
let's say speed change time is 10ms

So when reading blocks in ProDOS 8, it does motor on, but seems to rely on enable=0 to turn the motor OFF. Huh. It never sends a C motor off. ok, let's try John Brook's program now.

ok! I made the "disk switched" sense normal, and, it's only set when the user ejects. GS/OS seems to be working ok with that and not going into spazzy infinite loop.

This is all somewhat improved. I got Brooks test working. Format in GS/OS still fails, complaining about "damaged disk". HOWEVER, it is not giving us infinite hang. 
What I see here, is near the end of track, where a sector should be, there's just a ton of B4 bytes.

So that's telling me we're not terminating a write when we should be and maybe this just keeps writing the same buffer byte out to the disk? yah?

Regular writes are still working fine to floppy, and disk verify after a big write is clean.

Tomahawk still boots. Alien Mind doesn't. It's in a tight loop:
```
loop JSR vvv
BIT $C0ED
LDA $C0EE
RTS
BPL loop
```
So this is Q6=1,Q7=0. and sense 0010, diskIsStepping => 0. But the motor is off.
So, this should change nothing about the stepping state when motor off?

ok, ANY time I modify any of the variables that can modify sense_out, I have to call update_sense.

So we have this in Alien Mind:

[34429517] 3.5 set_enable: 0
[34429532] 3.5 timers: (0,0) stepping_cycles_end: 0, ready_cycles_end: 0
[34429532] 3.5 select_index: 1000 = 8
[34429532] 3.5 select_index: 1000 = 8
[34429532] 3.5 select_index: 1000 = 8
[34429532] 3.5 sense_out: 8 (lowerHeadData) = 0
[34429532] 3.5 set_enable: 1

what is that, 15 cycles? and after this sequence it's still trying to move the head as if the disk is still spinning. But when we did set_enable 0 we instantly turned the disk off. Did the set_enable come from them or from us..
it must be from them, because the only places we do it are in the same block and would show the same cycle count for the off and on. SO.
When did I add the code to turn off motor on enable..
In the alien mind code, 
```
B224: LDA C0E8
  LDA C0ED
  ldy #$F
  bra b232
b232: tya
  eor $C0EE
  AND #$1F
  bne b22e
  LDA C0EC
  LDA C0E9
  lda #$3
  JSR B2CE..
```

John suggests that maybe Alien Mind is using the "wait 1 second to shut off drive" on the 3.5 (which is possible, though not commonly used).


Found a doc detailing the 3.5 floppy interface PAL logic! Wow!

LE (Latch Enable / Multiplexer):

LE = 
	/LSTRB * /SEL + 
	/CA2 * /CA1 * /CA0 * CSTIN * SEL + 
	/CA2 * /CA1 * CA0 * WRTPRT * SEL + 
	/CA2 * CA1 * /CA0 * TK0 * SEL + 
	/CA2 * CA1 * CA0 * TACH * SEL
  

DIRTN (Direction):
/DIRTN = 
	/CA2 * /CA1 * /CA0 * LE * /SEL + 
	DIRTN * SEL + DIRTN * CA1 + DIRTN * CA0 + DIRTN * /LSTRB +	ENBL 

STEP:

/STEP = 
 	LE * ENBL * Q5 * /CA2 * /CA1 * CA0 * /SEL + 
	ENBL * Q5 * STEP * SEL + 
	ENBL * Q5 * STEP * CA1 + 
	ENBL * Q5 * STEP * /CA0 + 
	ENBL * Q5 * STEP * /LSTRB

MOTORON:

in formulate values are internal and all treated as active high

/MOTORON = 
	ENBL * LE * /CA2 * CA1 * /CA0 * /SEL +       # if ENBL and LE and 0100, motoron=1
	MOTORON * /CA1 +                             # if !LSTRB, if x010, motoron=1
	MOTORON * CA0 +                              
	MOTORON * SEL +                              
	MOTORON * /LSTRB                             

this seems like the second group is 
    MOTORON * (/CA1 + CA0 + SEL + /LSTRB) which is a (partial) logical inversion of the AND formula from the 1st line.


EJECT:
EJECT = 
	/CA2 * CA1 * CA0 * LSTRB * /SEL + 
	SEL + /CA1 + 
	/CA0 + 
	/LSTRB + 
	/ENBL

RD (Read Data Line):
IF (ENBL) RD = 
	/CA2 * SEL * /LE + 
	/CA2 * /CA1 * /CA0 * /SEL * DIRTN + 
	/CA2 * /CA1 * CA0 * /SEL * STEP + 
	/CA2 * CA1 * /CA0 * /SEL * MOTORON + 
	CA2 * /CA1 * /CA0 * /RDDATA + 
 	CA2 * CA1 * CA0 + 
	CA2 * CA1 * /CA0     /


Need to think more carefully about the logic, and transitions here. The input states imply certain outputs, and certain transitions.

E.g., let's say we're on 3.5 and enable. And we switch to 5.25. Well, we're going to immediately remove enable from 3.5 and apply to 5.25.

The drive we 'select' is determined by:
dr_enable35 and iwm_select and iwm_enable AND the 555 timer.
This makes the motor_on signal.
selected_drive = dr_enable<<1 | iwm_select

and the 555 timer operation is further activated by mr_motorofftimer.

ok, first step - use a single array of drives to simplify sending commands to drives.

I added a "get_current_cycle()" method to the floppy base because the different floppy methods use different clocks.
and there is still an awkward bit in IWM2. Instead of passing the cycles to advance in, let floppy->fast_forward get its own time.

OK I think I have the write_protect and read_sense mixed up. read_sense is a superset of wp on 3.5; identical on 5.25.

[ ] I ejected Tomahawk and it went to try to read and we got into an infinite loop in the emu. 

Alright, I am now back to Tomahawk working; Alien Mind not.

Next step, modifying the IWM code to not switch on dr_35enable, but on the mode flags.

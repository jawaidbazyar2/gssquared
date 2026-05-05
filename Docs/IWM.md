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
[X] GS/OS gets multiple disk insert sounds when I try to insert the disk in GS/OS. I wonder if it's continually reading a "disk changed" sense from it. (fixed, I think )
[ ] The track and side are not being displayed correctly.

[ ] attempting to Initialize a disk in GS/OS results in no drive activity (no spinning) and the system hanging until the disk is ejected


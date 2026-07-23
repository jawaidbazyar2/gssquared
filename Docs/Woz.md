# Woz Format (and enhancements to Floppy Core)

So the question is: is the fastest path to 3.5" through .woz, or building on my current code base.
current:
the nibblizer / denibblizer would have to be modified for 512 (524?) byte sectors/blocks. It shouldn't be that hard to do. 
the chunk map is 32-bit offset then 32-bit length I think. C340 is the typical length. The lengths do get shorter as we go. 49984 is the short one. Ah, this is in bits.

Remembering back to a2ts, it converts everything to .woz and when you load a non woz format it makes you do a save as. Perhaps that is a security thing, the save as gives it permission to write to the file.

Today, we rely on the calling code having tight timing and assuming it knows the right thing, so it's safe for us to not sync to a clock. however, if we start doing this stuff, we will need to clock-sync, and if it's been a while since the caller did a read, we need to advance the bit we're looking at accordingly. We can do a fast forward - see how many cycles elapsed and just move the position. That math is pretty easy.

Tracking bit position and grabbing bit should be pretty efficient.
```
update_bit_position(cycle_diff) {
  bit_position_in_cycles += cycle_diff;
  bit_position = disk_cycle_counter/4; // (4 cycles per bit)
}

bit = (bytes[bit_position >> 3] >> (bit_position & 0b111)) & 1
```
the bit cells are 3.9xuS, which is -precisely- 4 apple ii clock cycles, though in any given cell one of these might be a hair longer due to stretch cycles. For our purposes, I don't think this matters.

This becomes super-important for writes.

So plan A:
* .dsk / .po -> woz converter
* create Woz parser, read them

The converter can be tested against and validated with cp2.
FloppyImage class that has import, write methods.

ok! Got a lot done with Clod. It wasn't all that much code, really, though the structure is somewhat different.

What seems to be working well:
1. regular disk formats that worked until now (except nib see below).

Weird Things:
1. insert a new disk makes track go to 0 I think and makes the head move sound play, which is wrong. (fixed)

Incomplete things:
1. mount .nib images. it's failing these right now. (fixed)
1. quarter-track support
1. writing. (given the bit-level detail we're at now, this will probably not be hard) 
1. write-back to original file format

Once all this is done, it should be a relatively small lift to get 3.5 working. (famous last words).

## Changing floppy status display

To ease debugging, we should print the fractional track in the HUD. To display we could do .25/.50/.75 but that takes a lot of room. how about .0/.1/.2/.3. 

So track is fixed point with two decimal places, meaning the above.

none of the disks seem to be halftracking. maybe they're trying to quarter and we don't support that..

## half- and quarter- tracking

So - one track movement is two phases of movement.
the Apple II normally uses only the even phases (e.g. 0 and 2) (i.e., even positions), or, whole track numbers in our system.
however, the head can be easily moved to a half track - or, odd phase and left there, because it does go through 0 -> 1 -> 2 -> 3 -> 0, it can just stop on an odd one.

For half tracking: So let's say the head is on track 0 and last phase was 0. We turn on Phase 1 to move the head to track 0.5, and then turn it off and turn on Phase 2 to move the head to track 1.0. there is typically no overlap in the phases.

But let's say we want to position the head on quarter track 0.25. We would turn on Phase 1, AND Phase 0 and hold them until the head settles, then turn each off right away and that leaves us on quarter track 0.25.

Having Phase 0+2 on at same time, or 1+3 on at same time, produces no head movement.

ok, so I'm thinking about rationalizing the magnetic moment (how and where the motor wants to spin to) based on the relative position of active phases.

SO. The code is skipping quarter tracks, and I think I see why. But let's think it through.

It's only changing the magnetic position when we -turn on- a phase. If we turn off a phase, we make no changes. 

Let's assume our position (direction) is phase 0. (We can rotate to make whatever direction we're pointing zero).
One track = 2 phase movements. half track = 1 phase movement. quarter track = 1/2 phase movement.
There are two possibilities: 1 phase is on, or 2 phases are on.
If 1 phase is on, we move to that phase in that direction. Let's further think about this in terms of positive and negative. 0, -1 (or 3, rotate left) +1 (rotate right). 0 and 2 are no movement, either by themselves, or combined with each other. The following phase combinations can cause movement:

| Relative Phases Active | Movement |
|-|-|
| Ph0 | 0 |
| Ph1 | +2 |
| Ph0 + Ph1 | +1 |
| Ph0 + Ph3 | -1 |
| Ph3 | -2 |

"Cur Phase" is basically "current track" low bits, 2 low bits out of the track number.

Let's say we're at 0 and the code turns on Ph1 then Ph0. We'll move +2 on Ph1 active, but then we'll move -1 when Ph0 comes active and they're both on (but at that point it will look like Ph3).
Let's say the code turns on Ph0 then Ph1. We'll move 0 on Ph0, then move +1 on Ph1. (when turned off, we don't move). This -ought- to provide the quarter track locations.

ok, that's basically working HOWEVER when we turn off the magnets in rapid succession we are treating the first magnet off as a full move. When really what needs to happen is there be some inertia. i.e. if we turn them off 8 cycles apart there is not going to be any head movement. I previously tricked around this by ignoring magnet off, and only paying attention to magnet on. well what if I go back to that? NO. the last remaining magnet needs to pull that head over.

so, code is going to keep those magnets on for a while. So when magnet status is changed, set a timer for 10ms to actually move the head. i.e., change the variables, but don't do the move head routine. If they change 2 phase status quickly, the timer will reset after the first change, push it out a bit. This is the way.

This is rapidly getting out of hand! I think I'm going to save the current code (which is non-working) and revert to the code that works but only handles half-tracks, and switch instead of adding write support.

in a diskii, when the data latch is loaded for write, emitting the bits is clocked automatically by the hardware, it does not require a software strobe every 4 cycles.

see attached snippet from Beneath Apple DOS.

Some context from Understanding the Apple IIe:
The above example illustrates the principle of
writing continuous bytes of data: initiate the WRITE
sequence, then store data every 32 cycles. This will
normally be accomplished in 32-eyele loops. After
storing the last byte to be written, wait 32 eyeles.
switching READ/WRITE to READ on the 32nd
cycle. You don't have to write in 32-eyele loops. You
can store 6-bit data words in 24-cyele loops if you can
figure out some purpose for it. You can store data in
any multiple of 4 cycles and the data register will
accept it. Read syneing leaders are written by stor-
ing $FF to the data register in 36-cycle loops (DOS
3.2) or 40-cycle loops (DOS 3.3 or DIIDD).* This
creates a series of 111111110 or 1111111100 strings
which, we will see, synes up the sequencer for read-
ing following data.

So once a write is started, the cpu waits 32 or 36 or 40 cycles and then either does a new write C08D,X (to begin writing next byte of a field) or it does a read C08C,X to turn off write mode.

So we need to track these states, because if there are 36 or 40 cycles to one of those from the last write, there should be 1 or 2 extra 0's written (this is a sync byte).

I'm wondering if it might be easier to implement the diskii using the new nclock cycle_handler functionality, where a function in diskii will be called every cycle in order to update its state machine.
And implement the LSS (logic state sequencer) as part of that.
challenges with that: that function is clocked to the video clock, so always 1MHz, so we'd get no acceleration. And it would fail as you expect if we run the cpu faster than 1MHz. Clocking the disk routine to the CPU clock gets around that, allows acceleration; our pretend disk isn't actually rotating so can speed up to match the cpu, ha ha.

If the motor is off the handler can return immediately.

## LSS (Logic State Sequencer)

The LSS is a 256-byte ROM. the address inputs are:
  R/W
  Read Pulse / no Read Pulse (from the media on the drive)
  Shift/Load
  QA/QA
and the other four bits are SEQ, a 4-bit sequence. The details are page 9-19 of UtA2e. SEQ is updated each cycle from the high 4-bits of the output (so it's a "goto"). and the low 4 bits of the output are a command:
QA is bit 7 of the data register.

| Code | Mnemonic | Function |
|-|-|-|
| 0 | CLR | Clear Data Register |
| 8 | NOP | No Operation |
| 9 | SL0 | Shift zero left into data register |
| A | SR | Shift Write Protect Signal Right into data Register |
| B | LD | Load Data register from Data Bus |
| D | SL1 | Shift one left into data register |

Looking at OE, when its "updateSequencer" function is called, it does a "catch up" to update state for every cycle that has elapsed since the last time it was called. It also does writes all at once. So it's doing a sorta para approach, not calling it every cycle, BUT, doing a full replay when it does get called. 

ok, adding some debug logging. putting into a separate file, why didn't I think of that before. duh.
phases are never being turned off.
First bug, the phases and motor control work on a read or a write. In fact, any of them are going to because R/W is not connected to the bus decoder. However, RESET is and all variables should be set to 0 on a reset. Could the boot ROM be writing registers sometimes? It's NOT. But fix.
ok, I broke the switch updates into their own method and that is strikingly similar to the IWM stuff, as it should be.

ah ok the issue is in ndiskii_woz_controller. and there is an extra layer of complexity in here. these switches should be tracked in the controller, because that addressible latch (command decoder) is in the controller. 

here's the whole hierarchy:
```
ndiskii_woz
-> ndiskii_woz_controller
-> Floppy525_woz
```

So what is the interface to the drive itself supposed to be? the enable; the phases; write signal; write request; read pulse; write protect; 

The phases are not being turned off because that stuff is in the wrong place. That needs to move to the controller.
That's done, and, it is still working. I also have this nice debug log showing it turns the phases on, then off, then next phase on, then off, in sequence. There is no case where the phases are on at the same time! That is likely why this wasn't working before.

the bootrom holds a phase about 20ms. I'm now looking at Commando something and it definitely uses quarter tracks.
turns on ph 1: holds 10ms.
turns on ph 2: holds 5ms.
then turns off ph1, waits 5ms, then turns on ph 3, then holds 4-5 ms and it keeps doing that. I bet it's doing spiral tracking.

This is Blazing Paddles:
read_cmd: 4194024 reg:C sl:0  track=0, cur_track=0, Q7=0, Q6=0 en:1 ph [ 1 0 0 0 ]
read_cmd: 4203969 reg:3 sl:0  track=1, cur_track=0, Q7=0, Q6=0 en:1 ph [ 1 1 0 0 ]  turn on ph1
read_cmd: 4204118 reg:0 sl:0  track=1, cur_track=1, Q7=0, Q6=0 en:1 ph [ 0 1 0 0 ]  then right away turn off ph0
read_cmd: 4215526 reg:5 sl:0  track=2, cur_track=1, Q7=0, Q6=0 en:1 ph [ 0 1 1 0 ]  wait 10ms, then turn on ph2
read_cmd: 4215675 reg:2 sl:0  track=2, cur_track=2, Q7=0, Q6=0 en:1 ph [ 0 0 1 0 ]  right away turn off ph1,
read_cmd: 4252924 reg:4 sl:0  track=2, cur_track=2, Q7=0, Q6=0 en:1 ph [ 0 0 0 0 ]  this is holding ph2 for 40ms?
Not sure about this..

First math adventures:
read_cmd: 5245493 reg:9 sl:0  track=0, cur_track=0, Q7=0, Q6=0 en:1 ph [ 1 0 0 0 ] drive on
read_cmd: 5245597 reg:A sl:0  track=0, cur_track=0, Q7=0, Q6=0 en:1 ph [ 1 0 0 0 ] select 1
read_cmd: 5245795 reg:3 sl:0  track=1, cur_track=0, Q7=0, Q6=0 en:1 ph [ 1 1 0 0 ] ph1 on
read_cmd: 5245944 reg:0 sl:0  track=1, cur_track=1, Q7=0, Q6=0 en:1 ph [ 0 1 0 0 ] ph0 off (immediate)
read_cmd: 5257354 reg:5 sl:0  track=2, cur_track=1, Q7=0, Q6=0 en:1 ph [ 0 1 1 0 ] after 1ms ph2 on
read_cmd: 5262250 reg:2 sl:0  track=2, cur_track=2, Q7=0, Q6=0 en:1 ph [ 0 0 1 0 ] after 5ms ph1 off
read_cmd: 5266790 reg:7 sl:0  track=3, cur_track=2, Q7=0, Q6=0 en:1 ph [ 0 0 1 1 ] after 4ms ph3 on
read_cmd: 5271686 reg:4 sl:0  track=3, cur_track=3, Q7=0, Q6=0 en:1 ph [ 0 0 0 1 ] after 4ms ph2 off
read_cmd: 5276230 reg:1 sl:0  track=4, cur_track=3, Q7=0, Q6=0 en:1 ph [ 1 0 0 1 ] after 4ms ph0 on
read_cmd: 5276379 reg:6 sl:0  track=4, cur_track=4, Q7=0, Q6=0 en:1 ph [ 1 0 0 0 ] phase 3 off
read_cmd: 5313628 reg:0 sl:0  track=4, cur_track=4, Q7=0, Q6=0 en:1 ph [ 0 0 0 0 ]
then it tries to read, then it does a recalibrate. some pattern there is similar to BP.
"when a program reads data from the data latch, the implementation should -not- be clearing the latch."

Stickybear loads initially, but then runs back to around track 1ish, and then hangs trying to read something. Ah, yep, applesauce says it's got the same data from 0.75 - 1.50, so it's likely trying to hit the quarter track there. we're reading constant 0's from the register (which shouldn't happen?) ah, we are apparently in write mode? write_nybble is being called. Q6=1 Q7=0. The bit position isn't changing. So, who is supposed to own Q6/Q7? Technically it should be the controller. Q6 is coming on and it's not going off even after reset.


Take1 fails: bounces between track 1 and 0. same data from 0.75 to 1.25. it's got the same boot screen and 0-1-0 flapjacks that Blazing Paddles does. This uses "CROSS TRACK SYNC" (also blazing paddles). 

Bilestoad initially loads from whole tracks. But then switches to half tracks and reads those ok. NICE, so that works.

Print Shop Companion: loads a bunch then hangs around ~34ish. that is wackytrack. Track 34, this has a section with long runs of 0's (e.g., 16 bits, 20 bits, 17 bits, etc.) It must be expecting noise in there. [ FIXED!!! Indeed, inserting random bits when the source has long runs of 0's makes it BOOT!!!! ]

Wings of Fury: sits near track 0 / maybe 0.2? hangs for a second, then reboots.

So most of the Woz images work, and one that only uses halftracks, definitively works. 

ok, after refactoring to try to add mixed phase detent calculation, take1:
read_cmd: 4301761 reg:0 sl:0  track=2, cur_track=1, Q7=0, Q6=0 en:1 ph [ 0 1 0 0 ]
update_phases: detent: 3
update_phases: track: 3 slice_subtract: 7 slice_add: 1
read_cmd: 4313169 reg:5 sl:0  track=3, cur_track=2, Q7=0, Q6=0 en:1 ph [ 0 1 1 0 ]
update_phases: detent: 4
update_phases: track: 4 slice_subtract: 7 slice_add: 1
read_cmd: 4313318 reg:2 sl:0  track=4, cur_track=3, Q7=0, Q6=0 en:1 ph [ 0 0 1 0 ]
update_phases: detent: -1
read_cmd: 4350567 reg:4 sl:0  track=4, cur_track=4, Q7=0, Q6=0 en:1 ph [ 0 0 0 0 ]

"for long seek (step a number of tracks) two adjacent phases can overlap the phase on time in order to increase the torque of the stepper motor and to reduce the seek time. no matter how many tracks to step, the user has to allow the last phase to be on for 36.6 msec, this timing includes head settling time requirement of 25ms of the drive." -- IWM Info

ok, so they CAN overlap and the idea is two magnets will pull harder and spin the head sprocket faster than one. fine, I get that. 

We need to define the time interval it will take to get from now to target, based on some constant (KEGS uses 2.8ms per track?) 

Locksmith - practically this is the only surefire way to see quarter-tracking.

read_cmd: 65572725 reg:7 sl:0  track=14, cur_track=14, Q7=0, Q6=0 en:1 ph [ 0 0 0 1 ]
update_phases: detent: 7
update_phases: track: 15 slice_subtract: 7 slice_add: 1
read_cmd: 65572763 reg:1 sl:0  track=15, cur_track=14, Q7=0, Q6=0 en:1 ph [ 1 0 0 1 ]
update_phases: detent: 6
update_phases: track: 14 slice_subtract: 1 slice_add: 7
read_cmd: 65598568 reg:0 sl:0  track=14, cur_track=15, Q7=0, Q6=0 en:1 ph [ 0 0 0 1 ]
update_phases: detent: -1
read_cmd: 65598606 reg:6 sl:0  track=14, cur_track=14, Q7=0, Q6=0 en:1 ph [ 0 0 0 0 ]

So this is more like it. We're turning on 7 and 1, then holding about 25ms, then turning both off quickly. But because we're not taking time into account, we immediately jump back to track 14. (half tracks work because there is only one phase on).

on a phase change, OE sets a timer for 0.5ms (500 cycles-ish) out.
if a phase changes within that period, the timer is REscheduled. 
whenever the timer fires, at that point, it moves the head accordingly.

aw heck, the readme.txt goes into detail on each image and what you need to do right to get it booting. 

[X] Print Shop Companion
[X] Blazing Paddles  
[X] Bouncing Kamungas  
[X] Commando  
[X] Crisis Mountain  
[X] Dino Eggs  
[X] DOS 3.2 System Master (using BOOT13 from DOS 33 master)
[X] DOS 3.3 System Master  
[X] First Math Adventure  
[X] Hard Hat Mack  
[X] Miner 2049er  
[X] Planetfall  
[X] Rescue Raiders  
[X] Sammy Lightfoot  
[X] Stargate  
[X] Stickybear Town  
[X] Take 1
[X] Apple at Play  
[X] Bilestoad  
[X] Print Shop Companion  
[X] Wings of Fury Side A / B
[ ] Border Zone - disk 2 loading is very slow (we do not respect bit timing field in woz)

^^^ They all work.

## Long strings of zero bits

headWindowWidth is by default 4 bits (with this mask of 0x0f). 

```
        // MC3470 spurious bit behavior
        if(headWindow & headWindowWidth)
        {
            if(headWindowDelay) value = (headWindow & 0x02) >> 1;
            else value = (headWindow & 0x01);
        }
        else value = (random() % 100) < random1Percentage;
```

This is overly complicated. Just keep the last 4 bits shifted into headWindow. if its value is 0, generate a random bit. (don't use random, use a longish pseudorandom number like 64 bits or something.

if no disk is mounted, return 0's here, and insert the randoms later. 

This comes from the DRIVE, not the controller.

## Thinking about the LSS

Do we have to implement the LSS? We've been working pretty well without it, but, we know some of these disk images don't work and it's likely to do with LSS deficiencies. Maybe we can improve our handling of the various states without implementing the logic state sequencer.

OK, so I borrowed openemulator's abbreviated LSS. I don't think running the real, exact LSS would cost much, and it would probably simplify the logic quite a bit.


## Doesn't work at faster speeds

Does not boot at 14m. Does work at 2.8? seems to.
does not work at ludicrous speed.
could be the track motion stuff I just put in (not long enough at higher speed), or could be related to the fast_forward routine which is using regular cpu_cycles but might need to be vid_cycles. Using vid_cycles will keep the disk spinning at consistent time, meaning it won't be accelerated. Perhaps this can be tweaked to work right at faster speeds. yah, definitely a problem with the seek logic.
1.0-fine. 2.8-fine. 7.1-mostly not working.
currently clocking off 14M, but the number of 14M's chosen is appropriate for 1MHz clock.
try using the cpu clock, not the 14m clock.
Ah ok, I have an eventtimer that runs at 14m, and one that runs at video speed (1M), but I don't have one that runs at the cpu clock? oops.
So, I didn't HAVE a cpu event. I just had to wire it into the main loop in gs2. ugh! well let's try it.. yes, that works, though I don't like having so many checks in the main loop. Isn't working in ludicrous speed. that's weird, it ought to. ok, forgot to add the check to the LS loop.
Overall this costs us a couple mhz at L.S.
(FIXED)

## Test harness

SO, developing a test harness is somewhat problematic as disk code has a lot of feedback loops. i.e., if something is wrong it can cause behavior that is not normally seen and which can skew "expected" results.

However if we break the tests into clear components we should be able to figure things out. So, we record to a log file all the phase motion etc of the drive.

## Bugs to Fix

[ ] if no drive is enabled then we need to deactivate a bunch of the logic, phases, etc. 
[ ] all phases, Q6, Q7 etc should clear on a reset  

Wings of Fury now boots after fixing the bug where we weren't advancing the read pointer on a wpsense access.

## Testing

### DOS 3.2 System Master.woz

This doesn't work, but, it's because DOS3.2 uses different sector markers, e.g. D5 AA B5 instead of D5 AA 96. I don't think there's any other reason for it not to work, from .WOZ files. To be clear, full DOS 3.2 support would also require:
[ ] DOS32 DiskII ROM (should just have a different version of the card, basically the same but with different init)
[ ] conversion on import (based on smaller 116K size?)

etc. Ah, this won't boot, but won't copy II plus read them? let's try that.. ah the dos33 system master has a utility "boot13" that can boot DOS3.2! and it works, WOOT! (but boy is it slow, ha ha)

So for DOS32 we could easily support the different boot rom, and just say it's only compatible with dos32 woz images.

## How does this stuff actually work?

On disk, the polarity of field on a track periodically switches from say N to S or S to N. There is no absolute 0 and 1 - it is the CHANGE of polarity that indicates a 1, and NO CHANGE of polarity that indicates a 0. On a change, the drive generates a "Read Pulse" which is used in the LSS to determine next steps. 

A "write cycle" is four CPU cycles (3.98uS) and a read pulse from the drive is 1 microsecond. In the real hardware, there is a 2uS "mask period" to make sure a read wasn't noise. 

If there is too much time between the field reversals (i.e. long stretch of zeroes) the MC3470 has been increasing its amplification and starts generating random noise read pulses. (This is why we have the random data injection at two points in this code).

If we want to support FLUX tracks at some point, we will need to simulate this field reversal stuff, and that implies a different LSS design (one that follows the real LSS closely). For our purposes right now, we can short-circuit some of the LSS.

Because there is a read pulse once every 4uS (4 cycles) even though the LSS operates at 2MHz, from our simulation perspective its state likely changes only once every 4uS (with each read pulse or no). And each 4uS implies one bit read from the track buffer.

I guess a choice to make is whether to go ahead and implement the LSS as-is. OR try to reimplement its logic knowing we'll just have to implement the full LSS later anyway.

I'm thinking about the fast_forward routine - is it possible to implement a really fast fast forward optimization so if it's been a very long time since last fast_forward, it doesn't take forever. A rotation is 200ms. There can be 50,000 bits there - too many to simulate all at once. (I'm not sure what the current code does). 

So if we do this, we need to generate read pulses based on the track data. i.e. if the current and last bits are different, generate a read pulse. That's an XOR. So that is an output of the floppy525 class. 

Floppy525 class inputs and outputs:
inputs:
    ph0-3
    writerequest'
    write signal -- flux that is being written
outputs:
    read pulse
    wrprotect'
methods:
    fast_forward

Write Signal is generated from A7 that's fed into the LSS rom. So if it's on steps 0-7, it's 0, and steps 8-F, it's 1. 
UtA2e 9-15: "In writing, the state of QA is monitored and the WRITE SIGNAL is toggled at the bit writing interval when QA is set." A toggle (field reversal) is a 1. So yes, the LSS reverses field when QA (hi bit) is set.

So if we're going to use the precise LSS, what we feed into the LSS has to either be massaged to match this field flipping, or the in-memory representation changed to be flux.

The current "bitstream" form is flux at lower resolution after being run through this field flipping.

So our "floppy emitter" can have two versions:
1. one that read pulses from the actual flux of a flux track and injects that into the LSS
1. one that generates read pulses from the bitstream, and injects that into the LSS.

This latter really isn't that hard, it's very simple logic.

I think the above sets us up most easily to handle 2.1FLUX tracks (readonly very easy to play back), and all the current image types.

Then the question is, do we execute the LSS or smart-algo it like we do so many other things.
Two steps in the LSS == 1 apple II cycle. 

the four inputs to the LSS are READ/WRITE; SHIFT/LOAD; QA; RP (read pulse). QA is shift_register[7]. We should only have one shift register, not a read one and a write one. RP is derived from "was last bit different from this one". 

ok let's evaluate the READ LSS Logic. All what we care about here is READ=1 SHIFT=1.
if QA=0, we are basically shifting the bits into the SR from the disk.
if QA=1, everything is NOP with 3 points where we clear the register.
the RP Read Pulse is basically the indication of a GCR bit coming in. So we can interpret "RP" as "the bit from bitstream". OE does not convert these to flux transitions.

A bit cell is 4 cpu cycles. so registers CAN change in the middle of a bit cell and some copy protection does this.

There was an AppleWin ticket noting the behavior of the wp sense is to shift right into register, so if you do multiple wp sense in a row or leave it in that state you will get lots of 1's in the register.

So do I need to do a refactor to properly break up the Drive vs Controller concepts?

so the hierarchy:
```
ndiskii.cpp - just the machine / bus interface.
  DiskII_Controller - handles accesses to the 16 disk ii registers
    Floppy525_woz

iwm_device
  IWM - basically a new controller
   ? Insert the DiskII Controller here as a sub, knowing 3.5 is a little different?
    Floppy525_woz
    Floppy35_woz
```   

ok, after the big refactor, I lost: Miner 2049er II; Sammy Lightfoot. (FIXED! had to keep track of fractional phase difference).
need to check what happens when we switch tracks, validate all that stuff.

So, there are 2 outstanding issues I know of, besides whatever is needed to make the above broken images work:
[X] wp should not merely set hi bit, it should shift values into hi bit, inside fast_forward.
[X] we should not clear latch on every read and should not have this phantom lss register  

I think read should clear the data register ONLY when the hi bit is set (i.e. QA). 

after the "should not clear latch", First Math works!! ALSO DOS 3.2 is a lot faster. 

[ ] .nib 13-sector disk doesn't work, they're the same size as dos 3.3 .nib which doesn't seem right. Some way to detect .nib correct track dimensions?  

ok, I think the write protect stuff is handled correctly now. 

WRITES ARE IN THERE BABY!!!

Locksmith can successfully speed test the disk, and it is successfully nibble-copying a disk (itself).
I am curious about track length. Are woz track lengths typically the same +/- some small nominal amount? If so, this is probably fine. But if a track is short for some reason (uh, the bit writing speed is not 3.910) then we will need to expand the track length when we start writing.
Copy II Plus speed test is also working just fine. I did a lot of copying.
Accelerating seems to make no difference, as it shouldn't, since we clock floppy on the CPU.
Let's re-test the broken images.. just have four not working.

cleaning up the code, removing debug stuff (can't generate a 1.9GB debug file every time we run, lolz).

[ ] improve debug() emitters in diskii_controller and Floppy525.  
[ ] see if we can optimize rdpulse and wrpulse by not calculating the bit index from scratch every single call  
[ ] we did not writeback a .po file successfully

For that matter, are we writing out ANY non-woz files?


[ ] make all the 74LS259 Addressible Latch registers be bools

[X] Have Soundeffects support stereo
[X] have drive 1 play through left channel, drive 2 play through right channel.  




## P6 PROM

Should I eventually choose to go this way:

| Address Bit | tied to |
|-|-|
| 7 | O7 |
| 6 | O6 |
| 5 | O4 |
| 4 | read pulse gated to one Q3 (2mhz) and inverted so RP' |
| 3 | READ/WRITE |
| 2 | SHIFT/LOAD |
| 1 | QA |
| 0 | O5 |


```
000000: 18 D8 18 08  0A 0A 0A 0A  18 39 18 39  18 3B 18 3B  . . . . . . . . . 9 . 9 . ; . ;
000010: 18 38 18 28  0A 0A 0A 0A  18 39 18 39  18 3B 18 3B  . 8 . ( . . . . . 9 . 9 . ; . ;
000020: 2D D8 38 48  0A 0A 0A 0A  28 48 28 48  28 48 28 48  - . 8 H . . . . ( H ( H ( H ( H
000030: 2D 48 38 48  0A 0A 0A 0A  28 48 28 48  28 48 28 48  - H 8 H . . . . ( H ( H ( H ( H
000040: D8 D8 D8 D8  0A 0A 0A 0A  58 78 58 78  58 78 58 78  . . . . . . . . X x X x X x X x
000050: 58 78 58 78  0A 0A 0A 0A  58 78 58 78  58 78 58 78  X x X x . . . . X x X x X x X x
000060: D8 D8 D8 D8  0A 0A 0A 0A  68 08 68 88  68 08 68 88  . . . . . . . . h . h . h . h .
000070: 68 88 68 88  0A 0A 0A 0A  68 08 68 88  68 08 68 88  h . h . . . . . h . h . h . h .
000080: D8 CD D8 D8  0A 0A 0A 0A  98 B9 98 B9  98 BB 98 BB  . . . . . . . . . . . . . . . .
000090: 98 BD 98 B8  0A 0A 0A 0A  98 B9 98 B9  98 BB 98 BB  . . . . . . . . . . . . . . . .
0000A0: D8 D9 D8 D8  0A 0A 0A 0A  A8 C8 A8 C8  A8 C8 A8 C8  . . . . . . . . . . . . . . . .
0000B0: 29 59 A8 C8  0A 0A 0A 0A  A8 C8 A8 C8  A8 C8 A8 C8  ) Y . . . . . . . . . . . . . .
0000C0: D9 FD D8 F8  0A 0A 0A 0A  D8 F8 D8 F8  D8 F8 D8 F8  . . . . . . . . . . . . . . . .
0000D0: D9 FD A0 F8  0A 0A 0A 0A  D8 F8 D8 F8  D8 F8 D8 F8  . . . . . . . . . . . . . . . .
0000E0: D8 DD E8 E0  0A 0A 0A 0A  E8 88 E8 08  E8 88 E8 08  . . . . . . . . . . . . . . . .
0000F0: 08 4D E8 E0  0A 0A 0A 0A  E8 88 E8 08  E8 88 E8 08  . M . . . . . . . . . . . . . .
```

## Disk II Cable (I/O API to the drive)

non-power signals

| Pin | Signal        |
|----------|-----------------|
| 2        | PHS0    |
| 4        | PHS1    |
| 6        | PHS2    |
| 8        | PHS3    |
| 10       | WR REQUEST'   |
| 14       | ENABLE'   |
| 16       | RDPULSE   |
| 18       | WRITE SIGNAL   |
| 20       | WRPROTECT'   |

All signals:
| Pin | Signal        |
|----------|-----------------|
| 1        | GND    |
| 2        | PHS0    |
| 3        | GND    |
| 4        | PHS1    |
| 5        | GND    |
| 6        | PHS2    |
| 7        | GND    |
| 8        | PHS3    |
| 9        | -12V    |
| 10       | WR REQUEST'   |
| 11       | +5V   |
| 12       | +5V   |
| 13       | +12V   |
| 14       | ENABLE'   |
| 15       | +12V   |
| 16       | RDPULSE   |
| 17       | +12V   |
| 18       | WRITE SIGNAL   |
| 19       | +12V   |
| 20       | WRPROTECT'   |





## Detail trace/breakdown of Miner 2049er

miner
bp B958
    when it's on track 2
    it's looking for some sector

BDC4 is a JSR to something to find a sector
26.27 are header decode locations
2E is track, 2F is well it might be sector but it's 254 (volume?)
volume 2f
track 2e
sector 2d
checksum 2c

b98b: looking for 9E E7 (header trailer)

it's putting stuff into sta (48),y -> currently 40A4


T00,S08 FOUND MICRO FUN PROTECTION CHECK
T00,S08,$38: 4C6ABA -> 08B08E

that's JMP BA6A
BEAF - the actual nybble check subroutine

Ba70: LDA 2d (sector)
Ba72: cmp #$0F (yes, we are!)
lda 2E
cmp #$01 - track 1, sector F
we're not, but ..
JSR BEAF
ldy #0
STY 26
STY 27
- that's the code they're looking for below

it's reading a byte, looking for: FF, 9E, CF, if not any of those (we're DF from offset 4961)
if it's an FF it increments 26.27 and branches back to loop.
if it's 9E, it loads Y with 6 and branches back to BED5 - that's a lookup based on Y there


then TAY, lookup from a table BE9F,Y. (A is 7A from offset DF)
then TAY Again - Y now 7A)
now I think we're counting how many FF there are. (there are 550)
well, we read an FF, compares that to BF9F,Y == C01C (wrong!) == C1, doesn't match.
So it's looking for a single nybble after this stuff.

So what does it want to be here? As soon as it saw D5 AA, it is jumping into this mess.
ok, DF is 11011111, then it's just a bunch of 1111111
we know DF is wrong so skip one bit and see what makes sense in the table:
10111111 - BF
be9f+bf = bf5e, which contains AA (nope, still an overrun)

it's doing bpl so it has to have hi bit set, so it's expecting FF?
be9f+ff = bf9e is FF
it does a false read into Be but it's going to read well into c0

the 2nd lookup table seems to be
BF9F: F7 F7 EE DC B9 E7 E7 FF FF FF FF FF FE F9 F9 CF CF CF CF CF 9E 9E E7
      == == ==    ==       ==
24 bytes.

So the 1st lookup table is 24 or 25 bytes. 
BF86 - BFxx =
Bf86 - Be9F = E7
So it's expecting the values it reads to be from E7 to FF

nope it's neither of those. 
9E - 10011110
CF - 11001111

what if we're supposed to catch the tail-end of the AA, that's 10 1101 1111
is 1011 0111 is B7 nope


well I could bp this and see what applewin provides it.

(ok, right, if I set the routine to CLC RTS the game loads. That is definitely the issue.

AppleWin is giving it FF <- so we are not advancing the pointer/latch as much as we should.
AppleWin is also showing the bits shifting into the register (i.e., it's not '0' until the hi bit is set like we're doing)
ok so what it WANTS here is to see D5 AA ..skip.. FF, then read the 500ish FF's, then read a 9E E7 F9
then it does 3 nop
reads a 9E
then it's reading and comparing against a -different- lut address, bfb7,Y
E7, then F9 
then counting in Y the number of FF's after that
and then checking for D5
and exactly 0F of the FF's (well, maybe 16)
and AA, 
and then it's finally exiting. 
holy hell


;-------------------------------
; #MICROFUN
; RWTS jumps to nibble check after
; reading certain sectors
;
; tested on
; - Station 5
; - The Heist
; - Miner 2049er (re-release)
; - Miner 2049er II
; - Short Circuit
;-------------------------------
!zone {
         ; gTrack = 0
         ; Caller found DOS 3.3 RWTS

         ldy   #$09
         jsr   SearchTrack
         !byte $A0,$00    ; LDY #$00
         !byte $84,$26    ; STY $26
         !byte $84,$27    ; STY $27
         !byte $BD,$8C,$C0; LDA $C08C,X
         bcs   .exit      ; passport-test-suite/Miner 2049er.woz [C=0] matches
         jsr   PrintByID
         !byte s_microfun
         jsr   modify2
         !byte $18        ; CLC
         !byte $60        ; RTS
.exit
}

## Image generation and conversion

So we need to be able to import a block file to Woz format, and go the other way, same as we do for 5.25 floppies.

We're going to want to perform a conversion just like CP2 does it. 

We also want some menu items to create new blank 140K and 800K Woz disk images (and maybe .2mg too?)

## FLUX Tracks

So, flux tracks are run length encoded.

Any image with a FLUX track will need to be mounted readonly - we're just not gonna write to them. Well, ok, we could. We could unpack it, read/write it, then repack it to store back on disk. Seems unlikely any of these games are going to try to write to their flux track.

So let's say we unpack it, and pack the flux bits into byte, one 5.25 track will take around 200KB, and longest 3.5 track around 312K bytes. That is one bit per 125ns.

The issue here: can we do a fast-forward using the RLE data? Not sure we can. Much simpler to use non-compressed data.

However (and correct me if I'm wrong) a flux track is still going through the same drive electronics right? So in practice is there anything to do besides compress it down to a normal track?
apparently they think it's different enough.
Wait until June to have Opus take a crack at this.


## Test Regime

in Apple IIe and IIgs each, run the following test regime

Mount prodos 2.4.3 in 6/1 and a dummy disk in drive 2 
1. format drive 2; verify disk
1. block copy drive 1 to drive 2; verify disk

Mount locksmith 6 in 6/1 and dummy disk in drive 2
1. bit copy drive 1 to drive 2;
1. compare driver 1 to drive 2
1. verify drive 2 (quick verify in 16-sect utils)

On IIgs only, run the following test regime
1. boot GS/OS; initialize 3.5 with 2:1 and 4:1 formatting, verify
1. format with 2:1; copy a folder of test files to the floppy; verify


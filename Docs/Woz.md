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

## Testing

### DOS 3.2 System Master.woz

This doesn't work, but, it's because DOS3.2 uses different sector markers, e.g. D5 AA B5 instead of D5 AA 96. I don't think there's any other reason for it not to work, from .WOZ files. To be clear, full DOS 3.2 support would also require:
[ ] DOS32 DiskII ROM (should just have a different version of the card, basically the same but with different init)
[ ] conversion on import (based on smaller 116K size?)

etc. Ah, this won't boot, but won't copy II plus read them? let's try that.. ah the dos33 system master has a utility "boot13" that can boot DOS3.2! and it works, WOOT! (but boy is it slow, ha ha)

So for DOS32 we could easily support the different boot rom, and just say it's only compatible with dos32 woz images.

### Bouncing Kamungas

boots and runs, the thing shows every quarter track through track 0A.75.

### Commando - Disk 1 Size A

skips to track 17, doesn't find what it likes, then hightails it back to grind/0 repeating.  This shows quarter tracks all the way up to track hex 22, 34.

### Take 1 - baudville

loads booter and displays "Take 1" in corner of screen but then bops back and forth between track 1 and 0.

### Border Zone

seems to work

### Planetfall

seems to work

### Hard Hat Mack

good

### Miner 2049er

good


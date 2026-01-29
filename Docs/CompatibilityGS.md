# Compatibility - GS

## ProDOS 2.4

ProDOS 2.4 crashes at FF/2028.  2.4.3 slightly different address. I think this may now be resolved. Do more testing.

## Merlin-16

crashes after displaying MERLIN-16 splash screen, in FF/somethingorother.
Merlin-16 is booting now!!!! (LC Bank 1 fix)

## AppleWorks 4.3

crashes during ProDOS boot - likely same issue as ProDOS 2.4 above.
ah ha! Works now after I fixed the issue with ALTZP not switching in LC Bank 1.

## Thexder

Thexder hangs part way through boot. it uses AUX and LC heavily.  
Thexder on Total Replay loads and runs.
It seems to want to run only out of slot 6.

## ProDOS 8 1.1.1

crashes during boot, likely due to some MMU problem.  
THIS HAS BEEN FIXED. ProDOS 1.1.1 User's disk now booting on both 5.25 and 3.5.

## Apple Panic

Apple Panic is not clearing c010?? that is some wacky stuff. Issue here is that the keyboard buffer is always "enabled". 

On a real GS, if we read C000 w/o clearing strobe, it does change as we type characters. in GS2, it does not, because I didn't write it that way.

So what the hell does the GS do? ah ha!!! By default, GS has keyboard buffering DISABLED. When I ENABLE it, it behaves the same as GS2. So I need to respect the "buffer yes/no" bit and do the right thing.

## Taxman

TAXMAN loads switches to text page 2 and I got no other display..  hit reset, and then it starts working.. ?  ctrl-c switches back to text page 2. huh..  WE'RE NOT SHADOWING PAGE 2, HA HA  

So this works, according to ROM01 where text page 2 doesn't work.

## System 6.0.3

System 6.0.3 Install disk crashes into monitor after doing a bunch of stuff. BUT it does throw up the GS/OS boot shr screen!

Booting pretty reliably now.

## AppleWorks 1.3

works.

## MDT (multi display test)

seems to work all video modes

## A2Desktop 1.5

crashes with progress bar halfway
it's done a ProDOS call to BF00 -> DE00 to find the prodos routine overwritten.
Test correct behavior of:
ALTZP with LC enabled

A couple issues:
when ALTZP is set (well, any aux memory really), if we do a direct read of 1'0000, then we get data from bank 2! We're indiscriminately adding another 1'0000 to the 1'0000.
When, if the lo bit of bank is 1, we should not do that. (Is this that bank latch business?)

ok, ZP switches back and forth correctly page 0 anyway. However, it does NOT switch the LC bank space.

bank_shadow_read first checks LC - and subtracts 0x1000. Then that modified address is run through the aux thing. Except it's page C0-CF and that doesn't match D0-DF.

Oddly, double hires seems to not be working correctly. I only get every other byte-column on the screen. Other double hires stuff is working (Arkanoid, skull island splash). 

Now supports C029 mono mode in rgb (and composite) rendering.

WORKING SUPER DUPER PERFECTO

The text editor/viewer crashes, there is likely some issue with AUX/MAIN bank switching still.

## Arkanoid

Crashes during boot.
It's executing from E0/D100 page, does 00 => C068, and then the code disappears out from under it and hits a BRK.

Indeed, the code they're expecting is switched in if I hit c08b c08b. and 0 => C068 definitely turns OFF LC Bank 1. 
Is there even supposed to be a LC in banks E0/E1 ?

It now boots and plays (without sound!!) IT IS HAPPENING!!

IT NOW PLAYS WITH SOUND!!!!! IT HAS HAPPENED!!

## Airball

same crash location as Arkanoid. 
After fixing State/C068/LCBNK2 was incorrect, we now boot to finder!

The mouse isn't moving, probably because interrupts are not hooked up yet.

## Apple IIgs Tour

boots past a "loading" screen, but then is looping doing ??

Working now.

## Apple IIgs Dealer Demo

boots to "Demo Configuration";
asks for volume; 
tries to load stuff off slot 6 for a while then crashes.

## Qix

sometimes we have a crash booting GS/OS, and then a reboot, and it works.
This feels like there is a s/s not being set correctly on powerup or reset.
At splash screen hangs in tight loop waiting for 1D00 to be non-zero. It's waiting for an interrupt handler to set a variable.
it's just slow. The music is playing like 8 times too slow. Weeeird.


## Gauntlet

Boots - however joystick won't go right or down, only up or left. I believe the timing is wrong. ?pdl(0) in basic works, but, it is slowing down the CPU to read the value. 

## System 6.0.3 Installer

This now boots to the installer! (there may be that "doesn't boot on a first load" issue?)

## Airheart (standalone SANS Crack)

so, this  but then crashes after trying to calculate a slot offset from $2B. $2B in some docs says "boot slot * 16". we lsr that four times. ok fine.
Except when this code runs $2B just contains 7. 7 is right slot but in wrong place. 
Well, funnily enough, this is a 143K disk, not an 800K disk. So I bet its bootloader is just doing the wrong thing because it's expecting a 5.25.

## Donkey Kong

Plays very well with keyboard. Joystick auto-detect not working.

## Total Replay

### Airheart

crashes after displaying splash screen


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

## Bard's Tale II: Destiny Knight

My recollection is this worked ok.

## Christmas Music (Christmas.2mg)

Error: 13 (from PlayMidiStartup).
2/18/26: the midi tool is now starting, and playing a song.. way too slowly, just like QiX. Still, this is a big improvement over crashing.

## FTA XMAS Demo (FTAXMAS)

immediately hits BRK at 5300 after loading boot block. I read this loads stuff from disk while animations are occurring, so this may depend on directly manipulating the IWM.

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

With Floppies enabled, hangs forever trying to read floppies.

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

you can select some things in the UI via keyboard, but joystick is not responding.

## Apple IIgs Tour

boots past a "loading" screen, but then is looping doing ??

Working now.

## Apple IIgs Dealer Demo

boots to "Demo Configuration";
asks for volume; 
tries to load stuff off slot 6 for a while then crashes. is likely expecting IWM in slot 5.

## Qix

sometimes we have a crash booting GS/OS, and then a reboot, and it works.
This feels like there is a s/s not being set correctly on powerup or reset.
At splash screen hangs in tight loop waiting for 1D00 to be non-zero. It's waiting for an interrupt handler to set a variable.
it's just slow. The music is playing like 8 times too slow. Weeeird.

## Rastan

gets a little further now, you can start the game, but the game screen draws as the guy falls on the left, eventually screen blacks and audio keeps repeating in a loop. Getting closer!
Likely a Ensoniq issue.

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

## NoiseTracker

there was a MMU bug involving RAMRD/RAMWRT and direct access to bank 1. (we were adding an extra 0x1'0000 erroneously).
It now starts but then hits a BRK; it plays music for a while; then dies with a "RESTART SYSTEM". Could be Ensoniq IRQ or another memory issue.
2/18/26: gets a little further now before crashing, it displays a 3d Ball pyramid.

## Senseiplay

similar to noisetracker, now starts and plays a song for a couple seconds, then fails with a "RESTART SYSTEM-$01".

## Telcom 0.28 (1991)

my baby!

this crashes to a BRK on GS2. It does NOT crash on Kegs. Something is doing a tool call 2403 to call 00/C2CC in emulation mode. (Why?) This routine exits with a JMP $4xxx, meaning, it's expecting the program bank to be FF, not 00. So something in that stretch between 14/6360 (can't guarantee it's always here, but, it's the JSL $E10000 before the BRK).

^^ This likely points to an MMU bug?

YES. See DevelopLog.md Feb 18 2026. 

Telcom now runs. (Issue was IO space wasn't being mapped into bank E1 correctly).

## Total Replay

### Airheart

crashes after displaying splash screen (same as on IIe)


# Compatibility - GS

## ProDOS 2.4

ProDOS 2.4 crashes at FF/2028.  2.4.3 slightly different address.

## Merlin-16

crashes after displaying MERLIN-16 splash screen, in FF/somethingorother.

## AppleWorks 4.x

crashes during ProDOS boot - likely same issue as ProDOS 2.4 above.

## Thexder

Thexder hangs part way through boot. it uses AUX and LC heavily.  

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

## AppleWorks 1.3

works.

## MDT (multi display test)

seems to work all video modes


# Compatibility

## Apple Pascal - Disk 0.po

Does not boot. Loads some stuff from the disk then says "NO FILE SYSTEM.APPLE" or something like that. Same on both emulators.

## fireworks.dsk

Apple II+.

FIREWORKS.BAS works (it's just applesoft).

hello + fireworks (B) does not work. It is apparently doing some crazy cycle-timed stuff, switching display modes using very tight timing.
So this is "mixing video modes". This won't work with our emulation approach at all.

## Crazy Cycles.dsk

This is intended for Apple IIe only. hangs on boot waiting for VBL. Then it hangs trying to read C070 - which is likely supposed to return "floating bus" value. At least, it does in OE.

## crazy cycles 2.dsk

Apple II+.

when booting, displays "MB#" But disk is still running. it's loading a LOT? trying in OE it sounds like the head is scanning back and forth.
Nothing happens.
Same on OE with II+ and IIe. I guess this is just not working.
in 0.3x, we don't even ever get off track 0, like it's not booting correctly.

## four_voice_music.dsk

A random collection, but the most interesting thing here is a demo program that plays 4 voice music pieces.
Comparing OE to GS2, GS2 is accurately replicating some extremely high frequency harmonics. The overall tone is a little buzzier, but we know that already.

## keywin.2mg

OE does not support .2mg files.
Does not boot on GS2, disk spins forever and no visible boot process proceeds.
This is an 800k .2mg disk. Still won't boot on GS2.

## Lode Runner

works in OE.
in GS2, we throw a bunch of "unknown opcode" errors on the value 0x80.
What if we don't halt but just skip the instruction? ok, well now it's starting up. looks good actually. 
there's no audio...
OE has audio. according to Claude you can toggle the speaker by hitting any address in C030 to C03F.
I added that to GS2, and still no audio in Lode Runner.
there is lode runner disassembly at github.
Weird. So Control-S enabled audio. Seems like maybe there is some reason sound is disabled by default?

## timelord.dsk

gs2: prodos 8 v1.9 boots partially, cursor flashes over a . in the upper left, but does not complete booting.
oe: boots and it's a pretty good synthesizer using the apple ii speaker.

Timelord now boots after the Disk II head movement fix.

## applevision.dsk

works correctly.

## prodos 1.1.1.dsk

Works in GS2, but:
"Display slot assignments" shows "USED" for slots that don't have stuff in them. It should read "EMPTY".
It thinks there is a 80 column card in slot 3. That ain't right.
It thinks the slinky card is a "PROFILE". (This is correct for Prodos 1.1.1).

The issue with this could be that we are supposed to be reading floating bus values when c100-c1xx is read and there is no card in it. We should have a page table setting for "floating". And then pick a bit of display memory at random and return those values. ("At random" in other emulators would be based on where the display update virtual beam scan is. We don't do it that way. )

## ProDOS1.9.po

This crashes on boot at address 0x0914 in both GS2 and OE, on both II+ and IIe modes. I think this disk image is bad.

## ProDOS_2_4_3.po 

Works in II+ on OE. 
Hangs during boot on GS2 after showing the prodos splash screen. Drive light is still on.
Will need to trace this and see what's going on. Is supposed to work on Apple II plus and standard 6502. So who knows what is going on..
Though, OE II plus seems to have an 80 column card that activates on PR#3.
Now working just fine.

## Locksmith

### 6.0 
"Scan Disk" is not working.
locksmith ability to quarter track isn't going to work the way the code is written now.
16 sector fast disk claims "address" field is missing on every sector and track. I wonder if this is all due to lack of quarter-track support.
This did not work in the previous iteration of the code either.
All the 16-sector utils now work (format, verify, etc.) Nibble copying does not.

### 5.0
nothing working here either. It's never turning the disk drive motors on. it must be doing it some different way.
probably same status as 6.0.

## Glider

This boots far enough to say "LOADING GLIDER..." but then hangs. Docs say on a II+ it requires a mouse.
hard looping at $0E86. This is a tight loop BIT $C019 BMI $0E86. I.e., it's looking for NOT VBL. Then it looks for VBL, and exits when it has seen VBL not and then VBL plus.

The Apple II plus did not have the VBL register. So this software not compatible with Apple II plus!

Glider now boots, but, keyboard control doesn't work - it's reading $C010 and expecting the keypress to be there.

## Wall Street

reads a couple tracks then fails with:

Unknown opcode: FFFFFFFF: 0xFA

we're looping on jmp (3f0) -> FFFF -> BRK.


## Clue

boots and is working, but, disk drive continues running inappropriately. 

## Classic Concentration

Dies during game load with gazillions of unknown opcodes. last instructions before crash are sta c003 sta c005 - looks like a //e 128k game.
With current state of IIe, it now boots, loads, plays music. But it is a double hi-res program, so the display isn't right yet.
getting there, but still wrong. 

## Karateka (Brutal Deluxe crack)

seems to be working quite well! (fixed inability to run by updating joystick code)

## Alien Typhoon

in paddle mode, even when joystick is still, the paddle reading is very jittery. There are a few other games where this occurs too.

## Ascii Express

Ascii Express 3.46 loads. Of course, we don't have any serial ports on here right now. But, it also boots up into 80 column mode when it sees the Videx Videoterm! ha!

## Wizardry

if there's a Videx, wizardry (aka Pascal) turns it on. Then all you see is the Videx screen. Huh. there's a whole video from Chris Torrance about this.

## Epyx Preview Disk

baseball ok. Winter Games, something's not right, is it trying to double-buffer and it isn't triggering correctly?
Is now working ok in IIe mode.

## Skyfox

working as of 0.3x with mockingboard support. both standalone disk version and version on Total Replay. Was a language card problem.

## Rescue Raiders

boots - get the demo screen. 's' to start. terrorists have been found at Cherbourg - then it sits in a loop at 8147 - I put a 0 in address $59 a few times and it started workingish. at least I have animation, if not audio. ah ha! This is why:
https://comp.sys.apple2.narkive.com/7TzX1OSL/rescue-raiders-v1-2-and-1-3-questions
here's the manual for reference:
https://archive.org/details/rescueraidersusersmanual/page/n17/mode/2up
Requires speech synthesis support for Mockingboard.

## Total Replay

boots up detects 64K and mockingboard and then thinks "and it talks!" . uh. wrong. but lots of the games on it work, including Rescue Raiders.
Correctly detects 128K + joystick in apple //e mode, but then hangs waiting for VBL to appear.
ok, VBL is on and Total Replay boots to menu now.

## Carrier Aviation 

seems to work. don't know how to play :)

## Shufflepuck

needs: Mouse; VBL; 
gets past VBL check, but still dies on no mouse.

## Cybernoid Music Disk

works, but has lowercase. 4.0 version is Prodos 2.0.3 needs enhanced apple iie. [works now]
"DOS3.3" is Cybernoid 5.0, works. wants a mockingboard.
Mixing is not great.

## Apple Cider Spider - 4am crack

works with mockingboard. game controller not working with it? I thought it used to. weird. maybe I used the keyboard. In any event, it's doing a write to $C070 to reset. We're not handling that!
Working now.

## Apple II Bejeweled

looks like requires //e 80 column card.

## Jumpman

crashes after loading a few tracks of data; then it has also gone back to C600 and jumps again to $0801 which is an RTS which then wreaks various havoc, gets to a BRK, and ends up in monitor.
Boots now, and "plays", but there may be an issue with the joystick. only seems to be able to go up and left.

The gamecontroller timer used to be 2800 cycles. now I set to 2805 cycles, based on the PREAD routine 11 cycles * 255. Controls in jumpman work now.

## Ultima IV

Works and supports Mockingboard on II Plus. Now works with Mockingboard on IIe.

## Ultima V

Is now booting on IIe emulation with 128K. I should stick a Mockingboard in it and see if it works! It doesn't. ha! Fixed a bug in MB with new mmu routines for c1cf, still doesn't work. Fully implemented T2 counter and interrupt; still no dice.

## AppleWorks 1.3

on 2mg disk. Boots right up, but, needs 80col support. There is now 80 col support and it now works! (0.35+)

## ProTerm 2.2 on 143K

boots prodos, but crashes to a brk at 8ab0. Might require enhanced IIe. Does want enhanced iie. boots, says confirm hardware, hit ESC to continue but just loops asking for keyboard input.

## Zork I

works great. uppercase only.

## Hitchhiker's Guide to the Galaxy

works, asks if you want 80col mode. Would probably provide lowercase then.

## Nox Archaist Demo

Nox Archaist Demo hdv from the apple2ts source tree boots - but then hangs on C019 waiting for a VBL that will never come..
The VBL has now come! Why would it care about vbl? Don't know. Gets boot screen, plays the "Nox archaist" voice, and some mockingboard music.
Then hit a key, and it dies with the PC in ZP and ALTZP on. 

Well, it stops running correctly on Apple2TS also. Maybe the Demo just stops there.

## Total Replay

### AirHeart

crashes after doing some STA C081,X; STA C082,X; STA C083,X. that is lang card stuff and maybe phantom-read stuff.

## Berzap

boots. ctrl-c lets you configure mockingboard. there might be an attempt at speech which doesn't work of course.

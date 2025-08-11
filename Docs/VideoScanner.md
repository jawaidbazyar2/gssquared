# Video Scanner

The "Video Scanner" is a component that emulates the video generation circuitry in an Apple II and provides the following services:

* Floating Bus 
* VBL status
* Video frame generation

## Structure

There is a VideoScannerIIe, VideoScannerII, and then the frame-based stuff is completely different. 

## Floating Bus

The Floating Bus is what is read by the CPU when no device on the bus claims the access. This can happen when memory (RAM) is less than 48K, or, in certain I/O registers. The value of the floating bus is based on whatever value from video memory the Video Scanner read during this cycle.

## VBL Status

the Video scanner tracks, per cycle, current monitor horizontal and vertical counter values. These track the monitor beam position. VBL is a flag that indicates when the monitor is in the vertical blanking period, i.e., between the last pixel at the bottom and the first pixel at the top.

## Video Frame Generation

the prime purpose is to generate, per-cycle, video output by feeding video memory data into the video output routines. GS2 has two modes:
1. cycle-accurate video where the video data is streamed as every cpu cycle ticks
1. frame-based video where video data as of each frame render is taken at that time.

The first mode is used to allow running programs that switch video modes under tight timing control. Regular applications don't do this - this is for demos that do crazy stuff that is programmer flex but has little practical value - with one exception, that of "3200 color graphics" on a IIgs.

cycle-accurate really only makes sense for the defined-clock-rate cpu modes - in free-run mode there is no relationship between cycles and time elapsed. Also, c-a has significant overhead.

Even in a VM with c-a enabled, when we switch to ludicrous speed we want to disable c-a video mode. So we need the ability to switch this on and off during runtime.

The frame handler can check the clock mode and activate/deactivate. (That is working with a few bugs).

# Counting Time instead of cycles

when operating at more than 1MHz, we need to track time elapsed instead of just counting cycles. This will be important especially for IIgs where cycle time is constantly changing.

# Bugs / TODO

[ ] display the hcount/vcount after each frame render, and track to see if we are getting a whole frame out.  
[ ] when in framebased mode, split text is showing in green monochrome.  
[x] when in cycle mode, disable video memory shadowing.  

I set cycles per frame to 17030., and speed 1.0218mhz. 
yeah, the vertical position at end of frame is inching backwards from 260something to 0 and then starting back. Wherever the scanline is at end of frame, we typically have the wrong video mode. E.g., in sather.a, I have V 19. at V 19 it's in hires mode but 18 and 20 are lores mode. this is being drawn this way every time. So, wherever V is I have some kind of discontinuity. 
One fix is to make sure every frame starts the scanner at H=0 V=0. I tried that, and got very odd results. So I'm ending the frame having wrapped around. ok, now I'm forcing the h/v to end of frame (64/261) and sather is working w/o discontinuities now..

This does seem to fix this issue, but of course speaker output is distorted because I've changed cpu frequency.. tweaked speaker.hpp constants.. also, now setting end of frame v to 64/159, which makes fireworks more happy. whenever we have finished a frame, we then start vbl. (vbl is 160-262..)
mockingboard timing might be off now too.

except now crazy cycles is wrong, the bars don't quite go all the way across the screen. they stop about 5% early.

# The Cycle and Timing Conundrum

ok. We have a conundrum.

The Apple II clock operates at 1.023MHz, but every 65 cycles, the cycle is stretched by two cycles of the 14M clock, in order to keep the cpu clock synchronized with video timing.

https://mirrors.apple2.org.za/ground.icaen.uiowa.edu/MiscInfo/Empson/videocycles

WMs code didn't deal with that it seems. (I mean, it works anyway).

So, the issue is all my prior code to this point assumed 1020500 cycles/sec - the -effective- CPU clock rate once taking the stretched cycles into account. But, 

It's 262 * 65 cycles = 17030 cycles exactly per frame, and again, one of those is stretched a bit.

Sather IIe, pages 3-15 and 3-16 show the details here:
H Clocks 40-64 are h blank. v clocks 192-261 are vblank, and specifies /VBL. 
And, H hitting 40 is what causes the vertical line to increment.

One video frame is 17030 cycles. There are only 59.94 video frames per second! this might be my hangup. It's not 60fps at all.

[ ] The mockingboard isn't quite right since I changed the cycles, some stuff is at the definitively wrong value. on a reset and reboot it plays correctly..

ok I may have fixed the issue, I was "reset"int the vertical counter to 159 instead of 261, duh. 
nope one part of crazy cycles is still wrong, it's part 3 of crazy cycles 2, a bar doesn't go all the way to the right hand side of screen, it's short about 2 cycles worth.

ok, the really weird one is the last on crazy cycles 1, it's all text, except, the F and T that are scrolling sideways are starting in every-higher positions. it's switching between text and hi-res. So, the frames aren't starting on the right video address.

Another difference, the homer pic that has hires on the left and text on the right, in OE the whole display shows with no color - i.e., OE has no colorburst at start of line. Likely because the colorburst starts at h counter 53. We're using whatever color mode is at start of scanline, but maybe it's supposed to be whatever is at end of scanline.

now we don't really care about all these bits and how they count weirdly in an apple ii. I can just count hcycle and vline. and generate the LUT accordingly.

OK, so one frame is exactly 17030 cycles.
But we might run anywhere from 17030 to 17037 cycles.
In the synced modes in CC, the H and V are typically always the same at the end of a frame.
is my VBL working? yes it looks correct.
OH. In IIe mode it's working more better?! (53/254)
2nd phase, 42/258
maybe it's a 65c02 thing?!
3rd phase, H is incrementing once per frame and V is thus incrementing once per 1.1 seconds.
second run - 16/46, and there's a line through the middle of the display.
2nd run 2nd phase: 13/50, line on display.
however, in 3rd phase, the FT is not slowing inching up the screen.
how does the V get desynchronized? Shouldn't this thing be using vbl? it is.. 

THINK ABOUT HOW THIS WOULD PLAY OUT.

17,030 cycles - if we underrun (i.e. do less than 17,030 cycles) then we're going to be in VBL and it will be fine.
If we overrun (more than 17,030 cycles which is what is 99.9% certain to be the case) then we will exceed the vbl, and we will go back to the first scanline and start overwriting video data there.

so.. but when we reset, it will reset to scanline 0 and the leftmost pixel. 

setting to 0,0 and set_line(0) sort of works, however, this does desynchronize from the actual cycle count. (the FT moves up the screen).

So what we want to do here, is if we have exceeded 17,030 cycles:
  do not allow drawing any more pixels into the frame buffer. i.e. if we hit line 262, STOP, don't wrap.
  we need to re-play the first N fetches of the next frame.

So instead of putting into a frame buffer, maybe what we do is write video data and modes into a linear wraparound buffer. Then to create a frame, we take exactly 17,030 samples from it. So if there were a few too many, they are grabbed as part of next frame as they should.
Each frame, we will have exactly 17,030 samples, or a few more. If there are a few more, these will continue to accumulate. So, we want to run CPU for 17,030 - EXCESS_CYCLES each loop. Then we won't accumulate extra samples forever.


in split demo, first time run from power-up, one of the oscillators volume is super low. Hit reset and reboot, and the volume is normal. Then at a certain point it stopped playing correctly, the other volume went completely to zero. So weird.


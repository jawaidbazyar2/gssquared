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

[ ] reset the hcount/vcount after each frame render, and track to see if we are getting a whole frame out.  
[ ] when in framebased mode, split text is showing in green monochrome.  
[x] when in cycle mode, disable video memory shadowing.  

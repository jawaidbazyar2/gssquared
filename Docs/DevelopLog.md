## Display Handling

It's not easy to see the pattern in the character ROM, but it's there.

```
0x08 xx001000
0x14 xx010100
0x22 xx100010
0x22 xx100010
0x3E xx111110
0x22 xx100010
0x22 xx100010
0x00 xxxxxxxx
```

Apple II text mode : still uses 280x192 matrix.
Each character is 7 x 8 pixels.
280 / 7 = 40 columns.
192 / 8 = 24 rows.

Pixel size: Each character is made up of a 5x7 pixel bitmap, but displayed as a 7x8 character cell on the screen. there will be 1 pixel of padding on each side of the character.

Testing the TIGR library.

It looks like you do the following:
    create the display window.
    draw various things to the buffer.
    update the display.
So we can accumulate changes to the display memory/display buffer, and only update the display when we are ready to show it.
Say once each 1/60th of a second.
I guess we'll see how fast it is.
To start, update buffer every time we write to the screen.

well that sort of works! don't know if it's fast enough, but ok for now.

trying to boot actual apple 2 plus roms. Getting an infinite loop at f181 - 
this is testing how much memory by writing into every page. unfortunately everything
in the system right now is "ram" so it's never finding the end of ram ha ha.

OK that's fixed and it now boots to the keyboard loop! Though the prompt is missing, and there is no cursor because there
is no inverse or flash support right now.
Tigr doesn't look to be very flexible for keyboard input. It is very simple but may not do the trick.
Next one to try: SDL2: https://www.libsdl.org/

---

## Looking at SDL2

Downloaded source from:
https://github.com/libsdl-org/SDL/releases/tag/release-2.30.10

If you are building "the Unix way," we encourage you to use build-scripts/clang-fat.sh in the SDL source tree for your compiler:

mkdir build
cd build 
CC=/where/i/cloned/SDL/build-scripts/clang-fat.sh ../configure
make

the main GIT repo is version 3.0.
Lots of deprecations warnings, but it built.

Well, this is the easier way:

```
bazyar@Jawaids-MacBook-Pro gssquared % brew install sdl2
==> Auto-updating Homebrew...
Adjust how often this is run with HOMEBREW_AUTO_UPDATE_SECS or disable with
```

I'm going to go ahead and scrap the Tigr code because SDL2 looks like it will do the trick.

## Keyboard Logic

What better way to spend Christmas than writing an Apple II emulator?

SDL2 is better. I got the actual character ROM working. Looks pretty good.

Now working on the keyboard.

Currently keyboard code is inside the display code. This is not good.
SDL's use scope is broader than just the display. So we should move this out
into a more generalized module, that calls into display and keyboard 
and other 'devices' as needed.

OK, the SDL2 keyboard code is segregated out properly, and the keyboard code is working pretty well.

If a key has been pressed, its value is in the keyboard latch, 0xC000. The Bit 7 will be set if the strobe hasn't been cleared.
Clear the strobe by writing to 0xC010.

Looking at where I hook this in..

This is the SDL2 documentation for the keycodes:

https://github.com/libsdl-org/SDL/blob/SDL2/include/SDL_keycode.h



## Dec 25, 2024

I'm going to go ahead and scrap the Tigr code because SDL2 looks like it will do the trick.

For reference, the current design target is Apple II+ level.

I have some options for the next piece to tackle. Generally speaking, here is the project
status:

1. NMOS 6502 - (Complete.)
1. Keyboard - Complete. There may be tweaks needed.
1. 40-column text mode - Complete. Supports normal, inverse, and flash.

1. 40-column text mode - Screen 2. (Complete)
1. Low-resolution graphics - not started.
1. High-resolution graphics - not started.
1. Disk II Controller Card - Not started.
1. Sound - not started.
1. Printer / parallel port - not started.
1. Printer / serial port - not started.
1. Cassette - not started.
1. Joystick / paddles - not started.

Pretty much all of this is carried forward into the Apple IIe, IIc, and IIgs,
with the exception that the IIe non-enhanced uses 65n02, IIe enhanced uses 65c02, and IIc 
does also. Of course, the IIgs uses the 65816.

What is the best piece to tackle next? I think I'll work on the graphics modes. Not much I
can boot off a disk will do much without them.

## 40-column text mode - Screen 2

Poking 0xC055 will enable screen 2. It's still a 40-column text mode but it uses
memory at 0x0800-0x0BFF instead of 0x0400-0x07FF.

0xC054 will return to screen 1. These two flags affect text, lo-res, and hi-res graphics.

This should be a pretty easy piece to tackle.

We'll need:
   handlers to catch the accesses to 0xC054, 0xC055. When these are accessed we change
   the display buffer base address, and redraw the whole screen (only if the mode has changed).
   And we need to monitor for writes to the new display buffer base address.

This is where I want to implement the jump table for handling the 0xC000 - 0xC07F space.

### Low-resolution graphics

The low-resolution graphics are 40 x 48 blocks, 16 colors. Each nibble in a display page
byte represents a block. These will be 8 x 4 pixels in terms of the current display
buffer design. The color is one of a specific palette of 16 colors:

https://en.wikipedia.org/wiki/Apple_II_graphics

### High-resolution graphics

Implemented in fake green-screen mode.

Bugs:
[x] text line 21 does not update in mixed mode.


## Dec 26, 2024

Oddly, when I switch to screen 2, the display comes up. 
And when I write to 0x0800 with a character, it displays.
but it's not supposed to. txt_memory_write is not being called.
so it must be the flash handler. Try disabling that and see if the screen
doesn't update as expected.
if so, then the flash handler is updating too much.

Testing emulation speed.
Every 5 seconds, print the number of cycles since the last 5 seconds.
it's hovering around 1000000 cycles per second:

```
3747795 cycles
5194629 cycles
5233284 cycles
5287931 cycles
5082248 cycles
```

the low number was when generating tons of video display debug output.
I am running in the free-run mode, so slowness is not due to the clock sync emulation
let me turn off the display.
ok the display update is off, and it's in the same area. And it started at about 6M and worked its way down to 5.1.
it's pretty consistent..

```
5796507 cycles
6093662 cycles
5983934 cycles
5550091 cycles
5107896 cycles
5123350 cycles
5147278 cycles
5142563 cycles
5118742 cycles
5123895 cycles
```
am I sure I'm in free run ? I am.

SO. I was calling event_poll() every time through the instruction loop. So. Many. Times.
I put it inside the check for the display update, once every 1/60th second. This is per
docs that suggest "once per frame".

Removing that, I get a effective MHz speed of 150MHz, with optimizations -O3 enabled.
With -O0, I get about 61MHz effective, at free-run mode.
And of course I can turn off free mode.

I should implement some keyboard commands to toggle free-run mode. F9. That's convenient.
Maybe a key to toggle certain debug flags?

F9 works well. I should have it toggle between free-run, 1MHz, and 2.8MHz.

ok, that is working. I changed free-run from a flag, to a enum mode. So we can add other modes and even specify the exact mode.

This is a fun little program. Appleosft, and all text mode.
https://www.leucht.com/blog/2016/09/my-apple-iie-a-simple-text-based-arcade-game-in-applesoft-basic/
works just fine. All the scrolling causes the emulator to slow down to 3MHz effective rate.

It may end up being better to just draw the screen 60 times a second, and not worry about updating for every character. That feels weird, but, this
may be the modern way. 

ok let's think about this. 16 milliseconds per frame. 0.000087 seconds per scanline. (192 scanlines * 60 frames, 11520 scanlines per second).

I should only be copying the back buffer each frame, 16ms. this slows down to 1MHz:!
10 A=1
20 PRINT A
30 A = A + 1
40 GOTO 20

So something is wrong. This ought to FLY.

So let's try that other approach. Instead of updating the display for every character,
we'll just redraw the screen every 16ms. Will this be faster?

## Dec 27, 2024

Big performance drain on the video display.

Something that causes tons of scrolling:

795867523 cycles clock-mode: 0 cycles per second: 159.173508 MHz
796674469 cycles clock-mode: 0 cycles per second: 159.334900 MHz
218963831 cycles clock-mode: 0 cycles per second: 43.792767 MHz
5476743 cycles clock-mode: 0 cycles per second: 1.095349 MHz
5443788 cycles clock-mode: 0 cycles per second: 1.088758 MHz

Effective performance drops to 1MHz.

Sample shows:
Call graph:
    8077 Thread_49405054   DispatchQueue_1: com.apple.main-thread  (serial)
    + 8077 start  (in dyld) + 2840  [0x183e2c274]
    +   8077 main  (in gs2) + 376  [0x1006abf00]
    +     7981 run_cpus()  (in gs2) + 112  [0x1006abcc8]
    +     ! 7949 execute_next_6502(cpu_state*)  (in gs2) + 2052  [0x1006a9060]
    +     ! : 7943 store_operand_zeropage_indirect_y(cpu_state*, unsigned char)  (in gs2) + 120  [0x1006ab78c]
    +     ! : | 7938 memory_bus_write(cpu_state*, unsigned short, unsigned char)  (in gs2) + 52  [0x1006ac40c]
    +     ! : | + 7922 render_text(int, int, unsigned char, bool)  (in gs2) + 488  [0x1006addf8]
    +     ! : | + ! 7078 SDL_UnlockTexture_REAL  (in libSDL2-2.0.0.dylib) + 412  [0x100878ee8]
    +     ! : | + ! : 7075 SDL_UnlockTexture_REAL  (in libSDL2-2.0.0.dylib) + 324  [0x100878e90]

we're doing tons of SDL related calls. They're probably fairly expensive ones.
I'll note that if we're doing a scroll, then we're reading every character on the screen (pure
memory copy is fast), but then writing every character on the screen - which generates a call to 
render_text for every character on the screen.
Render character is a loop in C that is 56 total iterations; so that's around 60K iterations of generating
the character bitmap.
Let's get rid of that inner loop by pre-generating the character bitmap.
Each Apple II character is 7x8 pixels. One bit per pixel.
The output we need to feed into SDL2 is 32-bit values, one per pixel.
So one character becomes 64 bytes, pre-coded. 

ok well that generated this:
5468271 cycles clock-mode: 0 cycles per second: 1.093654 MHz
5356073 cycles clock-mode: 0 cycles per second: 1.071215 MHz

basically no change.
I still have a loop. And while I unrolled it, I am still copying from main RAM to the texture.
So that's expensive.
Can I create a second texture that just contains the character bitmaps, and then copy chunks of
that to the screen?

ok look at row at a time.
txt_memory_write is called for each byte written.
currently it calculates row and column from the address.
Now, it will just calculate row, and mark the row as dirty.
on a screen update, we recalculate the dirty rows and render them.

Wow, holy cow. So what I implemented:
dirty line checker. (24 element array)
txt_memory_write is called for each byte written. But this now ONLY marks the line as dirty.
The actual rendering is done in update_display which is called every 16ms.
That scans the dirty line array. For each dirty line, it renders the line.
I can do 2000.7fff in the rom monitor - which dumps 28KBytes of memory.
It occurs virtually instantly.
This is because no matter how many characters are written to vram, the actual rendering
is of at most 24 lines, 60 times per second. So that limits the number of locks.
So, this could probably render all 24 lines at a time no matter what. I guess I should
try that because it would simplify flash mode etc. Flash wasn't hard at all - just
identify screen rows that have a flashing character, then mark the row as dirty. And
move the flash state to a global so it's accessible to the other routines.

### Apple III info:

For reference:
https://www.apple3.org/Documents/Magazines/AppleIIIExtendedAddressing.html

## Dec 28, 2024

The clock cycle emulation is a bit off. I am sleeping the calculated amount. But, at start of each cycle
we need to do:

When CPU is 'booted', we do this:
next_time = current_time + cycle_duration_ticks

do work

when we call "emulate_clock_cycle", we do this:
if (next_time < current_time) {
   clock_slip++;
   next_time = current_time + cycle_duration_ticks;
} else {
   sleep (next_time - current_time);
   next_time = next_time + cycle_duration_ticks;
}

The very first time through we'll likely have a big clock slip, which is fine. But we should
generally not have them otherwise.
I am currently sleeping for cycle_duration_ticks without taking into account the time it takes
to do the work.

Well, it's a little better. I'm hovering pretty close to 1.024 / 2.8mhz.

next: speaker, or lo-res graphics?

## Speaker

Let's look at the audio. This will not be fun.

https://discourse.libsdl.org/t/sdl-mixer-accuracy/25160
https://bumbershootsoft.wordpress.com/2016/09/05/making-music-on-the-apple-ii/

Each tweak of $C030 causes the speaker to pop in, or the speaker to pop out.
If we do the in and out 880 times per second, we'll get an A tone (440 hz)- though not a pleasing round one.
How do we convert this to a WAV file type stream?
a WAV file with a 440hzsquare wave in it, has 96 samples of $2000, followed by 96 samples of $E000.
These are UNSIGNED 16-bit samples. $8000 is 0 (neutral). $e000 is plus $6000, $2000 is minus $6000, i.e. 75% volume.

Presumably it will take a certain amount of time for the speaker cone to travel.
And once we stop pinging the speaker, it stops moving.
Maybe we can simulate that.
We keep track of a direction.
then generate pre-defined samples for predetermined time period.
Use signed samples.
Each hit on the C030 generates either positive samples, or negative samples.
these are added to the current buffer. E.g. if there was a positive, then they hit the
speaker again really fast, it will cancel out the first hit.
hitting it twice in a row one right after the other will cancel out the first completely.
Test this. YES. That is corect. I get a quiet sound, just like the article was saying.

So, we can test different waveforms to add into here. 
How long those waveforms are will determine the size of the buffer.
This will only work at lower virtual cpu speeds. Or, we can slow down the clock for N cycles after a
speaker hit, hoping that stays inside the sound loop. (or does apple iigs software change the timing for this?)
For now, assume 1MHz clock speed.

So the Apple II speaker is: a 1-bit DAC, where every other access is the inverse sign of the previous one.

We assume: a ramp-up time; a hold time; a ramp-down time. The ramp-up and ramp-down are probably very similar.
I don't know the hold time, but, we can try different values. And to go from negative to positive there
will be overall double the ramp time.

4 + 4 + 2 + 216 * (2 + 2 + 2 + 2 + 2 + 2 + 2 + 3) + 3 + 2 
   216 * (17) = 3672

my test code is xd8 (216) iterations at about 20 cycles each iteration, or, 3672 cycles.
An up and down is therefore 7360 cycles. at 1MHz, that is 138 per second. That doesn't sound right.

In any given cycle, if the speaker is hit, add the sample to the buffer at the time index.
The time index for the audio buffer is calculated in 1023000 cycles per second.

The waveform timing won't change with the clock speed, because it's a physical property of the speaker.

https://www.youtube.com/watch?v=-Bjitqh7B0Y

## Dec 29, 2024

ok. SDL_OpenAudioDevice is the function we need for audio streams. When you call it, you set a callback
function. SDL calls it whenever it needs more audio data. So we just need to buffer data to always have some for it.
If we ever don't have enough we'll have to send back 0's (or fill with whatever the last sample was), and this will be
perceived as a skip/glitch in the audio.

Start with a short standalone program. I will return a short square wave every X ms.

Hmm, there is another method. SDL can also do a push method. We call "QueueAudio periodically to feed it
more data. That is the model I was looking for originally. I like the callback model. It is asynchronous -
is it even multithreaded?

ok sample program audio.cpp is generating some blips.
Each time I am being asked for more data, I am sending 4096 samples. 4096 is 10.766 per second. That is awkward.
100 per second would be 441 samples. Try that. 
Yes, that's 100 per second - frame is 10ms. 

What is a reasonable time period I can use for buffering?
We don't want it to be too long, or there will be latency.

the AI says:
#### Summary of the Process

1. Intercept $C030 reads in the emulator:
   1. Toggle an internal speakerState.
   1. Record the time of the toggle as an event.
1. Accumulate toggles over emulated cycles.
1. Resample to the host’s audio rate (e.g., 44.1 kHz) by stepping through time or through the toggle events.
1. Write each sample as a 16-bit signed integer (e.g., +32767 for “high,” –32768 for “low,” or some scaled variant).
1. Feed that buffer to the host OS audio API in blocks (e.g., every 1/60 second or 1/100 second).

This is an interesting approach. however, if we just slammed on $C030 nonstop it would be a lot of memory?
no, only 1/60th of a second's worth. That is maybe on the order of 4262 samples. So a reasonable amount of memory.
Keep the events in a circular buffer.
Each event is just recording the number of cycles.
Each buffer records the current cycle index too.
So then we play through the buffer until we reach the current cycle index.
Each event is laid into our output audio buffer at the appropriate time index.
at 50000 samples per second, each sample is 20us. To simulate a 'tick', we would paint 10 samples
into the output buffer.

For testing, I can dump a time-series log of the events into a file, from inside
the emulator. Then analyze that data in a separate program. "Play it back". Easier to test.
have F8 start and stop the audio recording.

That's working. I can dump the events to a file.
Recording what happens just on a boot - I get exactly 192 events. That is one of those "ah ha"
numbers. 
11871
12402
12933
...
113292

etc.
Each of these is exactly 531 cycles apart.
101421 cycles total duration.
So about 1/10th second.

531 cycles is half the frequency. So, 1062 cycles is a full oscillation.
I read that as 963Hz.

I might be off on my cycle counts a bit. Something to look into.

OK, now let's write the playback code.
Let's generate a 1/60th second buffer each chunk - that will make us have about 6 chunks.
that is 735 samples per chunk.

I think in my math I was slightly off. This might be why:

https://mirrors.apple2.org.za/ground.icaen.uiowa.edu/MiscInfo/Empson/videocycles

```
> Or is the difference too small to be noticeable (hearable? What about
> processor loops that access  $c030? Could you hear a "background-humming"?

If you were trying to generate a precise tone, you would need to allow
for this when determining the number of CPU cycles between accesses to
$C030, i.e. work on the assumption that the CPU is executing uniform
cycles at 1.0205 MHz instead of 1.0227 MHz.
```

Some great resources Apple IIers posted in response to this:

## Dec 30, 2024

OK some of my cycle counts are off. There are no single-cycle instructions, but my
current implementation has all the flag instructions at 1 cycle for instance. So
that's wrong. Also PLA and RTS are 2 cycles too fast.
The following instructions in the beep loop are the wrong cycle counts:
SEC, PLA, RTS

After fixing these, I have 546 cycles between hits on $C030 instead of 531.
That is 2.82% slower through the loop, which should bring the tone down a hair.
However, now my beep is at 926Hz according to the spectrum analyzer.

The SA says the real apple II beep peak is at maybe 937Hz. 915-958 for the whole hump.
Reproduced is 872-969 with peak at 926Hz. Ah, so I need to bring that bottom end up a bit.
That difference is 1.17%, 1.2%.

In the recreation code I have one sample as 2.3us. That's wrong. They're 22.6us.
But I think there is an assumption that 1us = 1 cycle which is not quite right. It's 1% slow. And that's about how far
off we are, 1% low.

Grabbed the "Understanding the Apple II" books. I have some assumptions about clocking that are
not quite right. And would be incredibly hard to replicate. And are probably not the reason for
the differences.

### Audio update


I need to add the concept of an audio buffer.
Each 'event' I need to paint a BLIP worth of samples into the output audio stream. Currently this is 434 samples but it could be an arbitrary number. And, I need to ensure that ALL the samples that are part of a blip, get put into the output, even if they get painted beyond the current audio frame being requested via the sdl2 audio callback.

So, I need an intermediate buffer.

Weird, my speaker_event_log file had the wrong data in it?? Re-ran GS2 with a test of the 
beep routine, and now it's 546 cycles between events as I noted earlier. Weird.

now my "audio working" is too low. Way too low. I went the wrong way.

## Dec 31, 2024

So the audio callback cycle counter is running quite a bit faster than the cpu.
```
4568199 delta 24826466 cycles clock-mode: 2 CPS: 0.913640 MHz [ slips: 8739, busy: 24817727, sleep: 0]
audio_callback 35325884 35342835 0
```

I'm getting 300 audio frames per 5 seconds, that's correct, because I set it up at 60 frames per second.
in that same interval 5,166,383 cycles. 1033276.6 per second average.
But the audio clock ticked up from  25629912 to 30715212 or delta of 5085300.
That's a slight difference but does not account for the discrepancy of 10M cycles.

We want the audio cycle counter to run just behind the cpu cycle counter, such that the 
current cpu cycle count is always greater than the audio cycle count.
So if the audio cycle window count is > cpu_cycle_count, set

buf_start_cycle = cpu_cycle_count - cycles_per_buffer
buf_end_cycle = buf_start_cycle + cycles_per_buffer

see what that does.
We could store time markers as events in the event buffer, and have audio_callback continuously sync to that..

```
    if (buf_end_cycle > speaker_states[0].cpu->cycles) {
        std::cout << "audio cycle overrun resync from " << buf_end_cycle << " to " << speaker_states[0].cpu->cycles - CYCLES_PER_BUFFER << "\n";
        buf_start_cycle = speaker_states[0].cpu->cycles - CYCLES_PER_BUFFER;
        buf_end_cycle = buf_start_cycle + CYCLES_PER_BUFFER;
    } else if (buf_end_cycle < speaker_states[0].cpu->cycles - CYCLES_PER_BUFFER) {
        std::cout << "audio cycle underrunresync from " << buf_end_cycle << " to " << speaker_states[0].cpu->cycles - CYCLES_PER_BUFFER << "\n";
        buf_start_cycle = speaker_states[0].cpu->cycles - CYCLES_PER_BUFFER;
        buf_end_cycle = buf_start_cycle + CYCLES_PER_BUFFER;
    } else {
```

OK, that is keeping it synced, however, I'm getting a lot of overruns and underruns and audio artifacts.
But, the window is staying aligned to the CPU cycle count and I am not getting into situations where
the audio is lagging.

My alternative here is to push audio stream, instead of having SDL2 pull it.

I made a loader function - passing -a filename will load a file into RAM at 0x0801, assuming applesoft basic.
That let me load Lemonade Stand. he he. Lots of sound in there, really shows the issue with audio sync.

I have added some extra 'guardrails' around the buffering, and I improved results and reduced underrun and overrun. But I'm still not entirely happy with the results. I'm going to have to just grind out some hard thinking on it.

And, it definitely only works in 1MHz. Actually, let's see what insanity ensues at free-run..
(I'm wondering if I can try syncing not to hard-set 1.0205 but to whatever the specified clock speed is)
Ah, at 2.8 it's sort of legible but super fast. And at free run the entire segment plays a short tone and then
skips to the next screen in under a second, lol.

https://github.com/kurtjd/rust-apple2/blob/main/src/sound.rs

This is an apple 2 emulator written in Rust, and this is the sound code.

** ok, well now. I took out the display optimization. effective cycles per second is 1.01xxxmhz. I think the audio is working better.
[x] So it's due to the cpu clock running slightly faster than the audio timing.


## Lo-Res Graphics

Let's do a sprint to get LORES:
https://www.mrob.com/pub/xapple2/colors.html
https://sites.google.com/site/drjohnbmatthews/apple2/lores

```
                 --chroma--
 Color name      phase ampl luma   -R- -G- -B-
 black    COLOR=0    0   0    0      0   0   0  0x00000000
 red      COLOR=1   90  60   25    227  30  96 
 dk blue  COLOR=2    0  60   25     96  78 189
 purple   COLOR=3   45 100   50    255  68 253
 dk green COLOR=4  270  60   25      0 163  96
 gray     COLOR=5    0   0   50    156 156 156
 med blue COLOR=6  315 100   50     20 207 253
 lt blue  COLOR=7    0  60   75    208 195 255
 brown    COLOR=8  180  60   25     96 114   3
 orange   COLOR=9  135 100   50    255 106  60
 grey     COLOR=10   0   0   50    156 156 156
 pink     COLOR=11  90  60   75    255 160 208
 lt green COLOR=12 225 100   50     20 245  60
 yellow   COLOR=13 180  60   75    208 221 141
 aqua     COLOR=14 270  60   75    114 255 208
 white    COLOR=15   0   0  100    255 255 255
 
 black    HCOLOR=0   0   0    0      0   0   0
 green    HCOLOR=1 225 100   50     20 245  60
 purple   HCOLOR=2  45 100   50    255  68 253
 white    HCOLOR=3   0   0  100    255 255 255
 black    HCOLOR=4   0   0    0      0   0   0
 orange   HCOLOR=5 135 100   50    255 106  60
 blue     HCOLOR=6 315 100   50     20 207 253
 white    HCOLOR=7   0   0  100    255 255 255
```
 Black [0], Magenta [1], Dark Blue [2], Purple [3], Dark Green [4], Dark Gray [5], Medium Blue [6], Light Blue [7], Brown [8], Orange [9], Gray [10], Pink [11], Green [12], Yellow [13], Aqua [14] and White [15]

```
 0 - 0x00000000
 1 - 0x8A2140FF
 2 - 0x3C22A5FF
 3 - 0xC847E4FF
 4 - 0x07653EFF
 5 - 0x7B7E80FF
 6 - 0x308EF3FF
 7 - 0xB9A9FDFF
 8 - 0x3B5107FF
 9 - 0xC77028FF
 10 - 0x7B7E80FF
 11 - 0xF39AC2FF
 12 - 0x2FB81FFF
 13 - 0xB9D060FF
 14 - 0x6EE1C0FF
 15 - 0xFFFFFFFF
```
OK that was largely working. 

[x] Now, however, I have a bug where when I type 'GR' it only partially clears the screen. There must be a race condition somewhere. (Fixed. in text_40x24.cpp:txt_memory_write I was not setting dirty flag on line when in lores mode.) (This has a hack in in regarding hi-res mode).



## Cassette Interface

On the one hand this seems silly. On the other hand, it is vintage cool, and there are actually web sites that distribute audio streams of old games stored in audio files, that you can play into an Apple II.

https://retrocomputing.stackexchange.com/questions/143/what-format-is-used-for-apple-ii-cassette-tapes

The Apple II recorded data as a frequency-modulated sine wave. A standard consumer cassette deck could be connected to the dedicated cassette port on the Apple ][, ][+, and //e. The //c, ///, and IIgs did not have this port.

A tape could hold one or more chunks of data, each of which had the following structure:

Entry tone: 10.6 seconds of 770Hz (8192 cycles at 1300 usec/cycle). This let the human operator know that the start of data had been found.
Tape-in edge: 1/2 cycle at 400 usec/cycle, followed by 1/2 cycle at 500 usec/cycle. This "short zero" indicated the transition between header and data.
Data: one cycle per bit, using 500 usec/cycle for 0 and 1000 usec/cycle for 1.
There is no "end of data" indication, so it's up to the reader to specify the length of data. The last byte of data is followed by an XOR checksum, initialized to $FF, that can be used to check for success.

For machine-language programs, the length is specified on the monitor command line, e.g. 800.1FFFR would read $1800 (6144) bytes. For BASIC programs and data, the length is included in an initial header section:

 * Integer BASIC programs have a two-byte (little-endian) length.
 * Applesoft BASIC has a two-byte length, followed by a "run" flag byte.
 * Applesoft shape tables (loaded with SHLOAD) have a two-byte length.
 * Applesoft arrays (loaded with RECALL) have a three-byte header.
Note the header section is a full data area, complete with 10.6-second lead-in.

The storage density varies from 2000bps for a file full of 0 bits to 1000bps for a file full of 1 bits. Assuming an equal distribution of bits, you can expect to transfer about 187 bytes/second (ignoring the header).

An annotated 6502 assembly listing, as well as C++ code for deciphering cassette data in WAV files, can be found here. The code in the system monitor that reads and writes data is less then 200 bytes long.


## Pascal Firmware Protocol

The Pascal protocol seems to be geared towards I/O, like serial devices, and displays.
However, there are "device class" assignments for :
Printer, Joystick, Serial/parallel, modem, sound/speech device, clock, mass-storage, 80 column card, network or bus interface, special purpose, and reserved.

Unclear how widely-supported this is.

## Generic Disk Interface - proposed

### Registers

Propose:

```
C0S0 - lo byte of sector number
C0S1 - hi byte of sector number
C0S2 - target address lo byte
C0S3 - target address hi byte
C0S4 - sector count
C0S5 - command. 0x1A = read, 2C = write.
```

Someone could easily accidentally write garbage to the disk by strafing these registers. Give some thought
on how to prevent that... Have the commands be values unlikely to be written by accident.
Require a specific double-strobe. Something like that.
Ah. Write values in. Strobe. Write command. Strobe.
if not done in this sequence, command is ignored.
Then the data is DMA'd into memory.

### Firmware
$CS00 - boot.
$CS??


## Disk II Interface

First, ROMs:

https://mirrors.apple2.org.za/ftp.apple.asimov.net/emulators/rom_images/

   * $C0S0 - Phase 0 Off
   * $C0S1 - Phase 0 On
   * $C0S2 - Phase 1 Off
   * $C0S3 - Phase 1 On
   * $C0S4 - Phase 2 Off
   * $C0S5 - Phase 2 On
   * $C0S6 - Phase 3 Off
   * $C0S7 - Phase 3 On
   * $C0S8 - Turn Motor Off
   * $C0S9 - Turn Motor On
   * $C0SA - Select Drive 1
   * $C0SB - Select Drive 2
   * $C0SC - Q6L
   * $C0SD - Q6H
   * $C0SE - Q7L
   * $C0SF - Q7H

So, the idea I have for this:
each Disk II track is 4kbyte. I can build a complex state machine to run when
$C0xx is referenced. Or, when loading a disk image, I can convert the disk image
into a pre-encoded format that is stored in RAM. Then we just play this back very
simply, in a circle. The $C0xx handler is a simple state machine, keeping track
of these values:
   * Current track position. track number, and phase.
   * Current read/write pointer into the track
   

Each pre-encoded track will be a fixed number of bytes.

If we write to a track, we need to know which sector, so we can update the image
file on the real disk.

Done this way, the disk ought to be emulatable at any emulated MHz speed. Our 
pretend disk spins faster along with the cpu clock! Ha ha.

### Track Encoding

At least 5 all-1's bytes in a row.

Followed by :

```
Mark Bytes for Address Field: D5 AA 96
Mark Bytes for Data Field: D5 AA AD
```

### Head Movement

to step in a track from an even numbered track (e.g. track 0):
```
LDA C0S3        # turn on phase 1
(wait 11.5ms)
LDA C0S5        # turn on phase 2
(wait 0.1ms)
LDA C0S4        # turn off phase 1
(wait 36.6 msec)
LDA C0S6        # turn off phase 2
```

Moving phases 0,1,2,3,0,1,2,3 etc moves the head inward towards center.
Going 3,2,1,0,3,2,1,0 etc moves the head inward.
Even tracks are positioned under phase 0,
Odd tracks are positioned under phase 2.

If track is 0, and we get:
Ph 1 on, Ph 2 on, Ph 1 off
Then we move in one track to track. 1.

So we'll want to debug with printing the track number and phase.

When software is syncing, it's just going to look for 5 FF bytes in a row
from the read register, followed by the marks. That's basically it.
For handling writing, we might want to have each sector in its own block and up
to a certain 

We'll be able to handle both 6-and-2 and 5-and-3 scheme in case anyone wants this -
it would be registering a different disk ii handler that would set a flag.
In theory this scheme could let you nibble-copy disk images too.

The media is traveling faster under the head when the head is at the edge;
slower when under the center. The time for a track to pass under the head is
the same regardless of which track the head is over. In the physical world,
this means the magnetic pulses are physically longer when the head is at the edge.
In our emulated world, it should be roughly the same number of bits per ms
regardless of position. So each track should be the same number of pre-encoded bytes.

If the motor is off, we stop rotating bits out through the registers.

OK, we have a 6:2 encoding table.

# Jan 1, 2025

## Apple IIe Dinking

I implemented some infrastructure to automatically fetch and combine ROMs from the Internet
for use by the emulator.

The Apple IIe ROMs aren't working, because we don't have correct memory mapping in place
for the ROMs.

Apple IIe Technical Reference Manual, Pages 142-143, cover this, but it's dense and the diagram
doesn't exactly match the text.

Booting up, we see this (we didn't get very far, lol):
```
 | PC: $FE84, A: $00, X: $00, Y: $00, P: $00, S: $A5 || FE84: LDY #$FF
 | PC: $FE86, A: $00, X: $00, Y: $FF, P: $80, S: $A5 || FE86: STY $32   [#FF] -> $32
 | PC: $FE88, A: $00, X: $00, Y: $FF, P: $80, S: $A5 || FE88: RTS [#FA65] <- S[0x01 A6]$FA66
 | PC: $FA66, A: $00, X: $00, Y: $FF, P: $80, S: $A7 || FA66: JSR $FB2F [#FA68] -> S[0x01 A6]$FB2F
 | PC: $FB2F, A: $00, X: $00, Y: $FF, P: $80, S: $A5 || FB2F: LDA #$00
 | PC: $FB31, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB31: STA $48   [#00] -> $48
 | PC: $FB33, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB33: LDA $C056   [#00] <- $C056
 | PC: $FB36, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB36: LDA $C054   [#00] <- $C054
 | PC: $FB39, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB39: LDA $C051   [#00] <- $C051
 | PC: $FB3C, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB3C: LDA #$00
 | PC: $FB3E, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB3E: BEQ #$0B => $FB4B (taken)
 | PC: $FB4B, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB4B: STA $22   [#00] -> $22
 | PC: $FB4D, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB4D: LDA #$00
 | PC: $FB4F, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB4F: STA $20   [#00] -> $20
 | PC: $FB51, A: $00, X: $00, Y: $FF, P: $02, S: $A5 || FB51: LDY #$08
 | PC: $FB53, A: $00, X: $00, Y: $08, P: $00, S: $A5 || FB53: BNE #$5F => $FBB4 (taken)
 | PC: $FBB4, A: $00, X: $00, Y: $08, P: $00, S: $A5 || FBB4: PHP [#30] -> S[0x01 A5]
 | PC: $FBB5, A: $00, X: $00, Y: $08, P: $00, S: $A4 || FBB5: SEI
 | PC: $FBB6, A: $00, X: $00, Y: $08, P: $04, S: $A4 || FBB6: BIT $C015   [#EE] <- $C015
 | PC: $FBB9, A: $00, X: $00, Y: $08, P: $C6, S: $A4 || FBB9: PHP [#F6] -> S[0x01 A4]
 | PC: $FBBA, A: $00, X: $00, Y: $08, P: $C6, S: $A3 || FBBA: STA $C007   [#00] <- $C007
 | PC: $FBBD, A: $00, X: $00, Y: $08, P: $C6, S: $A3 || FBBD: JMP $C100
 | PC: $C100, A: $00, X: $00, Y: $08, P: $C6, S: $A3 || C100: INC $EEEE $EEEE   [#E4]
```
 So, it's trying to enable "Internal ROM at $CX00" when it hits $C007:

 "When SLOTCXROM is active (high), the IO memory space from $C100 to $C7FF is allocated
 to the expansion slots, as described previously. Setting SLOTCXROM inactive (low) disables
 the peripheral-card ROM and selects built-in ROM in all of the I/O memory space
 except the part from $C000 to $C0FF (used for soft switches and data I/O).
 In addition to the 80 col firmware at $C300 and $C800, the built-in ROM
 includes firmware that performs the self-test of the Apple IIe's hardware.

So it's trying to enable the internal ROM. Which I have loaded, but I am not exposing that
part of the memory page.

This brings up the question of whether page sizes should be 4K as I was originally thinking,
or 256 bytes. There's a bunch of stuff here that gets paged in on 256 byte boundaries.
I guess I could add some logic to bus.c to try to handle this.

Should review all the 80-column stuff too. Might as well jump in and document all the "extra
memory bank" related stuff, and go straight from where we're at with 64K, to 128K.


## Apple II (non-plus) dinking

Going the other direction, loading Apple II (original) f/w on the emulator works.
Dumps you into monitor ROM, and, ctrl-B into Integer BASIC!

F666G puts you into the mini-assembler, which is especially fun.

This is an Integer basic game :

https://en.wikipedia.org/wiki/Integer_BASIC

that would be a pain in the ass to type in, so, was thinking about a copy/paste feature
to paste data into the emulator.

Since I've started this Platform work, there are some things to think about:

1. Even to support full Apple IIe functionality, we're going to need the ability to handle more than
64K of memory. This is done with switches, and the 'bus' is still only 16 bit.
1. The Platform concept should include selection of the CPU chip, and perhaps certain hardware. Though
we probably want to have some separation so we can have "Any card in any platform in any slot".



# January 2, 2025

I have an interesting concept, of extensive use of function pointers in structs / arrays.
For example, when defining the Apple II Plus platform, we would have a struct that
is a list of all the devices that need to be initialized. They would be executed in order.
A different platform could have a different set.
All the devices would interface specifically through the Bus concept - just like they do in the
real systems, and, must not have any cross-dependencies. And then all the devices
code could be 100% shared between different platform definitions.
A device class would be composed of:
  initialization method
  de-initialization method

They tie into the system through:
  bus_register
  bus_unregister
  timer_register
  timer_unregister

timer_register is something that's hardcoded in the gs2 main loop right now.
But we could maintain a ordered event queue of function pointers to call.
Let's say we've got video routines 60 times a second; and another thing that needs to
happen 10 times a second, etc. Each time a handler ran, it would reschedule itself
for a next run, as appropriate.

To integrate into SDL events, build a data structure that would have callbacks based on certain event types.

This is a goal, before we get too awful many devices into this thing.

At a little higher level, we'll have a slot device class. These will be called with a slot number.
And then register themselves into the system as appropriate.

## January 3, 2025

Thinking about display a little bit.
We're going to need to have the native display be 560x192 pixels. This is for a few reasons.
1) handle 80 column text. that's 7 pixels x 80 = 560.
2) handle double hi-res. This is 560x192.
3) even with standard hi-res, if we want to simulate the slight horizontal displacement of different color pixels, then
we need to have 560 pixels horizontally. This actually sort of simplifies HGR display.

The implication is that either I change the texture scale horizontally by a factor of 2
(e.g., in 40-column modes it would be 8x 4y scaling), or, I can just write twice
as many pixels horizontally to the texture.

Yes. We would still define the texture as 560x192.
In 40-col text and regular lo-res mode, we would only write pixels into 280x192, and SDL_RenderCopy 
double (see below) to stretch it.
In 80-col mode, double lo-res mode, and hi-res mode, and double hi-res mode, we would write pixels into 560x192
and copy w/o stretching.

This implies we need to keep the screen mode per line. i.e. each of the 24 lines would be:
   lores40
   lores80
   text40
   text80
   hires
   dhires

The display overall would also have a color vs monochrome mode.
And we need to keep track of the "color-killer" mode: in mixed text and graphics mode, the text is color-fringed. In pure text mode, the display is set to only display white pixels. Actually this implies that even on lines in text mode, we might need to draw the text with the slight pixel-shift and color-fringed effect. In which case, all buffers would always be 560 wide. And, we would likely post-process the color-fringe effect. I.e., draw a scanline (or, 8 scanlines), then apply filtering to color the bits. Then render.

In monochrome mode, a pixel is just on or off. no color. But, probably still pixel-shifted.

Depending on how intricate this all gets, we may even need to use a large pixel resolution than 560x192. Would have to read up on the Apple II NTSC stuff in more detail.

```
// Source rectangle (original dimensions)
    SDL_Rect srcRect = {
        0,      // X position
        y * 8,  // Y position
        280,    // Original width
        8       // Height
    };

    // Destination rectangle (stretched dimensions)
    SDL_Rect dstRect = {
        0,      // X position
        y * 8,  // Y position
        560,    // Doubled width (or whatever scale you want)
        8       // Height
    };

    SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
```

Example hires bytes:

  01010101 - 55. Purple dots. even byte (0,2,etc)
  00101010 - 2A. Purple dots. odd byte. (1,3,etc)

  11010101 - D5. Blue dots. even byte (0,2,etc)
  10101010 - AA. Blue dots. odd byte. (1,3,etc)

Apple2TS does not do color fringing on text. 

A very thorough hires renderer:

https://github.com/markadev/AppleII-VGA/blob/main/pico/render_hires.c?fbclid=IwY2xjawHlXzFleHRuA2FlbQIxMAABHY4HQtJ1e_ODqLnjQ3SEOp4Z_js9qJa7JArXYQJj-KmLULgEYBUDO24zUQ_aem_3K8UGHDmTyjZzMBticL4uA#L6

This is a decoder for how a bunch of things are supposed to look:
https://zellyn.com/apple2shader/?fbclid=IwY2xjawHlYI1leHRuA2FlbQIxMAABHWRHc9TVWKrSuP0JBIeQHS4OVXc7_nCmcu2hdOl6c-vtCk2Aiit1uXNFEA_aem_5LQvZKXfjFgJFIU0zM5WnQ

## Jan 4, 2025

The DiskII code is now correctly generating a nibblized disk image in memory.
I have tested this two ways, first, by comparing encoded sectors to the example
in decodeSector.py. Second, by taking an entire disk and inspecting it with 
CiderPress2. This has the ability to read nibblized disk images and pull data
out of them, catalog them, etc. Slick.

Getting ready to write the read register support, then, we ought to be able to
boot a bloody DOS 3.3 image!

In preparation for that, I took the code I was working on which was a separate
set of source files and executable, and moved it into a new folder structure,
src/devices/diskii. There is also apps/nibblizer. There's a hierarchy of Makes
and Cmake apparently makes it very easy to turn some of these into libraries
and link them into the main executable. This means the diskii_fmt files are now
shared between the main executable and the nibblizer executable. Slick.

I also have had some thoughts about how to do runtime configuration and linking of
a user interface with the engine. I was thinking, some kind of IPC mechanism.
If we use web tech like JSON to an API, that could open up some very interesting
possibilies. It would allow remote control over the internet; it would allow 
programmatic / scripted runtime configuration of the system, which could really
help automate testing. This could work kind of like the bittorrent client,
where the configurator is just a web app.

[ ] DiskII should get volume byte from DOS33 VTOC and 'format' the disk image with it.

[x] Create struct configuration tables for memory map for each platform.

[x] There's an obvious optimization in the DiskII area - we don't have to shift the bits out of the register. We can just provide the full byte with the hi-bit set. This would 
probably work and speed up disk emulation mightily. (ended up pre-shifting 6 bits out)

## Jan 5, 2025

Well as of this morning I'm booting disk images!

However, they are exceptionally slow operating. I think it must have to do with the interleaving.
I am laying them out in memory in logical disk order, but with the sector numbers mapped
to interleave. I think that's insufficient.

Check out my CiderPress2 generated .nib image and see what order the sectors are in.

yeah, physical sectors numbers in the CP2 image are: 0, 1, etc. So not only do they change the sector numbers
in the address field, but, we need to reorder the data so the physical sectors in the stream are in the physical sector order:
i.e., 0, 1, 2, 3, etc.

original way

gs2 -d testdev/disk2/disk_sample.do  50.24s user 0.40s system 98% cpu 51.397 total

new way (physical order)

gs2 -d testdev/disk2/disk_sample.do  47.40s user 0.35s system 99% cpu 48.173 total

That's not much difference - 3 seconds.

Here's some more potential stuff:

From Understanding the Apple II. page 9-37:
"Is is possible to check whether a drive at a slot is on by configuring for reading
data and monitoring the data register. If a drive is turned on, the data register will
be changing and vice-versa."

So when the disk is off, it is turning it on and waiting a second for it to spin up.

Understanding the Apple II. page 9-13: "The effect of this timer is to
delay drive turn-off until one second after a reference to $C088,X".

So I need to emulate the timer. Each iteration through the DiskII code, check
cpu cycles from "mark_cycles_turnoff". if it's non-zero, then see if it's
been one second. Only then mark the motor disabled. On a reset the delay timer
is cleared and drive turned off almost immediately."

So, simulate this timer. Because I think the II is waiting one second every time it
turns the disk motor on.

We will need a "list of routines to call on a reset" queue. Disk:
"RESET forces all disk switches to off, and clears the delay timer."

There is also this: "Access to even addresses causes the data register contents to be
transferred to thedata bus."

my attempt to delay motor off is not working, its causing the disk to chugga chugga.
Don't forget I'm starting on track 13 every time to be silly. Would help a little
to have it start at track 0.

None of this has really helped. here's what I'm thinking. say I'm in an apple ii.
I read a sector. I have to go off and do stuff with it.
by the time I come back to read the next sector, the disk has spun around a circle
for a while. But my emulator hasn't moved the virtual head. So all this interleave 
stuff is just wrong. 

I need to simulate the disk continuing to spin while we're away. So, between reads
from the disk, I need to spin the disk that many nybbles.

Never mind. I disabled my "accelerator" and now it boots fast. That code was causing a big
mismatch between the simulated disk and cycles going by - it was messing with the interleave
timing.

Now we're at 11 seconds!

This may yet be an issue when writing. Have to take a look at the write code in DOS.
And, of course, we'll need a denibblizer routine. I bet I can steal that from Woz too!

A ton of software requires 64K. So I guess that's the next thing to do.

I am thinking, a more generalized event queue is needed. For instance, the disk needs to turn
off one second after the command to turn it off is actually sent. Right now I am cheating
and checking cpu cycles but only when the disk registers are read. Software is going to want
to turn it off and then NOT READ REGISTERS for a while.

Of course I also have timing routines related to video and audio. So that's where the event queue
comes in. It needs to be -ordered-, when we read out of it.

It's going to be the following:
cycle count to trigger at
function pointer to call
argument 1 uint64_t
argument 2 uint64_t

This will be part of the CPU struct, so the actual function can be called with the CPU pointer.

[x] Add 'Language card' / 64K support.
[x] Add event queue system.
[x] Add Disk II write support.
[x] Add .nib and .po format support.
[x] Note infinite loop on the console, but do not terminate.


### Language Card:

4K RAM Bank 1: D000 - DFFF
4K RAM Bank 2: D000 - DFFF

10K RAM: E000 - F7FF
2K RAM: F800 - FFFF
Language ROM: F800 - FFFF

So thinking about 256 byte memory map. In the IIe like the stack page can be swapped
between main and aux memory.

So, 64K memory map is 256 pages of 256 bytes. Create memory table with 256 byte pages.

For GS, we could have a multi-level memory map that lets us have two different page sizes. An extension to do
later.

Separately allocate the actual memory areas and assign them to variables that exist independently of the memory map.
Because we will need to swap these in and out.

So for instance, for the II+ we'll do:
  * Main_RAM = alloc(48K)
  * Main_IO = alloc(4k)
  * Main_ROM = alloc(12K)

The pages do not have to be allocated separately. Contigious is fine.

Then assign subsets of these to the memory map in 256 byte increments. 

For language card, we'll have:
  * Bank_RAM_4K_1 = alloc(4K)
  * Bank_RAM_4K_2 = alloc(4K)
  * Bank_RAM_6K = alloc(6K)
  * Bank_RAM_2K = alloc(2K)
  * Bank_ROM = alloc(2K)

And then memory map altered when softswitches are toggled appropriately.
We will want some external functions to mediate this, in bus.cpp.
It seems straightforward how a card would swap its pages in. How would we swap back? Does it save the old entries? hm, yes, I think it does. On init_card, it will save the memory map entries that were present at init time. 
  * OR, 3rd: store the main system memory blocks into well-known variable names.

ok I'm gonna cut for the night - I have the new memory map implemented, and have gs2 booting again. I think the language card hooks are in, and now need to start testing that.

I ended up allocating all in one big 64K chunk. That's more like the real thing and is less to keep track of.

## Jan 6, 2025

Some of the memory management might be a lot easier if I think about it this way:
instead of allocating all these different chunks, do:
  * 1 64KByte chunk for RAM.
  * 1 12KB chunk for ROM.

Then I map the effective address space into the appropriate bits of these chunks.
E.g.:

1st 48K is mapped straight.
```
Then D000 bank 1 => RAM[0xC000]
D000 bank 2 => RAM[0xD000]
```

Also this is more likely closer to how the apple IIe does it - there aren't all
these separate RAM chips. they just had a 64K bank of RAM. This is because the same
patterns apply to aux memory (the additional 64K chunk there.) Then we can just have
aux memory be treated the same way.

OK, running the Apple II Diagnostic disk. I have C083 * 2 'good', C083 'bad'. I think this
is because the manual says we are supposed to read C083 TWICE to switch into RAM/RAM read/write
mode. What constitutes reading twice in a row? This is almost certainly some protection against
accidental reads of C083.

The first time C083 is hit, it's the same as hitting C080 - ram read, no writing. Hitting C083 a second time enables writing to RAM.

ok, it passes the diagnostic test now!!!

Trying to boot ProDOS - I got the ProDOS boot screen and blip!
but infinite loop doing this:
```
Thunderclock Plus read register C090 => 00
Thunderclock Plus read register C090 => 00
Thunderclock Plus read register C090 => 00
Thunderclock Plus read register C090 => 00
```
turned that off..
now fails here:
```
schedule motor off at 15126338 (is now 14376338)
languagecard_read_C08B - LANG_RAM_BANK1 | RAM_RAM
bank_number: 1, reads: 1, writes: 0
page: D0 read: 0x148014000 write: 0x14700ae00 canwrite: 0
...
languagecard_read_C08B - LANG_RAM_BANK1 | RAM_RAM
bank_number: 1, reads: 1, writes: 1
page: D0 read: 0x148014000 write: 0x148014000 canwrite: 1
...
Unknown opcode: FD6F: 0x82CPU halted: 1
```

it's running from ram.

So it's loaded code into page D6 and then hit invalid opcode 0x82.
When we hit the system halt, I should be able to dump memory and various things.



Hm, this could be a disk order thing.. maybe I can check bytes in the image to detect if it's DOS 3.3 or ProDOS and implement the correct interleave? doesn't seem likely..

In the debug prints, display the buffers in more informative way. Instead of raw pointer, say "main_ram(DE)" to specify main_ram page DE. Make a little subroutine for it.

Carmen Sandiego got a little bit further. Wants me to put in disk 2. now what?! hehe.

I don't think hires page 2 is working.. Hm, no, it is.

Something is still not quite working with the memory map stuff.

OK, Thunderclock utils disk downloaded. These things want "thunderclock plus firmware" to be present to work. 

I should also be able to freeze execution any other time and do the same.
And disassemble code. So, I need a system monitor app.
I could do a running disassembly, i.e. have the last 10 or 20 instructions in a circular buffer,
dumped also.


So, various things are trying to load Integer basic, but then failing to do so. There is some sort of test that's being done that fails, for if there is a language card present. The first thing I note, is that this hits:

C081, then C081 again 4 cycles later. and Apple IIe manual has "RR" next to C081. I think all the modes that enable RAM writes, require the double-read in order to turn on RAM writes. Let's go ahead and try that.

Apparently Locksmith 6.0 has a "really good language card tester".

Apple2ts , when it hits C08B once, switches immediately to R/W RAM Bank 1 for D0 - FF. That is not what the manual says. Maybe the manual is wrong.
also, when it hits C081, I can wait a very long time before hitting again and then it switches to read rom write ram.
and then C08B switches immediately to RW RAM Bank 1. wut.
c081 c08b goes to rw ram bank 1.
c089 by itself does nothing. again does read rom write ram.
So the 2nd read doesn't have to be the same switch. It can be any of the other double switches.
and C08b and c083 go immediately to read ram.
(They also trigger c100-c7ff / and c800-cfff 'peripheral' instead of 'internal ROM', which must be an Apple IIe thing.)


Notes from apple2ts

```
  // All addresses from $C000-C00F will read the keyboard and keystrobe
  // R/W to $C010 or any write to $C011-$C01F will clear the keyboard strobe

  if (addr >= 0xC080 && addr <= 0xC08F) {
    // $C084...87 --> $C080...83, $C08C...8F --> $C088...8B
    handleBankedRAM(addr & ~4, calledFromMemSet)
    return
```

There appears to be an extensive discussion of this stuff in Understanding the Apple II.
Chapter 5, page 5-26, The 16K RAM CARD

ok, going by this, my implementation is wrong, and misses the effect writes can have. This
was not documented by Apple. Bad Apple.

Sather discusses a number of flip-flops (state bits):
Bank_1 flip flop. 0 = bank 2, 1 = bank 1.
Read_Enable flip flop. 1 = enable read from expansion RAM. 0 = enable read from ROM.
Write_Enable' flip flop. 0 = enable write to expansion RAM. 1 = no write.
Pre_Write flip flop.

the write flip flops can be thought of as a write counter which counts odd read accesses
in the C08X range. The counter is set to zero by even or write access in the C08X range.
If the write counter reaches the count of 2, writing to expansion RAM is enabled.
Writing will stay enabled until an even access is made in the C08X range.

```
at power on:
Bank_1 = 0
Pre_Write = 0
Read_Enable = 0
Write_Enable' = 0 ()
 - says Reset has no effect. I bet they changed this.

A3
= 0, C080-C087 - any access resets Bank_1. 
= 1, C088-C08F - any access sets Bank_1.

A0/A1
==00, 11 : C080, C083, C084, C087, C088, C08B, C08C, C08F - set READ_ENABLE.
==01, 10 : C081, C082, C085, C086, C089, C08A, C08D, C08E - reset READ_ENABLE.

PRE_Write = 1
then can reset WRITE_ENABLE'
```

Pre_Write is set by Odd Read Access in C08X range.
Pre_Write is reset of Even access in range, OR a write access anywhere in C08X range.

WRITE_ENABLE' is reset by an odd read access in C08X when PRE_Write is 1.
WRITE_ENABLE' is set by an even access in C08X range.

So, Bank and Read enable are set on both reads and writes. That was not at all clear.

**I can now successfully boot ProDOS 1.1.1. I can load integer basic on the DOS3.3 master disk.**

## Jan 7, 2025

The JACE (java) emulator has a working Thunderclock implementation, including the thunderclock ROM file.
That's probably what I need to check to fix my implementation.

Looking at generic ProDOS clock implementation.

ProDOS recognizes a clock card if:
```
Cn00 = $08
Cn02 = $28
Cn04 = $58
Cn06 = $70
Cn08 - READ entry point
Cn0B - WRITE entry point
```

The ProDOS routine stores date and time in:
```
$BF91 - $BF90
day: bits 4-0
month: bits 8-5
year: bits 15-9
$BF93 - hour
$BF92 - minute
```

The ProDOS clock driver expects the clock card to send an ASCII
string to the GETLN input buffer ($200). This string must have the
following format (including the commas):
  * mo,da,dt,hr,mn
  * mo is the month (01 = January...12 = December)
  * da is the day of the week (00 = Sunday...06 = Saturday)
  * dt is the date (00 through 31)
  * hr is the hour (00 through 23)
  * mn is the minute (00 through 59)

It doesn't say but presumably the $200 getln ends with a carriage return.

Well this seems very simple. The only question is, how do we trigger a call
into a native routine?
CPU is going to do this:
JSR $Cn08
Cn08: XX YY ZZ
What values can we put there we can capture?
Options.
1) write our own ASM firmware that runs in Cn00. This will read registers.
2) some other option I forgot.

Ah ha! 6.3 of ProDOS-8-Tech-Ref is Disk Driver Routines. Same conversation regarding
"3rd party disk drives".

```
$Cn01 = $20
$Cn03 = $00
$Cn05 = $03
```

if $CnFF = $00, ProDOS assumes a 16-sector Disk II card.
If $CnFF = $FF, ProDOS assumes a 13-sector Disk II card.
If $CnFF <> $00 or $FF, assumes has found an intelligent disk controller.
If Status byte at $CnFE indicates it supports READ and STATUS requests,
ProDOS marks the global page with a device driver whose high-byte is
$Cn and low-byte is $CnFF.
E.g., in Slot 5, that would be $C5<value of $CnFF>. Let's say $CnFF was
23. Then ProDOS would record a device driver address at $C523.

This is the boot code.

So the start of the device driver is:
```
C700    LDX #$20
C702    LDA #$00
C704    LDX #$03
C706    LDA #$00
C708    BIT $CFFF - turn off other device ROM in c8-cf
C70B    LDA #$01
C70D    STA $42
C70F    LDA #$4C
C711    STA $07FD
C714    LDA #$C0
C716    STA $07FE
C719    LDA #$60
C71B    STA $07FF
C71E    JSR $07FF     ; call an RTS to get our page number
C721    TSX
C722    LDA $0100,X
C725    STA $07FF     ; finish writing instruction JMP $Cn60 to $7FD
C728    ASL
C729    ASL
C72A    ASL
C72B    ASL
C72C    STA $43
C72E    LDA #$08
C730    STA $45
C732    LDA #$00
C734    STA $44
C736    STA $46
C738    STA $47
C73A    JSR $07FD
C73D    BCS C75D  - if carry set, error
C73F    LDA #$0A
C741    STA $45
C743    LDA #$01
C745    STA $46
C747    JSR $07FD
C74A    BCS $c75D - if carry set, error
C74C    LDA $0801
C74F    BEQ $C75D - if zero, error
C751    LDA #$01
C753    CMP $0800
C756    BNE $C75D - if not equal, error.
C758    LDX $43
C75A    JMP $0801 - proceed with boot.
C75D    JMP $E000 - jump into basic (or something)
```

So when a JSR to $Cn60 is made, the Apple2TS emulator does its pretend
call. Here it's full of BRK, so, it's not actually executing anything.

Disk driver calls are: STATUS, READ, WRITE, FORMAT.
STATUS
   checks if device is ready for a read or write. If not, set carry,
   and return error in A.

   If ready, clear carry, set A = $00, and return the number of blocks
   on the device in X (low byte) and Y (high byte).

```
DEVICE numbers (physical slot 5)
  S5,D1 = $50
  S5,D2 = $D0
  S1, D1 = $10
  S1, D2 = $90

DEVICE numbers (physical slot 5)
  S6,D1 = $60
  S6,D2 = $E0
  S2, D1 = $20
  S2, D2 = $A0
```

  What are return vlaues for read and write?

Device number is ( (Slot + (D-1)) * 0x10  )

```
Special locations in ROM:
$CnFC - $CnFD - total number of blocks on device
$CnFE - Status byte
  bit 7 - medium is removable
  bit 6 - device is interruptable
  bit 5-4 - number of volumes on device (0-3)
  bit 3 - device supports formatting
  bit 2 - device can be written to
  bit 1 - device can be read from
  bit 0 - device's status can be read (must be 1)
$CnFF - low byte of entry to driver routines
```

Ah, so this lets us do CDROMs and stuff (read-only, removable).

```
Call Parameters: - Zero Page

$42 - Command: 0 = STATUS, 1 = READ, 2 = WRITE, 3 = FORMAT
$43 - Unit number. 
  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+
|DR|  SLOT  | NOT USED  |
+--+--+--+--+--+--+--+--+
DR = Disk Read
SLOT = Slot number

$44-$45 - buffer pointer. (Start of 512 memory buffer for data transfer)
$46-$47 - Block number - block on disk for data transfer.
Error codes:
$00 - no error
$27 - I/O error
$28 - No device connected
$2B - write protected
```
So, one method to handle this would be to do this:
when we're about to enter the execution loop, if the PC
is $Cn60, or, one of a list of other registered addresses)
then we can call our special handler.
Which needs to do the following:
  * execute
  * pretend to do an RTS

OK this is exactly what Apple2ts does, and it seems reasonable. And it will
be lightning fast.

So, we can implement:
  * ProDOS Disk II Drive Controller (280 max blocks)
  * ProDOS 800k Drive Controller (1600 max blocks)
  * ProDOS Hard Disk Controller ($FFFF max blocks)
I recommend we flush write on every write.
The extended commands for SmartPort would let us do multiple blocks at once,
improving performance there.

ok, I need byte Cn07 to be set to $3C. $00 means "smartport", which I'm not supporting yet. This disk
I have wants a //e or //c. So let's find another image.

Holy grail! I am booting ProDOS 1.1.1 off 800k "disk" media and the ProDOS block driver is working!

ok, tomorrow, implement writing, it should be a nearly trivial addition.

NO DO IT NOW.

## January 9, 2025

So I have this idea for how to handle the infinite loop detection - the 65c02 (or maybe just 816) implemented
an instruction called WAI, that halted processing until an interrupt occurred. So we can do that when we hit an infinite
loop case - trigger a WAI state - this will then not run CPU instructions, but, will still do the other
event loop stuff, and, will sleep for 1/60th of a second at a time, letting the cpu rest.
infinite loops are not that unusual an occurence on old software. Seems reasonable.

I am working on pushing the media_descriptor stuff down into the hardware layer as far as makes sense.
This way there is only one place where media is updated.
Also, we can have a single place in the main loop and/or exit, where we can "unmount" and safely write
media to real disk.

I need to find a variety of 2mg images to test:
  * dos 3.3 image
  * prodos 143k image
  * prodos 800k image
  * hard disk drive image

## January 10, 2025

I have a cool idea. Set up a web site that mirrors the various image sites.
Provide a search engine that lets you search for images by name, description,
or, by the various attributes, AND BY file contents on the files inside the images!
And quickly browse the image contents (catalog), and, view the files inside. Could
use cp2 behind the scenes since it already knows all the image and file formats.
This would be handy for calaloguing my own stuff.

For later reference: Apple III hardware description:
https://groups.google.com/g/comp.sys.apple2/c/_NYADLx16G8/m/MZKv-Y20uTQJ
https://ftp.apple.asimov.net/documentation/apple3/A3SOSReferenceVol.1.pdf

Everything seems straightforward except:
"Extended/enhanced indirect addressing"
https://www.apple3.org/Documents/Magazines/AppleIIIExtendedAddressing.html
also discussed in detail in 2.4.2.2 in the A3 SOS Reference above.

Huge amount of Apple /// info here: https://ftp.apple.asimov.net/documentation/apple3/

There are great disassemblies of Apple III SOS and probably ROM. These can be used to 
answer questions about the hardware.

Apple III disk image formats:
https://retrocomputing.stackexchange.com/questions/12684/what-are-the-most-common-apple-ii-disk-image-formats-and-what-hardware-disk-driv

A whole bunch of Apple III software:
https://www.apple3.org/iiisoftware.html

I started thinking about other Apple II+ things need to be done; 
  * color hi-res.
  * shift-key mod. (Should be super simple, but need software to test).

Shift-key mod. Basically, if shift was pressed with key, then mark PB2 high. What address is this?

More generally: 

https://gswv.apple2.org.za/a2zine/faqs/Csa2KBPADJS.html

[x] Hm, I need to add in handling mapping of the $C800-$CFFF ROM space based on which peripheral card was accessed.

Let's look at the game controller stuff.

| Function | State | Address |
|----------|--------|----------|
| Annunciator 0 | off | $C058 |
| Annunciator 0 | on | $C059 |
| Annunciator 1 | off | $C05A |
| Annunciator 1 | on | $C05B |
| Annunciator 2 | off | $C05C |
| Annunciator 2 | on | $C05D |
| Annunciator 3 | off | $C05E |
| Annunciator 3 | on | $C05F |
| Strobe Output | | $C040 |
| Switch Input | 0 | $C061 |
| Switch Input | 1 | $C062 |
| Switch Input | 2 | $C063 |
| Analog Input | 0 | $C064 |
| Analog Input | 1 | $C065 |
| Analog Input | 2 | $C066 |
| Analog Input | 3 | $C067 |
| Analog Input | Reset | $C070 |


The Analog inputs work like this: hit the reset switch.
Then, read the analog input over and over until the "bit at the appropriate
memory location changes to 0".
About 3millisecond delay. The time it takes to decay is directly proportional
to the resistance across the input.

This is a super-useful looking general purpose - machine identifier, and joystick tester!

Converting a mouse location to a pseudo joystick location shouldn't be too hard. Let's say
we have a sort of "dead" zone in the middle of the virtual display, where the mouse can rest
and the joystick will read 128/128. then it's probably a pretty straight translation to
x/y coordinates of mouse relative to focus window, to a timer.

Well that wasn't bad at all. A weird thing happened playing sabotage, the mouse buttons
stopped firing. Keyboard still worked, and, mouse still steered. So there may be something
weird or it may be a bug in the game crack or something. Choplifter didn't seem to have any
issues. Overall, having the mouse->joystick be linear makes for somewhat slow movement.
Depends on the size of the screen. And I guess you can speed up your mouse movement on the host.

Also, if you zoom off the window and then click, something comes up and covers the screen
and you're way off base. Ah, so what I need is if I click in that window, the mouse locks to that window
and can't leave it.
The mechanics of the analog input simulation work just fine.

OK, the SDL Relative Mouse mode helps. The mouse can't leave. What I did is, when you click
inside the window, it locks mouse to window. Hit F1 to unlock. Shades of deep VMware purple.

in thinking about the shift key mod, it's super easy. However, it needs to tie into the keyboard.
(Just like I did F1 above).
This will be the same with //e, open apple and closed apple which need to map alt to game controller buttons.
But would like it to be a separate module. And there are some other modules that 
tie into the keyboard too where you don't necessarily want the code to handle the *whatever*
in the event loop or keyboard handler. Like, there is keyboard handling for the emulated
machine, and keyboard handling for other stuff.

I think the number of things that will do this is probably relatively small, so create an array
of key values and handler function pointers and just iterate them on a key event.

Todo for this:

[x] get window dimensions by calling SDL_GetWindowSize
[x] buy a usb joystick and see what is needed for that to work right.


Hm, how hard to go to a fullscreen mode?

## Jan 11, 2025

Push media write protect handling down into the hardware drivers.

have two interfaces for device init.

```
init_mb_DEVICENAME(cpu_state *cpu)
init_slot_DEVICENAME(cpu_state *cpu, int slot)
```

Ultimately a platform definition will include a list of these functions, in order,
to initialize the VM. Pick and choose the ones you want.

ok, did that, then worked on getting this thing to run as a Mac app bundle. That opened a rabbit hole of "where are my resource files? How are resources packaged?"

Learned a lot. A modern Mac app is just a folder with a .app extension. There is some 
metadata, but, this is way better than what they used to call Resource Forks. It's basically just a convention for the Finder. SDL provides some utility functions for finding your Resource folder. This is good for cross-platform. Linux and Windows will then likely work similarly, though Windows you'll register your icon somewhere. Cross that bridge when we get to it.

Also Chat Gippity helped improve structure of the CMakeLists where all the Mac-specific stuff.
Also, changed to require C++17 in order to access std::filesystem and maybe some other stuff.
This will be the start of a bunch of refactoring to make the code more C++17 compliant.
Since I got past my include file location issues of earlier, I can now be consistent and disciplined about using only modern C++ idioms for I/O and other stuff.

So have more thinking to do - for debug output, can write to a file. And create a debug log abstraction. 

[x] When a VM is off, its window can display the apple logo and the machine name underneath. (e.g. Apple IIe, //c etc.)
[x] edit the icon so it's square, and, has a transparent background where the white is.

Thinking about UI. Two ways to go here.
1. Do a web browser type, discussed above. Gives access to a broad range of UI tools in-browser, except perhaps the one we really need, which is select local file.
1. Switch to SDL3, which has a variety of UI tools and a bunch of other stuff to boot.
1. Stick with SDL2 and use this: https://github.com/btzy/nativefiledialog-extended and other such things.

I suspect switching to SDL3 now is the way to go, before I get too much further invested in SDL2.
and it's got the file dialog stuff, etc.

It looks like the audio stuff may be a bit of a lift - but, it will be cleaner and thread-safe. Important if I'm going
to do multiple VMs at once.

So UI. I have a vision.

Control Window:

The whole Control Window can be shown/hidden with a key shortcut (e.g. F1)

At the top, we have a menu bar with some basic stuff like help, about, quit.

There is a pane you can open to show the current machine's state: picture of the emulated machine (e.g. an Apple IIe pic, IIc pic, IIgs pic, etc.);
effective MHz; etc

Below that, a pane with some controls: reset button, power off, break into debugger; reverse analog input axes; change display mode (color, green screen amber screen);   

An accordion pane with disk drive images, to provide visual feedback on: disk activity; slot and drive; what disk image is mounted; etc.
Click on an eject button to eject a disk from that drive, which will do a clean unmount. Then, click on the disk image to mount a new image.

An accordion pane with info on any other virtual peripherals where it makes sense. map serial / parallel port to real device with an icon of the virtual device (e.g. a super serial card); choose joystick/gamepad device; choose audio device; we could map a serial port to a TCP IP and port (fun); virtual modem (ATDT1.2.3.4); Each device registers its own icon / UI logic.

An optional debugger window. The debugger provides a part GUI / part CLI interface into the system.

An optional printer output window. Of course you can hook up a real printer. I bet a bunch of Apple II software supports HP PCL, and of course later
and GS stuff will support PostScript. The ImageWriter was ubiquitous. Also Epson MX-80.  

Images and iconography are SVG.


SDL3 - ugh! lot of work.
Looks like there is SDL_SetRenderDrawColor which might let us draw with white pixels but translate to a different color. Maybe good for a "green screen" mode or Amber mode.

I ripped the display code out of the emulator and put it in a separate test program. It's working. That is weird. I must have a memory management bug somewhere.

## Jan 12, 2025

OH there was a comment that "the texture is write-only.":
Warning: Please note that SDL_LockTexture() is intended to be write-only; it will not guarantee the previous contents of the texture will be provided. You must fully initialize any area of a texture that you lock before unlocking it, as the pixels might otherwise be uninitialized memory.

That's the issue, and that's what is different between the main thing and the test thing. The test thing is writing the entire texture.
Let me try writing only part of them.

NO that wasn't the issue. Though it could have been.

The issue was very simple: SDL3 defaults to BLEND. Previously we had set hint to overwrite. So, that was that! What a PITA!
Also, it defaults to fuzzy (linear) scaling. This provides a more old-CRT-like effect, which is common on other emulators. But for certain 
applications you might want to use nearest neighbor scaling, which provides exact-sharp pixel scaling. ultimately provide a toggle for this.

OK, I think the last thing I need to do is to get the audio working.

They changed a few things, including how the callback works. Now, the callback is called, and you use a call PUT to put however many bytes
of audio data you want into the stream. This is likely better. What may be even better yet is to include an audio frame processor
into the main event loop. Now I'm curious, I wonder if I can now open a 1-bit channel.. no. But they do offer floating point samples.
Consider that later.

I can shove as much or as little audio data as I want into the stream. It will take care of synchronizing buffers. So I will
do that. We will return data generated in the cpu cycles between this time and last time.

OK! I got the speaker/audio working again this time as a push-based system. Every 17000 'cpu cycles' I generate an audio frame.
However, over time the audio stream is getting delayed more and more. I.e., Out of sync with realtime. 
This means I'm pushing more data than I should, and the player is getting behind playing it.
So I need to compress the output data stream a little if it gets behind.
How do I know if I'm behind? Calculate how many bytes we sent to the player based on cycles, compared to how many we should have sent
based on realtime.

## Jan 13, 2025

I think I'm mostly done with the SDL3 refactor. I am not happy with the audio stuff. I am going to break
it out into a standalone test program to continue to tune it. There is almost certainly a bug somewhere
when handling blips that cross frame boundaries. But I took a long recording of audio and 
will be able to test iterations much faster this way.

In the meantime, clean up the mess in the code and push into the repo.

Watched a video by Chris Torrance on how to do audio.

Post events: cycles AND time. (or, just time).
Then time to samples.

## Jan 14, 2025

First pass at redoing the audio, as a test program, is done. I take the cycle-event recording,
generate audio data, and output to a WAV file. The WAV file reconstruction is based purely on
cycle timing. However, I think I'm underrunning the buffer, so need to fix that.

Then, I need to add the smarts per the Chris Torrance algorithm.

It will be imporant to fix the overall cycle timing. The last iteration, in the audio I was patching
around that. The timing is running a bit fast.

Let's say I have the event loop:

```
while (true) {
a1:
    execute_next();
       fetch opcode & cycle
       fetch operand & cycle (potentially many times)
a2:
    do_some_event_loop_stuff();
a3:
    video_update();
a4:
    every X cycles, generation audio frame.
}
```

When we 'wait', we want to pause for a time based on the last time. I.e., each cycle, 
calculate the next "wakeup" time. Then, any sleep, is "sleep for wakeup_time - current_time".
The wakeup_time is basically a1 + X. Everything else needs to be ignored.
This will take into account all the time spent doing other stuff in rest of the event loop.
And, keep track of whether we fall behind. If we do, we will need to be sure not to trigger
audio and video updates in the same event loop. (That should be easy to fix, do if else if else
instead of if, if.)

One thing that might be required, is to sleep and sync only every X cycles. Instead of trying to
microsleep every cpu cycle, sleeps are likely to be more accurate if we sleep less often for longer.
This also provides more time to do other stuff in the event loop.

Since audio events are recorded based on the cpu cycles, it shouldn't matter if we run 10 instructions
at full speed then sleep some. And it will all be too fast for humans to notice. Hopefully.

The fastest cycle time required to support is the 2.8MHz of the IIgs. Everything else can be
Ludicrous Speed.

At start of program, last_cycle_count = 0, last_cycle_time = GetTicksNS();
We don't know how many cycles will be executed beforehand, because it depends on the opcode
etc.
But, when we're through with an opcode, however many minimum number of cycles, we keep track of that
with cpu->cycles - last_cycle_count. 

Unless there is a slip event - the next target time should be calculated based on the **original start time**.

next_cycle_time = start_cycle_time + (ns_per_cycle * cycles_elapsed)
WaitNS(next_cycle_time - GetTicksNS())

Let's just do a test program for cycle timing.

Simple loop, I can do busy waits pretty well. I run 30 iterations, and I wake up right on time.
A single printf in the loop causes the timing to be off by a total of 130,000 to 260,000 ns over the 
loop.

## Jan 16, 2025

### Threads vs short bursts of free-run CPU

### Free-run bursts

I think we definitely need to be smarter about how we handle cycle timing.
One issue I was thinking about, is that the time it takes to execute a video frame update
is not 0. It's also not constant. In a real system, it's going on in the background, constantly.
Not just each 1/60th second. I think it makes sense to do this as a separate thread. Same with the audio.
The other general event processing, maybe. Some of that is probably pretty fast.
Certainly, anything that might print to the console is going to be slow as we've seen. 250-500 microseconds.
I've demonstrated that, absent doing things that take a huge amount of time, we can key pretty accurately
on cycle times with the nanosecond precision of SDL_GetTicksNS().

One approach is to separate cpu from other stuff in separate threads. Another, is to bunch up CPU
processing into bursts. I.e. not try to delay and sync every cycle, but instead run a bunch of cycles,
then execute 'stuff', then do a cycle sync.

My concern with this is over things like, say, reading the keyboard. So let's say we read the keyboard in
a tight loop like apples do. The average user might type a key every 1-2 seconds. An Apple II would
wait for keyboard like 500,000 loop iterations during that time. maybe it's going to do other things too. I guess if the
resolution of these is still much higher than human reaction time, it won't be noticeable. But, I still am
unsure about it. I guess the only way is to try it.

Video frames are 60 times per second. Audio maybe 30-60.

So a loop would be:

```
free-run CPU for 17,008 cpu cycles. (60fps @ 1.0205mhz)
video update.
audio update.
whatever else update.
wait/sleep for end of 17,000 cpu cycle.
```

17K cycles is 16,667 microseconds.
or, 16,667,000 nanoseconds. That seems like a fair bit of time to get things done.

We could do this at 100fps or 120fps. (Still only update the video 60fps). That's 8M nanoseconds.
What if we "free run" one instruction, then do other stuff. 1 instruction is anywhere from 2 to 6/7
cycles. That's 2000 to 7000 nanoseconds. A few thousand instructions.

This shouldn't be that hard to implement.

```
last_cycle_count; last_cycle_time;

execute_next();
do_some_other_stuff();
busy wait until (current_time > last_cycle_time + (this_cycle_count - last_cycle_count) * ns_per_cycle)

loop
```

This loop can operate with however many "execute_next" in a row we want. i.e. can have an inner loop
that does X instructions. This will always sync up afterward.

Today we have huge numbers of clock slips because we can't get much of anything done in the one microsecond
we have after the last cycle of an instruction.

Let's try this first.

Weird audio artifacts. Not worried about that right now. Doing a loop every instruction, we are getting a few
thousand slips per minute.

Let's do an inner loop of 10 instructions.

Ha! Now I'm getting almost exactly 300 slips per 5 seconds. That's 60 slips per second, i.e., video/audio frames.

ok, how about 100 instructions. (100-500 microseconds).. 11 to 12 slips per second.
free-run 17000 cycles:
   5119863 delta 75640749 cycles clock-mode: 2 CPS: 1.023973 MHz [ slips: 4, busy: 0, sleep: 0]

virtually no slips. Two weird artifacts: clicks when clicking in and out of the window.
and, on boot, the display didn't properly clear after showing all 0's.

Also note that the frequency check is now reading 1.023973. 

Remember I think clicks are due to underruns. The current speaker code is only ever emitting +1 or -1, but 
I bet the audio system fills in with 0s when it's behind.

Choplifter works well. Taxman did too. I did in fact see some screen flicker with this approach. If we implement
the IIe VBL register, and set it appropriately (hmm, it should basically always be off), then newer stuff
will work well. Notably I didn't get clicks on this run. It probably happens when it starts with timing
slightly wonky. 

ha! Forgot. I'm running the absolutely wrong audio code.

On a clock speed change, we need to bail out of the CPU thread early, to give our algorithm a chance
to resync to the new clock speed.

Overall so far this is working well. Just the video bug on startup.

### Threads

A second option is to do multiple threads. This will add pretty significant complexity, requiring mutex around
shared data structures. Let's examine.

SDL video updates can only occur from the main thread. That's pretty definitive. If video has to be on main
thread, then the CPU is going to have to be an alternate thread.

SDL has some support for inter-thread communication. SDL_RunOnMainThread() requests a callback be run on the main thread. There is also the ability to define custom messages for the event queue, and pass messages between threads this way.

So let's look at the data structures we have, and consider them in a multithreaded context.

Video memory. Video ram is written to by CPU. Then read by video code. As long as the location of the video
RAM doesn't change, then this is one-way data flow and should be thread safe. The CPU thread writing new values
into video memory won't hurt or break the video update process. it could cause flicker. But that's like the
real computer.

Paravirtual disk driver. right now, the cpu is sort of 'halted' and disk I/O performed reading or writing into
CPU RAM, then cpu resumed. This will not likely happen in space between cycles. So CPU would halt, disk I/O
performed in a separate thread, then CPU resume. We also don't want this I/O to tie up the video or audio
output.

For GS, there will be a keyboard buffer. There are several bits of hardware like that. That's all going to be
write at one point and read from another point. Synchronization here not strictly required.

The audio event buffer is like that, too. If over overrun (overfill the buffer) then events just have to get dropped.
But, we should be able to manage this w/o mutexes.

In fact, any mutex here could re-cause the problem we're trying to avoid. So these must be done anywhere
except in the CPU thread.

If we go this second route, then we get our support for multiple VMs much more easily.

Support vertical blanking sync emulation would be a lot easier as a separate thread. And the
IIgs "3200 colors" mode, which is vertical blanking sync on steroids, would also not likely work
without a multi-threaded approach.

Certain devices will have a "cpu context" and an "event loop context". E.g., video is split.
The CPU thread will decode a video address to set the line dirty flag. But it won't do the 
video update itself. Code that runs from memory_write etc also needs to be sure to be short.

## Revisiting the speaker code.

ok now that we have the clocking synced pretty well at 1.0205MHz (1.020571 to be precise),
let's revisit speaker code.

yes, I improved cycle timing in audio3, and the buzzing has gone away while it's running, and it's staying
very well synced.

The trick will be how to make that work at 2.8MHz and in Ludicrous Speed.

## Jan 17, 2025

audio4 (current iteration of the audio test) isn't bad. the timing is good. The algorithm needs to be improved
per Torrance. So. Let's do that, then, change the buffer algorithm to:

1. Call audio_generate_frame() roughly every 17ms. (As best as we can).
1. calculate number of samples to generate based on time elspsed since last call into audio_generate_frame(). Because it's critical that we generate enough samples to keep the buffer from underrunning.
1. map a cpu cycle range to the samples range. This will be based on the effective cycles per second: time_delta / cycles_delta.
1. Iterate the samples.

The naive approach is:
1. Loop number of samples:
    1. count cycles one by one, checking the queue for events.
        1.   If there's an event, switch the sign of contribution.
        1.   add +1 (or -1) for each cycle (i.e. iterate for cycles_per_sample).
    1. emit sample, repeat

Downside of this approach is that we have a loop iteration for every cycle.
It feels like it would be better to iterate only the samples and do some 
math to figure out how many cycles to skip. Let's get this working first,
then see if we can improve it.

When the emulator starts up, and this is a big difference from the audio4 test program,
we immediately encounter clock slips:

```
last_cycle_count: 2 last_cycle_time: 279866291
last_cycle_count: 17011 last_cycle_time: 296535125
 tm_delta: 16669375 cpu_delta: 17009 samp_c: 736 cyc/samp: 23 cyc range: [0 - 17009] evtq: 41 qd_samp: 1470
last_cycle_count: 34019 last_cycle_time: 612695375
queue underrun 0
 tm_delta: 92876800 cpu_delta: 94772 samp_c: 4096 cyc/samp: 23 cyc range: [17010 - 111782] evtq: 62 qd_samp: 0
last_cycle_count: 51029 last_cycle_time: 658447458
 tm_delta: 92876800 cpu_delta: 94772 samp_c: 4096 cyc/samp: 23 cyc range: [111783 - 206555] evtq: 31 qd_samp: 4096
last_cycle_count: 68037 last_cycle_time: 675941458
 tm_delta: 92876800 cpu_delta: 94772 samp_c: 4096 cyc/samp: 23 cyc range: [206556 - 301328] evtq: 31 qd_samp: 10240
last_cycle_count: 85045 last_cycle_time: 693876750
 tm_delta: 92876800 cpu_delta: 94772 samp_c: 4096 cyc/samp: 23 cyc range: [301329 - 396101] evtq: 32 qd_samp: 16384
last_cycle_count: 102056 last_cycle_time: 711035000
 tm_delta: 45802508 cpu_delta: 46737 samp_c: 2020 cyc/samp: 23 cyc range: [396102 - 442839] evtq: 26 qd_samp: 22528
last_cycle_count: 119066 last_cycle_time: 727704875
 tm_delta: 13960042 cpu_delta: 14244 samp_c: 616 cyc/samp: 23 cyc range: [442840 - 457084] evtq: 0 qd_samp: 26568
last_cycle_count: 136075 last_cycle_time: 744373833
 tm_delta: 16596833 cpu_delta: 16935 samp_c: 732 cyc/samp: 23 cyc range: [457085 - 474020] evtq: 0 qd_samp: 25752
last_cycle_count: 153083 last_cycle_time: 761041791
 tm_delta: 16648000 cpu_delta: 16987 samp_c: 735 cyc/samp: 23 cyc range: [474021 - 491008] evtq: 0 qd_samp: 25168
 ```

 The first time through the loop is fine, but then there is a huge delay to get
 back around. 
 There is an initial huge jump. The video is being called. I bet there is a bunch of
 setup required in event_poll(), the video display, etc the first we call them.
 So, call them once before the CPU loop starts.

 ## Jan 18, 2025

```
 last_cycle_count: 2 last_cycle_time: 582707583
last_cycle_count: 17011 last_cycle_time: 640102875
 tm_delta: 1554875 cpu_delta: 1586 samp_c: 69 cyc/samp: 22 cyc range: [0 - 1586] evtq: 41 qd_samp: 1784
last_cycle_count: 34019 last_cycle_time: 656770750
queue underrun 0
```

I've been jiggering around with the audio code. There are lots of assumptions here.
One was, the stream doesn't start automatically, but I think that might have been wrong. The SDL 
docs are inconsistent on this point.

The first time to audio_generate_frame, we do nothing.
The second time through, we're generating only 69 samples because the time delta
is only 1.5 microseconds. so that's why we underrun. Why is this..

the issue is that something near startup is sucking up a lot of time in the first
couple loops. Instrument to see what this is. There is a big jump in cycle time delta.

OK after a herculean (well perhaps I exaggerate) effort, I've got the audio working, at 1MHz
at least. At ludicrous speed, several things aren't working right.

At higher speeds, quickly gets out of sync. Cross that bridge later. The 1-bit speaker
is sort of ludicrous at 250MHz anyway ha ha. (It's playing back events at 1MHz speed no matter what, 
but at faster speeds it's loading up the buffer way faster). When speed shifting, flush the audio
buffer. (for a IIgs running Apple II stuff in emulation mode, slow down to 1MHz when PB=0 or PB=1).

Video - there is no restriction on the video update rate.

If we are in ludicrous speed, limit video updates to 60fps. ok, done. I did for events, etc. So we're back to 

```
1280493489 delta 4415324039 cycles clock-mode: 0 CPS: 256.098694 MHz [ slips: 0, busy: 0, sleep: 0]
```

256MHz apple II. I mean, come on man.

ok what's next?

I'm going to implement green and amber text and lores modes, and implement a color mode switch.

But then I need to do a code cleanup and move globals and statics in a few places into structs attached
to the cpu_state.

For that (which I should implement for this color mode thing as the first implementation of it), I need
a generalized data storage thing. If this was JavaScript I would just pile things into an associative array,
or properties.

But this is C++, and we need to allocate memory. What if I do an enum of 'names' ('properties'), and then
the cpu_state has an array of void pointers. Each module that needs this type of storage gets a name assigned.
When it needs its state, it typecasts the pointer to its correct type. Then the cpu_state doesn't care
what the structs look like. If a module needs multiple chunks it can define its static struct to have them.

## Jan 19, 2025

OK that's done for the display code. Wasn't too bad. I even made it a class instead of a struct to use
a constructor init. Will be pushing the codebase more into this direction.

For color-mode switch, I need to figure out how to display lo-res on a greenscreen. And, I need color
hi-res.

To do hi-res properly I need to double the number of horizontal pixels to simulate the phase shifting.
I think I probably also want to implement a border around the simulated NTSC screen. (GS will eventually require
this because you can set the border color).

these are pretty good discussions of hi-res:
https://forums.atariage.com/topic/241626-double-hi-res-graphics/page/4/
https://nicole.express/2024/phasing-in-and-out-of-existence.html

Hm found a bug. There is a bizarro artifact. If there is text update on lines y=22 or =23, it gets
blasted into the borders both bottom and right.

I don't think I'm doing anything wrong. If I don't specify the dstrect or srcrect, it defaults to scaling the texture to the window size. If I
specify dstrect, or, dstrect and srcrect, I get the artifacts in the odd places.

Yep. If I select the opengl renderer, it works fine. Using the default renderer (specifying with 'null' in CreateRenderer) 
I get the artifacts. WEIRD. And annoying.

So now I'll have to read up on these. And I guess send in a bug report to SDL.

ok Color Hi-res.

Understanding the Apple II has the best discussion of this. That guy was a genius.

Let's see if I can boil the rules down.
1. Two adjacent one bits are white. I.e., even if we set a pixel as a color due to preceding 0 bit, a second 1 bit in a row will turn the previous dot and the current dot white.
1. If even dot is turned on, they are violet.
1. Odd dots are green.

If the high bit of a hi-res byte is set, then the signal is delayed by 1/2 a dot, and the colors
violet and green become blue and orange.

Delayed and undelayed signals interfere with each other. So if we go from a 0 to 1 or 1 to 0, there will be special
stuff happening at the edges of that intersection.

"There are 560 dot positions in a row. Color depends on position. There are 140 violet,
140 green, 140 blue, and 140 orange positions."

"Any time you plot a green dot in the same 7-dot pattern as an orange dot, that orange dot
turns to green because D7 had to be reset in that memory location to plot the green dot.

Two adjacent color dots of the same color appear solid because of analog switch time and blurring. This is simulated
on Apple2TS by drawing 3 purple dots in a row in a case like this.

Pages 8-21 to 8-23:
The first 14M period of a delayed pattern (bit 7 = 1) is controlled by the last dot of the previous pattern.
Switching from 1 to 0 (delayed to undelayed) "will cut off the tail of the current pattern, cuts the last
dot in half".
cutting off or extending the dot has the effect of slightly changing the dot pattern and noticaebly,
changing the coloring of the border dots.
LORES colors 7 and 2 can be produced at even/odd memory addressing order.
Colors D and 8 can be produced at odd/even borders.
Colors B and E can be produced at odd/even or even/odd borders.
Lores 7 can also be produced at the far left of the screen, and color E can produced
at the far right.
color 1 can be produced only at the far left the hires screen. orange hires dots at the right side of the screen
are dark brown.

So Apple IIts has handled some of this (the stuff in-line) , but not all of it (the stuff that crosses scanlines). This all sounds complex but it's a fairly simple set of rules, I think.

In any event my idea of using the 560 dot positions seems correct.
```
                     0.1.2.3.4.5.6.
2000: 01             *_______
2400: 81             _*______
2800: 02             __*_____
2c00: 82             ___*____
```
creates a purple dot, then blue, then green, then orange, in very tight pixel positions.
Numbers on the top are a whole dot position. The periods are a half dot position. My IIe monitor
is super-fringy, you can clearly see a half dot, a full dot, and a half dot of each single color.

If hi bit of a byte is set, we will plot into odd pixel positions. And pick color based on 
bits being last=0, current =1; last=1, current=0; last=0, current=0; last=1, current=1. Keep a
sliding window of 2 bits.

So, a design choice. Do we make the entire Texture 560 pixels and change the scaling factor.
Or, only change for hires? I think it would be simpler to do all of them. The text and lores
code will need to change to plot twice as many horizontal pixels.

Thinking about this - color fringe effect on split screen text and lores/hires, the text pixels will be
colored exactly as if they were hires pixels with hi bit always cleared.
So architecturally, perhaps we paint everything into 560, and have a post-processing step that does
the coloration, which would be the same for all modes. Have to ponder how lores fits into this scheme
too, i.e., how are the pixels shifted there.. don't have to post-process. just draw using the hi-res algo,
but draw the data from the text matrix.

There will be an "RGB Monitor" mode that will have no doubling of color hires pixels and no text fringe; and 
composite mode that will fringe text and blue the color hires.


## Jan 20, 2025

It's the third Monday in January! You know what that means! It's National GSSquared Got Color Hi-Res Day!

So the above wasn't too bad at all. 

Like I did with the hgr function, move the x = 0 to 40 loops for text and lores into those routines.

All dislay rendering is now done with scanlines, i.e., an entire row of pixels horizontally.
This moves loop inside the render function, saving a bunch of function calls. Also makes
the API for a display handler consistent across all display types.

of course, it wasn't a bug - I had a brief exchange with the SDL lead dev and he pointed out
I needed to do SDL_RenderClear() every single frame. This is part of the GPU voodoo I don't have
experience with. But now I do! A little anyway.

## Jan 22, 2025


There are SDL variables that can make the drawing even blurrier. Experiment with them.

For the color-stretch mode, I'd say, if I draw a color dot and two dots to the left is the same color,
fill in the middle dot with same color.
I am also not sure about the drawing the white dot algorithm as it is. Review it and compare to other examples.

Hi-res tweaks. "Composite" mode is drawing too many pixels I think. Instead of 2 pixels (4 dots) maybe do 1.5 pixels (3 dots)?
letters like lowercase 'm' are completely filled in, and that's not quite right..

So I'm trying the Introducing the Apple II disk. It said it was iie only. it's running on my II+-ish.
it understands lowercase, but, they are reversed. shift-letter gives me lowercase. letter gives me uppercase.
I bet that's because my keyboard routines are not converting to uppercase when I am holding shift. That's funny.
up and down arrow don't work because I never mapped them. Can't play the rabbit game then!

## Jan 23, 2025

To get sound working on the Linux build:
install libasound2-dev libpulse-dev
and doing a cmake reconfigure. Works great with the $15 USB speaker I bought. That's just crazy, and fun. I remember how expensive sound cards used to be. And that didn't even include the speakers.

Experimenting with window resizing. I have it constraining size to the correct aspect ratio, and, 
modifying the scale factor in the renderer to match. Now test full screen mode. We'll toggle that
based on the F3 key since I already have F11 used for something else.

I think there is a bug in the lo-res code now where on switching to lo-res mode it's not dirtying the lines
to force a redraw. At least, my lo-res apple is not being drawn on boot of some of these disks. Ah, it was
a change in the code that reacts to writes to text memory. I "optimized" it to only do text updates
in lower portion when in graphics mode. But needed to also check to hi-res mode.

Full screen mode works but it allows use of the wrong aspect ratio. I think in fullscreen mode we need to 
force the scale to match aspect ratio, then center content inside the fullscreen window.

[x] in game code, window_width and height need to read the actual window size, or, use some other method
to constrain the mouse movement.

## Jan 24, 2025

The new gamepad arrived. This one is bluetooth and wired (supposedly). Compatible with everything supposedly. Works
on the Mac. I have it coded in to GS2 and it's working well. One notable thing, is if you go straight horizontal or vertical, it will scale to +/- 32767. IF you go diagonal, it will scale to +/- 24000 or so. The range of motion on the joystick is actually circular, whereas on the original apple II joystick it was square. I get the rationale here, my recollection is that the Apple II stick would sometimes get bound up in the corners.

This thing has a crazy number of controls. There are like throttle buttons on each side, that are pressure-sensitive (i.e.
scale depending on how far you depress them). Four X / Y / A / B buttons. And of course the four-way nintendo style + control. I am unclear on how button mappings work, and it is possible I will have to switch to the "gamepad" API which supposedly handles all the axis and button mapping stuff for you. Look into that more.

But I have successfully played the ole Choplifter with it. It's a lot easier with a proper joystick, even if it is
tiny compared to my original II stick, which I really miss. (Those things were great).

This had some sample code that helped me get started.

https://blog.rubenwardy.com/2023/01/24/using_sdl_gamecontroller/


## Jan 25, 2025

This doc has a pretty useful description of the hi-res circuitry.

"Apple II Circuit Description"
https://ia902301.us.archive.org/31/items/apple-ii-circuit-description/Image081317140426.merged.pdf

also the game controller stuff. That is extremely simple. I am pondering building paddles.

Need to re-do the gamecontroller logic to use the GameController API instead. This will help ensure compatibility with many different gamepad controller devices.

Interesting thought: map the nintendo style + control to A Z ARROWS or IJKL as an option. If we put a single game onto a floppy, that disk image could have
metadata to indicate what the controller - keyboard mapping should be. That is actually some hot stuff right there. Define some sort of metadata format.
Probably JSON, easy to store and generate and work with.

Other metadata: minimum system requirements. 

## Jan 27, 2025

had an idea. Do a "screen shot" key, that will dump $2000 to $3FFF to a file. Then we can write a hi-res tester that will much more easily
allow us to test hi-res rendering. Like what I did with the audio recording and test stuff. Yah baby.

## Jan 30, 2025

looking at the disk II code. The program Applesauce actually visualizes a disk image showing the sectors, data, etc.
There are some interesting differences in how mine looks versus their nibblization. First off, they show stuff on the quarter tracks.
Like, why. Overall, my image is darker. Probably because of the quarter track thing. Anyway.
Their nibblization has the following differences to mine:
They have more Sync A bytes than I do. 120 vs 80.
They have FEWER Sync B bytes. 6 vs 10.
Fewer Sync C than me. 17 vs 20.
Of course, in their thing, they display the sync bytes as 10 bit. Is that relevant to anything?
That said, their gap duration is a bit LONGER than mine - 657 microseconds vs 626. This is because of the 10-bit thing.
Perhaps I should *extend* my gaps a bit to make them the same number of microseconds. It's possible code isn't getting back
to this part of the read loop fast enough, and skipping. Though that would feel like 'stop working' entirely since it would
be exactly the same each time. Something to consider.

I found a "blank.nib" file. That image does not look like clean conical slices. Every track is skewed relative to the last one.
Inside a single track I'm fine. When we skip to the next track, we might have to wait a whole revolution. Whereas a regular inited
disk is probably more like the skewed version (blank.nib) because there is no sector alignment on the Apple II. And that
might affect performance. This would be implemented by starting the next track at the current location, and wrapping
the write position around based on wherever the virtual head is. NOT resetting to 0. This would really depend on the specific
init process used on a disk.

Looking at "DOS 3.3 System Master.woz" which is another nibblied format - that DOES track the 10-bit vs 8-bit thing.
These sectors are lined up in clean cones. And it has the quarter tracks too. Of course, .woz does not imply nibblized.
It can contain other formats too.

I think another difference between mine and the others, is that the blank space at the end of a track is all 0's in my image.
And the gap is large - it amounts to almost an entire sector width.

In blank.nib, the tracks are exactly 0x1A00 long as in mine. But, their tracks definitely do not always start with a sync gap.
```
0019F0: FF FF FF FF  FF FF FF FF  FF FF FF FF  FF FF FF FF  . . . . . . . . . . . . . . . .
001A00: B2 B7 CE B2  B7 CE A6 E7  D9 AF 9B A6  E6 DA AE 9A  . . . . . . . . . . . . . . . .
001A10: DC D9 9B 9A  CB CE B2 B7  CE FE F9 9B  AC DA DE AA  . . . . . . . . . . . . . . . .
001A20: EB FF EB FF  FE FF FF FF  FF FF FF FF  FF FF FF FF  . . . . . . . . . . . . . . . .
```

We go from sync bytes directly into some data. Then sync pattern later. This is what generates the skewed pattern we see.

This is being triggered by how much faster an image seems to boot up in apple2ts. I should time the same image in both
a2ts and gs2. Do that tomorrow when I'm not yawning.

Walking through the tracks in blank.nib one by one, each track starts with a different sector. F, then E, then D, etc.

[x] So, those two things to try: (1) pad out the sector gaps to make their uS duration similar to these other nibs; and do the skew
thing. And if there are any bytes left, we should pad out with FF instead of with 00. (2) try just jumping to next track at
same byte index, instead of resetting to 0.

I did a fair bit of work today, on arqyv. Got the vm setup, got asimov mirrored. There are other sites to mirror. I'm going to
need a lot more storage for those, likely (I'd prefer to just host here but Xfinity upload is too slow). If I ever get fiber again
I can self-host it.

So I can test this by d/l some nib format disks that are done this way and see how fast they are.

## Jan 31, 2025

So, add .nib support to the diskii routines.

Kaves of Karkhan seems to boot and play. It's a copy-protected disk that uses AB as the sync byte instead of FF.
There may be other differences too. But, the emulation works well enough to do this!

## Feb 1, 2025

The .Woz format document has some very interesting and useful info about copy protection schemes. And about the hardware.
Take-aways:

https://applesaucefdc.com/woz/reference1/

"Every soft switch on an even address should actually return the value of the data latch."

Also, "1 second delay after accessing the drive motor off" at $C088,X needs to be implemented.

We might need a concept that the virtual disk is spinning underneath the head. E.g., each time we're read, check the CPU
cycle count and move the virtual head position accordingly. 4uS per bit.

Also, "When you do change tracks, you need to start the new bitstream at the same relative bitstream position – you cannot simply start the pointer at the beginning of the stream. You need to maintain the illusion of the head being over the same area of the disk, just shifted to a new track."

This is for copy protection purposes. Finally,

"Now that we are back to having long runs of 0s in the bitstream, we now need to emulate the MC3470 freaking out about them. The recommended method is that once we have passed three 0 bits in a row from the WOZ bitstream to the emulated disk controller card, we need to start passing in random bits until the WOZ bitstream contains a 1 bit. We then send the 1 and continue on with the real data."

That is pretty wild. 

This document is implying that it stores the extra 0 bits in a track stream in the sync nibbles. Will need to find a .woz 
image of some crazy copy protected disk to test this. (And review with Applesauce).

## Feb 2, 2025

So I have some various stuff working to support the Apple II Memory Expansion Card.
The C8xx mapping, for one, and CFFF de-mapping.
Booting ProDOS, is causing a hang. I must have done something wrong. I will manually exercise
the 'hardware' to test.

## Feb 4, 2025

So, when there are things that are about to happen that we know will suck up a bunch of time (for example, opening a file dialog), we can Pause the audio stream until that call has completed. Other things in this realm would be, loading a floppy disk image.

After opening a file dialog, our window is no longer in focus. I have to click to bring it into focus, then click again to open the dialog again.

The dialog thing is only callable from the main thread. So we're back to considering, is this fine, or, do we need to put the CPU and audio into their own threads, to prevent stuff like this from interfering with the (one, really) realtime aspect to the software?

## Feb 9, 2025

When the OSD is open, keyboard and mouse events should go to it. So, perhaps the event routine should do this: return true if the event was handled, false if not. 

The joystick is now reading upside down in Choplifter. Is there a flag for controlling how the axes are read?

## Feb 11, 2025

Looking at Disk II code. When we switch tracks, we DO have the head in the same relative index into the track data. What we are not simulating, is the disk continuing to spin under the head when the CPU is off doing other stuff. 

The math here should be:

3.910us per bit. 

| Gap | Our bits | Applesauce bits | Action | 
|-----|----------|-----------------|------|
| Gap A | 640 | 1198 | add 1198 - 640 / 8 = 69 bytes more FF |
| Gap B | 80 | 60 | remove 1 byte FF |
| Gap C | 160 | 168 | add 1 byte FF |

Gap A is track start.

Now, at end, we also need to write 0xFF to the end of the track buffer. So from whatever the index is, through 0x19FF. That I did. Didn't make a difference.

I don't measure much difference at this point between the A2 emulator I downloaded and GS2.

## Feb 12, 2025

Bring checking of application-specific hot keys back into main event handler area, and out of 'keyboard'. We will have multiple keyboard handlers at some point.

Make speed control buttons.

## Feb 13, 2025

thinking about halt. Two things shouldn't crash the emulator:
* Halt (STP instruction)
* jump to self (infinite loop).

Now, we can treat the second as a case of the first. So what we want to do on a HLT, is, continue the event loop but stop executing instructions on the CPU. OK, this has been taken care of. On reset, halt is cleared. We only exit the event loop if halt is set to HLT_USER. (i.e., F12 or window close).

I disabled the jump to self test. We may want to enable it under special circumstances (i.e., running the test suite).

### Reset button

that was pretty quick.

### Power On / Power Off Button

Isolate the stuff that allocates memory, etc, and starts up the CPU. This is as distinguished from setting up the display window, and other host-level stuff.

Like, I should do something! The big daddy is to mount new disk images from the OSD. So let's work on that.

### Need to draw a black background with 100% opacity before we draw anything else in the OSD. Or, just fill the rest of the OSD. I may not be filling the entire thing.

### Hook up mount disk image code

let's do the naive approach first - change out the disk image from inside the callback handler. Apparently these are called from the event loop. We may assume it will take some significant amount of time to process the mount. That means we will likely need to fire off a thread to do it. But, start first and then measure.

Create a util/mount_unmount.cpp file to handle these.

## Feb 14, 2025

We need some kind of hook between the OSD display stuff, and something we can query to get and manipulate the status of disk drives, from a higher level than the diskii module for instance. From the GUI, we need to:

mount media
unmount media
write protect media (eventually)
query status (running, mounted, write protected, etc.)

The status will tell us how to display the particular widget in the OSD.

## Feb 15, 2025

Fixed a bug in the system - only 48K RAM was being allocated but of course the language card was using memory locations above that. Somehow that was working on the Mac, mostly, but blew chunks quickly on Linux. Enabled bounds checking in CMake Debug and rapidly diagnosed the problem, as well as a few other bounds problems.

The checks also identify memory leaks. There is a small amount of that going on right now. Of course we largely don't deallocate any memory on shutdown. Investigate later.

## Feb 16, 2025

We are now successfully mounting disk images at runtime. Was able to play Oregon Trail which requires flipping back and forth between two disks. And, can pretty reliably switch disks in DOS / ProDOS and CAT/CATALOG etc.

The OSD display of the drives does not track the actual drive status though. Have to somehow tie that in.

Getting close. However, the diskii drive motor never turns off. Here's what is happening:
* drive is running
* sector read
* drive off sent
* timer is set
* but the disk code never hits any diskii softswitches again, so I never get a chance to see the timer expired and to turn the motor off.

So, the diskII module needs to be called periodically to check on timers and update state.

I need a way for a slot to register a callback, called whenever a timer expires. SDL has SDL_AddTimer(), however, I am thinking of a more general purpose mechanism. Some devices will ultimately need to generate interrupts on the basis of timers.

Also, diskII needs a reset handler. From UTA2: "Pressing RESET causes the delay timer to clear and turns off the drive almost immediately."

ok, some progress. Drive status working. Need to be able to unmount, not just mount new media.

I sort of have mounting image on Drive 2 working. However, I get an I/O error or infinite spin the first time I catalog,d2. SEems like I'm overlooking a state change to drive 2 somewhere. If I then RESET, and try again, it works. OK. When I've booted, and then I do a catalog,s6,d2, it turns on the motor on drive 1, not drive 2! I reset, then do it again, then it turns on the motor on drive 2!

It's not seeing my motor on after doing drive_select 1 the first time:
I have a drive 0 motor 0 for some reason..

```
slot 6, drive 0, motor 0
slot 6, drive 0, drive_select 1
```

second time it does it:
```
slot 6, drive 1, motor 0
slot 6, drive 1, drive_select 1
slot 6, drive 1, motor 1
```

doing catalogs, the wrong motor drive light is coming on sometimes.

I guess I will need to read the UTA2 again. 

Whatever drive was last selected, if I try to catalog the other one, it barfs with I/O error. Then doing it again, works. It's probably not reading anything, it does a lot of CHUGGAS to try to reset the head, and then still fails, so gets I/O error.

## Feb 18, 2025

Thunderclock Plus firmware is now loaded, and confirmed working with ProDOS 1.1.1 disk. TCP DOS utils disk also worked.

It is however acting le funky using the TCP ProDOS utils disk. Dos 3.3 one booted and worked.. like it is reading register C090 => 00 in an infinite loop, never returning.
That's not cool, bro.

something caused cpu->cycle count to reset to 0; is it my reset routine or something? That's not right. cycle count should be set to 0 only on power up.

Apparently the CFFF to disable slot card map into C800, works on read or write. So tweaked that.

Trying to troubleshoot why the TCP c800 firmware isn't coming in and out properly. Here's the deal:

* whenever CnXX is read or written to, C8xx is enabled for that slot. IF it's not already enabled?
* read or write to CFFF turns that rom off.
* in memory_read, if the page type is RAM or ROM, we're fine. I just changed it so that if the page type is IO, we call the memory_bus_read handler.
* However, this means once a page is set to I/O, we are not reading memory values from the ROM.

In memory_bus_read after checking all the various I/O addresses, I return by reading the memory mapped value, instead of returning 0xEE. So far that seems to be working okay.

Now, still have the issue where we get infinite C090 reads. ok, the demo program 'clock' is doing "-TUT" which is loading the sys program TUT. That is what is infinite looping. I think that loads a new ProDOS command called TIME. TIME %, TIME #, etc then generate some output the basic program reads in.

Clock is a cute program that displays a seconds-ticking clock with moving hour, minute, and second hand. The DOS33 version works.
I must be missing a hardware command somehow. There is a program TEST on the DOS33 disk. It says my thunderclock is not operating properly.

```
Thunderclock Plus write register C090 value 40
Thunderclock Plus write register C090 value 0
```

in VirtualII, if I do this:
```
C0f0:40
c0f0:00
c0f0
c0f0-60
c0f8 (clear interrupt)
c0f0-40
```

So on a pure read, 0x20 is the interrupt set bit.

So it's turning on interrupts. And I'm not generating one, so it is probably then turning interrupts off and saying you failed, I didn't get an interrupt.


This is what I get when I tell it to set the interrupt rate to 1/64 sec.
```
Thunderclock Plus write register C090 value 0
Thunderclock Plus write register C090 value 20
Thunderclock Plus write register C090 value 24
Thunderclock Plus write register C090 value 20
Thunderclock Plus write register C090 value 40
Thunderclock Plus write register C090 value 40
Thunderclock Plus write register C090 value 40
```

The behavior of Cn00, C800, CFFF seems that once a particular slot is latched in, you HAVE to CFFF to disconnect it, and only then can you enable a different slot. This behavior is unclear.

This is correct. The first card hit will "hold on" until CFFF is accessed. 

## Feb 19, 2025

I have the data sheet for the UPD1990AC which is the clock chip used in the Thunderclock Plus. It is not very clear, it's in Japanglish. 

I did a detour and implemented a "generic prodos clock" device. This supports only month date day of week hour minute. But it's something.

The main bit of the TCP that will be complex, is supporting its timer interrupts. There is also a "test mode" which says it increments every counter in parallel 1024 hz. like, why, what does this do. I see that other emulators also moved to support No Slot Clock, and, generic prodos clock, instead of the TCP. Though one does attempt to emulate the TCP interrupt timers.

Consider this. We run chunks of 6502 code in bursts of 17,000 cycles, 60 times per second. In order to support a timer interrupt of 1/64, 1/256, and 1/2048 second intervals, we would need to create an event queue and have the CPU loop pick those events up each iteration. Because we don't execute the code evenly (i.e., it's fullspeed during each burst) then we can't use real system timers. And that's probably okay.

So, the timer routine would "post" an event to occur based on a calculated number of cycles.

```
cpu->cycles + (cycles_per_second / 64)
cpu->cycles + (cycles_per_second / 256)
cpu->cycles + (cycles_per_second / 2048)
```

if at the top of the cpu instruction handler we have hit this timer, we call the device interrupt handler.

Let's think about this a bit too from the perspective of something like a serial card, where we may want to generate an IRQ on an incoming character. Characters do come in quite a lot faster than 1/60th second. They'll get buffered of course, but, if we have it in interrupt mode, in a separate thread, and it comes in while executing code, we want to trigger the interrupt. Even if not an interrupt, we want to set the "data ready" flag. nah, character ready check would just check a buffer. That's no biggy. It's more specifically interrupt stuff where we want to use this queue.

We will need this same stuff when we get to the IIgs emulation, since it has a variety of built-in IRQ generating devices.

So, this is a proposal for an Interrupt event queue. And it will be a lot like the audio event queue. Do we actually need a queue? What if we just emulate IRQ systems. I.e., each device or slot triggers IRQ. All the IRQs are ORd together. Interrupt handler has to search for the device that triggered it. 

We'll have some callbacks that can be registered for timers / etc. Each iteration through the CPU loop, we call the routine. The routine will set a flag if the timer IRQ should fire.

IRQ status for the devices stored in a 64 bit bitfield. We can check for -any- IRQ by checking if that value is non-zero. The position of each one bit indicates which devices' callback to call. They can be registered in an array of callback handlers (up to 64 of them).

Instead of an ordered set or something, just do this sort of optimized thing:

* bit field - each bit is assigned to a particular device.
* array of handlers, array element corresponding to the bit position.
* array of "next cycle triggers", also corresponding to the bit position.
* once a trigger is called, the bit is cleared.
* if the device wants, it sets the bit again.
* Each time a next cycle trigger is reached, the next cycle is determined and stored. As an optimization so we don't scan the list every time, we scan it only when it needs to be updated.
* Whenever a event is registered, if the cycle desired is less than current next cycle trigger, we update the trigger to that.

## Feb 22, 2025

Taking a look at OpenEmulator, another Mac A2 emu. It is Mac specific, they claim other platforms coming but never got done.

Uses 3D effects to simulate CRT monitor "barrel distortion" and such. I think it's overkill. It's fairly accurate I think. It's flash mode is quite a bit faster than mine. I may have that wrong. I can check on the IIe. I do!

Clock speed: only 1MHz and Free Run are working correctly. 2.8 and 4MHz are not working right. They are both reading at like 1.2-1.4mhz effective rate.

I had an idea about the audio/speaker handler. Whenever C030 is tweaked, have it write directly into the audio buffer. However, the problem there is that could take up a lot of real cpu time.

another idea: whenever we change clock speed, flush current audio buffer and reset parameters so the new buffer is calculated only at new clock speed.

Maybe I need to tweak the number of clock cycles we run each burst. In fact that might be the issue with 2.8MHz and 4MHz. We need to run more emulated cycles at higher clock speeds. Let's see..

In free run mode, we run 17000 cycles per burst. Then we check to execute audio, video, and events only once every 1/60th second realtime.

In 1MHz mode, we run 17000 cycles per burst. Then check. Then sleep until next burst. So yeah, we definitely need to run more cycles at higher clock speeds. OK, easily done.. That's not quite right. When I do a simple lookup table, the flash gets faster. What? Ah my lookup table is in the wrong order. still not right. Duh have to use the lookup table, not just define it. THERE WE GO.

(I consolidated all the clock mode variables into a single place).

Uh, all the sudden audio is working correctly at higher speeds. It seems the audio code was written correctly, but, I was calling it in the wrong context.

WHEE!!!!

Reading some of the OpenEmulator code. It draws pixels out purely as a bitstream, then, it uses OpenGL shaders to render the pixels in different modes (RGB, Composite). The shader is readable-ish. It uses vector processing in the GPU, I see reference to PI in there. So it's definitely implementing something like the approach I've been testing on in hgr5 - simulating a color wheel.

hgr5 and hgr6 differ in how they handle starting the scan line. On the first couple pixels, we do not have enough prior pixel data to do a proper average from. So what should we start it with?  hgr5 averages only the samples we have. hgr6 averages the samples we have with zeroes.

Once you're 3-4 pixels into the scanline, it actually seems to be working really well.

If we adopt this technique, then we could use it for every mode. And I have been digging into this because handling DHR the way I've been doing HR is going to be problematic - it will just be too complex. But DHR with this color wheel thing will be pretty straightforward. Also, text with fringing in mixed mode will work this way too. Just emit the same bit stream. And lo-res would work this way too.

## Feb 23, 2025

I just had an incident where there was a blip, and then the audio got delayed by a good 30-45 seconds or so. So there may be some edge case bug where we get out of sync due to some bad math somewhere. It's definitely much improved.

When I stuff audio data into the buffer because we're running low on samples, I fill it with 0's. That is causing clicks. I should probably decay the signal over time like I saw mentioned in some other projects. Alternatively, fill the buffer with the last value. Let's say we tail it off - multiply by 1 / (time since last event).

Trying the "repeat last sample" method. Now we are still running out of samples in certain cases. One such case is when the emulator window is obscured. like, just click to bring a browser window to completely cover it. Something will have interrupted execution long enough to bring the effective cycles per second to 800khz. It's not bringing it up, it's covering it. The Mac must be doing something weird. Maybe there is a task priority setting; I should learn more about this memory compression business.

Another event that can trigger a buffer underrun is dragging the window to another screen.

## Feb 27, 2025

been programming my brain with NTSC stuff over the last week. I am starting to get a clear picture of it. The section of UTA2 page 8-20 is making sense.

The display emulation in OpenEmulator is the best one I've found. By far the most accurate. It processes in a number of steps. First, it creates a bitmap of 1/0 signal that is delayed or undelayed in a 560x192 grid. Good, that's basically what I came up with, though it generates it a little differently than I did. But that difference doesn't matter.
Two, it then applies a number of shaders to that data. A shader is just a small function/program that runs inside a GPU. OE is using the OpenGL 3D API to run shaders. It has shaders that do all kinds of crazy stuff, like project the display onto a curved surface of a simulated CRT tube, handle brightness and color hue controls, etc. I'm not interested in going that crazy, as I think those effects interfere with the usability of the emulator. (Though, they accurately simulate how frustrating it was to use computers on cheap monitors in the 1980s!)
The basic shader in OE though compares a phase signal to the bit pattern to calculate the color of each pixels, and, also performs multiple samples of each pixel at slightly different places to simulate blur that occurs because an old CRT can't shift its beams around very fast. (It is this that causes 010101 to generate solid green for instance even though every other pixel is off, and, that shows alternating green and black stripes on screen on a high-frequency-response CRT like the AppleColor RGB on the IIgs.)
I've got a utility program that can read a hires file and output a PPM image of the display (PGM, as it's in grayscale right now). Next step is to apply the logic from the shaders and see if I can get a good color output.

## Mar 6, 2025

Finally, success!! There were a few issues with the ./comp version of the openemulator display code. First, -33 needed to be coded as +33 offset. Second, the matrices in the openemulator code were transposed (row/column) from the perspective of my matrix multiply routines, at least. Flipping that fixed a bunch of issues. Finally, the C++ code that applied the filters was not right. Claude identified it was not looking at the neighboring pixels correctly. Once fixed, I am successfully convering hi-res data files into composite-style images!

They look really good.

Next step is to clean up the code, modularize it, make it so it can be a library. Then see if there are obvious optimizations that can be made to speed it up for emulator purposes. I don't know if it will be fast enough. But we can test and give it the old college try!


## Mar 8, 2025

Added some hot keys to dump the hires and text pages to files.

Added support to hot-mount disk images into the 3.5 drives in slot 5, and display their icons.

## Mar 9, 2025

there are some double hires pictures in /asimov/images/productivity/graphics/misc/picpacdoubleres.dsk
They are, however, compressed somehow. 

Got some info on forcing a cold start reset:

$3f2/3f3 hold the warmstart vector. $3f4 is the "checksum"; if $3F3 XOR'd with $a5 is equal to $3f4, then do a warmstart.

So on ctrl-alt-F10 (reset) set $3f2-$3f4 to $00 $00 $00. 

A remaining speaker issue - certain things cause the cpu to halt, but the speaker code is still running and then I'm not sure what happens.
This occurs on invalid opcodes as well as "infinite jump" loops.

So on a halt we stop executing code. But perhaps should continue to increment the cycle counter like we expect..
on a reset, we restart the cpu but the audio code is then out of sync. ok incrementing cycle counter anyway (based on expected cycles) is working there.

Our audio beeps are very buzzy. We're not filtering the audio in any way.

## Mar 12, 2025

On the Mac, "App Nap" changes execution priority, probably triggers memory unloading / compression etc., when an app is not the foreground. There are ways to disable this - you can set a flag via finder, and you can do it programmatically. The latter Claude says requires tying in to Objective C - erk why? 

It does not seem like App Nap is the culprit. When I flagellate a Alacritty window over top, it doesn't cause the same issues as using a Cursor window.
Hinting to use opengl, cause the problem a lot less also, though opengl itself is slower than metal. Keep an eye on it.

## Mar 14, 2025

Created some abstractions for a SystemConfig. This is the selection of a platform, and a list of devices and their slots. Currently pre-define several of these, with only an Apple II+ fully speced out. Eventually, default configs for each type of system will be here, and, users will be able to clone, customize, load and save their own configs.

As part of this, the OSD now displays the name of the card in each slot that has one.

This also cleans up the init code in gs2.cpp, which now iterates through the data structure to initialize devices. Next step is to gracefully de-initialize them on exit. And then, we'll have the framework we need to support powering a system on and off, and changing config at runtime.

## Mar 15, 2025

I've never been thrilled with the prodos block implementation. It relies on "ParaVirtualization" - something performs a JSR to a particular address, then we trap that in the CPU instruction execution loop and call a handler. Currently, this address is hardcoded in that loop, so the device in question is hard-wired to a specific slot.

I also just thought about a weakness in the design of devices, how we store state information in a block identified by an ID assigned to the device. This will prevent us from having multiple instances of the same device type in different slots. Is that something people care about in an emulator? Is there anything out there that requires having two Disk II controllers / 4 drives? Old timey BBS might have had a setup like that, but in emulatorville everyone gets a free hard drive, so.. The obvious solution is to use new slot data structure to store that data. 

I am also going to make the following changes to the build system to simplify the process. We currently have system dependencies on python, and as65 from Brutal Deluxe.

* Populate system ROMs into the assets directory
* Firmware we build will be in separate projects.
  * For instance: ProDOS Block firmware; system roms download and combine.
  * These are copied into resources/roms; and resources/cards. These files then stored in repo.
  * Such firmware will be loaded like any other ROM image asset.
  * Then we can just distribute the ROMs with the main gs2 source code.

Instead of putting roms in assets, we have the roms/ directory in the project root. So this is where they go. They are copied from here into resources/roms and Resources/roms on build-package.

This will greatly simplify other people building.

What is distinction between assets and resources? Assets is predominately image data; perhaps also sound data eventually. Resources are things like ROMs, and other data that is not user-visible.

Do I need this distinction? Let's put ROMs in assets. These are "inputs". They are processed. Then copied into the Resources directory.

Resources is either: *.app/Contents/Resources/ on Mac, or ./resources/ on Linux (or Mac when run from the command line).

The system ROMs; should I bother combining them? Why not just load them individually and directly in the code? We now have the handy routine to make it easy to load ROM and other asset files.

ok the new pdblock2 device is working. This eliminates the poorly thought-out Paravirtualization stuff. Maybe in the future we can do this better - for example, this could let us PV DOS33 RWTS routines so DOS33 could use super-fast disk access.

I did not properly implement the PD_STATUS command. It wants A=0, X/Y = number of blocks on device, and CLC. 

```
LDA ERR,X
PHA
LDA STATUS1,X
PHA
LDA STATUS2,X
TAX
PLA
TAY
PLA
CMP
RTS
```

This will let us return the correct number of blocks on the device (something the old PV wasn't even doing).

Interestingly, if no disk is mounted, PD_STATUS does return an error 0x28 and trying to boot with c500g causes it to jump to BASIC. However, if it's unmounted and you do PR#5, it retries in an infinite loop to boot. Weird. Is that because it's constantly trying to output data and thus calling the boot routine over and over?

Double-check to make sure we can't mount a disk that is not 140K onto a disk ii.

## Mar 16, 2025

```
<> Media Descriptor: /Users/bazyar/src/gssquared/newdisk.nib
  Media Type: PRE-NYBBLE
  Interleave: NONE
  Block Size: 256
  Block Count: 560
  File Size: 232960
  Data Offset: 0
  Write Protected: No
  DOS 3.3 Volume: 254
Disk image is not 140K
```

The size check I added to diskII mount fails on anything except a raw 140K disk image. Media Descriptor should populate Data Size. OK that's fixed, and I fixed a bug where trying to mount a .nib wasn't setting all the right properties.

I still see the occasional issue where booting dos3.3, trying to catalog,s6,d2, drive 1 lights up instead of drive 2. Reset then do it again, and it works.

Doing a little thinking about how we might handle demos that change screen modes in tight cycle loops to create interesting (albeit useless) effects. Instead of immediately changing screen mode variables, we could post an event to a queue. Then when we render a video frame, do it scanline by scanline regardless of mode. And keep track of the events, switching modes as we go. This is a fair bit of work to get a couple of demos working that if you want you can watch in OE or another emulator. I think the question will come down to, do any actual useful programs (games, etc.) do these tricks.

## Mar 18, 2025

thinking about data flow for OSD. Storage devices should Register with the OSD. either the OSD, or the Mount Manager. Mount Manager can be a standalone thing. OSD then uses Mounts to get drive / disk status etc. Currently the disk buttons / status are manually called (by name). so use Mount as an abstraction for both. Ultimately other things in the system might want drive info.
Device init registers. Device de-init de-registers with Mounts.

## Mar 27, 2025

Going to dive into debugging ProDOS 2.4.3 hanging on boot. It's getting stuck in a loop around $D380-$D3FF. There are two loops.
The first is this:

```
 | PC: $D3A5, A: $FF, X: $60, Y: $FF, P: $A5, S: $99 || D3A5: LDA $C08C,X   [#01] <- $C0EC
 | PC: $D3A8, A: $01, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $01, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#03] <- $C0EC
 | PC: $D3A8, A: $03, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $03, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#07] <- $C0EC
 | PC: $D3A8, A: $07, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $07, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#0F] <- $C0EC
 | PC: $D3A8, A: $0F, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $0F, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#1F] <- $C0EC
 | PC: $D3A8, A: $1F, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $1F, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#3F] <- $C0EC
 | PC: $D3A8, A: $3F, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $3F, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#7F] <- $C0EC
 | PC: $D3A8, A: $7F, X: $60, Y: $FF, P: $25, S: $99 || D3A8: BPL #$FB => $D3A5 (taken)
 | PC: $D3A5, A: $7F, X: $60, Y: $FF, P: $25, S: $99 || D3A5: LDA $C08C,X   [#FF] <- $C0EC
 | PC: $D3A8, A: $FF, X: $60, Y: $FF, P: $A5, S: $99 || D3A8: BPL #$FB => $D3A5
 | PC: $D3AA, A: $FF, X: $60, Y: $FF, P: $A5, S: $99 || D3AA: CMP #$D5   M: FF  N: D5  S: 2A  Z:0 C:1 N:0 V:0
```

That is code loading a value from the disk and looking for the marker $D5.

And then later it's spinning around here:
```
 | PC: $D385, A: $01, X: $60, Y: $00, P: $24, S: $95 || D385: LDX #$11
 | PC: $D387, A: $01, X: $11, Y: $00, P: $24, S: $95 || D387: DEX
 | PC: $D388, A: $01, X: $10, Y: $00, P: $24, S: $95 || D388: BNE #$FD => $D387 (taken)
 | PC: $D387, A: $01, X: $10, Y: $00, P: $24, S: $95 || D387: DEX
 | PC: $D388, A: $01, X: $0F, Y: $00, P: $24, S: $95 || D388: BNE #$FD => $D387 (taken)
...
 | PC: $D387, A: $01, X: $01, Y: $00, P: $24, S: $95 || D387: DEX
 | PC: $D388, A: $01, X: $00, Y: $00, P: $26, S: $95 || D388: BNE #$FD => $D387
 | PC: $D38A, A: $01, X: $00, Y: $00, P: $26, S: $95 || D38A: INC $D36F $D36F   [#01]
 | PC: $D38D, A: $01, X: $00, Y: $00, P: $24, S: $95 || D38D: BNE #$03 => $D392 (taken)
 | PC: $D392, A: $01, X: $00, Y: $00, P: $24, S: $95 || D392: SEC
 | PC: $D393, A: $01, X: $00, Y: $00, P: $25, S: $95 || D393: SBC #$01   M: 01  N: 01  S: 00  Z:1 C:1 N:0 V:0
 | PC: $D395, A: $00, X: $00, Y: $00, P: $27, S: $95 || D395: BNE #$EE => $D385
 | PC: $D397, A: $00, X: $00, Y: $00, P: $27, S: $95 || D397: RTS [#D170] <- S[0x01 96]$D171
```

it's updating a variable at $D36F. there are some other vars here too. 

Short version, it's scanning track data and not getting what it is expecting.
ok the track is starting at 0 and it's hammering it CHUGGA style. About 35 times as we'd expect. This is the C600 boot loader here.
we get to real track 5 (done loading prodos) and then it jumps into D1xx. 
Then it's jumping back and forth between track 1 halftracks 0 and 1.

So is it doing that to try to rehome on the track, or, are my disk2 register emulation not working as expected?

It's cycling doing :
* turn off all phases 
* ph1 on
* ph2 off
* ph0 on
* ph1 off
* turn off all phases

repeat.

OpenEmulator won't boot my image.nib. 

looking at my output .nib, first difference is AppleSauce is using Volume 001, whereas I'm using volume 254.
Second thing, my sector order/numbers are wrong. 

It was track 0 sector 1 as 03 00 05 00 ...
I have it as all 0's.
My sector that has 03 00 05 00 is marked as Sector E.

So I am generating the sectors incorrectly. The boot code is working ok for whatever reason, but when it switches to the ProDOS 2.4.3 code it barfs.

First thing to try, is use Volume 001 like a ProDOS disk should.

If Interleave = ProDOS, use volume 1.
Oh, nibblizer is not using the media descriptor thing and is manually generating DOS33 interleave and volume 254. Ergh.
ok, nibblizer is fixed. It converts a .po image to a .nib and this boots in OE. Also boots in Virtual2.
The volume deal did not fix ProDOS 243.

I did confirm with nibblizer that we are generating the right interleave etc. So it must think it's on the wrong track. Or it is on the wrong track. The phase cycling above is the indication.

## Mar 28, 2025

ok, I got some source snippets for these routines. John mentioned a couple possibilities. First, if we don't get an address field in 2000 nybbles then we generate a RDERR - this is a branch to $D3FB. I am not executing that.

The code being hit is the "fast seek routine".

It's the "fast seek" routine that keeps cycling over and over. It is trying to seek back to track 0, starting at halftrack A. However, it gets to halftrack 1 and then sticks there.
CURTRK starts at 0, and TRKN is also 0.

```
 | PC: $D122, A: $00, X: $0C, Y: $FF, P: $26, S: $99 || D122: JSR $D133 [#D124] -> S[0x01 98]$D133
 | PC: $D133, A: $00, X: $0C, Y: $FF, P: $26, S: $97 || D133: STA $D372   [#00] <- $D372
 | PC: $D136, A: $00, X: $0C, Y: $FF, P: $26, S: $97 || D136: CMP $D35A   [#0A] <- $D35A   M: 00  N: 0A  S: F6  Z:0 C:0 N:1 V:0
```
Current track is A. Desired track is 0. We go from this to:
```
 | PC: $D122, A: $00, X: $0C, Y: $FF, P: $26, S: $99 || D122: JSR $D133 [#D124] -> S[0x01 98]$D133
 | PC: $D133, A: $00, X: $0C, Y: $FF, P: $26, S: $97 || D133: STA $D372   [#00] <- $D372
 | PC: $D136, A: $00, X: $0C, Y: $FF, P: $26, S: $97 || D136: CMP $D35A   [#02] <- $D35A   M: 00  N: 02  S: FE  Z:0 C:0 N:1 V:0
```
track 5 to track 1?
How did D35A change to that in one call to JSR SEEK?
ok look at this...

```
 | PC: $0975, A: $65, X: $65, Y: $20, P: $20, S: $A1 || 0975: LDA $C080,XPH: slot 6, drive 0, phase 2, onoff 1
new (internal track): 10, realtrack 5, halftrack 0
 | PC: $D190, A: $63, X: $63, Y: $00, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 1, onoff 1
new (internal track): 11, realtrack 5, halftrack 1
 | PC: $D190, A: $61, X: $61, Y: $01, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 0, onoff 1
new (internal track): 10, realtrack 5, halftrack 0
```

The first step we are going in the wrong direction a half track. Then when we end all these seeks we end up at track 1 (02) instead of 0.
I wonder if it's got code somewhere I'm not seeing that reads the track number from the disk to update CURTRK. Yes, at D171 it takes the read TRACK number and calls CLRPHASE.

Is it possible that from the first to 2nd step we're not decremending the track number when we should? A half track down from track 5/0 is track 4/1, not 5/1. No, we count in half-tracks. 

```
 | PC: $D190, A: $61, X: $61, Y: $FF, P: $24, S: $96 || D190: LDA $C080,XPH: slot 6, drive 0, phase 0, onoff 1
 | PC: $D190, A: $66, X: $66, Y: $03, P: $24, S: $96 || D190: LDA $C080,XPH: slot 6, drive 0, phase 3, onoff 0
 | PC: $D190, A: $64, X: $64, Y: $02, P: $24, S: $96 || D190: LDA $C080,XPH: slot 6, drive 0, phase 2, onoff 0
 | PC: $D190, A: $62, X: $62, Y: $01, P: $24, S: $96 || D190: LDA $C080,XPH: slot 6, drive 0, phase 1, onoff 0
 | PC: $D190, A: $60, X: $60, Y: $00, P: $24, S: $96 || D190: LDA $C080,XPH: slot 6, drive 0, phase 0, onoff 0
 | PC: $D190, A: $66, X: $66, Y: $03, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 3, onoff 0
 | PC: $D190, A: $64, X: $64, Y: $02, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 2, onoff 0
 | PC: $D190, A: $62, X: $62, Y: $01, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 1, onoff 0
 | PC: $D190, A: $60, X: $60, Y: $00, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 0, onoff 0
 | PC: $D190, A: $63, X: $63, Y: $00, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 1, onoff 1
new (internal track): 11, realtrack 5, halftrack 1
 | PC: $D190, A: $64, X: $64, Y: $00, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 2, onoff 0
 | PC: $D190, A: $61, X: $61, Y: $01, P: $24, S: $95 || D190: LDA $C080,XPH: slot 6, drive 0, phase 0, onoff 1
new (internal track): 10, realtrack 5, halftrack 0
```

when we turn phase 1 on on that 3rd from last line, the code says:

```
case DiskII_Ph1_On:
            if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 1, 1);
            if (last_phase_on ==2) {
                seldrive.track--;
            } else if (last_phase_on == 0) {
                seldrive.track++;
            }
            seldrive.phase1 = 1;
            seldrive.last_phase_on = 1;
            break;
```
the last phase on was 0 (first line) so we increment the track - which is probably wrong.

UTA2 - "Even numbered tracks are phase-0 aligned, and odd-numbered tracks are phase-2 aligned".
This means that if we're on track 5, we're aligned with phase 2, which means turning on phase 1 will DECREMENT the track.
ok so instead of using "last phase" . last phase is not indicative necessarily. because we might be *changing direction* of movement.

Dude, that is working.

So, I see one of the "acceleration" things people talk about is probably setting all the prodos timer values to low numbers for track seeking, the phase on-off tables at $D373. We could detect when ProDOS is loaded and running and then set those variables to all be 1 or something, to minimize the amount of waiting around for a virtual disk head to move.

## Mar 29, 2025

I have two major pieces to do at the Apple II Plus stage:
* Writing to floppy images.
* Videx VideoTerm 80-column support.

I might want to take a stab at returning reasonable value for "floating bus reads". Doesn't seem to be preventing anything from working right now though.

See DiskII.md for notes on writing to floppy images.

Working on hgrdecode for dhgr. I have the pixels correct. However, the colors are wrong. Green becomes blue. orange becomes green. purple becomes red/yellow. blue becomes purple.
So the phase is just off by 90 degrees ish. Are we off by one bit position? It's 90 degrees off. that is one bit. If I insert an extra blank bit at the start of each scanline then the colors are correct. But that's not right... In the diagram in UTA2E it shows the Auxiliary byte coming in before the hgr bytes in the other examples for single hires? I don't understand.. One fix is to simply change the phase. But something is off here. It could well be the sample files? I need to do some paid work now!
There is something fonky in the OE where it's blowing off the last bit of pixel data?

this is at the end of drawHires80:
  if (x==39) {
      blank80Segment(p+CELL_WIDTH);
  }
"blank an 8 pixel segment".

ok, this is because the whole display is shifted left by one byte position (one 80 column char, or one dhgr byte) when in 80-column mode. It does start a byte early, so the phase is going to be shifted here because it starts early.

Something's not right here. And maybe it's the core - remember how I shifted negative colorburst to positive colorburst? Maybe that was covering up some other problem, and now I'm wrong by about 90 degrees.

Double hi-res uses four bits per pixel. 16 colors. How does it "get started" - i.e., does it start with 3 blank bits and 1 data bit? Or does it clock in data bits first before displaying a color? Maybe that's the pre-shift I see in the diagram in UTA2E.

Nick Westgate: "IIRC, all double resolution modes start one byte before all normal resolution modes."
So that explains the stuff to the left of the diagram on page 10 of UTA2E.

Also, UTA2E says the video ROM lookup inverts all the bits going out to video in hires modes. WHAT?!

Maybe it's not +1.. but -7?  (or -6).

Each byte in the diagram is a 90 degree shift? 

So tweak phase to start at 0.25 (90 degrees). problem solved.

## Mar 30, 2025

tweak to hgrdecode to make it start with phase difference of 90 degrees for doublehires.

Implemented write-protect detection on image files in Disk II. Still need to implement that in pdblock2. A nice workflow. If a file is erroneously WPd, you can unmount, change the FS bit, then re-mount. Voila!

Need to change the OSD so when I click on a mounted disk, it unmounts it. Then a separate click, to mount anew.

## Mar 31, 2025

Instead of setting a flag to raise the window, let's use an event queue structure. I was thinking we'd use this to manage the disk drive sound sample stuff.

Let's make a little test program to play sound samples. That was easy, sheesh.

To simulate disk sounds: the motor on sound is easy, just keep playing the same audio data in. Head movement: it seems like we want to play the whole track sound, but, if we determine the head has -stopped- moving, flush it at that point. Have to come up with some appropriate hueristic for "stopped moving" (for example, hasn't moved in 1/60th second?) The bump "stop" sound should be a "one and done", we play it, then forget it. 

Looking at the existing speaker code with this example code in consideration, I now see that I don't HAVE TO send it constant data including empty frames. I -can- just feed it data when I have it. It will fill with silence otherwise. And, if I decay my values (going from + or - to 0) in a reasonable time, then there won't be clicks when that occurs. Something to think about..

I should also move the general audio subsystem init into its own file, util/sound.cpp. Then the various things that need to emit sound will reference that. When we get to the GS, we'll need lots of AudioStreams (one for each "voice" of the Ensoniq chip).

## April 1, 2025

We need to somehow detect multi-track head movements. One track at a time sounds right. But when the head swoops across a big area we need to play successive chunks of the sound, not simply repeat the first bit of the sound for each track moved.

I think we need more border. I would like to draw disk drives in the margin when they're running, so more border will help. Draw them with maybe 50% opacity. And draw using a Container. Instantiate new Disk II / Unidisk buttons. And the container needs to be dynamically managed to contain only actively spinning media. Have the opacity fade in and fade out.

Trying to format a 5.25 with ProDOS 1.1.1 filer, I get the error "Disk II drive too slow". That is special.
DOS33 is doing an INIT. I guess we'll see what it looks like in AppleSauce!
Well it looks ok to me. 

The DiskII "wrong drive thing"... this is what we do.

MOT: slot 6, drive 0, motor 1
DS:slot 6, drive 0, drive_select 1
PH: slot 6, drive 1, phase 1, onoff 1

turns on the motor, then does drive select 1. apparently, this is supposed to switch the motor to drive 1.

FIXED!

Well, well. You can write to the drive select and drive on/off registers, and, locksmith does that. Locksmith is now validating disks.

A format isn't working right. And maybe my DOS3.3 format didn't actually work right, but are they verified when you do an INIT? Unsure. This is likely going to be caused by not fake rotating the fake disk under the fake head even when we're not doing anything. I could try reenabling that code...

OK, still have problems initializing disks. I'm only modifying track position on reads. That's an issue. So need to bring that code out to the top of the disk II registers routine. So any time we hit a register we're updating.

Call me crazy, the dos33 system master boot seems faster.. well it's not working right.

I bet on inits, this thing is trying to write 10-bit FFs. and here in a sector header we have..

FF FF FF FF D5 AA 96 FF FE AA AA AF AF FA FB DE AA EB E7 FF FF FF FF FF D5 AA AD 96 96....

DE AA EB E7 FF ?

That E7 is supposed to be an FF.

here's another broken one:
FF96FF
that's after a sector, it ought to be FF.

ok, will have to study the cycle timing on writes. 

## Apr 2, 2025

Writing a self-sync 

Proposal: read or write a bit at a time.
on read: set "bitsleft" to 10 if FF. otherwise to 8. So we actually feed emulated DOS 2 zero bits on a read.
on write: we're not going to store the 0 bits, so just set it to 8.

I have made the following tweaks: increment the disk 'head' position by one BEFORE a write. An init is working and can save the file, however, I am getting extraneous 96 after the newly written data fields. Before, I was getting the last byte of the address field chopped off - that was creating I/O errors.

(I may have to go ahead and implement this sequencer state machine.)

ProDOS still not working, filer reports "disk ii drive too slow".

So, what if that is ProDOS complaining that there are too many bytes in Gap A? This whole 0x1A00 thing is a suggestion, not a command.. let's measure how many bytes we are actually generating from our conversion.
yes. So comparing to an AppleSauce conversion to nibblized, we have 2100 more bits, 360 more nibbles, and duration of 208200us versus 199997us. 300rpm is 5 rotations per second is 200,000us. So, we need to actually mark how much data we write, and then use THAT as, um, track_data_size.
I am generating a track.size. So let's use it..
ProDOS still no likey, however, disk stuff ought to be up to 5% faster since we cut 300 bytes out of 6000. DOS read, write, init is working ok still.

Comment from Nick Westgate:
ProDOS will have something similar to what I mentioned in a previous comment: [For DOS] you need to provide at least one different nibble in every 16 nibbles to defeat the SAMESLOT drive speed check in DOS/RWTS at BD34-BD53 (according to Beneath Apple DOS). ProDOS is different though IIRC.

that's not it.

However, I am starting to think ProDOS is doing something not mentioned in manuals. 

## Apr 4, 2025

Thanks to a tip from Nelson, forcing a disk image track to be fewer bytes (reducing GAP A when generating nibblized version for instance) makes ProDOS Format work.
But now DOS 3.3 format does not. I reduced from 149 to 64 bytes. Let's increase a bit..

Final update: Many thanks to Nelson Waissman who put me on the right ... track ... ha ha pun intended. The number of nibbles in the virtual disk track determine the "speed" the disk is spinning. 
If too short, DOS3.3 won't format. (not enough room for how DOS33 init's a track!)
If too long, ProDOS thinks the disk is too slow.
His number of 0x18D0 is exactly right.

Locksmith now: Verify 16 sector ok; Format 16 sector ok; it cannot bit-copy. It keeps complaining about "FRAME BITS 02, LONGSS #FB02". run locksmith disk speed thing. Still reports the disk is really really slow.
ProDOS now: format volume ok and can copy files to it;
DOS3.3 now: init volume ok and can write files to it.
DOS33 and ProDOS 2.4.3 format, disk copy, etc with CopyIIPlus- works.
Copy II Plus drive speed test - doesn't work.

Proposed task list for next release (0.20)
* finish up Disk II write by denibblizing (if needed) and writing disk II data back to disk file.
* clean up video code so we call old monochrome hires code in that mode. [ complete ]
* Center display in window when in fullscreen mode. [ complete ]
* Redo joystick code so it maps correctly when it comes online after emu start. (sometimes starts with Y reversed!, or not right joystick at all)
* Decay the audio so we don't get so many annoying clicks due to OS interrupting my event loops [ complete ]

That ought to be easy!

## Apr 5, 2025

building on windows - Win doesn't like strndup because it's a POSIX function. Find a standard c++ library alternative.
and really do some cleanup, use C++ variants of std library stuff instead of a mishmash of c and c++.

First pass, not awful, though having some issues when linking against SDL. Some weird Windowsism.

on the speaker decay. When we hit an event, set the amplitude to 0x6000. Each sample after that, decay the amplitude 0x0100. (that's about 90 samples, can adjust this). If we fill empty frame, continue using decaying amplitude. Sample value at any point is amplitude * polarity. I currently have it to decay at 0x0300. That doesn't seem to hurt frequency response at all. I also reduced the max amplitude to 0x5000 from 0x6000. 

There is a bug occurring where the display stops updating right when I move the window between screens, sometimes. oh weird. It's when the left edge of the window is near the left edge of the screen?? only my right screen. And, if the left edge is within the first inch or so. What the. Special.

hm, see what events we might be getting when we're close to the window edge.

Time to make the denibblizer!

To be fair, I have denibblizer examples in both the DOS3.3 source code, and, the Disk II boot firmware. How hard could it be?

The basic process for what was block data file:

1. For each track:
   1. Mark 16 "sector found" flags all false.
   2. Start at position 0 in the track. As we scan, we might have to wrap back to the start of track.
   3. Scan until we get to a D5 AA 96 whatever the sector address field marker is.
   4. Extract the sector info.
   5. Scan until we get to a D5 AA AD?
   6. Read 342 nibbles into denibblizer buffer
   7. Decode buffer and validate checksum
   8. Copy decoded buffer into track block buffer with offset based on track X + sector Y * 256. Will have to take the interleave into account.
   9. Set "sector found" flag for that sector.
   10. If all sectors were found, Write track out to image file 
2. Repeat for all tracks.

### For the user-interface:

Position an overlay container over OSD:
   * Media FILENAME in SLOT X DRIVE Y was modified! Write changes back to file?
   * YES | DISCARD | CANCEL

Perhaps this will be a Dialog, not a Container?

For this we will draw everything, then draw this on top (it will always be after all other containers), and direct input events only to this container. (it is a modal dialog after all). 

[x] strndup is a posix function. Replace with a C++ convention. (replaced with our own version for windows)

## Apr 6, 2025

OK the denibblizer is done! A lot of my trouble was being tired and mixing up what files were going where so all my testing was bogus. Don't TARRED AND CODE!

Denibblizing in the standalone tool, we need to know what interleave to use on output. We don't have it from raw nibble form. So the user needs to specify it on the command line. either --prodos or --dos33. (-p or -d).

Inside the emulator, we will track the interleave 

Either way it should get passed into the denibblizer routine as a parameter. pass the table, or a flag? 

Audio issue - I still get clicks in the first few minutes after boot when opening/closing the file dialog and causing underruns. So we must still be generating non-zero fills somewhere.

in the OSD code that mounts a disk, it is passing in the char * filename. I bet this is getting deallocated. We better strndup it.

Some additional thoughts:
don't allow unmounting if modified is true and the motor is on? or just punt that and give them UI choice anyway?

OK, the diskII write code is DONE! Works for DOS33, and ProDOS. Writes back to original block format, or, writes back to nibblized file, depending on origin. (i.e., back to original format). There is no UI yet - if you unmount a modified disk, the image WILL BE saved back out if you unmount.

If you don't want to save changes that were written to an image, for now, don't unmount it, just F12 / close the emu which will exit without saving.

## Apr 7, 2025

Added a couple more disk II sound effects (for open door close door). The Unidisk makes all sorts of sounds too. I will have to record those myself! Shouldn't be too hard. But not my priority right now.

I want to clean up and reorganize the display code.

strndup must go. I can make my own implementation and avoid having to refactor all the string code to use std::string.

Or, just bite the bullet and do the right thing. I mean, I really ought to. FINE.

## Apr 8, 2025

So for the modal dialog, stick it smack in the middle of the OSD. Make it 200 x 100 pixels for now. We'll need a modal flag - if set, it is a pointer to the modal container. Updates are requested of that container AFTER everything else is drawn, and, events are exclusively sent to that container instead of the regular container list. Then we can define any number of modals to use for different things. (For example, selecting what card goes into a slot, choosing CPU type, etc.)

DiskIISaveContainer - 
   Save
   Save As
   Discard
   Cancel

The Modal Container needs to somehow return the selection when done.

Also create a FadeOutStatus concept. This is a similar thing. It is a container button that displays for N frames, then decays to nothing over the next D frames. I need the ability to draw a button (text or image) with a forced opacity. I can do it with text. not sure about images. So, if we hit F9 to change speed for instance, the new speed will display at the bottom of the screen for a while. Maybe towards the left edge, to leave room for drives. And to leave room for another status display on the right side, to show effective CPU speed, cycles, etc. whatever we want over there.

OK, I do want to create a sub-type of Container, that is ModalContainer. It will accept another Constructor argument, the Msg Text. This is displayed without being a button. It will also lay out its Buttons a certain way (Centered, in one line.). 

ok, that's sort of done.

So when we click to unmount a disk, we are in the middle of an OSD button callback. Since this Modal Dialog needs to display and receive events as part of our main event loop, the button callback can no longer be the code that calls unmount, plays the sound, etc. We need to trigger an event that will cause an appropriate piece of code to be executed from the main event loop. We need an EventQueue.

The EventQueue is a simple ordered (FIFO) queue. we push an event onto it. We push:

EventQueue
    Event
        uint64_t timestamp
        event_id enum (one of defined set of events)
        event_callback ( function to call with event )
        event_data ( additional data to pass with event )

We can have a variety of Event subclasses, one for each type of event. (This is how we should have done the cpu module data stuff, and, still can, if we REFACTOR!)

So here's the overall flow.
OSD is up.
Disk button is clicked.
Disk button issues DiskIIClickEvent, with data of 'key'. (i.e., ID of disk drive).

1. ModalShowEvent first shows the modal (some of its data specifies which modal).
1. All the buttons in the ModalContainer have (same) callback that will issue a ModalClickEvent with the button ID.
1. ModalClickEvent sends the click info to the Event handler of the current modal (in this case, ModalClickEvent).
1. It then takes appropriate action to make the modal go away.

So the Modal has two event handler routines? an SDL_event handler and a gs2 Event handler. (Container only has the one type of event handler).

Other Event types:
* Play standalone Sound Effect

SDL allows user-defined events. However, that would require use of void * and other horsepuckey. Implementing one ourselves is no biggie. So let's do it.

Or is this overwrought? I could just store the key in the DiskIIModal and let it handle state itself.
OTOH, the EventQueue idea lets us queue up several events. Say, on a Quit - we have multiple modified disks, we need to ask about each one. We can queue the events all at once. We could also pass the Events to a separate thread to handle stuff that might take a while to execute.

Lot of flexibility for what is a small amount of work. Go for it!

Does it go into UI? Or into util? Base class goes into Util. UI-related child classes go into UI.

on a modal button click, get the button ID from the button itself.

OK this is working well!

Looking at the goof going on with the game controller stuff. After ludicrous speed, it just stops working. This may be a SDL thing (polling the joysticks too frequently?)
There is another issue, the IIe reference manual says "Addressing $C07X" resets all four timers. This implies reads and writes. Also, it's clear that it is C07X. So all of those should do the same strobe reset.

## Apr 9, 2025

tweaking layout of the modal - (auto) centering stuff. centering text in buttons. That also affected the slot buttons, but, those can be their own type later, or, can specify a style element for text alignment.

Current Apple II+ todo list:

* Video Videoterm
* finish refactoring video code so we have three modes: composite, RGB, monochrome.

Mono: the raw bits placed into the 560x192 video buffer, without any color processing.
'RGB': the first-generation color stuff.
Composite: the next-generation color stuff.

First step is to implement Composite for lo-res, and then also text.

So let's do it!

ok, I now have lores wired into Composite (NG), and, mono Lo-res as well! I see that the lo-res colors are WAY different between the Comp and RGB code. That's what you get for trusting the internet!

So from a user interface perspective, I think there are a number of dimensions.

Color Engine: Composite or RGB
Color Mode: Color or Mono
Pixel Mode: Pixel-Perfect or Fuzz
Mono Color: Green, Amber, White

I was previously thinking like:

Color, Green, Amber, White  |  Comp , RGB  | Fuzz, Square

What if we want White mono as well?

So the difference here is really just, four controls or three. With mono, there are no artifacts. So it really is a distinct mode from Comp and RGB?

It doesn't make sense to have a Comp Amber or an RGB amber. They're the same.

So:

Comp, RGB, Mono | Green, Amber, White | Fuzz, Square

These various combos of the four rendering variables. Refactoring the mode definitions and variables now.

Experimenting with a different aspect ratio - this multiplies the vertical by another 1.22 (making the initial ratio 2 / 4.9). This makes the Sather book square example actually square.

#define SCALE_X 2
#define SCALE_Y 4.9

I got so used to the current way that I'm not sure how I feel about this. Everything works just fine with it, though, it is quite large. I might have to shrink it a bit, i.e. go with 1.5 x something? Seems to look ok with the fuzzy scaling. top and bottom borders would need to be adjusted.

## Apr 10, 2025

Been thinking about creating new blank disk images. It would be easy enough to have a button in the control panel to create a new blank disk image. Save As.. to save to a file. Then you can mount straight away! Basically we'll just have some blank disk images in resources/ and copy them as needed.

Next step is to do text via the same routines! This will involve refactoring the text font drawing stuff - but not much. let's see how I prepare the font.. it creates a map of 32-bit pixels. We don't need that. We would actually just need 8-bit pixels, same as ever, either 0x00 or 0xFF to feed into the new graphics routines.

text rendering then will have two modes: color and mono:
when in RGB display mode, text is drawn in mono white.
When in composite mode, it's drawn as if it was graphics and run through the LUT.
When in mono mode, it's drawn in mono white, green, or amber accordingly.

## Apr 11, 2025

current render_line conditionals are a mess with lots of repeated bits. Let's take a stab at fixing..

I think at a high level, we should do this.

The pixel mode (linear or etc) is independent.

```
if color_engine = NTSC
   LORES: lores_ng
   HIRES: hires_ng
   TEXT: text_ng
   (all) renderflag = NTSC
else if color_engine = RGB
   LORES: lores-rgb. renderflag = RGB
   HIRES: hires-rgb. renderflag = RGB
   TEXT: text_ng. renderflag = NTSC

if (renderflag = RGB) we're done, return.

if (mono || text-mode-only)
   ng_mono with selected mono color.
if (color)
   ng_color_LUT.

return.
```

Well, I can't put it off any longer. I am at the point where I include a Video VideoTerm, or punt on it for now. Let's release current code as Version 0.2.0, and then include Videx into a 0.3.

I'm monkeying with a Windows build again. 

1. go ahead and accept the replacement strndup in a header (strndup.h)
2. add #include sdl3/sdl_main to gs2.cpp.
3. soundeffects.cpp - there is an unneeded include sdl_main here. eliminate it.

Then, rip out the old prodos_block code, and move its firmware construction to the src/gs2_firmware build tree, and clean that bit out. I think that's confusing cmake on windows later. Also, it won't work because I don't have the assembler on there.
gs

## Apr 12, 2025

When disk drive is running (e.g., when in boot mode with no floppy in drive), and in free-run mode, effective CPU rate drops by 40% to 50%. (Issue #25).

It's not the OSD, I commented that out. So it must just be the disk code itself. It's probably this: the disk read routines are in an extremely tight loop just hammering on the read register. That is executing the C0XX read routine quite a lot.
commenting-out the debugs added about 10MHz effective.
This isn't a general problem with the disk code. If I cut out the disk handling altogether by putting "return 0xEE" at the top of the handler, it makes no difference!
Sitting in the keyboard loop we get 425MHz. So, maybe it's the thing that dispatches the slot I/O locations generally?
Should do some general-purpose profiling..
if I read C0e8 in a super-tight loop, I have nothing. So what else is the disk ii boot code doing? well, it -is- running in c600.

Running the Disk II boot code at $1600 (1600 < C600.C6FFM ) speed slows down to only 350MHz.

Tests:
* sitting idle in keyboard loop: 410 - 420
* Booting via C600: 270
* Boot code copied into RAM at 1600: 350

So first off, the the thing that handles I/O memory areas is on the slow side.
Second, something in the 6502 emu in the disk loop is slow - maybe it's indirect X. It's likely sitting in this bit waiting for a D5 that will never come:

```
LDA $C08C,X
BPL $165E
EOR #$D5
```

I turned incr_cycles into a macro. Speed went from 270 to 315MHz during boot. 597MHz when keyboard idle. (Whoa. That was a huge improvement.) That was simply getting rid of a subroutine call.

I should add time instrumentation to the cpu loop too. btw each cpu loop iteration is a subroutine call. There are some other questionable bits of code in the cpu here:

```
uint8_t read_byte_from_pc
  uint8_t read_byte(cpu_state *cpu, uint16_t address)
    uint8_t value = read_memory(cpu, address);
```

none of these are inlines. I should try that.. made no difference. The compiler must have already been optimizing them out.

[ ] bug: when you run the apple ii graphics demo (bob bishop image waterfall thing) in ludicrous speed, way too many disk sounds get queued up. WAY. they keep playing forever. Maybe clean the soundeffect queues on a ctrl-reset? Or, disable disk soundeffects in ludicrous speed?

## Apr 13, 2025

Thinking about keyboard shortcuts. I really like Control-F10 (or whatever similar) for control-reset. However, by default on a MacBook, the touchbar shows media keys, brightness, etc. You can configure it to default to F1-F12 and hold Fn to get media functions. So, workable. But it might be cool to set special touchbar keys for GS2. For instance, we could have RESET, keys for the different display modes, etc. I have a bit of sample code in the repo now that purports to program/modify the touch bar. Haven't tested it yet, but will do shortly.

* clang++ -x objective-c++ -std=c++17 -framework Cocoa -framework AppKit src/platform-specific/macos/TouchBarHandler.mm -o TouchBarHandler.o -c

now that doesn't help my windows laptop. It doesn't have that. but it does have prt sc, home, end, insert, delete where the function keys ought to be. So, could use ctrl-del for instance for ctrl-reset. what about ctrl-alt-delete? hehe. yeah that doesn't work. but could do control-insert. or home. Ahh. Fn-ESC will "lock" the Fn keys. 

Alternatively, have controls at the top of the window. Hover over the top 15-20 scanlines to access those controls (only when mouse isn't captured). You'd have reset, hard reset, crt modes, etc. I'd still have all my keyboard shortcuts, but, then you'd have this control strip.

There are adb to usb converter widgets. People could just use their actual Apple IIgs keyboard! ha!

I could replace a good part of the current OSD stuff with the HUD type stuff.

So let's do the MHz stuff at the bottom like I was talking about. ok. Coo.

how about a debug window?
* on a keypress, open/close the debug window.
* debug window:
   * shows last N disassembled instructions and register status, and memory in/out. (basically the opcode debug).
   * don't use printf. construct these in a character buffer as we go, fixed positions. buffer in cpu struct so various things in cpu all have easy access to it.
   * define macros to set various bits of information as we go.
   * update display each frame like always.
   * have pause, step functions, and a one-instruction-per-second mode. (that's setting clock speed to 1 Hz)
   * allow setting debug flag register via cli (-d XXXX).
   * two areas: disasm output, other debug output.
* at end of instruction, optionally emit the status line to stdout. (if we do, trim whitespace from end of line before output)

So, this is hundreds of thousands of instructions per second, even at 1mhz mode. One million line buffer is maybe 5 seconds worth, and will take 100MB of ram. whoo. Have a key to stop trace and dump to disk. Circular buffer until key hit. I did like the thing in Steve that showed the DISASM there, and if there was a tight loop it would stay in that tight loop.

## Apr 14, 2025

I can no longer avoid the Videx. So: Videx in our same main window, or, Videx in a separate window? Let's do it in the main window for the moment, and auto-switch between it and hgr as necessary. Things I'll need:

* Videx character set (openemulator has these)
* Videx rom (I have this already, also OE has it)

Let's see how OE handles it.. pr#3 turns it on. pr#0 does not turn it off (reset does). I'm not sure there's much software that is going to work with this.. some word processors. AppleWorks if you hack it.

Is there anything left I need to do for the II+? I'm not sure there is. Maybe move along to the unenhanced IIe?

or, do a fun little thing, implement a 320x200 VGA card. (16 color fits in 32K, and 256 color fits in 64k). Could do a bank switching thing, or have it work like the slinky card. Or something more like the Second Sight, with a coprocessor on it. Could implement all sorts of drawing primitives. Support mixed text/vga mode. Would need some software to go along with it. What would be a cool demo? Do something that could actually be implemented on a real card. So there are existing cards. What about the A2DVI ? It's in a position to do what we need.. 

## Apr 15, 2025

All Videx characater rom files are 2KB. each matrix in the rom is 8 pixels by 16 pixels. 

so the Videx rom "ultra.bin" contains the regular char set in the 1st half, and 7x12 character set in the 2nd half of the ROM. (2K per half). The characters are actually 8 x 12. The first column of each character is usually blank, to provide spacing between chars, unless it's a graphics character.

So, 80x18 screen this way is: 640 x 216 pixels. 24 x 9 (normal) is also 640 x 216 pixels. The ultraterm has a 132 column mode, but the VideoTerm does not. honestly, that might be really cool later on..

Have the basic model in there. however when I pr#3 C800 gets overwritten with 0x20. It's trying to clear the screen, but writing to the wrong memory. I was pointing CC and CD to the rom memory. ok, now I pr#3 and shit happens!

Control-G gives the modifed Videx beep. Hey, this is progress! So the firmware is running. And I'm not overwriting it, ha ha.

This is coming together pretty fast. there were a few minor issues, having the hi/lo of the start of frame reversed was a big one. not looking up the characters in the right place another. All minor stuff. I am successfully viewing 80-col text through the Videx firmware/hardware!

things to do:
* we are doing an awkward scale. it's not super-awful. But, it would be much better if we weren't scaling 216->192 and 640->560. oh yeah, that's WAY better. well I only get 7 1/2 of the last 10 characters. oopsie. I will have to resize the window when we're in this mode. (I went ahead and scaled this to fit inside the normal display area of the window. Probably how it would have looked on an Apple II? I don't have a real one to try.)
* needs a cursor. Shouldn't be hard, just need to apply the inversion as specified by the cursor control registers. (done)
* 18-line mode. with the much nicer tall 9x12 characters. (videx 2.4 stopped supporting this. I wonder why? Anyway, drop it) (done)

I might be doing something wrong on the drawing yet. Compare to openemulator.

Well let's think about this. I will need 640 pixels wide when apple iigs is in super hires modes. So I will need to resize window for that too. And SHR cannot mix-mode text.
ok so I want to resize it to borders (maybe bit smaller borders?) plus 640 plus 216.
I want to keep the same scaling factors I have now, 2:4 basically. 

amping up the brightness of the result by using ADD blend mode and doing the texture render twice, brings the brightness up to a good level. very readable. ADD - increase brightness. Multiply - increase contrast.

Things to do:

* optimize by only updating lines that need it (done)
* have update_display call a different render_line just for this purpose (done)
* the cursor on/off stuff needs to be pulled up into update_display. update_display is called only once each 60th second. (done)
* consider whether we should have a update_display hook. We will need the same thing for Apple IIgs super hi-res modes. Maybe Apple III. i.e., this would replace the usual update_display with alternative update_display. Having the update routine for Videx in the Videx file would be a big style improvement. (done)

This would be for things that completely replace the normal update_display logic (VideoTerm, )

VideoTerm needs a line-at-a-time optimization. And support to redraw the line the cursor is on. But that is based on:

* video memory update (done)
* cursor blink status update (done)
* screen memory start position update (done)
* alternate character set. not a register. must be a bit flag in video mem. (yes, high bit.) (done)
* blink rate is not working correctly. supposed to be bit 5. (done)

* shift-key mod

tested: blink / non-blink. cursor start and cursor end.

alt: control-Z 2 standard; control-Z 3 alternate. Switches to garbage. Ah ha. High-bit set is another 2048 bytes of video data. So, allocate 4K for font, load the ROMs, but copy the ROM data into the char set map.

Working on optimizing the display updates. I need to know when we write to pages CC and CD, whenever slot 3 I/O memory is switched in. 
We also have a dirty hack in other code that checks for accesses to mobo video memory to flag those lines as dirty. So, go ahead and hack this, we need to fix it up later.

ok, good enough to ship the code to repo.

## Apr 23, 2025

Mockingboard is coming together!

I think the envelope is phasing in a little slowly though. You can hear it on certain notes in the music box dancer song. 
Alternate doesn't seem to work. wait, it is working with continue and attack=n and hold=y.

OK, that has been fixed. The envelopes all work correctly according to the data sheet; that's better than Virtual II, whose emulation here does not seem very good.

I should be testing on AppleWin, too..

## Apr 24, 2025

some improvements to the audio generation, primarily due to filtering on each channel before mixing, based on the selected frequency. but apparently I broke noise generation in the last couple commits. oops. There's the IF statement that checks, I did change it from volume to checking a bunch of flags, that's almost certainly where the noise has gone. ok, fixed!!

Still need to fix the volume / compression. As voices come and go there are pretty disjointed discontinuities in overall volume. I like the idea of doing dynamic gain control based on recent max samples. But we don't want to lose dynamic range. So maybe the closer we get to 1.0, the more we should compress, and do it more the closer we get. That's where that tanh thing comes in, I think, but that really harshed the sound.

Also, need to implement the logarithmic volume. Linear just isn't right.

Maybe the thing to do here is deal with stereo. That will reduce the number of channels we're mixing. Chip 0 is left, Chip 1 is right. We can create a SDL stereo audio stream, and pass in two. We mix each chip separately, then chuck each chip's mixed audio into its own SDL audio channel.

So in this order:
* do logarithmic volume
* verify what volume my float samples should be (i.e. scale gain appropriately - we are WAY louder than e.g. virtual 2)
* split the mix into two stereo channels
* tweak the mixing algorithm from there.

Also, create something to log and record the chip events so we can play them back from saved files later with a CLI util. The CLI util should allow playing a saved clip, and, just generating a WAV file.

It may be the case that they just had to be careful not to overload the output channel by playing 3 tones at the same time at loud volume? Who the heck knows how the OS mixes all this stuff, or the SDL layer.

There is maybe another option here, and that is to use some separate signal generator. Would SDL or the OS have a waveform generator? Or use a SIN wave generator? (would have less harmonic stuff to deal with but would probably lose its 80s 8-bit character.) 

btw the Mockingboard manual RTF I have confirms the registers are 0 through 13, with 14 and 15 being unused or "not significant". Contradicts the data sheet. But, it's right.

Each 6522 has two timers, T1 and T2. when they count down to zero, they trigger an interrupt. the GS is going to have stuff like this too, and, in order to be accurate, it will need to count down while we execute CPU cycles and trigger the IRQ while we're in the CPU loop.

Since these are simple counters, each time we tick a cpu cycle we just need to tick these counters as well. So we need to 'register' a clock whenever ..
alternatively, we could register an event to occur. e.g., we know when the next zero crossing will occur. there is an ordered queue of items to call whenever the cpu ticks over to that cycle. If the user reads the current counter, we can just calculate from the last zero crossing. This way we're not calling functions every clock cycle. We're essentially doing it by math any given cycle we only need to check one uint64 to see if we 'got there'. this will work sorta like we track events in speaker.cpp; except we need to maintain an ordered queue. (speaker.cpp events are always ordered, it's a FIFO).

if envelope is 0 and tone amplitude is zero, we're still playing a note. that's wrong.. oh except tone amplitude in this program means "use the envelope". So, the envelope is still playing even if env_period=0.

Logarithmic volume lookup table: done.
I am using a gain of 0.6, built into the lookup table.
split into stereo channels - done. And it causes less weirdness with the simple "divisor mix" in the output, since at any given time there's a max of 3 channels being mixed instead of six.
Also applying the gain of 0.6 seems to help overall results.
Should do much more 

Now I will need to analyze the sample generation window stuff. Let this just sit here over lunch and see if we get out of sync. though I think I generated based on cpu->cycles / nominal cycles per second. in the event we have clock slips, though, this probably gets out of sync. Also, we may be generating too many samples.
One thing we could do, is if no sound sources are active, don't send samples. then the next time we send samples, we will automatically be in sync.

the end of NY, NY I think is just a buggy demo. everything else is working so well..

yes, it got out of sync just sitting there. There was a delay before sounds started emitting, due to backlog (too much buffered data).

There is apparently some technique for auto-detecting a mockingboard, it's in SkyFox. Some other software lets you select mockingboard version and slot. Will have to look into that and make sure we react appropriately. that's what they get for not having a ROM! (having no luck determining what this detection routine is. Will have to boot skyfox and see what registers it hits, and where.)

[x] need to hook ctrl-reset to Mockingboard reset.

Add some debug diagnostics to see if we can figure out how/where Bank Street Music Writer is trying to detect the MB.

## Apr 26, 2025

mb / 6522 interrupts! I implemented an IRQ handler in the cpu. Now, to implement the counter mechanism in the 6522. This is the interrupt 

This is what the demo disk "interrupt-driven music" does.

mb_write_Cx00: 0b 40 ; auxiliary control register - 0x40 bit 6 means "continuous interrupts". (0 in bit 6 means one-shot interrupt)
mb_write_Cx00: 0e c0 ; interrupt enable register - bit 7 = set flag; bit 6 = set 'timer 1 interrupt enabled'
mb_write_Cx00: 04 ff ; W - T1 low-order latch ; R - T1 low-order counter
mb_write_Cx00: 05 69 ; R/W  - T1 high-order counter

That's 0x69FF - 27135. About 37 times per second. or, 2,220 per minute.

Whenever the low-order is written, it first goes into a latch. Then it's transferred into the counter whenever the high-order is written, so that the lo and hi are always put into the counter at exactly the same time.
The high order also goes into a latch.
Whenever the counter ticks down past 0, it reloads the counter from the latches automatically.
When latch is transferred into counter, the interrupt flag is cleared, and the timer begins countdown.

The cool thing here is this thing runs like clockwork; no matter how long our code might take, this counter will fire again exactly on interval. Saves a huge amount of effort trying to count cycles.

Page 2-42 of the 6522 data sheet goes into detail on the difference between REG6/7 and REG4/5. basically whether T1 interrupt flag is also set/reset.

So why would they enable shift register? Hm. Disregard for now.

So - write to 4, just store lo-order in latch. Then write to 5, trigger the other stuff.

So let's create event timer. ok, that's in and working. It does seriously slow things down however! Instead of maintaining an ordered tree (an expensive data structure on the heap) - at any given time there won't be -that- many of these things. Keep them in a fixed-size array. Whenever we insert one, we do this: we find an empty slot in that array and write the new record to it; and keep track of the cycle count of the next event (if new_event < next_event then next_event = new_event>). Then we can just scan the array when an event occurs. OR we can just cache the next cycle time and keep the current object and structure.

Or, maybe, we just fetch the next event time when we enter the cpu loop (or the index of the next event). that won't work, because it might change inside the loop. Yes, update the cache value whenever we modify the queue.

interrupt routines are working in the cpu - however the song is powering through at warp speed. I suspect the IRQ is not getting cleared, which will cause the code to immediately loop straight back in to the IRQ handler. In the morning, make sure I put in code to DE-assert IRQ in appropriate cases. (I am certain I am not doing this at all right now).

## Apr 27, 2025

So we have this current concept of getting and storing device state information based on a device type. The problem is, we may want multiple instances of the same type of device. For instance, two Disk II cards, or, two Mockingboards.

So, we need different routines for storing non-slot device info (for which there can only be one instance), but also slot-device info (for which we may have multiple instances.) 

Sweet! the interrupt-driven music demo now works, however, it doesn't shut off at the end, so I am missing something that is supposed to shut them off.. last thing they hit was this:

mb_write_Cx00: 04 1 0b 00   // 
mb_write_Cx00: 04 1 0e 7f   // disables all interrupts

0b is ACR - that sets bits 7, 6, 5 to "one shot interrupt".

Also still need to fix up reading the current counter values.

only reschedule an interrupt when Timer 1 interrupt enable is set.

[x] Refactor all the slot cards to use the Slot State concept instead of Device State.

Once a T1 counter hits 0, it either stops, or, restarts depending on setting in ACR.

What is the default (reset state) value of these registers?
Note that a load to a low order T1 counter is effectively a load to a low order T1 latch

If we're in once-decrement mode (ACR6 is 0) then if we are reading at a time beyond cpu->trigger then the counter read should be zero.
If ACR6 is 1, then we read a continuously cycling interval (i.e. the modulus).

Reset clears all internal registers except T1 and T2 counters and latches and the SR. T1 and T2 and the interrupt logic are disabled from operation. (This is while Reset is being held). 
let's say they're zero and ACR6 is 0 - then the counter will not count down or generate interrupts.
So if ACR6=0 and counter=0 return 0 on a read.
every time we trigger and interrupt and re-load latches, should we 
the WDC doc contradicts the Rockwell doc. Reg 7 write in WDC says IFR6 reset. Rockwell does not mention it. A 2004 WDC doc also says:
IFR6 is reset on write to 4, 5, 7, but not 6.
on a reset we need to propagate irq.
claude claims the counters are initialized to 0xFFFF on a power-on. That would make sense. but is it true?
chatgpt is saying they are undefined. but that's referring to output of pins while in reset state I think.
interrupt flags in IFR are set when they would be; interrupt disabled just means they won't propagate.
there's only one way to know for sure; check another emu ( ha ha ). apple2ts implements mockingboard too. on power up, it has latches at 0 and the counter is counting. That makes sense, that this is what bank street writer is checking for. it also lets you select sound fonts.
oh, funny, that's exactly what I'm doing right now. BSMW still claims no mockingboard. hmm.
T2 operates as a one-shot counter only, but otherwise similar to T1.

I broke the mockingboard demo where it plays a launch sound after the mockingboard. i.e. I broke the interrupts.

## Apr 28, 2025

Fixed the broken interrupts. it was in a couple places. Also, got music working in game play stage in ultima IV.

ok, this is the Skyfox MB detect code:

```
 | PC: $6856, A: $00, X: $04, Y: $04, P: $20, S: $A5 || 6856: LDY #$04
 | PC: $6858, A: $00, X: $04, Y: $04, P: $20, S: $A5 || 6858: JSR $686C [#685A] -> S[0x01 A4]$686C
 | PC: $686C, A: $00, X: $04, Y: $04, P: $20, S: $A3 || 686C: LDA ($70),Y  -> $C404.   mb_read_Cx00: 04
irq_to_slot: 4 0
  [#52] <- $C404
 | PC: $686E, A: $52, X: $04, Y: $04, P: $20, S: $A3 || 686E: CMP $70 -> #$00   M: 52  N: 00  S: 52  Z:0 C:1 N:0 V:0   ; 3 cycles
 | PC: $6870, A: $52, X: $04, Y: $04, P: $21, S: $A3 || 6870: SBC ($70),Y  -> $C404.   mb_read_Cx00: 04 ; 5+ cycles. Assume it's 5 since no wrap.
irq_to_slot: 4 0
  [#5A] <- $C404   M: 52  N: 5A  S: F8  Z:0 C:0 N:1 V:0
 | PC: $6872, A: $F8, X: $04, Y: $04, P: $A0, S: $A3 || 6872: CMP #$08   M: F8  N: 08  S: F0  Z:0 C:1 N:1 V:0
 | PC: $6874, A: $F8, X: $04, Y: $04, P: $A1, S: $A3 || 6874: RTS [#685A] <- S[0x01 A4]$685B
 | PC: $685B, A: $F8, X: $04, Y: $04, P: $A1, S: $A5 || 685B: BNE #$07 => $6864 (taken)
```

So it puts $C100 into $70 and $71, then indexes indirect by Y (which contains 4). So it reads each slot $CS04. Then it burns some cycles. Then it subtracts from C404 and if the difference is not 8, then it assumes we're not a mockingboard.

Issue is we're reading the same value twice. no we weren't. I wasn't printing the value! Derp.  ok, now I'm printing the value - determined I was counting up, not down (of course I was). now that I'm counting in reverse, skyfox detects the card; gets the first notes out; then hangs in an infinite interrupt loop. After it gets the first notes out, it is writing zeroes to all C400 to C4FF in reverse. What? Why? That's when we get the interrupt hang. Probably when we zero out the counter/latches for no good reason. hah.

Another note: initially, skyfox is only writing the low counter, 0xFF. that means interrupts every 255 cycles. Is it assuming the high register is something other than 0? Is it thinking there are many chips on here? I don't understand. of course we ignore this? yes.

what does mockingboard2.dsk do btw?loads a bunch of stuff.. then writes to $C443, $C440, $C443?? Whaa? then never gets an interrupt. That must be related to the speech chip. yes:
https://git.applefritter.com/Apple-2-Tools/4cade/commit/3f0a2d86799d72f5109951854825264b4daf40a5

```
         lda   #$80                  ; ctl=1
@mb_smc2
         sta   $c443
         lda   #$c0
         sta   $c443                 ; C = SSI reg 3, S/S I or A = 6522#1 ddr a
         lda   #$c0                  ; duration=11 phoneme=000000 (pause)
```
yah. So I guess that would be the final bit to do.

## Apr 29, 2025

Something is different between platforms - on linux, the Mockingboard1 demo disk blows chunks when it gets to the interrupt handling bits. Non-interrupt driven seems to work just fine.

btw here is some subtle 6522 voodoo:

http://forum.6502.org/viewtopic.php?f=4&t=2901

apparently after a 0, it takes a cycle for the latch to get reloaded. So that's why they load with value-1 in some places.

the interrupt is never clearing when we're in debug build. Also, coredump on linux. what the what

Whoa what's this? scheduleEvent: 13744632839234567870
Then we never get another schedule event and the interrupts never go away. Weird. ok.
That value is 0xBEBEBEBE (64 bits). I needed to initialize some registers, derpy derp.

So still not working on linux. what else is uninitialized. Ultima turns interrupts on before doing writing the counters. So, t1_triggered_cycles is zero. the IRQ never clears..

ok, I have added checks so we never use a 0 value for t1_triggered_cycles, nor do we ever use a zero value for t1_counter or latch. In the event of a 0, assume 65536 cycles. Made sure this change was done throughout the code, and that resolved the issues with Ultima IV. Let's try Skyfox again.. big fat nope! This stuff sure is twitchy.

Run another set of test across these games..

Rescue Raiders hangs after displaying Terrorists have been found at Cherbourg. I am wondering if that's a copy protection thing, because this is a .nib disk image. I can disable the mockingboard and try it again.. nope, with MB disabled it gets past that screen.

oh, there's the noise register, I'm not even sure what that is. I'm not checking it anywhere. That is an omission.

## May 1, 2025

big RGB discussion in DisplayNG.

## May 2, 2025

I just patched up my "RGB" mode instead of writing a whole new RGB mode. One thing to note: if we have blue, then black, then blue again, I cut off the first blue too quickly. I do everything that way.. it makes for crisp single-pixel lines. It's not necessarily bad.. though noticable in Taxman where the top left corner of boxes the lines don't quite meet the same way. It's definitely the trailing pixel.. I'm gonna call it good for now.

Things for a "real" II+ release. Whoa, is that happening?

This release goals (0.3):
* [done] refactor all the other slot cards (like mb) to use the slot_store instead of device_store.
* [x] implement reset routine registry
* [x] implement accelerated floppy mode
* vector the RGB stuff as discussed in DisplayNG correctly.
* make OSD fully match DisplayNG.
* refactor the hinky code we have in bus for handling mockingboard, I/O space memory switching, etc.
* Fix the joystick.
* implement floating-bus read based on calculated video scan position.

Release 0.35:
* Bring in a decent readable font for the OSD elements

Next release goals (0.4):

* GS2 starts in powered-off mode.
* can power on and power off.
* can edit slots / hw config when powered off.
* "powered off" means everything is shut down and deallocated, only running event loop and OSD.
* when we go to power off (from inside OSD), check to see if disks need writing, and throw up appropriate dialogs.
* put "modified" indicator of some kind on the disk icons.
* Implement general filter on speaker.cpp.

Then, we'll be in a position to start working on the IIe, which will be (0.5)!

We can legit only have one Videx in a system. Doesn't make sense to have multiple. Change its config setup to be slot-based, but,
[ ] somewhere we need to have a flag that we can only have one in a system.

I have the Videx building. HOWEVER: I hard-coded references to Slot 3 all over the place. So that needs to be fixed tomorrow.

## May 3, 2025

First bit of code is the annunciator hooks. These should go in their own module (i.e. global) that videx can read.

So I think the Videx is now slot-independent (i.e., *I* don't hard code slot 3 anywhere) except the Videx firmware itself assumes slot 3, I think.

I wonder if I should also check into the ntsc lookup table code and instrument to see how fast it is. yeah, it's taking a half second to initialize. That doesn't seem right?

Mike's hgrd lookup table calculator does this:
for each of 4 phases; it iterates through all 1 << 17 (131072) possible bit patterns that are 17 bits long.
then it runs processAppleIIScanline_init on each one of those to get an output pixel, and store it for the LUT.
For each of those iterations, it's creating two vector objects, doing a bunch of sin/cos, etc. Maybe this is a good thing to test profiling on. ok, yeah.

## May 4, 2025

Integrated in the new optimized video LUT generation routines, and am testing with 

Interestingly, there is a minor "fade-in" effect at the left and right edges, where even with a white field, there are artifacts as we come onto white. Not sure if that's supposed to happen - not sure if it happened before. I don't think it did. Checked it out - in fact, they were there before. it might be a hair more pronounced on 7/45 than 8/50. But the images are -extremely- similar.

[x]: don't forget that the mockingboard still gets out of time sync slowly.

735 is sometimes not enough samples and causes crackling.
736 consistently is too many - it's 3,600 extra samples per minute.
perhaps I can read the number of samples in queue, and insert a number of samples appropriate to keep the queue length within a range of say 3,000 to 4,000.
if it's more than 4,000, do 734.
if it's less than 3,000, do 736.
in that range, do 735.

Well, ok, that is keeping it in range. it remains to be seen if that causes weird artifacts. Well, let's test that1

## May 5, 2025

I think I may have stabilized MB in ultima - sometimes it was getting in a loop where interrupts were never turning off. I fixed a bug - write to T1C-H is supposed to clear interrupt. I had that wrong - I had it on read T1c-H.

looking at skyfox again - it is crashing with PC=0000, which is a brk. and it's infinitely looping there because interrupts are enabled. So need to enable the full opcode trace to see what's happening.

I am not 100% certain, but I think my having all the interrupt-generating stuff and other MB stuff hard-coded to 1.0205mhz means mockingboard music that's interrupt-driven plays at a normal 1mhz rate. i.e. it's as if the mockingboard is running with a 1mhz clock even if the cpu is clocked faster. Stuff that is managed by the CPU tho gets buffered. So perhaps we should change the 10205 everywhere except the interrupt timer. Or, maybe that would mean scaling the interrupt timer based on the clock speed.. ok this isn't strictly true. if you run fast for a while the MB music is nowhere to be found..

Disk II reset caused a segfault on linux? Oh, I may still have a problem there.. not that, it's the parallel card trying to fclose a null pointer. fixed.

## May 6, 2025

on a lark I added "auto ludicrous speed when any disk II is on". it does indeed accelerate disk II accesses. they're instant. it screws up mockingboard output, which doesn't work after any period of ludicrous speed. But everything else is ace , ha ha. The audio generation window must get ahead of realtime, and can't come back. cycles gets ahead of the real time timer.

[x] needs to disable ludicrous speed --when disk is scheduled for turnoff, not when it's actually turned off--  (moot, removed this experimental feature)

Also, you can't ctrl-reset to stop things from booting because they --boot too fast--.

XPS Diagnostic IIe disk has a disk drive speed tester thing on it. It says I'm at 266 rpm, instead of 300. Be nice to investigate that code and see what it's doing.

made a lot of improvements to the joystick/game controller code. Still to do:

[x] support a 2nd joystick connecting  
[x] see about scaling when we're doing diagnoals. Karateka I can't make the guy run by pushing to upper-right, even though I'm supposed to be able to.  

## May 7, 2025

the Atari Joyport / Sirius Joyport is a device that maps an atari joystick (essentially a number of buttons) to the 3 button inputs on an apple ii. combined with the annunciators you can read two axes on two joysticks and 3 buttons.

## May 8, 2025

I have a thing here to refactor so the video subsystem initialization and state is kept in a separate area. Right now it's part of the "mb display". However, since we now have two and ultimately even more display subsystems, all the SDL variables (renderer, window, etc.) ought to live outside the display code, in their own CPU struct.

ok the first pass at video refactor is done. However, I think I need to bring the texture back into the apple ii display mode - each display renderer subsystem should have its own texture. ok that's done.

And, the video_system_t should be a class, with constructor, destructor, and more importantly, methods to render a full frame, managing scaling and the bodrer area appropriately. Did it! Also brought in the toggle_fullscreen concept.

That's a fair bit cleaner.

## May 9, 2025

Star Wars II is cool, but the paddle axes are exactly backwards from what makes sense on a joystick. Ugh. So we need a "reverse paddle/joystick axes" button.

## May 10, 2025

did some thinking about trace architecture. See the Tracing file.

Doing some work on the speaker. It's still buzzy. I don't like that. OE's speaker beep is very clear, and has relatively high frequency pitch.
I put a filter on the output, and it doesn't affect the buzziness, but it does dampen the high frequencies making it sound muffled.

test-first - starting point

test-2nd: set decay rate to 0x20 (was 0x200)

test-3rd: set filter alpha from 0.6 to 0.3
this produces a much smoother incline - HOWEVER.
the start of a transition is very sharp. The end of a transition is very smooth. in OE, the output is nearly linear.
changing alpha to 0.1 makes the waveforms very nearly a sawtooth or triangle wave. The frequencies cut off around 9khz. BUT there were still frequency spikes at higher frequencies! Hmmm.


oh that's interesting. OE is using 48K samples, we're using 44.1K samples. 

*** So, we can have a fractional cycle contribution per sample.

oh holy shit. I set the sample rate to 102000, samples per frame 1700. This creates an even divisor and the output is PERFECT.

SO. The issue all along has been, we are introducing noise probably for several reasons. But the big one was, we were not accounting for fractional cycles in sample calculation. I suppose after the loop is over I could do a one-time check for 

lots of testing. That was DEFINITELY the issue.

Timelord has a high pitched whine on GS2, OE, and a fuzzy whine on Virtual II. Ugh. Guess what? On the real thing, it doesn't.

So, have to figure out a good filter system. And, figure out how to handle the fractional cycle contribution. BUT IT WORKS.

I believe SDL3 is properly doing resampling as needed in software, even with weird non-standard sample rate etc. No need to complexify my algorithm. However, we might benefit from a low-pass filter of the input square samples before we cram it into SDL. And, might experiment a bit with the parameters. (Yes, I did a low-pass on the individual cycle samples as we accumulate in the contribution, and a second low pass after downsampling.)

I wonder what would happen if I passed in 1020500 samples per second to SDL and forewent all my contribution stuff.. there's only one way to find out!

On Mini, audio_time is now 40-50uS. On PC, it's 130uS. So this is quite a bit slower than before.

So, the audio routines are currently taking between 40 and 50 microseconds. This is pretty good, but I will miss when they didn't sound right but only took 6 microseconds. Ten times faster but garbage, ha ha. So, there are these following potential optimizations:

* switch to fixed-point math?
* maybe we can SIMD the filters by filtering the whole input and output frame all at once, instead of inline in the code.

## May 12, 2025

"For read operations, it's usually OK to read from a wrong address first, then reread from the correct address afterwards. You definitely don't want to write to a wrong address though, so on STA nnnn,X, the write is always on the 5th cycle (when the address is fully calculated), and the 4th cycle (where the address is sometimes wrong) is just a read that gets thrown away."

this is 6502 voodoo! if it's a RAM read nobody cares, but, if it's a I/O read it could impact a state machine. For instance, 

https://stackoverflow.com/questions/78183925/need-clarification-on-the-dummy-read-in-absolute-x-indexed
https://retrocomputing.stackexchange.com/questions/14801/6502-false-reads-and-apple-ii-i-o
https://groups.google.com/g/comp.sys.apple2.programmer/c/qk7MZPRgVXI

This may NOT be present in 65c02, but MAY be present again in 65c816.

audit.dsk fails test 0007 due to this: I am not doing the false reads. I guess I will have to look into that! That could be breaking a few other things..

```
	clc					; Read $C083, $C083 (read/write RAM bank 2)
	ldx #0					; Uses "6502 false read"
	inc $C083,x				;
	jsr .test				;
	!byte $23, $34, $11, $23, $34		;
						;
```

Audio: Changing everything to double and making process() an inline cut audio frame processing time by 50%.

## May 14, 2025

implemented SDL_DelayPrecise instead of busy-waiting, which apparently is working quite well. It "sneaks" up on the desired time delay. That is basically the algorithm I was contemplating recently. Works great! Only 4% cpu use!

## May 15, 2025

Btw testing x86 code on Apple Silicon: $ arch -x86_64 ./yourapp

## May 16, 2025

system tracing! Step 1 is refactor the 'debug' logging stuff to populate the trace record instead.

## May 17, 2025

debugger dna has been laid. I've implemented Tracing (per Tracing.md). A couple different interfaces. First, trace logs into a circular trace buffer, which stores for each instruction, the value of all registers at the start of the instruction, then the instruction opcodes, then the instruction disassembly, followed by the effective address and memory value read or written. the effective address varies based on the instruction; for memory movement it's the actual memory location referenced, which is handy to explain indirect and indexed address modes (don't have to figure out the effective address manually.) As with everything else in the system, 

I also implemented "pause execution". this sets the halt flag in the cpu. however, the way the code is right now, the clock keeps ticking along. This has the interesting effect that the audio routines will keep working. (Well, particularly, the mockingboard will). however, that also means that there will be big cycle discontinuities after you pause, or especially when you're single-stepping.

Even after fixing so we don't keep incrementing cycles, the mockingboard stays synced when step tracing. ha ha ha. It's miraculous!

Let's not call it halt. let's call the flag: execution mode. Execution mode will have the following values: normal; step-into; step-over. Then tie in some keyboard and buttons 

if debugger is open
   * go into single step mode on a BRK. otherwise handle BRK normally. can use brk for manual debugging checkpoints. otherwise, it means a crash, and you'll be able to see the backtrace.
   * when we write the trace record to the buffer, we can check the PC, the EFF address, against the breakpoint list.
   * handle up and down arrows; pgup and pgdn; home and end; mouse wheel up and down. These will scroll through the instruction buffer. Provide some visual indicator of where we are on the side of the window.

For progress bar, we can have it down the right edge -
   * draw rectangle
   * draw portion above current location in blue
   * draw portion below current location in green

## May 18, 2025

We can likely prune down the image formats SDL_image handles. We really don't need all those. Shrink the code!
SDL_ttf isn't building. Not sure why.. oh, got it done ultimately by simplifying some cruft in the cmakelists.

## May 21, 2025

sdl_ttf complained when I built into a target directory. I got past that. however, I keep running into other issues, so I have been hammering away at the CMakeLists.

then I couldn't build SDL_ttf due to harfbuzz.cc failing compile for extremely weird reasons.

SDL_ttf says it loaded a newer vendored version of harfbuzz yesterday. That almost certainly has to be the issue. Man that was a waste of two hours. I disabled HARFBUZZ to at least get my builds working again.

## May 22, 2025

ok, got building working. I think the error was specifying /Library causing cmake to mix the command-line and the xcode libraries. 

Notes from the 10X people on SDL group:
Use the MACOSX_BUNDLE target property to tell CMake it should be a Mac app bundle
See this: https://github.com/Ravbug/sdl3-sample/blob/main/CMakeLists.txt#L165-L179
You can also use the RESOURCE target property to tell CMake to add resource files to the bundle in the right subdirectory
And if you have dynamic libraries, cmake can fix the rpaths for you as well: https://github.com/Ravbug/sdl3-sample/blob/main/CMakeLists.txt#L187-L202

```
set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch. 
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ravbug.sdl3-sample"
	XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries 
# or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS ${EXECUTABLE_NAME}
    	BUNDLE DESTINATION ./install COMPONENT Runtime
   	    RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
	
    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    INSTALL(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        " 
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
```

## May 24, 2025

I have been in build-system hell for the past 3 days. But learned a LOT about cmake. Had some folks jump in and start contributing! Awesome.

Other than that, have just tweaked a few things.

[x] Still need to get Windows running with the new build system.  


## May 25, 2025

Thinking about handling multiple CPUs. There is a decision point here: other emulators allow running multiple emulated systems at the same time - perhaps as threads. Threading presents issues with SDL (lots of stuff has to run in main thread). Also, that's not my interest - the user can always start multiple instances of the emulator, and that's easier to conceptualize.

My original thought was to have a potential multi-core '816, for multitasking. Of course no software except GNO/ME (after heavy revisions) is going to know what to do with that.

But let's think through what that implies. You have multiple CPU cores. The cores however share: memory, peripherals, display, etc. So, even a multi-core system would share a display screen, an OSD, a system configuration, etc.

So practically instead of "CPUs[x]" being the top-level concept, we need a "system" or "motherboard" as the top-level (a global) that contains:
   cpus[x]; slots[]; device state; etc.
Practically, to support multi-core, there are two options: time-slice the cores inside the main event loop; or, have each 'core' have its own system thread. Geez, imagine a 4-core '816 running on the native system cores. That would be CRAY CRAY. The multi-threading approach - each thread(core) would need shared memory for the virtual RAM; how would they access hardware? How do we serialize h/w access? normally, the emulated OS would mediate multiple process access to h/w. That's not possible here. And I don't think it's possible in the emulator layer to do it.

Let's say we wanted to emulate the Z80 softcard. That is a different type of CPU, but it's still two CPU "cores" in the same system. It would fit this model. (i.e. we'd execute it with time-slices like I am considering doing here with multiple '816 cores).

So what we can do:
* multiple emulated systems, each with their own hardware, display windows, time-sliced; (run each instance as a whole new process);
* multiple CPU "cores" but the entire thing is time-sliced inside a single process/thread (or, a 6502 and a Z80 in the same process, time-sliced);

so let's consider a computer struct.

videosystem; // the higher level video system stuff for this window.

struct computer_t {
  systemconfig; // the "system configuration"
  cpu_state *cpu; // cpu state for this "computer". in theory could have an array for cpu cores.
  mmu_t *mmu; // mmu, and, memory, I/O decoding, etc lies behind this
  module_store; slot_store; // 
  OSD; // OSD for this "computer" instance 

  void reset(); // calls reset on registered peripherals, devices, and the cpu.
}

computer_t computers[MAX_COMPUTERS];

Ah ha, let's think about this now - SDL has CategoryProcess. What if for each virtual system we launched off a whole new OS-level *process*, not a thread. We could just fork off a new process from scratch. 

If we do it this way, we end up with the following things:

computer_t computer;

only ONE of those.

Maybe things will become clearer if I start to refactor cpu_struct into an object. There are a couple low-hanging fruit I can do - init_cpus, and cpu_reset. Let's dip toe in water. OK, that's done. Didn't break anything.

ok, I pulled a few things into cpu_state, like reset, init, etc.

Now, let's pull videosystem out of CPU and into computer. This is likely to be somewhat involved. Yes, it needs access to a bunch of memory stuff. And of course, is interacted with via the bus/memory code. So, perhaps we need to first look at that..

Currently memory.cpp/bus.cpp are a conglomeration and mishmash of stuff relating to memory (ram and rom), I/O, and routines that CPU uses to simulate a memory read or write (incl. incr_cycles). This needs to be split up into an MMU concept:

```
MMU -> MMU_IIPLUS
         -> MMU_IIE
         -> MMU_IIC
         -> MMU_IIGS
    -> MMU_III
```
etc. MMU is basic routines for managing a memory map and performing ram/rom read/write. The sub-classes add memory map handling for I/O, bank-switching, etc. Maybe MMU_IIE and MMU_GS are subclasses of the IIPLUS class, which defines critical Apple II concepts like the slot-related C8XX memory, the C0XX softswitches, the CNXX slot memory, etc. I think the MMU should also have to handle more than 64K of memory.

```
ADDRESS SPACE
   PAGE
      0400 - 07FF : monitor and flag video update when modified ("shadow")
      0800 - 0BFF : can check and only do something if that video mode is active
      2000 - 3FFF : no need to calculate hires stuff all over the place if hires mode isn't active
      4000 - 5FFF : 

      C0XX        : table of handlers for these
      C1XX - C7XX : individual page handlers for these
      C800 - CFFF : check page handlers, plus, CFFF access to "reset"

      CC00 - CDFF : Videx 512 byte memory window, should set to RAM
```
if page type RAM, allow r/w. If page type ROM, allow read only. If page type I/O, call a handler.
Handlers should all set and get a pointer for their callback, to a data object that holds all the state they need.

In *addition* to the read and write pointer (language card has different read/write pointers for the same addresses), we need a shadow pointer.
Shadow pointer is "tell me about writes".

for an 8 bit apple the memory map is 256 pages. For a GS, it's 65536 pages. And very few of the GS pages need to point to anything except ROM or RAM. So I wonder about having two layers of memory map, because this is just a LOT OF ENTRIES. In a GS, Banks E0 and E1 are basically just an Apple II's RAM. So, it really does seem like we'd want a IIGS MMU that contained IIGS memory mappings with large pages (maybe even no pages, that). So that's what we'll do - we'll have a IIGS mmu that handles IIGS fast ram and rom; handles shadowing in banks 00/01 -> E0/E1. The IIGS MMU will break memory up into 64KB chunks, and that will be the "page" size. OR, for my GNO/ME ambitions, break it up into 4K page sizes. that's 4096 pages for 16M address space. That's reasonable. And then anything in E0/E1 (and maybe 00/01 if shadowing enabled) we then make a call down into the IIE MMU that we allocated internally, and THAT MMU is set up for 256 byte pages.

So what are the key elements here? RAM, ROM, IO. RAM - option for shadowing writes to other RAM. Plus, I/O needs to be able to direct reads and writes to routines.

For later GS/OS / gno virtual memory, the virtual memory page tables should be in main RAM, and are a layer -on top of- the physical memory map.

Was just thinking about GS handling of speed shifting where sometimes specific cycles operate at 1MHz and some at 2.8MHz. Instead of tracking execution time per frame using just cycles, perhaps we do it by counting nanoseconds. e.g. 1MHz ~ 1000ns, and 2.8MHz ~ 350ns. The MMU has some control over the CPU clocking. It knows which cycles should be 1MHz and which 2.8MHz (or faster).

I modified the TRACE() macro as follows: removed the conditional inside it. Put the conditional in the beginning and ending blocks only.
If you define TRACE(SETTER) (to turn it off at compile time), you get about 650MHz sitting at basic prompt.
if you re-enable trace at compile, BUT disable trace at runtime, you can get around 580-600MHz. This is because we removed a -lot- of conditionals from all over the CPU execute_next function. While we do write some values to memory (e.g. the operand, memory read/write value, effective address), it's apparently a lot faster to do that than to do conditionals everywhere. Makes sense. So if you want to go super hyper mega mega fast, open the debugger and disable tracing. Seems like maybe we could default to tracing off when we 'boot'.

I have been hammering away at an MMU class. Two classes. One is barebones MMU, fairly generic, knows nothing about Apple II address space. A subclass of that is MMU_IIPlus, which does know about the Apple II address space more specifically, and handles: C000-C0FF softswitches, C100-C7FF slot card memory map, and C800-CFFF slot card additional memory map, and CFFF "disable slot card memory in C800-CFFF". That's all it knows. It acts as a broker for all the other functionality: RAM and ROM reading/writing; calling registered functions to handle C000-C0FF. 

The MMU classes implement a number of new concepts.

* Memory Shadowing - register a function that is called on write in any given page. This is -in addition- to for instance writing the value into RAM. This will be used to handle writes to the video pages to trigger video updates. This concept will also be used in the IIGS later with its bank0/1 shadowing to E0/E1.
* read_p and write_p. with the Language Card, we can have reads and writes going to different chunks of memory. So for each page, reads and writes can be set separately. And, this will allow us to handle the Videx combo of RAM and ROM in its C800 space w/o special IF statements anywhere.
* read_h and write_h. These are -handlers-, i.e., function pointers; what we do today on a more limited scale with register_C0XX_read_handler etc., is now applicable also to pages. This will be used to handle the weird Mockingboard C400-C4FF which is all softswitches and not memory at all.
* We no longer rely on the value of type (ROM, RAM, IO) as this is not dispositive. It's used now just for diagnostic output.
* There are useful dump methods for diagnostics.

This is performance testing done with the "mmutest" app test harness.
```
Time taken: 1214654625 ns
total bytes read/written: 983040000
ns per byte read/written avg: 1.235611
maximum simulated clock rate w/memory access every cycle: 809.316467 MHz
```

If we did nothing but NOP, one cycle is a memory read, the other cycle is nothing. So the maximum possible speed of that is 1.6GHz. However, of course, we do a ton of other processing around every instruction. So, uh, no. But, this should be no slower than the current routines and probably faster as I made some decoding optimizations that reduce the number of conditionals. I could make the same optimizations in the current code, but, that is limited utility if I'm going to replace that code shortly anyway.

Now here's a fun thing - in theory, we could write special read_word routines that could then probably read twice as fast.

Analyzing the Steve2 code. It uses global variables for the Apple2_64K_RAM, and the aux ram. It's saying that this can save on pointer lookups. I guess that makes sense.
Another optimization is *potentially* the use of uint8_t (full bytes) instead of bit fields for the Processor status register. You'd have to create a P status value whenever you push or pop it from the stack. But that may save a bunch of load and bit test operations, and potentially a bunch of read-modify-write operations. We do hit these almost every instruction. 
There is also a lot of hardcoding of various I/O switches. This means there is no flexibility in what hardware is included, or what slots it's in.
One other trick they do is have a page that "throwaway" writes can go to
They return a random number on a floating bus read, as opposed to one calculated from video memory. 
on reset they set SP to 0xFF, and flags to 0x25. Hmmm. I need to check the data sheet on that.

Giving thought to memory mapping in the IIe environment. There is the added complexity of Auxiliary memory; treatment of page 00/01 with bank-switching there; etc. The current setup will require us to modify a number of pointers whenever we bank-switch. You can set reading and writing independently, which we can handle with read_p and write_p. But it seems like a lot of setting pointers whenever we switch. I guess honestly it's not that much - the disk ii and video code do far more mangling. I was thinking that instead of storing whole pointers, I could store page indexes.. e.g. text display bank bank 004, and aux bank 104. Then we just shift left 8 bits to get the offset into a 128K memory bank, then use the page offset. I.e., we map the page to a new page, then keep the offset part. my thought was, what if I could just add that 0 or 1 (for main or aux) based on the soft switch settings as part of the address calculation and thereby not have to change 256 pointers every time we switch. i.e. trade a little math every read/write, versus changing pointers whenever the switch is hit. if programs generally sit in one bank, then setting the pointers is a win. If programs flip back and forth a lot, then the bit of extra math is a win. I guess we can only test it and find out. Let's just proceed with the current setup and see how it goes.

Testing just the base MMU class, we get this performance:
```
Time taken: 514877875 ns
total bytes read/written: 983040000
ns per byte read/written avg: 0.523761
maximum simulated clock rate w/memory access every cycle: 1909.268311 MHz
```
that implies we lose more than 50% of performance by adding the 2nd layer.

Since I now have this separate MMU class, I should be able to construct a CPU test target that runs the 6502 test suite against this MMU, to allow testing of the CPU code independently of the rest of the emulator. (Full circle!)

Learned about Apple Instruments - profiling visualizer - and how to use it to profile code. Seems like it would be much easier to use this than the other profiling I was using. And, use this to performance test various modules. What I learned about GS2 so far: it spends most of its time busy-waiting for timing. ha!

## May 27, 2025

Apple IIe memory management:

* ALTZP: switches pages 00/01 between main and aux, independently of other settings.
* 80STORE
  * 80STORE ON AND HIRES OFF: PAGE2 switches 4-7, between main and aux, independently of other settings. 
  * 80STORE ON AND HIRES ON: PAGE2 switches 4-7 and 20-3F between main and aux, independently of other settings.
* RAMRD: ON means READ from Aux memory in 0200 - BFFF; OFF means READ from MAIN memory in 0200 - BFFF
* RAMWRT: ON means WRITE to Aux memory in 0200 - BFFF; OFF means WRITE to MAIN memory in 0200 - BFFF

Note: "When you switch in the auxiliary RAM in the bank-switched space, you also switch the first two pages, from O to 511 ($0000 through S01FF)." What does this mean?
"The other large section of auxiliary memory is the language-card space, which is switched
into the memory address space from 52K to 64K ($D000 through SFFFF). This memory
space and the switches that control it are described earlier in this chapter in the section
"Language-Card Memory Space." The language-card soft switches have the same effect on
the auxiliary RAM that they do on the main RAM: The language-card bank switching is
independent of the auxiliary RAM switching."
ok asked the group.

Short version: while there are cases where you can switch the entire 48K memory space, there are many situations where you don't. So we're going to be manipulating lots of "pointers" for each page no matter what. The only way to reduce the amount of work would be to change the page size. If we do that, then we have to have several layers in some cases to further decode accesses. Let's say we switched to 1K pages. We'd have to have special handling of 0-3ff (it's split half and half between stuff that can toggle in and out of AUX mem). And the I/O space would be chopped up awkwardly. 4K pages - 0-FFF is super awkward, but, the rest of the memory map less so. In short, a fair bit of complexity. I still think it's best to try using 256-byte pages, as we have it now.

ok I've got the 6502 CPU test program working again, in its own app.

Working on decoupling cpu & old mmu - see [MMU.md].

## May 28, 2025

next step is to disable all the devices, and start adding them back in one by one after modifying for the new MMU. This will typically involve them no longer being passed cpu_struct.

OK, I did the IIPlus keyboard. Relatively easy! I say "Relatively". ha. I suppose I can do the speaker next. It has a super-simple I/O interface.
Speaker is hooked back up. Muahaha.
Display doesn't work. That will be some effort, lots of modules accessing memory. That needs some refactoring too, to use direct memory accesses instead of the MMU interface.

With this MMU work, it might be feasible to go for an Apple III configuration right out of the gate as my next deal as opposed to a IIe. That could be fun.

ok, the display code is updated! That wasn't bad at all. 
Gamecontroller done.. 
Language card. It's a definite maybe. ProDOS boots! Need to run the language card tester.. which means, DiskII is next.
Disk II done. whoo!
lang card test seems to work. Fixed a bug in the keyboard (AI changed the C010 to C000 on writes, boo, hiss).
Fixed the prodos_clock.
So, booting with -p 0 doesn't work because something important is missing. It must be the Videx it won't run without. Well we know what's next then, don't we..

## May 29, 2025

I still have a few cards left to refactor, but this is actually going swimmingly. I was very worried about the LangCard because of it's complexities, but the register interface didn't change, and that was the hardest part. (Still need to fix the double read in certain instructions). 
mockingboard and parallel cards done. That might be it?? it might be. Do a day of testing, then clean up all the commented-out stuff, before doing a commit and push.

So, when there is not a mockingboard in the systemconfig, we crash. Yah. Let's see.. ok, let's take bus.cpp and memory.cpp out of the build.

Overall my Effective MHz is down about 15% to the 350 range. Ah of course that's with tracing going on in the background and stuff.

There is an obvious optimization to be had in the device code: instead of passing cpu as the callback context, and then calling get_slot_state etc etc., just store the device record directly. A lot of these devices don't care about the cpu, only the mmu. So store mmu in the device state, and use device state as the callback. Then we have a number less indirections to do. Another optimization could be: move the mmu->read and write stuff up directly into MMU_II. 
Hmm, Claude is suggesting my mmu->read *won't* be inlined because it's virtual. That's a concern I had. 

Claude request ID: a6e8c069-3392-4714-8bde-77d0e9417dc0

of course we're also: doing a function call, and, doubling up on 

We can put the memory read first - need to make sure we never have both read_p and read_h, or write_p and write_h. (should be exclusive). Might also consider struct of arrays instead of array of structs for the page table arrays.

Action items:
[x] the MMU should be cognizant of the starting address of the system ROM rd. Perhaps pass in the rd struct instead of the raw address, because then we can get the starting address to do the page map correctly on setup.

manually putting the mmu:read stiuff into mmu_ii is good for about 30MHz improvement.
of course in the keyboard loop there are a ton of calls to the keyboard C000 read routine. look at the context trick above.

keyboard, had to create a state record. We were using a global before. We will of course have different keyboards in the future so need to be agnostic there.

testing with trace turned off (not compiled out: in, but disabled)
test 1: 520MHz
test 2: 550MHz - 

test with trace compiled out: 

[ ] There is a slight issue here which is that the cpu has a member for MMU_II. This should be MMU. And then all the things that need to use it need to cast it to MMU_II. Or, provide some sort of middleware that converts to a minimal interface for the CPU that only provides read and write (and context to use).

[x] I had a thought about speed-shifting. Which is, changing speeds in the middle of a frame. Instead of only counting cycles, we will count virtual nanoseconds. We know how many nanoseconds per cycle there are. We can do it in the main loop. And then, instead of the main run loop waiting for about 17,000 cycles, it checks to see if exactly 16.6666667 milliseconds have elapsed, regardless of speed (60fps). We could also count the ns elapsed based on feedback from the MMU - this would be for the GS and other "accelerated" platforms where inside a single instruction the clock could be numerous different speeds. (right now, done by counting discrete whole 14Ms)

OK, next step is to start going through devices and seeing if I can reduce the codependencies on the CPU some more. Some things go into Computer. Like VideoSystem. 

Some terminology:
* a "slot" is a device that you could have multiple of in a system.
* a "module" is a motherboard device that you will only have one of in a system.

First thing: implement a "event delivery registry". Keyboard, and game controller, need to receive events. Display should too - to handle F2, F3, F5, etc. Keyboard would check for key down events. Game controller for gamepad add and delete. Just like they do now, except, not hardcoded into a loop. But placed in a list. When an event is accepted and handled, return a true and stop processing other handlers. Will this be part of "computer"? Perhaps its own thing, but an instance lives in Computer.

Then we also need a "process frame" for cards / modules. Now, you think this is only for mockingboard and videx - and you would be wrong! The Disk II "frame handler" can update the disk drives status for the OSD. We can do for other devices too that need to present status. Then OSD isn't tightly coupled to Disk II. Annunciator can publish its status too for the display subsystems. So for this we have:
* mockingboard
* speaker
* display (update flash state; update display;)
* videx (update display)
* diskii / smartport - post drive status for osd
So we have these categories of "frames": cpu frame; audio frame(s); video frame(s); OSD frame; Debugger frame; event frame(s).

It makes sense to have each type of "frame" in its own queue, in its own loop. Easily done.

## May 30, 2025

[x] need to hook "init default memory map" to the reset routine.. ? Was in reset.cpp.

All the slot cards and modules now get passed computer_t to their init_ functions.

So this "register reest routine" stuff is a good idea. I removed a bunch of module dependencies. However, I have new dependencies in computer_t - video_system. The main body of code is fine with this, but the standalone speaker app uses speaker.cpp which uses computer which wants video_system etc. So, the speaker test app is more about testing the reproduction logic. Separate speaker into core speaker logic, and the speaker module. Then speaker app will use core speaker, not slot card. The "core" knows how to play back from the speaker event buffer. The slot module knows how to add events to the buffer. 

OK, have the start of the event queue. I think we need a "System queue" that has a chance to process events before, particularly, the emulated machine keyboard routine gets them. Things like the F keys. I'm finding I can rip lots out of the old event_poll this way. It will soon be history.

## May 31, 2025

Keyboard is now free of needed get/set module state, because it's context is now set whenever callbacks are made. It's possible none of the devices will need this at the end of the day, which would be swank.
Finish up the event poll refactor. Coming along. set up a "system" event queue, events that get handled as a priority before the regular event handlers.

OK, the event loop has been re-done, replaced with EventDispatcher. This use of the lambdas was a good idea, it definitely is helping to decouple stuff. The code modules that register event handlers are, right now: computer, videosystem, gamecontroller, keyboard, display.

Right now have two separate queues - system and normal. system basically just gets first grab at an event. Instead of this, I could queue with a priority.

i did -not- wire back in keypresses that generate screen captures, or enable audio recording. I could have debugger commands for these functions instead.

Now I'm wonder if the reset() queue should just be an instance of EventDispatcher to benefit from the lambda fun. Hmm. YES. NO. That is set up for SDL events. Put that on ice while we think about it.

I need to pass computer-> into OSD so OSD can call reset.
    osd = new OSD(computer, computer->cpu, vs->renderer, vs->window, slot_manager, 1120, 768);
So a lot of the other stuff I'm passing in, is available under computer. cpu, videosystem (and then to renderer and window).
slot_manager should probably be in computer then, too.
The get_module_state and set_module_state etc. can be less about finding our own state now, more about publishing state.

I want to add the little tab thingy to the edge of the OSD. First, I am now tracking a "control opacity" which is a thing that will fade controls out after mouse inactivity. practically, should also keep them displayed if the mouse is -not captured- and moves, or, not captured and is in the controls area. That part is easy enough.
Trouble now is: when I click the button, the right thing happens. But the mouse is captured. because we keep processing the event afterward. I need to do:
in tile:handle_mouse_event I need to return a bool from that whole thing back up the stack to tell higher-up we took the event.

I guess now let's get back to the debugger!!

## June 1, 2025

The hideaway tab for opening the OSD looks and works pretty good I think. It follows these UI design rules:

it should be obvious to user when users do normal user things, what the interaction options are.

[x] I should finish implementing the "disappearing message".  (done!!)

I should maybe also put the fade-away logic into its own widget type, like FadeButton or something.

Thinking about copy and paste, because, I have a hundred things to do on this project but this one would be fun. ha ha! Since there is no keyboard buffer on a II+/e, the GS has one but it's quite small.

So - when a paste is done, the text must be put into a buffer, and, we need to meter it into the keyboard routine.

Each frame:
    if there is pending paste text remaining,
        check to see if the keyboard latch is clear.
        if yes, inject a Key Down event ourselves.

So that could (likely) work at up to 60cps. And if software is a little slow for some reason to read the keyboard and clear the latch, we sit and wait. There should be some way to stop a paste operation. A reset() should do it. Drag and drop of text would work the same way. Two different events, same implementation. Using the "create stream of keyboard events" makes it device-agnostic.
I don't think there is any feasible way to support any media type other than text. where should this live? split. define that keyboard modules must support the paste semantics. they're the ones that check themselves for readiness, and, register a frame callback to do so.

Now, for copy. Copy - we create a clipboard object that takes a snapshot of the screen at a particular moment. And we can offer multiple media types. text page could for instance offer text and a graphics image of the frame buffer.
We then tell SDL3 we're "offering" a particular media to the clipboard. We don't actually -provide- it until requested via a callback.

Shift-insert is fine for paste as a keyboard shortcut. copy? maybe print screen? What are the SDL mechanics of grabbing the frame data? Will have to investigate.

claude suggests the following:
https://claude.ai/chat/d62f96a2-8d7b-473c-8847-e8578269d0bb

i.e. keep all my screen data in CPU primarily and just chuck it to the gpu once in a while. That is actually working really well. When there are full-screen updates going on, it's actually running faster to do just one update per frame of the entire texture than up to 24 partial texture updates. Running in about 1/2 the time it used to.

Still getting a 3x benefit of partial display recalculation vs doing the whole thing (500us for full update, 175-200us for partial). Of course, the hires calculations even with LUT are fairly expensive, so not having to do all of those is a savings. But - are there any possible optimizations if we get rid of that?
* fewer conditionals
* don't have to do the txt/hires shadowing. That could save a lot of horsepower on the CPU side not having to do all those calculations on the update side, and I don't have a way to measure that easily.
* what about generating the scanline bits in actual bits, instead of in bytes. I would be moving 8x less data around. This doesn't help once we get to the IIgs of course but would for the legacy II modes.

But I -still- have the entire display in VRAM texture that I can use to create thumbnails. And in RAM I can stuff into the clipboard.

(Just tested on linux - this made an even bigger performance improvement there, I have frame draws getting down to 100us! I wonder how windows will react..)
ok, I got build working on Win with PROGRAM_FILES=ON. The other causes it to try to insert /share/blah. still need to switch windows to use cpkg.

## June 2, 2025

got event_queue pulled out of cpu and into computer. Getting there! A good next bit will be 'mounts'. 
Done. Also doing a bunch of misc. cleanup. Some functions have multiple iterations of commented-out code. ick.
Got video_system out of cpu too.

[ ] For efficiency, we should draw the control panel template - the styling, "Control Panel" text, etc whatever nice graphics we want, into a texture and then just lay down that texture first with the dynamic elements on top of it. Right now not a big deal, but, if we want something really nice-looking it will need to be done this way.

Event_Timer needs to go into computer too. This should involve only the mockingboard right now.. yep, and done. 
Two main things left in CPU that should go, are the clocking stuff (not the clock counter, but the stuff that determines clock speed) and module_store / slot_store.
I bet I can remove a lot of headers from cpu.hpp..

The Videx code should be modified to only LockTexture once per frame, just like the updated display code. done

The modal dialog should be updated with fonts that are actually readable. Done, though I just noticed that if you hover the Cancel button it changes the whole frame coloring. Something's not setting the colors it wants.

* UI principle: either save and restore all context you touch, or, just set all the context you need every time.

videx: Videx color used should track video engine mode: color and RGB should be white; mono should use selected mono color.
Currently these routines live in display. They should live in computer.

And we should have a single flag somewhere in videosystem to force a full frame redraw. Key frame draw as it were.

that's sort of in place. But, let's think about the engines. We have engine, which is basically the type of virtual "monitor" connected.
NTSC; RGB; Monochrome.
But we also have different video sources: Videx; Apple II Display; GS display; etc.
for now I need to push the current display engine and mono settings down into videosystem so videx can get it. And, practically, these settings ought to be in the videosystem itself.

Remove bounds checking from all scanline emitters, they're not necessary.

ok, the breakdown is this:
stuff that is general to the emulaTOR - like how we present an application window to the OS; how we draw pixels, and scale; and whether the user wants ntsc, rgb, or monochrome, are decided by the app. It sets values into videosystem; the various display modules can get those values to make rendering decisions. They do not need to handle 
BUT, things like the NTSC configuration is done specifically for the NTSC rendering system in the emulaTED, which is specific to the apple II display module. Videx has no need of it, nor will the Apple IIgs SHR stuff, which will be much simpler direct pixel mapping from GS SHR buffer to modern video buffer.

I've discovered a problem in system timing! a basic benchmark that runs in 1:53 on OE and according to the Creative Computing benchmark from 1983/4, runs in 1:43 on GSSquared. Oops. That's about a 9% difference.
I think the cycle timing is accurate in the main loop, i.e. that I'm getting 1020500 cycles per second. So what could be wrong is that some instructions tick the wrong number of cycles.
basic is gonna be doing a ton of (indirectzp),x etc. 
ROR ZP,X : should be 6 cycles, we're only ticking 4!
oops.
I wonder if this was something broken recently with the mmu updates.. hmm..
in the debugger I see tons of:
ROR $02,X
they are only ticking 4 cycles. 6502opcodes says they should be 6! hey, this is the new super-efficient 6502D.
* ROR A - 2 cycles. load the opcode; rotate.
* ROR ZP - load the opcode; load operand; load memory; rotate; write memory
* ROR ZP,X - load the opcode; load operand; add X; load memory; rotate; write memory (6 cycles)
* ROR ABS - load the opcode; load operand, load operand_hi; load memory; rotate; write memory (6)
* ROR ABS,X - load the opcode; load operand, load operand_hi; add X; load memory; rotate; write memory (6)

This is likely what's making the disk speed wrong.
ok, I added a cycle tick in rotate. I'm still one short. I need an extra one for zp,x.
Made the same fixes for ASL,LSR,ROL.
ZP,Y is counting right?

This is a lot to stare at. And lots of potential interactions. I need to write a test program that tests each instruction and the cycles generated by each. That should be pretty straightforward. Have each instruction in turn in a big assembly file. Have an index of them in the C program. do a single execute_next with the PC set to that. Have to test each case and condition.

Hey, I'm at 1:53 now. So for purposes of all this FP math the ROR was the big deal. for fun let's check locksmith disk speed.. nope, still reads very very slow. 

I have the program ccbench1 on testpo.po

## Jun 5, 2025

ok to work around the problem with mac in the GoodFullscreenMode blowing up when it opens up the file dialog, we've got two options.
* build our own file dialog. (I really don't wanna)
* or, leave fullscreen mode then open the dialog.

I've tried doing this code:

```
#if __APPLE__
    if (osd->computer->video_system->get_window_fullscreen() == DISPLAY_FULLSCREEN_MODE) {
        osd->computer->video_system->set_window_fullscreen(DISPLAY_WINDOWED_MODE);
        osd->computer->video_system->sync_window();
    }
#endif
```

right before we call the dialog thing, however, it doesn't help. I think we don't have time to run through some events and actually perform all the fullscreen mode change stuff.

This would suggest the following process:
instead of immediately calling the OS file selector, we send an event to the main loop to do it.
that event handler does: screen change, then send ourselves an event for next time round, to open the dialog;
this is a hassle! But maybe it's better to do the dialog from the event loop than deeper in the code anyway?

You know, opening the Mac dialog is slow ugly and awkward anyway.. and then we'd have the same interface per platform that would be consistent across platforms.. not now. Put this down for a Later Project.

for now let's ask the user to leave full screen before opening disk. tee hee.

Also, I implemented a diskII accelerator. Got the idea from apple2ts. instead of trying to load the entire byte at once to speed up (as I'd tried), they only skip 6 bits or something. So I tried that and it's working pretty well. Let's alpha test this a while before releasing to the wild. (I still like the "ludicrous speed when disk is on" but I suspect it's going to have other issues even if I get around the current one..)

for Container - allow specifying different Layout object dependencies. E.g., grid layout - but also linear, etc.

also, for buttons/tiles etc - refactor to use lambdas instead of the C-ish setup I have now. Lambdas are superiore! Do it now before it gets to be way more stuff to refactor later. I am able to do it both ways (separate version of the function for lambdas. so we can refactor a bit at a time).

```
boot prodos 2.4.3 in vscode debugger;
hit tab a couple times;
hit a bad memory reference 

    media_descriptor *media = pdblock_d->prodosblockdevices[slot][drive].media;
        fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);

media-> is a bad pointer here.
(Fixed).
```

in the trace Pane, we need to eventually make room for 24 bit addresses and 16-bit registers. So we'll need maybe 10 extra characters per line.
[ ] in trace, option to hide raw bytes and only show PCPC: LDA $xxxx  This will save 10 chars.   
[x] Can also bring the EFF left another few characters to save room.  
[x] Shorten Cycle some more

[x] Develop a "line output" class that hides some of the complexity of generating this text line output. e.g. line->putc() stores char and automatically increments.  
[ ] Develop a "scrollable text widget" that will allow display of lines from a generalized text buffer.

ok, I think we want to move (or replicate) the following controls from the osd control panel, to hover controls (like the pull-out tab):
* display engine
* cpu speed
* reset button

## Jun 6, 2025

I am pretty sure the Skyfox problem is just that it's a bad crack. It's overwriting the interrupt vector (or never setting it in the first place). Looking at 3F0.3FF you can see it get overwritten backwards along with other stuff in the text page 1. So when the interrupts kick on it's just garbaged there.

But the big news is: I determined this with the DEBUGGER, BABY!

I made huge progress on this debugger over the past couple days. Adding to the trace screen from before, I now I have a (very) basic monitor and a memory monitor. I need to add monitor commands to control the memory watch.

Have a TextEditor widget that lets you edit a line of text. insert, delete, backspace and arrows work. don't have copy/cut/paste.

[x] next bit: do a forward-looking disassembler.  

## Jun 7, 2025

I added monitor commands for controlling breakpoints and memory watch.

I'm wrong! Skyfox claims to be Apple II compatible, and, it boots fine in II+ mode in Virtual II. My theory is now that Skyfox is failing because something in there is accessing language card control through a (ZP,X) lookup, which may be relying on 6502 ghost reads aka phantom reads. 
let's go back to that system tester program : audit.dsk
bp's: d17b; c080-c08f;

639e - inc $C083,X - ok, what does this do.
reads c083;
reads c083 again?
writes c083

Test Suite for Mockingboards and 6502/65c02:
[ ] https://github.com/tomcw/mb-audit  

ok, three reads in a row does not seem to cause any problems. how about writes..

Ah, well I found a difference in my language card impl: STA C083 on mine does nothing; in Sather and in a2ts it enables read from RAM!
DOH! That was it. Skyfox is working!! I wonder what else will work now.. HA TOTAL REPLAY boots now. It also detects 64K and the mockingboard automagically. and skyfox on that runs.
checking rescue raiders.. how do I start the game.. 

HERE is the holy grail bible of exactly what the NMOS 6502 does on each cycle.
https://xotmatrix.github.io/6502/6502-single-cycle-execution.html

## Jun 8, 2025

ok! So I got the inc abs,x stuff refactored. here it is. A few innovations here. First, I defined a union struct to make dealing with address calculations like this way easier - instead of doing tons of manual masking and bit-shifting I can just refer to lo and hi portions of an address or the whole thing. This makes coding the pieces that are modifying these bits much easier, and probably more efficient TBH.

Second is just formatting the code in a nice way to show the cycle breakdowns more clearly inside the function.

```
struct addr_t {
    union {
        struct {
            uint8_t al;
            uint8_t ah;
        };
        uint16_t a;
    };
};
        case OP_INC_ABS_X: /* INC Absolute, X */
            {
                inc_operand_absolute_x(cpu);

inline absaddr_t get_operand_address_absolute_x_rmw(cpu_state *cpu) {
    addr_t ba;
    ba.al = cpu->read_byte_from_pc();      // T1

    ba.ah = cpu->read_byte_from_pc();      // T2

    addr_t ad;
    ad.al = (ba.al + cpu->x_lo);
    ad.ah = (ba.ah);
    cpu->read_byte( ad.a );                // T3 

    absaddr_t taddr = ba.a + cpu->x_lo;    // first part of T4, caller has to do the data fetch

    TRACE(cpu->trace_entry.operand = ba.a; cpu->trace_entry.eaddr = taddr; )
    return taddr;
}

inline void inc_operand(cpu_state *cpu, absaddr_t addr) {
    byte_t N = cpu->read_byte(addr); // in abs,x this is completion of T4
    
    // T5 write original value
    cpu->write_byte(addr,N); 
    N++;
    
    // T6 write new value
    set_n_z_flags(cpu, N);
    cpu->write_byte(addr, N);

    TRACE(cpu->trace_entry.data = N;)
}
```

A next step is, I think, to try redefining these as macros instead of as inline functions. That will result in much faster code when compiling non-optimized. It may also eliminate the question of whether the compiler inlined a function or not. More importantly, I think it will avoid all the errors I get when editing the cpu.cpp code.

Another revelation is that the 65c02 implementation will be very different because of this - some of its instructions are different cycle counts, and it does fewer of these false reads. And then the 65816 puts many of them back in! holy toledo.

And also I had been doing a lot of thinking about the '816, its 16 bit registers, the D and B handling, etc. are all going to make it very difficult to just reuse this code.

And this implies a different code re-use strategy. In fact it may not be possible to reuse large chunks of the switch statement because of this as I'd originally envisioned. And, the "undocumented opcodes" probably shouldn't be ignored.

[x] I have a lot of functions where the instruction switch just calls the function. I should flatten that out. That will also help unoptimized execution time.  
[-] Explore: can I have an optimized memory read function for zero page, stack since they can't possibly do any I/O stuff? They -can- be remapped but can't do I/O. Not sure it will make a big difference. (deal with this very differently with new mmu optimizations)


[x] implement cache of things like 'is trace on' by checking once per frame, not every instruction execution.  

## Jun 9, 2025

some loop optimizations? See what kind of difference some of these things make:
15.7610, 15.8359
move cycle subtraction outside of loop: 15.9243, 15.9073, 15.8835 - seems to have helped a bit.
cache processEvents: 16.3768, 16.4482: good. make sure it works. ha!
16.4 / 15.8 = almost 4%.

When we're in free run in the main loop, we are only processing 17008 cpu cycles. but in free run, this will do the frame-based stuff checks a hundred times more often maybe? Wasted time. however I set the cycles per 'frame' for free run mode to 66665 (4 times as much) and effective mhz slowed down? I am getting tons of audio queue underruns. I am not generating enough audio data. 170080. a bit slower yet. frame processing stuff must be failing somehow. compiled with optimizations, I get 356MHz. That might be a hair better. Let's put it back for now.

## Jun 10, 2025

ok, disassembly in debugger is there! it could certainly stand to be fancier. Particularly, . There is also prospective 10-line disassembly following the program counter when in stepwise trace mode.
What we don't have: retrospective diassembly pane. this would be: current instruction centered vertically; disassembly going backwards, and forwards, from that point.

Backwards disassembly on the 6502 is tricky. Here is the basic idea though. This is going to be some kind of a tree search algorithm to maximize the successfully decoded instructions.

1. check PC-1 to see if it matches any 1-byte instructions. If so, mark it as tentative.
2. then check PC-2 to see if it matches any 2-byte instructions. if so, we may need to rethink step 1.
3. then check PC-3 to see if it matches any 3-byte instructions. If so, .. you get the idea.

the trouble with this is, for any given chunk of bytes, there could be many interpretations.

[ ] create a scrollbar widget. the up, down, home, end, pg up and pg down events will go to it. it handles paging, and lets the caller query the current position.  
[ ] scrollbar should be clickable, and this jumps the position to that location  

[x] Have device frame registration function and call from main event loop  

I decided that devices can have private state - but also "published" state, that other modules in the system can query. I am currently kind of using SlotData and MODULE data for both purposes. But, we want these separate.

The "published" data will be pushed by a device during the device's "frame handler". as far as other parts of the system are concerned, it's readonly. And it rests on a base class that stands alone, that doesn't require something like the OSD or debugger to pull in all the detailed headers and data structures from Disk II, etc.

So aside from - generating audio data - emitting video frame - devices can push this status stuff out. The "published info" will be to "well known names". The first time, an entry is created. Succeeding times, the entry is replaced. go with an integer as the name, sort of like I did the Disk "keys". And the stuff may not be super-high-performance (ie. using vectors and such) so you want to only access any of it during frame time. (i.e., NOT during the CPU loop).

How about we call them "mailboxes"? Let's ask claude. ha. Claude agrees. It's so easy to manipulate claude. Poor Claude. NOPE. MessageBus and Message, to be completely boring.

ok, have an implementation. Now to try it.

## Jun 11, 2025

implemented DeviceFrameHandler concept, and mockingboard now uses it. Works! Now I should be able to have TWO mockingboards in a system_config. 

The reset handler stuff needs to be switched around to use lambdas and pass more specific context. with two mockingboards the 2nd one doesn't reset correctly because it's hardcoded for slot 4 lol. oops.
Do a text search on SLOT_4 to make sure we get 'em all. (done)

Well this is interesting, Ultima V supports a MIDI interface for music. Huh. Apple2ts supports it. how? 
"Passport card" and software. Web MIDI supports virtual devices in browser. 

This looks like the ticket right here for what will be a cross-platform library to take MIDI in and synthesize output:

https://github.com/FluidSynth/fluidsynth/wiki/MadeWithFluidSynth

This will be for the //e implementation because Ultima V requires 128K iie to do music. And as far as I can tell ultima v is the only program that can use multiple mockingboards.

Mockingboard mixing really takes a beating in Cybernoid music demo.

need to refactor these to use MessageBus:

bool any_diskii_motor_on(cpu_state *cpu) {
int diskii_tracknumber_on(cpu_state *cpu) {
drive_status_t diskii_status(cpu_state *cpu, uint64_t key) {

But MessageBus -will- need a concept of "find me all messages that match a class type". E.g., "any diskii motor on" needs to iterate all disk II's. I'm not gonna use this but the IIgs does this (slows clock while any diskII motor is on..)
But also OSD wants to iterate all the possible disk controllers in the system to construct its displays, instead of hardcoding them.
OR. the diskII routines have a global variable, which is the status for all diskII devices. (it's that old slots[8][2] array again..) and that is registered mailbox also. Or there is a routine in the diskII code that whenever we update motor on we update that global.. or the first disk device that starts, creates the record, and registers it, which contains data for all slots. Is this ick? or genius?

Also: new concept for Device Debug callbacks. when debugger window is open, and certain device types are selected, we will display useful diagnostic info; e.g. mockingboard registers; disk ii track no and head position; that sort of thing. Then each module is responsible for its own debug info! Snazz.

All the mount/unmount stuff is kind of fugly too. Sigh. One thing at a time. But.. ooh.. we could do the cassette device this way. Hmm. CiderPress can import WAVs to a disk image. Perhaps I could use the CiderPress code. When they 'load' the cassette, feed the audio data into CiderPress, get the binary data out (even if we have to do it in batch) and can play the WAV also out through the mac speaker.

ok, cool. nobody's gonna use this but it will be there for completeness (and maybe an apple I mode..)

## Jun 12, 2025

started refactoring callbacks in videx to use videx_d - done for the bus-facing stuff. However the video-facing stuff isn't. Seems like video frames should be called with a lambda. BUT - who determines what module has control of the video display? Maybe : videosystem clients can return a "priority". whichever one has a higher priority wins. Right now there is only the mainboard display, and videx. In the future, there will be mainboard display and IIGS SHR mode. Perhaps these can determine on their own if they execute, and, return true/false. There is this "Video7" RGB card stuff that a2ts supports now - "provided improved text and graphics, including 40-column color text, and various low-resolution and high-res modees with 16 colors". This is another example.

update_display will then move down into videosystem.

What's an easy way for various modules to get cpu->cycles without having to put cpu in every one? We use this timekeeping concept a lot..

ha! The only thing lc wanted cycles for was debugging. Super lame.

Does the MMU need the "can read / can write" flags at all any more? Or for that matter, the type? Replace the "type"s with the descr. Done. Yeah, nothing was using the flags any more. Text descriptions much more useful.

ok, monitor now has "map" command to query mmu page table entries. "map" by itself dumps C0-CF and D0 and E0, basically, I/O, C8, and language card status. When we get to IIe land we'll need to add 00,01, Text page 1 and hires page 1 (because these can be remapped by the aux memory stuff).

I could put a pin in the debugger and start working on the Apple IIe.. let's look at the roadmap. Ah, I need to do the platform selection on boot. If the user runs with GSSquared -p xx then just boot that machine. Otherwise, offer the user a choice of machines to boot, based on the systemconfig.

```
options:
    Draws apple color logo, then a big rectangle with the tiles in it. And make it opaque and fuzzy. yah yah.
    click on config, it shows that machine's configuration, and has a "start" button.
    also has an "edit" button that pops into the OSD to edit that machine config.
    You can save as a new name.
    When you boot a machine, it uses the nice big hires apple color logo, which fades out over a couple seconds.
```
Put it into UI.

This can all happen in a special event loop, since none of the emulation is running. Once the main event loop exits, it can come back to this.

[x] So, F12 powers off the machine and returns to the menu.  

## Jun 13, 2025

Hm I'm going to need a way to specify the MMU as part of the system config. Should go in platforms struct.

wondering if content_rect should be relative to the tile rect. i.e., whenever we draw we add tp.x,y to cp.x,y to get the content location on screen. This feels right..

usual use cases:
new Tile_t:
  create tile with cp.h,cp.v.  (cp.x,cp.y are relative to tp.x,y so set to 0,0 by default).
  set_tile_pos(x,y) - sets tp.x,tp.y

new button: img.
  create tile with cp.h,cp.v
  set_content_pos (to locate the content inside the content area)

I think I might be rendering the OSD (completely off screen) when I shouldn't be.. let's debug that actually. Yes. oops.
Rendering time for a text frame is now down to 230us when scrolling a lot. When we're just flashing the cursor (only doing one line update) as low as 22uS. Yow.
Debugger uses 440-450uS when trace is open. That's a lot lower. the unneeded OSD drawing was probably taking 100-130uS per frame. Keep eye on issue where window close to screen edge was flaking out.. I wonder if drawing OSD in negative coords all the time was causing that issue.

Well, how hard could it be to do a shutdown thing with F12 now?

[x] all the text buttons need padding respected again.  
[ ] Work from the linux build machine, to clean up memory allocation on VM shutdown.  
[ ] pull sdl init / deinit out of the loop. that can't be good. but the screen textures are probably taking up a lot of space.

We leak about 100-150MB per power on / power off iteration on the Mac.

## Jun 14, 2025

checking out the code that William Simms did, to generate video per emulated cycle. It works! I'm not 100% sure about some of the colors being right - hard to tell, I could have typed my program in wrong :-) 
It does limit the CPU to 135MHz or so. It does also work at higher clock rates, which makes sense, because if he's timing the virtual scanlines based on virtual clocks, that's fine, he's just generating many more FPS than a 1MHz machine. It does slow down the maximum speed, but, it is still pretty darn fast.

So, can we have this as a separate display module, and if so, how? Calling a different frame-generator is no big deal. Let's see how he is creating the output.. maybe there are optimizations.

First thought - skip all this and use current video if not in 1mhz mode. second - there is a lot of math going on here. masks, bit shifting, and a fair number of conditionals. a call to get_module_state.

It uses vcount/hcount to generate a memory address based on the video mode. Instead of that, how about a lookup table? Precalculate the memory addresses for each possible hcount/vcount on the display. Different table of course for each possible combination of "normal" video mode. if we switch video mode in the middle of a scanline, it's no big deal, we automagically switch to a different LUT. Maybe don't need to do that.

He's doing small LUTs to convert each video memory value to "bits". Which the current code does though it converts to bytes.

What if we did this as a separate thread? We'd have to synchronize the threads. Could get hairy. Scratch that.

These things are cool (and accurate) though they have limited (no?) practical application.
incr_cycle: a macro: increment cycles; if incr_handler is non null call it. inline.
So William's code (the cycle-accurate) would only kick in when the system is in 1MHz mode. In faster modes the current approach is used.

Switch the frame processors to read from an array of bits, like I've wanted. Then those bits can be constructed in one of two ways:
* William's code
* a frame pre-processor - as we have it now, except we generate a bit stream instead of a byte stream.

And maybe take this opportunity to handle dhgr which can start 7 pixels early. So the buffer is not 560 pixels, but.. 567 ?

Now, let's contrast this with the approach I had been considering, which is to store cycle times of display mode changes and consider those when rendering out frames. This feels like special-casing and doing a lot of work. It would require a special frame pre-processor. Which is ok. Instead of "hires", "lores", etc. it would be "any mode" and would operate very similarly to the math in William's code except instead of doing it in incr_cycle interleaved with CPU execution, it is done in a tight loop by itself. In fact we can have the function pointer point to a function specialized for the video mode we're in. A lot fewer IF's.

He suggests RGB might be hard with just a bitwise approach but isn't that what I'm doing now? I just go back and cheat and fill in spaces between pixels.

Oops. Just sitting at the retro experience menu, we're leaking memory. Whoopsie!

there is no leak running with the VM on. started VM, then stopped. now 138MB. start: 193, then 145MB. now 150MB. ok, definitely some leakage.. Hmm, we need to shut down the audio system.

[x] sometimes after a stop vm and start new vm it's like reset got hit but the vm starts right back up where it left off. Whaa? I should erase all of RAM on MMU init. (that was it)  

now I am leaking while just running the drive at boot. HMM. tracing? turn off tracing.
once I open the debug window, it uses and does not free up memory. maybe I should destroy the debug window and renderer..
leaking just sitting at basic prompt. Hm, audio routines?

pretty dramatic leak in OSD. Ah, but maybe only when built with debug, asan, etc.

## Jun 15, 2025

so, this has some ugly:
```
void update_display(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    annunciator_state_t * anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
    videx_data * videx_d = (videx_data *)get_slot_state_by_id(cpu, DEVICE_ID_VIDEX);

    // the backbuffer must be cleared each frame. The docs state this clearly
    // but I didn't know what the backbuffer was. Also, I assumed doing it once
    // at startup was enough. NOPE.
    ds->video_system->clear();

    if (videx_d && ds->display_mode == TEXT_MODE && anc_d && anc_d->annunciators[0] ) {
        update_display_videx(cpu, videx_d ); 
    } else {
        update_display_apple2(cpu);
    }
    // TODO: IIgs will need a hook here too - do same video update callback function.
}
```

This functionality should get pushed down into videosystem. And video devices should register their frame processor (FP) routines for videosystem to select which ones get executed at any given time, using the sort of priority system noted earlier. if a FP returns true it means it is active and handled it. Otherwise we go to the next priority routine.

So what that looks like:

mb_display registers with priority 0. videx registers with priority 1.
videx in its FP looks up the annunciator state. (This should be done with MessageBus)

## Jun 16, 2025

I think the approach of reading video memory --as it is in a given cycle-- is correct. The memory could change between when the "beam" is at a given location, and the end of the frame. If we read the memory only at the end of the frame, we could display the "wrong" data. So let's consider optimizations again.

He's got H and V counters already.
First, additionally keep a cycle counter - a count of where we are in this frame. I believe, the counter can be up to H * V.
Second, use that as an index into lookup table. VAddr, video address lookup.
Have a VAddr LUT for each mode. Text, Lores, Hires; DText (80-col), DLGR, DHGR. Of course have to switch from graphics to text in mixed mode, which is based on the line counter. Each LUT is 15360 bytes (40 x 192 x 2 bytes) - bigger really since we track cycles outside the visible area. But you get the idea.

Each LUT entry is the video memory address we fetch OR it's zero for "no video output this cycle" (i.e. outside the display area).
We put the video data into a buffer, and also put the current mode into a buffer.
Then, at the per-frame render stage, we process that buffer into bitstream.
So we don't need H/V inside incr_cycle or any of the math.

So incr_cycles looks like this:

```
inputs:
 int mode; // // text, 80text; lgr, dlgr; hgr, dhgr; some flag also for mixed mode?
 int cycle_in_frame; (0 - 17000ish)

mega_ii_cycle() {
  uint16_t vaddr = clut[mode][cycle_in_frame];
  
  if (vaddr) {
    uint8_t video_data = mmu->video_read(vaddr) // also store internally for floating bus value
    *vdata++ = video_data;
    *vmode++ = video_mode
  }
}
```

for double hi-res and 80-column we have to read and emit two bytes into this buffer. The buffer is at most 3 bytes x 80 x 192 = 15360 bytes consumed.

Lookup tables are initialized at boot time.

composite bitmap: 560 / 64 is not even. we may need a slightly bigger buffer anyway to account for 80-mode shifting. 64 bits x 9 is 576. To process a scanline then we only do 9 64-bit reads. bit 63 = leftmost - using MSB here simplifies. We don't have to mask in at a high bit; we can just |= 0 or 1. Well it's the reverse on the other end, so it's six of one.

OK let's do this for 0.4. I feel like I can put a pin in 0.3.5, the primary goal of which was refactoring to get things ready to start implementing IIe stuff.

And I think we can start working on IIe Rev A as soon as I cut this. IIe rev a gives you: 80 column text but not dhgr or dlgr. This is actually a fun little step from II+. So:
* 80-column card
* extended 80-column card (128k)
* IIe MMU (This integrates lang card stuff in a sort of new way, it has to also map to aux / etc., so new module)
* 80-column text.

Then IIe Rev B adds:
* DLGR
* DHGR

We want to do Wm. code integration first.

And I think switch to bit stream instead of bytestream. Need to relearn how that's done.

## Jun 19, 2025

Pixel Format

Optimal pixel format for these platforms:

* on linux, RGBA
* windows, BGRA
* mac, ABGR

What the actual! LOL. We're going to want to identify the optimal pixel format per platform. Will have to be compile-time since we need to change our byte order depending on platform.

OK, that's been hooked in.

## Jun 20, 2025

'dpp' the little display-plus-plus test harness continues to be fun! right now you can select 1 of 4 generators - text40, text80, lores40, lores80 - and one of two renderers, monochrome and ntsc. We will add of course hires and hires80, as well as an RGB renderer. I think the RGB renderer we'll try implementing according to what appears to be the IIgs RGB patent #US4786893. If it can be translated into English, ha!

## Jun 21, 2025

https://retrocomputing.stackexchange.com/questions/13960/how-is-the-apple-ii-text-flash-mode-timed

According to that, the II flash rate is 2.182Hz. That is 0.458. What ratio is that out of 60.. 27.49. We can't do fractional frames. So let's say, it's 28. That then is 14 frames per portion of the flash cycle. 

## Jun 22, 2025

I think the MMU_IIe needs a concept of "default setting". But let's explore the softswitches.

Aside from the Language card switches, which work like they do now except they also need to work in the alternate memory bank, there are these:

activated on writes:

| Switch | Name | Description |
|-|-|-|
| $C000 | 80STOREOFF | Allow page2 to switch video page1 / page 2 |
| $C001 | 80STOREON | Allow page2 to switch between main & aux video memory |
| $C002 | RAMRDOFF | Read enable main memory from $0200 - $BFFF |
| $C003 | RAMRDON | Read enable aux memory from $0200 - $BFFF |
| $C004 | RAMWRTOFF | Write enable main memory from $0200 - $BFFF |
| $C005 | RAMWRTON | Write enable aux memory from $0200 - $BFFF |
| $C006 | INTCXROMOFF | Enable Slot ROM from $C100 - $CFFF |
| $C007 | INTCXROMON | Enable main ROM from $C100 - $CFFF |
| $C008 | ALTZPOFF | Enable Main Memory from $0000 - $01FF & avl BSR |
| $C009 | ALTZPON | Enable Aux Memory from $0000 - $01FF & avl BSR |
| $C00A | SLOTC3ROMOFF | Enable Main ROM from $C300 - $C3FF |
| $C00B | SLOTC3ROMON | Enable Slot ROM from $C300 - $C3FF |

* 80STOREOFF/ON

$C000 is normal Apple II behavior. $C001 means any access to the page2 switch (C054?) will toggle the banks between main and aux memory, i.e. it will swap $0400-$07FF, and $2000-$3FFF between main and aux memory. It is unclear what happens if aux is swapped in and I do C000. Does main swap back in?

* RAMRDOFF/ON

This is fairly straightforward. However, the overall behavior probably depends on 80STOREON.

* RAMWRTOFF/ON

Same.

* INTCXROMOFF/ON

This seems straightforward. However, we need a way to RESTORE the slot rom. I.E., we need to store the Slot ROM / Write Handler routines in a parallel data structure and be able to "restore" them when needed. I.e. a slot card registers itself with say $C6, we need to record that also in the backup slot config. When INTCXROMOFF is hit then $C1 - $C7 is restored from that.
I would assume with this ON, the normal $C800 behavior also is suspended?

let's say we have a ROM device. it registers INTCXROMON. when called it will set the map for itself.
Who would own INTCXROMOFF then?

Alternatively we can have a IIe_Memory device that handles all the memory management. It could even be built into the IIe_MMU. 

* ALTZPOFF/ON

Pages 0 and 1 pretty self-explanatory. BSR I think refers to the language-card ram, and this switches in and out with pages 0 and 1. Which means we need to be able to apply one additional parameter to the language card address calculation - add a base address of $0/0000 or $1/0000 to the calculation.

So, yes. When a ROM connects, we need to be able to "revert to default ROM mapping".

Maybe what we need is a main ROM device instead of special-case handling of ROM. when "ROM device" starts up it hooks itself into memory map like any other device, but it flags that it is main rom / default rom. This is permanently recorded and can be "reverted" to.

The heart, the key difference, between all these 6502 based systems is memory management / mapping. The Apple II, Plus, E, and /// can all take disk II controller cards. That card needs to map its ROM into address space. Apple I does it differently, uses D000 for I/O, doesn't have C800. the /// doesn't have C800 (or does it, it must for Apple II compatibility?)

The GS encapsulates an Apple IIe (or IIc?) memory management inside its much bigger memory space.

[x] oops. Videx isn't taking over screen any more. (not an mmu issue. had not accounted for changed color value scheme)
[x] HGR doesn't clear (or, doesn't shadow update) the video screen? between 3000-3FFF. (there was no memory being mapped!)
[x] trying to boot applepanic.dsk results in crash at memory c014  

ok, back to the work at hand. on a reset we pretty much immediately hit INTCXROMON. So I need a plan for handling that.

This is a document explaining what the startup MMU / IOU errors are:
http://absurdengineering.org/library/MASTER%20Tech%20Info%20Library/Apple%20II%20Hardware/Apple%20IIe%20Hardware%20Issues/TIL00297%20-%20Apple%20IIe%20-%20Component%20diagnostics%20(1%20of%202).pdf

## Jun 23, 2025

I got rid of the IOU errors by having display set up switches that provide status of hires, etc. But that really belongs in the MMU. So there is this interplay between the IIe MMU and the display stuff. Toggling various display switches can have an impact on the memory map. So, thinking about this:

have IIe_Memory register the display softswitches. It configures memory map. Then, it calls into display to feed display the display mode changes. This is instead of display directly registering the display mode switches. We do already have routines to set the display mode stuff. For simplicity maybe have a single routine inside display to accept the call then it dispatches to the correct internal routine.

Pages that are switched:

* $00, $01, $D0 - $FF - ALTZP
   * inside $D0 - $FF, map is controlled using $C080 - $C08F "language card" registers.
* $02 - $BF - switch between "auxiliary" and "main"

On RESET, bank switch area is set to: READ ROM, write RAM, 2nd bank. (on IIPlus, RESET did not affect BSR).

RAMRD - choose between read main or aux
RAMWRT - choose between write main or aux

with 80STORE on, then PAGE2 selects main or aux memory. with HIRES off, the memory switched by PAGE2 is Text Page 1.
with HIRES on, PAGE2 switches both Text Page 1 and HIRES Page 1.
If you use both AUX-RAM switches (RAMRD/RAMWRT) and the display page controls, the display page controls take priority.
If 80STORE is off, RAMRD and RAMWRT work for the entire memory space from $0200 - $BFFF.
If 80STORE is on, RAMRD and RAMWRT have no effect on the display page.

So let's linearize this:

1. set $02 - $03, $08 - $1F, $40 - $BF based on RAMRD
1. set $02 - $03, $08 - $1F, $40 - $BF based on RAMWRT
1. set $00, $01, $D0 - $FF based on ALTZP and set D0 - FF more specifically based on BSR registers
1. IF 80STORE ON,
   1. IF HIRES OFF,
      1. set $04 - $07 based on PAGE2; set $20 - $3F based on RAMRD/RAMWRT
   1. IF HIRES ON,
      1. set $04 - $07, $20 - $3F based on PAGE2
1. IF 80STORE OFF,
   1. set $04 - $07, $20 - $3F based on RAMRD
   1. set $04 - $07, $20 - $3F based on RAMWRT

Now it might be easier from a compositional view, to perform:
Paint ALTZP
Paint RAMRD/RAMWRT
then overwrite as needed with 80STORE

AH, or what we can compose is derivative flags:

ZP: main or alt
TEXT1: main or alt
HIRES1: main or alt
ALL: main or alt (ALL is whatever is left over after subtracting the above)

When composing, keep track of OLD and NEW values (e.g. oldZP and newZP) and only change the page table entries that need to change. i.e. if old and new are same value, don't do anything.

Whenever HIRES or PAGE2 switches change, IIe Memory needs to know. So, that thing where it takes over those switches? What if we just overwrite the existing settings, and ensure IIeMemory always comes after Display in the systemconfig? Side-effect hell, but, simple and effective.

[x] I think the video scanner draws from main memory no matter the settings of RAMRD/RAMWRT etc. This implies we have to change the code in HGR etc. to lookup base_memory instead of pulling the page table entry. Which it seems they are doing, though I am unsure why, I thought I used mmu->read in places.. check it out anyway.

When 80STORE is on, PAGE2 should NOT change video page. Video settings should be set to PAGE1 even though PAGE2 flag is set.

interesting tidbit from Scott:
```
Although AN3 (aka: FRCTXT, aka: DHIRES) doesn't activate double-resolution graphics when 80COLUMN is off, it *does* alter the display of both LORES and HIRES graphics. A Prince Of Persia forum has an whole thread discussing this behavior at https://forum.princed.org/viewtopic.php?f=63&t=4450
```

ok I have the languagecard code ported over to iie_memory. What's lacking is a way to return to default ROM. Taking a break...

self test is failing now with "ROM:E8" which is ROM from C100 to DFFF I think. There is code around $C549 that is checksumming and skips specifically $C400 in the calculation. $C400 is the checksum. Our checksum was 09 and C400 contains 1B. Hunh.
Algo is:
iterate from C100 - DFFF skipping C400.
start with A = 0. CLC. Add ($00),Y. iterate.
I suppose I should somehow verify this checksum myself. 


## Jun 24, 2025

So:
```
C007:0
1500<C500.C7FFM
1564:0 (or breakpoint)
156C:0 (or breakpoint)
```
if you hit 1564, the checksum did not match
if you hit 156C, the checksum -did- match.

ah ha! the error is CFFF. CFFF should return the value from the ROM underneath, but right now we are returning EE!
So what the CFFF check does in the MMU_II is: set_default_rom_map. Apparently INTCXROM overrides this. 
C800-CFFF that CFFF resets, is part of "slot rom". ALL "slot rom" is overridden by INTCXROM.

alright, the self-test passes but this is really all bad right now.


Slot devices register their SLOT ROM with mmu.
Then MMU is responsible for composing the memory map, instead of various parts of the system.
We could have a couple different mmu routines to "update" the map.
C100-CFFF

[x] open question: does CFFF function go away when INTCXROM is ON ? or does it also remove the C8-CF mapped area?  
[x] open question: memexp card, slot 7 - if I access C700 does it activate C8-CF ROM for that under the INTCX? I am guessing not.. though it may not matter. (I think this logic proven correct at this point)  

maybe move INTCXROM handling to the mmu itself? Then I know the things (inside mmu).. checking..

ok. so mmu will directly record:
io_pages(C1 .. CF)

set_slot_rom sets those. also need the same to handle mockingboard memory-mapped I/O.

Then a routine to compose based on:

if INTCXROM set
   use internal CX ROM C1 - CF
else
   use SLOT ROM from C100 - CFFF (which includes C800 handling)

if SLOTC3ROM is set
   use SLOT rom from C300-C3FF
else
   use INTERNAL ROM from C300-C3FF

it either pulls from the slot_rom ptes (15 of them, kept just like regular PTEs), or, configures PTE from INTCXROM.

ALL slot cards manage the "slot rom pte" via these APIs:

set_slot_rom
set_slot_rom_etc
c800_handler

whenever these run, we run the compose_c1_cf routine which will install actual PTEs based on switch settings.

lets review the cards we have with firmware and/or C8:
* disk ii. (tested)
* memexp (tested)
* thunderclock (ok)
* pdclock (tested)
* pdblock2 (tested)
* videx (not working, needs to be updated)

The II Plus version of compose_c1_cf is simple: just put slot_rom_pte in place. IIe version does the composition.

So, SLOTC3ROM default seems to be internal rom. and status 1 = slot rom, 0 = internal rom. (based on apple2ts).
duh the switch is called SLOTC3ROM. 

What does this mean..
"When SLOTCROM is on, the 256-byte ROM area at sc300 is
available to a peripheral card in slot 3, which is the slot normally
used for a terminal interface. If a card is installed in the auxiliary
slot when you turn on the power or reset the Apple lle, the
SLOTROM switch is turned off."

is this "turned off" done in software or hardware? In any event I had the sense of INTCXROM backwards; the original IIe manual doesn't even call it that. It calls it SLOTCXROM.

iie tech ref claims on reset or powerup the BSR switches are set for bank 2, read rom, write ram.

"When you initiate a reset, hardware in the Apple IIe sets the memory-controlling soft switches to normal: main board RAM and ROM are enabled; if there is an 80 column card in the aux slot, expansion slot 3 is allocated to the built-in 80 column firmware. auxiliary ram is disabled and the BSR is set up to read ROM and write RAM, bank 2. (hardware)

Then the reset routine sets display softswitch to: 40 column text page 1 using primary character set. (software)

add "debug" command to monitor to set the debug flag.

Sather IIe:
"if 80col was the RAM addressing input, then there would be only one displayable page of DOUBLE-RES raphics. This though is not the case. By resetting 80STORE and setting PAGE2, a programmer can select the $800-$BFF area for TEXT/LORES display or the $4000-$5FFF area for HIRES display, even in the double-res modes. If 80STORE is set, the secondary pages cannot be displayed."

So, you CAN use Page2 of 80-col/dhgr.

Ah, Sather to the rescue:
You will find, how-ever, that it is possible for motherboard ROM to steal response to $C800-$CFFF without setting
INTCX ROM. The $C800-$CFFF range is assigned to motherboard ROM (INTernal) anytime SLOT-C3ROM is reset and an access is made to $C3XX.


I have some issue with the LC RAM space, because ProDOS isn't successfully booting. The II+ LC test does work though.

so somehow I'm ending back up in the diskII boot rom c600.. because intcx is 0 and I'm reading all EE's.
prodos called C311, which is EEEE..

slotc3 is 0. which means it's supposed to be internal slot 3 rom. and, as we discovered, that means also we should be pulling in the C3 and C8 internal ROM!

So for the IIe, we'll just say: 80 col card device MUST be in slot 3, no exceptions. Then we register a slot 3 c800 callback handler to also enable C8-CF internal ROM.

Progress!

So on a IIe, Total Replay detects joystick and 128k, but, hangs waiting on Vertical Blank.

So what issues now. OK, I don't have working flash or inverse? hm flash works, but not inverse. 

0x00-0x7F are inverse characters. 0x80 to 0xFF are normal. (The rom of course has pixels inverted).

This differs markedly from the Apple IIplus ROM. The II plus ROM has all pixels "normal". logic must make inverted. (And, the hi bit set indicates a flashing character I think). In my current 40 col code I manually invert 0-3F. So I am double-inverting. I need to leave the char data inverted for 0-3F. OK, that works for now. But having to manually process the rom differently depending on rom file and platform is kind of icky.

Mockingboard not working in the IIe.

[x] Bug: PAGE2 not working in IIe mode. 80ST is acting like it's always set in the monitor when I'm setting page 2.

There is lots of the firmware flipping between C055/C054 and 80STORE. However, when the monitor does get around to reading c055 for me, page2 does come on. ok..
The next C055 is back in the firmware. BUT 80Store is on! I had an extraneous "set video page to page 1" whenever I turned off 80store. huh. fixed.

I've patched the video routines to pull directly from video memory instead of two incorrect methods (asking page table, and using mmu->read). Now gets ram base and looks up video pages directly.

## Jun 26, 2025

I'm tempted to throw in 80-col text just for fun and to have something to show off in a video. And then might as well do dhgr, it's so easy. dlgr maybe too. Going to be bringing in Wm's code shortly.

## Jun 28, 2025

mb doesn't implement writing to T2CL/H. oops. let's see what uses it.. nothing that I can see. Well, I implemented on the basis of the doc and the T1 counter/interrupt. SHould work, maybe..

## Jun 29, 2025

went ahead and implemented hires and double hires methods for dpp. Quite simple. Also modified the instrumentation - displays the average per frame for the last 300 frames. color modes run around 250-300us, and monochrome mode runs around 100-110us. Remember this is for full frame redraw. Of course, text is very unsatisfactory in ntsc or in rgb. rgb will have to special-case for text. So how will we know whether to run the text or the color decoding routine? Well we can just check the linemode. I will also need to know if a line is 80 mode so as to pass in the phase offset and draw the left border pixels as needed.

Wm's code has its tendrils fairly deep in the code. I might want to reorg it, and it will take some time in any event. And in the meantime, use the dpp display code to be able to cut an Apple IIe with 80-col and dhgr.

So if we have 560 plus 7 on the left, I'm not sure how this will affect performance (if at all?). But test in dpp! 

OK, issues!
1. Rendering is supposed to be full frame because it is supposed to represent a monitor. However:
   1. in rgb, we want full text to be rendered using Mono and white. [fixed]
   1. in rgb, mixed mode we want graphics lines rendered using RGB and text lines rendered using Mono/white.
   1. in ntsc, we want full text to be rendered using mono and white; (otherwise all as ntsc, that's easy..) [fixed]
   1. but, the bottom text needs to be rendered Mono and white and we don't have that option since we only do full frames.
   1. will have to break it up.. or the Frame classes can have a color killer flag (or mode) per line, which the RGB module would respect and then just emit output like mono.
1. There are some defects in the RGB rendering in graphics. Stuff that should be white is colored; ladders in certain spots on Lode Runner are wrong color (should be white!).
1. I didn't implement flash; it just sets the whole line to inverse? [fixed]
1. inverse doesn't work right on iie.
1. in all modes there is a white block at the end of the first line (line 0). 
1. I think the text is like one scanline off. 40 column there is a empty scanline at top of chars; 80 column there is not. Investigate.
1. Wms cycle-accurate will have to generate its own output frame, and just not use my renderers, because of the same problem as in number 1-2 and 1-3 above.
1. I'm scrunching the 580 texture into 560 when video_system displays it.

ok, I tied flash state into a2_display. But the whole line is flashing. oops. ah it was not setting back to a default state for values of pixel_on and pixel_off.
I think that text code can be better. since we're just pushing 1 and 0, maybe we should just have:
bit <- data from the char rom
invert <- bit is set to 1
push(bit ^ invert )

Now. If we just redraw the entire screen every time, we can: get rid of the text and hgr shadow handlers; get rid of the thing that scans the text page for flash update.

Practically speaking I like my original RGB routine. But, this new RGB is definitely more like the IIgs. Wm's is going to work pretty good too.

OK, I'm a stone's throw from 80-column support now.

So on an NTSC display, whether the colorburst is present at the start of a scanline will control the entire line. So "per scanline-ish" color killer is accurate.
in RGB, that doesn't really apply. But we can use the same flag to determine whether to render using rgb algo or monochrome.

btw I'm still seeing the "window doesn't update right when we're close to screen edge". (also btw, I saw the same thing with chrome on amazon video)

I've got double lo-res in and tested now. Now I need to test double hi-res. What to do it with.. 

[x] I think I am missing a memory management soft-switch to reset on a RESET, esp with stuff that wants to do 80-col, dlgr, dhgr.  
[x] need vbl c019 support  
[x] open-apple and closed-apple  

OA-CA can go into the game controller. it can just call SDL to see if the appropriate modifiers are being pressed.

[x] Broke original Apple II model.  crashes in update_apple2_display.  

## Jul 1, 2025

working out what open apple and closed apple need to be. Right now I am using LGUI and RGUI. On my desktop Mac, this is ALT (which is a PC keyboard where I have personally reversed GUI and ALT keys because I like ALT better for "Command"). on my laptop, Command is LGUI. This means on a PC/Linux/Windows keyboard, it will not be the ALT keys, but the Windows keys. I might have to switch these checks to ALT on Linux/PC.

And, implemented in the game controller module.

So VBL. The most straightforward approach is to start ticking VBL right at the start of a cpu loop. Gemini suggests VBL period is 4,550 cycles long. "a horizontal scan" takes exactly 65 cycles. A vertical scan takes "exactly 17030 machine cycles". (This might be instructive for our main loop, 17030 instead of 17000..)

So for basic VBL (not doing cycle counting madness) just:
```
cpu->cycles % 17030 < 4550
```
let's just try math. Seems to work ok. Dunno about performance! This will be replaced with the videoscanner anyway, so this is fine for now.

nox archaist crashing: this is dying after boot screen and playing music. It turns on ALTZP then does an RTS, and the stack has garbage on it. nothing jumping out at me with the code right now, looks ok.. oh, it dies there in a2ts also. no problem then.

## Jul 2, 2025

the boot screen will display pre-packaged IIs. When you hover, you also get a text box somewhere towards the bottom that explains what's in that package. There are the built-in ones. You can also choose Load (to load a previously saved template), or Custom, to create a new template. I've got room for 8 tiles, if I use one of the spaces for the info box.

going to try keyboard capture along with mouse capture. Linux ctrl-win-reset seems to work. if I only hit windows, it goes to linux. so, that seems to be working ok.

Character set magic

This is a great reference:

https://retrocomputing.stackexchange.com/questions/8652/why-did-the-original-apple-e-have-two-sets-of-inverse-video-characters

in the Apple II, there are only 64 glyphs, repeated 4 times basically. (In the ROM we see them mostly repeat but with minor differences e.g. the flashing char area has the hi-bit set in the bytes). 0-3f inverse; 40-7f flash; 80-bf normal; c0-ff repeat of 80-bf.

On IIe, what we have is this in the alt character set: 0-1f inverse uppercase letters; 20-3f inverse symbols; (same as II);
40-5f: inverse uppercase letters, 

So what we need is a character map that is built, depending on character mode. ok, see Apple IIe Doc.

## Jul 3, 2025

ok I have a new template-based CPU implementation. It's all the same internal code, just wrapped up differently. First, it's a class. Claudio made a README.md for it. Short version, it uses template features and passing in static const "variables" that are resolved at compile-time when the 

Ideally, I move all the cpu_struct stuff into it so it has "local" access to all the registers. However, that would be a ginormous lift. Push that change until sometime later. Also, Wm code touches in CPU in a place or two. So, let's get his code integrated in before I implement these new CPU modules.

One thing, this approach will let me create 6502 instances with and without: trace, debugging, or the per-cycle video, and allow easily switching between CPU types at runtime. The '816 code will leverage this because an '816 has four modes; four combinations of A = 8/16 bit, XY = 8/16 bit.

What's wild: the cpu emulator with two cpus is only 90 kilobytes. C++ makes super-compact code. I love it.

But, can go ahead and implement 65c02 and test using the test harnesses.

Other modules that need to be touched to properly support additional CPU types:
* disassembler
* opcodes.hpp (for use in the big switch statement)

## Jul 5, 2025

making progress on 65c02! There are a couple odds and ends that shouldn't affect operation much (some cycle counting stuff.)

Wm hooks his stuff in via cpu_struct, so, switching out cpu->execute_next for cpu->cpun->execute_next won't be a bother. So, we're a stone's throw from the enhanced IIe.

disassembler is wrong for the BBS etc instructions (which aren't in my impl.)

ha! Booted appleworks 4.3 and it recognized enhanced //e, used mousetext. Other mousetext aware programs are doing the right stuff. I bet MouseWrite doesn't run on unenhanced //e.

Audit.dsk fails on iie :

after sequence 
```
LDA #$1D
STA $C007
STA $C00B
LDA $C300
STA $C006
want:
C100-c2ff: ?
c300-c3ff: ?
c400-c7ff: ?
c800-cffe: ?
got:
c100-c2ff: ?
c300-c3ff: ?
c400-c7ff: ?
c800-cffe: ROM
CXXX ROM test failed
zellyn.com/a2audit/V0#E000B
This is a the Cxxx-ROM check part of the auxiliary memory data-driven test (see E000A for a description of the other part). After a full reset, we perform a testdata-driven sequence of instructions. Finally we check which parts of Cxxx ROM seem to be visible. We check C100-C2FF, C300-C3FF, C400-C7FF (which should be the same as C100-C2FF), and C800-CFFE. For more details, see Understanding the Apple IIe, by James Fielding Sather, Pg 5-28.
```

[x] Fix bug shown by audit.dsk in regards to c800-cffe. Seems like on a C006 we should reset the C800 space to nothing (default).  

## Jul 6, 2025

back to MB - a french touch demo (DD) doesn't work right at all. On startup, it programs the ACR on both AY chips with value 0x40. This is "continuous interrupts" on T1 "each time T1 is loaded", and "timed interrupt T2". I don't implement any checking of things on the ACR register at all. It's possible some stuff needs it. This, however, seems to program with the way I support in code now.
Then it puts $C0 into C40E, and $40 into C48E. that's interrupt enable. So that is enabling INT 0x40 (T1) on chip 0 and disable T1 int on chip 1.
then it turns interrupts off. Then it turns interrupts back on.
This is after setting the timers, and storing $40 in both chip ACR registers.
then it waits forever for an interrupt that never comes. You know this thing has source. Let's look at it.
So this is T1 on chip 1. T1 interrupt is disabled on that chip. but we're looping waiting for the IFR flag to go high.
Bit 7 of IFR is 1, when "any enabled interrupt" is 1. But, T1, T2 interrupt flags SHOULD get set even if they are disabled.
If they are disabled, we just don't propagate that into Bit 7.
I think I'm not setting that 0x40 bit, ever, if interrupts are disabled. When I should be. i.e. the timer callback etc should happen regardless of whether interrupts are on or off.
```
            if (tc->ier.bits.timer1) {
                mb_d->event_timer->scheduleEvent(cpu_cycles + tc->t1_counter, mb_t1_timer_callback, 0x10000000 | (slot << 8) | chip , mb_d);
            } else {
                mb_d->event_timer->cancelEvents(0x10000000 | (slot << 8) | chip);
            }
```
So those stanzas should get rid of the if, and just have the first if clause. should always schedule. Similar check in the callback routines mb_t1_timer_callback. 
ok that makes the FT demo work, but, makes Ultima IV stop working! maybe. We probably need to set the timer callback every time we load the counter from the latch. found another place I needed to remove cancelEvent. Basically, we should never call cancelEvent. Ultima IV works again. Ultima V still doesn't work. Oh I wonder maybe it's trying to do voice? let's try MB-A for U5 then.. no dice.
Maybe we need to start timers going by default no matter what.. skyfox still working generally but problem of certain channels stopping remains. That could be a timer thing. Skyfox seems unaltered / unimproved. It misbehaves a little differently every time.

back to DD. 
[x] DD: there's a part where the screen goes into dhr mode back and forth but it's probably not intended. Might be using that annunciator-with-80col-off trick to turn off color delays and just change the color rendering, which I don't support (yet) (yes it was, and it works now).  

[x] uh, bne 08cb (to itself) on chp.dsk demo is causing very odd behavior in the debugger, cycles are going crazy (139841mhz!) and I can't pause execution. Wut. might want 2 MB. (assuming this was because I was erroneously halting the cpu on branch to itself)  

TLB2V12. 12 channel mockingboard music player. seems to work great!
TLB1 loads up but then fails to play anything, it's sitting in a JMP loop to itself.
CHP dies with a "KO" message. ok KO can be a number of things but these things DO check if they are a PAL system quite often. using the debugger I managed to trick chip. It plays some quite good songs on the mockinboard. 

## Jul 7, 2025

Got the audit to pass. oh weird. 

ProTerm still failing. ok. Proterm says "was not installed on this machine". Press return to verify hardware or esc to continue. It did this from 848: JSR $C100. C100 is 0, because we have a mockingboard in slot 1. And that then does badness. This will probably work if we put a serial card and its firmware into slot 1.

In selectsystem, have an enhanced iie option with "cycle accurate video". Add a systemconfig flag for that. And, create a system tile that is "make your own.."

## Jul 13, 2025

Some UI feedback:
people are regularly confused by the disk ii icons that show up at the bottom when disks are on, thinking those should be interactive. and folks are confused by the mouse capture, and how to get out.

some possibile treatments:
* make the mouse captured message stay on screen much longer;
* only do mouse capture on user request;

* make the disk ii icons at bottom of screen also be interactive? But then it won't be clear how to unmount a disk.
* Could have the disk ii icons open up the control panel.
* scratch the control panel altogether and just have hover-activated controls for everything.

* short and sweet how-to instructions on first bootup, or, prominent help menu

* also suggestion to pause the emulator when the window is minimized.

``
[x] Bug:
full-screen
click in another window on 2nd monitor, causes app to minimize
un-minimize
exit full-screen
there is no title bar on window
full-screen and exit full-screen again - the window has a title bar again.

ok that was me.
``

So, action items

1. Do a round of UI improvements
1. implement Wms code
1. bring in the enhanced speaker code
1. Support .woz format?
1. Choose between //c or GS as next stage.
1. need to support the mouse card.

I am feeling the bug for GS work. Will require a 65816 core.

## Jul 15, 2025

steps towards implement Wms code:
1. eliminate all the old commented-out display code. Keep my RGB routines (somewhere) in case I decide to go back to those. [done]
1. test performance eliminating the "display update" cachey stuff, and see what performance is just updating every frame 100%.

There is a bug in the RGB code exhibited in (for example) Choplifter. The counter digits have inappropriate color pixels when transitioning in certain cases.
Maybe, black white black? Almost everything else is really good.
counter>2 makes it worse.
counter>3 is 99% correct. There must be some other issue..

patterns:
110111 (3b)    111100111111 000...
111111 (3f)    111111111111 000...
111011 (37)    111111001111 000...

it's the last one, 111011, that gets hosed. That is a six-bit pattern.. it's generating a whole extra pixel compared to the other patterns. 

## Jul 17, 2025

So, the lookup table for the VideoScanner currently includes tables for:
* lores
* hires
* mixed

Plus versions based on the selected page. This does not discriminate based on 80-col mode, presumably he just uses the same address but in the aux bank. This is a 16-bit address; this will be added to mmu memory base to fetch bytes.
If a cycle is in hbl or vbl then the address is 0 and we don't fetch any video memory there and just return fast.
In addition to address, put in the hcount and vcount. And maybe we store the full 64-bit pointer so we don't have to do any math or get_memory_base() calls. We want video_cycle to be hyper-efficient.

Videx is a completely different video system, so leave it as-is to generate a FrameRGBA directly.

So what we want to do is, in any call to video_cycle:

get current "video cycle index" - this is a number from 0 to 262*65-1 (17,030 - 1)
lookup current mode
fetch video memory byte based on LUT and video cycle index
fetch video memory aux byte also (if iie)
emit mode/vidbyte/vidbyte tuple into a FrameVid567 frame buffer.

Wm uses hcount/vcount. but let's bake all that into just the cycle index.

Have a VideoScanner call that converts display record settings to a simple mode index.

Then we will have a Generator that is VideoScanner. This will look like Wm code, with a big switch statement to process the video byte bits based on the video mode in each datum.

It's 7 pixels per cycle in regular mode, and 14 pixels per cycle in 80-col mode. When mode is changed, we start emitting the new mode in the tuples, and that will trigger different switch case. This is purely to generate the bit stream! Which I think is different from current code.

Then the Bitstream will be rendered by whatever current backend rendering engine is selected.

Process: implement as much as possible as-is. Then start hammering on optimizations. So:
* start with current video_cycle implementation (except use a FrameVid).
* change to cycle instead of hcount/vcount.

OK! I have a test harness "vpp" for this. I implemented lores40 and hires40 so far. (options 3 and 5 in vpp). n and m work. r doesn't, it's rendering everything as monochrome because I'm not setting the line mode flags, which I should do.. ok that's easily done. hang a sec. ok, done. This seems to be working pretty well so far. I also simulated a "change mode every 4 cycles" thing. maybe do 5.. 

So, need to implement the other video modes. I am wondering if mode selection should be based on flags instead of just values. How many toggles do we have..

* shr = 1 / 0
* 320/640 = 1/0
* text / graphics
* hires / lores
* page 1 / page 2
* 40col / 80col
* altcharset 0/1
* unused

Any given video byte's handling is defined by one of those. now what about split txt/graph - ..

40/80 doesn't matter. this is always the same byte, just in aux+mem but at same address.
text/lores: 0-191 point to 0x400
hires: 0-191 point to 0x2000
I think video_cycle should handle the split screen. because in split, 0-159 is mode and 160-191 is text. it can just put the right token in there.
I whittled the list down. 

## Jul 18, 2025

So the VideoScanner starting to come together. have done lores, hires, dhires. still need dlores and the text modes. 

DHGR frame time is 285-295us. Same for hires, and lores. So they're all going to be pretty consistent. Maybe hires takes a hair longer. 
Mono takes 150us. Makes sense. RGB - about the same as ntsc. 
Now once we have a full frame we need to ensure we don't try to shove more data into it. i.e. we need to run our loop for exactly 17,030 cycles.
What happens if we go over a few, as is likely to happen inside an instruction - let's say worst-case is 7 or 8 cycles. video_cycle will be called maybe 7 times in the hbl and/or vbl. I think that's alright. In higher speed modes, however, video_cycle is going to have to count only 1mhz clocks, and be de-synchronized from the cpu clock. I think we can count this with a uint64_t.

The video_byte needs to be readable from many different places. I am thinking maybe this can store the video_byte in the mmu. Then have an mmu method "read_floating_bus()" that will do the thing. (and when we're not using VideoScanner, that routine can just return 0xEE, which it will be set to by default.

ALT char set - I kind of want the ROM to be passed not to Scanner, but to Generator. Remember the goal of the scanner is just to get the video bytes into a buffer as fast as possible, and not to process the data (which is timeconsuming). So, Scanner needs to tell Generator what the alt char set mode is at the time the byte was read. We have another whole byte of data we can use here. Let's use bit 0 (i.e., value of 1/0) to store the alt flag. That said, that also leans again towards: the mode byte is a bit field, not a value. Or, maybe have it be a value, but, -also- have the bit field. 
So the Scanner set_video_mode will also calculate the flags field.
ok, that's working. got altcharset support in. So what's the other thing I wanted in here.. the mixed mode I believe?
ok, mixed mode and 80col both needed to be passed in. because in mixed mode, text could be in 40col or in 80col.
ok, put in lgr80.

## Jul 29, 2025

post-vaca.

So, things to do to continue integrating the cycle-accurate-video.

[x] hook in to incr_cycles  
[x] hook in to update_display  
[x] hook in the floating bus read (this will be somewhat extensive)  

This should get the Sather demos working.

Those are the first bits to figure out. Then:

[-] alternate VBL implementation based on the LUT (this was done by calling is_vbl) 


## Jul 31, 2025

the first thing needed here, is to hook in the softswitches. So I need to change how the softswitches are tracked. Would it make sense to have the code read values from display_state_t ? Is everything I need in there?

VideoScannerIIe needs to know the softswitch settings. So, feed it the display_t record when instantiating.
The code runs set_video_mode every time there's a softswitch change. I don't have an analogue today in display.cpp for that. set_video_mode is setting the following:
* video_addresses - video_cycle
* video_mode - 
* flags

running into coupling issues. maybe I should create a separate struct for just the state, and not all the other stuff? or, use a Message? But the key thing is exposing only the fields that need exposed, in order to reduce coupling between modules.

## Aug 3, 2025

Are ya gonna do it already?
instead of all these IF statements, what if I generated (and maintained) a bitfield and used that as a LUT index? Our values are:

TEXT/GRAF
LORES/HIRES
PAGE1/PAGE2
40COL/80COL
SNGRES/DBLRES
FULL/MIXED

Wm code also vectors on 80STORE and chooses page1 vs page2 addresses, but I don't think that's right. I don't think 80STORE affects the video scanner, just the CPU interface to AUX memory. Well, if I'm wrong can add another flag later.

So this table would be these 6 bits (64 values), and each entry the LUT address for the scanner values, and, the mode identifier (e.g., VM_TEXT80, VM_TEXT40, etc.)

    display_mode_t display_mode; // text / graf
    display_graphics_mode_t display_graphics_mode; // lores / hires
    display_page_number_t display_page_num; // page1 / page2
    bool f_80col = false; // 40col / 80col
    bool f_double_graphics = true; // normal / dblres
    display_split_mode_t display_split_mode; // full / mixed

    display_page_t *display_page_table;
    bool f_altcharset = false;
    uint64_t vbl_cycle_count = 0;

    bool flash_state;
    int flash_counter;

display_state_t is what handles all normal Apple II graphics modes. e.g., Videx is a separate device. The concept here is -device-.
But, should this be broken into a device and some sub-components?

OR, maybe the VideoScanner approach should be its own display device, instead of display.cpp.

Then I'm going to have to replicate all the softswitch handling etc. ick.

VideoScanner's logic can be used to drive floating read and vbl regardless of whether we're using the full video scanner (i.e., call a variant of video_cycle where we pass in the cycle index ). So, we will want a scanner in here no matter what to help with those functions even if we don't use it to generate frames.

So how to I tell the system to use cycle-accurate scanner vs frame-based? Call a different init routine that each calls a common setup routine.
video_state_init creates appropriate VideoScanner (in either event) and installs in cpu if there is intent to use. Sets a flag in video_state.

OK, got that - and I've made a few tweaks to hook in video_cycle, and, to tell mmu what the floating bus value is, and, to then return that value from mmu and a few other modules when appropriate. 

SUCCESS!!!

Well, 98%. in Crazy Cycles, there is the occasional artifact. I suspect it is because of mismatch in VBL. (17008 cycles vs 17030 cycles, and every now and then it is out of sync in wrong place and draws wrong stuff?) Will work on that.

Action Items:
[x] Artifacts in Crazy Cycles  
[x] color killer in text mode not working  
[x] RGB not working  
[x] Hunt down all the places where we need to call floating_bus_read()  
[x] Allow optional selection of cycle-accurate video on VM init (default to on for speeds other than LS)  
[x] Apple II+ crashses on startup  
[-] SPLIT DEMO has mixed text mode sometimes. it is not setting C052 so we might need to reset that switch on a RESET/powerup. (I think this is just a split demo bug)  
[ ] disable invalid modes (no 40 col text in double graf modes)  
[x] color kill lines 160>> in rgb.  
[x] at end of 80col lines we need to emit some blank pixels  
[x] Double hires not working  
[x] 80col not working  
[x] double lo-res is rendering as mono i.e. no color processing  
[x] alternate character set is not working  

Invalid modes: double lores + 40 col text; double hires + 40 col text (mixed modes); only valid is hires + 80 col text.

So, demos that take advantage of this:
* Crazy Cycles
* Crazy Cycles 2
* DD (maybe?)
* Fireworks
* Sather
* Apple II Split Screen Demo

Ludicrous speed is now down to 215MHz. partly because we are doing the frame scan every fast cycle too instead of only on slow cycles. Maybe the thing to do is, automatically disable cycle-accurate when we're in Ludicrous speed. <-- this is probably good

### Color killer

let's see what split screen looks like in OE. It's got fringed text. So where should color killer be applied.. let's say, let's try having color killer set based on what mode is set AT THE START OF EACH SCANLINE. Google insists a monitor would switch to monochrome extremely quickly with loss of a color burst.

Well, that's a thing.. it does not quite do the right thing in for instance split.dsk. It's less wrong in crazycycles.

### Artifacts

the artifact slowly scans up the display from bottom to top. It's almost certainly the 17008 vs 17030 thing. Hm. Well let's try 17030. That causes speaker distortion, and an artifact that is at the same place every frame. eeenteresting. 

### Apple II Crashes

was not creating vs and setting mmu etc., and also MMU_II does not have enough RAM. Corrected.

### Mixed mode color

if text mode (text40 or text80), all lines are colorburst off.
If any graphics mode, all lines are colorburst on.

if mixed mode and NTSC, all lines are colorburst on.
If mixed mode and RGB, 0-159 are colorburst on, 160-191 are colorburst off.

so this implies that we need to have special handling in RGB for this last case. OR, in ntsc mode, only check first scanline for colorburst on/off for the frame.

## Aug 18, 2025

Looking at issue ".2mg" file can't mount successfully to Disk II. Indeed, the pdblock2 code properly deals with the data offset etc. and the Disk II code does not.

This should be an easy tweak.. yes, but it's showing some weakness.

Everything wants to deal with straight normal blocks/sectors. Only Disk II deals with nibblized (but we want to handle nibblized for conversion purposes, and disk ii wants it also)

class Media
    new Media(filename)
    new Media(types, filename, etc)
    read(offset, length) - applies any data offset that might be in the file
    write(offset, length) - applies any data offset that might be in the file
    

class NibblizedImage
    new NibblizedImage(Media) - create and load nibble data based on the specified Media file.

## Aug 23, 2025

took a look at roadmap, and see the new more efficient speaker code on the list. Hunh! ok. So Mike's A2DVI CPU is either not FP efficient or it doesn't have FP. (I forget which). So, a Fixed point only solution could be beneficial for that project, as well as for what I'm doing here.

Mike's project would require 44,100hz audio. Mike's approach is sampling at a consistent 380kish rate.

What I have been contemplating is recording events by time, not by cycle, because the cycle times could change (and if I wanted to super-accurately simulate the 'stretched 65th cycle' I would). (The mockingboard code records events by time, though it does it statically by dividing cycles by a 1.0205mhz rate. Which I need to change to 1.0219 instead since the change to the cycle-accurate video.)

So is fixpt the best representation here? What about 64-bit "whole seconds" and a separate 64-bit "fractional seconds"? Then I could record accurate times to arbitrarily small portions of a second, good for cycle times.

Then the question becomes, translating those into 1/44100ths.

For GS2, it's likely I could just pass raw very-high-frequency data into SDL3 and have it do the sample rate conversion.

this is a productive chat:
https://claude.ai/chat/9accfc75-7217-44ff-97c7-268b62723480

the current cs code is extremely efficient but does not have filtering in it. But, going for the pure fixed-point approach might be worth a try, since the code in the chat builds in the filtering via modeling the speaker physics.

A negative side-effect of the current code is that it takes more cpu time if the cycle count increases (at 2mhz execution goes from 4us per frame to 10us per frame).  The faster the emulated cpu the more time this speaker code will take. 23us at 4mhz). 

An alternative approach entirely: 'sample' every time the cpu ticks a cycle. Would basically be like Mike's approach (except he's not filtering to model the speaker physics).

ok, I'm done noodling around here. I think the current code is ok. However, if we're going to forklift this, there should be some clear benefit from it. That benefit should be to be able to have the cpu cycles each individually taking different amounts of time. I.e., recording speaker click events based on emulated realtime, instead of cycles. Secondarily, fixing the number of samples so this algorithm does not take more time if the emCPU is going faster.

## Aug 29, 2025

[x] in normal hires, if you hit $C05E (double hires related) without turning on 80-column, this is supposed to disable the color delay (i.e., ignore bit 7 of each hires byte).  
[x] the mouse VBL interrupt is definitely in the wrong place compared to my video routines. mouse disappears in shufflepuck in roughly same place as in dazzledraw.  Going to have to 

[x] when in dhr and switch to text, c054/c055 should go back to controlling display page not memory. Spacequest is switching from dhr to text40 and we are not selecting page 1 when we do that (I was wrong, with 80store on, displays are forced to page 1) 

Skyfox audio dropping out might just be how it works. it seems to behave the same way in Mariani. Trying to get a real MB from a friend.

## Sep 4, 2025

* Altering the Emulation to not use Float for event timekeeping

At a certain point we will start to lose resolution as we increment the event counter.
Also, this doesn't work at faster speeds. So the solution is to track a nanosecond counter in a uint64_t.
uint64_t can hold 580 years worth of nanoseconds, so this won't lose precision and it won't ever wrap.

Places that need to change:
debug_register_change
RegisterEvent::timestamp
MockingboardEmulator::queueRegisterChange
processChipCycle
processEnvelope
generateSamples
variables: current_time, time_accumulator, envelope_time_accumulator
inside the 

Let's just add tracking the realtime, display in debugger, and see what happens.
Works ok except ludicrous speed where I run into divide by zero issues. What if ludicrous speed is instead a fixed rate? that will cause issues with different speed PCs. I could measure it before and after every instruction. if I try to just take average I'll have the whole first frame with no data.
what if I assume current speed for first iteration of "subloop". In LS we run some number of 17000 cycle iterations until we've done 1/60th second of frame. So maybe this:
* when enter LS, set 5ns cycle time.
* after each subloop iteration, calculate actual time.
* reset base time to actual time.

ok I'm runnin ga test. 3.1194 is the current clock skew. Let's see how it changes over time.
3.1195.. 3.1196.. we're ticking up ever so slightly.

Interesting so my realtime is increasing compared to my etime, which means I'm not executing enough cycles. Probably because of that +1 in the other cycle_ns calculation, and ending the loops based on cycles instead of time. ok so right now we're ten microseconds off per second or so.

Mini-roadmap for this:
* time the cpu loop based on this instead of cycles.
* set cycle rate for 1mhz to 1023xxx. Then the effective MHz calculated should come out to 1020500.
* add in handling the stretched 65th cycle, ensure synced to video scanner.

Maybe the solution for ludicrous speed is to just somehow switch to realtime instead of emulated time, and to set emulated time to realtime when we switch from LS back to an emulated speed. (at 4mhz we have even divisors and the clock stays much more steady).

ok, I sort of have this. And I am getting a variable number of cpu cycles per frame, which isn't exactly right. Also, they're between 17020 and 17028 (at least according to cpu_delta calculated in speaker). We should be aiming for 17030 exactly?
however, the frames per second is 59.94, so.. It's actually 59.923. Once I do that, the average cpu_delta gets up to about 17030 cycles.

Consideration: the cycle accurate video requires exactly 17,030 cycles of data per frame. (technically 12,480 outside the vbl).

Let's focus for now on getting exactly 1020484 cycles per second effective rate. For some reason I keep "sticking" at 1.0217mhz.

## Sep 5, 2025

Apple II clocking is complex. That's all I got to say!

Laying down some facts for consideration.

| Item | Duration | Notes |
|--|--|--|
| 1 video frame |  17,030 cycles exactly (262 lines, 65 cycles per line) | |
| Frames per second: 59.923 | |
| CPU Clock Rate | 1.022727MHz | 14M / 14 |
| Normal Cycle duration | 977.7779019ns | |
| 14M Freq | 14.318180MHz | |
| 14M Duration | 69.84127871ns | |
| Long Cycle duration | 1117.460459ns | 977.777 + 69.841 + 69.841 - total of 16 14Ms |

The 14M clock is the master clock signal. All other signals are derives from this master clock. 

Wrote a test program in testdev/clocking; purpose is to explore granularity of different fixed point representations.

ok. So instead of trying to track all of these in decimal fractions, what if we track them in integer? And instead of tracking the 1.0xx mhz as a base, why not represent the actual Apple II system base clock?

i.e., have a F_14M counter. Have a F_1M counter, but it's units are in terms of the 14M counter. e.g., counting "1M" cycles looks like this: 0, 14, 28, 42, ... 896, 912
The last increment was by 16 (i.e., normal 14 units of 14M plus two more).

In a IIgs, the cpu clock is constantly changing but it's doing so in reference to a similar setup - e.g., a 28MHz clock. Then our counters are:
1) wholly integer
2) very high precision (our 'fraction' part on the 1M clock is only divided by 14, so we can still count up to 580/14 = 41 years of emulated time).
3) if we ever need to calculate the current realtime in nanoseconds or microseconds it's just counter * nanoseconds per 14M. We will, when we determine how long to sleep in the main execution loop.

So with this, one frame is: 262 scanlines; each scanline is 64 * 14 plus 1 * 16, or 912 14M clocks precisely; 238,944 ticks of 14M for a frame. 
Times 69.84127871ns/14M, and we get : 16,688,154.5000 microseconds per frame. Go into one second, and we get 59.9227434 frames per second.
So this is how we want to time cpu clocks in the main execution routine; 
The frame delay/sleep routine has a very nice round number above (16,688,154.5) to time on.

## Sep 7, 2025

The new timing system is in - we are locked tight to 59.923 frames per second, and, 1020484 cycles per second!

So for standard 1MHz Apple II, the clocking is working well.

Audio was a mess, so I spent some time massaging the "new" speaker code to drop in. That is shall we say mostly working, though there is something causing an annoying click sound. Either something is wrong with the event buffer, or I'm underrunning like a mofo.

The latter appears to be the case. I am always generating 735 samples, and doing it 59.92 times a second, so I am not generating enough data per second. I think I'm short about 59 bytes per second.
And aside from that, I am now getting a super high pitched whine. That would imply super rapid polarity cycling which means something in event buffer isn't workin right..

There are things that can cause the audio to desynchronize. Now I'm wondering if I should have it call me as a callback.. also, I need to add filtering. But so far the audio_time to generate one audio frame is significantly lower than it was before, usually 2us-4us sometimes 10-12us.

cs4 is the latest test harness iteration that shares code modules with speaker.cpp.

ok, trying to change output rate to 44042.. it's fine at the start. But then it gets progressively more distorted.

it's got to be something related to the way I pull data out of the queue. (or feed it into the queue?)

## Sep 9, 2025

Progress! The relationship between Frames per second (59.923), output rate, and samples per frame need to result in samples per frame and output rate being absolutely as close to whole numbers as possible. I am currently using: 59.923, output rate = 44343.0f, samples per frame = 740. 

The issue is we were not quite generating enough samples per frame, so it would eventually underrun, when the relation was not whole numbers.

1) We still have the occasional underrun issue when the host system slows down the emulation (clicking between windows, etc.) I solved this before by adding samples to ensure there is a reasonable buffer in this sort of event. In the older audio code I just repeated the last sample. We can just return more samples than requested.
2) we also still need to add a low-pass filter to the stream.

I modified the volume to emit samples of only 5120 (instead of 32768, or 16384). And it's still more or less full volume on my Mac. Someone in the audio chain must be 

So I should revisit the Mockingboard audio generation and mixing and see what I get there - perhaps I should just emit and add relatively low-volume signals, instead of adding and averaging. I.e., let components further down the stream do AGC which seems to be what's happening. Just make sure I don't overflow before emission in a given output channel. (I have two?)

[x] Need to add back support for the faster CPU speeds. part done. have everything except true free-run.

[-] I should also completely pause the emulation when we open the file dialog. I should be doing that in the main event loop so just setting a halt flag to continue out of the loop except for event handling ought to work. (I use catchup for now instead)  
[ ] there is a request to do the same when the emulator window is minimized.  
[ ] I think amplitude between noise and actual mockingboard output is imbalanced. (if tone and noise on same channel it might get inappropriately louder..)  

Faster cpu speeds: I don't think the cpu speed is directly tied to the output generation. Testing cs4 with 2.8mhz. Whups, that sounds like garbage. Increased buffer space.. no dice, though, I think that is potentially an issue. I tried doing exactly 2x 1020484 and there is some accumulated error. 
Again, it wants the cycle/sec rate to be as close to an exact multiple of the frame cycle rate as possible. So, 59.923 * 34060 = 2040978 instead of the exact double of 2040968.

Right now the question is am I correctly handling some fraction - and what is the fraction causing problems? And why doesn't it cause problems in cs2? I can put arbitrary stuff in cs2 and it's fine.

ok I tried something close to 2.8, 2756458.0f, based on spreadsheet calculations. That failed. However, 2756460.0f worked.
the first was a fraction under 46000 cycles per frame. The latter a fraction -over- 46000 cycles per frame.
I think the issue is:
if there is fractional cycles per frame;
if it's under .0, like .999, then we are losing most of a cycle's worth of contribution.
If it's a hair over .0, then we are losing almost none of a cycle's worth of contribution.
cs2 uses float cycles_per_frame, but, does not use cycles_per_frame anywhere important.
cs4 uses float cycles_per_frame but uses it to determine how many cycles to iterate through the loop, which is integer cycles, so we have an error that grows across time until it pops back close to the next integer cycle. So we're either missing or replaying data or something.
So we need to use exactly whole cycles per frame - this is another constraint. Or we somehow need to keep track of contribution across a fractional cycle.

Would reworking this to be based on time instead of cycle-based help this situation? 

This first concept is, decoupling speaker audio generation from cycles. This would allow us to keep cycle-awareness in the softswitch handler. On a tick, it would convert 14M to say nanoseconds. Then the speaker code is independent, and converts nanoseconds (or some fraction thereof) to 

In-emulator, we also have a situation where if something causes me to have a slow frame, the audio can fall behind realtime. Speaker.cpp has cycle_index which I bet is far behind real cycles now. 


also getting hangs occasionally:
```
Terminating Process:   wezterm-gui [486]

Thread 0 Crashed::  Dispatch queue: com.apple.main-thread
0   GSSquared                     	       0x104ed0dac Speaker::generate_buffer_int(short*, unsigned long long) + 332
1   GSSquared                     	       0x104ed0dc8 Speaker::generate_buffer_int(short*, unsigned long long) + 360
2   GSSquared                     	       0x104ecfaec audio_generate_frame(cpu_state*, unsigned long long, unsigned long long) + 132
3   GSSquared                     	       0x104ea5e2c run_cpus(computer_t*) + 532
4   GSSquared                     	       0x104ea73f8 main + 3656
5   dyld                          	       0x1905f6b98 start + 6076
```

ok I asked Claude and it concurred, and added a bit of code to track fractional cycles in the main loop as well as in the Speaker.cpp code. That seems to resolve the issue there. However, we still need for the output_rate / samples_per_frame to be as close to the frame_rate as possible. 44100/736 is 59.918 which causes artifacts.

I wonder if one more fractionalization adjustment would help. And, is there some benefit to having 

## Sep 11, 2025

ok, I was radically increasing the end_frame_14m count because in single step mode I would add a full frame's worth ever iteration through loop, even if I was only single-stepping instructions. This caused the main loop to execute instructions for a long long time. 
Audio gets fuzzy after single-step mode tho. Suspend normal audio generation during step - this is likely causing underflows or something (every time we do a step we're doing an audio frame and the counters get all out of whack). Or, perform some kind of forced-sync.

Instead of lots of IFs everywhere on each component of the event loop.. maybe I should just have a different event loop for each mode. Modes:
standard mode with fixed speed (1.0, 2.8, 4.0mhz - run based on 14M clock cycles)
free-run mode (here we)
step-wise mode

Then it's easier to execute just the right functions. 

Boy there's a lot of commented-out old stuff, unused variables, etc etc esp in the gs2 main loop and cpu etc. Cleaning some of that up a bit.

## Sep 12, 2025

### Standard Mode with fixed speed

execute all the per-frame stuff every frame, like we have now.

### Debugger Single Step Mode

Disable Audio
Run old video update instead of scanner. However, we are still accumulating ScanBuffer data and if we have accumulated 7,680 or however many bytes it is, then run VideoScanGenerator to refresh screen and empty it out. Using the old video update means we'll see changes as they happen. It might also be cool to be able to spool out video byte updates as they happen in VideoScanner mode, you could debug cycle-accurate code that way.. hmm!

### Free run mode

use original 60fps video routines.
disable audio



so for all the stuff that I track (e.g. the event_time, audio_time, display_time, app_event_time) etc let's call this an instrumentation. And let's create a class for it. Instrumentation will record the last 60 samples worth of data, then give us methods for min, max, average whenever we call it. Pretty easily done as a circular buffer of depth 60. Since we rarely need the figures, calculate them on demand. Should take fractions of microseconds.

class Meter {}

we need to be able to switch modes between frames. So, call a func based on mode. OK that refactor is done. The 1MHz standard-mode loop is pretty clean now.

Now, build back in 2.8MHz and 4MHz modes. 2.8 is 14.318/5, but, 4MHz is .. what, exactly? Start with 2.8 anyway.
Have to consider how a GS @ 2.8mhz syncs to video. I suppose in ntsc we still have to sync at the end of every scanline but how many 14M cycles is that, exactly?
ok once I go into faster mode, audio stops tracking at the correct speed. So I need to check the cpu speed settings and reconfigure the Speaker.

ok, now tracking the cpu speed with audio speed, it starts out right but then gets off-kilter. 


In-emulator, I really need cycles_per_frame to be a whole number. it is input_rate / frame_rate.
2857355
59.923
47683.777 -> should be exactly 47684

I use the "mixed effective clock speed" for cycles_per_frame. 2,857,368.332. So bump that up a hair so we're a bit over 47684. Make it 2'857'370.

ok, I made a spreadsheet to calculate the correct figures, for 1, 2.8,7mhz, and 14mhz. I hadn't updated incr_cycles to use the right extra clock cycle count.

There was one spot where I switched to a fast mode and back and audio was all messed up. But now I'm testing with Music Disk going back and forth between different speeds and it's all groovy.

Definitely still getting the issue where delays caused by opening the file dialog cause audio to get out of sync. So it's Speaker::cycle_index. Could try resetting this to current cpu cycle on a reset.
And should have an easy way to dump it and its skew at any particular moment. (is it this? or do we somewhere jam in a ton of extra samples so we're running behind playing?)

[ ] resolve issue where speaker output can get delayed  

OK, this is passable. Shall we now see about making the cycle-accurate video correct?

video buffer that can hold around 32768 video samples. has to be more than 17030.
Each VideoGenerator pass we will pull exactly 17030 samples out of it.
Sometimes the cpu exec logic will use a little more than 17030 cycles; sometimes a little less - by a little, I mean, 0-7 cycles more or less (because cpu execution can't split an instruction).
Right now the buffer that holds our scanner samples is:
FrameScan560 *frame_scan = nullptr;
That's a Scan_t.
ok, created a new ScanBuffer class.
We had a thing where we could store flags in the FrameBuffer. For instance, set_color_mode, which is colorbust on or off. pretty sure this should be sampled per scanline at the end of the video data, or end of the hbl. But, we need to include the value of that in the flags in each sample.

in single step mode, we get an error "less than a full frame in ScanBuffer". This is because, yes, of course there is. We could tweak VideoScanGenerator to generate X cycles worth of new video from last position and then return. So as we single-step through the code we will be updating small bits of the video.

[x] Finally, modify to only grab video samples from RAM based on the 14M clock.   

also, a branch (e.g. bne) to itself causes the emulator to lock up in single step mode.

800: lda #1
802: cmp #0
804: bne $804

shows this..
uhh, I had this in there still:
```
if ((oaddr-2) == taddr) { // this test should back up 2 bytes.. so the infinite branch is actually bxx FE.
  fprintf(stdout, "JUMP TO SELF INFINITE LOOP Branch $%04X -> %01X\n", taddr, condition);
  cpu->halt = HLT_INSTRUCTION;
}
```
commented this out. This was dumb. ha.
[x] HLT other than the one should exit the run loop. no, just eliminate all other halt.

shufflepuck still has vbl issues because the mouse vbl is not synced with display.

ok, now I need to only generate 17,030 video entries per frame, otherwise, when we switch to faster clock rates, the video scanner gets desynchronized. Ludicrous speed works because it falls back to the other video system.
track in incr_cycles, or in video_cycle.
We need 14 14Ms to elapse before we grab the next video byte.

[x] in RGB mode, split graphics/text text is being color decoded, instead of showing as white. Man I keep regressing this. lol  

now set a mixed_mode flag along with color mode in the per-scanline color_mode in frame.

## Sep 13, 2025

ok so I switched from 1 to 2.8mhz, and triggered the video desync issue. Right now, there are 41252 entries in the Scanbuf. That's not right. We should be getting only 7,680 samples per frame. So what happened here.. oh, my cpu struct is wrong. oops.
So we are typically not getting more than 7680 samples in the buffer, because the handful of cycles plus or minus, are occurring during blanking typically (though not always)
Current incr_cycles routine is not correct.
ok there are exactly 17,030 (65 * 262) CPU cycles per frame. Each of these CPU cycles matches one video byte read.

ok my issue here was the logic around tracking the cycle overrun or underrun each frame loop was wrong. Now I just keep track of the 14M count we're supposed to have at the end of each video frame. I was causing oscillation around that value unnecessarily. Now, when running one of the cycle counting demos, we lock in on the exact same h/v location at the end of each frame as reported by osd. And the number of excess bytes in the video buffer is always 0. So this is spot on for 1mhz. Now let's see what happens at 2.8. ha ha.

Now I -could- skip trying to support video_scanner stuff when the speed is ANY speed except 1mhz. but that may not work with iigs stuff. OK, keep pressing onward.

So how do I still get exactly 17,030 video scanner calls per frame even when we're at higher speed.
OK, we will always emit bytes on the same 14M cycle boundaries per scanline. i.e., bytes emitted at 14, 28, etc. 
and on the last cycle of a scanline, just emit and reset the counter.
0, 14, 28, etc etc then whatever.
that is working now for 1m and 2.8m. not for 7m. Must have something wrong in my counts/setups.

[x] when using cycle-accurate video, disable the old call to update_flash; also look into removing the shadow setups. (though these won't need shadow, and it might not matter that much, it should probably improve performance a lot at 7mhz..)
[ ] also evaluate whether shadow + optimized is better than just doing whole frames.

So I still need to re-implement ludicrous speed, which is going to be differences in the main event loop.
[x] reimplement ludicrous speed.  


Of course, this now opens the door to support PAL video timing. 

ok single step is working. Check breakpoints.. when I bp, Scanbuf has some random # of bytes in it. Then if I resume, it's as if we forget to update the screen afterward.. of course I need to call the old videogen.

oh duh I do have a routine that switches video backends and disables shadow. it's display_update_video_scanner. I just added a bit to it to also check step mode. So when we switch into or out of step mode we need to run that. it's run automatically at the end of each video frame update. So we should switch into it in single-step.. yes. but When I exit step mode the video is out of sync.

Ludicrous speed coming along, but, you cannot shift back into normal speed(s). it locks up. Probably need to reset the 14M counters that the other methods use to gauge frames. Also we've lost like 150MHz since then because incr_cycles used to be cycles++ and now it's all this horrendous stuff. Maybe do this:

```
if (ludicrous) cycles++;
else slow_incr_cycles();
```

This may actually be better from a cache perspective.. 

[x] in cycle-accurate video mode, videx never consumes any data, then video is out of sync when exit videx mode.  

## Sep 14, 2025

Mouse velocity. It looks like Shufflepuck on a real apple he moves about 1.5 to 2 inches to go horizontally.
I tried dividing the movement update by 8, however, this loses precision - slow precise movements end up not allowing the mouse to move at all.
So we need to track at higher resolution, and then scale down (or use floating point).

was playing a game and ran into some kind of infinite loop in the Speaker code. I think I've seen this a few times before. I think I found an inappropriate cast.
How many cycles is 4,913,136,230 .. 
4814 seconds.

Yeah I think that fixed the problem. that's a pretty radical issue though, once the counter exceeded int (32 bit?) 

[x] mouse vbl is hardcoded for 1mhz - improve the code to run at other clock speeds (vbl stops working after higher speeds). test running glider after high speed. 
[-] ON A Restart (i.e. close vm and start new vm) the joystick sometimes isn't recognized. I have code in for this, is it not working right? recompute_gamepads. (I think it's working?)  

## Sep 15, 2025

compare current speaker code to previous speaker code, and look at how I previously handled buffer sync etc. There are a couple edge conditions I probably need to handle.

[x] Remove vbl_cycle_count from display, since I don't use that any more. it just calls is_vbl.

For the mouse issue, I need to calculate the next vbl from the VideoScanner. and ask the VideoScanner for it. And if the scanner isn't active (ludicrous speed) don't activate that timer. ok, to test this: shufflepuck, verify vbl working. Then speed up and slow down, should still be working. however, not working at higher speed. looks like it's still using 12480 cycles for that.

[x] when speed-shifting, we end up with 80 excess (sometimes other) unused bytes in the ScanBuffer.  

I think we have a problem if we schedule events for before the current cpu cycle esp if we keep doing it.

when leaving ludicrous speed for a slower mode, c_14m and end_frame_c_14m are wildly out of sync.
            // this gets wildly out of sync because we're not actually executing this many cycles in the loop,
            // because we are basing loop on time. So, maybe loop should be based on cycles per below after all,
            // while just periodically doing the frame update stuff here.
oh duh:

    inline void incr_cycles() {
        if (clock_mode == CLOCK_FREE_RUN) cycles++;

So if I do it like I did before I have to calculate some fake whole number of 14m frame counts, based on the -time-. which makes some sense - 14m is a faster clock but not as fast as our 300mhz pseudo-cpu. I just increment by the 14m rate for that frame. it's close, at least, it doesn't cause a hang leaving LS any more :)

So, the concept here might be, when leaving free-run mode, resync cpu to video frame.

the next challenge is to get the mockingboard to work at faster cpu speed. a basic choice: have it run 2x as fast (i.e. timers go twice as fast etc), or, refactor it to be time-based?

For that matter, keep thinking about having Speaker be time (instead of cycle-) based.

## Sep 16, 2025

Could LS be a fixed but very fast clock speed? I contemplated this before. It would mean the speed is limited even if you have a very fast computer. But it would solve some of these timing problems. OTOH, 1, 2.8, 7MHz are all perfectly good speeds for an Apple II.

So let's contemplate my original reasons for the ludicrous speed setting. Part of my vision here is to have an ultra-fast Apple IIgs type computer. For software development, the benefit of compiling/testing at super-fast speeds is attractive. And CrossRunner is still not available for Mac/Linux and may not be that fast. can't tell, it doesn't have a speedometer.

So, leave LS as really just for an experimental thing. Most people just wanting to run old software, well, it doesn't work right at 300mhz lol.

ok, so shadow + optimized versus just redraw the whole frame. Let's try it.
I will need to:
disable shadowing
disable the flash mode stuff that scans the text frame
disable stuff that checks linemode

ok flash isn't working.. [fixed]

ack I ran into another hang in mouse/some infinite loop. maybe put a check in the scheduleEvent routine itself.

[ ] split gr/text mode in L.S. is monochrome in ntsc mode, should be color/ntsc    
[x] videx mode (apple ii+) in l.s. causes crash  

Trace on/off seems to make hardly any difference in cpu performance. That was not the case before. 

I readded the multiply in the speaker code that gradually reduces the polarity to 0 but increased the timeout to two more decimal places. it seems to work ok. The thing where I add 5 extra samples when the buffer is low, does cause some audio artifacts, as I would expect it to, esp if I load a disk then hit reset I can hear a warble.

Last question is whether to add the lowpass filter. I think before, filtering was required because of the distortions. I should listen to some of this stuff on the real II. SAM, and timelord. Timelord I am not hearing any high pitched whine. I didn't on SAM either. interesting.

Hm, supporting drag'n'drop for disks will avoid having to use the file dialog. maybe that's the thing to do.. so let's say we drag and while hovering we show a drive icon 

So each disk type can ask for SDL_EVENT_DROP_FILE. If it's the wrong file type or otherwise doesn't like it, it can return false and then maybe the other disk type will handle it.
We're gonna want some method like mountByMediaType that automatically checks out the media type, figures out the slot, etc. For now, it's just hardcoded slot 6 which of course will fail for anything except a 143k floppy image.
[ ] Drag and drop mount anywhere in window, it tries to figure out the best place to go.  
[ ] drag and drop onto a drive icon at the bottom, it will use that specific device  

There is also drag and drop of text, which can be used for copy/paste.

### Preferences, and Save File Paths

we currently use get_base_path which is for determining where the app resources lie.
There is also get_prefs_path which is where we can store per-user per-application preferences. This is likely where we should place custom configurations. (unless we end up doing it with individual files).
However there is also GetUserFolder, which looks for: Documents, Desktop, Downloads, Music, Screenshots, etc etc.

So, we want to dump the trace log to let's say Screenshots (or desktop?) or Home.

And we want a, I think, static global class to grab this stuff. That might be the easiest. "Singleton" with automatic initialization. Unless we need something passed in from main, in which case we do it as static class with manual initialization. 

## Sep 17, 2025

There is some hinky code in mount, that assumes the diskii is slot 6 and smartport slot 5. What we need to do, is that devices construct a Mount map as they come online and define mountable media spots; e.g. disk ii controller will register two mount locations as well as various callbacks. (or a single message-passing callback with a message type?)

the computer->Mounts will look like this:

slot / drive
media type (hard disk, 143K floppy, 800k floppy)
Mount callback
Unmount callback
Save callback
Status callback

Then OSD, drag and drop, debugger etc are clients of this class.


btw, French Touch crazy cycles says "headache for emulators, good luck guys"! I should record a video and somehow send it to their attention. But I should validate how these things work on a real apple ii.

Those mockingboard pops are definitely present on ultima iv and were not before. So I bet it is related to the clocking/timing change. One note, we are not 1020500, we are 1020484 - I wonder if that's related. Also, we could change the timing to be based on the cpu c14m instead of the regular cpu clock, so the timing is correct regardless of the current cpu speed or history of cpu speed. Another possibility is - the generator is being called 59.9227 times per second, but the audio parameters are assuming 60fps (44100 / 735). Maybe I need to step up to the whack 44135 / 740 or whatever it is?

Test She's the Shooter on French touch 12-channel audio.
The event timer should be based on 14m because this will always be accurate (except l.s.)
the synthesizer should always be based on whatever the audio stream is using - but should be related to the 59.9227 fps because that's how often we're being called. We -could- run this in a thread perhaps, since there is no need to exactly sync to cpu emissions (cpu is referenced only to generate event times, but the cpu is not directly generating waveform data unlike in the Speaker case )
ran the 12ch FT demo for a while - came back and the event timing is a little out of sync, as I'd expect it to be given the 1020484/1020500 difference.
As to losing precision as our numbers get a lot bigger, I don't think that's the key thing here.
[x] switch to using c14m as the event timer base;
[x] switch to 44345/740 (or whatever) to deal with 59.9227 fps instead of 60fps.  
[x] lower amplitude and mix by simple addition, not averaging.

the latter seems to have dealt with the crackling. The former has provided some ability to handle and sync back up with cpu speed changes - sortof. There is a delay before a change taking effect - this could be based on us having a bunch of stuff buffered. it like goes on for almost a second.. I should put some queue monitoring into the mockingboard debug view. oh, I forgot to change the output audio rate. Fixed, and, speed/sound changes take effect immediately..

the new mixer seems to be working quite well too. I am not getting the highly variable output that I was before. Wonder what that will do to skyfox and some other things I was having trouble with before..

Cybernoid music disk works much better than it used to with the new mixing. It's possible the noise channel is too loud. I believe it does respect the volume, however, maybe the noise amplitude should be tamped down a bit. ok so I multiply it down by 0.5 and that seems better balanced on cybernoid. Let's try the french touch ch12 again. esp with the external speakers, it's fricken amazing!

weird ok somehow all the audio has gotten delayed by a couple seconds - speaker and MB. that's gonna be because of the fricken delayed audio issue when too much time is taken by the file dialog. So for testing purposes right now, use the drag'n'drop. I think maybe I just have to build my own file picker. (They're just not that hard).

[x] instead of dynamically altering the mb filter, just set it to a fixed value. Trying 14KHz for now. It sounds way more "open" and dynamic.  

ah ha, I found a volume issue that is fairly easily reproducible. The music disk for cricket and mockingboard. 
play gay nineties ragtime dance. a number of times the first few bars come across in ultra-low volume, even though the Ampl register is set to F. 
of course it doesn't want to fail when running in the debugger. ok now it's doing it.
alright, so when we finally get around to setting the volume, current_time is 280.07056, and timestamp of the event we're processing is 280.0306.. So we're out of sync on the frame. The register change is happening in the past compared to current audio frame. so it's missed? 

## Sep 20, 2025

OK so the mockingboard issue is definitely when: open file dialog causes emulator to skip frames, the decoding window and current cpu cycles/time gets out of sync, and events get posted to times BEFORE the current decoding window. If I load disks through drag and drop then there is no problemo. After I cause a few delays, I get these issues.

This is the same cause as speaker issues. 

ok lets try the "freeze emulator while dialog is open" thing.
Alright so let's say I'm doing that. SDL_GetTicksNS is gonna continue incrementing even if the cpu counters don't. Let's see where all I use that.
it's used by Metrics; no problem. OSD for calculating some stuff, no problem. pdblock2 to turn the drive light off after 1s; ok. WAS used in mb.cpp but not any more. 
used in cpu/get_current_time_in_microseconds but that's commented-out.
void computer_t::frame_status_update() just for display purposes.
of course by frame_sleep, to sync to the next frame. Give that one some thought.
main event loop ludicrous speed stanza;

OK. So nothing except ludicrous speed cares about this, and that only during execution of a frame which will not cause the issue. Let's see where we call SDL_FileOpen. 
only in OSD event handler, which occurs after CPU processing but before emitting other device frames. So what we want to do is:

stop all audio output;
instead of doing show open file dialog there, we pass a message to halt execution and then open it inside a special event loop. (This is such a big PITA). 

Let's analyze and more thoroughly document how the MB works.

### processChipCycle

the intention here is for every chip (CPU) cycle that has elapsed (assuming fixed 1.0205 mhz rate) we process one iteration of the counters. The counter starts at the programmed tone interval, and for each cpu cycle, decrements. At 1/2 the counter, and at 0, we flip the channel.output variable between 0 and 1, which causes code later to include a contribution of either +volume or -volume. I.e., toggling between high and low of a square wave.

generateSamples loops from 0 to number of samples (740 currently) per frame. We have calculated the fractional cycles per sample. 

except for the timestamp check at the top, this function deals with all integer values. 

The Envelope Generator can tick at the fastest at 3.986khz. This is further divided by the Envelope Period. So if EP is set to 4096, the envelope will process at 0.973hz. (this is then divided by 16 sub-cycles). 

## Sep 22, 2025

on windows, we're crashing out somewhere in calling the shutdown_handlers in computer. I guess I need to get a debugger going cuz printf isn't catching it. I want to cut a release so let's log an issue.


# Sep 24, 2025

bugs!

[x] skull island dhgr splash screen displays wrong hires page

300g
turns on double hires (c05e)

does auxmove 2000 - 3fff to 4000-5fff in aux
C00D - enable 80-col mode
c050 - graphics mode
c052 - full screen graphics
c055 - page2 - uses dhgr page 2???
c057 - hires mode
c010 - clear keyboard 

return to basic

after this, page 2 is enabled (C01C is high)

I'm guessing this is supposed to be still on page1. 

ok, in L.S. I get the correct results (display is page2 dhgr)
There must be something wrong in the cycle-accurate display code
VideoScannerII:calc_video_mode_x.. 

The VideoScanner calc_mode_x routine was wrong. it was not allowing the page2 switch to control if 80col happened to be set. We actually prevent page2 s/s from doing that in iiememory:

        case 0xC054: // PAGE2OFF
            if (!iiememory_d->f_80store) retval = txt_bus_read_C054(ds, address);
            else iiememory_d->s_page2 = false;
            break;

ok space quest is now not remotely working correctly.

I am getting inconsistent behavior between emulators:

So let's say we do:
   lda c054 <- page 1
   sta c001 <- 80store

What happens? 

   lda c055 <- page 2
   sta c001 <- 80store

a2ts: we get text page 2, aux ram.
mariani: we get text page 1, ?? ram.
real iie: text page 1, ?? ram.

This is feeling like when 80STORE is on, the video scanner is forced to page1.

what happens here:
   sta c001
   lda c055
page2 flag reads 1; display is still page 1; 80store=1

How about:
  lda c055
  sta c001
  sta c000

Mariani: we end up displaying text page 2.
real iie: display text page 2

ok. SO. it's the SAME switch. the MMU and IOU each track the value.
But when 80STORE is on, the video display IGNORES the page2 setting, and displays page1.

So these tweaks are going to fix VideoScanner, and I will have to separately fix AppleII video generation.
* iiememory_read_display
* update_display_flags
* calc_video_mode_x (leave this as-is)

So -always- sync s_page2 in iiememory; -always- call the display txt_c054/5 handler; 
page2 doesn't change; but video mode needs to be recalculated because of the changed handling of page2;

oh oh. 
video scanner is checking 80col, not 80store. DUH.

"80STORE, PAGE2, and HIRES are mechanized identically in the MMU and IOU. "- sather
this means, display and iiememory BOTH need to track these. More specifically, iiememory can tell the video scanner the state of 80store whenerver it's updated.

[ ] bug:
   booted with joystick off
   keyboard kept reading joystick button 0 down and rebooting.
   turn joystick on, then the reading clears to 0 and ctrl-reset works as exepcted.

[ ] ds->video_system->set_full_frame_redraw(); <-- we can get rid of all these once we're confident we're sticking with the "redraw full frames in LS mode.

[ ] check condition after reset on table 5.4 of understanding iie - make sure we are implementing those reset conditions.  
[ ] per table 5.4 also, C018 is MMU, C01C/C01D are IOU (display).

[ ] check condition after reset on table 7.1 of understanding iie - make sure we are implementing those reset conditions.  

## Sep 25, 2025

[x] Mouse Reset tends to cause emulator to lock up by sending an infinite series of new scheduleEvent for current cpu cycle. Do it by launching Skull Island, wait for MB to start, then hit reset a few times. should probably also disable interrupts? (added a check to scheduler to prevent request before current time, a hack..)

[ ] Chiptunes stuff (skull island, crazy cycles 2) generate unending streams of: 
[Current Time:   101.782942] Event timestamp is in the past:   101.777735

## Sep 26, 2025

looking at issue #60. So here's what happens high level on a speed switch down.

I switch from 1M to 2.8, and back to 1. On the 4th try, going from 2.8 to 1 resulted in ScanBuffer having 80 extra samples. This is two scanlines' worth of data,
and indeed V=2,H=12-14 at end of each frame.

Premise: 
not every time, but sometimes:
cpu cycles exceed the frame maxmimum by 1-7 (1M speed) or 3-21 (2.86MHz) speed.
This means there are already a few extra samples of video data in ScanBuffer (probably, since we are 

We're measuring frames by multiples of 14M.
But we call cpu->set_frame_start_cycle to equal the cycle counter as it is now. (only used by mouse, so disregard this for now).
we speed shift - this is done in the event handler area. This calls set_clock_mode. 

Now the number of 14M should be synced to the video frame. So if we exceeded 

I can do a check and breakpoint whenever samples > 0 after doing video update.

done - there are 15 samples in buffer, and hcount is 39! That is not right, it's out of bounds.
what if I display this data whenever there's a speed change. Maybe I have something accumulating?
the most number of extra samples we should ever get at end of cpu loop is 1-7 in 1mhz mode. 
maybe 2.8 mode figures are wrong?
if there are extra bytes, we are speed-switching in the middle of a scanline. now the 14m counts should work out.
I can check to make sure the video_cycle_14M_count is always exactly 0 at the end of a scanline. It's not. in 1mhz mode, we need 2 extra 14M's. 
The video scanner in a real II is clocked on the CPU clock, not the 14m clock (though it does use the 14m clock as the pixel dot clock).
if we have emitted a few extra video bytes in a frame, at 14m, we need to make certain when the speed switches, we don't end up emitting too many.

```
        if (++cycle_65th == cycles_per_scanline) {
            cycle_65th = 0;
```

So this counter has changed.. will not always be 0 at end of a frame. But is changing from 182 per scanline to 65 per scanline, so its scale is changing.
This feels like the culprit.. 
the goal here is to add the extra 14ms to the end of a cpu cycle. 
may could do something more like.. 
65 * 14 is 910.
182 * 5 is 910.
455 * 2 is 910.
and then these all have 2 extra. 
So instead of checking the 65 counter here, just check if we've hit 910. 
or if we have emitted 65 video bytes

```
        scanline_video_bytes_emitted;

        c_14M += c_14M_per_cpu_cycle;
        
        if (video_scanner) {
          video_cycle_14M_count += c_14M_per_cpu_cycle;
          scanline_14M_count += c_14M_per_cpu_cycle;

          if (video_cycle_14M_count >= 14) {
            video_cycle_14M_count -= 14;
            video_scanner->video_cycle();
          }
          if (scanline_14M_count >= 910) {  // end of scanline
            c_14M += extra_per_scanline;
            scanline_14M_count = 0;
          }
        } else {
          // add extra cycles or not when scanner disabled.
        }
```
ok, yah, that did it!
speed-shifting in Crazy Cycles results in oddball behavior, but we expect that, because the app's counting and timers is going to get confused.
however, I slowed 1 mhz, rebooted, and it worked just fine. up and downshifting speeds a lot, and the video is not getting out of sync at all.

Bug 61:

There are two possible views here.
1. simulate the electron beam cycle by cycle as it scans. This would let people possibly more accurately debug cycle-accurate programs as they could see exactly what is going out the beam and when.
1. when we pause or bp - ignore the contents of scanbuffer - but don't zap it - do frame updates using Apple II. all display updates while in step mode are done by Apple II. When we resume,  

in single step mode, the below ought to give us the ability to watch the electron beam do updates:

if there is a partial frame to display, process what we have to the frame texture.
As we single-step (or however), also process what we have to frame texture whenever we have it.
So these are the same tasks - if we are single-stepping, do a partial frame update using AppleII class.
Any time there is a partial frame update, we must track H/V location in the partial update.

but maybe what we want is this:
if we're inside step_into, track whether we went past frame end after (potentially very short) cpu loop - and then and only then execute frame update stuff. we DO need to run these each loop:
frame_event
frame_appevent
video - if we executed any instructions, draw full updated video frame and leave scanbuffer alone

but only at end of 238944 c14's:

update end_frame_c14m
device_frames
eat one frame's worth of scanbuffer

frame_video_update is doing a bunch of excess stuff tho. it is doing video->update, osd->render, debug->render, video->present which we already did.

when we return to regular mode after step, we need to finish the previous frame's worth of cpu. it feels like we're running too much.

OK IT HAS BEEN FIXED!!!! What a pain.


Bug reported in A2Desktop:
3.5 not seen after boot;
it tries to do a read on each drive; we return 
```
    if (st) {
        pdblock_d->cmd_buffer.error = PD_ERROR_NO_DEVICE;
        return;
    }
```
for each. Then it doesn't try to scan or read from us again even on a Special>Check All Drives

## Oct 13, 2025

OK well there's an 816 CPU in the repo now. :-)

I also implemented it for a lark on a IIe host. This code proves it:
```
800: 18 FB C2 30 AD 00 09 8D 00 04 38 FB 60
800g
```
writes TWO bytes to video memory.

I don't like the coupling between platform and display.cpp where it needs to know which "platform".. I mean, ... eh maybe it's ok.

It's not running as slowly there (270mhz?) as it was in the snes-test. I wonder why that is.

## Oct 15, 2025

The current system operated well at 400mhz effective. When I added cycle accurate video, speeds dropped to around 280-290mhz. I think the MMU can benefit from some improvements.

MMU work:

[x] I put a bounds check into MMU_II read() and write() functions. It throws a warning to stdout when it detects an out of bounds address and also normalizes to 0xFFFF.
[ ] Switch the MMUs to use a Struct-of-Arrays approach instead of an Array-of-Structs. Compare performance. This could be a video subject.
[x] Implement as templates so that these values can be static: num_pages, page_size, debug.
[ ] Allocate the page table statically and use alignas(64). 
[ ] Statically define an address mask to apply to all addresses passed into read() or write().
[ ] in debug mode we'll also have a separate out of bounds check, that will log a warning.

I originally defined pages as 256 bytes because in the //e we have various memory regions that swappable:
zp: 256
stack: 256
text1/text2: 1024
hires: 8k
Lang Card: 4K, 8K

So I chose 256 as the least-common denominator. However, it's kind of funny, the "page table entry" is 80 bytes to map 256 bytes of memory. lolz.

Now the GS will allocate probably a 16M chunk. That would be 65536 page table entries which is kind of silly. Most banks in the GS cannot be delineated like that. Just the legacy stuff.
Everything else is done at the bank level I think. Bank E0/E1 mapped to MMU_IIe. Bank 0 and 1 are only partially shadowed (text pages, hires pages). I/O Space is always available in 0/1/E0/E1. So that could imply a 4k page size. And this is a reasonable choice also for 65816-with-virtual-memory pseudo-device: 4k is a good page size for memory protection. And:
Text pages are in the first 4k. we can have a simple handler that sub-divides handling of these areas.

I might be able to simplify the MMU design by:
register the C000-CFFF space as r_handler and w_handler, inside another object. The MMU itself would then have no knowledge of this and would not have to do a bunch of page checking, nor have the specialized C8xx etc handling stuff in it. That would go into the anciliary class. We'd use 4K pages; and 3 of them - 00-0F; C0-CF; would have sub-handlers, with sub-pagetables at either 256 bytes, 1024 bytes (text pages); or the very specialized CXXX handling stuff. Registered as a generic handler. 
Then there'd be 16 pagetable entries on a II, II+, IIe. Then the same independent handlers could easily be registered into the IIgs memory map as appropriate. (The II+ doesn't even need a handler for anything except C0-FF). Page changes for things like hires main/aux would be much faster since fewer 16 times fewer entries are changing. (1 entry for D0, instead of 16).
I like this for a post-v1.0 enhancement. Or if we run into any significant GS EMU performance issues.

I spent most of the last 3-odd weeks in the 65816.md file, for anyone who might be following along.

## Oct 16, 2025

Just thinking about the IIgs Disk motor-on-detector and was wondering about how we would handle speed shifts inside a frame. Well, it would actually work. Since we now clock based on 14M, the clock speed change would just trigger and we would start counting a different number of 14Ms per instruction immediately. The frame loop would still exit on or around counting the right number of 14Ms. It could even take effect -inside- an instruction depending on how we hit it.

Should really consider having that clock module.

the "internal" shr pixel closk is 16.xMHz. It is apparently exactly an 8/7 ratio from 14M.. so, 16.3636. An Apple II reads 40 bytes to generate a single scanline of text/lores/hires, with 7 pixels emitted per byte. And this is 14 14M's, 1 pixel each two 14M's in essence though we don't operate at that resolution. Double hi-res/lores/80col is 80 bytes per scanline, or twice the video data bandwidth. A IIgs SHR display needs to read - for 320 4-bit pixels - 160 bytes per scanline, or exactly four times as many. That each cycle spits out one extra pixel here (hence the 16M clock) is irrelevant to our emulation purposes. The 16M clock is entirely internal to the VGC and does not impact the CPU -- paraphrasing John Brooks.

IIgs video is very straightforward in theory.  Also from John:

```
So I think the VGC does this:
6 cycle right border, reading $C034 border color reg each cycle
1 cycle start hblank & read SCB, determine 320 or 640 pixel clock
8 cycles to read selected palette
4 cycles complete hblank
6 cycle left border, reading $C034
40 cycles active video, reading 4 bytes per cycle for SHR modes
```
Note this is still that magical "65 cycles" per scanline.

The details around border colors are In IIgs mode for cycle-accurate video are above and are interesting. This implies the following changes:
the border area left and right would be: left and right border are "6 cycle", i.e. 7 pixels * 6 = 42 pixels. Or is some of this hidden on a typical monitor?

What if instead of simply having a 560x192 (er, 580x192) buffer, we have a buffer for this complete picture - 560 + 42 + 42 = 644? hm. what about 640 + 42 + 42 = 724.. ah, that is close to 720 which feels like a thing. The border color can be changed in the middle of drawing border and there is a demo that shows that. In the Scanner LUT we can have a flag "read border color". The challenge is: the content area is either 560 pixels or 640 pixels before scaling.  When switching between modes we don't want the relative proportions of the border/content to change. So: emit an appropriate number of border pixels depending on the mode; in apple ii modes, base on 7 pixels per byte; in shr mode, base on 8 pixels per byte. Use different buffers for a2/shr. Then when we "paint" in it's scaled correctly and should "just work".

This feels like there is too much border. I suppose on a real machine this was adjustable; change the horizontal beam rate to increase the picture width and get more "x". ok, I drew it. It's actually a good ratio. Each border is only 7.5% of the content area. For a total of 15%. Let's compare to my current border numbers..
and we know double hires-80 etc. shift the start of that by 7 pixels. So that gives us area to work inside for this effect.
So shr: 736 x 200
apple2: 644 x 192

Currently our border width is 30 pixels pre-scaled. So we just need to make the window a hair wider. 

Now, for vertical, is it the same thing? it's only 8 additional scanlines, so this feels like we probably just have less top and bottom border and instead of scrunching it a fraction we define slightly smaller vertical borders.

NTSC has 525 lines; an apple ii frame is 262.5. that means, in apple II mode we have 35 scanlines above and below our content (31 in shr). Currently we only set up 20. So after doubling vertical, it's 42 pixels by 70 pixels, which looks about like maybe what kegs is doing. hard to tell exactly.

Depending on the settings of a real monitor, some of those top and bottom border scanlines would not be visible. You would typically have some of them hidden in order to have lots of vertical space in your display area. So, we don't have to actually generate that many pixels.

The native window size would be nominally 1288 x 1048 if I stuck with 35 scanlines top and bottom. OK, that's not right - Claude points out that some of the period is taken up doing vertical blanking, and that 20-25 of the 262.5 scanlines might be used up by VBL. I bet Sather can tell us exactly.. (IIe, page 3-16) it's shown as 4 cycles plus another 4 cycles of colorburst. A GS wouldn't have colorburst on the ntsc port.
9 cpu cycles on the right and 8 on the left, which is wider than the 6 cycles John suggested, so just a diff ratio on the IIgs. on IIe, all the borders area would be hblank/vblank.

I just found this:
https://www.brutaldeluxe.fr/documentation/cortland/v6/Apple%20-%20IIgs%20RGB%20monitor%20-%20Timing%20chart.pdf

Excellent detail from Apple.

So this is suggesting: 19 scanlines of top border, 21 of bottom border; more like 6 cycles left and 7 cycles right. ISH. This aspect of the timing does not correspond to the cpu cycles exactly. It might to 14M, explore that some more. (and 14M would be, two 14M per dot). So this would be pretty straightforward to implement into the VideoScanner, just knowing the hcount and vcount where we should read and emit border color. This is where I think we use a special flag or value in the VideoScanner table - "no output". (that could just be memory location 0). For reading the palettes in SHR, those would have certain addresses, and be in the LUT but would be interpreted not as "emit color dot" but "transfer palette value". (might have to do 4 transfers per cycle). Also, a flag for "border color" which would read the border color register at that moment. 

KEGS has an extra-wide right border in Apple II mode. I think he is doing only integer scaling, so in SHR mode I would expect less border.. yep, that's what's going on. In SHR in Kegs the aspect ratio is really off.

So then this comes into the aspect ratio conversation. I suppose since I have a "real" monitor I can just take some measurements!

For cleanliness in the system, we'd backport this stuff to the stock Apple II platforms.

Using an aspect ratio of x/y = 2.4 seems to work pretty well and gets us close. I think making this a user option is the way to go - switch between the current 2.0 ('square' pixels) and 2.4 (more accurate).

## Oct 17, 2025

Key bindings! Figure out key bindings for Apple II special keys that don't conflict on various O/S for defaults. 

I think this is the desired outcome here..

| Platform | Apple II | Mapped | SDL Keymod |
|-|-|-|
| MacOS | Open-Apple | Option (Left) | SDLK_LALT |
| | Closed-Apple | Option (Right) | |
| | Reset | F10 | |
| MacOS 104-key | Open-Apple | Option (Left) | |
| | Closed-Apple | Option (Left) | |
| | Reset | F10 | |
| Linux | Open-Apple | Windows (Left) | |
| | Closed-Apple | Windows (Right) | |
| | Reset | F10 | |
| Windows | Open-Apple | Windows (Left) | |
| | Closed-Apple | Windows (Right) | |
| | Reset | F10 | |

F12 - should get rid of F12 to "quit" and just rely on whatever the platform's "quit" key is.  Put up a dialog asking user to confirm they really want to quit, when they do this.

I do already have Option mapped to "fake control-openapple-reset" in Apple II+ mode. This snippet of code should only work on pre-IIe.

On Mac, people don't necessarily always have Function keys enabled or mapped. 

On a PC keyboard on a Mac, Windows => Command; Alt => Option; that's how SDL will read them on a Mac keyboard.
I had the keys reversed in my setup with PC keyboard. 

So I am now checking for ALT.. on a regular Mac KB, that will be option. And on my Mac with PC KB, I have these reversed, so I actually hit the Windows key for these.
BUT on a regular Windows PC or Linux, ALT is ALT. And we know that this is going to have lots of conflicts. SO, the root issue here is, between Mac and PC/Linux, I need to reverse which keys I am checking for.

Mac: ALT (option);
PC/Linux: GUI (windows);

### Testing Results

* Aplworks 4.3 / IIe_Enh

the OA-? does not work. I wonder if we're getting some whack key map or if SDL is not mapping right when modifier is set?
OA-up arrow and down arrow are working.

Here's what happens when we map keycode with modifiers down:
Key down:       2F [mapped: 0000003F] 00000002 00000002
Key down:       2F [mapped: 000000BF] 00000102 00000102

ha, whoops. So when I send to map, I need to filter out these modifiers.

* Merlin-16

OA-A is not assembling. This was working before but now it's not.

OK I hope I have it right this time! Mac and PC are the opposite of each other.. and on MY Mac, I have option/cmd reversed so they're the same as a PC, on my PC keyboard.


[ ] Aside: to generate a phosphor-persistence effect, can keep 2 (or more) textures. Alternate between them. When emitting a frame, draw the old one at some fractional intensity, then draw the real one on top of it normally.  

working on bug #74: the issue isn't when we switch out, it's when we switch -in- to L.S.

So here's the event loop when in normal mode:

```
execute cpu:
    if we're not free run, we call video_cycle (putting stuff into scanbuffer)

execute events:
    if ((cpu->clock_mode == CLOCK_FREE_RUN) && (cpu->video_scanner)) cpu->video_scanner->reset(); // going from ludicrous to regular speed have to reset scanner.
    cpu->clock_mode = mode;

display_frame:
        if (ds->framebased || force_full_frame) {
            update_flash_state(cpu);
            ret = update_display_apple2(cpu);
        } else {
            ret = update_display_apple2_cycle(cpu);
        }

    if (cpu->clock_mode == CLOCK_FREE_RUN) {
        ds->framebased = true;
        cpu->video_scanner = nullptr;
    } else {
        ds->framebased = false;
        cpu->video_scanner = ds->video_scanner;
    }
```

Maybe this would be clearer/cleaner if speed changes were handled somewhere else. Being done in the middle of the event loop must somehow be the problem. Working on that.. still getting the extra stuff in the scanbuffer.

incr_cycles calls video_cycle as long as we're not in FREE_RUN.
video stuff is based on framebased, which is set by update_display_mode whatever. 
well this ought to be working. It's possible I am somehow allocating two ScanBuffer and sometimes populating the wrong one.
let's check the values in SB when I: change speed INTO LS and when I change out.

wait.. so let's go step by step in frame mode and see what the frame parameters are. Because I get hit first with a "no data" and then a "too much data" thing.
yeah ok, when we go from LS back to 7, the end_frame_c14M is all out of whack.
So thinking about this, what we're after, is that:
   in LS, we still process a 'frame' at a time.. where a 'frame' is still 260xxx 14m's. But we'll keep doing those until we ALSO exceed the time limit.
So this means keep incrementing end_frame_c14M same as we do in normal mode.

## Oct 18, 2025

thinking about reset - if you hold control-reset on a //e, it holds the reset signal against the cpu (and presumably everything else in the system). In our current emulation, we send a transient reset.

What we could do - when we get ctrl-reset key down, set a "reset" flag that would:
  stop cpu, frame, etc execution -except- for events (waiting only for key up event)
  on ctrl-reset key up - restore normal frame operation.

Game controller - we want these Modes:

Joystick (Gamepad)    - Default
Joystick (Gamepad) Atari Joyport
Joystick (Keyboard) Atari Joyport
Joystick (Mouse) - also Trackpad, Trackball.
Paddles - somehow

Currently we have joystick_mode (Apple, Atari) but then also game_type (per pdl() input). Is it still correct to have these two vectors? There are some combinations not currently handled with this, e.g. "Mouse+Atari" (and, this would be very awkward to use). Though, having joyport via 10-keypad would actually be workable.

Apple Joystick - Gamepad (Default)
Apple Joystick - Mouse
Atari Joyport - Gamepad D-Pad
Atari Joyport - Keypad
Paddles - somehow (Keyboard?)

When in Gamepad mode, do -not- automatically switch to Mouse if no Gamepad connected.

Well that went fairly well. (done)

### Back to the (Future) Scanner

Back to IIgs video scanner - I have a detailed spreadsheet with all the timings in it. II modes are the same as now with the addition some flags. Ah, except I need more bytes with which to store the border and/or text colors. Currently the struct is:

```
Scan_t scan;
    scan.mode = (uint8_t)video_mode; // so far, a value up to 11.
    scan.auxbyte = aux_byte; // these two can't change
    scan.mainbyte = video_byte;
    scan.flags = mode_flags  // so far, 3 bits
```

so we'll need a new video_mode for shr. But where o where do we put the color info. border color: 4 bits; text color: 8 bits (4 fg, 4 bg).
well now, for border, we can just put the color index right into the mainbyte and/or auxbyte.
For text, I'm gonna need 8 bits.
So perhaps what we do here, is stuff the palette reads into here. Palette is read once per scanline, as part of the scanner process.
For shr, each "cycle" we need 32 bits of data. So maybe just expand this thing to 64 bits.
It won't make that big a difference.

Scan_t scan == 64 bits.
```
    scan.mode = (uint8_t)video_mode; // so far, a value up to 11.
    scan.flags = mode_flags  // so far, 3 bits
    scan.auxbyte = aux_byte; // these two can't change
    scan.mainbyte = video_byte;
    scan.shr_data = (uint32_t). 
```
Alternatively, we could reserve aux/main for apple ii stuff and have vbyte0-3.. oh, actually, we can read 4 bytes at a time here. I.e. have the address be 
B + 0, B + 4, B + 8, etc. and read the 32-bit word from each of these, and put into scan.shrdata when in that mode.

We need the following additional scan.mode values: shr320, shr640, palette, border. text just adds the fg/bg stuff in the new bytes.

--
looking at videoscanner performance a bit.

boot apple iie enhanced
hit ctrl-reset.
sitting at applesoft prompt.

273-275mhz.
--
make set_floating_bus and floating_bus_read inline funcs: 271-272mhz. Wut.
UN inline them: back to 273-274. So that did not have the expected effect. Must be cache locality stuff.
--
set up SCANNER_LUT_SIZE as a constexpr and that gave us 1-2 mhz (275.7)

trying alignas on the LUT arrays now.. no difference. but leave it in.
--
II+ is running at 281-282.
eliminating weird double-pointer thing from VideoScanner. Still 282. But way easier to read.
iie is at 274-276.
Oh I'm a dummy, this isn't affecting these modes AT ALL. I need to check vpp.

--
ok these aren't really helping. The compilers are just too good. And I bet even if I got rid of the .. ok it says 5 to 10 cycles for a virtual method lookup. 1 million per second, is 5-10M cycles/sec. 0.1% to 0.2% of 300uS which is 3uS which is not going to move the needle here. So stop worrying about this part.. I even just doubled the scanner LUT struct size and that makes no difference either. Also, I gotta keep reminding myself, THIS DOES NOT AFFECT L.S.

If I really wanted to make a difference here, it would likely be to use shaders to do the image processing.

## Oct 19, 2025

I've added blanking flags during VideoScanner init so instead of checking hcount/vcount for blank time, we check the flags.

Oh, THIS is fun:
https://paleotronic.com/2019/03/11/microm8-update-super-hi-res-shr-support-1-2/

Yah, gotta do that. Also, would provide a step 1 platform for testing shr code (besides in vpp).

A challenge:
VideoScanner starts with h=0, v=0, and idx=0. But this is not the upper left corner of the screen.
We actually need to start with h=0, but v = 243, idx = 15795.

I've got overrun here. ScanBuffer has 17,030 entries, which is exactly what we expect.
VideoScanGenerator is overrunning the output Frame at 645 (it's 644 wide). 
12 * 7 = 84, plus 560, is indeed 644. let's step through and see what data we're getting..
ok, I struggled through it - I made the border cycles 7 pixels (instead of 14) to make the border smaller. Also, the colorburst isn't working.
Also, frames are taking up to 30% longer, because we have quite a lot more data we're pushing out. Even more once I add the top/bottom borders.

Checked out some videos of VidHD. I think it makes sense to display the wide (6 cycle) borders on big widescreen monitors in fullscreen like the card does; however, it seems like way too much windowed.

Will probably want some options: for apple II stuff, I can imagine some people wanting no borders. For GS, I think a border of half the width (I mocked up 42 pixel wide border and I that does not seem unreasonable.) Note that 19 / 21 top and bottom borders are also quite large as they will be scanline doubled. So after multiplying by standard scale 2/4 respectively, the left/right borders will take up 84 pixels each and the top/bottom will take up 80 pixels each, or 168 pixels of the X and 160 pixels of the Y. That's a lot.

Also, it occurs that I'm currently trying to interpret border data elements as apple ii bitstream, which is wrong. I mean I could hack it to work that way, but that's not what we want.. what we can do is push a special value like 0x8X where X is the color index, and the 8 is a flag saying "this is a special value".

A whole different approach here could be, generate border info to a separate texture, then draw the border texture, then overlay the content texture. Then the content texture is the same (current) Apple II size. When doing VideoScanGenerator we'd emit the border stuff to a different texture buffer. And we can compose by drawing 4 sections; top, bottom, left, right, using rects. (maybe even have 4 textures). This is fun because we can also scale the border area when we blit - instead of generating 14 pixels per cycle, the left area would be (for instance):
6 x 192 (or, 6 x 262). Hm, I think there are 8 sections here that each need separate X/Y scaling. E.g. if we are medium (half scaling) then the top left corner or border must be scaled X * 0.5, Y * 0.5.
But the Right side border would be scaled only X * 0.5 - Y stays the same to match the content scanlines. etc. When I say scaled here, I am just talking about 
The existing NTSC/RGB/etc processing doesn't change, that frame size doesn't change, and so the performance won't change!

So, ONE texture update, and 8 texture copy calls. Precalculate the rectangles for these blits.
So the actual border texture then is 65 x 262, 17030 entries of RGBA_t.

So it's a very small texture, and we rely on the video hardware to expand it to 84 wide or 42 wide or *any border scale* we want, while not losing any information. Can have setting, Border Scale: Small, Medium, Large.

[x] I also have a potential optimization for the Frame stuff. Use a single linear array:
```
now:
    alignas(64) bs_t stream[HEIGHT][WIDTH];

    inline void push(bs_t bit) 
        stream[scanline][hloc++] = bit;   // currently does likely 2 multiplications and an addition. (or a mult, a shift, and an add)

new:
    alignas(64) bs_t stream[HEIGHT*WIDTH];
        stream[index++] = bit;           // no multiplies, just an add to go to next index.
    setline
        index = line * WIDTH;            // only do one multiply for whole scanline, and then only if needed.
```
And we don't even have to call setline, if we are careful with our pixel emission counts, we just automagically wrap to the next line after we push last pixel on this line.

Swank.

[x] and another optimization: the final stage, Render, can be made to render directly into the LockTexture buffer. Right now we are double-buffering and have an excess copy of a half megabyte. That's not trivial.


One thing my overall approach will not handle, is rapid switching between super hires and legacy modes in the way the Apple II demos do. Each frame is one or the other. I'm not aware of any though that doesn't mean there aren't any - but handling the 7/8 scaling issue on a per-cycle basis would be very difficult. This prompted a big discussion on Slack and there are in fact a few programs that do this.  when GS switches from legacy (14mhz) to shr (16mhz) the screen is distorted for what looks like 6-8 scanlines while the PLL in the monitor catches up, gradually changing frequency.


Overall question: is it better to scale 8 to 7, or 7 to 8? Plan so far is 8 to 7.

For L.S. mode we can color and update the texture only as needed on a color change.

## Oct 22, 2025

ScanBuffer is a circular buffer. That part doesn't change - all the bits of data are mixed into there. In fact we could add bits if we wanted like where HSYNC and VSYNC, presence of COLORBURST, etc.. The split into border and main content will occur in VideoScanGenerator.

Currently we pass it ScanBuffer and the output buffer; so it needs to change to get two output buffers, one for border and one for main content. These should be: 184 x 262 dimension in pixels; but, we really only need it in *cycles*. So that's 6 + 7 = 13 cylces. We will expand to however many pixels we want, by stretching it in RenderTexture. So, it's 13 x 262.

VideoScanGenerator needs to know the mode - or, needs to obey hsync and vsync markers and set and increment hcount/vcount accordingly. I guess I could say, if border, do to 53 otherwise do to 40. But there's the top border too.

OK have to think about how to handle transition from border into single-res mode. Single-res modes fill either with black (II, IIe) or need to fill with the underlying border color. OH, we can alpha that. Let's say we push value 2 (or 255) - this is interpreted as "0, transparent." for purposes of the video display algorithm. The same thing applies to the right side, as in 80 col mode the entire display shifts to the left 1 80col char. So we have empty bits at the right? yes. The 560 pixels are either at col 0 (80 col mode) or col 7 (40 col mode).

## Oct 24, 2025

thinking about the dpp harness, and SHR. Do I add it to AppleII_Display? Sure why not. 

MTL_HUD_ENABLED=1
export MTL_HUD_ENABLED

So the time to render a 320 shr frame and get it to window is 135 to 140uS. Comparable with text! Which makes sense because we're only doing a single stage, and no complex lookups. Scrunching the image from 640 pixels into 560 pixels looks ok on here too. of course we're not really downsizing we're upsizing once the double-scale is taken into account. I haven't implemented 320 fill mode of course.

Linear interpolation subtly changes perceived colors in dithered source by altering the ratio of one color to the next. An area in the Airball screen streaming from the moon is orange dithered with blue, and that gets noticably more blue when using linear interpolation. In straight upscaling however some IIgs pixels are smaller than others, as you might expect. The only way around this will be to have a mode where we display the pixels 1:1 (well, guaranteed 2:4 anyway :). In KEGS there is exactly ONE scale (presumably, the integer scale) where a 640 desktop doesn't have noticeable and awful weird banding in the GS/OS desktop dithering.

Ah ha! SDL3 now supports scale mode PIXELART. with the legacy apple ii modes, it seems to be indistinguishable from nearest - ah, because we're not doing fractional scaling in dpp. however, in SHR mode, it DOES make a difference since we are not scaling horizontally. it makes a big difference!! it's got those sharp edges line nearest, but pixel dot sizes are much more consistent.   I think add this scale mode to GS2 and experiment with it a lot. Compared to scaling in preview, the colors seem unchanged, and color balance unchanged. This could be a big game changer.  

ok, that actually looks pretty good - EXCEPT in text mode, where the color is too bright. Well, that's because I'm doing 0xFFFFFF, and what I'm used to is linear toning that down somewhat. I think this is definitely the way to go for GS modes. And maybe tone that white and green levels down a bit when in that mode in text.

Acid test: I got a GSOS desktop image out of kegs. In pixelart mode, it's not awful - the scaling from 640 to 560 is every 4th dither line is a little different than the others. It's noticable, but consistent unlike KEGS where you get the weird banding. nearest IS awful. and linear is pretty decent too. The fuzz actually isn't that bad. There would likely be a window size with no scaling artifacts, just like there is in KEGS. And, of course my aspect ratio is better.  Text on shr is nice and crisp.

-> Again, I think maybe we start an emulated GS in a window size that provides integer scaling - with PIXELART mode that should be pretty good. And hope they have a big enough monitor. ha ha.

I should put window resizing and scaling into dpp.

Lots of testing:

"Final Aspect Ratio" is the ratio of X to Y pixels in Squareland. We want this to match the original monitor aspect ratio, which I measured to be approximately 1.2. But between 1.2 and 1.3 is fine, and VidHD uses 1.28:
640x200 -> 1280x1000 : this is what VidHD does.

For GS display, we really truly need integer multiples - or, some kind of way of converting dithered 640 colors to a "real" color shade. 1.28 aspect provides integer scaling for both X AND Y, for two target resolutions:

640x500; 1280x1000

the X scaling is 2 and Y scaling is 5. And don't forget we need some extra pixels for  

So, if I take out the memcpy in the typical rendering loop, for shr my render times goes from 200uS to 136-150uS. This is a 30 to 35% time reduction, just not doing the double-copy. Well then. Seems like rendering the new frame texture directly into the buffer SDL3 gives us for a texture update, is the way to go.

Struggling with scaling math. SHould be butt simple but I always have difficulty keeping different concepts straight.

So let's define terms first. Canvas is the window we're drawing into. We want two: one optimized (and as a default size) for old Apple II modes; and one optimized for the Apple IIgs. 

Target 1.28 Canvas Aspect.

For 1.28 aspect, we need the following ideal window sizes and scales:

640x200 -> 1280 x 1000 -> [2, 5]
580x200 -> 1160 x 906  -> [2, 4.53]
580x192 -> 1160 x 870  -> [2, 4.53]

the extra 8 shr scanlines come at the expense of border area. So we will draw both 

Then there's source Aspect. e.g. 640/200 = 3.2; 560/192 = 2.92; but I think we can ignore this. 

C_ASPECT = 1.28
xscale = canvas.w / source.w
yheight = canvas.w / canvas.aspect
yscale = yheight / source.h

OK I'm real close now, except that apple II modes should use 8 scanlines less of the target, and they're not, they're being scaled to exactly fit the window. the window is based on 200 scanlines, the source is 192 scanlines, and I need to account for that in the Y scale calculation. We should assume the canvas is always set up to be 200 scanline capable. Right? Yes because shr works fine here. apple II mode is actually a little different. 

Recent testing: 320 mode frame time is 138uS; 640 mode around 200? Interesting. It's fewer lookups I guess.

## Oct 26, 2025

Doing some thinking about scaling. If we're in 640 mode and doing dithering, instead of scaling the pixels, scale the pattern. e.g.
ABABABAB
scaled naively might be
AABBAABBAABBAABB
however, what if you scaled that instead as:
ABABABABABABABAB

Alternatively, what if we define C = average A and B. Then we draw as:
CCCCCCCCCCCCCCCC
Could special case when pixels are black and white and not do the averaging, assuming these are text and require clear definition.

Scaling generally: did some reading which suggested that you might get good results for this old pixelart stuff, by doing the following:
1. use nearest integer scale and nearest, from 560x192 to something somewhat bigger than your target.
2. Then use linear to downscale from that to your target.

Integer up and integer down, I saw something about this talking about audio resampling. So let's see here: we're normally 2x5. 3x7.5 is non-integer. So 4x10 is 2320 x 1920. Then scale back down.
ok using pixelart as the last step renderer I am not seeing any difference between bare pixelart and the "upscale first" approach. Maybe pixelart is already doing this.

Claude's got some discussion about modeling at the analog crt phosphor-dot level. Could be done with 4K, 5K (retina) and 8k displays that have sufficiently small pixels.
https://claude.ai/chat/a3a08084-6186-4f0d-9d95-59f3b0b4ffb4

This is an awesome compendium of SDL3 examples:
https://www.bilgigunlugum.net/gameprog/sdl3/sdl3_img_stretch

This is a good discussion on shaders to simulate crt subpixels
https://forums.libretro.com/t/ten-years-of-crt-shaders/22336/33

took a photo of the GS screen, I measured 1.36 AR for shr. but if I was at an angle that could be skewing it (could also have something to do with crt curvature). Redo this later. But that 1.28 has the benefit of being integer-scale for both X / Y and that's worth something.

Now need to put borders onto dpp. Then implement SHR mode in VideoScanner/VideoScanGenerator. Back to border mixing.
Draw borders first; then draw content on top; 7 pixels on left or right need to be rendered as alpha=0 so existing border shows through. Right now, bit->push() we use values 0 and 1 for, you know. However, what if we also include 0x80 as a flag bit to say "treat as zero but render transparent"? We could alternatively have a flag for whether the line is -7 or 0 offset, and set the alpha appropriately. (Need to always draw the Frame567 (580) at the -7 position.) Flag could work for full frame render..
what about VideoScanner.. here we spit out 7 pixels of blank (or trail) if we are the first or last byte of a line and rendering an 80 mode. Those are not being interpreted as video bits, we can just emit them transparent right there.

Also learned about highdpi displays. My Macbook has a native resolution of 2560x1600, in a 13" display, and about 230dpi pitch. This is "Retina" and how Macs obtain extremely clear text. We could detect if the user has a display of this type, and if present enable a . The dot pitch of these systems we're trying to emulate is 
MB screen is 11.25" and 2560 pixels means 230dpi. 0.116mm pixel size (not counting subpixels). AppleColor RGB has "0.37 millimeter tri-dot pattern". Suggesting 0.37mm dot pitch shadow mask triangular layout, or, distance between center of one to the next similar color.

```
 R G B R G
G B R G B R
```
In a shadow mask like the above, the dot pitch is the distance between the first G on 2nd line and 1st G on 1st line. I don't know if the measurements are correct but here is example layout to modern pixels:

```
  ....BBB.RRR.
  ....BBB.RRR.
  ....BBB.RRR.
  ............
  ..RRR.GGG.BB
  ..RRR.GGG.BB
  ..RRR.GGG.BB
  ```

  We can alias the corner pixels to provide a "rounder" appearance. Our target is about .4mm from one red to to the next and I think this layout is too big. 

  ```
  ....BB.RR.
  ....BB.RR.
  ..........
  ..RR.GG.BB
  ..RR.GG.BB
  ```

  This might be close: 5 x 3 x 4 where the 4 is the dot pitch; yes, close. No opportunity for antialiasing here unless we do it in the (otherwise black) border pixels. Yes, we can have an effective 4x4 pixel with the edges shared with the next phosphor dot. Some of that bleed could be legit anyway.
  
TIL that CrossRunner does a color-mixing mode when in 640. "solid color conversion". I didn't see a way to turn it off, but, it does make the desktop and all the colors you can set on the desktop to be solid. Apparently the AppleColor RGB did not have high enough bandwidth to display the 640 columns as discrete stripes. The algorithm is likely something like: current pixel is weighted average of this pixel and surrounding two pixels. Won't mix black and white. just b+color or w+color. Or c+color.

## Oct 28, 2025

Testing putting the shr stuff into VideoScanGenerator. however, VSG creates an Apple II bitmap. So there are some changes required here.

Remember, the pipeline is:

VideoScanner -> VideoScanGenerator -> Frame Render (NTSC, Mono, RGB)

VideoScanner: input: mmu bytes; output: ScanBuffer. get and queue video byte values as they are at a particular CPU cycle
VideoScanGenerator: input: ScanBuffer; output: Frame (bit). dequeue these and create a 560-bit-wide image
Frame Render: input: Frame (bit). Output: convert bit image into monitor output using different algorithms.

1. VSG will need to paint into a 640-wide buffer if it is to accommodate shr pixels.
1. VSG target is a Bit map, not rgb.
1. SHR does NOT generate a bitmap, we need to emit 24-bit RGB values. The place for that is likely inside Frame Render.

What if we do something like this:

(II) VideoScanner -> VideoScanGenerator -> Frame Render (NTSC, Mono, RGB) --> Composite the correct frame or mix of both frames.
(SHR)                                  |-> Output Frame (directly). --------^

VSG sees both II and SHR data. and based on video mode at that time it can emit to a different output buffer. Then a final stage composites either or both buffers.
Initially we will just display one or the other based on video mode at top of frame. Then we can try to ID if there was a mode switch mid-screen, and shape accordingly. This will involve shaders to create and map the images the way we want, to simulate the pixel clock change.

alright! I've got a shr image up in VideoScanner. Abeit with a fake gray palette. And a fake flags check.

I have gotten around an issue where when we instantiated a scanner, we were calling the wrong init_addresses. Now we have to call ->initialize() whenever we instantiate a scanner. I'm not super-thrilled by that. Do more research.

ok next step is to implement the flags that say "grab next palette bytes" and "grab mode byte". OK, have the cycles mapped out in the CycleTiming spreadsheet. 
```
46	6		Right Border		SCB	$9D00+vcount	1
47	7	X	Front Porch		Palette	$9E00+(pnum*32)+0	4
48	8	X	H. Sync		Palette	$9E00+(pnum*32)+4	4
49	9	X	H. Sync		Palette	$9E00+(pnum*32)+8	4
50	10	X	H. Sync		Palette	$9E00+(pnum*32)+12	4
51	11	X	H. Sync		Palette	$9E00+(pnum*32)+16	4
52	12	X	Backporch		Palette	$9E00+(pnum*32)+20	4
53	13	X	Backporch		Palette	$9E00+(pnum*32)+24	4
54	14	X	Backporch		Palette	$9E00+(pnum*32)+28	4
```

Display is out of sync in shr meaning I am generating the wrong number of Scan entries. Let's think this through..

video_cycle: original was: "emit video data whenever we're not in blank". So it checks !SA_FLAG_BLANK.
new is: emit data when: border, shr, scb, palette. Issue likely 192 vs 200 scanlines. So how do we know in VSG how many scanlines. Or do we care? Once vertical borders are in, we need to generate 244 (or whatever) period. But right, how do we know how many lines to process? This is where I think maybe we do need to put in the hsync/vsync entries. Then VSG just eats up to 262 markers. (it's always 65 horz). In one respect we've been thinking of this as "emit commands into the circular buffer". However another way to look at this is, the scanner LUT is also the instructions for VSG. There are instructions - i.e., is it border, is it 'active' area, is it 'get the scb'. Then there is the data - the video bytes PLUS video state so we know how to render the bytes. We don't need to replicate the instructions in the data stream? 
Give this some thought. I think we still need circular buffer because we may go past 17030 cycles in any given cpu execution run. That hasn't changed. Maybe the difference here is really, always emit 17030 entries per frame, instead of trying to shortcut sometimes.

Another approach: put in hsync and vsync markers. hsync means end of line; vsync means end of frame.

for any given cycle, we may:
emit pixel (if border or shr or old type non-blank)
load control data (if scb or palette)

for a scanline, there will either be 40 or 53 pixel elements, some other number of control elements.
So I am counting the pixel elements; but, I am also counting control as pixel. So I need to increment lineidx only on pixel elements;

ok so I am updating lineidx only as we consume video data bytes, and that is working!

Things remaining: 
[x] text colors  
[x] top and bottom borders. Combined, these are 53 cycles wide and 40 (or 48) cycles tall. So need a Border texture here that is 53 x 240 tall.
[x] we're cutting frames off a hair (maybe 2 1/2 scanlines?) early in legacy II modes, regardless of scanner.
[x] Detect 192 vs 200 scanlines of active area.  
[ ] Aspect Ratio correction in vpp.  

Top of content area is always exactly the same. Bottom varies - i.e., we have 8 fewer scanlines of border when in 200 mode. So, draw border first assuming 192 scanlines. Then when we draw the shr content, it will overwrite the top 8 lines of bottom border.

A thought occurred: have the Frame objects (optionally) tie to a texture. i.e. we can request local storage, or, texture storage and when we render we render into a locked texture area directly. Maybe the local storage could be done as a Surface, giving us the ability to perform higher level SDL operations against either?

ok, have the 3 scanners updated to use the new ScanBuffer commands (vsync, hsync) and VSG now: reset scanline on hsync, and return on vsync.

It's line 24; 4th scanline.  Well it's working inside GS2! I should have tested that before. maybe my texture in vpp isn't big enough and it's getting overwritten or something? or I'm not copying enough bytes?
MOTHERFLICKER! That was it. In the memcpy I was not copying the right # of bytes (567 scanline instead of 580).

So I should now have 200 scanlines worth of SHR data. Check my rectangles.. that's fine. I bet I am marking that as being in the blanking area... nope, but I had a < 192 that needed to be < 200 in VideoScannerIIgs. And now I have 200 scanlines.

We don't 'detect' it. VSG just spits out however many scanlines are in the ScanBuffer, under the control of the Scanner.
Whups. if I go into gr or hgr it hangs. set_color_mode line out of bounds: all the way up to 65535. But games were working fine.. weird.
taxman still works.. maybe it's mixed mode? try gr2.. still crash. ok it IS mixed mode. c050 followed by c053 crashes. 
ah well, fix it tomorrow.
ok, hitting 'x' in vpp causes crash. Definitely a problem with mixed mode.
even if I'm in text, if I hit the mixed mode switch, I crash. Check what set_video_mode does here..
in vpp I was instantiating frame_byte with large buffer parameters - which are larger than the actual buffer? which was causing render to exceed line width.
If instead you instantiate with 567 wide, that is the correct thing. The underlying buffer is still always 580, but 567 is the actual data width and controls what the renderers do. I should maybe reconsider that..
ok split is still crashing. 
```
        lores_p1[idx].flags = fl;
        lores_p2[idx].flags = fl;
        hires_p1[idx].flags = fl;
        hires_p2[idx].flags = fl;
        mixed_p1[idx].flags = fl;
        mixed_p2[idx].flags = fl;
```

So there are six lookup tables here. (This has been working, right?) lores_* is text and lores graphics, same memory locations. hires is hires of course. mixed must specifically be hires+text. I bet in calc_video_mode there are "invalid" entries being set with invalid vaddr. let's check for that.. not it.
ok we're exceeding on scanline 160, the first text scanline in "mixed" mode. color_mode is color 0 and mixed=1. 
ok, so we're generating a full text line, but then adding another character beyond that. hcount is 40. are we missing an hsync?
ahhh, in mixed mode, VSG is overwriting the hsync/vsync markers. ooopsie! FIXED!

ok, we are now checking for colorburst at the beginning of each frame only. That check should get moved to where exactly.. maybe should have a colorburst element?

Crazy Cycles 2 has 1-2 pixels that seem incorrect on the left edge on first demo.. I wonder if this is related to starting on index 6. Also CC1 on certain screens. Almost like it's stuff leftover from the previous scanline? Something wrong about scanline lead-in.. fixed. "last_byte" needed to be reset to 0 each scanline for the hgr code. So it WAS relying on some data from the last scanline. I feel like there was something in Sather about this somewhere. Research it. Yeah, it's UTA2 page 8-21, 2nd column. It's not the bit from the previous scanline, it's bit 6 of the byte scanned during HBL just before video goes active. Looks like Mariani (i.e. applewin) doesn't handle this either. Eeenteresting.

For vpp text background and text color, we need to include the color nibbles in the data byte. Stick them into two low bytes of the 32-bit data value.

So text colors.. hmm.. first off, text mode renders in white, or green/amber, depending on the mode setting. AppleII text routines do not render the colors, they only generate bitmaps. In NTSC560:
```
if (color_mode.colorburst == 0) {
  // do nothing
  for (uint16_t x = 0; x < framewidth; x++) {
      if (frame_byte->pull() != 0) {
          frame_rgba->push(color);
      } else {
          frame_rgba->push(RGBA_t::make(0x00, 0x00, 0x00, 0xFF));
      }
  }
```

that's it right there. Should be similar code in RGB and Monochrome. GS can render grayscale. need to look into that some more. But for now, let's say it's just NTSC and GSRGB. GSRGB already has the color tables! tee hee. 
```
  for (uint16_t x = 0; x < framewidth; x++) {
      bool bit = frame_byte->pull();
      frame_rgba->push(bit ? gs_rgb_colors[15] : gs_rgb_colors[0]);
  }
```

this is fine for AppleII. However, the cycle-accurate code needs to be able to change these colors every character. So we just have to have another byte of input data, that gives us the fg/bg colors for text at that moment. What if it's just the byte? 00 would still be a black pixel. Any other value would be byte FB, where F is foreground nibble and B is background nibble. To be applied only when "no colorburst". That might work. For II+/IIe we'd just always set the color values to F and 0. 00 would be black (two black values) as it is now. F0 white on black. NTSC and Mono can just interpret any non-zero as white/fg color at that moment.

```
        vvv read text colors here
(II) VideoScanner -> VideoScanGenerator --> Frame Render (NTSC, Mono, RGB) --> Composite the correct frame or mix of both frames.
                                AppleII -|
                                       ^^^ inject text colors here.
```

So to be clear about this:
1. vpp will read the text color values in VideoScanner; inject them as part of a Scan_t entry (the IIgs 32-bit); and VSG will inject those into frame_bit to feed into Frame Render.
1. dpp will read the text color at start of 8-line group render; and inject those into frame_bit to feed into Frame Render.
1. only GSRGB renderer will respect the text colors; the other two renderers will treat any non-zero as 'white' (or the mono color), and 0 as black.
1. Render class will have methods to set text_fg and text_bg as RGBA_t values - to be used only for non-RGB-text rendering.
1. AppleII and VideoScanner class will have methods to set text_fg and text_bg as nibble values.

So could have a little fun by implementing individual text color selection mode. The straightforward approach would be, put these color nibbles into say page 2 at 800-BFF. So you have a byte for the character, and a byte for the color. 

Another fun thing would be to allow software-definable text characters using 8x8 matrix (80 x 8 = 640, 25 x 8 = 200). So let's say it's still in the SHR buffer, but part of it is used for the character set. And ideally the text pages (allow 2?) are mapped linearly. 
80 x 25 = 2000 * 2 = 4000 bytes.  so have at $2000, $3000. 
And because we have color control, don't need to steal hi bit for flash/inverse; can have a full 256 colors. If we're doing this, use the PC character set or another one that has.
Some peripherals need bytes in the otherwise unused text areas. So ignore those.
Alternatively, implement it like the Videx, with the video memory mapped into CXXX?
Additional features: have hardware scrolling where we can move the start of frame buffer?
using mvn / mvp we can scroll in 28000 cycles; even at 1mhz, slightly more than one frame. At 2.8mhz, faster than a frame.
ok ok that's for later.

Instead of inverting every single text bit, why don't I invert cdata first/ Duh.
```
before:
Render Time taken:101833709  339445 ns per frame
Render Time taken:99777049  332590 ns per frame
Render Time taken:96198921  320663 ns per frame
to
Render Time taken:97616251  325387 ns per frame
Render Time taken:97821327  326071 ns per frame
```
maybe a hair better.

Minor issue: when in NTSC or Monochrome mode, and BG is non-zero, we get an all-white display. So these need to what, mask with 0xF0 instead of the equivalent of (bit != 0) ?
text color controls added to dpp; same issue with mono etc. So let's fix that..
That's not right..
I need to send the color (hi nibble) and the on/off (bit 0) so the receiver can read what it needs- bit 0 for mono/ntsc, bits 7-4 for rgb.
ok then!

[ ] Maybe instead of uint8_t I should have the bit-frame data type be a 1-byte struct to give it some readability and structure.

| 7-4 | 3 | 2 | 1 | 0 |
|-|-|-|-|-|
| Tixel Color | n/a | n/a | n/a | Pixel On/Off |

## Nov 1, 2025

So what's next?  Top and bottom borders. and aspect ratio in vpp. At this point, top/bottom borders should be pretty straightforward - just flag the appropriate border bit in the video scanner.

I need the transparency. I think we can use bit 1 above for that! Have bit - use bit. THIS IS THE WAY.

## Nov 2, 2025

so, border! Getting pretty close. Things I've learned so far - scaling from a single pixel to 42x5 pixels or whatever the scale is, using linear mode, that just does not work well. It creates a gradation from left to right that is just not correct. It also tends to want to mix with stuff that isn't exactly there. The only solution to that, would be to emit 14 pixels instead of 1 per 'cycle'. Which is certainly doable, though it's a lot more computation. So the border is just going to be kind of blockyish, unless I do that more horizontal pixels thing.

Well I just discovered that on a IIgs and AppleColor (at least) that there is -no- 7 dot shift when in double-width modes. This is good as it simplifies some things. To handle this, we can either:
1. not insert the 7 extra dots at all, based on a config variable in VideoScanGenerator, or;
1. remove them when rendering in RGB.

I suspect this was done because otherwise handling the border timing etc would not work right. As it wasn't for me here, ha!

[x] if we're in IIGS mode, have to draw the frame w/o 7-pixel shift
[x] test correct colors in lores, hires, and text fringe
[x] test correct colors in dlr, dhgr, 80col fringe.


Some other things I learned: 
1. on composite output, we do have borders. Including on a green screen. They are grayscaled.
1. also, (using a Monitor /// green) but in graphics modes, they are generated using ntsc bit patterns.

## Nov 3, 2025

So, it's when emulating a IIgs. Only the IIgs has borders. If we do the "RGB" display on a II+/IIe we still want the position shift. So I think we want to, when we're being a IIgs, set a flag in VideoScanGenerator to not insert bitshifts. We'll still use a 567 buffer because there's no point having yet another data struct.  It's possible I am going to have to deal with phase now? yes, in 40 modes, the phase is wrong in all renderers. Currently I hard set phase to 1 (inverted) because with the 7-dot shift for 40 modes, that gives us the correct phase at the first actual dot. Do I now need another way to tell the phase? This is a little complex. 

So maybe the simpler thing is in fact to remove the shift when rendering rgb. Except it's not just rgb, on a IIGS there is no shift on composite either.

If I keep this functionality in VSG, then instead of adding 7 pixels, it needs to produce a phaseOffset: 1=shifted (80) 0=unshifted (40). And the NTSC and RGB renderer would use that. ntsc would grab the phase offset per scanline from the colorburst value perhaps. 

ok, so let's think about this.. Two modes. shift enabled (for IIe/IIc), shift disabled. (for IIgs). Does not apply to II.

Shift enabled:
in VSG:
the bit buffer is only 560 pixels
add a phase bit to the colorburst value.
instead of adding 7 extra pixels to the bit buffer, we deal with this later on a per-scanline basis in the renderers.
40 lines: phase=1
80 lines: phase=0

in Renderer:
So the renderer also needs the shift enabled flag
pixel buffer is 567 pixels
reads phase bit from colorburst value
if phase=1: insert 7 blank pixels at start (40 mode, move whole image over a bit)
if phase=0: insert 7 blank pixels at end of line
use phase to guide color selection

Shift Disabled:
in VSG:
still set phase per above

in Renderer:
pixel buffer is 567 pixels but we will only ever emit 560 pixels per line, so 40 and 80 are aligned (just drawn with different starting phase)
still use phase to guide color selection
Do not insert extra pixels

Then, based on the noshift setting, we draw the entire frame 7 pixels to the left (shift enabled) or not (shift disabled) and 560 versus 567 wide.

In short, right now the presence of shifted pixels is overloaded with the pixel phase on each scanline, and this is decoupling those so we can control these independently. So really, shift enabled will only be set on the IIe. Borders will only be enabled on the IIgs (we want to skip the whole border mode if not using VideoScannerIIgs).

ok this is coming along. I do have an issue in a couple places where I am not copying enough pixels from output texture to the screen.. need to copy 567 pixels. May need to make sure we're emitting 567 per line..
got a weird one. 80 col mode doesn't work in: dpp, rgb. Fine everywhere else. wtf, over.

At this point I undid all that stuff with 0b10 ("transparent flag" in bit) because it was in the wrong place. It's much better having the scanline flag for the offset, and letting that control transparency (in the renderers).

Now I still need to add and respect the "shift" flag. Let's default that to false, allow setting to true. when using a iie scanner we need to set to false.

Well ok then! That wasn't so awful. And it's done, and looks great!

not done of course. 

[x] in SHR, we're drawing 7 pixels too wide. (fix in calc_rects)
[x] the right border is not quite right its offset - because the color goes down a line at right border, but then goes back up for last cycle. and it should be 

So this latter, let's discuss. In the video scanner, Hcount 0 to 39 is data; then we do right border, then hsync, then left border. Basing it on the 17030 cycle count, hsync is at hc = 11. I think these are off because the scanner is emitting the hsync at the wrong place. I can -sort- of patch over this by grabbing different pieces of texture, but that's a dirty dirty hack. 

Study the Sather some more and be real sure where the syncs are and that we have them in the right place.

Let's think about this from the perspective of the video beam first. there is these things in order: h.sync, backporch, left border, data, right border, front porch.

Sather has hsync as 49-52 but I have it (From the GS doc?) as 48-51 ? 

It says Vertical position increments after data cycle 39. 

There is a discrepancy between John Brooks description of cycle timing above and the IIgs document I have - he suggests hblank is 13 cycles; it says 12. He says right border is 6 cycles; it says 7. 

Wm has scanner start at hcount = 0, vcount = 0x100. Hcount 0 is beginning of HBL - "clock 40". vcount 0x100 is beginning of the first data line. So this is our cycle 0. ok yes, that's what we have here. Though in the spreadsheet what I call "hcount" is 40, which isn't right.

So we now have what is needed to create an SHR device. Things needed:
1. systemconfig needs the VideoScannerIIgs
1. register the shr softswitches
1. register the additional display softswitches
1. create the SHR texture and border texture

It feels like VideoScanner could just be another system device. When we boot a system it will have one (and only one) VideoScanner. Or could these be a 

IIGS-video Softswitches

```
      |   7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
$C029 | SHR  | MM  | DHR |  reserved             | EBL |
$C022 | Text FG                |         Text BG       |
$C034 | RTC                    |      Border Color     |
$C021 | M/C  |   reserved                              |   
```

M/C: 1 = b/w (grayscale); 0 = Color (Composite)
SHR: 1 = super hires video; 0 = legacy Apple II modes
MM: 0 = "apple iie memory map"; 1 = "shr memory map"
DHR: 0 = dhr in color; 1 = dhr in black and white
EBL: if 1, 17th address bit used to select main / aux bank. data bit 0 used as 17th address bit. <? what?> if address bit 0, memory softswitches determines bank as on IIe. 0 = 17th address bit ignored. Default in Monitor mode is 1.

I think EBL is that thing that will map ALL even/odd banks to bank 0 and bank 1. 
Writing C1 to C029 enables SHR mode.

When Display is set to Monochrome, we get grayscale output on the Monitor ///. Horizontal lines are filled in. I don't see any bit patterns. Since we're greenscale, the scale is provided by modulating the luma.
When set to Color, we get a composite bitstream rendering on the Monitor /// (e.g., an shr display is shown as a composite bitstream, as is gr, hgr, etc). Text (white) shows up as you'd expect too, the normal text bits are on. All pixel brightness is modulated by the luma. So 'orange' is not just the orange bit pattern, it is those bits with luma also set for orange.

So it must be mapping color to 16 colors and then perhaps also adjusting the luma to try to approximate the colors better. I wonder how good a job it does. And maybe that is one of the other patents.

## Nov 4, 2025

The videosystem is a repository of methods for managing the display. The stuff that calculates the display rectangles (esp for borders) will go in there, replacing the code there now. It also has update_display which drives video generation, by calling frame handlers in order by weight, stopping after the first one claims the frame. That's how the Videx code works. Now the Videx is a little different because it does not do any cycle-accurate stuff. In display, there are conditionals for full-frame or cycle-accurate. 

SHR: weight 1, return true or false
normal: weight 0

current update_display_apple2_cycle:
call VideoScanGenerator
call display render

update_display_iigs:
  call VideoScanGenerator(with border and shr texture pointers)
  render border
  if shr, render shr display
  else render legacy display

When doing GS mode we also need to render the border in legacy modes. So have a separate frame_processor callback for GS. Then there are less conditionals to confuse things.

I think the logic here is pretty tightly bound up with display.cpp for better or worse and was designed to drop in to that. So be it - no separate shr device. (Could always segregate these out some time in the future if there is a reason to).

Where should the window sizing stuff live. 

Roadmap:

[x] refactor Frames to allow using locked texture region as buffer - for performance and simplicity  
[x] frame allocates its own texture  
[x] render_frame modified to accept texture, src, and dst rectangles. 
[ ] implement correct aspect ratio and new window size defaults  
[ ] hot key to generate "ii-friendly" and "gs-friendly" window size toggles  
[ ] function for border render  
[ ] function for shr render  
[x] write update_display_iigs
[x] register distinct iigs frame processor callback  
[x] see if pixelart obviates need to draw Videx twice for brightness (no it did not)  
[ ] The PrntScrn button is referencing illegal memory. the pointer from LockTexture is not valid after unlock.  
[x] calculate_rects needs to center display in window esp in fullscreen mode  
[x] implement border color for Apple2_Display  

Currently Frame uses a static array defined in the template as opposed to allocated memory. First we need to figure out how to use regular array semantics. Then test performance difference if we malloc vs static. Claude suggests no difference in optimized code. So:
1. change to malloc'd memory. Test. very slow.
1. switched counters to 32-bit and that improved back to original performance.
1. add a template parameter to tie to Texture instead of using Malloc.
1. the lock texture / unlock texture semantic will need to be replicated, with let's say open and close methods.
1. add constructor checks to ensure supplied width/height do not exceed bounds.

Which frames are we copying into textures: shr; border; Frame560RGBA. Maybe these can get less unclear names.

After switching to malloc'd memory, performance went from 360uS to 530uS. Ouch!
After changing to 32-bit hloc, it improved to 300uS. What. This is all compiler and cache nonsense. The compiler can create significantly faster code by assuming no aliasing. Let's check some more __restrict applications here..
Actually maybe we should check -JUST- int16.
It could be that the issue was the combination of int16 with whatever code calculates indexes. Because I felt like I should not -lose- performance. If I switch back to using local memory+memcpy and it's WAY faster then I'll know.
Tried removing the row[] lookup optimization - yowza, that is a huge performance hit.

We are defining frame size in *3* different places: the template, arguments to the constructor, and the texture. This is madness.

I have the code sort of back to the 'old' way (except for 32-bit indices) and I'm at 300uS in F1 40-col text.
memcpy to texture; 32-bit indices; F1 40col; 300uS
memcpy to texture; 16-bit indices; F1 40col; 475uS
direct to texture; 32-bit indices; F1 40col; 295uS

THE reading in emu at this point is 258-264uS per frame. So we got a 15% improvement there. 

Let's see if index size makes a difference to ScanBuffer.. nope, nor did reducing buffer size to 16384. So this 16-bit effect is limited to indexing into memory that's been malloc'd.

"On the software renderer, you can write directly to the final destination with LockTexture, whereas UpdateTexture will need to make a copy from your buffer to ours.
On the other renderers, it's probably the same (the GLES2 renderer, for example, literally just calls UpdateTexture for its UnlockTexture implementation)...but if you ever need a software renderer, this is the thing that would benefit most from texture locking.
But in real life, with actual GPUs, it doesn't really matter a whole lot."

So this seems to be saying if I just use UpdateTexture it will be just as fast on virtually every computer. And a fair bit simpler. 

## Nov 5, 2025

well we have support for a minimal "sorta looks like a VidHD" and IIgs Video in the Apple IIx! In Total Replay, if you hit Control-Shift-2 you get a slideshow of specifically all the SHR box art. There are, I dunno, upwards of 200 maybe. That's a great "demo" of SHR graphics.

Also got colored text support implemented in emu. The init_display arch is creaking a bit - lots of IF's etc that are hard to read. Will have to work on that code some more. But this is great for demo video purposes. And maybe a nice long-form one of just the slideshow.

[x] AppleII_Display renderer is broken - ludicrous speed - but only using VideoScannerIIgs.  

oops, I did not even consider how to do border in A2_Display. Since there is no cycle-accurate business to worry about, could we simply have a 1-pixel texture and blow that up to full screen size? why not? the gpu is much more efficient, and then we're also only updating a isngle pixel texture instead of whatever. Alternatively, instead of doing a texture update thing (kinda hinky) can we just draw rectangles in the correct color? sure why not. Using the same rectangles we calculated.

## Nov 6, 2025

reviewing RGB finally.

First off, do in hgr:
2000:87 2001:87

the pixels at 2000 are wrong. they should be white just like the pixels at 2001. So there's an incorrect lead-in. We should probably latch and use the first color of actual pixel data - NOT assumed leading 0's. It's possible I broke this just recently by getting rid of leading 0's in the frame_bits.
[fixed]

Second test case: 11011 = 1b
2801:9b
this resolves to 560 pixels as (shifted) 1111001111
This should render as two white, a black, and two white pixels. in ntsc, this gets plenty of fringe such that the black in the middle is actually obscured. But that's likely what someone would want. This is basically what we get in the choplifter score counters, which is wr0ng.

[ ] another bug: in crazy cycles, when the line starts as text then switches to a hires mode, I believe it's trying to render the whole line as no-colorburst, and so it's expecting color nibbles with the (hires) bits but of course there aren't any. This is because it thinks the whole line is txt.. the line -should- be monochrome. but need the bits!  

## Nov 10, 2025

Analyzing the graphics conversion routines in CiderPress2. What I find is that hires and double hires use very different approaches. The hires is similar to my old "rgb" code in that it's looking at hires bits, not 560 bits. So that won't necessarily be relevant. The current RGB renderer is actually working really well EXCEPT for places where we go white to black to white and it fills in all with white. 

## Nov 11, 2025

Went down a rabbit hole - I was curious so I checked out the GUS emulator source code. And I found this:
```
The basic idea is to get 11-bits worth of IIgs Hires/DoubleHires
pixel data and lookup the pixel colors in this table for 4 distinct
Macintosh pixels (560 pixels across being the mimic'd IIgs screen).

After each 4 pixel color values are figured out, the 11-bit "window"
is advanced by 4 bits in the IIgs pixel data stream, then the
process is repeated.

The Hires "Mirror" tables are needed to bit-double the hires bits
into Double-Hires Bits and display everything according to the
double-hires display algorithms.

According to John Arkley (who wrote the original Apple IIe card code)
he wrote a state machine for the set of PALs that were in the Mega II
which decode some lines into a color signal.  He then fed all possible
values for each of those lines (which makes me think there were 11 lines)
into the state machine and recorded the value which was output.  There
are 2048 outputs because there were 11 lines of input to John's original
state machine (and I would guess that also applies to some set of traces
inside the Mega II in the IIGS).
```

where have I heard this before? haha. 
```
// Prev = 0000				M1 A2    Prev Sorc Next    Video Pixel Colors
		0xFFFF,			//  00 00 >> 0000|0000|0000 => Blk.Blk.Blk.Blk
		0xFFFF,			//  00 40 >> 0000|0000|0010 => Blk.Blk.Blk.Blk
```
The LUT is organized like so: 16 chunks (by 'Prev') as the high 4 bits; then Sorc (current 4 bits); then a 3-bit lookahead.
And that is shifted left 1 bit because the pixel entries are 16 bits (2 bytes) each so we're doubling that.

So what we'd do here is have an 11-bit shift reg. We shift in from the right. i.e.
shift = (shift << 1) | newbit
At the left edge of screen, we start with shiftreg = 0. Then preload with 7 bits;
then iterate 560-7 = 553 times more. at the end of the scanline (last 4 bits) the 'next' bits need to be just shifted in as 0. So break it up like I do in rgb today, which is:
preload
iterate 560-7-4; (normal, with ->pull)
iterate 4 (shift in 0)

we can run in a test program and see what we get with my test patterns.

ok. So we preload 3.
then the first iteration, we load 4 more. <- 139 iterations
then load 1 last bit and 3 0 bits, and clock those out

that looks amazing! It is for sure the GS algorithm.

The double hires table is not laid out the same at all, and pixels are laid out backwards (e.g look at what should be smoothly curving edge in the apple pic). So they must have manipulated them somehow due to differences in their code. So I will need to unravel what they did, or, separately try to figure out a phase difference when doing color lookups. Likely the best approach is to generate a new table for the DHGR phase offset.

// Hires - 2nd entry (index 1)
Prev = 0000				M1 A2    Prev Sorc Next    Video Pixel Colors
0xFFFF,  			//  00 40 >> 0000|0000|0010 => Blk.Blk.Blk.Blk

// ("Dbl hires") - 2nd entry (index 1)
//  - I suspect just means phase shift. But unsure.
0xFFFF,				//  08 00 >> 1000|0000|0000 => Blk.Blk.Blk.Blk
                                     ___
for dbl, the bits shown there are in reverse. The last chunk, the final bit never is 1, so I think that is "next". BUT that is the high bit of the index.
So the leftmost bit here, is the leftmost bit in the data stream.

ok, so rotating 90% is sort of right. Pink to Blue is good. let's just try thexder for fun hehe. That doesn't look that bad actually. Maybe I can suss out the mapping?
The color palette should be exactly the same, yes? And just bitshifted or something? Because that's what "phase" means here. 90 degree phase is 1 bit difference, because the entire color wheel is 4 bits. (14M / 4 = 3.58).

Compared photos on the GS to my current output rendering. Hires is exactly spot-on. Lores on the GS does -not- show any gaps/artifacts between bars, whereas I do. I surmise that the GS handles lores specially. i.e. the way I did in the very first iteration of display code. hah. Circling around back to the start.

ok, so I think let's reorder the table. the dhgr table is clearly backwards compared to the hgr table (why? don't know!)

for each index in the table;
  get value by index;
  exactly reverse bits;
  store value in new index
forend
print out the new table alone with the bits and the just like ...

nope!

```
2100:11 44
supposed to be two sets of red pixels
1 0 0 0 1 0 0 -   x x x x x x x -  x x x x x x x x - 0 0 1 0 0 0 1 -
                  0 1 0 0 0 1 0    0 0 1 0 0 0 1 0
```
literally just 1 0 0 0 repeating. 

well here that is:
                           prev | cur| new
0xFFFF,				//  08 00 >> 1000|0000|000 => Blk.Blk.Blk.Blk
0x3333,				//  40 08 >> 0001|0001|000 => Red.Red.Red.Red
0xBBBF,				//  38 07 >> 1110|1110|000 => Aqu.Aqu.Aqu.Blk real index is 000 0111 0111

Aqua is what.. 14? yes, 1110. So that tracks. 
So I have the real index. but what we need is an index that has:
1 1 1 0 1 1 1 0 0 0 0 <- newest bit, lsb
and that's just the reverse.

oh that's a confusion here. I am preloading 3 bits. That is the lsb 3 bits in the index in the hires table.
But what that means is, we're doing a 3-bit lookahead.

## Nov 13, 2025

well ok duh. I was doing that the superduper hard way. It WAS just a matter of a phase shift. i.e., the regular hires table works just fine if I preload in only two bits instead of three. I also have it grab an extra one at the end.

SO. Now that leaves lores and double lores, which are interesting cases. Testing on the GS showed they are -not- just handled with the hires logic because there are no artifacts.
Space Quest looks good. splash screen on Skull Island looks great.

Yeah, Skull Island dlores shows perfectly rectangular and consistently sized dots.

## Nov 14, 2025

Perform testing to make sure we're not missing any pixels anywhere due to the preloading: hcolor=7, hplot 0,0 to 0,191 and hplot 279,0 to 279,191 show basically the same on ntsc/rgb. now how do I do that in dhgr.. yah, so on the right side of screen, 1F shows some pixels, 3F one more, but 7F does not add one. 

put into double hires mode
2027:7f
after the main loop, shiftreg contains 0x1F. 
shiftreg becomes 508 0x1FC. That lookup entry is four white pixels.  the previous emission was b.b.w.w. So we have a total of 6 white pixels. and really ought to have 7..
but I'm not missing any data here. that's just the rendering.

let's see what something like a2desktop looks like. the mouse on the very right edge, you can see one pixel worth of angle at the top of the cursor. the whole screen shifts to the right one dot going from ntsc to rgb mode. Something to review later.

## Nov 17, 2025

oh my pal scanner is crashing because I'm not iterating enough cycles in the main gs2 loop.
ok well "first draft" PAL is in and sort of working. Things that remain to be done:
[x] refactor the speaker for hopefully the last time.  
[ ] fix mockingboard to use new cpu clock construct  
[ ] fix mouse to work properly at variable clock rates  
[ ] make cpu clock construct a class so it's less janky  

The speaker is getting ever so slightly out of sync in PAL. Let's double check ntsc.   

Speaker enhancements required:
[x] refactor to use 100% fixed point using large uint like I did in new synth test code  
[x] examine the "two close clicks cancel each other out" phenom which implies going not from 1 to -1, but from 1 to 0.  

## Nov 18, 2025

SpeakerX is a small module that does the actual conversions from blips to modern sample stream. We want to create a new Speaker class that encapsulates the following:

event queue
recording
management of clock rates, input rate, output sample rate, etc.

We also want to do something similar with the Clock concepts.

There is some occasional crackle; but also, in PAL mode the speaker gets out of sync still (even though I tweaked a few things). it's likely due to math errors with fractional samples.

So key elements of the new (5th? haha) implementation:

samples: we must draw an integer number of samples each frame.
We must also draw exactly the correct number of samples per second (also integer).
input rate: currently based on CPU effective speed at any given time. Must be somewhat arbitrary.

this is how I ended up with 44343 - it's almost exactly an even number of samples per second given our weird fractional frame rate per second.

input rate is: number of cycles per frame * frames/sec. e.g. 17030 * 59.9227 = 1020484. (3.5 rounded up actually).

Beep Sound on the GS - we're going to have to track this on the basis of 14m? Because 'speed' will change pretty regularly. i.e., it's just counting different divisions of 14m, not in any even way, but it can/will be quite irregular. The same principle applies even to 1mhz as time between certain audio cycles will be stretched 65th. (GS CPU is essentially an "accelerator" that periodically slows down for 1mhz speed, so other "accelerated" speeds would work similarly.)

Now - regardless of input rate, the number of input cycles per -our frame- is a whole number. 

Maybe the thing to do here is go back to the thing where SDL3 pulls audio data, instead of us pushing. the generate routine can always pass it some whole number of samples, based on fractional contributions and fractional input cycles. It doesn't have to always return the requested number of samples?

now here's an idea, since we can pass more or less arbitrary streams to SDL, what if we let it handle the conversion? like, we'd just give it input rate of cpu cycles per second; each chunk of data it would ask for would then be that many unsigned 8-bit values. Each frame could be quite large. I think we should test that approach. Their resampling code is likely to be quite good. 

## Nov 19, 2025

Well, the results are mixed. It is quite a lot of data we're throwing at it, so there is some CPU usage and maybe cache contention. It's a little buzzy but I'm not doing any filtering on it, nor am I doing the slow fade to 0.

At 2.8mhz, process time per frame is 248uS average. At 1MHz, it's 86uS. So scales roughly with the 'speed'. And significantly slower than my code. If we were to switch to 14M timing for this it would be well over 1ms, and this is without the filtering we likely need. So I don't think this is a good approach - the SDL3 code and/or just the sheer volume of memory bytes being thrown around is not efficient.

Alright, let's review my code again, but switch it to full fixed-point processing. Since we're dealing with negative numbers, use int64_t instead of uint64_t. Then we should get some stuff for free (e.g. sign extension). Instead of bit-shifting, we need to code constants using * and /.

The algorithm is this:

generate_samples
  iterate a whole number of samples desired
  start with contribution=0
  calculate what fractional cycle this sample ends on
  if there was any leftover fractional cycle component add it now
  iterate whole number of cycles_per_sample, add contribution.
  (there is also the optimization if the next tick is after this sample, we multiply once to  do the whole sample).
  add any remaining fractional cycle contribution to this sample.
  decay polarity
loop

At 1mhz there are about 23.14 cycles per (44.1KHz) sample. 
At 14.31818MHz there are 324.675 cycles per (44.1KHz) sample. That's a lot of looping and math - worst-case. best-case is same as now where most samples are optimized out.

Variables:
  cycle_whole
  cycle_fractional
  polarity
  polarity_decay

The issue here is that with escalating CPU frequency our workload increases here dramatically (granted, many many of these will be optimized out). Also, you can't possibly generate sound at these frequencies. Practically, anything over the 44100Hz rate cannot generate human-audible sound. So is there an optimization there? Can a tick be quantized down to 44100 in some way. Well that's what my optimization does. I had Claude analyze the code and Claude thinks it's already pretty heavily optimized. We could get somewhat fancier. Currently I only optimize if we can convert the entire sample with one, as Claude calls it, "rectangle integration". Instead of ticking cycles, then, what if we tick *rectangles* at a time.
So the loop is
  for each sample in (whole number of samples in this period):
    start with current rectangle, or calculate the next rectangle.
    identify portion of rectangle that is in this sample;
    if (rect >= sample) add contrib(based on sample) and go to next sample;
    if (rect < sample) add contrib(based on rect) then continue inside current sample;

calculate next rectangle means, pull next event and create rectangle of that size. if we have run out of samples, we have an underrun and should return some reasonable replacement sample.
https://claude.ai/chat/6ad0e230-0c8e-44a7-b06b-906f2cbbafbc

So let's say we reserve 12 bits for whole portion and 52 bits for fractional portion. (This could also be 12 bits and 20 bits using 32 bit values). (2^20 is one millionth, which is pretty precise). I would still like the algorithm to work with arbitrary input and output rates.

```
Variables:
   uint64_t cycles_per_sample
   uint64_t rect_remaining // remaining portion of rectangle from input square waveform represented by events
   uint64_t sample_remaining // remaining portion of current sample being constructed
   int64_t polarity
   int64_t polarity_decay; (or some other clever math to decay polarity)

calculate cycles_per_sample = blah blah (so it's responsive to speed changes)
sample_remaining = cycles_per_sample

for (i = 0; i < num_samples; i++)
  sample_remain = cycles_per_sample

  while (sample_remain)
    if rect_remain = 0, 
      if an event, rect_remain = next_event_time - last_event_time; flip polarity; last_event_time = next_event_time;

    if (rect_remain == 0)
      add_contrib(sample_remain, polarity); sample_remain = 0;
    else if (rect_remain >= sample_remain)
      add_contrib(sample_remain, polarity); rect_remain -= sample_remain; sample_remain = 0; 
    else
      add_contrib(rect_remain, polarity); sample_remain -= rect_remain; rect_remain = 0;

```

if there are not (at this time) any next events, rect_remain can stay 0? and we chuck in whatever's left of sample as the contrib.

Now, a key thing here is for either:
main loop asks for correct number of samples in each request to ensure we emit exactly 44100 samples/sec, or,
we use the callback type.

ok! I did initial implementation with doubles, not fixed point. I had a few tweaks to the algorithm above, but it basically worked first time. How bout them apples!? I guess that's the W you get from doing it 5 times, lol.

Some perf testing: at 1mhz, average 19uS, low: 2uS, high: 48uS
At 14mhz, low 2.5uS, high: 47uS, avg 18uS.

The PWM programs caused more 'load' on the speaker, as you might expect, because there is a lot more flip-flopping back and forth. using a very large EventBuffer might cause some slowdown as the modulus causes a divide instead of a bitshift. (This is why we had the buffer be a power of 2).

Now here's what I think is going to happen: using 14M markers but 1M actual data source, we'll have very low utilization because it will be the same O (flips) just with the numbers scaled up some.

So next step, reimplement this using fixed point.

## Nov 20, 2025

First, clean up all the unused stuff from four generations of code, lol.

Testing a few different options: double vs float; use a pre-calculated sample scaling factor instead of doing a divide -and- multiply to scale to the integer output.

One thing I noticed: I can select arbitrary output rates, but if it's not evenly divisible by 60, then I get some super high pitched aliasing. This makes some sense as I'm missing some fractional data here by only using an int. Not a huge issue, but something to keep in mind. It's certainly far less erroneous than the current gen code with poorly chosen output sample rate.

The input sample rate also suffers from some high pitch aliasing when playing for example speaker_turkish at arbitrary input rates. However, not with lemonade. I will suspect this is because the PWM approach taken by the music player relies in some way on the 1020484 clock rate when generating pulses.
At 1MHz tho, turkish is super clean.

Timelord has a high pitched whine even at 1mhz. We're definitely going to need to use a lowpass filter.

double vs float is only a hair slower, likely due to just memory bandwidth. On a 64-bit ARM. I can't discern any difference in audio quality.

At 14318180 cycles per samp is 325ish. That's as fast as we'll try to run the code. (Though, should try it at 50M for lolz). How many bits do we need? 12 bits whole (up to +/- 2048) + 20 bits fractional are probably good. Do we need +/-? What if we just track it between 0 and 1, then apply a DC offset when scaling the output sample? Hm, this goes back to that video about how the Apple II toggle works exactly.

https://www.youtube.com/watch?v=o7zE7rPMarU

From baseline, voltage starts at 1.2V offset, then settles to 0.5V, where it is held for 30ms, then relaxes over the next 70ms.
If we toggle speaker low, and leave it for 30ms, it will relax back to high state at end of 100ms. Next access then tries to pull the voltage high, but it's already there, so no speaker movement.

so, I get double clicks because I am not taking the above into account. Also Mariani does the same. Virtual II get no clicks at all from single C030 accesses. KEGS also doubles up the clicks. Only OpenEmulator currently gets this right.

mathematically identical is: start at 0, then click high goes to 1.0, and click low goes to 0.0, and we decay from 1.0 back to 0.0 just like we do now. Need to check the coefficient tho to match the 70ms dropoff.

Will need to see about applying a DC offset here?

So polarity toggle needs to go from 0 to 1 and to 0. Instead of polarity = -polarity, we do polarity = 1.0 - polarity. or polarity = (polarity == 0) ? 1.0 : 0.0;

OK, coeff of .9999 results in still having 0.73 value at 70ms mark. Let's tweak that down.. 0.9990 for testing right now.
But the other part of this, is the hold for 30ms. I can set a hold_counter and tick that down, and only when that number of samples has elapsed do we then start to decay.

## Nov 21, 2025

12 bits whole, 20 (or 52) bits fraction for all these values.

The fixed point version is coming along! However, there is some weird clicky distortion after sounds play. This could be overflow of a fixedp number somewhere. It doesn't seem to happen if sound plays continuously.

Doesn't seem to make a diff if fract bits is 10 or 20.. maybe i'm overflowing 16-bit output. look into that.. ahh, yes, I am overflowing the output result. eeenteresting. by how much?
oh, I think the polarity decay was actually operating in reverse, lolz. IT WORKS! IT IS HAPPENING!!! fixed point, but still 64-bit. I tried 32-bit and it all played with large gaps of silence being super compressed. That would almost certainly be the event timer and rect_remain overflowing now. If you're just processing one frame at a time, you could bypass this issue. i.e. process events as relative to start of current frame.

Observations: 20 bit fract works great; 10 bit seems good; 8 bit is great on lemonade. 8 bit not bad on turkish - might be some slight increase in distortion.

it occurs there is yet another optimization we could add: check to see if there is no event before the end of the current audio frame, in which case the entire frame is all the same sample and we can skip a loop of 735 

this code is likely to blow chunks if we ever catch up to end of buffer. So we do need a check in there for that... NO I think it's ok. If we don't get any new event from peek, then rect_remain is 0 and we fill the rest of the sample with sample*polarity.

if we have a superduper long gap, we need to create a fake rectangle to fill in the current frame, instead of trying to consume the entire (perhaps superduper long) gap.

Kent Dickey reported a real GS clicks on every $C030 hit, and I confirmed on my GS. So, the speaker behaves differently in this respect between a II(/+/e) and a GS. Fun!  SO. What's the easiest fix here? For my purposes I'll just use the floating point version, and have a setting for which algorithm is used to toggle polarity (either 0 to 1, or 1 to -1).

I theorized this is because the GS needs to perform audio mixing with the ensoniq and couldn't do that with a permanent DC offset. He also suggested the volume control and amplifier.

samples per frame: 44100 / 59.9227 = 735; 59 * 735 = 43365; 

```
Practical implementation using a fractional accumulator:

Start with remainder = 0
Each frame: remainder += 0.94815
If remainder ≥ 1: emit 736 samples, remainder -= 1
Otherwise: emit 735 samples

This gives a pattern where you emit 736 samples most of the time, and only occasionally emit 735 samples (roughly 1 out of every 19-20 frames).
```
this makes sense, should work. This concept is all over the audio code! remainder can just be times 10,000 or times 100,000, and subtract 100,000 or 1,000,000. 

During nap, I considered how to handle running out of samples. if there are no more samples in this frame, assume a fake "event" at end of frame that does not toggle polarity. Thus a rect will never be much longer than a frame. We need to know what cycle is at the end of the frame, so that needs to be an argument that is passed in.

In the test harness, there is -always- a next event. Until the last, when we shut down the simulation.
In emulator, we will have situations where there is not a next event quite regularly - for instance if there just have been no clicks for a while. I will be processing a frame at a time, asking for a certain number of samples to execute that frame, but not necessarily exactly.
```
If no event;
  if there's a next event, current code;
  if not, pretend there is an event;
    from now until start of next frame;
      event_time = start of next frame;
      rect_remain = event_time - last_event_time;
      last_event_time = event_time;
      ;; do not change polarity, or reset hold_counter;      
```

This will keep event and last_event synced up to our input stream.

To simulate this in the test harness, we need to keep track of frame cycle counts, and only feed events into the buffer as they occur.

ok, I am trying to implement the above; however, num_samples and frame_next_cycle_start don't equate. I'm doing 17030 cycles and 735 samples; yet 17030 / 23.14022.. is 735.9478.
Maybe I need to return less than the requested number of samples?
I feel like the issue is that we're adding too much time when breaking across the frame, based on incorrect cycle count or something.

## Nov 22, 2025

ah, no that's not the issue. The problem is this:
```
[728] (10.774694 - 0.000000) 5120
  rr < sr 10.774694 23.140227
999999999999999999 17008 16880
  rr >= sr 128.000000 12.365533
[729] (115.634467 - 0.000000) 5120
  rr >= sr 115.634467 23.140227
[730] (92.494240 - 0.000000) 5120
  rr >= sr 92.494240 23.140227
[731] (69.354014 - 0.000000) 5120
  rr >= sr 69.354014 23.140227
[732] (46.213787 - 0.000000) 5120
  rr >= sr 46.213787 23.140227
[733] (23.073560 - 0.000000) 5120
  rr < sr 23.073560 23.140227
999999999999999999 17008 17008
  rr == 0 0.000000 0.066667
[734] (0.000000 - 0.000000) 5120
```

We're adding the right rectangle, but, the wrong polarity. Because 16880 was a real event, we need to flip that. Later, it's only if the previous event was a fake event, we don't flip polarity. So we need state "last_event_fake": 1 = last event was fake, don't flip. 0 = last event was real, flip. When we see a new event, that's when we create the "rectangle that just passed" and generate samples from it.

look at the boot beep. We start with polarity=1 (why?). Should be 0. And last_event_fake set to 1.
last_event is 0; we read the first event at 11966. Since lef is 1, we do not flip polarity (still 0); and set lef to 0. The rect is 11966.
when we run out of that, we read the 2nd event, which is 12512. Since lef is 0, we flip polarity and set lef to 0. The rect is 546.
...
we're still in the same frame. last event was 16880, **there is no pending event**. So we create a fake event at 17008; since lef is 0, we flip polarity; and set lef to 1. the rect is 128.
**might need to bail if last_event == frame_next_cycle_start**
when we get into the next frame, we encounter the next event at 17426. lef was 1, so we don't flip; set lef to 0. then we keep flipping as normal inside this frame.

Now if there is NOT a new event inside this frame, we will be out of rect; there is no event, so create fake event at 34016; since lef is 1, we don't flip; and we set lef to 1. So we "maintain and hold" the current polarity the entire frame, with sample decay of course.

I seem to have it playing ok now except during "live playback" I am getting some dropouts late in a 10-second synth - but in the wav file, there are no problems. So that is going to be not feeding enough samples to sdl.

I tried the remainder method suggested by Claude and that did the trick. no more running out of samples. Will still need a way to deal with running out of samples due to macOS being dumb and taking 1.5 seconds to open the damn file dialog.

## Nov 23, 2025

ok! I've got SpeakerFX integrated into GS2 now. The frame routine does the remainder stuff, and, it seems to be working well in PAL mode. Leave it running until 3pm and see if the audio is still in sync.

What isn't hooked in yet: 
I am also feeling like the frame rate and some of those other timing parameters ought to be kept in the Class, so it's all in one convenient place, even if we don't use it inside the class.

[x] samples_per_frame isn't correct. and we don't use it inside the module. Move it out for debug.    
[ ] allow output rate to be configurable with a const in speaker.cpp  
[x] fix reset() to resync audio after it being off for a while in LS.  
[ ] Detect audio de-sync due to MacOS dumbness and flush queue and call reset() during frame handler  
[x] change speaker to work based on 14M clock.  

after LS last_event_time is reset, but stays frozen. is it because it's in the past or something? Hm. maybe just switch it now to 14M. Get some real work done first!

testing! the French Touch SSS demo works in both ntsc and PAL. On PAL, the screen timing is good. However, there is some problem with mockingboard in PAL mode. 
shufflepuck is actually working just fine! I took 17030-specific stuff out of mouse.cpp a while back I guess.

The MB code does have one hard reference to 1020484 in it. MB is working at faster CPU speed (i.e. 2.86 and 7.1) so it's not likely anything to do with 14M. Sound finally came out like 5 minutes after I booted the MB demo disk. Weeeird. And it was feeling slow. slower than a half percent. Last_event and Last_Time are out of sync, badly. And we're not emitting enough samples per frame to keep up. Oh maybe that is the issue and we're inserting some huge number of excess samples when we underrun..

weirdly, if I hit P to play, Ampl a/b/c and last event/last time update, but the tone values do not. Am I directly changing ampl instead of queueing the changes? That could be my longstanding MB bug..
I have no clue what is causing this.

For posterity, an amazing math treatment of Apple II speaker simulation / conversion.

https://gitlab.com/wiz21/accurapple/-/blob/wozwrite/speaker/maths.pdf?ref_type=heads


(side track) This mouse implementation is compatible with the actual Apple ROM. I can use this to fix up my implementation and have it use the Apple ROM.
https://github.com/oliverschmidt/mouse-interface/

## Nov 24, 2025

the MB code should never be throwing these "timestamp is in the past" things and yet it is. examples:
```
[Current Time:  1183.432312] Event timestamp is in the past:  1183.430920
[Current Time:  1183.432312] Event timestamp is in the past:  1183.430989
[Current Time:  1183.432312] Event timestamp is in the past:  1183.431057
```

So the event timestamps are just c14m / 14318180/. They have to monotonically increase. In theory, current time should never go past our current frame_end_c14. But, that must be what's happening. There are likely rounding errors in here.

Similar concerns as speaker:
1. we nominally must generate 44100 output samples per second
2. Because of our odd frame rate of 59.9227, we are currently using a sample rate of 44343 (divide by 59.9227) to get us 740.003 bytes per frame.
3. Should use the new "remainder" method to figure out how to generate the right samples/sec based on 44100, instead of hacky 44343.
4. unlike speaker, mb doesn't have an input data stream, it is synthesizing outputs based on programmed frequencies and changes these during synth at times dictated by user input.

Should use c14m as the event time. 

I guess the thing to do is rip this out into a test harness and go from there, integrating in the new synth code. I already have the synth and envelope stuff, need to add noisegen.

For ease of testing, must create a recording bit to record MB register changes into a file. For just testing synthesis, that ought to do the trick.

I am now calculating frame_rate and samples/frame from the computer clock settings. So now MB utilities in PAL is working -way- better.

I also brought over the sample remain logic from speaker and now while I'm still getting "timestamp in the past", for example:
```
[Current Time:   825.396742] Event timestamp is in the past:   825.396169
[Current Time:   862.511209] Event timestamp is in the past:   862.510639
```
They're not as far off as they used to be.

event loop:
process cpu
  register MB events using 
generate frame
  process MB events
  update "cycle time" which is a double of: seconds of cycles emitted.

Shufflepuck and Glider are both working well in PAL mode (audio, and there is no flicker so we're doing vbl correctly).

But we may be a hair off on cycle count. I mathed it before but maybe that's not what people are expecting. Hmm.

// what is the actual PAL clock?
14250450 - 1015657 . Discrete clock.
14250000 - 1015625 . Hybrid clock.

This is working as well in PAL as it is in NTSC, so let's defer the full fix and refactor of the MB code (needs it) to 0.7.

## Nov 25, 2025

fixed the issue with speed changes. I had the speed-change-detect logic IN audio_generate_frame but of course when we shifted INTO LS that stopped getting called, so it never updated its internal state to LS. Coming out of LS it was still set to the previous speed. (assuming step up to LS and step down. wrap-around speed change would have triggered it..) I'm not happy having to call audio specifically from the gs2 loop so need to rethink this. Maybe speaker itself could return and generate no audio if in LS. It's coupled to CPU speed that way, but, gs2 is decoupled from speaker internals. Probably the better trade-off..

## Nov 27, 2025

likely bug:

[ ] iiememory does not manipulate LC memory mapping in Aux Memory.  

I'm impressed as much stuff works as it does. Things that are broken that are likely due to this:
Airheart on Total Replay

## Nov 29, 2025

Short version on iigsmemory/MMU_IIgs: woefully complex. As in, full of woe. Ultimately, this may suggest a very different approach compared to the MMU_IIe.

GuS and KEGS both rely on the same concept I have in MMU_IIe, namely, a page table where the handler for each group of 256 bytes (or the memory address of the real storage) is stored in a table. For a 16M machine, there are 65536 of these page table entries. That's a lot. It breaks memory locality, but, it also potentially requires setting vast numbers of entries when certain changes are made.

For instance: if we enable shadowing in all RAM banks via the Speed/CYA registers; we potentially have to do language card calculations for all those banks; set up shadowing to E0/E1 for all those banks; just the shr shadowing is 32Kbyte or 128 pages times say you have 8M of RAM, or 128 banks; that means setting 16384 page table entries. ugharoonie! If this register is changed rapidly, this becomes a problem. (Does that happen much in real life? Perhaps not).

But even just having to perform the same LC logic in four banks ($0/$1/$E0/$E1) is awful.

So the possible solution is to perform address munging logic for these. Now the downside of that is we then perform these calculations on every single memory access.

On the other hand, perhaps this can be optimized with lookup tables that are more specialized per type of memory area. E.g., let's say $C000 - $CFFF, there could be a table like this:
16 entries ($10 pages), with some number of input state bits. Let's say there are 20 state bits. That then is 20 * 16 = 320 entries. Manageable. Then the trick is to efficiently determine when we're hitting that $C0 range and to use its lookup table routine. Perhaps that could be done with a 4K or some other resolution map. 16M of 4K is only 4096 entries.

The tradeoff is: does managing the state take more resources than calculating the state. Remember that "managing the state" also includes cache locality.

Bank characters:
1. ROM only
2. RAM only
3. RAM with shadowing
4. SLOW RAM

Then for 3 of course there is the specific shadowing config. Lots of options there. but "shadowing on" generally means: video shadowing; AND Act like Apple IIe Memory.
Normally any shadowing is only banks $0/$1. The "ALL Bank Shadow" enables that for all RAM banks. So an update there is going to modify (up to) 128 bank entries. 


OK so let's toy with LC flags. Our inputs are:
* FF_BANK_1
* FF_READ_ENABLE
* FF_WRITE_ENABLE

BANK_1 is stored in "Real RAM" $C000. Bank 2 is "Real RAM" $D000.
READ_ENABLE=1 means reads come from RAM; =0 from ROM.
WRITE_ENABLE=1 means writes are to RAM; =0 to ROM (i.e., basically ignore)

For READ or WRITE ENABLE:
Input addresses are from $D000 to $FFFF.
If input addr is $D..., and FF_BANK_1, change $D... to $C...

Then offset in the real 64K bank. otherwise, remap to ROM.

Using those three inputs we have a total of 8 possible tables. So then we can do this:
N = ADDR >> 12;
map_addr = lc_map_table[FF_BANK_1][FF_READ_ENABLE][FF_WRITE_ENABLE][N]
7 bits, or 128 entries. Each entry can be a real address, maybe a ram/rom flag so we can calc  the offset.

The flag can then case a switch to add the offset to the correct base (ram or rom) address to obtain the final address.

In a IIe, all pages may vary based on memory mapping.

with 80store
$0 - text pages can be main/aux depending on PAGE2
$2-5: hires, can be in main/aux on PAGE2

with RAMRD/RAMWRT:
$0200 - $BFFF

with ALTZP:
$0000 - $01FF
$D000 - $FFFF - main or aux

Other "all the time" stuff:
$C: I/O
$D-$F: language card

If shadowing enabled, this functionality must be replicated in banks $00 (and potentially all even banks). So set the bank map for those.

Shadowing is a separate stage, where we pass the original address/val down through the "MegaII" (i.e. MMU_IIe).

## Dec 4, 2025

ok the test generator for GS MMU stuff is in place, and a number of tests written so far which all pass on a real GS. Some tests don't pass on KEGS but these are all related to "all banks shadow" which KEGS only handles far enough to make one demo work. (Same with the other currently working GS emulators).

Have done a minor refactor of MMU to allow for page sizes other than 256 bytes; MMU_IIgs of course is using 64KByte "pages" (i.e. banks). Does not seem to be affecting performance using variables instead of constant for page table calculations and memory fetches.

looking at using read and write handlers for certain banks now (e.g., ram banks above 0x80 should return the bank number on a read).

I am not directly using set_
Terror!

I had to convert all the read_h and write_h routines in various places to use 32-bit address inputs, previously they were all 16-bit. Now, I don't -think- there were any places where I was sending in a 32-bit value with stuff in the high half to these 16-bit routines. But, this change touched every device so it is certainly possible something is screwed up here.

And thinking about this, all these things are in the MMU. But could I even have a different interface on the MMU_IIgs for these routines versus the MMU_IIe? Could IIgs override MMU's 16-bit with a 32-bit? 
It seems like it might have sped up the IIe a hair in LS.

Cursory testing shows that I didn't break anything too seriously..

## Dec 5, 2025

Let's give some serious thought to 

The GS emulation of IIe Aux Mem and II+ LC stuff can apply to many banks. At minimum, $00-$01, and $E0-$E1. But also every other bank. So the address transformations need to be generic.

In the II+ and IIe, we use the page table for everything, sometimes requiring updating/changing many PTEs for a single softswitch access. E.g., RAMWRT requires us to change 190 PTEs sometimes. And it requires breaking down the 64K bank into 256 pages.

For GS stuff I would prefer a transform that converts an address based on the softswitch settings. Then the same transform routine can be used across any relevant bank.

The transforms that are required:

IIe Memory:
    even bank w/shadow, or bank $E0: access memory in AUX bank. (i.e., add $1'0000 to effective address)
    access IIe internal vs card ROM is $C100-$CFFF: (lookup effective address from table)
    access IIgs built-in slot ROM vs add-in card slot ROM (lookup effective address from table)

II+ Memory:
    access RAM vs ROM
        access ROM "bank" (i.e., use the ROM base address)
    access bank 1 vs bank 2
        $D000 -> $C000 if bank 1; subtract $0'1000.

* Main/Aux

The bits in the State register are those needed for efficient interrupt handling. I guess they don't figure people will mess with 80STORE, HIRES, etc. in an interrupt. And they're probably right.

| 7 | ALTZP | 1 = ZP, Stack, LC are in Main; 0 = in Aux |
| 6 | PAGE2 | 1 = Text Page 2 Selected |
| 5 | RAMRD | 1 = Aux RAM is read-enabled |
| 4 | RAMWRT | 1 = aux RAM is write-enabled |
|   | 80STORE | 1 = 80-store is active (PAGE2 controls Main vs Aux) |
|   | HIRES | 1 = hires graphics mode active |

These 6 flags plus page number put into a LUT to return a bool 0 = main 1 = aux that adds $1'0000 to effective address.

| 3 | RDROM | 1 = ROM is read enabled in LC area; 0 = RAM read enabled in LC area |
| 2 | LCBNK2 | 1 = LC Bank 1 Selected |

| 0 | INTCXROM | 1 = internal ROM at $Cx00 is selected; 0 = peripheral-card ROM |

| 1 | ROMBANK | Must always be 0 |

So we also have to configure the MegaII when these (and other) softswitches are hit. I think the thing to do, is call the gsmmu->mmu_iie field "megaii"; and then to have replacements for the routines in iiememory in mmu_iigs. The megaII must be configured like iiememory does it, page at a time. Well no, it doesn't, actually. We could set page table handlers for read_h and write_h just like we're doing for banks, and handle it that way, instead of constantly changing the PTEs. This is trading perhaps some runtime performance for taking time every time we change softswitches - it's the whole reason I went with the PTE approach in the first place. At some point I should do a performance test harness for this...

The other key difference here is I am building most of this logic into iigs_mmu instead of a separate iigsmemory device. (remember, iigs_mmu is basically an implementation of the FPI). This is the end of the trail for this class - nothing will be overriding it later. And if I do an "Apple IIx" thing later, then it is gonna get its own mmu class because that will be very different from this. And software compatibility with weird things like Demos won't be an issue.

OK, I have made peace with this interesting approach.

$00-$01: shadow_bank_(read|write)
    if C000 and not inhibit, then call megaii->readwrite
$E0: bank_e0_(read|write)
    call megaii->read|write
$E1: bank_e1_(read|write)
    do what now? need some special handling here

megaii_c0xx_(read|write): C0XX handlers
triggered by calling megaii->write(Cxxx)

What should the interface to the Mega II be. There are two:
normal Main bank interface. When writing into Even Shadowed, or, $E0. uses pagetable entries.
Direct-to-Aux bank interface (only from IIgs). this is if shadow_bank bit 17 is high. When writing into Odd Shadowed or $E1.
let's call these MainWrite and AuxWrite.
MainWrite just sends byte to the MegaII. AuxWrite must index into the aux memory directly.

00 ->  MainWrite -> MegaII(16-bit)
E0 ->

01 -> AuxWrite -> MegaII(index ram directly)

## Dec 6, 2025

Making some progress in the tests! The layers of behavior are pretty hard to keep straight. Ugh.

But, here is an example call stack for a write to a softswitch:

MMU::write() - MMU_iigs write
bank_e0_write() - write_h routine for that bank
MMU_II::write() - megaII write, dispatches to C0XX handler
megaii_c0xx_write() - wrapper for C0XX handler routine
MMU_IIgs::write_c0xx() - back inside mmu_iigs to actually process the s/s write

Some of this would get optimized out, (e.g. the last two steps I think). But there are several routines here! Now that's bank E0 write.
A Shadowed bank would be similar.
A full-RAM bank would just be:
MMU::write() - MMU_iigs write
So native IIgs software is going to be pretty fast.
The above should be optimized quite a bit I think, using inline.

I'm on test 13 - this is where I need to be configuring the memory mapping inside megaii based on the softswitches.

## Dec 7, 2025

around test 17 now - working on LC mapping including ROM mapping. So we need a ROM passed into MMU_iigs and into MMU_iie.

## Dec 8, 2025

we're passing a bunch of tests, but there is a bunch of stuff we're not testing. Here are some action items, and some things to add tests for:

[x] Test 2 ROM bytes mapped into LC area;
[x] test same bytes accessed directly via FF/xxxx  
[x] need to bring in C006/7 and C00A/B switches
[x] Implement Slot Register ($C02D)
[x] save handlers set by display for page2, hires, whatever else, and call them.  (maybe just copy the whole thing?)
[x] bring in C01X status read switches (part done, did C013 - C018) 
[x] create a debug display handler  

the first ROM I got had the FE/FF banks reversed. Weird. Using ROM01 for now.

C006/7 and C02D all need to work in tandem. The "slot" rom exposed works just like it does in IIe, except, C02D provides ROM (and I/O?) from the IIgs ROM, instead of exposing the slot ROM.

## Dec 10, 2025

The IIgs supports the C1/CF mapping like the IIe but has another layer, the "built-in" versions of those devices. So right now, I have it just calling the megaii mmu for C006/etc but something needs to take into account the slots register that can ALSO substitute in the GS built-in firmware. AND choose between two sets of softswitches.

For initial testing, I should be able to just act as if I have all slots set to "Your card".

But, for reality: we're going to have "GS onboard" devices. These devices will be GS-only. And they will need to register their softswitches 
normal slot:
set_C0XX_write_handler
built-in device:
set_gs_C0XX_write_handler

now: do these rely on C800 mapping for anything? Or do they go into native mode and jump into big rom area?
C600 does not go into emulation mode. it does however, force 8-bit registers. does it assume nobody calls C600 except in emulation mode?

c600 does not trigger c500. no built-in slots do EXCEPT C300. That is 80 column firmware. 

## Dec 13, 2025

So overnight, the speaker code got out of sync. I think I see the issue: I now take a running 60-frame average of number of samples queued up. At first boot, this started at about 5,000. Now, after being up for about 10 minutes, the count is up to 5800. So we are slowly feeding too many samples into the buffer.

Frame rate is 59.9227; samples/fr = 735.948. That is basically exactly 44,100 samples per second.

Double clicking the window to expand it, caused about 1,000 samples to be consumed (dropped to 4900). But then it's slowly ticking up again.
I am currently using a float to track these variables. A float "guarantees between 6 and 9 decimal places of precision". But 1/44000 is 0.000022. So perhaps I should try .
I could also force a reset to 0 of the counter every so often; on frame counts where 59.9227 and 60 are coincident.

let's also count how many samples we add over time average.

The figures seem spot on. That tell us that perhaps something in the timing is causing us to add more samples per second than we think.. which could mean the entire emulation is running slightly fast.
Or, that SDL3 is consuming fewer cycles than expected.

A suggestion was made to slightly slow down the emulation if we exceed a watermark of samples in the queue. I am doing 5uS right now, and that is definitely correcting, maybe a bit too hard. It's possible I am running the emulation just a bit too fast? I am not measuring at the frame start when calculating the frame rate etc. 
what's really interesting, is if I open speaker debug, and run applevision with this hack in place, while the video is playing, the avg samples crawls downward. Then when it stops, it starts crawling upwards.

The code in there now to delay 2.5 microseconds does bring the queue length down and hold it around 5000. Maybe the issue is around timing frame lengths, and should be reviewed. There are noticeable audio artifacts though. Why?

Also interesting: the samples in the buffer should start at 0, right? But by the time I get the debug window open, it's already shot up to 7000 or more samples in queue. which means it's even higher but I just don't see it cuz it takes a minute to get the debug open.

So we are pumping data into the buffer before SDL starts playing it? That must be the case. I am currently using a new approach, tweak by a whole frame. I still use the frame remainder calculation when I decide to emit a frame. Seems to work okay, though in the first few seconds there is a bit of warble and the boot beep is not great. maybe I can buffer that a bit more.

Makes no difference. And shouldn't.
However, we have the frame_buffer_start thing. if we skip generating a frame, perhaps we shouldn't change that.

Sometimes (not every time) there is a glitch in the boot beep. Like, one out of 5. Try starting the stream sooner, instead of when we're putting data in the first time.
I did at the end of init_speaker, no difference really. Hmm. It does feel maybe like a threading issue. Is there a call to ask "is audio actually fully started"?
The default device on my Mac is built-in speaker 44100. The hdmi audio is 48000 but that's not being used, these monitors don't have speakers, they do have an external speaker jack. which is weird. So the rate isn't being changed, requiring maybe extra buffering.
Warbles do seem to settle down after boot time, it's just annoying. That first BEEP is such a key part of the Apple II experience, it's got to be right!
I will put this aside for the time being.

## Dec 16, 2025

ok then, let's try composing a GS system!

I have some pieces together, but I am getting an infinite loop of this:

Write: Effective address: 00019A
Write: Effective address: 000199
Write: Effective address: 000198
Read: ROM Effective address: 00FFFE
Read: ROM Effective address: 00FFFF
Read: ROM Effective address: 00F37C
Read: ROM Effective address: 00F37C

FFFE/F is the BRK vector? yes, emulation mode vector.
Then we're doing what, running instruction at F37C?
then writing some stuff on the stack
then brk again.

ok I am now starting the emu (for GS mode) with debug window. I also disabled the prospective disassembly because read_raw is crashing - so is monitor read, which uses read(), so, something not getting set up correctly there. what mmu is being passed to debugger?
but, importantly, the **CPU** is correctly reading memory.

ok, got TSB working. I have to (re-) set the processor type in the trace_buffer after startup, for dumb reasons. And I needed to recalculate the cpu_mask in there, which I wasn't doing. So that's fixed.. So here are our first two GS ROM instructions:
```
LDA #01
TSB $C029
```
I believe this is setting the bank latch. Then a bit later is reads C036, but we're getting 0xEE (garbagio). Why isn't that working.. 
oops, need a C036 read routine.

[x] after a CLD the debugwindow doesn't show D as 0, it still shows 1.  was displaying B in wrong place.  
[x] Show the P bits in order from MSB to LSB, and, change the legend in 65816 mode since we have additional bits.  

Handling the display softswitches - unlike iiememory, gsmmu is setting handlers *before* device init is executed. So need a better scheme for this. Maybe "set_mmu_softswitch_handler" as well as the current one? Have to think about this..
[x] Monitor needs to do 32-bit addresses.  
[ ] Monitor should be able to "remember" banks like the IIgs monitor does so you don't have to type full address every time.  

The mmu the debugger is getting, is megaii. That's not right. however then it's maybe especially confusing why read c035/6 aren't working.
[x] GS should change the mmu in the debugger.  
done. And the others set the debugger mmu also. Wonder about splitting this up some..

OK, the disassembler was doing read_raw, which really isn't what we wanted. It probably somewhat worked by accident in the IIe mode. definitely not in GS, we have to have megaii memory mapping.
[x] four byte instructions (e.g. lda $xx/yyyy) don't disasm correctly in the monitor disassembler.  

STILL $C036 isn't reading, lol. (ah, it was being overwritten by speaker. fixed.)

Hanging on $C034:7 to go to 0. But we're reading EE, so that never works. Ah, the data bank is $E1 and -that- is not working. Eenteresting.

[x] Debugger trace- correctly display DB, K, etc in header and disassemblies.  

C035 and C036 reads aren't working because they're connected to the Speaker and I get inappropriate clicks. So this is coming back to my notes above, that other motherboard devices may be overwriting the MMU stuff.
But why is is 0xEE? the scanner value isn't getting set, or is getting set in the wrong mmu.

[x] if the screen isn't just the right size, you can see scaling artifacts between the border and main display area. tweaked/hacked around. 

## Dec 17, 2025

So at minimum these registers are not working right:
$C034 (not implemented)
$C036 - returning 0xEE

## Dec 20, 2025

one of these has been fixed. 

battery ram/RTC registers have been implemented at C033 C034.

C034 also saves the display handler for C034 (Border) and calls down to that display routine.

hitting ctrl-oa-reset actually triggered the self test / ram test somehow, and it ran pretty well for a while, tho eventually crashed in generate_frame.
There are other times where I've hit ctrl-reset where it did the same, as well as other failures (in MMU). So there's some weirdness still in memory handling.

## Dec 21, 2025

I've been wondering why the system does not come up in white-on-blue text. The 0911 ADB error seems to throw before we set display colors from battery ram.
So, is C022 FC by default on power-on in the VGC? Clemens defaults them as such (but border to 0). GuS sets to 0xF0. 
I'm going to default all these to white-on-blue.

## Jan 2, 2026

This little project:

https://github.com/cheyao/sdl-menu

shows how to override default MacOS menu in SDL with something more like what you want.

That is working pretty well in the adbtest thing. I can change and/or just remove keybindings for menu items; have whatever menu items I want; etc. I made Close Window cmd-F12. I can program the command strip, which would be fun, even if they don't make them any more.

but let's say I went this route - I'd have 3 separate versions of menu code, for Windows, Mac, and Linux, ultimately. 

## Jan 8, 2026

Ah, so the issue with the debugger breakpoints early in boot:
1. if we BP we exit the main frame CPU loop early.
1. then this line doesn't have any / enough data to operate on.
            MEASURE(computer->display_times, frame_video_update(computer, cpu));

In fact this would be any breakpoint hitting at a point in a frame before enough data has been emitted.
We need to skip the other end-of-frame stuff too. Or rethink how we handle stepwise.

[ ] with 8 bit A, we're printing too big a value in the register status line.  
[ ] register status line, registers are wrong width, and not enough space given for them.

## Jan 10, 2026

so we're booting past the ADB setup code, but crashing when the rom does
JMP ($0036)

00/0036 fdf0.   e1/0036: ffb6.   e1/0036: ffb5; 

36 is being overwritten with 0's, by 00/F8C9:MVN $00,00

however the setup here is supposed to only clear 800 - BFFF, so why is 36 being written over? oh, it's somewhere before the mvn.
let's do a watch on 0.ff.
page 0 starts out as all BEs. Something clears it to 0's.

is the memory map starting off wrong and then we're switching ? Doesn't seem right, wouldn't it all be BE to start?
Actually, why is page 0 BE to start. The first 4K (0-FFF) is BE. red herring.
FCDE is part of some delay routine before the beep starts. 
it's FCA8. nope, issue is before that.

ok boom - it's a #$0C -> C068. This is turning on LC Bank 2, and that is (erroneously) causing my RAM to disappear.
OH. that's why that region is BE, and why it's 4K. We're subtracting 4K from the effective address inappropriately.
the culprit seems to be:
calc_aux_read:
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xD0 && page <= 0xFF)) && (g_altzp || g_lcbnk2)) return 0x1'0000;
when altzp is on, it has nothing to do with lcbnk. 
ok, I fixed that. (I think it was AI nonsense?)

Now getting past that thing in the boot, and am in an infinite loop around FF/CA64. Note: text page hasn't been cleared yet.
it's hitting 057B with F0.
ah I seem to be infinitely looping on a BRK now:
```
JML E1/0010
"CC CC 00" - CPY $00CC
$00 - BRK
```
So this is uninitialized memory. Something needs to set up this vector and it's not doing it correctly.
bp on 0010 isn't working right.. 
oops, supposed to do the same fix in calc_aux_write.

WAHOO!!!! I am now at the "Apple IIgs" boot screen.


the watch command and display only supports 16-bit addresses. Fix this..
[x] MVN isn't setting the effective memory address. 
[ ] enable click-to-move-scrollbar-point  
[ ] add monitor command to display stack (and/or have a debug or special watch)  
[x] page up/down in trace listing isn't displaying the right chunk - it's not getting rid of the proforma trace when you page up. And it's miscalculating the display range.
[x] "l" disasm with a 24-bit address crashes.  
[ ] debugger disassembler needs to let you set flags for A and X width to disassemble correctly. And it might have to trace direct rep / sep. no way to track other modifications to P.  
[ ] address parser should support /  
[ ] bp on 0 inappropriately triggers on things like LDA #1
[ ] create a BP on "memory location equals value"
[x] trace header in 816 mode is not aligned correctly with trace

for mvn / mvp, we have choice of TWO addresses. in each instruction loop iteration, there's a read and a write. These are the only instructions where these could be different. I guess for now, let's make it the write, and, maybe I need to add another field in the trace log for this (ugh!) maybe I can overload a field..

ok, the text color is screwed up: $E1/02DB is 06. BRAM is copied to $E1/02C0. 1B is background color. Medium blue is $6, not $C. Well how did I get that screwed up?
The table I am using, has the colors ordered for use by hires, which is of course different from these text color assignments. Maybe it's a phase thing? unsure. Anyway, I have fixed the colors. $F6 is correct.

On a boot, I get the GS screen. 
What I'd LIKE to do is hit ctrl-reset to get into basic. But sometimes that is triggering selftest; and sometimes crashing. oh I am probably not resetting some memory map flags correctly on a reset.

ok, self test is working up until test 09 (or 08?) where the firmware is reading a bunch of values out of the ADB ROM:

```
ADB_Micro> Executing command: 09 00 14 
ADB_Micro> Response (2 bytes): 81 BE 
ADB_Micro> Executing command: 09 01 14 
ADB_Micro> Response (2 bytes): 81 BE 
ADB_Micro> Executing command: 09 02 14 
ADB_Micro> Response (2 bytes): 81 BE 
```

Fixing this a couple ways I think it's supposed to work, causes the ADB 0911 error. Well, gotta do some other stuff for now, come back to this later. Good progress!!

[ ] the debug frame step stuff - when enter/exit step, don't try to consume frame; also don't modify the regular frame end stuff.
[x] if we exit when in step mode, the "emit one more frame" call is causing a crash  


ok, we have a classy problem.

MMU_IIgs derives from MMU.
Computer stores an MMU_II (indeed, is configured with a pointer to the mega_ii), which is right.
so we need to set a reset handler in gs2 when we set this stuff up.

## Jan 11, 2026

ok, I removed the slot5/6 devices. -- there is no apple iigs ROM in CXXX right now --. I crash in some routines intended to set up the RAMDisk. The RAMDisk shows up in slot 5. (if you have 3.5 drives it will get moved to S5D2 or S2D1? that seems right). I bet the crash is because I got no GS ROM showing up there. But I'd like to know -why-.

The BRK it hits is $F8B0.

but I now handle the vector pull read. and we are now pulling the correct vector!
the prospective disasm returns EE for all instructions here, because it won't read out of IO space (needed, but a little awkward).

It's going through some monitor stuff, FF/BB0D: jml $00FA56
Well this is all 0's, because the LC is enabled for read and write.

```
This bit in State is set:
| 3 | RDROM | 1 = ROM is read enabled in LC area; 0 = RAM read enabled in LC area |
but READ_ENABLE:1 in LC also.
```

So they are inconsistent.
Is this the same bit as READ_ENABLE ?
Does it have any effect on /WRITE_ENABLE?

ok the funky thing here is that FF_READ_ENABLE has the same function, but opposite sense, of State:g_rdrom. So I am faced with having to, when one is set, setting the other to the inverse at the same time.

ok that is fixed - and VOILA! on a power-up I now hit the "Check startup device" screen!! M0R PR0GRESS!

however, when I hit control-reset to try to get to BASIC, the ROM does JML $00/2007 and there I crash into a BRK.

ok, well, that is something to do with the self test. And there is supposed to be code there that isn't there, it's not being copied or not being copied correctly.
Whenever I hit ctrl-reset, we are jumping into the self test. I think this is because there's an LDA $C046 that is coming back with value $FF.
It was reading C046 and sometimes going into selftest on a reset. C046 bit 5 is the AN3 flag; bit 7 is.. ? it's checking bit 7 = 1 to force self-test.

Now when I hit ctrl-reset it keeps looping to Check Startup Device and doesn't let me RESET into BASIC.

Self-test is ctrl-leftwin-rightwin-reset. System self test fails with System Bad: 03011500
It dies on BRK trying to jump into the Cxxx space.

But, Technote 95 says that is 03 (softswitch test failed), 01 (state register bit), CC (15) low byte of switch address. Handy.
OK that is SLOTCXROM not working, or at least, not reflecting in C015's output. and indeed it seems to be not!
C015 returns bit from state; but c006/c007 tweak the megaii. So they are not in sync with state.
bp on 77a8
ok I stepped through the routing SOFT_SW, and it's failing when it tests setting RAMWRT. It does:
sta c005 (set ramwrt)
JSR
return address is being stored in bank 1 (aux)
RTS
return address is being read from bank 0 (main)
ALTZP is 0 across this, so, my code is wrong and not handling ALTZP / RAMRD-WRT.
FIXED! I needed to add an extra IF stanza checking if PAGE between 02 and BF.
Now we're failing on 03, 40, 1C, which is the Page2 Selected switch. Now that's odd, should be in Display?
ah, C01A-C01D are not handled properly in mmu_iigs. They need 
iiememory has a routine to fetch the values from display whenever we update the memory map. Doesn't quite work like that here.. maybe just call the updater when we need to read these values.
page2 is not updated when we tag c054/c055. I had the logic wrong in set_state; setting page2 there works now. But setting c054/c055 doesn't. 

## Jan 12, 2026

Maybe: A cleaner way around some of these issues, is to more fully Classy up some of these modules, and, then have a device for the platform that manages shared registers. E.g.:

iigsmemory is a simple device that:
1. reads current c054/c055 handers
1. stores them
1. registers new ones
1. When called, can call each module accordingly.

In this case, we need to call mmu_iie (megaii), and, display.

iiememory tracks text and hires mode flags, but, doesn't appear to use it for anything except reporting the softswitch states. It does not use it to calculate the main/aux memory map.
Should this be in display?

Got into the monitor, when self test 4 crashes to it! ctrl-B cleans BASIC and can write programs!

04000000 - is "RAM ADDRESS TEST", BB is failed bank number which is 00. Not very helpful.
It is actually crashing into monitor on a COP instruction. which isn't super-helpful because the address is 0. That doesn't seem right.
But, likely is a memory management failure. Have to read the source again.

my handling of caps lock works! however, it can, of course, get out of sync with other applications. I've seen that before. Will have to give that some consideration.

ctrl-reset is still being treated like a 3-finger. will have to see how that's handled in the ROM.

ah ha! It was the ADB ram location 0x51. The ADB uP commands that returned data were not returning the correct data. I can now boot, get Check Startup Device, and hit ctrl-reset to get a applesoft prompt!!! WHOOHOO!!

Trying to boot now. 3.5 Prodos stuff is crashing after the ProDOS splash screen. Some horsepucky. Let's try 5.25 dos..
ah, no, infinite hang in the ROM because it thought it was the IWM drive. I can try slot 2..
Nope, we crash mounting media in slot 2. There's a bunch of hardcoded stuff that requires these stupid drives to be in slots 5 and 6, isn't there.. ok neither of those are working.

I suppose the next major device step is the IWM. But I should boot trace a 3.5 with ProDOS8 and see if I can figure out what memory map thing is wrong. Something ain't right.

It could be missing firmware, particularly, missing 80-col firmware. I suppose I should try mapping that in. See if I can get 80-col mode, etc. Oh yeah, it's doing a JSR $C311. So definitely need some peripheral memory magic.

## Jan 13, 2026

[ ] have bp for: PC only, MEM only, BOTH

[ ] Debug window open. When edge of debug window gets near screen edge, and there is flash on the main window, the flash starts going really fast. and debug window updates go slow..

ok test 4 that's failing is the "RAM_ADDR" or "RAM Address Test". It starts at ff/7C86. So let's BP there. In the ROM03 source, It's in file diag.tests.asm.

Failures can come from:
subroutine cmpit returning with carry set;
Failure reading KEY_VERSION
KEY_VERSION is < 6 (032B) (check value of this) (E0=00, E1=FF, unsure if it's even been set here yet)

ROM01 doesn't seem to be checking that.ok, in a ROM03 it checks that to see if it should test up to RAM Bank $10.
ok, so the sub WriteIt does this:
X = start addr; Y = end addr;
```
loc= bank << 16
while (y < RamEndAddr)
  add current address to ram bank.
  store in loc,y
  iny, iny (incr by 2)
```
So in bank 0 you'll get a pattern like:
1000:00 10
1002:02 10
in bank 1 you'll get:
1000:01 10
1002:03 10
bank E1:
E1/400:05E1
Hm, F0 is E1 01 so it's the bank plus another 100. Hmm. 

shadow was =0, we set to 5f, which should disable all shadowing and inhibit C000 space. It does seem to. So wait, what does this imply then about operation of the lc bank switching when inhibit is set. (Well, it should still work in E0/E1? )

ok the error is checking E1/0800. The value there is 0901, but the value should be 09E1. 
This value is identical to bank 1's value. So bank 1 page 8 is being inappropriately shadowed to E1.
Whereas the data on Page $0C is correct. And at $0400 is correct because it tested those values okay.

Interesting. The shadow value they set is $5F. Which means "text page 2" shadowing was still enabled. Did I implement that??
| 5 | Inhibit Shadowing text page 2 | ROM03 only | value = $20
```
if ((address_16 >= 0x0800 && address_16 <= 0x0BFF) && !(reg_shadow & SHADOW_INH_TEXT2)) {
    return true;
}
```
I sure did!
ok, man, that was painful!
So, somehow, I need to have a flag for ROM01 vs ROM03 hardware, and, not shadow Text 2 on a ROM01. For now, just create a flag and have it check the flag.

Awesome!! Passed Test 4, moving on to Test 5: 05010200
That is "Speed test: Speed stuck fast". Well, it sure is cowboy.

This test relies on two things.
1. the video counters C02E/C02F
1. speed control register.

After implementing 1 above (see VideoScanner.md), we now crash with 05012400.

I need to figure out how to implement speed control. We can't use the generic speed control because that is only made to kick in at end of frames, and that's not good enough here.

This one is challenging. I think it's basically:

Emulator speed levels of 1.0, 2.8, 7.1, etc. need to be constrained. Don't have a 1.0.
And then, the VM speed control - which is a combination of the speed control reg and -syncing to the mega ii-, has to be done in the clock routine. Let's examine that:

That main execute loop is now only checking 14M's. So we can actually speed shift, I think, in the middle of a frame. The video is also timed on 14Ms. Yep, just call set_clock_mode.

See, correct architecture gives you stuff for free!

(Well, hold up till you see if it actually works cowboy, ha ha guffaw).

So the speed register $C036 is all speed control except for bit 5 which is "shadow all banks". *rolls eyes*. So where should $C036 live practically speaking? Speed stuff requires CPU, and, mmu_gs does not have a pointer to the cpu. Well. I could always just inject the cpu into mmugs. 

Eventually, have a clock construct independent of cpu. then that would be responsible for setting speed, speed changes, etc.

It works!! Sorta. The audio is fonky and is out of sync. well, we are in debug. let's try recompiling. Oh, yeah, that's much better. Well it's possible I will pass self-test 5 now.. nope, still croaks at 05012400. Well, need to do some "real" work for now, will come back to this later.. Arekkusu says this routine is very timing sensitive and depends on the RAM refresh stuff as well.

Currently megaii->compose_c1_cf is the iie version. Need to make a GS-specific version of this, and use it, instead of calling megaii's version. This will handle the additional complexity the GS has here.
Right now I tried setting c3 rom with megaii and compose_c1_cf overwrites the entry with rom_d0 + 0x300, which is wrong.
In iie that rom starts at C000, so that + 300 is correct. But in mmu_iigs I set the rom offset to D000? 
Ah, I set to C000, and there is some improvement! The c300 rom is in the right place.
in 80 column mode, we are often flipping between page1 and page2.

[x] 80store/page1 stuff isn't working right. i.e. with 80store on, page2 is changing to page2 instead of forcing page1.
[x] ^G in 80 column mode causes a page out of range assertion and hard crash.  

Aside from the above, there must be something wrong with language card mapping, because ProDOS ain't workin right.

This is a great writeup on Apple IIgs timing complexity:
2011-krue-fpi.pdf
(in Apple II Documentation folder)

there are 

Great googley moogely. There is a "volume control register". $C03C. Shared with some DOC flags. We'll have to have an event queue and record volume changes, and make available to both speaker and ensoniq code. It's not good enough to record volume at time of speaker event, it needs to be a separate thing. Should be pretty straightforward.

In a NORMAL FAST cycle, PH2 is running at its fastest rate. This cycle is used for accesses to fast RAM and ROM, when the system speed is set to FAST. When accessing ROM, the fast RAM can also be refreshed during this cycle, incurring no speed loss.

In a FAST REFRESH cycle, the high phase of PH2 is extended by 5 ticks. A RAM refresh is performed in the first 5 ticks, followed by the normal access in the second 5 ticks. This cycle is used for accesses to fast RAM, when the system speed is set to FAST. When running completely from fast RAM, every 9th PH2 cycle is a FAST REFRESH cycle.

In a SYNC cycle, the high phase of PH2 is extended by as many ticks as are necessary to align with a full cycle of PH0. If the falling edge of PH2 coincides with the falling edge of PH0, then "only" 9 ticks must be added (11 ticks for a STRETCH cycle). If the falling edges do not align, then an additional 1 to 13 ticks are added to wait for PH0. Fast RAM can also be refreshed during this cycle, incurring no

So we're doing "normal fast" cycles right now, basically.
The FAST REFRESH, says "every 9th PH2 is a FAST REFRESH". So in the incr_cycle, we'd add 5 more 14Ms. This would be before the video_cycle check.

```
uint64_t fast_refresh = 9;

    inline void slow_incr_cycles() {
        uint64_t this_14m_per_cpu_cycle = c_14M_per_cpu_cycle;
        cycles++; 
        if (--fast_refresh == 0) { this_14m_per_cpu_cycle += 5; fast_refresh = 9; }
        c_14M += this_14m_per_cpu_cycle;
        
        if (video_scanner) {
            video_cycle_14M_count += this_14m_per_cpu_cycle;
            scanline_14M_count += this_14m_per_cpu_cycle;

            if (video_cycle_14M_count >= 14) {
                video_cycle_14M_count -= 14;
                video_scanner->video_cycle();
            }
            if (scanline_14M_count >= 910) {  // end of scanline
                c_14M += extra_per_scanline;
                scanline_14M_count = 0;
            }
        }
    }
```

this code is causing a video crash, so we're doing something very wrong..

[ ] put in some assert checks on the VideoScanGenerator to ensure we won't crash.  

not sure. Have to do some real work now. This area of code is so sensitive to bugs. I should put in some checks so if it's wrong we won't crash. I'm unclear what's happening here, is it overrunning or underrunning the buffer? I inserted 14Ms.. 
Is the main loop timing on CPU 14Ms, or video system 14Ms? (CPU) Because it should time on video system 14Ms, if there is any difference between these two. I'm also unclear on the effect of the video_14m = 0 instead of video_14m -= 912. What if it's 915 instead of 912? Then we're going to be out of sync.

[x] I think I'm going to implement a C0XX bus concept where multiple routines can register handler.

This ought to greatly simplify a lot of wacky code - instead of daisy chaining things like PAGE2, there are just two routines, one from iiememory or gsmmu, and one from display.

How do we resolve who returns a value .. hmm.. have to look at some sample situations.
Could OR all the results, and if one handler doesn't have any input, just return 0x00? So it won't affect result?

iigsmemory: C050-C057 would work fine this way.
display/rtc: C034 sharing would be fine this way.
iiememory: writes to C000-C00F for various switches then call things in display. This could be refactored to reduce cross-dependency and improve code readability.

only question is do we need more than 2. In theory you could have any number of things.. well it's easily extensible.
what if these routines returned their data -and- a mask. then the algo could be:
start with cur = floating bus value
apply mask (cur &= ~mask), and new data (cur |= newdata)
apply (cur &= mask) mask and new data (cur |= newdata)
This would entail changing all those functions to return 16-bit values (mask and data).
or on read we call them with an "input value" and they modify it and return an output value.. or can pass a reference to the output and similarly modify it.

OK, I modified the C0XX_handlers stuff. and it built. I am expecting some things to fail because .
I should have some asserts in here..

iiememory has been doing two different ways of sneaking a peek at state variables.
1. copying them periodically from display.
2. replacing display's c0xx handler with its own, then calling back into display.

these both need to be replaced with the new scheme.

other things that have weird couplings that this solves:
* display + anc3 <- double hires mode.
* pull memory management behavior from iiememory into mmu_iie. since it doesn't matter now what order c0xx handlers are registered.
* Videx monitor ancX

if we do the thing where we pass reference to read / retval to the read handlers, we can also solve:
iiekeyboard + display/mmu status switches which need to return the low 7 bits of C000 keyboard register.
A thought:
the read and write handlers could be exactly the same signature, if we set up both to pass bus val by reference, and a read or write flag per call also. or I could register the r/w flag and skip the wrong type on any particular thing.
Then we could just have up to 4 read or write handlers per address, in any combination.
this is a case where more abstract == more work and lower performance maybe?

ok, so now back to IIGS!

with 80store on, page2/page1 works the way I expect for display. however, 80 col isn't working. Also, mmugs page2 is '1' all the time. Let's see.. ah, now that it is super-extra-clear what is going on, I found that read_c0xx switch was missing break after each case. Duh. and now pr#3 80 columns works!!!

Rerunning the diags for fun. nah, still crashing with roughly this:
```
3     0
        0
          0
```

in C1xx memory. So, missing more ROM. This is where I need to properly handle c1_cf_compose, because I'm betting since the switches are still there, this wacky 80-col ROM is still all over that space too.

I enabled the slot ROM for C1-C6. (the default GS slot mapping for internal / your card).

There was a long standing bug in pdblock2 where it only allocates structs for 7 slots (0-6) so trying to put it in slot 7 was triggering all sorts of weirdness. That's fixed now.

[ ] Should change pdblock2, so it's like diskii where I don't have a single static array.

Also, pdblock is more realistically a "generic hard drive" device, not a 3.5. for GS stuff I'm going to need a hard drive device anyway, of potentially very large size, and support having a partition table in it. 
So maybe make a version of it that is in slot 7 and has different status/mount icon etc.
More urgently, the OSD needs to be able to figure out what slots the drives are in and do the appropriate display, use the right keys, etc.

Now that I've cleaned that up, and can have DiskII/PDBlock in slots other than 5/6, I can boot DOS3.3 on a floppy on the GS!!!!! (slot 7) YEEEHAW!!!
ok fine let's try choplifter. YES!!!! YES!!

[x] implement handling ctrl-oa-del to "flush keyboard buffer"  

ok, prodos 1.1.1 booting crashes to BRK at 2141. The sequence right before is:
```
sta c00a - c3romoff
sta c001 - 80store on
sta c055 - page 2
```
hires:0 but hires is being switched out.
So, hires should switch to aux ONLY on 80store and page2 and hires on.

[x] if we're in STEP, stop sound effects from playing.  (handled in diskii device frame update, checks cpu->execution_mode) 

doing BP on c080.c08f, on iie (working) ProDOS does this:
```
LDA C082

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C082
D0,E0: ROM r/o

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C083
LDA C083

D0: lc_bank2 r/w
E0: LC_RAM r/w

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C081
D0: rom rd, write lc_bank2
E0: rom rd, write LC

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

LDA C08B
LDA C08B

D0: lc_bank1 r/w
E0: LC_RAM r/w

.. bunch of the same ..

then 

LDA C082
D0, E0: RD ROM, no write
```

well it's crashed before here, so let's see what's wrong here in gsmmu..

the LDA C083 pair gets us mapped as:

D0 LC_BANK2 LC BANK2
E0 LC_RAM LC_RAM

that's correct. Everything up to the crashy point is right, but the debug output here is from megaii. 
So where in mmugs does it map the LC memory..
hmmm, actually, I think I'm overwriting my ROM image, hurr durr. no.. close. 
Even when _WRITE_ENABLE=1 (i.e., rom, no writes) we are writing into the LC RAM. These should be ignored.

ah, ok here is a broken test:

c083
c083
fded:dd
c08b
fded => dd

that's broken. the write is correct because the value is read back when I'm in c083. so let's look at read..
ah, this needs to be qualified like so:
                if (page >= 0xD0 && page <= 0xDF) address -= 0x1000; // only this area.
YES. ProDOS boots now!! Take that, MF!


[x] in step mode, the shr screen goes away. (never wired it in in display)  
[x] shr should clear on a reset? (clear bits 1-7 in newvideo)

ok the ^G in 80-col mode crashing the emulator is twofold:

1. we were generating an address > 0xFF_FF_FF - causing buffer overrun in address_long_x. I patched the CPU there, but, feel like maybe it ought to be handled somewhere else more general.
2. the thing is executing code in CAAx and then does an 0C => C068, and then the code disappears out from under it. So we were in a C800 area, and write to C068 caused C800 to get unmapped.

[ ] first pixel of hgr row disappears in Ludicrous Speed  

[x] On a speed change, we're calling speaker config reset. We are NOT. But we are printing a bunch of debug info. Emit less cruft.   

[ ] investigate whether there is a key map conflict between adb and gamecontroller. (i.e., how am I treating alt + win in both places) (YES, there is)

Failed on this self-test:
03 00 1A
C01A-D were doubled up in video + gsmmu. Remove from gsmmu.
cool, we're back to 05012400

## Jan 18, 2026

ok so I have a mapping inconsistency for the OA- and CA- keys. 

Found "Apple IIgs Diagnostic v2.2". 800k disk, ProDOS 8. Sweet!

[ ] need to apply "joyport workaround" to IIgs mode.  It does. could be the mismatch between the modifier keys above.  

ProDOS 8 & 8-bit software is working pretty well now!! Total Replay is working very well except Airheart which TROUBLES ME.

I can't run Audit, it won't run on IIgs. Why? 

Arkanoid Crashes during boot.
It's executing from E0/D100 page, does 00 => C068, and then the code disappears out from under it and hits a BRK.
Indeed, the code they're expecting is switched in if I hit c08b c08b. and 0 => C068 definitely turns OFF LC Bank 1. 
Is there even supposed to be a LC in banks E0/E1 ? The IIgs HW Ref strongly suggests there is. Says: "I/O and LC in banks e0/e1 are not affectd by this bit (shadow register iolc inhibit), this space is always enabled"
Yeah, and my testing indicates there is. For sure.
What if I have bank 1 / bank 2 reversed? 

Yes, that's exactly what happened. GS Hw Ref described State C068 bit 2 (LCBNK2) backwards. Add a test to the test suite for this. (Tech Note 30 corrects the manual).

HOLY CRAP! ARKANOID BOOTS AND PLAYS!!!!!

Let's give some thought as to next steps. Missing hardware is responsible for several of these issues - for instance, a bunch of s/w is trying to access C0E0/IWM all the time and failing.
Options are IWM and Ensoniq. The IWM would open up more titles to test; the Ensoniq would be more immediately fun, e.g. sound in 

MAME's source for the Ensoniq is quite compact, under 500 lines. 

[x] Test behavior of INTCXROM and SLOTC3ROM in GS  

the HW Ref shows 3 variations on I/O memory map. Peripheral expansion ROM (c800), internal rom and peripheral rom, and "internal rom". 
Seems okay.. 

VCG interrupt. Decide where these registers should live, and insert them as dummies.
As weird as it sounds, I think it belongs in the video. 
so:
when we instantiate VideoScannerIIgs, pass it an interrupt_handler_t and a context. The context will be the cpu. Alternatively could we pass it a closure? By default VSGS will store a null handler which will do nothing on a vbl.
We can just add a 
```
if (scan_index == cycle_to_trigger_vbl) {
   *handler(context);
}

enum device_irq_id {
  IRQ_ID_SOUNDGLU = 8,
  IRQ_ID_KEYGLU = 9,
  IRQ_ID_VGC = 10,
};

void handler(void *context) {
  set_device_irq((cpu_struct *) context, device_id, true);
}
```
at the bottom before the last bit. Thi
assign device IDs to the onboard devices: SOUNDGLU, KEYGLU, VGC.

ok, various things C047 read and write both clear the interrupt; fixed that (cribbed from KEGS). when booting GS/OS, I am getting Unclaimed Sound Interrupt 08FF.
I'm gonna guess that the interrupt handler somewhere is reading SOUNDGLU regs; sees interrupt flags; but there isn't a handler for them; it ticks a counter and after so many of them it throws this error.

FF/B7CC - main interrupt handler

ok, this gets around to checking C03C, D, and E, and calls JSL irq_sound (a vector). By default it's set to do unclaimed sound interrupt. So this is all just because the bits are floating. GR!

ok I have now thrown in dummy ensoniq and scc devices, it's still blowing up. 
Need to add C023 (VGC interrupt status), yeah, it's checking that with a lda bpl. 
ok it's still croaking on the ensoniq, albeit, not a 8FF, but one w/o a code. it's sending a command 5 and then reading data..
B8FE
yes, it's still thinking it's a sound irq. the sound check is a bmi.
So I'm returning 0x80 in ensoniq data and..
it's booting farther, with a progress bar this time! (because vbl is on)
Now it crashing on a 08 => C068, and the code is disappearing out from under the C8xx space.
when I reset from this thing with the interrupts on, I think I need to be resetting c041 to 0 on a reset. I added that..
back to C068..
We were executing in C300 space. So, is there a c8xx there that's supposed to be latched in until a CFFF, that sticks even if we turn off INTCXROM ?

[x] mmugs should add output of status of page C8.  

in KEGS, if I do in debugger:
c300
c800l (80-col f/w)
c068:4
c800l (STILL 80-col f/w)

in GS2
c300
c800l (80-col f/w)
c068:4
c800l (80-col f/w is gone gone)

having been in c300 previously should make it sticky and appear there EVEN IF I disable INTCXROM.
ok, this feels like the following:
call megaii->set_C8xx_handler to set a handler for slot 3

// the handlers must only use functions in this area, to set slot-parameters.
void MMU_II::set_C8xx_handler(SlotType_t slot, void (*handler)(void *context, SlotType_t slot), void *context) {
    C8xx_handlers[slot].handler = handler;
    C8xx_handlers[slot].context = context;
}

the handler will set C8 to the same as we do now for INTCXROM, except just C8? 
and in compose_c1_cf if C8xx_slot is set we need to take that into account.
"For more details, see Understanding the Apple IIe, by James Fielding Sather, Pg 5-28."
I'm wondering if I have this wrong in the //e also. Let's see.
Sather refers to "INTC8ROM" as a "unreadable soft switch, set by access to $C3XX with SLOTC3ROM reset, and reset by access to $CFFF or an MMU reset"
Joint control of the C8 range is an OR function if INTernal is true and SLOT false;
"the INTC8ROM follows protocol for a Slot 3 peripheral card that responds to I/O select and I/o strobe; consistent with IIe philosophy of emulating An Apple II with 80-col card installed in Slot 3."
So, INTC8ROM is as if there was a slot card in slot 3 and it makes its ROM appear in C8 when C3 is accessed.

I think the //e may be wrong also?
for //e testing, do:
```
c300
c800l (80-col f/w)
c006:0
c800l (what is result?)

!
800:lda cfff
 sta c007
 lda c800
 sta 1000
 lda c300
 sta c006
 lda c800
 sta 1001
 rts
```
Real //e: 4C 4C
Mariani: 4C 4C
Virtual II: 4C C2
kegs: 4C 4C
Clemens: 4C 4C

GS2 //e: 4C A0 *womp womp*
Real GS: 4C XX ???

is it possible the GS behavior should be: if INTCXROM isn't changing, leave it be?

I have gotten very confused over how this C1-CF stuff works. Diagram and review it.

## Jan 20, 2026

OK, let's do some cleanup of this C1CF code. It's confusing and gunked up.

First, slot_rom_ptable - the ONLY thing that should ever manipulate this is cards registering their roms.
This includes CX, and C8-CF for whatever card C8xx_Slot is set to.

There are four routines:
map_c1cf_page_both(uint8_t page, uint8_t *data, const char *read_d);
map_c1cf_page_read_only(page_t page, uint8_t *data, const char *read_d);
map_c1cf_page_read_h(page_t page, read_handler_t handler, const char *read_d);
map_c1cf_page_write_h(page_t page, write_handler_t handler, const char *write_d);
set_default_C8xx_map is special - don't need to run it all the time. it's intended for: CFFF, and Reset().

Let's make sure there are no other uses of these. Used in:
Videx: setting C8-CF. uses set_slot_rom for its own slot, which calls map_c1cf.
mmu_ii: set_default_C8xx_map. sets C8-CF to nulls.

II+: compose_c1cf simply copies slot_rom_ptable.
IIe: compose_c1cf takes into account the values of the following registers:
  INTCXROM, SLOTC3ROM, INTC8ROM (C8xx_Slot==3) to compose C1-C7.
  IF INTCXROM || INTC8ROM, compose C8-CF.
IIgs: ??? (don't worry about it yet, is identical to //e but with the additional GS onboard devices/roms)



Regression tests:
    [x] Ctrl-OA-CA-Reset (built in self test)
    [x] audit.dsk

Manual Tests:
    [x] my little program above  

set intcxrom


activate memcard rom
set intcxrom
reset intcxrom
memcard rom should still be in c800

[ ] Later change: make slot_rom_ptable 16 entries to make loops look more sensible.

ok, well that was likely a red herring. I have modified the IIe code to work correctly now, and it behaves like other GS emulators but not the same as my real GS. 

tracing the GS/OS boot after enabling interrupts. So far, so good..

[ ] forward disasm is not correctly disassembling MVN/MVP: it thinks it's a 2-byte instruction, when it's a 3-byte. This is a more general broader problem too.  

OK!
the P register being pushed to the stack on IRQs in emulation mode, has B bit set. 
FIXED. Oh man, that was a super duper wild goose chase.
the test suite checked for B set, but had no way to test IRQ so it did not check for B *cleared* in E-mode.

## Jan 21, 2026

troubleshooting a2desktop. It is only writing to aux memory. Currently it's clearing the menu bar area to all white, writing 7Fs. ALTZP=1, 80STORE=1, PAGE2=0. but, ramwrt/ramrd are both on. The Mega II memory tag is "MAIN". ok, debug display of banks is wrong, fixing that.. PC = 4BFF. 
Think about RAMRD/RAMWRT AND 80STORE being active at the same time. (Remember, this works in IIe mode).
So, this should be writing to main memory in the hires region. but I think it's writing to aux.. 
bps on 2000 and 4bff
testing in another instance, manually setting these flags. with PAGE2=0, 2000:3f I get E12000=3f. is it possible I'm not passing a needed flag setting down into megaii? no, because we don't use the megaii's flags here, we compose its memory map in MMU_IIgs.
direct access to E02000: byte goes into the right place. ok, so that leads me to think the issue is is calc_aux_write.
let's trace that..
ah, yeah, here's the problem:
```
    if ((page >= 0x20 && page <= 0x3F) && ((g_80store && g_page2  && g_hires) || (!g_80store && g_ramwrt))) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && (g_ramwrt)) return 0x1'0000;
```
if 80store is on, and hires is on, and page2 is off, it falls through to the second statement, which inappropriately switches to AUX because ramwrt is on.
SO. This should look more like the IF statements in the megaii_compose stuff.. well, I'm going to proceed as-is.

```
// 80store overrides any other settings, for text page 1 and hires page 1 areas.
    if ((page >= 0x04 && page <= 0x07) && g_80store) {
      if (g_page2) return 0x1'0000;
      else return 0x0'0000;
    }
    if ((page >= 0x20 && page <= 0x3F) && g_80store && g_hires) {
      if (g_page2) return 0x1'0000;
      else return 0x0'0000;
    }
```

[ ] in full screen mode GSRGB, we're not accounting for or drawing the side borders.  

Okay I have some new memory mapping weirdness (maybe). in the ROM at FF/2E52 we're loading state reg, pulling something off the stack, replacing with c068, and RTS to the middle of bogus code. Whaaaa.
Only if we boot straight from a RESET. Weeeeird. It's pretty consistent. Track this down later.. 

[x] Display DP register in trace  

It would be superduper helpful to have gstrace be able to decode the address labels. I could have a labels on/off mode. Will need a wider display for that.
What would actually also be super-cool, 

[ ] would be to have a cursor in the trace display. whatever record the cursor is on, we display the PC, registers, P bit decode, etc from that moment.  

OK the problem seems to be: C700 rom can be my pdblock card, or it can be internal rom - which would be the appletalk boot code. 


Slot Cards register their fw into slot_map.
Internal ROM registers its fw into int_map. (simpler, single api call, since it's R/O by definition).

SlotReg.

if INTCXROM is 0,

If SlotReg Bit 7 = 1, slot card ROM is selected, else internal.
etc.
It SLOTC3ROM = 1, slot card ROM for slot 3 is selected, else internal. (same logic, but check different bit).

if INTCXROM is 1,
All C1-CF 

mmu_iigs updates the slot_map. 
but how does it get the original address the slot cards registered? Save the entire table after startup?
or mmu_iie can save two entries, and it can store the SlotReg, but always have value 0xFF. (all slot rom).

is SLOTC3ROM like what bit 3 slotreg would be ?

then storing the Internal ROM offset locations becomes something done once at setup, not every compose.

Now this is a little ick because the //e does not have a SlotRom register. But, mmu_iigs does not directly inherit megaii, so it cannot override the method.

ok, let's do this, and see if we can not break the //e first.
I think that's working.. but the GS is not.

trying to boot slot 7 is borked - I did a C700. a bunch of shit happens, then FF/BA9F does a JML $00C072. which is a RTI in bank 0.
the RTI goes to C700. WEIRD. but then that pretty quickly exits at C736 and dies. BUT this is the appletalk code, this is not my code.
So I need clean trace from between the C700G to hitting C700.
let's dump and see what happens.. it booted fine, wtf, with media.
w/o media, it dies on an RTS at FF/2E58 (goes to bum return location)

ok, clean start.
boots to basic prompt.
insert media.
pr#7
boots.
now ctrl-alt-reset.
still works..
exit to selector
run apple iigs again..
pr#7
still works?
run again..
from basic prompt, insert media
ctrl-oa-reset
CRASH

ok ok ok.
going to BASIC, that is not right. I need to see what these block devices are supposed to do.
but if I try to boot from ctrl-reset with media already in the drive (or from power-on with media in the drive) I get this crash from FF/2E58.
if I bounce to BASIC and then pr#7, it will boot all day long.

and then it making a liar out of me. Now I'm only just powering on, and getting a crash. there are BEBE in $0036 a vector. wth.
something is very inconsistent from run to run. So I'm not initializing something? buffer overrun?
oh, what did I set to return random number.. IWM. hm. 

## Jan 22, 2026

added label display to gstrace. the format is bad, but seeing labels is already immediately helpful.
in my crash trace from last night, when the trace starts we're in SEND80 - might be disk code? Ah, no, it's AppleTalk.
why am I trying to boot via appletalk? the wrong ROM is in place. oh, no, no it IS disk.
let's take out the random number thing.
ok now it's trying to boot, infinite sit and spin. it won't boot slot 7. plain ctrl-reset forces reboot.

it's checking slot 7 for ID bytes. c705=3, c703=0, c701=20, c7ff = 60
then it skips to slot 6
it's in FF/FA05

Well duh, is this how auto boot works? Is the "control panel" set to auto, so it's scanning for a bootable disk?

I'm tracing from a reset. Get to FAA6, which is JSR NEWRESET2
and we're not returning from that?

gah I need a step over:
so if JSR or JSL, that is:
1. set bp of following instruction.
1. When we hit it, automatically stop and clear it.
1. special location, not a "regular" breakpoint?
step out:
set BP type that will break if last instruction is RTS or RTL or RTI.
well that was easy. WHY DIDN'T I DO THAT 6 MONTHS AGO.

then it feels like maybe we need to track "instruction at current PC" uniquely. so it's highlighted as "we haven't executed this yet but the next step/debugger instruction will apply to it."

0249 F8FF A0 9D                 LDY   #$9D                     ;Do some more coldstart stuff
0250 F901 20 9C F8              JSR   GOTOBANKFF               ;Go to bank FF

this routine is: A23E: RAMVCTRS. 

it's never returning from here. This is after it's done the D2D check.

that ends up at B255

ff/a29d: jsr LFF6540 - this never returns.
FF/6540 does a jmp 706f "@reset"
"this routine is used to setup the smartbus driver and reset the smartbus devices, assign their ids, and assign ids by making init calls to the appledisk 3.5 driver routine"
eventually calls SETIWMODE which does inifinite loop until set the mode byte only when it's not what we want, and if it's not we stay here until see that it is what we want".

So, short version: I think I'm beyond 

It's a little odd that we can't reset to BASIC here, but, it is what it is. 3F2 is the softev vector. We could do: 3f2: 00 e0 e0^A5
Nope, it calls ATSReset Soft Reset, then softhandler, but some vector isn't set up so we crash at E1/1010.
ok no biggie.

HM. The Slot Register is not -just- the rom space, it must also be the I/O switches. It's unclear if turning off slot5/6 rom would cause the rom to skip this loop.
so, it must be the case that this just wasn't working before (maybe the rom wasn't being properly switched in / activated?).

Well I guess I have to implement an IWM, and that gives me the next thing to do. 
for demo purposes I can try turning off slots 5 and 6 in "control panel".  Nope. I'd have to recalculate the checksum, and it detected bad checksum and reset the ram to defaults! Tricksy GSSes!

## Jan 23, 2026

ok, I implemented enough real IWM logic that the ROM now lets me boot again. man what a PITA!

Observations: I had done a bunch of memory mapping fixes prior to the IWM issues, and, 

[ ] on a ctrl-reset sometimes we're getting an extra 0x7F (backspace). I suspect what is meant is an 0xFF, like a clear or something.

[ ] Can't boot the nucleus demo - it immediately dumps into BASIC implying unrecognized boot block or ... ? (but, not always).

[ ] Have an "system idle" measurement that tells us what % of realtime is idle vs emulating.


ok so I thought I had figured out where mouse interrupts come from, but no! KEGS says it's the ADB "mode". says it's mode & 0x2. 
that is:
| 1 | Disable auto-poll of fdb mouse |

OH. So if you turn off auto-poll it turns ON interrupts?
but that bit isn't being set in GS/OS.
let's see if anyone is hitting the mouse reg at all..
nope.
What if GS/OS polls the mouse itself?
KEGS clearly has this:
if(g_c027_val & ADB_C027_MOUSE_INT) {
} that value is 0x40, so it's ..

yeah. I did not have a routine to actually write c027. c027 is a mix of read-only status bits, and r/w config bits, including 0x40 which is mouse interrupt enable.
KEGS does have a red herring in there somewhere.. checking adb_mode 2.. oh, perhaps he disables interrupts when mode&2 is set for "disable automatic polling". Makes sense.

ok, I was crashing into monitor on boot..
      //(data_interrupt_enabled && data_register_full) ||
I commented this out. the "data interrupt enable" was set to 1. and "data reg full". Is there some general "no interrupts" thing? Hmm. or I could be triggering it inappropriately.
Well, let's proceed..

ok GS/OS is using a ROM01 routine GETMDATA FF/C4D2. 
1. if bit 7 = 0, return error. (no data available)
1. put bit 2 into carry flag
1. load c024, put into x
1. if carry is set, return error. (bit 1 = 1, meant "y register data available" but we just read x). We have now read X out, and exit, meaning the next read should be a Y.
1. load c027 again, and if bit 1 = 0, return error. (we are about to read y, but it still says x)
1. read c024 into y
1. return with C clear

We got there from an IRQ - FF/B7CC
checks C027 - if bit 6 is on, and if bit 7 is on, then dispatch to..
FE/C5C8 - Mouse

ok I think I have the ADB mouse interrupt and status and x/y indicator thing working the way the ROM expects. I still have no mouse movement.. curses!
or, uncursors!

OK, so I -have- made progress here. the GS is processing mouse interrupts. it is just not acting on the data it's reading tho.. 

The interrupt handler on the GS is very inefficient. No wonder I wrote my own interrupt-driven serial handler and put it at the top of the chain!

OHHHHH

it's working but the mouse CURSOR isn't drawing at the new location! like I can move the mouse and click and get menus to pull down and stuff.
WTH. 

Cursor draw is handled by the SCANLINE interrupts!

with scanline interrupts I should now be able to do 3200 color pics.. that will tell me if I'm throwing the interrupt in the right location.

I am getting double events - i.e. if I click, it's like I get two clicks one after the other. Better check into that "last mouse button status" field.

Splash screens of "The Tinies" game is a 3200 pic, looks correct.

## Jan 24, 2026

[x] mouse - if you have moved, but then do ONLY a click, we're getting an event that still has movement components from last event. probably need to clear/ignore that.
[ ] mouse - should only interrupt max 60 times per second during vbl

KEGS does this thing where it doesn't capture, it somehow syncs the GS mouse location with the real OS mouse location. how the heck does it do that? (I'm not sure I like that, and it seems it would be prone to problems / weird interactions).

arekkusu writes:

Re timing: I have been looking at this exact area ^_^; Remember that the MegaII performs "memory refresh" for the first five cycles of HBL  .:  the SCB value can not possibly be known any earlier than cycle six into the right-hand border.  Now here, it looks like my implementation and testing notes are currently out of sync, so the IRQ actually fires either on cycle six or cycle eight of HBL, I'll have to go re-verify this.  Please confirm with your own investigations!
You can verify this visually; i.e. spin on the SCB status bit (not using an IRQ handler) and toggle the border color after some known phase delay to observe the result on screen.  But you can also verify it analytically, counting cycles until that SCB status bit flips, relative to another timing source, i.e. C019.  If you do it that way, you also need to be aware of the subtle detail that the entire MegaII appears to be delayed one cycle relative to the VGC.

[ ] make sure the SCB status bit sets/resets regardless of whether interrupts are enabled.  
[x] "interrupt enabled" doesn't belong in the video scanner. The scanner should simply provide the info. Decision to assert int lives in VGC area.

[x] Do a test of page-flipping on the hcounter (horzpage2flip2).

I changed Gamecontroller to use 14M clock instead of 1M clock, which wasn't appropriate in fast mode. now, we're not slowing the system down when accessing c064/c065, so the Gaultnet joystick detection code at 00/B2CE isn't working. Its "key mouse" with up/down detection is working great tho.
Basically this routine fails and likely marks no joystick, and so it's not using JS later.
JS still works in other GS stuff and in IIe mode. 

[x] when in mouse capture mode, the "real" mouse is still moving around (but hidden), and can sometimes click on the OSD slideout control  

## Jan 25, 2026

So there are FOUR interrupts that depend on the Video Scanner: 
* scanline interrupt
* vbl interrupt
* quarter-second interrupt
* one-second interrupt

the 1/4 sec interrupt is defined as: 16 frames. the 1 sec is defined as: 60 frames.

So really these are variations of the vbl.

Instead of passing a bunch of state in to the VideoScanner and having it make these decisions about when to fire an interrupt, I was thinking it should simply call a single callback in display, with an event type, and then display can decide what to do with it given state flags.
I.e., VS will simply, on -every- scanline with the interrupt bit set, call display's "update interrupt". And on -every- vbl time, do the same. Then that's it. Yes, THIS IS THE WAY.

experimenting with having "fast speed" be 7mhz. It's pretty smooooooth. 

Since we now have important system interrupts depending on the VideoScanner, ludicrous speed video mode is not going to have those interrupts thrown. We're going to need a concept where the cpu cycles is dissociated from the 14m clock, but the 14m clock continues as normal.

Now we could have a super fast speed, even 28mhz. multiples of 14m would be easy to keep in sync and use the same basic routine as now. but when the system speed is completely asynchronous (unclocked) that's where we run into problems.

If we DID have a ludicrous speed with a defined clock number, that could be user-selectable based on their hardware. Or we could estimate and pick one based on .. system idle measurement? i.e. inch upward in 14M increments until we get to a certain idle %, like, down to 20% idle or something. that gives us some buffer.

And the important part here is then we'd get rid of the special separate gs2 execute loop, and, get rid of Apple2_Display. We'd rely solely on the videoscanner. 

Alternatively we could try to dynamically measure and estimate when frames have ended. but scanline interrupts just aren't gonna work unless we're tracking a scanline! i.e. doing videoscanner.


[ ] A systemconfig option needs to be "gs system speed". Then the speed selector switch will toggle between 1mhz and the system speed.

This can be 2.8 (normal), 7.1 (basic ZIP) 14.3 (ZipGS Supra). Maybe we should put the ZipGS control registers, I think they're pretty simple.

We aren't doing any cycle sync/slowdowns at the moment of course. But when we do, I wonder how that interplays with the 7/14/ludicrous speeds.

I fixed when Scanline interrupt triggers (when SCB is read) but the iigs pointer still disappears moving upward at 7mhz. Moving downward is fine.
At 2.8mhz it's fine. This may just be a thing that happens "accelerated"? does it assume it is going to take a while to get to the handler and if it happens too fast then it 

the guys confirm that's normal on an accelerated GS, and that system 6 has a setting "smooth cursor" or something. It's the "Smoother mouise cursor" in Monitor CDEV.

[x] Setting Mouse Speed Fast in control panel causes a hang.  (is is trying to send a adb message to mouse which we don't support yet?) This could change the scaler we use in the mouse code. (now works!)

yeah, it sure is:
ADB_Micro> Executing command: B3 
TRANSMIT 2 BYTES - unimplemented

It's either looping waiting for a response that isn't coming, or just otherwise confused.

ok, in the rabbit hole of : transmit 2 bytes.
apparently ADB listen packets, the data bytes for the register are sent MSB first. i.e. in reverse. 
```
0268 80A5 A9 B3                 LDA   #$B3                     ;LISTEN R3 ADRS 3 (MOUSE ID)
0269 80A7 20 72 81              JSR   CMDSEND                  ;SEND COMMAND
0270 80AA A9 03                 LDA   #$03                     ;SEND NEW HANDLER - ADDRESS 3
0271 80AC 20 72 81              JSR   SENDDATA                 ; & DISABLE SRQ
0272 80AF AD F9 02              LDA   |USERHMR                 ;GET MOUSE TICK RESOLUTION
0273 80B2 C9 02                 CMP   #$02                     ;>=2 then ask mouse to change resolution
0274 80B4 B0 04                 BCS   HISPEED                  ;Bra for high resolution 
0275 80B6 A9 01                 LDA   #$01                     ;Else set mouse handler ID=1, low res
0276 80B8 80 02                 BRA   SENDID
0277 80BA              HISPEED  EQU   *
0278 80BA A9 02                 LDA   #$02                     ;Mouse handler ID=2 for high resolution
0279 80BC              SENDID   EQU   *
0280 80BC 20 72 81              JSR   SENDDATA                 ;SEND HANDLER 2 (LOW RES) OR 3 (HI RES)
0281 80BF
```
this is from ROM03. It's sending cmd B3; followed by 3 - mouse is address 3 - followed by the handler.
ADB_Micro> Executing command: B3 03 01
This is most significant byte first.
So in the Mouse routine that listens, I need to interpret them in the register backwards or should I copy them out backwards?
the first byte sent is the device ID, high byte of the 15-bit register value.
I should make sure to rationalize all the ADB Messages this way, for consistency. Bits 15-8 first, then bits 7-0. 
Remember, it's this:
https://www.lopaciuk.eu/2021/03/26/apple-adb-protocol.html

SO. Setting Mouse Handler=2 means "high resolution" -i.e. we're changing the mouse scaling. woot!

## Jan 26, 2026

According to John Brooks I'm right, Bit 7 of 5503 register E0 is active LOW. 
C03D double-read is because the sound GLU has a read pipeline. Each read returns the state of the 5503
at the cycle <after> the last read access
If you want the 'current value' you need to read twice
so you do the double read for basically any read from the 5503?
Whenever you change the DOC register being read, or if it has been awhile since the last read and you need the latest value.
Yes, the 5503 runs slow at 895KHz
57 cycles per scanline

the Rastan interrupt handler code is:
```
   LDA #E0
   sta c03e
   lda c03d
   lda c03d
   bmi :notasoundirq
```
this mirrors the rom code in irq_sound. 
```
stale read of current FIFO contents
read from 5503 triggered
whenever that completes, the FIFo is updated
while waiting for 5503 read, C03c[7] will be 1 (busy)
```

ok, so one way to model this is for soundglu to NOT call the 5503 every time. Ah, I could clock it with the 14m.

use C03C[7] busy flag.

one 895000 cycle is 16 14M's.

if c14m < doc_read_complete_time
    return current stored value
    doc_read_complete_time = c14m + 16 ; // or whatever appropriate (subtract some for current cycle?)
    set c03c[7] = 1
if c14m >= doc_read_complete_time:
    call 5503 and return that value
    set c03c[7] = 0

the update logic has to be on c03c reads also to -just- turn off the busy bit. but let's say most s/w doesn't care about this.

got it!! it's working pretty well.

Noisetracker is hanging on C03C:7 waiting to go to 0. it's never going un-busy. what if I comment out the bmi..

oops! at 7MHz apparently 16 14M's is too much to get snagged in the next read. figure out what the # should be... (should probably be something small like 2? difference between 14 and 16?)
try to reason it out .. it's a 1MHz read. We start the read, it will be 2 cycles slow at 1MHz.
oh, no 16 14Ms isn't the right math. 
128 nanoseconds
a 14M is 70ns..
so if we make the delay 2 or 4 that should get it done. IT DID.

[ ] Qix audio is playing at a fraction of the rate it should. like 1/8.  


## Jan 27, 2026

ok well it's time to think about this dumb-arse speaker sync issue again. Let's think this through.

Leaving emu overnight results in skew, and several seconds delay. I should have checked the stats. But I bet it wasn't queued audio. It was probably mismatch between the cycles counter inside Speaker, and c14m counter. meaning Speaker's generation of samples is behind. This is a pretty simple check. if at end of audio Speaker counter is different from c14m, then do an assert or a true-up.

GS bonk doesn't have artifacts ever, really. Is this because we're starting in debugger and we have given the audio system enough time to start up? Maybe we should just delay processing to give audio time to sync up? i.e. do a sleep or delay instead of padding the queue? we know SDL will fill its own buffer when needed. and this has to be driven outside the event loop, couldn't work any other way..
Well, I have a little hack in the code right now that checks if we're out of sync, and it then resets. Sometimes when the IIe starts up this goes crazy. It -never does- on the GS.. OR on the iie with 816.
Ah ha, so there is some RESET condition not being cleared properly on a 6502 causing this?!
Eeenteresting. Come back to this later..

Well Claude fixed my speaker malfunction - when we reset the cycle counter in SpeakerFX, we were creating a situation where there were stale events in the event queue, causing essentially an infinite blowup.
I've tested the modest fixes by moving windows around, hiding the GS2 windows, etc., and these events DO create skew, which sp->reset now fixes without then exploding the module.

If we get rid of the Apple2_Display from normal emulator usage, it still has a purpose: to render screens thumbnail size in the debug window. in fact, that will be superduper cool - and it's well-suited to it, except we might alter the API to make it easier to select the video mode we want it to render.

ok I want the dumb CDA control panel to work! How..

look at the ROM03 interrupt handling code in ADB.md.

## Jan 28, 2026

ok, making some progress with control panel ctrl-oa-esc

DoCDAMenu (FE/ADF1) is calling tool 0B05, and therein we get a Fatal System Error 0911.
This is in the interrupt handler for 0x20. 0B05 is SaveAll "save all variables that DM preserves when the CDA menu is activated". Sets display to text mode. 
C027=B0, C026=20 (datareg). Shouldn't the IRQ clear on reading C026?

b9a9
FE/AFCB is the tool call 0B05. 
ok in that routine, at fe/b076, it is doing JSL E1/0094 to "init the 80 column firmware". That's where we're dying. uh, E1/0094 is "TOBRAMSETUP" set up system o bram parameters routine.
in additionif called with carry set it does NOT set up slot config (internal vs external). (there's sec right before call).

ok now we're at FF/B6F4, in tobramsetup. Usersetup. 

Uh, why is the IRQ asserted and DataReg[5] set again? 

ok, so at FF/B769 there is a jsr PANELFDB (FF/85CB) which is going to send an abort to the ADB . ok, that's what is breaking..
continue to drill down..
ah ha

```
0249 808B F4 01 00              PEA   $0001                    ;ABORT command
0250 808E 4B                    PHK                            ;Dummy address
0251 808F 62 04 00              PER   @1-1                     ;Return address
0252 8092 22 14 00 FE           JSL   >ADBSENDTL               ;Direct ADB tool call for sending a cmd
0256 8097 E2 30                 SEP   #$30                     ;8 bit m/x
0257 8099 90 07                 BCC   @3                       ;BRA if successful send
0258 809B 68                    PLA                            ;Get back retry counts
0259 809C 3A                    DEC   A                        ;
0260 809D 10 E8                 BPL   @0                       ;RETRY !!!!
0261 809F 4C B1 81              JMP   ADBERR                   ;Display $911 error
0262 80A2              @3       EQU   *
0263 80A2 C2 10                 REP   #$10                     ;8/16 BIT m/x
0264 80A4                       LONGA OFF
0265 80A4                       LONGI ON
0266 80A4 68                    PLA                            ;Clean stack
```

ok, we're expecting a response from the ABORT uC command. 
huh, KEGS doesn't implement abort at all.

the rom01 code is quite different here, but should be doing the same thing. 

we have sent the abort already, so this should be reading the response..
```
LDA #0F  <= this is "send an adb command"
FF/85E2: JSR 83A2
JSR 83D6 - read response byte from Micro with timeout
LDA C027
ROR A
TXA
BCS 83ED - carry was low
STA C026 => 0F    ; what is this? read layouts? no, doesn't make sense.
LDA C027 <= B0
ROR A   <= checking bit 0 = 1
bcs loop 
rts     <= so it IS sending a 0F layouts command and waiting for command to be accepted.
bcs..
AND #10   <= actually checking 20 since we did a ROR
beq 83b1 <= if nothing in data reg..
LDA C026 <= 00    but we didn't put anything into data reg
TSB 0FD7 <= 00   so it's testing, and setting nothing
RTS
BCS 8657  <= adberr skipped this.. so no err here..

LDY #0
JSR 8442       ; RCV Data, this loops waiting for C027[5] to go high, which it won't
```

adb debug should show detail status on command buffer and response buffer.
I should write an ADB Test routine. Alternately, is there a way to trigger the ROM's ADB self test?

ok the issue here is we are returning a 00 every time before we return other data. because of the test we added in read_data_register.

whoa what just happened. I booted a floppy, hit ctrl-reset, and got the CDA panel.
IRQ=0 too.
but it took a reset?

## Jan 29, 2026

[x] need to call Ensoniq reset on a reset  

[x] F1 should also capture, in addition to mouse click  

[x] Add a 14MHz mode between 7MHz and Ludicrous Speed  
[x] Properly rename 4MHz to 7_1MHz everywhere, including in OSD  

ah, I think I can resolve my border/content scaling problems by doing this:
1. Render the individual components first into a target texture using 1:1 (no scaling).
1. scale that all at once to target size

This will also work better for testing out the postprocessing from rikkles.

an idea for rendering 640 mode dithering: instead of scaling pixels A-B-A-B pixels like this:
A-A-B-B-A-A-B-B
do it like this:
A-B-A-B-A-B-A-B

so there would be pre-scale this way first, getting us 1120 pixels, then scale accordingly into the target window. 

## Jan 31, 2026

So the issue with QIX audio playing slowly is likely a result of this:
we are only generating a frame every 1/60th second. that means any interrupts that ought to have been generated during the frame are delayed triggering until frame time. 

I also need to check to make sure we leave the oscillator ID in the register after we clear the interrupt.

There are some possible approaches.

1. interleave ensoniq "cycle update" as we do the cycle-accurate video.
1. have the code calculate and scheduleEvent to throw interrupts during code execution.
1. have the ensoniq run in its own thread.

Gemini summarized MAME's architecture and it looks like pretty much what I'm doing, with the exception that some devices I'm trying to make frame-based, might be better off being "cycle accurate" - i.e., leveraged into the incr_cycle architecture.

https://gemini.google.com/app/43a03be14e7bd935

1. The "Next Event" CalculationInstead of just waiting for an update call, your 5503 model should look at its active oscillators and calculate the Time to Next IRQ ($T_{irq}$).Since the 5503 knows its current sample pointer, the frequency (step size), and the loop end point, you can calculate exactly how many cycles remain until a wrap-around or a halt occurs:
2. Registering a MAME timer
ok, that's basically what I do in the Mockingboard code also - keep track of important event times in the future and set a schedulerEvent to callback so we can handle it.
3. Force Update on Register Writes. 
this is something claude has noted, which is, when registers change, we need to generate audio up to that point and then change the register. But by itself that wouldn't be enough.

OK I think all of this is probably not that heavy a lift.
If we did it cycle by cycle, we'd have to track fractions because 14M and 875KHz are not even multiples.



Probably the next thing I should do is figure out and implement the Apple IIgs RAM/ROM/MegaII cycle timing.

Rather than putting in increasingly more complex logic into incr_cycle, I should probably do this:

incr_cycle() is a very simple that just does this:

inline void Clock::incr_cycle() {
  if (cycle_handler) (*cycle_handler)();
}

Then we can stuff different cycle handlers into it, e.g. switching in the LS handler when we want, having different handlers for IIe/IIgs. The inline and this being a very small routine, should make this inlinable throughout the CPU code.

What does a new clock abstraction look like?

Set the cpu clock speed.
interrogate information about the clock.
It's allocated separately, injected where needed.
Track clock ticks for the CPU. 

Having the handler like that is icky, what I should do is use, you know, proper class stuff. Have a abstract base class that does nothing, then have a variety of clock subclasses that do increasingly useful stuff.

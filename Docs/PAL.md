# PAL Video //e

Apple //e systems destined for PAL markets had slightly different clocking and a different video scanner setup.

## Video Scanner

The horizontal scan portion is identical to an NTSC //e, but the vertical is different:
instead of 262 scanlines per frame, there are 312 scanlines per frame. All the extra 50 scanlines are during VBL, so there is no change to the video display's resolution, just to the timing outside the visible area.

Horizontal seems to be identical, namely 912 14M cycles per scanline. (The 14M is, of course, slightly different than in the NTSC models).


## Clocking

Instead of a main system clock of 14.31818MHz, the system clock is either:
14.25045MHz - "discrete clock"
14.25MHz - "hybrid clock"

This provides an effective CPU clock rate of 1.015657MHz, a hair slower than American II.

Will need a way to choose/select the "region" when composing systems. 


## Architecture

Impacted code is as follows:

### systemconfig

the struct should probably now include settings for:

the Video Scanner selection.
the system clock selection.

### VideoScannerII, VideoScannerIIePAL

a new module to generate the correct scanner lookup table. However, the existing Scanner class tree assumes 17030 cycles per frame, and, there is a macro of that value. There is a type defined that controls how big all the lookup tables are, and that's in VideoScannerII.hpp. So this needs to be somehow overridden or defined per class. I suppose a proper subclass (VideoScanner) could dynamically allocate it in the constructor.

typedef scan_address_t scanner_lut_t[SCANNER_LUT_SIZE];

### ScanBuffer

This should be fine as it is, as it's defined with 32768 entries. Still plenty for storing memory fetches.

### mouse.cpp:

assumes 17030 cycles per frame. This will break on a speed change then. Its update needs to occur *after* we know what system speed is going to be for next frame because need to keep synced with vbl. Or identify a different method of generating mouse VBL interrupt.

### mb.cpp, speaker.cpp:

both assume effective CPU clock rate is 1020484 hz.

### gs2.cpp:

assumes 238944 14Ms per frame.
also assumes 16688154 / 16688155 microseconds per frame.

### cpu.cpp:

clock mode table assumes various NTSCisms, and 17030 cycles per frame.

### Videx

will only be called 50fps but should otherwise be fine since it's a full frame update every time.

### clock.hpp

probably define some utility functions in here to simplify managing all this data.


## clock struct

```
typedef struct {
    uint64_t hz_rate;            // this is used only in speaker to detect a speed shift.
    uint64_t cycle_duration_ns;  // REMOVE: this is no longer used.
    uint64_t cycles_per_frame;   // used only in mouse.cpp
    uint64_t c_14M_per_cpu_cycle; // critical to incr_cycle, copied into cpu on a speed change; unit is 14Ms
    uint64_t extra_per_scanline;  // critical in incr_cycle; unit is 14Ms
    uint64_t cycles_per_scanline; // used in mouse to calculate initial vbl

    // need to add
    uint64_t c14M_per_frame;       // either 238944 (US) or 284544 (PAL)
    uint64_t us_per_frame_even;
    uint64_t us_per_frame_odd;
    uint64_t eff_cpu_rate;
} clock_mode_info_t;
```

The Video Scanners need to know 17030 or 20280. But they use these figures regardless of the cpu clock rate. Whereas mouse wants the figure based on current system speed setting.
On the other hand, c_14M and extra_per are super important to the video scanning - a calculation involving them should result in the correct number of video cycles per frame, but does not need to directly interact. Perhaps we can include that sanity check and assert out at startup time if I get the calculations wrong (as I did many times before!)


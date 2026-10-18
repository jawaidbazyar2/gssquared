# IIgs PAL Video Scanner

Specification for adding 50 Hz (PAL) video timing to the IIgs video scanner,
with runtime switching between NTSC and PAL under software control.

Relates to TodoList item: `IIgs PAL timing mode`.

Reference: Apple IIGS Technical Notes 36–49 (Tech Note #39, Mega II video counters).

## Background

The IIgs can switch its video timing between NTSC (60 Hz, 262 lines) and PAL
(50 Hz, 312 lines) via a softswitch. Some games and demos switch to 50 Hz to get
a slower frame rate in exchange for many more cycles in VBL to update the
display without tearing.

## Hardware model

- The vertical counter counts up from $100. Display line 0 is counter $100.
- When the counter rolls over from $1FF, it loads a preset value:
  - NTSC: $FA, giving $100–$1FF + $FA–$FF = 262 lines.
  - PAL: $C8, giving $100–$1FF + $C8–$FF = 312 lines.
- All the extra PAL lines are in VBL. Lines 0–255 (counter $100–$1FF) are
  identical in both modes.
- The NTSC/PAL softswitch is a live one-bit input to the preset logic. It is
  not latched or sampled at any frame boundary. Software can change it at any
  time, but the change only has a visible effect at the instant the counter
  rolls over from $1FF and loads the preset.
- Display address generation is a pure function of the H and V counters, so
  the address for a given counter value is the same in both modes.

### Mode transitions mid-frame

| Switch happens while counter is in | Result |
|---|---|
| $100–$1FF (lines 0–255) | The preset at the next rollover uses the new mode. |
| NTSC, $FA–$FF | No preset is loaded before $100. The frame finishes as NTSC; the next frame is PAL. |
| PAL, $C8–$F9 | The counter keeps counting up through $FF. The frame finishes as a full 312-line PAL frame; the next frame is NTSC. |

Neither the hardware nor the emulator ever "warps" the counter.

### Softswitch

$C02B (LANGSEL) bit 4: 0 = NTSC (60 Hz), 1 = PAL (50 Hz). Confirm against the
Hardware Reference / Tech Note before implementing. `display_write_C02B` in
`src/display/display.cpp` already stores this bit (`set_langsel`) and has a
`TODO: set video mode timing ntsc vs pal.`

## Design

### LUT layout: counter order, rotated

There is one LUT per video mode table (`lores_p1`, `hires_p1`, ..., `shr_p1`),
sized for the PAL frame: `312 * 65` entries. Build it by running the hardware
counter sequence with a $C8 preset:

| LUT line | vcounter |
|---|---|
| 0–255 | $100–$1FF |
| 256–311 | $C8–$FF |

This is a counter-indexed table, rotated so that index 0 is still the top of
the display. Because the scanner only ever increments, no per-cycle conversion
between index and counter is needed.

- PAL uses lines 0–311 in order.
- NTSC uses lines 0–255, then lines 306–311 (counter $FA–$FF).

Constants:

```cpp
constexpr uint32_t SCANNER_LINES_PAL   = 312;
constexpr uint32_t SCANNER_LUT_SIZE    = 65 * SCANNER_LINES_PAL;
constexpr uint32_t ROLLOVER_INDEX      = 65 * 256;  // $1FF -> preset
constexpr uint32_t NTSC_TAIL_INDEX     = 65 * 306;  // counter $FA
constexpr uint32_t FRAME_INDEX         = 65 * 312;  // $FF -> $100
constexpr uint32_t NTSC_FRAME_CYCLES   = 65 * 262;  // 17030
constexpr uint32_t PAL_FRAME_CYCLES    = 65 * 312;  // 20280
```

`SCANNER_LUT_SIZE` is currently `65*262` and is used both for the
`scanner_lut_t` typedef and as the init loop bound. It must grow to the PAL
size.

### Hot path

The per-cycle cost stays at one compare, the same as today's
`if (++scan_index == 17030)`:

```cpp
if (++scan_index == next_boundary) on_boundary();
```

`on_boundary()` runs twice per frame:

```cpp
void on_boundary() {
    if (scan_index == ROLLOVER_INDEX) {          // $1FF -> preset
        if (!pal) scan_index = NTSC_TAIL_INDEX;  // preset $FA
        next_boundary = FRAME_INDEX;
        // quarter-second / other rollover-timed events go here
    } else {                                     // FRAME_INDEX: $FF -> $100
        scan_index = 0;
        next_boundary = ROLLOVER_INDEX;
    }
}
```

`pal` is read live at the rollover, which models the one-bit input to the
preset logic exactly. `reset()` sets `scan_index = 0` and
`next_boundary = ROLLOVER_INDEX`.

The hardcoded `17030` wraps in `VideoScannerII::video_cycle()`,
`VideoScannerIIe::video_cycle()` and `VideoScannerIIgs::video_cycle()` are
replaced by this mechanism.

### Values computed on demand

These are derived from `scan_index` only when asked for, not every cycle:

- `get_vcounter()`:
  `line < 192 ? 0x100 + line : line < 256 ? 0x1C0 + (line - 192) : 0xC8 + (line - 256)`.
  This is correct in both modes because NTSC lines 306–311 map to $FA–$FF.
  $C02E/$C02F reads (`display_read_C02EF`) then work unchanged.
- `get_hcounter()`: unchanged.
- `is_vbl()`: `scan_index >= 192*65`, unchanged.
- `get_vcount()` (OSD/debug): returns the LUT line. In NTSC it shows 306–311
  for the last six lines instead of 256–261. This is cosmetic; optionally
  subtract 50 when `!pal && line >= 306`.

### Interrupts

- VBL interrupt at `scan_index == 192*65`: unchanged.
- Quarter-second interrupt, currently `scan_index == 256*65` in
  `VideoScannerIIgs::video_cycle()`: in NTSC the jump happens on the
  increment, so `video_cycle()` never sees index `256*65` and this check would
  stop firing. Move it into the rollover branch of `on_boundary()` (or have
  `on_boundary()` set a pending flag consumed by `video_cycle()`).
- The SCB scanline interrupt is tied to `SA_FLAG_SCB`, which only exists on
  lines 0–199, so it is unaffected.

### Flags

Addresses are identical in both modes. Flags (`SA_FLAG_HBL`, `SA_FLAG_VBL`,
`SA_FLAG_HSYNC`, `SA_FLAG_VSYNC`, `SA_FLAG_BORDER`, ...) describe the raster
layout and may differ between modes.

NTSC flags are built from today's flag code using:

```cpp
uint32_t ntsc_line = (L < 256) ? L : L - 50;
```

so lines 192–255 and 306–311 get today's NTSC flags for lines 192–261 (for
example, the top border covers lines 243–255 and 306–311). NTSC lines 256–305
are never visited; fill them with PAL flags or zero.

If the PAL VGC places bottom border, blanking, VSYNC and top border at the same
counter values as NTSC, with all 50 extra lines added as top border or
blanking, one table serves both modes. Otherwise, use two flag sets:

- Add `PAL` to `vs_mode_switch_t`, with an appropriate entry in each platform's
  delay LUT.
- Allocate each table as `2 * SCANNER_LUT_SIZE`: the NTSC half and the PAL
  half have the same addresses and different flags.
- `set_video_mode()` adds `pal ? SCANNER_LUT_SIZE : 0` when selecting
  `video_addresses`.

This way the flags respond to the bit immediately (through the normal mode
change path) and the preset responds at rollover (through `on_boundary()`),
without special-case code.

Memory: 7 tables × 2 × 20280 entries × 4 bytes ≈ 1.1 MB. Acceptable.

### Mode-change queue

`queue_mode_change()` currently computes:

```cpp
uint16_t apply_at = scan_index + delay;
if (apply_at >= cycles_per_frame) apply_at -= cycles_per_frame;
```

and `drain_due_mode_changes()` compares `apply_at == scan_index`. With the NTSC
jump, a change queued in the last cycles of line 255 gets `apply_at` in the
skipped range (`256*65 + n`) and never fires.

Replace `apply_at` with a free-running video cycle counter:

```cpp
uint64_t vcycle = 0;   // incremented once per video_cycle()
// queue:  apply_at = vcycle + delay;
// drain:  while (mode_q_count && mode_q[head].apply_at == vcycle) ...
```

This removes any dependence of the queue on frame length or LUT layout. The
cost is one increment per cycle.

### Frame length

`cycles_per_frame` is split into:

- LUT size: always `SCANNER_LUT_SIZE`, used by `allocate()`.
- Actual frame length: 17030 or 20280 video cycles, which can vary from frame
  to frame. Expose it through an accessor (for example
  `get_frame_cycles()`), and have the scanner record the length of the frame
  that just completed in `on_boundary()`.

A transition frame can have NTSC addresses for its first part and a PAL
length, or the reverse. Consumers must not assume every frame is exactly 262
or 312 lines.

## Consumers to audit

### Based on 14M cycles

The speaker, frame pacing in `gs2.cpp`, and other 14M-based timing need
careful math when the number of 14M cycles per frame changes at runtime.
`NClock` already has `pal_clock_mode_info` (`c14M_per_frame` 284544,
`vid_cycles_per_frame` 20280), but it is selected per system, not switched at
runtime, and it uses a 14.25045 MHz crystal (IIe PAL). See open questions.

### Based on 17030 video cycles

These assume a fixed 17030 video cycles per frame and need review:

- `VideoScannerII::video_cycle()`, `VideoScannerIIe::video_cycle()`,
  `VideoScannerIIgs::video_cycle()`: hardcoded `17030` wrap.
- `NClock`: `vid_cycles_per_frame`.
- `src/devices/mockingboard/mb2.cpp`: frame rate from
  `get_vid_cycles_per_frame()`.
- `src/devices/applemouseiii/MouseController.hpp`: `US_60HZ_CYCLES = 17030`.
- `src/gs2.cpp`: `get_vid_cycles_per_frame()` use.
- `VideoScannerII::queue_mode_change()`: `cycles_per_frame` wrap (replaced
  above).

### Scan generator

`VideoScanGenerator.cpp` counts HSYNCs per frame and has a `vcount >= 263`
check. It must accept at least 312 lines. Check the frame buffers
(`frame_byte`, `border`, `frame_shr`) for line-count assumptions as well, and
decide how the extra PAL border lines are presented in the output frame.

## Optional: unify IIe PAL

`VideoScannerIIePAL` could become the same scanner with `pal` fixed to true,
and `VideoScannerIIe` the same with `pal = false`. This also fixes two existing
bugs in `VideoScannerIIePAL::init_video_addresses()`:

- It presets to `0xFA` although the comment says `0xC8`, so lines 262–311
  repeat the addresses for $100–$131 instead of $C8–$F9.
- Its `mixed_mode_text` is missing the `|| vcount < 0x100` term that the other
  scanners have.

Both only affect the floating bus during VBL.

## Testing

- Build the LUT and dump it (extend `dump_cycles()`): verify the counter
  sequence $100–$1FF, $C8–$FF and that lines 306–311 match today's NTSC lines
  256–261 for addresses and NTSC flags.
- NTSC regression: with `pal = false`, frame length is exactly 17030 cycles,
  and per-cycle addresses, flags, $C02E/$C02F values and interrupt timing
  match the current implementation.
- PAL: frame length is exactly 20280 cycles; $C02E/$C02F report $C8–$FF
  during lines 256–311.
- Transitions: flip the switch during each region in the transition table
  above and verify the resulting frame lengths (via debug protocol
  breakpoints and `get_vcounter()`).
- Mode-change queue: queue a delayed switch in the last two cycles of line 255
  in NTSC and verify it applies.
- Quarter-second interrupt fires once per frame in both modes.

## Open questions

- Confirm the softswitch bit ($C02B bit 4) and its polarity.
- Does a PAL IIgs use the same 14.31818 MHz-derived clock as an NTSC IIgs
  (only the line count changes), or a different crystal? This decides whether
  PAL mode changes only frames per second, or also cycles per second.
- Where does the PAL VGC place VSYNC, blanking and the borders? This decides
  whether one flag set suffices.
- How should 50 Hz output be presented to the host display (frame pacing
  and presentation at 50 Hz on a 60 Hz host)?

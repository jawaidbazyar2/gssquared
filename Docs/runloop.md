# Keeping the Emulation Running During macOS Menu Tracking

## The Problem

When a user clicks on a native `NSMenu` on macOS, the Cocoa framework enters a modal
event-tracking run loop inside `[NSApp sendEvent:]`. This blocks the calling thread
until the menu is dismissed. The call chain that blocks is:

```
run_cpus()
  → frame_event()
    → SDL_PollEvent()
      → SDL_PumpEvents()
        → Cocoa_PumpEventsUntilDate()
          → [NSApp sendEvent:menuClickEvent]
            → macOS menu tracking loop (BLOCKS HERE)
```

The entire `run_cpus` while loop freezes: no CPU emulation, no audio generation,
no device frame processing, no display updates.

The same blocking occurs during window resize drag operations (also a modal tracking
loop in Cocoa).

## Current Solution (adbtest / display-only)

An `NSTimer` is scheduled in `NSRunLoopCommonModes` when `NSMenuDidBeginTrackingNotification`
fires. Since `NSRunLoopCommonModes` includes `NSEventTrackingRunLoopMode`, the timer
continues to fire during menu tracking. The timer calls a user-provided callback
(in adbtest, this is `SDL_AppIterate`). The timer is invalidated when
`NSMenuDidEndTrackingNotification` fires.

This works perfectly for the adbtest harness because `SDL_AppIterate` encapsulates
one complete frame of work with no external state dependencies.

## The Challenge for Full Emulation

`run_cpus()` is a monolithic function whose while-loop body IS one frame of emulation.
The frame logic cannot currently be called from a timer because:

### 1. Local State Lives on the Stack

`run_cpus()` holds frame-pacing state as local variables:

| Variable | Type | Purpose |
|---|---|---|
| `last_cycle_time` | `uint64_t` | Wall-clock time of last frame start |
| `frame_count` | `uint64_t` | Frame counter (odd/even timing) |
| `last_start_frame_c14m` | `uint64_t` | 14MHz cycle count at last frame start |
| `speaker_state` | `speaker_state_t*` | Cached pointer to speaker module state |
| `ds` | `display_state_t*` | Cached pointer to display module state |

These would need to be accessible to a timer callback. Note: `frame_count` already
exists in `computer_t` (line 91 of computer.hpp). The cached pointers (`speaker_state`,
`ds`) are derived from `computer_t` and could be re-fetched. The two timing variables
(`last_cycle_time`, `last_start_frame_c14m`) would need to move to `computer_t`.

### 2. Three Execution Modes with Different Inner Loops

The while-loop body branches into three modes, each with a different structure:

- **EXEC_STEP_INTO**: Run N instructions, then sleep ~16ms. (Debugger stepping.)
- **EXEC_NORMAL (clock != FREE_RUN)**: Run CPU cycles until end-of-frame (14MHz cycle
  target), then process events/audio/video/devices, then sleep remainder of frame time.
- **Ludicrous Speed (FREE_RUN)**: Run CPU as fast as possible for ~16ms wall time,
  then process events/audio/video/devices. No sleep.

Each mode handles frame-end bookkeeping (counter updates, clock advancement) slightly
differently.

### 3. `frame_event()` Calls `SDL_PollEvent()` — Reentrancy Concern

During menu tracking, the main thread is already inside `SDL_PollEvent()` →
`Cocoa_PumpEventsUntilDate()` → `[NSApp sendEvent:]`. If the timer callback also
calls `frame_event()` → `SDL_PollEvent()`, we re-enter SDL's Cocoa event handling.

**This is likely safe in practice**: the inner `SDL_PollEvent` call reaches
`[NSApp nextEventMatchingMask:... inMode:NSDefaultRunLoopMode ...]`. Since the run
loop is currently in `NSEventTrackingRunLoopMode`, this returns nil immediately (no
events in default mode). `SDL_PollEvent` returns 0. No events are processed, no harm
done. However, this is relying on implementation details of both SDL and Cocoa.

A safer approach is to **skip `frame_event()`** entirely during menu tracking. The user
is interacting with the menu — there are no meaningful SDL input events to process.

### 4. Frame Sleep Would Block the Timer Callback

Each execution mode ends with a sleep to pace the emulation to real time:
- EXEC_STEP_INTO: `SDL_DelayPrecise(wakeup_time - now)`
- EXEC_NORMAL: `frame_sleep()` (busy-wait + optional SDL_DelayPrecise)
- Ludicrous: no sleep (but no pacing either)

If the timer callback calls a function that includes the sleep, it blocks the timer
for ~16ms, and the timer fires every ~16ms, so we'd get ~30fps at best. The sleep must
be **skipped** when called from the timer, since the timer already provides the pacing.

### 5. Audio Continuity

SDL3's `SDL_AudioStream` uses an internal device thread that pulls data from queued
streams. Audio output continues as long as data is in the stream buffers. When frames
stop being processed, no new audio data is generated, and the buffer drains — resulting
in silence or glitches after a fraction of a second.

Keeping the emulation running means audio frames keep being generated and queued,
so audio remains uninterrupted.

## Proposed Approach: Extract `run_one_frame()`

Refactor `run_cpus()` so that the body of the while loop is a callable function.

### Step 1: Move Local State to `computer_t`

Add to `computer_t`:

```cpp
uint64_t last_cycle_time = 0;       // wall-clock time of frame start
uint64_t last_start_frame_c14m = 0; // 14MHz cycle count at frame start
```

`frame_count` is already there. The cached `speaker_state` and `ds` pointers can be
re-derived inside `run_one_frame()`, or cached in `computer_t` as well.

### Step 2: Create `run_one_frame()`

```cpp
enum frame_context_t {
    FRAME_NORMAL,          // called from main while loop
    FRAME_MENU_TRACKING    // called from NSTimer during menu tracking
};

void run_one_frame(computer_t *computer, frame_context_t context);
```

The function contains the current while-loop body, with these modifications:

- **Event processing**: Skip `frame_event()` when `context == FRAME_MENU_TRACKING`.
  (Or make it safe to call — see reentrancy discussion above.)
- **Frame sleep**: Skip the sleep/busy-wait when `context == FRAME_MENU_TRACKING`.
  The timer already paces at ~60fps.
- **Timing**: When entering/exiting menu tracking, `last_cycle_time` needs to be
  re-synced to avoid a large time delta that would cause the emulation to think
  it slipped many frames. This can be done by setting `last_cycle_time = SDL_GetTicksNS()`
  in the `NSMenuDidBeginTrackingNotification` and `NSMenuDidEndTrackingNotification`
  handlers.

### Step 3: Restructure `run_cpus()`

```cpp
void run_cpus(computer_t *computer) {
    // initialization...
    computer->last_cycle_time = SDL_GetTicksNS();
    // ...
    while (cpu->halt != HLT_USER) {
        run_one_frame(computer, FRAME_NORMAL);
    }
    // cleanup...
}
```

### Step 4: Wire Up the NSTimer Callback

```cpp
void menu_tracking_frame(void *ctx) {
    computer_t *computer = (computer_t *)ctx;
    if (computer->cpu->halt != HLT_USER) {
        run_one_frame(computer, FRAME_MENU_TRACKING);
    }
}
```

Register it after `initMenu()`:

```cpp
setMenuTrackingCallback(menu_tracking_frame, computer);
```

### Step 5: Re-sync Timing on Menu Open/Close

In `menu.mm`, when `NSMenuDidBeginTrackingNotification` fires (before starting the
timer), and when `NSMenuDidEndTrackingNotification` fires (after stopping the timer),
call a timing-resync function. This could be a second callback, or the
`MenuTrackingCallback` could accept a "begin/end" flag, or `run_one_frame` could
handle it by checking wall-clock deltas.

The simplest approach: at the start of `run_one_frame(FRAME_MENU_TRACKING)`, reset
`last_cycle_time = SDL_GetTicksNS()` on the first call (or unconditionally — the timer
already controls pacing, so the sleep is skipped anyway).

## Alternative Approach: Emulation on a Background Thread

Move the CPU emulation to a dedicated thread. The main thread handles only SDL events
and display presentation. The emulation never blocks on the main thread, so menu
tracking has no effect on it.

### Architecture

```
Main Thread:                 Emulation Thread:
  SDL_PollEvent()              while (!halt) {
  [NSApp sendEvent:]             execute CPU cycles
  menu tracking                  generate audio frames
  display present                dispatch device frames
                                 signal main thread to present
                               }
```

### Advantages
- Clean separation: UI never blocks emulation
- Also solves the window-resize-drag freeze
- Also solves any future modal dialog freezes

### Disadvantages
- **SDL Renderer is not thread-safe.** On macOS, the renderer (Metal/OpenGL) must be
  used from the main thread (or at minimum, from a single consistent thread). The
  emulation thread cannot call `SDL_RenderPresent()` directly. Options:
  - Render on the emu thread to an `SDL_Texture`, signal the main thread to present it.
    Requires the main thread to have its own run-loop-based presentation mechanism.
  - Use a CVDisplayLink for presentation (fires on its own thread at vsync rate).
  - Double-buffer: emu thread writes pixel data to a shared buffer, main thread uploads
    to texture and presents.
- **Shared state synchronization**: `computer_t` is accessed by both threads.
  The debug window, OSD, event dispatchers, and mounts all touch shared state. Needs
  mutexes or lock-free structures.
- **Event dispatch**: `frame_event()` processes SDL events that affect emulation state
  (keyboard input, mouse, etc.). These would need to be forwarded from the main thread
  to the emulation thread via a thread-safe queue.
- **Debug window**: The debugger UI is tightly coupled to the emulation. Breakpoints
  modify `execution_mode` and `instructions_left` which the emulation thread reads.
  Stepping becomes a cross-thread coordination problem.
- **Significantly more complex** than the single-threaded refactor.

## Recommendation

The **`run_one_frame()` extraction** (single-threaded approach) is strongly recommended
as the first step. It requires moderate refactoring but introduces no threading
complexity, and the changes are mechanical:

1. Move 2 local variables to `computer_t` (~5 lines)
2. Extract the while-loop body to a function (~200 lines moved, ~15 lines of
   conditional logic added for `FRAME_MENU_TRACKING`)
3. Skip `frame_event()` and `frame_sleep()` when timer-driven (~4 lines of guards)
4. Re-sync timing on menu open/close (~2 lines)

The threading approach could be pursued later as a larger architectural improvement,
but it solves a broader set of problems (resize-drag, modal dialogs, future UI features)
at the cost of significant complexity.

## Window Resize Drag

Worth noting: macOS also runs a modal tracking loop during window resize/move
operations. The `NSTimer` approach handles this too, since the timer fires in
`NSRunLoopCommonModes` which includes all modal tracking modes. The
`setMenuTrackingCallback` infrastructure could be augmented to also observe
`NSWindowWillStartLiveResizeNotification` / `NSWindowDidEndLiveResizeNotification`
for that case. (Or just keep the timer always running in common modes and skip the
notification dance entirely — the overhead of a timer firing 60x/sec and immediately
returning when there's nothing special to do is negligible.)

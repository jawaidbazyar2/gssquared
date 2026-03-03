# Restructuring gs2.cpp to the SDL3 App Callbacks Paradigm

## Problem Statement

GSSquared currently uses a traditional `main()` with nested `while` loops for its
run loop. On macOS (and potentially other platforms), this architecture causes the
emulation to **freeze** during:

- **Window resize** — the OS enters a modal run loop for the resize drag, blocking
  our `while` loop from executing.
- **Menu pulldown** — the Cocoa menu tracking enters its own modal run loop,
  again blocking our main thread.

The root cause is the same in both cases: when the OS takes over the main thread's
run loop, our `while`-based emulation loop cannot run. No CPU cycles execute, no
audio frames are produced, and no video frames are rendered.

SDL3 offers an **App Callbacks** paradigm (`SDL_AppInit`, `SDL_AppEvent`,
`SDL_AppIterate`, `SDL_AppQuit`) that is designed to solve exactly this class of
problem. Instead of the application owning the main loop, SDL owns it and calls
back into the application at appropriate times. On macOS, SDL can integrate with
the native run loop so that iteration callbacks continue to fire even during
modal operations like resize and menu tracking.

The `apps/adbtest` project already uses this paradigm successfully, including a
macOS `MenuTrackingHelper` (`apps/adbtest/menu.mm`) that fires `SDL_AppIterate`
via an `NSTimer` during menu tracking.

---

## Current Architecture

### Entry point: `main()` in `src/gs2.cpp`

The current flow is:

```
main()
├── Parse args, initialize paths
├── while (1)                           // "outer loop" — system-select loop
│   ├── Create computer_t, video_system_t
│   ├── initMenu(window)
│   ├── SelectSystem::select()          // BLOCKING inner while loop
│   │   └── while (!selected)
│   │       ├── SDL_PollEvent()
│   │       ├── render()
│   │       └── SDL_Delay(16)
│   ├── Configure platform, MMU, CPU, slots, devices
│   ├── run_cpus(computer)              // BLOCKING — main emulation loop
│   │   └── while (cpu->halt != HLT_USER)   // one iteration = one frame
│   │       ├── Execute CPU cycles for ~1/60th second
│   │       ├── frame_event()           // SDL_PollEvent loop
│   │       ├── frame_appevent()        // internal event queue
│   │       ├── device_frame_dispatcher->dispatch()
│   │       ├── frame_video_update()    // render + present
│   │       └── frame_sleep()           // busy-wait to frame boundary
│   ├── Cleanup (delete osd, computer, mmu, etc.)
│   └── Loop back to system select
└── SDL_Quit()
```

### Key functions called per-frame

| Function | Purpose |
|---|---|
| `frame_event()` | Polls all SDL events, dispatches to sys_event, debug_window, OSD, then general dispatch |
| `frame_appevent()` | Processes internal `EventQueue` (sound effects, refocus, modals, messages) |
| `frame_video_update()` | Calls `video_system->update_display()`, `osd->render()`, `debug_window->render()`, `present()` |
| `frame_sleep()` | Busy-waits (optionally with `SDL_DelayPrecise`) until the frame boundary |

### Execution modes within `run_cpus()`

The frame loop has three paths:

1. **`EXEC_STEP_INTO`** — Single-step debugger mode. Runs only the requested
   number of instructions, then does a full-frame render at ~60fps paced by
   `SDL_DelayPrecise`.

2. **`EXEC_NORMAL` + clocked** — Normal emulation. Runs CPU cycles until
   `c14m >= frame_end_c14M` (one video frame's worth), then does event/audio/video
   processing and sleeps to the frame boundary. Two sub-paths: with debug window
   open (checks breakpoints) and without (fast path).

3. **Free-run ("Ludicrous Speed")** — Runs as many CPU cycles as possible in
   real-time ~1/60s, skips audio, does event/video processing, no sleep.

---

## Proposed Architecture: SDL3 App Callbacks

### Overview

Replace `main()` + `run_cpus()` with the four SDL3 callbacks:

```c
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
SDL_AppResult SDL_AppIterate(void *appstate);
void          SDL_AppQuit(void *appstate, SDL_AppResult result);
```

### App State Structure

All state currently spread across local variables in `main()` and `run_cpus()`
would be consolidated into a single struct:

```cpp
enum AppPhase {
    PHASE_SYSTEM_SELECT,
    PHASE_EMULATION,
    PHASE_SHUTTING_DOWN,
};

struct GS2AppState {
    // Phase management
    AppPhase phase = PHASE_SYSTEM_SELECT;

    // Initialization state (parsed once in AppInit)
    int platform_id;
    std::vector<disk_mount_t> disks_to_mount;

    // System selection
    SelectSystem *select_system = nullptr;
    AssetAtlas_t *aa = nullptr;

    // Emulation state
    computer_t *computer = nullptr;
    OSD *osd = nullptr;

    // Frame timing (moved from run_cpus locals)
    uint64_t last_cycle_time = 0;
    uint64_t frame_count = 0;
    uint64_t last_start_frame_c14m = 0;

    // Cached module pointers (avoid per-frame lookups)
    speaker_state_t *speaker_state = nullptr;
    display_state_t *ds = nullptr;
};
```

### SDL_AppInit

Handles everything currently before the `while(1)` outer loop:

- Parse command-line arguments
- Initialize paths
- Set SDL app metadata
- Create the initial `computer_t` (for its `video_system_t` / window)
- Create `AssetAtlas_t` and `SelectSystem`
- Set phase to `PHASE_SYSTEM_SELECT`
- Store state in `*appstate`

```cpp
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    auto *state = new GS2AppState();

    // ... parse args, init paths, set metadata (same as current main()) ...

    state->computer = new computer_t(nullptr);
    initMenu(state->computer->video_system->window);

    state->aa = new AssetAtlas_t(...);
    state->select_system = new SelectSystem(state->computer->video_system, state->aa);
    state->phase = PHASE_SYSTEM_SELECT;

    // Register macOS menu tracking callback
    setMenuTrackingCallback(SDL_AppIterate, state);

    *appstate = state;
    return SDL_APP_CONTINUE;
}
```

### SDL_AppEvent

Replaces `frame_event()` and the `SDL_PollEvent` loops in `SelectSystem::select()`.
SDL calls this once per event — we do **not** call `SDL_PollEvent` ourselves.

```cpp
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    auto *state = (GS2AppState *)appstate;

    if (state->phase == PHASE_SYSTEM_SELECT) {
        if (state->select_system->event(*event)) {
            // System was selected (or quit). Transition handled in AppIterate.
        }
        if (event->type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS;  // clean exit
        }
        return SDL_APP_CONTINUE;
    }

    if (state->phase == PHASE_EMULATION) {
        computer_t *computer = state->computer;
        cpu_state *cpu = computer->cpu;

        // Same dispatch chain as current frame_event(), minus the SDL_PollEvent loop
        if (computer->sys_event->dispatch(*event)) return SDL_APP_CONTINUE;
        if (computer->debug_window->handle_event(*event)) return SDL_APP_CONTINUE;
        if (!state->osd->event(*event)) {
            computer->dispatch->dispatch(*event);
        }

        if (event->type == SDL_EVENT_QUIT) {
            // Signal the emulation to stop
            cpu->halt = HLT_USER;
        }
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}
```

### SDL_AppIterate

This is the heart of the restructuring. It replaces both `SelectSystem::select()`'s
while loop and `run_cpus()`'s while loop. Each call performs **one frame** of work.

```cpp
SDL_AppResult SDL_AppIterate(void *appstate) {
    auto *state = (GS2AppState *)appstate;

    if (state->phase == PHASE_SYSTEM_SELECT) {
        return iterate_system_select(state);
    }

    if (state->phase == PHASE_EMULATION) {
        return iterate_emulation(state);
    }

    if (state->phase == PHASE_SHUTTING_DOWN) {
        return iterate_shutdown(state);
    }

    return SDL_APP_CONTINUE;
}
```

#### `iterate_system_select()`

```cpp
SDL_AppResult iterate_system_select(GS2AppState *state) {
    // Events already dispatched by SDL_AppEvent

    // Render the selection UI
    SDL_SetRenderDrawColor(state->computer->video_system->renderer, 0, 0, 0, 255);
    state->computer->video_system->clear();
    if (state->select_system->update()) {
        state->select_system->render();
        state->computer->video_system->present();
    }

    // Check if a system was selected
    int system_id = state->select_system->get_selected_system();
    if (system_id == -1) {
        return SDL_APP_CONTINUE;  // still waiting
    }

    // Transition: configure the selected system and enter emulation
    transition_to_emulation(state, system_id);
    return SDL_APP_CONTINUE;
}
```

#### `iterate_emulation()`

This is the most complex part. It is a direct translation of one iteration of
the `while (cpu->halt != HLT_USER)` loop in `run_cpus()`.

```cpp
SDL_AppResult iterate_emulation(GS2AppState *state) {
    computer_t *computer = state->computer;
    cpu_state *cpu = computer->cpu;
    NClock *clock = computer->clock;

    if (cpu->halt == HLT_USER) {
        // User requested stop. Transition to shutdown/system-select.
        transition_to_shutdown(state);
        return SDL_APP_CONTINUE;
    }

    // --- Speed shift handling (same as current) ---
    handle_speed_shift(state);

    // --- Execute one frame of CPU cycles ---
    // (Same three-mode logic: EXEC_STEP_INTO, EXEC_NORMAL+clocked, free-run)
    execute_frame_cycles(state);

    // --- OSD update (was in frame_event) ---
    state->osd->update();

    // --- Process internal event queue ---
    frame_appevent(computer, cpu);

    // --- Device frame dispatch ---
    computer->device_frame_dispatcher->dispatch();

    // --- Video frame output ---
    frame_video_update(computer, cpu,
        computer->execution_mode == EXEC_STEP_INTO);

    // --- Frame timing, stats, sleep ---
    handle_frame_timing(state);

    return SDL_APP_CONTINUE;
}
```

#### `iterate_shutdown()`

Handles cleanup of the current emulation session. Optionally loops back to
system select for the "play again" flow.

```cpp
SDL_AppResult iterate_shutdown(GS2AppState *state) {
    // Save trace buffer
    std::string tracepath;
    Paths::calc_docs(tracepath, "trace.bin");
    state->computer->cpu->trace_buffer->save_to_file(tracepath);

    // Cleanup
    delete state->osd;  state->osd = nullptr;
    delete state->computer;  state->computer = nullptr;
    // ... delete MMU, etc. ...

    // Go back to system select (or return SDL_APP_SUCCESS to quit)
    state->computer = new computer_t(nullptr);
    initMenu(state->computer->video_system->window);
    state->select_system = new SelectSystem(state->computer->video_system, state->aa);
    state->phase = PHASE_SYSTEM_SELECT;

    return SDL_APP_CONTINUE;
}
```

### SDL_AppQuit

Final cleanup:

```cpp
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    auto *state = (GS2AppState *)appstate;
    // Any remaining cleanup...
    delete state->osd;
    delete state->computer;
    delete state->aa;
    delete state;
    // SDL_Quit() is called automatically by SDL after this returns
}
```

---

## Key Challenges and Solutions

### 1. The CPU Execution Inner Loop

**Challenge:** In the current design, `run_cpus()` runs a tight CPU execution
loop for an entire frame (~16,667 CPU cycles at 1 MHz) before returning. In the
callbacks model, `SDL_AppIterate` is called once per frame by SDL, so we still
need to execute all those cycles within one call.

**Solution:** This is actually fine. `SDL_AppIterate` is allowed to take a
reasonable amount of time — it just can't block indefinitely. Running ~16,667
cycles of CPU emulation plus audio/video rendering is well within a frame budget.
The tight inner CPU loop (`while (clock->get_c14m() < clock->get_frame_end_c14M())`)
remains inside `SDL_AppIterate`; it just isn't wrapped in an outer `while` loop
anymore.

**Key insight:** The CPU inner loop executes for ~1/60th of a second and returns.
That's exactly what `SDL_AppIterate` wants — one frame's work, then return.

### 2. Frame Sleep / Timing

**Challenge:** Currently, `frame_sleep()` busy-waits until the frame boundary.
In the callbacks model, SDL controls when `SDL_AppIterate` is called and may
align it to vsync or display refresh rate.

**Solution:** Two options:

- **Option A (Recommended):** Keep `frame_sleep()` as-is inside `SDL_AppIterate`.
  SDL's callback loop will simply call us again immediately after we return,
  but since we've already slept to the frame boundary, the timing works out.
  The sleep ensures we don't run faster than the emulated clock.

- **Option B:** Remove `frame_sleep()` and let SDL pace the callbacks. This
  could work if SDL paces to ~60 Hz (vsync), but it removes our precise
  sub-microsecond frame timing. Not recommended for cycle-accurate emulation.

### 3. Event Handling Changes

**Challenge:** Currently `frame_event()` calls `SDL_PollEvent()` in a loop
to drain the event queue. With App Callbacks, `SDL_AppEvent` is called
individually per event by SDL — we must not call `SDL_PollEvent` ourselves.

**Solution:** Move the event dispatch logic from `frame_event()` into
`SDL_AppEvent()`, processing one event at a time. Remove all `SDL_PollEvent`
calls. The `osd->update()` call, which doesn't depend on SDL events, moves
into `SDL_AppIterate`.

### 4. The System-Select / Emulation / Restart Cycle

**Challenge:** The current code has an outer `while(1)` loop that allows
returning from emulation to system select. This becomes a state machine.

**Solution:** Introduce an `AppPhase` enum (`PHASE_SYSTEM_SELECT`,
`PHASE_EMULATION`, `PHASE_SHUTTING_DOWN`). `SDL_AppIterate` dispatches to
the appropriate sub-handler. Transition functions handle setup/teardown
between phases. `SelectSystem::select()` no longer has its own `while` loop;
instead, each `SDL_AppIterate` call renders one frame and checks
`get_selected_system()`.

### 5. Menu Tracking on macOS

**Challenge:** When a macOS menu is pulled down, the Cocoa run loop enters
a modal tracking mode that blocks SDL's normal event pump.

**Solution:** This is already solved in `apps/adbtest/menu.mm` with
`MenuTrackingHelper`. The helper registers for `NSMenuDidBeginTrackingNotification`
and fires an `NSTimer` at 60 Hz that calls `SDL_AppIterate` directly during menu
tracking. The same pattern applies directly to the main app. The existing
`src/platform-specific/macos/menu.mm` needs to be extended with the
`MenuTrackingHelper` class and the `setMenuTrackingCallback` function.

### 6. Window Resize Freezing

**Challenge:** During a window resize drag on macOS, the OS enters a
modal event loop that blocks our code.

**Solution:** There are two complementary approaches:

- **Resize timer approach (like menu tracking):** Register for
  `NSWindowWillStartLiveResizeNotification` and
  `NSWindowDidEndLiveResizeNotification`, and use the same NSTimer pattern
  to call `SDL_AppIterate` during the resize.

- **SDL3 native support:** With the callbacks paradigm, SDL3's macOS backend
  can potentially integrate resize handling more cleanly. The callback model
  means SDL's own run loop integration can fire `SDL_AppIterate` during
  the resize. However, this may depend on the SDL3 version and platform
  backend details — the NSTimer approach is the reliable fallback.

### 7. Ludicrous Speed Mode

**Challenge:** In free-run mode, the current code runs CPU cycles in a
tight loop for one real-time frame (~16.6ms), without sleeping. This
doesn't change conceptually.

**Solution:** The free-run path in `execute_frame_cycles()` works the same
way — it runs cycles until `SDL_GetTicksNS() >= next_frame_time`, does the
frame update, and returns. No architectural change needed beyond moving
it out of the while loop.

### 8. Debug Step Mode

**Challenge:** In step mode, the emulator runs only the requested number of
instructions, then waits at ~60fps for user input in the debugger window.

**Solution:** In the callbacks model, step mode simply executes the requested
instructions (if any), renders the debugger state, and returns. The 60fps
pacing comes from SDL's own callback timing (or from `frame_sleep` if
retained). The `SDL_Delay(16)` at the end of the step-mode path can remain
or be handled by SDL's pacing.

---

## Migration Plan

### Phase 1: Extract State from Locals

Before touching the callback structure, refactor `run_cpus()` to store
all its local variables (like `last_cycle_time`, `frame_count`,
`last_start_frame_c14m`, `speaker_state`, `ds`) in the `computer_t` struct
or a new `EmulationState` struct. This makes each frame iteration stateless
with respect to local variables — a prerequisite for the callback model.

### Phase 2: Extract Single-Frame Functions

Refactor `run_cpus()` so the body of its `while` loop is a standalone function:

```cpp
bool run_one_frame(computer_t *computer, EmulationState *es);
```

At this point, `run_cpus()` becomes:

```cpp
void run_cpus(computer_t *computer) {
    EmulationState es;
    es.initialize(computer);
    while (computer->cpu->halt != HLT_USER) {
        run_one_frame(computer, &es);
    }
}
```

Verify the emulator works identically after this refactor.

### Phase 3: Extract Event Handling

Refactor `frame_event()` so it no longer calls `SDL_PollEvent` but instead
processes a single event:

```cpp
void handle_single_event(computer_t *computer, cpu_state *cpu, OSD *osd, SDL_Event &event);
```

Create a compatibility shim so both the old `SDL_PollEvent` path and the
new `SDL_AppEvent` path can use the same logic.

### Phase 4: Refactor SelectSystem

Remove the blocking `while` loop from `SelectSystem::select()`. Instead,
make selection a per-frame check: call `event()` from `SDL_AppEvent`, call
`update()`/`render()` from `SDL_AppIterate`, and poll `get_selected_system()`
to detect when a selection is made.

### Phase 5: Implement the Callback Entry Points

1. Replace `#include <SDL3/SDL_main.h>` with:
   ```cpp
   #define SDL_MAIN_USE_CALLBACKS
   #include <SDL3/SDL_main.h>
   ```

2. Remove `main()`.

3. Implement `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate`, `SDL_AppQuit`
   using the state machine and extracted functions from phases 1-4.

4. Remove `run_cpus()`.

### Phase 6: macOS Menu and Resize Fixes

1. Extend `src/platform-specific/macos/menu.mm` with the `MenuTrackingHelper`
   class from `apps/adbtest/menu.mm`.

2. Add `setMenuTrackingCallback(SDL_AppIterate, appstate)` in `SDL_AppInit`.

3. Add resize tracking notifications using the same NSTimer pattern.

4. Test that emulation continues during menu pulldown and window resize.

### Phase 7: Cleanup

- Remove any remaining `SDL_PollEvent` calls
- Remove the outer `while(1)` restart loop
- Ensure all cleanup paths go through `SDL_AppQuit`
- Verify that the "return to system select" flow works via phase transitions

---

## Impact Assessment

### Files Requiring Changes

| File | Change |
|---|---|
| `src/gs2.cpp` | Major rewrite: remove `main()`, `run_cpus()`, add callback functions |
| `src/gs2.hpp` | Add `GS2AppState` definition (or new header) |
| `src/ui/SelectSystem.cpp/.hpp` | Remove blocking `select()` loop; make event-driven |
| `src/platform-specific/macos/menu.mm` | Add `MenuTrackingHelper`, `setMenuTrackingCallback` |
| `src/platform-specific/menu.h` | Add `setMenuTrackingCallback` declaration |
| `src/videosystem.cpp` | Minor: remove `SDL_Init` (SDL3 callbacks handle this) |
| `src/computer.hpp` | Possibly move frame-timing state here |

### Files Unchanged

Most of the codebase is unaffected. The CPU emulation, device emulation, display
rendering, audio system, MMU, slot system, OSD, and debug window all continue
to work exactly as before. They are called from the same functions — those
functions are just invoked from `SDL_AppIterate` instead of from within a
`while` loop.

### Risk Assessment

- **Low risk:** The CPU execution, device frames, and rendering logic remain
  identical. Only the outermost control flow changes.
- **Medium risk:** Frame timing could drift if SDL's callback pacing interacts
  unexpectedly with our `frame_sleep()`. Testing on both normal and Ludicrous
  Speed modes is important.
- **Medium risk:** The macOS resize/menu NSTimer callbacks need careful testing
  to ensure they fire at the right rate and don't cause re-entrancy issues.
- **Low risk:** The state machine (select → emulate → shutdown → select) is
  straightforward and already implicit in the current code.

---

## Relationship to Existing Code

The `apps/adbtest` project serves as a working proof-of-concept. It already:

- Uses `SDL_MAIN_USE_CALLBACKS` with all four callback functions
- Has the `MenuTrackingHelper` NSTimer pattern for menu tracking
- Demonstrates the `appstate` pattern for carrying state across callbacks

The main app's migration is essentially scaling this pattern to the full
complexity of the emulator, with the addition of multiple app phases
(system select vs. emulation) and the much more complex per-frame work.

---

## Summary

The restructuring is feasible and well-aligned with the existing code's
natural frame-based structure. The `run_cpus()` function already does
"one frame per iteration" — the `while` loop is the only thing that needs
to be removed, with its body becoming `SDL_AppIterate`. The hardest part
is not the emulation logic itself but the lifecycle management: converting
the outer restart loop into a state machine, and ensuring the macOS
platform-specific code properly fires callbacks during modal operations.

The expected payoff is significant: eliminating UI freezes during resize
and menu interaction, better platform compatibility going forward, and
a cleaner separation of concerns between SDL's event loop and the
application's frame logic.

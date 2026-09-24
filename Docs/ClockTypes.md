# Clock rails

Spec for binding each event queue to the counter it is processed against, so a deadline cannot be stamped in one domain and fired in another.

Related: [Clock.md](Clock.md) (how NClock increment and modes evolved), [AppCallbacks.md](AppCallbacks.md) (run loop), [IWM.md](IWM.md) / [SCC8530_Serial.md](SCC8530_Serial.md) (prior domain mix-ups).

This document is the design. Do not treat it as already implemented.

## Problem

`EventTimer` is a min-heap of `uint64_t` deadlines. It does not own a “now.” NClock owns three counters that increment together but are not interchangeable:

| Counter | Getter | Typical rate | Meaning |
|---|---|---|---|
| CPU | `get_cycles()` | 1.02 / 2.8 / 7 / 14 MHz, or ludicrous N×14M | Instruction retirement |
| Video / PH0 | `get_vid_cycles()` | ~1.023 MHz always | Video scanner, 1 MHz bus |
| 14M | `get_c14m()` | ~14.318 MHz always | Master crystal, wall-clock-ish |

`computer_t` holds three heaps (`event_timer`, `vid_event_timer`, `cpu_event_timer`), all constructed with the same `NClockII*`. `gs2.cpp` must pair each heap with the matching getter when polling. Devices hold `NClock*` and `EventTimer*` independently and compute `clock->get_<domain>() + delay` themselves.

Three places have to agree: which heap, which getter at schedule, which getter at process. They do not always agree. `EventTimer::scheduleEvent` then compares every deadline to `clock->get_cycles()`, which is only valid for the CPU heap. After a speed-up or ludicrous stretch, CPU time can lead 14M or video, and a correctly stamped 14M/video event is dropped as “in the past.”

That is a structural bug, not a missing `assert` at call sites. A call-site assert still names “now” separately from the queue.

## Goals

- Domain is a type. A VIA cannot be handed a 14M rail.
- `now` and the heap travel together. `schedule_after(delay)` cannot mix units.
- Existing tick getters stay one load from a constant offset (`[reg + constexpr]`). No extra pointer chase, no virtual `now()`.
- Inner-loop poll becomes one call. Still between instructions, not inside `incr_cycles()`.
- `computer_t` stops owning three `EventTimer*`.
- Debug builds assert a past deadline; release queues it so the next poll fires it. Never skip.

## Non-goals

- One 14M heap for everything. “After N CPU cycles” is not a fixed 14M delta on a IIgs (slow access, ROM, refresh, stretch). Three schedulers remain.
- Firing events from `incr_cycles()`. Callbacks re-enter devices; the increment path stays a counter update.
- Event-horizon execution (“run until next event”) in this refactor. Keep `next_event()` public so that can be added later.
- A fourth Ensoniq rail. DOC timing stays on 14M / its own catch-up.
- Changing baud math, VIA periods, or mouse VBL formulas except to stamp them on the correct rail.

## Current state

Construction (`computer.cpp`):

```text
event_timer     = new EventTimer(clock);  // processed vs get_c14m()
vid_event_timer = new EventTimer(clock);  // processed vs get_vid_cycles()
cpu_event_timer = new EventTimer(clock);  // processed vs get_cycles()
```

Poll (`gs2.cpp`, five copies: step-into, ludicrous probe, breakpoint loop, 1 MHz / 2.8 / 14 M loops):

```text
if (event_timer->isEventPassed(clock->get_c14m()))
    event_timer->processEvents(clock->get_c14m());
if (vid_event_timer->isEventPassed(clock->get_vid_cycles()))
    vid_event_timer->processEvents(clock->get_vid_cycles());
if (cpu_event_timer->isEventPassed(clock->get_cycles()))
    cpu_event_timer->processEvents(clock->get_cycles());
```

NClock increment (II vs IIgs) already couples the three counters. Frame window (`frame_start_c14M` / `frame_end_c14M`) is 14M. Video-scanner kicks and per-video-cycle handlers are increment side effects, not EventTimer work.

`EventTimer` stores `NClockII* clock` only for the past-check. `processEvents(currentCycles)` already takes “now” from the caller.

Incidental rot this refactor should swallow:

- `computer_t` destructor deletes `event_timer` only; `vid_event_timer` and `cpu_event_timer` leak.
- `set_clock()` updates `event_timer` only.
- IWM constructs both 5.25 and 3.5 drives with `computer->event_timer` (14M heap). `Floppy525_woz` stamps `get_cycles() + 520`. Slot Disk II uses `cpu_event_timer` correctly. IWM 5.25 settle is the same class of units bug as the past-check.

## Hierarchy

Rails are not subclasses of NClock. NClock **owns** three typed rails and is the only thing that advances their counters. II vs IIgs stay increment strategies.

```text
NClock                          // coupling engine
 ├── CpuRail  cpu               // CPU-cycle counter + heap
 ├── VidRail  vid               // PH0 / video-cycle counter + heap
 └── C14mRail c14m              // 14M counter + heap

NClockII   : NClock             // IIe increment
NClockIIgs : NClockII           // variable 14M/cycle, refresh, slow sync
NClockFactory                   // unchanged
```

```text
ClockRail                       // one body; no virtuals; not a clock
 ├── now_                       // uint64_t, first field
 ├── EventTimer events_         // heap only; no NClock*
 ├── now() / schedule() / schedule_after() / cancel()
 ├── due() / next_event() / process_due()
 └── add() / tick()             // private; NClock* are friends

CpuRail  : ClockRail            // empty; type identity only
VidRail  : ClockRail
C14mRail : ClockRail
```

Prefer empty subclasses over `ClockRail<ClockDomain::D>`. One method body is kinder to I-cache; `VidRail&` still will not accept a `C14mRail`; no vptr if `ClockRail` has no virtuals. A template would emit three copies of every non-inlined wrapper for no gain unless a rail later needs per-domain code (it does not).

`now()` returns `uint64_t` until the late tightening pass (strong `CpuCycles` / `VidCycles` / `C14mTicks` structs). Do not use `typedef uint64_t CpuCycles_t` — that is an alias, not a type.

### What lives where

| On NClock (coupling) | On each rail |
|---|---|
| US/PAL / ludicrous mode tables | `now_` |
| `incr_cycles()` / `slow_incr_cycles()` | event heap |
| Video scanner kick, `cycle_handlers` | `next_event` |
| Frame start/end in 14M | `schedule` / `cancel` / `process_due` |
| Rate helpers (`get_c14m_per_second()`, …) | debug assert vs `now_` |
| Existing getters as inline wrappers | |

`cycle_handlers` (every video tick) stay on NClock. They are not scheduled events.

### Friendship and mutation

`now_` is read-only to devices. `add(n)` / `tick()` are private. Friends: `NClock`, `NClockII`, `NClockIIgs`. Devices cannot advance time by holding a rail.

## Types do not add a virtual layer

NClock already has virtuals (`slow_incr_cycles`, `debug`). That vptr is unchanged and is not on the `now()` path.

A rail must satisfy all of:

1. No virtual functions, so no vptr on `ClockRail`.
2. Stored **by value** on NClock (`CpuRail cpu;`), not `CpuRail*`.
3. `now()` is `inline` in the header and returns `now_` (first member).
4. NClock wrappers stay inline in the header:

```cpp
inline uint64_t get_cycles()     { return cpu.now(); }
inline uint64_t get_vid_cycles() { return vid.now(); }
inline uint64_t get_c14m()       { return c14m.now(); }
```

Expected codegen, same as today:

```text
clock->get_cycles()
clock->cpu.now()
    → mov rax, [rdi + offsetof(NClock, cpu) + offsetof(ClockRail, now_)]
```

One load, constant displacement. Today’s offsets are already large (mode tables sit in front of the counters). A different displacement is fine; a second load is not.

What would break the contract:

| Choice | Effect |
|---|---|
| `CpuRail* cpu` on NClock | load pointer, then `now_` |
| `uint64_t*` in the rail back to an NClock field | two loads for `rail.now()` |
| `virtual now()` or `std::function` time source | indirect call |
| `now()` only in a `.cpp` without LTO | real call |
| Device holds `ClockRail<D>*` to a heap-allocated rail | extra load; also a lifetime footgun |

Devices may hold `C14mRail*` **into** the NClock member (`&clock->c14m`). That pointer is the rail object. `c14m->now()` is `[rail + 0]`, still one load. Do not heap-allocate rails.

### Increment locality

`incr_cycles()` touches CPU, 14M, and video together. If each `now_` sits at the front of a rail that also contains `EventTimer` (`std::vector` plus `next_event`), the three counters are no longer adjacent.

Accept that for the first cut: getters stay one load; increment may touch three cache lines instead of one cluster. Do **not** “fix” it with a pointer from the rail back to a clustered counter — that makes `rail.now()` two loads, and devices will call `rail.now()`.

If increment clustering shows up in profiles, the counters can move to a hot `struct { uint64_t cpu, vid, c14m; }` at the top of NClock **only** if the rail type is a typed overlay of that field (the rail object address is a fixed displacement from its counter). A handle that points at the cluster is disallowed.

## API

Callback signature stays `void (*)(uint64_t instanceID, void* userData)` so existing static wrappers keep working.

```cpp
enum class ClockDomain { Cpu, Vid, C14m };

template<ClockDomain D>
class ClockRail {
public:
    uint64_t now() const;
    void schedule(uint64_t when,
                  void (*cb)(uint64_t, void*),
                  uint64_t instanceID,
                  void* userData = nullptr);
    void schedule_after(uint64_t delay,
                        void (*cb)(uint64_t, void*),
                        uint64_t instanceID,
                        void* userData = nullptr);
    void cancel(uint64_t instanceID);
    bool due() const;
    uint64_t next_event() const;
    void process_due();
    bool has_pending() const;
};
```

Semantics:

- `schedule(when, …)` — absolute deadline on **this** rail. Same `instanceID` replaces the existing event (current EventTimer behavior).
- `schedule_after(delay, …)` — `schedule(now() + delay, …)`. Preferred. Units cannot be wrong.
- `when == now()` is legal (fire on the next poll).
- `when < now()` is a programmer error: `assert(when >= now())` in debug; in release **queue it** so `process_due` runs it immediately. Do not skip. Do not print-and-return.
- `cancel` is the current `cancelEvents`.
- `due()` is `now() >= next_event()`. Empty heap ⇒ `next_event()` is `uint64_t` max, `due()` is false.
- `process_due()` calls the heap with `now()`. No getter argument.

`schedule_after` is the common path. Use absolute `schedule` when the deadline is already in this rail’s units (mouse VBL, VIA `t1_triggered_cycles`, RTC first-fire alignment).

Do not put the assert at every call site as the primary guard. Call-site asserts still name the wrong “now.” The rail assert is the invariant. Callers who compute a deadline far from `schedule` may assert locally for earlier failure; that is optional documentation.

### EventTimer

Keep it as the heap implementation inside the rail.

- Drop `NClockII* clock`, `set_clock`, and the past-check.
- `processEvents(uint64_t now)` stays; the rail passes `now_`.
- Public surface can remain for `apps/mbtest` until that app takes a `VidRail&`.
- Devices in `src/` do not include EventTimer after the migration.

### NClock additions

```cpp
class NClock {
public:
    CpuRail  cpu;
    VidRail  vid;
    C14mRail c14m;

    inline uint64_t get_cycles()     { return cpu.now(); }
    inline uint64_t get_vid_cycles() { return vid.now(); }
    inline uint64_t get_c14m()       { return c14m.now(); }

    inline void process_due() {
        c14m.process_due();
        vid.process_due();
        cpu.process_due();
    }
};
```

Public members so construction is `&computer->clock->c14m` with no accessor call. Increment still uses private `add`/`tick` via friendship (the public `cpu` object’s counter is not writable from outside if `now_` is private and `now()` is const). Implementation choice: public rail object, private `now_`.

Keep every existing NClock query (`get_c14m_per_second`, frame bounds, mode names, …). This refactor does not relocate rate tables onto rails.

Commented-out `event_vid` / `schedule_vid_event` in NClock.hpp go away; `vid.schedule` replaces them.

## `process_due`

Today each poll site repeats six calls and must remember the pairing. After:

```cpp
clock->process_due();
```

`NClock::process_due()` polls all three rails. Each rail’s `process_due()` is:

```cpp
inline void process_due() {
    if (due())
        events_.processEvents(now_);
}
```

`due()` is the current `isEventPassed` cheap path: compare `now_` to the cached earliest deadline, skip the heap if nothing is ready.

**Do not** call `process_due()` from `incr_cycles()` / `slow_incr_cycles()`. Call it where the six-call block lives today: once per instruction in the run loops and the step-into loop (`gs2.cpp`). Five copies collapse to five one-liners (or a single helper used by each loop).

### Poll order

Keep **14M, then video, then CPU**. Callbacks may schedule onto any rail. A CPU callback that schedules a 14M event already in the past will not run until the next instruction if 14M was already processed this poll. That matches today. Do not shuffle order without a reason.

### Re-entrancy

`processEvents` already drains the heap in a loop; a callback that `schedule`s another due event on the **same** rail can run in the same drain. A callback that `cancel`s its own `instanceID` is fine (already popped). A callback must not destroy the rail or the clock.

### Ludicrous / step-into / breakpoints

All of those loops poll events before `execute_next`. They all become `clock->process_due()`. No special rail rules for ludicrous: 14M and video still advance (sometimes more slowly relative to CPU); events on those rails still fire in their own units.

## Construction and lifetime

Clock is created first, as today. Rails are value members; they die with the clock.

```cpp
// computer.cpp — delete
event_timer = new EventTimer(clock);
vid_event_timer = new EventTimer(clock);
cpu_event_timer = new EventTimer(clock);

// computer.hpp — delete
EventTimer *event_timer;
EventTimer *vid_event_timer;
EventTimer *cpu_event_timer;
```

`set_clock(NClockII*)` after devices have taken `&old->c14m` dangles those pointers. Today `set_clock` only rebinds `event_timer`’s clock pointer and is already racy vs devices that cached `computer->clock`. Rule: the clock is created once for the `computer_t` lifetime; `set_clock` is startup-only, before device init. Prefer devices that need rates to hold `NClock*` and use `clock->c14m` rather than a long-lived `C14mRail*` if a swap is ever required.

Destructor: no EventTimer pointers to delete. Rails clean up their heaps.

Reset does **not** auto-cancel all events. Devices already cancel in their reset handlers. Optional later: `NClock::cancel_all()` on cold reset. Not required for this cut.

## Who holds what

| Client | Takes | Why |
|---|---|---|
| MOS6551, Z85C30 TX, Thunderclock, RTC 1 s, 3.5 motor-off, mouse VBL | `C14mRail&` (or `NClock*` and `clock->c14m`) | Wall-clock / baud / frame-relative 14M |
| Mockingboard / 6522 | `VidRail&` | PH0-timed VIA |
| Disk II 5.25 phase-settle | `CpuRail&` | “N instruction cycles” (until someone moves it) |
| IWM | `NClock&` or both 14M and CPU (and video if 3.5 stepping stays on vid internally) | Two domains in one device |
| Rate math (14M per second, per frame, per scanline) | `NClock&` | Coupling, not rail state |

If a device only schedules and never needs Hz, pass the rail. If it needs both, pass `NClock&` and use `clock.c14m` / `clock.vid` / `clock.cpu`. Do not pass `NClock&` plus a *different* rail.

`instanceID` uniqueness is **per rail**, as today per heap. MB IDs and SCC IDs can overlap across rails. Do not merge heaps; IDs would collide.

## Call-site examples

### 6551 TX (14M, delay known)

Today (`MOS6551.hpp`):

```cpp
uint64_t cycles = get_cycles_per_char();
if (cycles > 0 && event_timer && clock) {
    uint64_t when = clock->get_c14m() + cycles;
    event_timer->scheduleEvent(when, tx_complete_callback, timer_base_id + 0, this);
}
```

After. `get_cycles_per_char()` already returns 14M ticks; `clock` was only used for `get_c14m()`.

```cpp
// ssc.cpp
st->acia = new MOS6551(st->irq_control, &computer->clock->c14m, irq_id, timer_base);

uint64_t cycles = get_cycles_per_char();
if (cycles > 0 && c14m) {
    c14m->schedule_after(cycles, tx_complete_callback, timer_base_id + 0, this);
}
```

SCC TX is the same shape: `c14m->schedule_after(cycles_per_char, …)`.

### Mockingboard T1 (video, delay)

Today:

```cpp
mb_d->event_timer->scheduleEvent(
    mb_d->clock->get_vid_cycles() + next_counter + 1,
    mb_t1_timer_callback, instanceID, mb_d);
```

After:

```cpp
// init
mb_d->vid = &computer->clock->vid;

mb_d->vid->schedule_after(next_counter + 1, mb_t1_timer_callback, instanceID, mb_d);
```

Passing `&computer->clock->c14m` here is a type error.

### Mockingboard T1 (video, absolute)

Today already has `tc->t1_triggered_cycles` in video ticks:

```cpp
mb_d->event_timer->scheduleEvent(tc->t1_triggered_cycles, mb_t1_timer_callback, id, mb_d);
```

After:

```cpp
mb_d->vid->schedule(tc->t1_triggered_cycles, mb_t1_timer_callback, id, mb_d);
```

### Disk II 5.25 settle (CPU)

Today (standalone Disk II, correct heap):

```cpp
event_timer->scheduleEvent(clock->get_cycles() + 520, phase_change_callback, instanceID, this);
```

After:

```cpp
cpu->schedule_after(520, phase_change_callback, instanceID, this);
```

IWM must construct 5.25 drives with `clock->cpu`, not `clock->c14m`. See migration table.

### 3.5 motor-off (14M)

Today:

```cpp
event_timer->scheduleEvent(
    clock->get_c14m() + clock->get_c14m_per_second() * 0.5,
    motor_off_callback, instanceID, this);
```

After. Period still needs the rate helper on NClock:

```cpp
c14m->schedule_after(clock->get_c14m_per_second() / 2,
                     motor_off_callback, instanceID, this);
```

### Mouse / RTC (14M, absolute)

Mouse VBL is an absolute 14M deadline already computed on that domain:

```cpp
c14m->schedule(ds->vbl_cycle, mouse_vbl_interrupt, instanceID, ds);
```

RTC one-second interrupt:

```cpp
c14m->schedule_after(ds->clock->get_c14m_per_second(),
                     rtc_pram_1sec_interrupt, instanceID, ds);
```

First-fire alignment that computes an absolute 14M tick (`display.cpp` init) stays `c14m->schedule(ticks_14m, …)` if that value is in 14M units from the same epoch as `c14m.now()` (zero at clock construct). Do not convert it through `get_cycles()`.

### Thunderclock

```cpp
c14m->schedule_after(period, thunderclock_tp_tick, timer_id_tp(slot), st);
c14m->schedule_after(clock->get_c14m_per_second(), thunderclock_1hz, timer_id_1hz(slot), st);
```

### Run loop

Today: six calls, three pairings, five copies.

After, every copy:

```cpp
clock->process_due();
(cpu->cpun->execute_next)(cpu);
```

## Device migration table

| Site | Today | Rail |
|---|---|---|
| `ssc.cpp` / `MOS6551` | `event_timer` + `clock` | `C14mRail` |
| `scc8530.cpp` / `Z85C30` | `event_timer` + `clock` | `C14mRail` (keep `NClock*` if baud still reads rates) |
| `thunderclockplus.cpp` | `event_timer` | `C14mRail` + `NClock*` for period |
| `display.cpp` RTC | `computer->event_timer` | `clock->c14m` |
| `mouse.cpp` / `applemouseiii.cpp` | `event_timer` | `C14mRail` |
| `Floppy35_woz` motor-off | `event_timer` (14M heap) | `C14mRail` |
| `mb.cpp` / `mb2.cpp` / `N6522` | `vid_event_timer` | `VidRail` |
| `ndiskii_woz.cpp` / `Floppy525_woz` | `cpu_event_timer` | `CpuRail` |
| `iwm_device.cpp` / `IWM2` | one `event_timer` for both drive types | 5.25 → `CpuRail`, 3.5 → `C14mRail` |
| `apps/mbtest` | stack `EventTimer` + `NClock` | `VidRail` on a test clock, or keep EventTimer until last |

`Floppy_woz` base currently takes one `EventTimer*`. Split the constructor or pass `NClock&` and let each subclass pick `clock.cpu` vs `clock.c14m`.

## Existing getters and call sites that only read time

Most of the emulator only reads `get_cycles()` / `get_c14m()` / `get_vid_cycles()`. Those wrappers stay. No need to rewrite speaker, Ensoniq catch-up, game paddles, debugger, etc. This refactor is about **scheduling**, ownership of the heaps, and the poll site.

Debug HUD lines that print the three counters keep using the wrappers.

## Tests and apps

- `apps/mbtest` constructs `EventTimer` on the stack. After N6522 takes `VidRail&`, the test clock’s `vid` member is enough.
- `apps/cycletest` / `cputest` / `cpu816test` only increment and read `get_cycles()`. No rail work.
- Any test that called `scheduleEvent` on a bare EventTimer with a fake clock must use a rail so the past-check domain is defined.

## Future: event horizon (out of scope)

`next_event()` on each rail is the earliest deadline in that rail’s units. A later “run until next event” would need a conversion into a common unit. IIgs makes “next video event in CPU cycles” approximate. Do not build the converter now. Do not process events inside increment in the name of that future.

## Implementation sequence

Three phases. Do not mix strong tick wrappers into phase 1–2.

### Phase 1 — rails inside NClock, external API unchanged

Goal: a bootable emulator with the same getters, the same `computer_t` EventTimer pointers, and the same device call sites. Only the *location* of counters and heaps changes.

1. Add `ClockRail` + empty `CpuRail` / `VidRail` / `C14mRail`. `now_` first. Heap inside. Header-inline `now()`. Friends for `add` / `tick`.
2. Move `cycles` / `c_14M` / `video_cycles` into the three rail members. Existing getters become wrappers (`return cpu.now()` etc.). Increment writes through `cpu.add(1)` / `c14m.add(n)` / `vid.tick()`. Semantics of `get_cycles()` / `get_c14m()` / `get_vid_cycles()` stay identical.
3. Put the three heaps in the rails. `computer_t`’s `event_timer` / `vid_event_timer` / `cpu_event_timer` become pointers **into** those heaps (`&clock->c14m.events()` etc.), not separately `new`’d objects. Device `scheduleEvent` call sites do not change yet. Destructor / `set_clock` rot goes away because there is nothing extra to free or rebind.
4. Strip EventTimer’s `NClock*` and the `get_cycles()` past-check. That check is a behavior bug; removing it is part of making phase 1 “same semantics” for *correct* schedules. Do not replace it with a skip. The rail assert can wait until a device actually calls `rail.schedule`.
5. Add `NClock::process_due()`. Switch the five `gs2.cpp` blocks to it. Devices still enqueue via the EventTimer pointers. One poll path, not two.
6. Smoke-test a IIe and a IIgs at 1 MHz and at least one fast speed (serial TX, MB, Disk II, IWM 3.5 motor, mouse, RTC if GS). Confirm a Release getter still folds to one load if you care to check.

After phase 1, `clock->get_c14m()` is still `uint64_t`. A device can still stamp the wrong units into the right heap. You have not made that a type error yet; you have stopped the heap and the counter from living in different objects, and you have stopped the CPU-domain past-check from dropping 14M/video events.

### Phase 2 — migrate call sites one by one

One device (or one tight group) per change. Test that device before touching the next. `schedule` / `schedule_after` still take `uint64_t`.

Suggested order (increasing coupling):

1. Thunderclock, RTC, mouse / mouse III (14M, easy to see if VBL / 1 Hz dies)
2. MOS6551, then SCC (14M baud)
3. Floppy35 motor-off (14M; IWM still passes one timer until the split)
4. Mockingboard / N6522 (video)
5. Standalone Disk II 5.25 (CPU)
6. IWM constructor split: 5.25 → `cpu`, 3.5 → `c14m`. This is the one that changes behavior if 5.25 settle was misfiring on the 14M heap. Test seek / boot / format separately from the 3.5 motor timeout.
7. `apps/mbtest` last
8. Delete `computer_t`’s three `EventTimer*` only when nothing in `src/` takes an `EventTimer*` from computer.

No compatibility typedef from `EventTimer*` to a rail.

Manual checks for each of these steps are in [Manual regression tests](#manual-regression-tests).

### Phase 3 — strong tick wrappers

Only after every schedule site is on a rail.

Replace rail `uint64_t` with distinct structs (`CpuCycles`, `VidCycles`, `C14mTicks`), not typedefs. `now()`, `schedule`, and `schedule_after` use those types. Stored deadlines that already live next to a rail (`t1_triggered_cycles`, `vbl_cycle`) become the matching struct.

Leave `get_cycles()` / `get_c14m()` / `get_vid_cycles()` as `uint64_t` unless you want a second, larger sweep of HUD / speaker / debugger sites. Safety belongs at the schedule edge.

Heap storage stays `uint64_t` inside EventTimer; unwrap at the rail boundary.

## Manual regression tests

Perform these by hand after the named phase or phase-2 step. Do not skip the **shared smoke** after phase 1; later steps assume increment + poll still work.

**Pass** means the listed behavior happens and nothing in the shared smoke regresses. **Fail** is a hang, a silent no-op (IRQ/event never fires), an event that fires immediately (wrong domain / past-check), tempo or baud that tracks CPU speed when it should not, or a motor that never spins down.

Shipped configs worth using: `assets/gs2/IIe_ThunderClock.gs2`, `IIPlus_ThunderClock.gs2`, `IIe_AppleMouseIII.gs2`, `DualMockingboard.gs2`, `IIgs_DiskII.gs2`. Use your usual IIe Enhanced and IIgs profiles for the rest.

Speeds: **1 MHz** is `CLOCK_1_024MHZ`. **Fast** is 2.8 MHz on IIgs (default) and 2.8 or 14.3 on IIe. Change with F9 / OSD. After a speed change, reset or reboot the guest if the title is picky.

### Shared smoke (every phase-1 landing, and once after phase 3)

Do this on **IIe Enhanced** and **IIgs**.

1. Cold boot to BASIC or the usual boot disk. Reset (F10 / Control-Reset). Quit cleanly (`c.quit()` / menu; no hung Quit modal).
2. F9 through 1 MHz and one fast speed. Video keeps scanning; speaker click is sane; no `scheduleEvent: Event in the past` spam (that path is gone after phase 1).
3. Debugger step-into for a few instructions, then continue. Events must still fire in step mode (`process_due` lives in that loop too).
4. IIe: boot a 5.25 DOS 3.3 or ProDOS disk, `CATALOG`, load a small file.
5. IIgs: boot a 3.5 System / GS/OS disk far enough to see the desktop or a splash, then a 5.25 disk if you have `IIgs_DiskII.gs2`.
6. IIgs: Control Panel clock advances; ADB mouse moves the firmware cursor (this is not the slot-mouse EventTimer; it only proves increment/poll).
7. Optional: enter ludicrous, run ~10 seconds, drop back to 1 MHz or 2.8. 3.5 motor must still be able to spin down afterward; IIgs 1-second interrupt must still tick. Those were the past-check victims.

### After phase 1 (rails inside NClock, API unchanged)

Shared smoke, then every scheduled device once. Phase 1 can break *all* of them (wrong `now` in increment, poll order, or heap pointer).

| # | What | How | Pass |
|---|---|---|---|
| 1 | ThunderClock | `IIe_ThunderClock.gs2`, ProDOS boot | `DATE` / `TIME` match host wall clock (no year on the chip; ProDOS invents one). Do not hang in the clock driver. |
| 2 | IIgs RTC 1 s | IIgs, Control Panel or GS/OS | Clock advances in real seconds, not instantly and not frozen. Leave it running >2 s. |
| 3 | Slot mouse | `IIe_AppleMouseIII.gs2` or Mouse card + Shufflepuck | Moves; VBL-dependent titles get past the VBL wait. If VBL events die, firmware spins. |
| 4 | SSC | IIe, Super Serial, ProTERM (or any terminal), modem device | `AT` → `OK`. Type a few dozen characters; they appear paced, not in one blast. Repeat at 1 MHz and fast: **baud must not change** with CPU speed. |
| 5 | SCC | IIgs, same modem / terminal idea (Spectrum / ProTERM GS) | Same as SSC: `AT` → `OK`; baud independent of CPU speed. |
| 6 | 3.5 motor-off | IIgs 3.5 boot, then idle | Drive HUD / spin sound goes off about 0.5 s after the last access. Motor stuck on = event never fired. Motor dies mid-boot = fired immediately. |
| 7 | Mockingboard | `DualMockingboard.gs2` + Cybernoid music disk, Skyfox, or a Total Replay MB title | Music plays. At fast CPU, **tempo stays 1 MHz** (video rail). Too-fast music at 2.8/14 = wrong domain. |
| 8 | Disk II 5.25 | IIe slot-6 Disk II | Boot, `CATALOG`, seek (load a file that is not the first). Works at 1 MHz **and** fast (CPU-clocked drive). |
| 9 | IWM 5.25 + 3.5 | IIgs | 5.25 boot (`IIgs_DiskII.gs2`) and 3.5 boot both work. After phase 1 the IWM 5.25 units bug is **unchanged** (still CPU stamps on the 14M heap); do not treat a long-standing 5.25-at-fast IWM quirk as a new regression unless you just introduced it. |

### After each phase-2 step

Do the row for **that** step, then a 60-second shared-smoke subset: IIe boot + IIgs boot, one speed change, reset. Full shared smoke only if that subset fails.

#### 2.1 ThunderClock, RTC, mouse / mouse III

1. `IIe_ThunderClock.gs2` (and `IIPlus_ThunderClock.gs2` if you touch II+ ROM mapping). ProDOS `DATE`. Reset and read time again.
2. If you use TP IRQ / 64–2048 Hz pulse: enable IRQ as in [Thunderclock.md](Thunderclock.md); confirm IRQs keep arriving at 1 MHz and at fast (14M rail — rate must not scale with CPU).
3. IIgs: watch the Control Panel or GS/OS clock for ≥3 seconds. Toggle the 1-second interrupt if you have a C023 test; it must re-arm (callback reschedules itself).
4. `IIe_AppleMouseIII.gs2`: move, click, reset while the pointer is live, then move again (VBL event re-scheduled). Shufflepuck or another VBL+mouse title if you have it.
5. Slot Mouse II (not III) if that card is still a separate init path — same VBL check.

ADB / KeyGloo is out of scope for this step.

#### 2.2 MOS6551, then SCC

Do 6551 **before** SCC. Test 6551, then migrate SCC and test both.

1. IIe SSC + modem: `AT`, `ATI`, `ATS0?`. `AT` → `OK` within a beat, not after a long stall and not before the last character is sent (TX complete event).
2. Type a line of text at 300 and at 9600 (or whatever the terminal is set to). 300 is obviously paced; 9600 is faster but still not instant.
3. Change CPU to fast, same baud in the terminal. Character pacing must match step 2. If TX suddenly completes immediately, the event is on the wrong rail or `schedule_after` used CPU ticks.
4. Incoming: if you have a loopback or a second session, a short receive still sets RDRF / does not freeze ProTERM on “waiting for connect.”
5. Reset / `IN#2` / hang up (`ATH`). Next `AT` still works (TX timer cancelled and rescheduled).
6. Repeat 1–5 on IIgs SCC (slot-free built-in). Both channels if you have a title that uses B.

#### 2.3 Floppy35 motor-off

IWM still passes one timer into both drive types. You are only changing how 3.5 **schedules**.

1. IIgs: boot 3.5, idle. Motor off ≈ 0.5 s after last IWM access. Time it roughly; it should feel the same at 1 MHz and at 2.8 (14M delay).
2. Boot a second 3.5 disk (eject / mount). Motor comes back, then spins down again. Cancel-then-reschedule on the same `instanceID`.
3. Reset while the motor is on. Motor must not stay on forever after reset.
4. 5.25 on IWM: boot `IIgs_DiskII.gs2` once. This step must not break 5.25 (still the old heap until 2.6).

#### 2.4 Mockingboard / N6522

1. `DualMockingboard.gs2` + a known-good MB title (Cybernoid music disk, Skyfox, Ultima V, Apple Cider Spider). Title detect + music/SFX.
2. Same title at 1 MHz and at fast. **Pitch/tempo stay put** (video rail). CPU-speed music is a fail.
3. Reset mid-tune, reboot the title. Timers re-arm from the MB init path (`vid + 65536` oneshots).
4. If you have a two-card / dual-6522 title, both halves still play.
5. IIgs + MB in a slot, if you use that: music still at 1 MHz while the GS CPU is at 2.8.

#### 2.5 Standalone Disk II 5.25 (not IWM)

1. IIe / II+ slot-6 `disk_ii`. Boot DOS 3.3, `CATALOG`, `BRUN` or `LOAD` a file that seeks.
2. Repeat at fast. Must still work (CPU rail — this is the feature).
3. Two drives if configured: boot D1, catalog D2.
4. Write / format only if you have a disposable image. Recatalog after write.
5. IIgs with a **slot Disk II** (not built-in IWM) if you have that config — same boot.

Phase-settle is the 520-cycle event. Failures look like missed seeks, hang in boot, or “wrong track” more than a silent motor.

#### 2.6 IWM constructor split (5.25 → cpu, 3.5 → c14m)

This is the step that can **change** IWM 5.25 behavior. Test 5.25 and 3.5 as separate stories.

**3.5 (must stay the same)**

1. Repeat 2.3 motor-off (idle spin-down at 1 MHz and 2.8).
2. Boot GS/OS or a 3.5 game (Tomahawk / Alien Mind if you use those). Format a blank 3.5 only on a disposable image.

**5.25 (may start working better at fast if the old units bug was biting)**

1. `IIgs_DiskII.gs2` (or IIgs 5.25 in the built-in IWM). Boot, catalog, seek, at **1 MHz** first.
2. Same at **2.8** and one faster IIgs speed. Drive must keep up with the CPU (same product choice as slot Disk II).
3. Switch 5.25 ↔ 3.5 (`$C031` / DiskReg) in one session: boot 3.5, then a 5.25, then 3.5 again. No stuck motor, no stolen `instanceID`.
4. IIe IWM is not a thing; do not skip slot Disk II (2.5) when judging IIe 5.25.

A new hang at 1 MHz is a real regression. A change only at fast IWM 5.25 may be the intended units fix — compare to slot Disk II at the same speed.

#### 2.7 `apps/mbtest`

1. Build and run `mbtest` (`apps/mbtest`). It must complete without asserting or looping.
2. If it is a cycle-script vs the 6522, T1/T2 fire at the recorded video-cycle marks, not CPU marks.

#### 2.8 Delete `computer_t` EventTimer pointers

No new device logic. Shared smoke on IIe and IIgs. Cold boot, reset, quit. If anything still compiled against `computer->event_timer`, it will not link — that is the test.

### After phase 3 (strong tick wrappers)

Mostly a compile test. Then shared smoke once. Re-run only the devices you touched in the wrapper sweep (usually MB `t1_triggered_cycles`, mouse `vbl_cycle`, RTC first-fire). If `get_*` stayed `uint64_t`, speaker / paddles / Ensoniq / HUD need no extra pass.

### Suggested session log

Copy and tick.

```text
Phase 1
  [ ] IIe smoke  [ ] IIgs smoke  [ ] step-into  [ ] speed change
  [ ] ThunderClock DATE  [ ] IIgs 1 s  [ ] slot mouse VBL
  [ ] SSC AT/OK 1M + fast  [ ] SCC AT/OK 1M + fast
  [ ] 3.5 motor-off  [ ] MB tempo 1M + fast
  [ ] Disk II 5.25 1M + fast  [ ] IWM 5.25 + 3.5

Phase 2.1 Thunder / RTC / mouse
  [ ] DATE  [ ] 1 s  [ ] mouse VBL  [ ] subset smoke
Phase 2.2 6551 / SCC
  [ ] SSC AT 1M+fast  [ ] SCC AT 1M+fast  [ ] subset smoke
Phase 2.3 Floppy35 motor
  [ ] spin-down 1M+2.8  [ ] remount  [ ] reset  [ ] IWM 5.25 still boots
Phase 2.4 Mockingboard
  [ ] music  [ ] tempo vs CPU  [ ] reset  [ ] subset smoke
Phase 2.5 Disk II
  [ ] boot/seek 1M+fast  [ ] subset smoke
Phase 2.6 IWM split
  [ ] 3.5 motor + boot  [ ] 5.25 1M  [ ] 5.25 fast  [ ] 5.25↔3.5
Phase 2.7 mbtest
  [ ] harness exits clean
Phase 2.8 drop EventTimer*
  [ ] IIe + IIgs smoke

Phase 3
  [ ] build  [ ] shared smoke
```

## What not to do

- `schedule_c14m(when)` / `schedule_vid(when)` methods on NClock that still take a raw `uint64_t`. Domain in the method name does not stop `get_cycles() + n`.
- A single 14M queue with converted CPU delays.
- `process_due()` inside `incr_cycles()`.
- Runtime skip of past events.
- Call-site asserts as the only guard.
- `ClockRail` as a virtual `TimeSource`.
- Heap-allocated rails hanging off `computer_t`.
- Storing `NClock*` inside EventTimer “to have a now.”

## Open product decision (not blocked, not locked)

5.25 stays on the **CPU** rail. This refactor does not change that domain. IWM’s bug is only that 5.25 settle was stamped in CPU cycles onto the 14M heap; the fix is to enqueue it on `cpu`, same units as standalone Disk II.

Why CPU, not video / 14M: guest floppy code is written in CPU cycles (phase pulses, nibble loops, settle delays). If the CPU runs fast and the drive stays on PH0/14M, the firmware outruns the drive and 5.25 I/O fails. Clocking the drive in CPU time is a feature choice — the stepper and data path accelerate with the CPU — and it is why floppy still works at 2.8 / 7 / 14 MHz. That pairing is known to be fine on IIe.

On GS/IWM the same choice may have side effects that are not fully thought through (IWM’s own state machine, 3.5 vs 5.25 select, ludicrous stretch vs 1 MHz Mega II slots). That is context, not a mandate to move 5.25 off the CPU rail in this work. If it is revisited later, formal rails make it a constructor change.

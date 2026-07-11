# Ensoniq chip management issues
_Exported on 7/1/2026 at 13:49:59 EDT from Cursor (3.9.16)_

---

**User**

read code (*.s files) and identify how it manages the ensoniq chip in the apple iigs.

How does it utilize interrupts?
How does it time loading new data into the chip and its DOC RAM?
how does it set up and manage Apple IIgs memory - is it doing anything unusual?

I am trying to identify a potential bug in gssquared - this program works up to the point where you select a song; it loads the data file; it plays perhaps a quarter second or half second of audio, sometimes repeating a couple times, then hangs in a loop, perhaps waiting for an event to come that never comes.

---

**Cursor**

I'll search the assembly sources for Ensoniq, interrupt, and DOC RAM handling to map how audio is driven on the IIgs.


Here is how SenseiPlay / NinjaTrackerPlus drives the IIgs Ensoniq DOC, and where a gssquared hang is most likely coming from.

## Architecture Overview

Audio is handled entirely by **NinjaTrackerPlus** (`ninjatrackerplus/ninjatrackerplus.s`), loaded into its own 64K bank at runtime. SenseiPlay (`src/sp.s`) only loads songs, calls `NTPprepare` / `NTPplay`, and polls the player from `PlayerLoop`. It does **not** touch the sound chip directly during playback.

```947:962:src/sp.s
StartMusic      mx    %00
                ldx   BnkMODPtr
                ldy   BnkMODPtr+2
                lda   SPOPT_DOUBLE_CHANS    ; configurable (0=off/1=on)
                jsr   _NTPprepare
                bcc   :ok
                ...
:ok             lda   SPOPT_LOOP_SONG
                eor   #%1                   ; it's reversed to NTPplay
                jsr   _NTPplay
```

---

## Ensoniq / DOC Management

NTP talks to the chip through **direct-page I/O**, not through GS/OS Sound Tools:

| DP offset | Real address | Purpose |
|-----------|--------------|---------|
| `$3C` | `$C03C` / `$E1C03C` | Sound control |
| `$3D` | `$C03D` | Sound data |
| `$3E` | `$C03E` | Sound address |

During prepare, stop, and every IRQ, NTP sets **DP = `$C000`** so `$003C–$003E` hit the classic Ensoniq ports.

Three access modes:

```179:194:ninjatrackerplus/ninjatrackerplus.s
access_doc_registers    = *
                        ldal irq_volume          ; system volume from $E100CA
                        sta sound_control
                        rts

access_doc_ram          = *
                        ldal irq_volume
                        ora #%0110_0000          ; DOC RAM, auto-increment
                        sta sound_control
                        rts
```

**Prepare phase** (`NTPprepare`):
1. Copies non-streamed instrument samples into DOC RAM (`copy_instruments_to_doc`)
2. Ensures DOC page 0 has non-zero data (needed by the timer oscillator)
3. Builds per-track oscillator register tables
4. Installs the sound interrupt handler
5. Starts **oscillator 31** as a silent free-running timer

**Play phase**: the interrupt handler updates oscillators each tracker tick via `talk_to_doc`.

**Stop phase** (`NTPstop`): silences all used oscillators, stops osc 31, restores the original vector at `$E1002C`.

---

## Interrupt Usage

This is the core of playback timing.

### Vector hookup

NTP **replaces the system sound interrupt pointer** at `$E1002C`:

```929:967:ninjatrackerplus/ninjatrackerplus.s
setup_interrupt         = *
                        ldal  sound_interrupt_ptr
                        sta   backup_interrupt_ptr
                        ...
                        lda   #$5c
                        stal  sound_interrupt_ptr      ; JMP long
                        ...
                        lda   #interrupt_handler
                        stal  sound_interrupt_ptr+1
                        ...
                        lda   #2
                        sta   oscillator_job_tbl+interrupt_oscillator
                        ...
interrupt_oscillator    =     31
reference_freq          =     249                   ; ~50 Hz
```

Oscillator **31** fires periodic interrupts. Its job code is **2** → `tracker_interrupt` (the mod player tick).

### IRQ dispatch

On entry, the handler reads **`$E100CC`** (`osc_interrupt`) to see which oscillator(s) fired, then dispatches via `oscillator_job_tbl`:

| Job | Handler | Role |
|-----|---------|------|
| 0 | noop | One-shot instruments |
| 2 | `tracker_interrupt` | ~50 Hz tracker tick (rows, effects, register updates) |
| 6 | `interrupt_stream` | Refill streamed sample half-buffer |
| 8 | `interrupt_handler_instr` | Twin-osc swap (looped DOC-RAM samples) |
| 10 | `interrupt_streamsound` | SFX streaming API |

### Tracker tick timing

```1542:1564:ninjatrackerplus/ninjatrackerplus.s
tracker_interrupt       = *
                        lda   playing
                        beq   interrupt_handler_rts

                        lda   timer
                        beq   parse_tracks              ; new row when timer=0

                        inc
                        cmp   tempo
                        blt   tracker_interrupt_1
                        lda   #0
tracker_interrupt_1     sta   timer
                        ...
                        jsr   handle_effects
                        jmp   talk_to_doc
```

- Default **tempo = 6** → one pattern row every 6 timer IRQs (~120 ms at 50 Hz)
- Speed effect `$20+` retunes osc 31 via `bpm_to_freq`
- Speed effect **0** sets `pattern_jump = 6` → `stop_playing`

### Dangerous busy-waits inside the IRQ

The handler spins on DOC hardware status:

```1457:1466:ninjatrackerplus/ninjatrackerplus.s
]loop                   lda   sound_control
                        bmi   ]loop                     ; wait until DOC not busy

                        lda   #$e0
                        sta   sound_address
                        lda   sound_data                ; read pending IRQ flags
                        ...
                        bpl   interrupt_handler_loop    ; process more osc IRQs
```

If gssquared never clears bit 7 on the control register, or never sets bit 7 on the `$E0` status read, the **CPU hangs inside the interrupt handler**. That matches “plays briefly, repeats, then freezes.”

---

## Loading Data Into DOC RAM

Two paths:

### 1. Static instruments (prepare time)

Copied wholesale into DOC RAM during `NTPprepare`:

```773:827:ninjatrackerplus/ninjatrackerplus.s
copy_instruments_to_doc = *
                        jsr setup_docram
                        ...
                        lda instrument_types,y
                        and #%1000
                        bne copy_instruments_1          ; streamed → skip copy
                        ...
copy_instrument         = *
                        jsr access_doc_ram
                        stz sound_address
                        lda instrument_osc_a_ptr,y
                        sta sound_address+1
                        ; byte loop: read_instrument → sound_data
```

### 2. Streamed instruments (runtime)

Marked with type bit `$800`. Sample stays in **main RAM**; only 512-byte ping-pong buffers in DOC RAM are filled at play time:

```2262:2278:ninjatrackerplus/ninjatrackerplus.s
talk_to_doc_prepstream  = *
                        ; set stream_instr_ptr from instrument in RAM
                        jsr stream_copy                 ; first 512 bytes
                        jmp stream_copy_no_init
```

On each stream oscillator interrupt, `stream_copy` writes **256 bytes** into the next half-buffer and toggles `stream_doc_page_offset`. Osc A runs in **free-run-with-interrupt** mode; osc B plays audibly:

```1947:1957:ninjatrackerplus/ninjatrackerplus.s
                        lda #%0000_1000                 ; free run + interrupt on osc A
                        sta sound_data
                        ...
                        lda #6
                        sta oscillator_job_tbl,x        ; stream refill job
```

`stream_copy` also does **`PHB` / `PLB`** to switch to the sample’s bank before reading — bank switching must be correct in the emulator.

---

## Apple IIgs Memory — Unusual Choices

SenseiPlay does several things a typical GS/OS app would not:

### 1. Pre-locking banks 0 and 1 (fallback path)

If `MMStartUp` fails, it grabs **all of bank 0 and bank 1 from `$0800–$BFFF`** as locked, fixed, absolute handles before restarting the Memory Manager:

```1129:1168:src/sp.s
                _MTStartUp
                ...
                pea   $0800                 ; bank 0, $0800-$BFFF
                _NewHandle
                ...
                pea   $0800                 ; bank 1, $0800-$BFFF
                _NewHandle
                ...
                _MMStartUp                  ; retry MM
```

That reserves ~46 KB per bank for the app on tight 256K machines.

### 2. NTP engine in a full bank

`AllocOneBank` allocates an entire 64K block; the JSL stubs patch the bank byte:

```1094:1108:src/sp.s
PrepareNTP      jsr   AllocOneBank
                sta   BnkNTP
                sta   _NTPprepare+2         ; patch JSL bank byte
                ...
                PT_LoadFilenameToPtr 'ntpplayer';0
```

NTP is assembled at `$0F0000` but runs from whatever bank the allocator returns.

### 3. Song data in a separate contiguous block

Songs are loaded via `AllocContiguousPageAlign` into their own handle (`BnkMODHnd`), then the pointer is passed to `NTPprepare`. NTP returns a pointer past the instrument headers so that RAM can be reused.

### 4. Full zero-page save/restore around NTP calls

```876:893:src/sp.s
SaveDP          ; backs up $00–$FE to _DPBAK
RestoreDP       ; restores before returning to main loop
```

NTP sets **DP = `$C000`** internally; SenseiPlay protects its ZP-based ProDOS helpers.

### 5. Bypasses the OS sound driver entirely

No Sound Tool calls for playback. It overwrites `$E1002C` directly. On real hardware that works if nothing else owns sound; in an emulator, this must be wired to the same path real IRQs use.

---

## What SenseiPlay’s Main Loop Is “Waiting” For

During playback, `PlayerLoop` is a **tight poll loop**, not an event-driven wait:

```307:327:src/sp.s
PlayerLoop      inc   SP_ISPLAYING
:key_loop       lda   SP_ISPLAYING
                bne   :cont
                ...                             ; exit when playing=0
:cont           jsr   ShowTrackPos              ; reads NTPgetsongpos → playing flag
                jsr   ShowVUs / ShowGRVUs
                jsr   CheckKeyPlayerActions
                beq   :key_loop                 ; spin until key or song ends
```

`SP_ISPLAYING` comes from NTP’s `playing` word (byte 0 of `songinfo`). If the **tracker IRQ stops** but `playing` stays 1, the UI keeps spinning forever with frozen pattern/position — that can look like “waiting for an event.”

If **`playing` goes to 0**, the loop exits cleanly via `__cleanup` → `_NTPstop`.

---

## gssquared Bug Hypotheses (Ranked)

Your symptom — **~0.25–0.5 s of audio, sometimes repeating, then hang** — fits these patterns well:

### 1. Oscillator 31 timer IRQ stops (most likely for “repeating then freeze”)

- First row plays (initial `timer=0` → `parse_tracks` → `talk_to_doc`)
- Timer IRQs stop arriving
- Sample oscillators keep looping their DOC buffers → **short repeating snippet**
- `playing` stays 1 → **`PlayerLoop` spins forever**

**Check in gssquared:** delivery of sound IRQs via `$E1002C`, osc 31 enable (`$E1` global enable write in `timer_sound_settings`), and free-run mode on osc 31.

### 2. IRQ handler stuck in DOC busy-wait (full system freeze)

The `]loop / bmi ]loop` on `sound_control` bit 7, or the `$E0` status poll, never completes.

**Check:** DOC “busy” bit timing after register/RAM accesses; behavior of reads at address `$E0`.

### 3. Stream completion IRQ never fires (for streamed songs)

If the song uses streamed samples (`instrument_types & $800`), osc A should interrupt every 256 bytes to call `interrupt_stream`. If that IRQ never comes, you get one buffer worth of audio then silence/repeat while the main loop still spins.

**Check:** swap/free-run-with-interrupt modes, `$E100CC` osc identification, and whether stream osc interrupts are generated separately from osc 31.

### 4. `$E100CC` / osc identification wrong

The handler masks `osc_interrupt` with `%00111110` and shifts to get the osc number. Wrong emulation → dispatch to wrong job → unpredictable stalls.

### 5. Bank switching during `stream_copy`

```3717:3719:ninjatrackerplus/ninjatrackerplus.s
                        lda   stream_instr_ptr_hi,y
                        pha
                        plb                         ; switch to sample bank
```

Wrong `PHB`/`PLB` behavior could corrupt streaming after the first chunk.

---

## Suggested gssquared Debug Steps

1. **Breakpoint on `interrupt_handler`** (`NTP bank + offset of `interrupt_handler`) — does it keep firing after ~0.5 s?
2. **Log which osc triggered** each IRQ — expect regular osc **31** ticks plus occasional stream osc hits.
3. **Watch `$E1002C`** — confirm the JMP long to NTP’s handler is installed and not overwritten.
4. **Inspect `playing` at `$C000+` offset in the NTP bank** (or via `NTPgetsongpos`) when hung — if still 1, tracker tick stopped; if 0, SenseiPlay should exit (different bug).
5. **Try a song with only small DOC-RAM instruments** vs one with large streamed samples — narrows issue #1 vs #3.
6. **Compare with KEGS/ActiveGS** on the same `.2mg` — if those work, diff Ensoniq IRQ and DOC status behavior.

The highest-value fix area in gssquared is almost certainly **Ensoniq interrupt delivery and DOC status register semantics**, not SenseiPlay’s ProDOS memory management. The brief-audio-then-repeat pattern is classic “music voices still looping, but the 50 Hz tracker IRQ died.”

If you want, I can help trace a specific hung state (register dump / which song file) or map exact runtime addresses once you know which bank `BnkNTP` landed in under gssquared.

---

**User**

I want you to be able to boot, execute senseiplay, and interrogate gssquared automatically.

You can already specify an image to load and boot to on the gssquared CLI.
what interface would you need to be able to directly, interactively debug this application in GS2. be specific

---

**Cursor**

I'll look for any existing gssquared integration in the repo and what CLI/debug hooks it exposes today.


---

**User**

there is no external debug interface currently. I am asking you to devise one

---

**Cursor**

Here is a concrete debug interface design for GS2, aimed at letting an agent (or script) boot SenseiPlay, drive the UI, and diagnose the Ensoniq/IRQ hang without a human at the keyboard.

---

## Design goal

**Machine-first, human-second.** Every operation must be callable from CLI or a socket, return structured JSON, and support scripted “boot → play song → watch these addresses → break on hang.”

Recommended transport: **line-delimited JSON-RPC 2.0** on a Unix domain socket (or TCP port), started with:

```bash
gssquared --image senseiplay.2mg --debug-server /tmp/gs2.sock
# or
gssquared --image senseiplay.2mg --debug-server 127.0.0.1:8765
```

The emulator runs the main loop; a lightweight debug thread accepts requests and pauses the CPU only when needed (or uses lock-free reads for safe “peek” operations).

---

## Layer 1: Execution control (minimum viable)

These are required before anything else works.

| Method | Purpose |
|--------|---------|
| `run` | Continue after pause |
| `pause` | Stop at next instruction boundary |
| `step` `{ "count": 1 }` | Single-step 65816 |
| `step_over` | Step, skip one `JSL`/`JSR` |
| `reset` `{ "hard": true }` | Power-on reset |
| `get_state` | Full machine snapshot |
| `set_registers` / `get_registers` | A,X,Y,S,DP,P,DBR,PBK,PC (24-bit) |
| `read_memory` `{ "addr": "00/2000", "len": 256 }` | Bank/offset hex |
| `write_memory` | Patch RAM |
| `disassemble` `{ "addr": "00/2000", "count": 20 }` | Merlin-ish syntax preferred |

**Address format:** `"bb/oooo"` (bank byte + 16-bit offset), e.g. `"0F/0000"` for NTP bank base. Also accept `"E1002C"` for direct 24-bit tool/global addresses.

**`get_state` response must include:**
```json
{
  "running": false,
  "paused_reason": "breakpoint",
  "registers": { "pc": "00/2000", "a": 0, "x": 0, "y": 0, "dp": 0, "p": 0x28, "b": 0, "k": 0 },
  "cycles": 12345678,
  "irq_pending": false,
  "in_irq": false,
  "irq_vector": "E1002C"
}
```

Without this, nothing else is usable.

---

## Layer 2: Breakpoints and watchpoints (for the hang)

| Method | Purpose |
|--------|---------|
| `break_set` `{ "addr": "bb/oooo", "id": "ntp_irq" }` | PC breakpoint |
| `break_clear` | Remove |
| `watch_set` `{ "addr": "E1002C", "len": 4, "on": "write" }` | Catch vector overwrite |
| `watch_set` `{ "addr": "C03C", "len": 3, "on": "read_write" }` | DOC access trace |
| `break_on_irq` `{ "vector": "sound", "enabled": true }` | Break when sound IRQ taken |
| `break_on_condition` | Expression-based (see Layer 4) |

For SenseiPlay specifically, you want to break on:

1. **Sound IRQ entry** (after vector dispatch to NTP’s `interrupt_handler`)
2. **Infinite loop sites** — e.g. PC stuck in NTP’s `]loop` busy-wait on `sound_control`
3. **Write to `$E1002C`** — something restoring the OS vector mid-play

---

## Layer 3: Input automation (boot + navigate UI)

SenseiPlay is ProDOS 8, text UI, keyboard-driven. The agent must not rely on GUI clicks.

| Method | Purpose |
|--------|---------|
| `key_inject` `{ "key": "RETURN", "modifiers": [] }` | One-shot key |
| `key_sequence` `{ "keys": ["DOWN","DOWN","RETURN"], "delay_ms": 100 }` | Menu navigation |
| `type_string` `{ "text": "/senseiplay/songs\\n" }` | If BASIC/shell ever needed |
| `wait_for_text` `{ "addr": "00/0400", "len": 1024, "contains": "Loading" }` | Screen scrape main/aux text |
| `wait_idle` `{ "cycles": 1000000 }` | Deterministic delay |

**Screen read:**
```json
"read_text_screen": {
  "page": 0,
  "row": 0, "col": 0,
  "width": 80, "height": 24
}
```
Returns raw Apple text (high bit set) or ASCII-normalized.

**Boot script target for SenseiPlay:**
```json
{ "method": "script_run", "params": {
  "steps": [
    { "wait_cycles": 500000000 },
    { "wait_text": { "contains": "Sensei" } },
    { "key": "DOWN" },
    { "key": "RETURN" },
    { "wait_text": { "contains": "Loading" } },
    { "wait_ms": 2000 },
    { "pause": true }
  ]
}}
```

---

## Layer 4: Condition / expression engine (critical for “hang” detection)

Hang detection cannot rely on the user noticing silence. The interface needs **predicates over machine state**:

| Method | Example |
|--------|---------|
| `eval` `{ "expr": "read8('C03C') & 0x80" }` | DOC busy bit |
| `wait_until` `{ "expr": "read8('E1002C') == 0x5C", "timeout_ms": 5000 }` | Vector installed |
| `wait_stuck` `{ "pc_range": ["bb/1457","bb/1466"], "cycles": 100000 }` | Spin in IRQ busy-wait |
| `wait_no_progress` `{ "watch": "bb/songinfo+0", "timeout_ms": 500 }` | `playing` unchanged, PC in PlayerLoop |

Expression language (minimal):
- `read8(addr)`, `read16(addr)`, `read24(addr)`
- `pc()`, `cycles()`, `in_irq()`
- `reg('a')`, `reg('dp')`
- Comparisons, `&&`, `||`

For SenseiPlay hang:
```json
"wait_stuck": {
  "expr": "pc() >= 'NN/1457' && pc() <= 'NN/1466' && (read8('C03C') & 0x80)",
  "min_consecutive_cycles": 50000
}
```
(where `NN` is the runtime NTP bank from `BnkNTP`)

---

## Layer 5: Ensoniq / sound introspection (the differentiator)

Generic CPU debug is not enough. The bug is almost certainly **IRQ + DOC**. GS2 needs first-class sound debug hooks:

### 5a. Sound system state snapshot

```json
"sound_get_state": {}
```

Response:
```json
{
  "irq_vector": { "addr": "E1002C", "bytes": ["5C", "xx", "xx", "xx"] },
  "irq_vector_target": "0F/1420",
  "osc_interrupt_reg": "E100CC",
  "last_irq_osc": 31,
  "last_irq_pc": "0F/1420",
  "irq_count": { "total": 412, "osc31": 380, "osc_stream": 32 },
  "oscillators": [
    {
      "num": 31,
      "enabled": true,
      "mode": "free_run",
      "freq": 249,
      "volume": 0,
      "job": 2,
      "wavetable_ptr": 0,
      "wavetable_size": 256
    }
  ],
  "doc_busy": true,
  "doc_control": "0xCA",
  "doc_last_access": { "type": "register", "addr": "C03E", "cycle": 12345670 }
}
```

### 5b. IRQ trace ring buffer

```json
"trace_irq": { "enable": true, "max_entries": 1000 }
"trace_irq_get": {}
```

Each entry:
```json
{
  "cycle": 12345600,
  "osc": 31,
  "vector": "sound",
  "handler_entry": "0F/1420",
  "handler_exit_cycle": 12345680,
  "doc_busy_waits": 3
}
```

**Agent use:** after playing a song 2 seconds, if `osc31` count stops increasing but `osc_stream` or sample osc keeps firing → timer IRQ died.

### 5c. DOC access trace (optional, high volume)

```json
"trace_doc": { "enable": true, "filter": "register_only" }
```

Log reads/writes to `$C03C–$C03E` (and `$E1C03x` if mapped separately), with cycle timestamp and PC.

### 5d. Break on sound-specific events

```json
"break_on_sound": {
  "event": "osc_interrupt",
  "osc": 31
}
```

Events enum:
- `sound_irq_taken`
- `sound_irq_vector_called`
- `doc_busy_timeout` — **DOC busy bit set > N cycles at same PC** (automatic hang detector)
- `osc_buffer_complete` — stream half-buffer interrupt
- `osc31_tick` — tracker clock

The **`doc_busy_timeout`** event is the single most valuable automation hook for this bug.

---

## Layer 6: Symbols and program awareness

SenseiPlay is Merlin-assembled. GS2 should load debug info without manual address math.

| Method | Purpose |
|--------|---------|
| `symbols_load` `{ "path": "sensei.system.sym", "base": "00/2000" }` | Merlin/OBJ symbols |
| `symbols_load` `{ "path": "ntpplayer.sym", "base": "auto:bank_handle" }` | Bank-relative NTP |
| `resolve` `{ "name": "PlayerLoop" }` → `"00/0133"` |
| `break_set` `{ "symbol": "interrupt_handler" }` | Symbolic breakpoints |

Minimum symbol sources:
- Merlin `.sym` / listing exports
- Optional: JSON map checked into repo:

```json
{
  "sensei.system": { "base": "00/2000", "symbols": { "PlayerLoop": "0x0307", "StartMusic": "0x03B4" }},
  "ntpplayer":     { "base": "bank:BnkNTP", "symbols": { "interrupt_handler": "0x1420", "playing": "0x4195" }}
}
```

Runtime **`BnkNTP`** is only known after `PrepareNTP`; the agent resolves it via:
```json
"read_memory": { "addr": "00/xxxx", "len": 2 }  // BnkNTP variable in sp.s
```
or `symbols_load` with `"base": "read16('00/BnkNTP')<<8"`.

---

## Layer 7: Events (push, not poll)

Long waits should be event-driven. Client subscribes once:

```json
{ "method": "subscribe", "params": { "events": ["pause", "break", "irq", "doc_busy_timeout", "text_match"] }}
```

Server pushes:
```json
{ "event": "doc_busy_timeout", "data": { "pc": "0F/1457", "cycles_stuck": 65536, "doc_control": "0xCA" }}
{ "event": "irq", "data": { "osc": 31, "count_since_boot": 401 }}
{ "event": "pause", "data": { "reason": "breakpoint", "id": "ntp_irq" }}
```

This lets an agent run `run`, subscribe, inject RETURN to pick a song, and react when `doc_busy_timeout` or `osc31` stall fires — no polling loop in the client.

---

## Layer 8: Recording and replay (optional but powerful)

| Method | Purpose |
|--------|---------|
| `record_start` `{ "include": ["irq", "doc", "keys"] }` | Capture session |
| `record_stop` → file | Share reproducer |
| `replay` `{ "file": "senseiplay_hang.gs2trace" }` | Deterministic repro |

Useful for filing GS2 bugs with a 50 KB trace instead of “sometimes hangs.”

---

## Minimal CLI surface (for shell agents)

Even with JSON-RPC, expose thin CLI wrappers:

```bash
gs2ctl --socket /tmp/gs2.sock pause
gs2ctl --socket /tmp/gs2.sock read E1002C 4
gs2ctl --socket /tmp/gs2.sock break 0F/1420
gs2ctl --socket /tmp/gs2.sock key RETURN
gs2ctl --socket /tmp/gs2.sock sound-state
gs2ctl --socket /tmp/gs2.sock wait-until 'irq_count.osc31 > 100' --timeout 5
gs2ctl --socket /tmp/gs2.sock run-script boot_senseiplay_play_first.json
```

---

## End-to-end agent workflow (SenseiPlay hang)

A complete automated interrogation script would look like:

```json
[
  { "reset": {} },
  { "run": {} },
  { "wait_text": { "contains": "Sensei" } },
  { "symbols_load": { "file": "debug/senseiplay.json" } },
  { "trace_irq": { "enable": true } },
  { "break_on_sound": { "event": "doc_busy_timeout", "threshold_cycles": 10000 } },
  { "key_sequence": { "keys": ["RETURN"], "delay_ms": 200 } },
  { "wait_text": { "contains": "Loading" } },
  { "wait_until": { "expr": "irq_count('osc31') > 10", "timeout_ms": 3000 } },
  { "wait_ms": 2000 },
  { "sound_get_state": {} },
  { "get_registers": {} },
  { "trace_irq_get": {} },
  { "read_memory": { "addr": "sym:playing", "len": 2 } },
  { "read_memory": { "addr": "E1002C", "len": 4 } }
]
```

**Interpret results:**

| Observation | Likely cause |
|-------------|--------------|
| `osc31` count frozen, sample osc still looping | Timer IRQ / vector dispatch broken |
| PC stuck at NTP `interrupt_handler` busy-wait, `doc_busy` always true | DOC status bit 7 never clears |
| `$E1002C` no longer JMP to NTP bank | Vector restored or never installed |
| `playing==1`, PC in `PlayerLoop` `:key_loop` | Tracker stopped, UI spin (matches user report) |
| `playing==0` | Song ended or speed-0 effect; different path |

---

## Implementation priority (if building incrementally)

1. **JSON-RPC + pause/run/step + memory/registers** — 1–2 days, unlocks everything else  
2. **Key inject + text screen read** — boot SenseiPlay unattended  
3. **`sound_get_state` + IRQ trace + `break_on_sound: doc_busy_timeout`** — targets this bug directly  
4. **Expressions + `wait_until` / `wait_stuck`** — automated hang detection  
5. **Symbols + scripts** — ergonomics  
6. **DOC trace + record/replay** — deep dives and bug reports  

---

## What I explicitly do *not* need

- Full GUI debugger parity on day one  
- Source-level 65816 debugging (nice later)  
- GS/OS toolbox call tracing (SenseiPlay is ProDOS 8 + direct DOC access)  
- Network remote desktop — socket RPC is enough  

---

## Summary

The interface GS2 needs is a **scriptable debug server** with three properties ordinary emulators lack:

1. **Deterministic automation** — boot, keys, text wait, no human.  
2. **Sound-native introspection** — osc IRQ counts, `$E1002C`, DOC busy, stream buffer events.  
3. **Hang as a first-class event** — `doc_busy_timeout`, `wait_stuck`, `osc31_stall` rather than “CPU still running.”

With Layers 1–3 + 5a/5b/5d, I could boot SenseiPlay on every CI run, play the first song, and produce a structured report pointing at vector dispatch, DOC busy-wait, or stream IRQ failure within seconds of the hang.

If you want a next step in-repo, I can draft a `debug/senseiplay.gs2script` plus a `debug/symbols.json` keyed to the Merlin labels from `sp.s` and `ninjatrackerplus.s` so the protocol has concrete addresses to target on day one.

---

**User**

I'm wondering, MAME offers Lua for external scriptable debug access. what you're proposing above looks like a DSL (domain specific language), which I think I'd like to avoid. I am not necessarily a big fan of LUA though, so I'm wondering if there are some other options that would work well. 

I envision the architecture being something like this:
from CLI, start gssquared with a -debug option or something similar
this then would launch a debug thread at startup. the debug thread would communicate commands to the main thread using a thread-safe ring buffer. the main thread would process commands off the ring buffer as part of its execution loop.

would the debug thread run a specific debug script, generated by the LLM, and the debug script is executed by gs2 debug thread, then llm inspects output and interacts in that manner? Or would it be better to give the LLM a tool that it could use to dynamically and dynamically interact with the machine state through the debug thread?

---

**Cursor**

Your ring-buffer split (debug thread ↔ main emulation thread) is the right shape. The main design choice is not really “Lua vs DSL” — it is **batch script vs interactive tool API**, and whether the “language” lives in GS2 or on the host.

---

## DSL vs API (clarifying the earlier proposal)

What I described before was closer to **QMP-style JSON-RPC** — a fixed vocabulary of methods (`read_memory`, `pause`, `sound_get_state`), not a DSL with expressions like `wait_until pc() in range`. You can keep that shape and still avoid a DSL entirely:

- **No expression language inside GS2**
- **No `wait_stuck` mini-language**
- Just ~20–30 imperative commands with plain arguments

Anything “smart” (conditions, loops, “if osc31 stalled for 500ms then…”) runs in **Python or the LLM agent on the host**, not inside the emulator.

That matters for your Lua question: MAME Lua is powerful because the *host* logic runs in a real language. You do not need to embed that in GS2 if the debug thread only exposes primitives.

---

## Script-at-startup vs dynamic LLM tools

**For LLM debugging, dynamic tools win.** A one-shot debug script is a good supplement, not the primary interface.

| | Generated debug script (runs to completion) | Dynamic tools (pause → inspect → act → repeat) |
|---|---|---|
| Fits LLM workflow | Poor — one shot, no mid-course correction | Good — matches observe / hypothesize / test |
| Handles surprises | Bad — wrong menu state, wrong bank, timing | Good — read screen, re-navigate |
| Repro / CI | Excellent — same script every run | Needs wrapper script on host |
| Token cost | Low round trips, high upfront script size | More round trips, smaller steps |
| Failure mode | Silent wrong conclusion at end | Visible at each step |

**Recommended hybrid:**

1. **Primary:** LLM (or Cursor) calls tools like `gs2_pause`, `gs2_read_mem`, `gs2_sound_state`, `gs2_key` one at a time.
2. **Secondary:** Host-side Python (or a JSON list of tool calls) for CI: “boot, pick first song, assert osc31 count increases for 2s.”
3. **Not recommended:** GS2 executes an LLM-authored *program* with conditionals and loops inside the emulator — that becomes a DSL whether you call it Lua or not.

So: the debug thread should **not** mainly “run a script generated by the LLM.” It should **service a command queue**; the LLM (or a Python driver) is the script.

```
┌─────────────┐     tool calls      ┌──────────────┐   ring buffer   ┌─────────────┐
│ LLM / agent │ ──────────────────► │ debug thread │ ◄──────────────► │ main thread │
│ or Python   │ ◄── structured JSON │ (I/O only)   │   responses     │ (6502/DOC)  │
└─────────────┘                     └──────────────┘                 └─────────────┘
```

The debug thread’s job: socket/pipe/MCP, parse requests, enqueue to main, block until response. It should not interpret “wait until playing == 0” — the agent does that by polling `read_mem` in a loop with timeouts.

---

## Ring buffer architecture (concrete)

**Command record (fixed size, no strings in the hot path for simple ops):**

```c
typedef enum { CMD_PAUSE, CMD_RUN, CMD_STEP, CMD_READ_MEM, CMD_WRITE_MEM,
               CMD_GET_REGS, CMD_INJECT_KEY, CMD_GET_SOUND, ... } cmd_type;

typedef struct {
    uint32_t seq;           // request id
    cmd_type op;
    uint64_t arg0, arg1, arg2;  // addr, len, keycode, etc.
    // bulk write data follows in separate slot if needed
} debug_cmd;
```

**Main thread processing:** drain the command ring at a **safe point** — ideally once per emulated instruction when debugger attached, or when explicitly paused. Never handle `READ_MEM` mid-instruction unless you document that as best-effort.

**Important command classes:**

| Class | Behavior |
|-------|----------|
| Instant peek | `read_mem`, `get_regs`, `sound_snapshot` — OK while running (document consistency) |
| Must pause | `write_mem`, `break_set`, `step` — main thread pauses first, then executes |
| Run control | `run`, `run_cycles(N)` — main thread runs until pause condition |

**Responses:** same `seq`, status, small inline payload or offset into a shared response blob arena (for screen dumps, disassembly).

This keeps the debug thread dumb and thread-safe; all machine access stays on the main thread.

---

## Alternatives to Lua (if you want *something* embeddable)

If the goal is “external scriptability without inventing a DSL,” ranked for your use case:

### 1. No embedded language — JSON lines + host Python (best for LLM)

GS2 exposes only the ring-buffer protocol over stdin or a Unix socket. A tiny Python library wraps it. The LLM uses tools that call Python (or calls the socket directly).

- No Lua, no DSL, no expression parser in C++
- CI and agents share the same API
- Fits Cursor MCP naturally (`gs2_read_memory`, etc.)

### 2. GDB remote serial protocol (RSP)

Implement enough of RSP for memory, registers, breakpoints, continue/step. Then:

- `lldb`, `gdb`, or Python `pygdb` can attach
- LLM tools wrap standard debugger commands
- Zero custom language; huge existing ecosystem

Downside: no built-in Ensoniq/sound concepts — you add **custom RSP packets** or a side channel for `sound_get_state` anyway.

### 3. Python embedded in GS2 (if you want in-process scripts)

Pybind11 module on the debug thread; callbacks enqueue commands to main thread (never touch emulation state from Python directly).

- LLMs are good at Python
- Scripts are real code, not a DSL
- Heavier dependency than JSON lines

### 4. QuickJS / Duktape (Lua-like but JS)

Same model as MAME Lua, different syntax. Only worth it if you want plugins/user mods. For *your* LLM-driven workflow, inferior to host Python.

### 5. WASM guest modules

Sandboxed, language-agnostic. Overkill unless you expect third-party debug extensions.

**Practical pick:** **JSON-line protocol on the debug thread + optional Python shim.** Skip embedded Lua unless you want user-facing mod scripting unrelated to the LLM.

---

## What the LLM should actually call (minimal stable surface)

Keep the command set small and orthogonal — ~25 commands, no composition inside GS2:

**Execution:** `pause`, `run`, `step`, `step_over`, `reset`, `run_cycles`  
**CPU:** `get_regs`, `set_regs`, `disasm`  
**Memory:** `read`, `write` (bank/offset or 24-bit)  
**Break:** `break_add`, `break_del`, `break_list`  
**Input / UI:** `key_down`, `key_up`, `read_text_page` (main/aux)  
**Sound (GS2-specific):** `sound_state`, `irq_trace_start`, `irq_trace_read`, `doc_trace_start`  
**Meta:** `version`, `image_info`, `cycle_count`

The agent composes workflows:

```text
pause → read_text_page → key RETURN → run_cycles 1e9 → pause → sound_state → read E1002C 4
```

If osc31 count did not increase → hypothesis: timer IRQ. If PC in NTP busy-wait → hypothesis: DOC busy bit.

No in-emulator `wait_until`; the agent loop is:

```python
for _ in range(100):
    st = sound_state()
    if st["irq_count"]["osc31"] > prev + 10:
        break
    run_cycles(100_000)
else:
    report("osc31 stalled")
```

That is easier for an LLM to reason about than a 200-line script that failed at line 47.

---

## When a “script” still makes sense

Use **host-side** scripts, not GS2-internal:

| Use case | Where script lives |
|----------|-------------------|
| CI regression: “play 5s without doc_busy hang” | Python in repo |
| Share repro with human | Python or JSON array of API calls |
| One-shot LLM investigation | Agent tool sequence in chat (dynamic) |
| User macro “always boot to /senseiplay” | Shell + `gs2ctl` |

A JSON “script” that is literally `[[ "key", "RETURN" ], [ "run_cycles", 500000000 ], ...]` is fine — it is data, not a DSL, and the interpreter is 50 lines of Python.

---

## MCP / Cursor integration (how I would use it)

Expose GS2 as an MCP server (or subprocess tools) mapping 1:1 to ring-buffer commands:

- `gs2_pause`
- `gs2_read_memory(bank, offset, len) → hex`
- `gs2_sound_state() → json`
- `gs2_inject_key(name)`
- `gs2_disassemble(addr, count)`

Then I boot with `gssquared -debug /tmp/gs2.sock`, connect tools, and debug SenseiPlay interactively — same as debugging a live process, not running a prebaked script.

For CI, a Python test harness calls the same MCP server or socket without an LLM.

---

## Direct answer to your architecture question

**Would the debug thread run an LLM-generated script, or should the LLM use dynamic tools?**

**Dynamic tools through the debug thread**, with the main thread executing commands from the ring buffer each emulation step (or while paused).

- The debug thread: I/O, framing, sequencing, timeouts — **not** program execution.
- The LLM: writes the “program” as a series of tool calls, adapting after each observation.
- Optional: Python on the host for repeatable regression scripts using the **same** commands.

Avoid embedding Lua or a custom DSL in GS2 unless you have a separate goal (user plugins). For automated SenseiPlay / Ensoniq investigation, **a thin command protocol + host-side Python + MCP tools** is simpler, more testable, and matches how LLM agents actually debug.

If you want a next step in-repo, I can sketch the JSON request/response schema and a `debug/senseiplay_repro.py` that uses only those primitives to detect the osc31-stall hang — no GS2-side language at all.


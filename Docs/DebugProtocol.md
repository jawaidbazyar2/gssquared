# External Debug Protocol (Framing v1)

Wire protocol for host-driven debugging of GSSquared. External tools (CLI, Python, LLM agents) speak this framing over a local byte stream; the emulator does not embed a scripting language or parse JSON.

This document is the source of truth for the wire format. Exploratory notes in `ExternalDebugInterface.md` are historical and not normative. Host-side library: [DebugClient.md](DebugClient.md). **Agent cookbook:** [gs2debug.md](gs2debug.md).

## Goals

- Host-driven, imperative commands only — no DSL, Lua, or expression language inside GS2.
- Opaque binary payloads — no JSON encode/decode in the emulator.
- Transport-agnostic framing that works on Unix domain sockets first, TCP later.
- Dedicated protocol-driver thread so the main emulation loop never blocks on socket I/O.
- Extensible type namespace and sequence IDs for later pipelining, streaming, and client-side routing.

## Non-goals (v1)

- TCP listen/connect (frame is ready; transport comes later).
- Required request pipelining (header supports it; implementation may allow only one outstanding request).
- MCP, GDB RSP, or an embedded script runtime.
- Full debug command set — session meta plus GET_STATUS / RESET / PAUSE / CONTINUE / STEP_INTO / GET_TRACE / READMEM / WRITEMEM / BP_* / KEYEVENT / STATE_GET / STATE_SET / QUIT below.

---

## Frame layout

All multi-byte integers are **little-endian**.

```
Offset  Size  Field
0       4     type     — command / response / event kind
4       4     seq      — correlation / routing id
8       4     length   — N, payload byte count
12      N     data     — opaque; meaning defined by type
```

| Property | Value |
|----------|-------|
| Header size | 12 bytes |
| Max `length` (v1) | 1 MiB (`0x00100000`). Larger frames are rejected. |
| Empty payload | `length == 0` is valid; no data bytes follow. |

Receivers must read the 12-byte header, validate `length`, then read exactly `N` payload bytes before parsing the next frame.

---

## Type word

The `type` field packs flags, a main command, and a subcommand:

```
Bits 31–24  flags      (8 bits)
Bits 23–8   main       (16 bits) — command family
Bits 7–0    sub        (8 bits)  — operation within family (0–255)
```

```
type = (flags << 24) | (main << 8) | sub
```

| Field | Extract |
|-------|---------|
| flags | `(type >> 24) & 0xFF` |
| main | `(type >> 8) & 0xFFFF` |
| sub | `type & 0xFF` |

### Main command numbers

| `main` | Use |
|--------|-----|
| `0` | Meta / session (`HELLO`, `PING`, `ERROR`, `EVENT`) |
| `1` | Execution control |
| `2` | CPU / regs / disasm |
| `3` | Memory |
| `4` | Breakpoints |
| `5` | Input / UI |
| `6` | Sound / Ensoniq |

### Flag bits (high byte)

Direction is implicit on the wire: client→server is a request; server→client is either a **reply** or an **event**.

| Bit in flags | Mask on `type` | Meaning |
|--------------|----------------|---------|
| 7–0 | `0xFF000000` | Reserved; must be zero in v1. |

Rules:

1. **Client → server:** requests with `flags == 0`.
2. **Server → client (reply):** same `type` as the request (`flags == 0`, same `main`/`sub`); **`seq` echoed**. Matched by the client against outstanding requests. Replies never use `type=EVENT`.
3. **Server → client (event):** `type=EVENT` (`main=0`, `sub=4`); server allocates `seq` (monotone per connection is fine). Not correlated to a pending request. Client routes by `seq` and/or the `event_id` in the payload.
4. **Unknown or failed request:** server replies with `ERROR` (`main=0`, `sub=3`), echoing the request `seq`.
5. Client must not send `EVENT` or `ERROR`.

Convenience:

```
FLAGS_MASK = 0xFF000000
MAIN_MASK  = 0x00FFFF00
SUB_MASK   = 0x000000FF
TYPE_MASK  = 0x00FFFFFF   /* main + sub; flags cleared */
```

---

## Sequence ID

- Client chooses `seq` for each request. Non-zero is recommended; `0` is reserved for “no correlation.”
- Server **must** echo `seq` on the matching response.
- Enables later: pipelined requests, fan-out to different client modules, and multi-frame streams that share one `seq` without changing the header.
- **v1 behavior:** at most one outstanding request per connection is acceptable. The header still carries `seq` so relaxing that later is not a framing break.

---

## Payload rules

- Opaque binary; layout is **per `type`**, documented with each command.
- No JSON in GS2.
- Fields use explicit widths (`uint8` / `uint16` / `uint32` / `uint64`), little-endian, packed sequentially. Do not rely on compiler struct padding across the wire — document field order and sizes.
- Strings (rare): either length-prefixed inside the payload, or “remainder of payload is UTF-8” when there is a single trailing string (no NUL required).

---

## Transport and session

The protocol is a **byte stream**. Intended transports:

| Phase | Transport |
|-------|-----------|
| First | Local socket (AF_UNIX on macOS / Linux; AF_UNIX on Windows 10+, named pipe as a later fallback if needed) |
| Later | TCP |

Session rules (v1):

- One client connection at a time; additional connects are **rejected**.
- After connect, client sends `HELLO`; server replies with the same `type` (`HELLO`) plus version/caps in the payload. Further commands are undefined until handshake succeeds.
- Socket path / bind address is a CLI concern (e.g. `-debug /path`); not part of the frame.

---

## Threading model (hard requirement)

```
Client  --byte stream-->  protocol driver thread  --ring-->  main emu thread
        framed messages   accept / read / write /     drain cmds at a safe
                          frame only; never touch     point; never block on
                          emulated machine state      socket I/O
```

Rules:

1. **Protocol driver is its own thread**, started when the debug socket is enabled. It owns the listen socket, accept, framed read/write, and wire-level seq correlation.
2. **Main emulation loop stays non-blocking** w.r.t. this interface: no `recv` / `send` / `accept` on the main thread. Only a cheap non-blocking drain of a thread-safe command ring (and enqueue of replies) at a safe point.
3. Protocol thread **must not** read or write emulated machine state. Peeks, pokes, and run-control go through the ring to the main thread.
4. Main thread posts results to a response ring (or equivalent). Protocol thread frames them and writes to the socket. Socket backpressure is absorbed on the protocol thread, not the emu loop.
5. Meta commands that need no machine state (`HELLO`, `PING`) **may** be answered entirely on the protocol thread so handshake does not depend on the emu loop ticking. `QUIT` runs on the main thread (force-halt).

---

## Initial command set

### Type IDs (meta, `main == 0`)

| Name | `main` | `sub` | `type` | Direction |
|------|--------|-------|--------|-----------|
| `HELLO` | 0 | 1 | `0x00000001` | client request; server reply uses same `type` |
| `PING` | 0 | 2 | `0x00000002` | client request; server reply uses same `type` |
| `ERROR` | 0 | 3 | `0x00000003` | server reply only — when a request fails or is unknown |
| `EVENT` | 0 | 4 | `0x00000004` | server → client only — unsolicited notification |
| `QUIT` | 0 | 5 | `0x00000005` | client request; force-quit (skips QuitModal) |

### Type IDs (implemented non-meta)

| Name | `main` | `sub` | `type` | Thread | Reply payload |
|------|--------|-------|--------|--------|---------------|
| `GET_STATUS` | 1 | 1 | `0x00000101` | main | 8 bytes: `execution_mode`, `platform_id` |
| `RESET` | 1 | 2 | `0x00000102` | main | empty |
| `PAUSE` | 1 | 3 | `0x00000103` | main | empty |
| `CONTINUE` | 1 | 4 | `0x00000104` | main | empty |
| `STEP_INTO` | 1 | 5 | `0x00000105` | main | empty |
| `GET_TRACE` | 2 | 1 | `0x00000201` | main | 8-byte header + `N×40` entries |
| `READMEM` | 3 | 1 | `0x00000301` | main | `length` data bytes |
| `WRITEMEM` | 3 | 2 | `0x00000302` | main | empty |
| `BP_SET` | 4 | 1 | `0x00000401` | main | 4 bytes: `id` |
| `BP_CLEAR` | 4 | 2 | `0x00000402` | main | empty |
| `BP_CLEAR_ALL` | 4 | 3 | `0x00000403` | main | empty |
| `BP_ENABLE` | 4 | 4 | `0x00000404` | main | empty |
| `BP_LIST` | 4 | 5 | `0x00000405` | main | `count` + records |
| `KEYEVENT` | 5 | 1 | `0x00000501` | protocol (`SDL_PushEvent`) | empty |
| `STATE_GET` | 6 | 1 | `0x00000601` | main | device-specific blob |
| `STATE_SET` | 6 | 2 | `0x00000602` | main | empty (or device ack) |

### Protocol version

Current protocol version: **1**.

### `HELLO` — main 0, sub 1 (`0x00000001`)

First message after connect. May be handled on the protocol thread.

**Request payload** (8 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `version` | Client protocol version (`uint32`). Send `1`. |
| 4 | 4 | `flags` | Client capability flags (`uint32`). v1: `0`. |

**Success reply** (same `type=HELLO`, echoed `seq`), payload (12 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `version` | Server protocol version (`uint32`). `1`. |
| 4 | 4 | `flags` | Server capability flags (`uint32`). v1: `0`. |
| 8 | 4 | `max_payload` | Max accepted payload length (`uint32`). `0x00100000` (1 MiB). |

If the client version is unsupported, server replies with `ERROR` (same `seq`) instead of `HELLO`.

### `PING` — main 0, sub 2 (`0x00000002`)

Liveness check. May be handled on the protocol thread.

**Request payload:** empty (`length == 0`).

**Success reply** (same `type=PING`, echoed `seq`): empty payload.

### `QUIT` — main 0, sub 5 (`0x00000005`)

Force-quit the emulator process without the QuitModal confirmation or dirty-disk save prompts. Runs on the main thread (sets `no_quit_confirm`, posts `HLT_USER` / `SDL_EVENT_QUIT`). Prefer this over killing the process from test harnesses.

Also available as CLI: `--no-quit-confirm` (same skip for any `SDL_EVENT_QUIT`, including SIGTERM mapped by SDL).

**Request payload:** empty (`length == 0`). Requires successful `HELLO`.

**Success reply** (same `type=QUIT`, echoed `seq`): empty payload. The socket may close shortly after.

### `ERROR` — main 0, sub 3 (server → client only)

Used for failures and unknown request types. Sent with the failing request’s `seq` and `type=ERROR` (`0x00000003`).

**Payload** (4 + M bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `code` | Error code (`uint32`), see below. |
| 4 | M | `message` | Optional UTF-8 text; `M = length - 4`. Not NUL-terminated. `M` may be 0. |

**Error codes (v1):**

| Code | Name | Meaning |
|------|------|---------|
| 1 | `E_UNKNOWN_TYPE` | Request `type` not recognized. |
| 2 | `E_BAD_LENGTH` | Payload length invalid for this type, or exceeds `max_payload`. |
| 3 | `E_BAD_VERSION` | `HELLO` version not supported. |
| 4 | `E_NOT_HANDSHAKED` | Command before successful `HELLO`. |
| 5 | `E_BUSY` | Previous request still outstanding (if single-flight). |
| 6 | `E_INTERNAL` | Unspecified server failure. |

### `EVENT` — main 0, sub 4 (server → client only)

Unsolicited notification (breakpoint hit, run-state change, etc.). Not a reply to a request. Server chooses `seq` for client-side routing.

**Payload** (4 + M bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `event_id` | Event kind (`uint32`). |
| 4 | M | `data` | Event-specific bytes; `M = length - 4`. May be 0. |

No `event_id` values are defined in v1. Concrete ids and `data` layouts will be added when those notifications are implemented. Clients should ignore unknown `event_id`s.

### Execution control (`main == 1`)

Commands in this family are executed on the **main emulation thread** (via a request/reply bridge from the protocol driver). They must not be answered solely on the protocol thread.

#### `GET_STATUS` — main 1, sub 1 (`0x00000101`)

Read-only snapshot of run-control and platform identity.

**Request payload:** empty (`length == 0`). Requires successful `HELLO`.

**Success reply** (same `type=GET_STATUS`, echoed `seq`), payload (8 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `execution_mode` | `uint32` matching emulator `execution_modes_t`: `0=NORMAL`, `1=STEP_INTO`, `2=PAUSED`. |
| 4 | 4 | `platform_id` | `uint32` matching `PlatformId_t` / CLI `-p N`: `0=II`, `1=II Plus`, `2=IIe`, `3=IIe Enhanced`, `4=IIe 65816`, `5=IIgs`. `0xFFFFFFFF` if unknown. |

If no machine (`computer_t`) is available yet, server replies `ERROR` with `E_INTERNAL` and message `no machine`.

#### `RESET` — main 1, sub 2 (`0x00000102`)

Invoke `computer_t::reset(cold_start)` on the main thread.

**Request payload** (4 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `cold_start` | `uint32`: `0` = warm reset, `1` = cold start (clears `$3F2`–`$3F4` before reset). |

**Success reply** (same `type=RESET`, echoed `seq`): empty payload.

**Bounds:** handshake required; payload exactly 4 bytes; `cold_start` must be `0` or `1`; no machine → `E_INTERNAL` / `no machine`.

#### `PAUSE` — main 1, sub 3 (`0x00000103`)

Enter `EXEC_PAUSED`. Emits `EVT_STOPPED` (`STOP_PAUSE`) and `EVT_RUN_STATE`.

**Request payload:** empty. **Success reply:** empty.

#### `CONTINUE` — main 1, sub 4 (`0x00000104`)

Resume `EXEC_NORMAL` from pause / step. Emits `EVT_RUN_STATE`. Applies Policy A when leaving an `EXEC` / step stop (see breakpoint semantics).

**Request payload:** empty. **Success reply:** empty.

#### `STEP_INTO` — main 1, sub 5 (`0x00000105`)

Arm the existing emulator step path: set `execution_mode = EXEC_STEP_INTO` and `instructions_left = count`. The next emulation frame runs `count` instructions (same as the built-in debugger’s step-into), then idles in `EXEC_STEP_INTO` with `instructions_left == 0`.

**Request payload** (4 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `count` | `uint32` instruction count (`instructions_left`). Must be `>= 1`. |

**Success reply** (same `type=STEP_INTO`, echoed `seq`): empty payload. The request only arms the step; it does **not** wait for completion.

**On completion** (after the instruction batch finishes on the main thread): unsolicited `EVENT` `EVT_STOPPED` with `reason = STOP_STEP` and the post-instruction `system_trace_entry_t` snapshot (same 72-byte layout as breakpoint stops). Also emits `EVT_RUN_STATE` when entering `EXEC_STEP_INTO` from another mode.

**Bounds:** handshake required; payload exactly 4 bytes; `count == 0` → `E_BAD_LENGTH` / `STEP_INTO count must be >= 1`; no machine → `E_INTERNAL` / `no machine`.

Breakpoint checks are **not** performed while executing the step batch (same as UI step-into).

### CPU / trace (`main == 2`)

Commands in this family run on the **main emulation thread**.

#### `GET_TRACE` — main 2, sub 1 (`0x00000201`)

Read a window from the CPU instruction trace ring buffer (`cpu->trace_buffer`, capacity 100000, 40-byte `system_trace_entry_t` records — same layout as `EVT_STOPPED.trace`).

**Request payload** (8 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `ago` | `uint32`: how many instructions **before the newest** completed entry to place the window’s newest end. `0` = most recent entry. |
| 4 | 4 | `count` | `uint32`: number of records to return, extending **into the past** from that end. Must be `>= 1` and `<= 16384`. |

**Window:** logical indices `[newest − ago − count + 1, newest − ago]` (inclusive), clamped to what exists. If `ago >= available`, `returned = 0`. Empty ring → `returned = 0`.

**Success reply** (same `type=GET_TRACE`, echoed `seq`):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `available` | Total entries currently in the ring (`uint32`). |
| 4 | 4 | `returned` | `N` — may be less than requested `count` (`uint32`). |
| 8 | `N × 40` | entries | Packed `system_trace_entry_t` blobs, **oldest → newest**. |

Allowed anytime (like `READMEM`). Snapshot is consistent for that bridge call; while the guest is running, `head` may advance between calls. Host pages older history with a larger `ago`.

**Bounds:** handshake required; payload exactly 8 bytes; `count == 0` or `count > 16384` → `E_BAD_LENGTH`; no CPU / trace buffer → `E_INTERNAL` / `no machine`.

### Memory (`main == 3`)

Commands in this family run on the **main emulation thread**.

#### `READMEM` — main 3, sub 1 (`0x00000301`)

Peek bytes from a memory domain.

**Request payload** (12 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `domain` | Memory domain (`uint32`), see below. |
| 4 | 4 | `address` | Base address (`uint32`), little-endian. |
| 8 | 4 | `length` | Byte count (`uint32`). Must be > 0. |

**Success reply** (same `type=READMEM`, echoed `seq`): exactly `length` raw data bytes.

**Domains:**

| Value | Name | Source | Status |
|-------|------|--------|--------|
| 0 | `MAIN` | CPU view: `computer->cpu->mmu->read(addr)` (II/IIe MMU, or IIgs FPI / banked MMU) | Implemented |
| 1 | `MEGAII` | Mega II / IIe-view MMU: `computer->mmu->read(addr)` | Implemented (Apple IIgs only) |
| 2 | `ENSONIQ` | DOC RAM (`ensoniq_state_t::doc_ram`), address = DOC offset `0`–`0xFFFF` | Implemented (Apple IIgs only) |
| 3 | `ADBMICRO` | ADB microcontroller memory | Reserved |
| 4 | `MAIN_RAW` | Physical RAM: `cpu->mmu->get_memory_base()[addr]` | Implemented |
| 5 | `MEGAII_RAW` | Physical Mega II RAM: `computer->mmu->get_memory_base()[addr]` | Implemented (Apple IIgs only) |

On IIgs, `computer->mmu` is Mega II while `cpu->mmu` is the FPI; `MAIN` / `MAIN_RAW` use the CPU MMU. Addresses for `MEGAII` are passed through as-is (typically `0x0000`–`0xFFFF`, e.g. text `$0400`).

`MAIN_RAW` / `MEGAII_RAW` index the contiguous RAM allocation (not the page table, not bus `read`/`write`):

- II / II+: ~48 KB
- IIe / Mega II: 128 KB (main at `[0..]`, aux at `[0x10000..]`)
- IIgs FPI (`MAIN_RAW`): 8 MB (`bank * 0x10000 + offset` for banks 0–127)

**Bounds:**

- Request payload must be exactly 12 bytes; otherwise `E_BAD_LENGTH`.
- `length == 0` → `E_BAD_LENGTH`.
- `length` capped at **65536** (and never above frame `max_payload`).
- Reject if `address + length` wraps `uint32`.
- `MAIN_RAW` / `MEGAII_RAW`: reject if `address + length` exceeds `get_memory_size()` → `E_BAD_LENGTH` / `out of range`.
- Unimplemented domain → `E_INTERNAL` with message `unsupported domain`.
- `MEGAII` / `MEGAII_RAW` on a non-IIgs platform → `E_INTERNAL` / `MEGAII only on Apple IIgs`.
- No machine / no MMU for the domain → `E_INTERNAL` / `no machine`.

Unmapped addresses still succeed for `MAIN` / `MEGAII`: MMU `read()` returns floating-bus data as usual.

#### `WRITEMEM` — main 3, sub 2 (`0x00000302`)

Poke bytes into a memory domain.

**Request payload** (`12 + length` bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `domain` | Memory domain (`uint32`), same table as READMEM. |
| 4 | 4 | `address` | Base address (`uint32`), little-endian. |
| 8 | 4 | `length` | Byte count (`uint32`). Must be > 0. |
| 12 | `length` | `data` | Raw bytes to write. |

**Success reply** (same `type=WRITEMEM`, echoed `seq`): empty payload.

**Domains:** same as READMEM. `MAIN`, `MAIN_RAW`, and `MEGAII` / `MEGAII_RAW` (IIgs only) are implemented.

**Bounds:**

- Frame payload length must be exactly `12 + length`; otherwise `E_BAD_LENGTH`.
- `length == 0` → `E_BAD_LENGTH`.
- `length` capped at **65536** (and never above frame `max_payload`).
- Reject if `address + length` wraps `uint32`.
- `MAIN_RAW` / `MEGAII_RAW`: reject if `address + length` exceeds `get_memory_size()` → `E_BAD_LENGTH` / `out of range`.
- Unimplemented domain → `E_INTERNAL` with message `unsupported domain`.
- `MEGAII` / `MEGAII_RAW` on a non-IIgs platform → `E_INTERNAL` / `MEGAII only on Apple IIgs`.
- `ENSONIQ` on a non-IIgs platform or with no Ensoniq → `E_INTERNAL` (`unsupported domain` / `no ensoniq`).
- `ENSONIQ`: reject if `address + length` exceeds `0x10000` → `E_BAD_LENGTH`.
- No machine / no MMU for the domain → `E_INTERNAL` / `no machine`.

`ENSONIQ` peeks/pokes DOC RAM with raw `memcpy` (no Sound GLU side effects).

### Input (`main == 5`)

#### `KEYEVENT` — main 5, sub 1 (`0x00000501`)

Inject one SDL keyboard event into the emulator event queue (`SDL_PushEvent`). Handled on the **protocol thread** (`SDL_PushEvent` is thread-safe). II/e keyboard and IIgs KeyGloo see the same events as a real keypress.

**Request payload** (12 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `down` | `1` = key down, `0` = key up. |
| 4 | 4 | `scancode` | `SDL_Scancode` (`uint32`). |
| 8 | 4 | `mod` | `SDL_Keymod` flags for this event (`uint32`). |

**Success reply** (same `type=KEYEVENT`, echoed `seq`): empty payload.

Server fills `event.key.key` via `SDL_GetKeyFromScancode(scancode, mod, false)`, sets `repeat=false`, and pushes `SDL_EVENT_KEY_DOWN` or `SDL_EVENT_KEY_UP`.

**Bounds:**

- Handshake required; payload exactly 12 bytes.
- `down` must be `0` or `1`; otherwise `E_BAD_LENGTH`.
- `SDL_PushEvent` failure → `E_INTERNAL`.

Clients must set `mod` to the desired modifier mask **on that event**. Control-Reset on macOS/Windows: Control key-down, then F12 key-down with `mod` including `SDL_KMOD_CTRL` (handlers check `event.key.mod` on the Reset key itself).

### Devices (`main == 6`)

Generic device ops. Devices register in-process handlers via `computer_t::register_device_debug(device_id, …)`; the protocol server routes by `device_id` and does not interpret device blobs.

#### `STATE_GET` — main 6, sub 1 (`0x00000601`)

**Request payload** (4 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `device_id` | `uint32` matching `device_id` / `DEVICE_ID_*` (e.g. `DEVICE_ID_ENSONIQ` = 22). |

**Success reply:** opaque device blob (layout owned by the device; versioned).

**Bounds:** handshake required; payload exactly 4 bytes; unknown / unregistered device → `E_INTERNAL` / `unknown device` (or handler message).

##### Ensoniq `STATE_GET` blob (v1) — 784 bytes

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `version` = `1` |
| 4 | 1 | `soundctl` |
| 5 | 1 | `sounddata` |
| 6 | 1 | `soundadrl` |
| 7 | 1 | `soundadrh` |
| 8 | 1 | `rege0` |
| 9 | 1 | `rege1` |
| 10 | 1 | `oscsenabled` |
| 11 | 1 | pad `0` |
| 12 | 4 | `output_rate_hz` |
| 16 | 32 × 24 | oscillators |

Per-oscillator (24 bytes): `freq` u16, `wtsize` u16, `control` u8, `vol` u8, `data` u8, pad, `wavetblpointer` u32, `wavetblsize` u8, `resolution` u8, `irqpend` u8, pad, `accumulator` u32, pad u32.

##### Disk II `STATE_GET` blob (v1) — 60 bytes (`DEVICE_ID_DISK_II` = 9)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `version` = `1` |
| 4 | 1 | `select` (drive 0/1) |
| 5 | 1 | `motor_on` (physical / 555 grace) |
| 6 | 1 | `motor_latch` (`diskii_enable`) |
| 7 | 1 | `q6` |
| 8 | 1 | `q7` |
| 9 | 1 | `data_register` |
| 10 | 1 | `sequencer_state` |
| 11 | 1 | pad `0` |
| 12 | 8 | `mark_cycles_turnoff` (c14m deadline, or 0) |
| 20 | 8 | `cpu_cycles` |
| 28 | 16 | drive 0 |
| 44 | 16 | drive 1 |

Per-drive (16 bytes): `track` i16 (quarter-tracks), `max_tracks` i16, `phase0`…`phase3` u8 each, `enable` u8, `write_protect` u8, `mounted` u8, pad×5.

##### Apple Mouse III `STATE_GET` blob (v1) — 32 bytes (`DEVICE_ID_MOUSE` = 17)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `version` = `1` |
| 4 | 1 | `slot` |
| 5 | 1 | `rom_bank` (0–7) |
| 6 | 1 | `operating_mode` |
| 7 | 1 | `int_state` |
| 8 | 1 | `irq_asserted` |
| 9 | 1 | `button0` |
| 10 | 1 | `button1` |
| 11 | 1 | pad `0` |
| 12 | 2 | `x` (i16) |
| 14 | 2 | `y` (i16) |
| 16 | 2 | `clamp_min_x` (i16) |
| 18 | 2 | `clamp_min_y` (i16) |
| 20 | 2 | `clamp_max_x` (i16) |
| 22 | 2 | `clamp_max_y` (i16) |
| 24 | 1 | PIA `ORA` |
| 25 | 1 | PIA `ORB` |
| 26 | 1 | PIA `DDRA` |
| 27 | 1 | PIA `DDRB` |
| 28 | 1 | PIA `CRA` |
| 29 | 1 | PIA `CRB` |
| 30 | 1 | PIA `IA` |
| 31 | 1 | PIA `IB` |

#### `STATE_SET` — main 6, sub 2 (`0x00000602`)

**Request payload:**

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `device_id` | `uint32` matching `DEVICE_ID_*` |
| 4 | N | blob | device-specific (versioned) |

**Success reply:** empty (or a small device ack blob).

**Bounds:** handshake required; payload at least 4 bytes; unknown / unregistered device → `E_INTERNAL`.

##### AppleMouse III `STATE_SET` blob (v1) — 8 bytes (after `device_id`)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `version` = `1` |
| 4 | 1 | `flags` — bit0 apply relative motion (`dx`/`dy`); bit1 set buttons |
| 5 | 1 | `dx` (i8) |
| 6 | 1 | `dy` (i8) |
| 7 | 1 | `buttons` — bit0 button0, bit1 button1 |

---

## Example exchange

Client connects, then:

1. Request: `type=HELLO`, `seq=1`, `length=8`, payload `version=1`, `flags=0`.
2. Reply: `type=HELLO`, `seq=1`, `length=12`, payload `version=1`, `flags=0`, `max_payload=0x00100000`.
3. Request: `type=GET_STATUS`, `seq=2`, `length=0`.
4. Reply: `type=GET_STATUS`, `seq=2`, `length=8`, payload `execution_mode=0` (NORMAL), `platform_id=…`.
5. Request: `type=READMEM`, `seq=3`, `length=12`, payload `domain=MAIN`, `address=0x0400`, `length=0x28`.
6. Reply: `type=READMEM`, `seq=3`, `length=0x28`, payload = 40 memory bytes.
7. Request: `type=WRITEMEM`, `seq=4`, `length=12+0x28`, payload `domain=MAIN`, `address=0x0400`, `length=0x28`, then 40 data bytes.
8. Reply: `type=WRITEMEM`, `seq=4`, `length=0`.
9. Request: `type=PING`, `seq=5`, `length=0`.
10. Reply: `type=PING`, `seq=5`, `length=0`.

---

## Future commands

Main numbers 1–6 are reserved for execution, CPU, memory, breakpoints, input, and devices (up to 256 subs each). Beyond documented commands, any type outside the documented set yields `ERROR` with `E_UNKNOWN_TYPE`.

---

# Breakpoint semantics reference

The following section documents breakpoint / watchpoint behavior for the implemented `main == 4` commands and related events.

---

## Breakpoints and watchpoints (`main == 4`)

Status: **implemented** (commands listed under Initial command set). This section is the semantic reference.

### Design principles

1. **GS2 is a typed stop engine.** Match address / range / access class / optional fixed value / ignore-count / **address mask**. No expression language, symbols, or source lines inside the emulator.
2. **Host evaluates fancy conditions.** On `EVENT`, the client may `READMEM` / (future) read regs and `CONTINUE` if the stop is uninteresting. Thrashing is mitigated with ignore-count and temporary breakpoints, not with host round-trips on every instruction.
3. **Stop reason is an unsolicited `EVENT`.** Setting a breakpoint is a request/reply; hitting it is never a reply to `CONTINUE`.
4. **Same memory domains as `READMEM` / `WRITEMEM`.** Watchpoints name a domain explicitly (IIgs FPI vs Mega II vs raw buffers). Address validity and domain errors follow the same rules as memory ops (see Errors below).
5. **Opaque breakpoint IDs.** Clients address entries by `id` returned from `BP_SET`, not by “remove this address,” so overlapping ranges and temporary BPs do not collide.
6. **Step helpers stay under `main == 1`.** Step-into / over / out and “run to address” may install internal temporary stops, but they are execution-control ops; user breakpoints live under `main == 4`.
7. **`RESET` does not clear breakpoints.** Warm/cold reset leaves the breakpoint table intact. Clients that want a clean slate call `BP_CLEAR_ALL` (or clear by `id`).

### Built-in debugger today (reference, not wire)

The in-window debugger is roughly:

- **Pre-instruction:** if `(full_pc & 0xFFFF)` is in a breakpoint range → pause.
- **Post-instruction:** if `(eaddr & 0xFFFF)` is in a breakpoint range → pause.
- Step-over via a one-shot PC; step-out via RTS/RTL opcode check.
- No R vs W distinction; no banked PC on the wire; no remote stop-reason payload.

The draft below is the remote semantics we want, not a 1:1 export of that UI.

### Shared stop list (**decided: shared**)

**Decision:** one process-wide breakpoint table. Monitor `bp` / the built-in UI and protocol `BP_*` read and write the same entries. Pre/post checks consult that one table whenever any breakpoint is armed.

Tradeoff accepted: UI and remote client can interfere (`BP_CLEAR_ALL` wipes interactive breakpoints; either side can enable/disable/clear by `id` or address once the UI grows ids). That is preferable for now to maintaining two lists with divergent address/mask rules. Connection drop does **not** clear the table (same as `RESET`). If interference becomes painful in practice, split later without changing match semantics.

Background (why not protocol-only): a separate socket list avoids clobbering the UI but duplicates the stop engine and invites the old “mask to 16 bits on one path only” class of bugs.

### Kinds

| Kind | Value | When checked | Trigger |
|------|-------|--------------|---------|
| `EXEC` | `1` | Before the instruction at PC runs (pre) | Masked PC in range (see Address match) |
| `DATA` | `2` | After the access (post), when effective address is known | Masked access address in range, filtered by access flags |
| `IO` | `3` | Same as `DATA` (post) | Soft-switch / I/O space: offset in range **and** bank ∈ `{0x00, 0x01, 0xE0, 0xE1}` (see I/O kind). Not “every bank’s `$C0xx`.” |

Optional later kinds (out of scope until needed): IRQ/NMI entry, tracepoints that do not stop.

**Why `IO` is not `DATA` + `addr_mask`:** `addr_mask = 0x0000FFFF` would match `$C0xx` in **every** bank (`$02C030`, `$80C000`, …), which is far broader than real Apple II / IIgs I/O mirrors and fires on noise. Without a general expression language, the fixed bank set `{00,01,E0,E1}` belongs in a dedicated kind. Keep `addr_mask` on `EXEC` / `DATA` for other uses.

### Flags and fields (logical model)

Each breakpoint / watchpoint entry:

| Field | Type | Meaning |
|-------|------|---------|
| `id` | `uint32` | Server-assigned; unique until cleared. `0` reserved / invalid. |
| `kind` | `uint8` | `1=EXEC`, `2=DATA`, `3=IO`. |
| `flags` | `uint8` | Bitfield, see below. |
| `domain` | `uint32` | Same domain table as `READMEM`. For `EXEC` / `DATA` / `IO`, typically `MAIN` (CPU view). |
| `address` | `uint32` | Base address. `EXEC`: full PC. `DATA`: domain address. `IO`: offset base in `$C000`–`$C0FF` (e.g. `0xC000` or `0x00C000`; bank bits in `address` are ignored — bank filter is fixed). |
| `length` | `uint32` | Byte span; `1` = single location. `length == 0` invalid. For `IO`, span is on the **16-bit offset**. |
| `addr_mask` | `uint32` | Bits participating in the address compare for `EXEC` / `DATA`. `0xFFFFFFFF` = all bits (default). **Ignored for `IO`** (bank whitelist + offset range replace mask). |
| `access` | `uint8` | `DATA` / `IO`: `1=R`, `2=W`, `3=RW`. Ignored for `EXEC`. |
| `data_value` | `uint32` | Optional; meaningful when `FLAG_DATA_MATCH` set (`DATA` / `IO`). |
| `data_mask` | `uint32` | With `FLAG_DATA_MATCH`: low-byte match (see Data-match rules). |
| `ignore_count` | `uint32` | Skip this many hits before pausing; decremented on each match that would otherwise stop. `0` = stop on first hit. |
| `hit_count` | `uint32` | (List / event only) times matched since set; informational. |

**`flags` bits:**

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `FLAG_ENABLED` | Cleared = retained but inactive. |
| 1 | `FLAG_TEMPORARY` | Auto-clear after the hit that causes a pause. |
| 2 | `FLAG_DATA_MATCH` | `DATA` / `IO`: require value match (`data_value` / `data_mask`). |
| 3–7 | reserved | Must be 0 until defined. |

### Address match (range + mask — not an expression language)

Applies to **`EXEC` and `DATA`**. (`IO` has its own rule below.)

Still a fixed bitwise filter, in the same spirit as `data_mask`: no predicates, no OR of arbitrary addresses, no host-side callback per access.

For an observed address `A` (full PC for `EXEC`, domain effective address for `DATA`):

```
masked_A    = A & addr_mask
masked_base = address & addr_mask
hit         = (masked_base <= masked_A) && (masked_A < masked_base + length)
```

The range is a **half-open** interval on the masked address space: `[masked_base, masked_base + length)`. `length == 0` is empty and rejected at `BP_SET` (`E_BAD_LENGTH`). `length == 1` matches a single masked location; larger `length` spans consecutive masked addresses.

Then apply `access` / `FLAG_DATA_MATCH` / `ignore_count` as usual.

**Which bits participate is entirely `addr_mask`.** Bits cleared in the mask are ignored in both `A` and `address`. Bits set in the mask must agree (within the length window).

Concrete case — break only at bank 0 offset 0 (`$00/0000`), not at `$04/0000`:

| | Value |
|--|-------|
| `address` | `0x00000000` (`$00/0000`) |
| `length` | `1` |
| `addr_mask` | `0xFFFFFFFF` (full; bank bits **included**) |

| Observed `A` | `A & mask` | Hit? |
|--------------|------------|------|
| `$00/0000` (`0x00000000`) | `0` | yes — `masked_base == 0`, window `[0, 1)` |
| `$04/0000` (`0x00040000`) | `0x00040000` | **no** — not equal to `0` |

Same `address` / `length`, but mask `$00/FFFF` (`0x0000FFFF`, bank bits **cleared**):

| Observed `A` | `A & mask` | Hit? |
|--------------|------------|------|
| `$00/0000` | `0` | yes |
| `$04/0000` | `0` | **yes** — bank ignored; both collapse to offset `$0000` |

`addr_mask = 0x0000FFFF` is useful when you **intentionally** want the same offset in every bank. It is the **wrong** tool for soft-switches (too broad — see `IO` kind).

**`masked_base == 0` is not “no breakpoint”.** For `address = $00/0000`, `address & addr_mask` is often `0`. That zero is the compare key for a real location. Implementations must not treat `(address & addr_mask) == 0` or `(A & addr_mask) == 0` as failure / unset. (Breakpoint **`id` `0`** is a different namespace — reserved/invalid. **`length == 0`** is the empty/invalid span.)

Examples (`EXEC` / `DATA`):

| Want | `address` | `length` | `addr_mask` | `$00/0000` | `$04/0000` |
|------|-----------|----------|-------------|------------|------------|
| Only `$00/0000` | `0x00000000` | `1` | `0xFFFFFFFF` | hit | miss |
| Offset `$0000`, any bank | `0x00000000` | `1` | `0x0000FFFF` | hit | hit |

| `addr_mask` | Effect |
|-------------|--------|
| `0xFFFFFFFF` | Full compare; bank matters. Default. |
| `0x0000FFFF` (`$00/FFFF`) | Ignore bank. Same offset in every bank matches. |

**Why not “just truncate to 16 bits always”?** That is what the built-in debugger does today, and it is wrong for `EXEC` on IIgs (bank matters for code). Mask defaults to full-width; clients opt into ignore-bank only when they mean it.

### I/O kind (`BP_KIND_IO`)

Apple II / IIgs soft-switches live at offsets `$C000`–`$C0FF` and are mirrored in a **small** set of banks, not in all 256 banks.

**Bank whitelist (fixed in the protocol):** `0x00`, `0x01`, `0xE0`, `0xE1`.

For observed effective address `A`:

```
bank   = (A >> 16) & 0xFF
offset = A & 0xFFFF
base   = address & 0xFFFF          // bank nibble in `address` ignored
if bank not in {0x00, 0x01, 0xE0, 0xE1}:
    miss
else:
    hit = (base <= offset) && (offset < base + length)
```

Then apply `access` / `FLAG_DATA_MATCH` / `ignore_count` as usual. `addr_mask` is **ignored** (clients should send `0xFFFFFFFF`).

On 8-bit machines (no bank byte), treat `bank` as `0x00` so `IO` still matches `$C0xx` accesses.

**Bounds:** `base + length` must not wrap the 16-bit offset space; prefer requiring the watch to lie within `$C000`–`$C0FF` (`base >= 0xC000` and `base + length <= 0xC100`) so “I/O” cannot be used as a sneaky any-bank RAM watch — reject otherwise with `E_BAD_LENGTH`.

Example — whole soft-switch page in the real mirror banks only:

| Field | Value |
|-------|-------|
| `kind` | `IO` |
| `domain` | `MAIN` |
| `address` | `0xC000` |
| `length` | `0x100` |
| `addr_mask` | `0xFFFFFFFF` (ignored) |
| `access` | `RW` (or `R` / `W`) |

| Observed `A` | Hit? |
|--------------|------|
| `$00/C030` | yes |
| `$E0/C000` | yes |
| `$02/C030` | **no** (bank not in whitelist) |
| `$00/D000` | **no** (offset outside range) |

Narrower watches (e.g. only `$C030`) use `address = 0xC030`, `length = 1`.

**Data-match rules (**decided: byte only**):**

- Only for `DATA` / `IO` with `FLAG_DATA_MATCH`.
- The emulated data path is treated as **8-bit** for watchpoint purposes (6502 and 65816). Match one byte: `(observed_byte & (data_mask & 0xFF)) == ((data_value & 0xFF) & (data_mask & 0xFF))`. High bytes of `data_value` / `data_mask` on the wire are ignored (send `0`).
- Multi-byte 65816 transfers are not a special case: if both bytes of a word access should be watched, use a `length` covering both addresses (or two watches). No page-wrap word semantics.

**Address / domain rules (draft):**

- Prefer **full PC** for `EXEC` on 65816 / IIgs (`0xE10000` style), not truncated `$xxxx`.
- `DATA` addresses follow the same domain conventions as `READMEM`.
- `IO` compares 16-bit offset + bank whitelist as above.
- Cap `length` like memory ops (**65536**), except `IO` which is capped by the `$C000`–`$C0FF` window.

### Scale and performance (guidance)

Checks run on the **hot path** (pre-PC and post-`eaddr` once per instruction) whenever any breakpoint is armed — same cost class as today’s debug-window loop.

Rough budget (order-of-magnitude, not a guarantee):

| Active entries (linear scan) | Expectation |
|------------------------------|-------------|
| `0` | Free: skip checks entirely (required fast path). |
| tens (`≤ ~32–64`) | Negligible vs `execute_next` on a modern host at 1 MHz–class emulation. |
| low hundreds (`~128–256`) | Usually fine; may show up if the debug path is already heavy (UI + ludicrous speed). |
| thousands | Noticeable: prefer a denser structure or raise the cost deliberately for “debug build” use. |

**Draft protocol cap:** **256** armed entries (`BP_SET` → `E_BAD_LENGTH` / message `too many breakpoints` when exceeded). Enough for agents and UI; keeps worst-case linear scan bounded. Revisit if real workloads need more.

**Alternate structures** (implementation, not wire):

| Structure | Good for | Weak for |
|-----------|----------|----------|
| `vector` of ranges (start with this) | Few BPs; arbitrary `length` + `addr_mask`; simple | Large N |
| Bitset / byte map (e.g. 64 KiB = 8 KiB RAM per bankless 16-bit space; or per-bank maps) | Dense exact addresses, full `addr_mask` | Ranges; ignore-bank masks (unless OR’d carefully); 24-bit full maps are large (~2 MiB/bit-per-byte) |
| Page map (e.g. bit per 256-byte page) + vector of BPs in hot pages | Many scattered exact BPs | Still need list walk inside a hot page |
| Hash set of exact addresses | Many single-byte full-mask BPs | Ranges and masks |

Practical approach: **vector + empty fast-out** for v1; add a 16-bit or page bitmap later if profiling says N hurts. Masked / ranged / `IO` watches stay on the vector (or a short “slow list”) even if exact BPs move to a map.

### Errors

Do **not** invent breakpoint-specific `E_*` codes when an existing code already matches the failure class. Align with `READMEM` / `WRITEMEM`:

| Situation | Error |
|-----------|-------|
| Wrong payload size; `length == 0`; `address + length` wraps; raw domain out of `get_memory_size()`; more than **256** breakpoints; `IO` range outside `$C000`–`$C0FF` | `E_BAD_LENGTH` (same messages as memory ops where applicable, e.g. `out of range`; cap → `too many breakpoints`) |
| Unknown / unimplemented `domain`; Mega II on non-IIgs; no machine | `E_INTERNAL` + same messages as memory ops (`unsupported domain`, `MEGAII only on Apple IIgs`, `no machine`, …) |
| Bad `kind` / `access` / reserved flag bits | `E_BAD_LENGTH` if it is a payload-validity issue; else `E_INTERNAL` + short message |
| Unknown `id` on clear/enable | `E_INTERNAL` + message such as `unknown id` (no new code) |
| Handshake / busy / unknown type | Existing `E_NOT_HANDSHAKED` / `E_BUSY` / `E_UNKNOWN_TYPE` |

Setting a breakpoint on an address that would be rejected for a peek/poke in that domain must fail the same way as that peek/poke. Domains that accept floating-bus reads for unmapped addresses (e.g. `MAIN`) likewise accept breakpoints there.

### Proposed commands (`main == 4`)

| Name | `sub` | `type` | Purpose |
|------|-------|--------|---------|
| `BP_SET` | 1 | `0x00000401` | Create; reply returns `id` |
| `BP_CLEAR` | 2 | `0x00000402` | Delete by `id` |
| `BP_CLEAR_ALL` | 3 | `0x00000403` | Delete all user breakpoints / watchpoints |
| `BP_ENABLE` | 4 | `0x00000404` | Set/clear `FLAG_ENABLED` by `id` |
| `BP_LIST` | 5 | `0x00000405` | Snapshot of current entries |

All run on the **main emulation thread**. Handshake required.

#### Constants

| Name | Value |
|------|-------|
| `BP_KIND_EXEC` | `1` |
| `BP_KIND_DATA` | `2` |
| `BP_KIND_IO` | `3` |
| `BP_ACCESS_NONE` | `0` (EXEC) |
| `BP_ACCESS_R` | `1` |
| `BP_ACCESS_W` | `2` |
| `BP_ACCESS_RW` | `3` |
| `BP_FLAG_ENABLED` | `1 << 0` |
| `BP_FLAG_TEMPORARY` | `1 << 1` |
| `BP_FLAG_DATA_MATCH` | `1 << 2` |
| `BP_MAX_ENTRIES` | `256` |
| `BP_IO_BANKS` | `0x00`, `0x01`, `0xE0`, `0xE1` (fixed whitelist) |

#### `BP_SET` — main 4, sub 1 (`0x00000401`)

**Request payload:** exactly **32** bytes, little-endian, packed:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `kind` | `BP_KIND_EXEC`, `BP_KIND_DATA`, or `BP_KIND_IO` |
| 1 | 1 | `flags` | `BP_FLAG_*` (unknown bits → error) |
| 2 | 1 | `access` | `DATA` / `IO`: `R`/`W`/`RW`; `EXEC`: `0` |
| 3 | 1 | `pad` | `0` |
| 4 | 4 | `domain` | Same as `READMEM` |
| 8 | 4 | `address` | Base (full PC, domain address, or I/O offset) |
| 12 | 4 | `length` | Byte span; `≥ 1` (`IO`: within `$C000`–`$C0FF`) |
| 16 | 4 | `addr_mask` | Default `0xFFFFFFFF`; **ignored for `IO`** |
| 20 | 4 | `data_value` | Low 8 bits used if `FLAG_DATA_MATCH`; else ignored |
| 24 | 4 | `data_mask` | Low 8 bits used if `FLAG_DATA_MATCH`; else ignored (`0xFF` = compare all value bits) |
| 28 | 4 | `ignore_count` | `0` = stop on first hit |

**Success reply:** exactly **4** bytes: `id` (`uint32`, non-zero).

#### `BP_CLEAR` — main 4, sub 2 (`0x00000402`)

**Request:** exactly **4** bytes: `id`.

**Success reply:** empty.

#### `BP_CLEAR_ALL` — main 4, sub 3 (`0x00000403`)

**Request / reply:** empty. Does not run on `RESET`. Clears the shared table (UI and protocol).

#### `BP_ENABLE` — main 4, sub 4 (`0x00000404`)

**Request:** exactly **8** bytes:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `id` |
| 4 | 4 | `enabled` (`0` or `1`) |

**Success reply:** empty. Sets/clears `BP_FLAG_ENABLED` only.

#### `BP_LIST` — main 4, sub 5 (`0x00000405`)

**Request:** empty.

**Success reply:** `4 + 40 * count` bytes:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `count` (`uint32`) |
| 4 | `40 * count` | records |

Each **record** is exactly **40** bytes:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | `id` |
| 4 | 4 | `hit_count` |
| 8 | 32 | same layout as `BP_SET` request (`kind`…`ignore_count`) |

### Events

Unsolicited `EVENT` frames. Clients ignore unknown `event_id`s.

| `event_id` | Name | When |
|------------|------|------|
| `1` | `EVT_STOPPED` | Entered a stopped/paused state (breakpoint, step done, explicit `PAUSE`, …) |
| `2` | `EVT_RUN_STATE` | `execution_mode` changed, including **resume / started running** after `CONTINUE` / run, and transitions into `STEP_*` if useful to the client |

#### `EVT_STOPPED` (`event_id = 1`)

**`data` layout:** **32**-byte header + **40**-byte CPU snapshot (`system_trace_entry_t` wire image) = **72** bytes total.

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `reason` | See table below |
| 4 | 4 | `bp_id` | User bp `id`, or `0` if not a user bp |
| 8 | 4 | `pc` | Full PC at stop (real address; may be `0`) |
| 12 | 4 | `eaddr` | Unmasked effective address for `DATA`; unused for pure `EXEC` — do not use `0` as N/A sentinel; use `reason`/`kind` |
| 16 | 4 | `value` | Observed **byte** for `DATA` when available (low 8 bits); else `0` |
| 20 | 1 | `access` | `R`/`W`/`0` |
| 21 | 1 | `kind` | `EXEC`/`DATA`/`IO`/`0` |
| 22 | 2 | `pad` | `0` |
| 24 | 4 | `execution_mode` | Mode after stop (expect paused / step) |
| 28 | 4 | `trace_size` | Size of following snapshot in bytes; **`40`** in this draft. `0` = no snapshot appended (compat) |
| 32 | 40 | `trace` | CPU / instruction snapshot (see below) |

**`reason` values:**

| Value | Name | Meaning |
|-------|------|---------|
| 1 | `STOP_BP_EXEC` | User `EXEC` breakpoint |
| 2 | `STOP_BP_DATA` | User `DATA` watchpoint |
| 3 | `STOP_BP_IO` | User `IO` (soft-switch) watchpoint |
| 4 | `STOP_STEP` | Step-into / over / out completed |
| 5 | `STOP_PAUSE` | Explicit `PAUSE` from host |

**Including a trace / CPU snapshot — yes, valuable.** At a stop the host almost always wants registers, opcode, effective address, and data byte without a racey follow-up `READMEM` / future `GET_REGS`. GS2 already fills `cpu->trace_entry` on the instruction path; copying that blob into the event is cheap vs socket I/O.

Wire image matches `system_trace_entry_t` (**40** bytes, little-endian, natural C layout / `sizeof == 40` today):

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | `cycle` |
| 8 | 4 | `operand` |
| 12 | 1 | `opcode` |
| 13 | 1 | `p` |
| 14 | 1 | `db` |
| 15 | 1 | `pb` |
| 16 | 2 | `pc` |
| 18 | 2 | `a` |
| 20 | 2 | `x` |
| 22 | 2 | `y` |
| 24 | 2 | `sp` |
| 26 | 2 | `d` |
| 28 | 2 | `data` |
| 30 | 2 | `(pad for align)` — must be `0` on the wire if the in-memory struct has padding here; prefer documenting the packed offsets clients use |
| 32 | 4 | `eaddr` |
| 36 | 2 | `flags` (`f_irq`, `f_op_sz`, `f_data_sz` in low bits as today) |
| 38 | 2 | `unused` |

*(If in-memory padding ever drifts, the protocol freeze is this 40-byte map, not a blind `memcpy` of a future struct — implementers should serialize field-by-field or `static_assert(sizeof == 40)` against this layout.)*

**Population rules:**

| `reason` | Snapshot content |
|----------|------------------|
| `STOP_BP_DATA`, `STOP_BP_IO`, `STOP_STEP` (after insn) | Copy the just-completed `trace_entry` (regs **before** that insn, `eaddr`/`data` for that access) — ideal fit. |
| `STOP_BP_EXEC` | Pre-instruction stop: fill from **live** CPU state at the about-to-execute PC (`pb`/`pc`/`a`/…); `opcode`/`operand` may be peeked from memory or left 0; `eaddr`/`data` typically unused. Do **not** pass off the *previous* instruction’s `trace_entry` as the current stop without labeling — prefer a live snapshot. |
| `STOP_PAUSE` | Live CPU snapshot at pause. |

Header `pc` / `eaddr` / `value` remain for quick filtering; `trace` is the authoritative register picture.

#### `EVT_RUN_STATE` (`event_id = 2`)

Emitted when execution leaves or enters run-like states so clients can sync UI / agents without polling `GET_STATUS`. Includes **started running** after `CONTINUE` / run.

**`data` layout:** **8** bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `execution_mode` | New mode (`NORMAL`, `STEP_INTO`, `PAUSED`, …) |
| 4 | 4 | `prev_execution_mode` | Previous mode |

Emit at least on: pause→run (`NORMAL`), run→pause, and step-mode transitions. Exact set can be tightened when `CONTINUE` is specified.

### Execution control dependency (`main == 1`)

Breakpoints assume these exist (names provisional; not specified in full here):

| Command | Role |
|---------|------|
| `PAUSE` | Enter paused; `EVT_STOPPED` / `EVT_RUN_STATE` |
| `CONTINUE` / `RUN` | Leave pause; `EVT_RUN_STATE` (started); arm checks again |
| `STEP_INTO` | Run `count` instructions via `instructions_left`; `EVT_STOPPED` with `STOP_STEP` + trace when the batch finishes |

While paused, breakpoint checks do not run.

### Re-hit / “step off” policy (**decided: Policy A**)

**Problem.** Suppose an `EXEC` breakpoint is armed at PC=`P`, execution stops there (`EVT_STOPPED`, still sitting at `P`), and the host sends `CONTINUE` without clearing the bp. The next pre-check sees PC=`P` again and pauses immediately. No instruction retires; the machine makes no progress.

**Policy A (required).** When leaving pause after an `EXEC` stop at `P`, the stop engine ignores `EXEC` matches for address `P` until either:

1. PC becomes something other than `P` (normal case after the instruction at `P` runs), or
2. One instruction at `P` has retired (equivalent for straight-line code),

whichever the implementation can do cheaply. Then the breakpoint is armed again, so a loop that returns to `P` still stops on the next visit. (`P` may be `0` — same rules; zero is a normal PC.)

This is “step off the breakpoint” / continue-from-breakpoint. It applies to `EXEC` (and to step helpers that land on a user `EXEC` bp). It does **not** suppress `DATA` / `IO` watches. Spell the same text under the resume command when that command is specified. Temporary (`FLAG_TEMPORARY`) breakpoints avoid the issue by deleting themselves on the hit that paused.

### Host vs emulator split

| In GS2 | On the host |
|--------|-------------|
| `EXEC` / `DATA` / **`IO`**, R/W, range, domain, **`addr_mask`** (non-`IO`) | Symbol → address |
| Fixed I/O bank whitelist `{00,01,E0,E1}` | Choosing which soft-switch offsets to watch |
| Temporary, enable/disable, ignore-count | Source line breakpoints |
| Optional fixed **byte** value/mask | Arbitrary predicates (regs, “after boot”, OR of unrelated sites) |
| `EVT_STOPPED` (+ trace snapshot) / `EVT_RUN_STATE` | Re-arm, conditional continue, logging UX |

`IO` is in GS2 because the soft-switch bank set is platform structure, not a general expression. `addr_mask` remains for other aliasing needs; it is not the soft-switch mechanism.

### Implementation notes (non-normative)

- Start with a vector + “count == 0 → skip checks.”
- Soft-switches: `BP_KIND_IO` + offset range in `$C000`–`$C0FF`.
- Serialize `EVT_STOPPED.trace` to the frozen 40-byte map (or `static_assert` on `system_trace_entry_t`).

### Open questions

1. ~~Cap~~ → **decided:** soft expectation tens–low hundreds; **hard cap 256**; vector first, denser structures if needed.
2. ~~Shared vs protocol-only list~~ → **decided: shared.**
3. ~~Wire layouts~~ → **decided:** `BP_SET` 32-byte request; list records 40 bytes; see above.
4. ~~Error codes~~ → **decided:** reuse existing `E_*`.
5. ~~`RESET` clears breakpoints?~~ → **decided: no.**
6. ~~Word / 65816 bus width~~ → **decided:** byte (or byte range) only; data bus treated as 8-bit for watches.
7. ~~Events beyond stop~~ → **decided:** `EVT_RUN_STATE` including started/running after continue.
8. ~~Re-hit policy~~ → **decided: Policy A.**
9. ~~Trace blob on stop?~~ → **decided: yes** — append 40-byte `system_trace_entry_t` image on `EVT_STOPPED`.

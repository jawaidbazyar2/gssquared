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
- Full debug command set (`pause`, …) — session meta plus GET_STATUS / RESET / READMEM / WRITEMEM (MAIN) / KEYEVENT below.

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
5. Meta commands that need no machine state (`HELLO`, `PING`) **may** be answered entirely on the protocol thread so handshake does not depend on the emu loop ticking.

---

## Initial command set

### Type IDs (meta, `main == 0`)

| Name | `main` | `sub` | `type` | Direction |
|------|--------|-------|--------|-----------|
| `HELLO` | 0 | 1 | `0x00000001` | client request; server reply uses same `type` |
| `PING` | 0 | 2 | `0x00000002` | client request; server reply uses same `type` |
| `ERROR` | 0 | 3 | `0x00000003` | server reply only — when a request fails or is unknown |
| `EVENT` | 0 | 4 | `0x00000004` | server → client only — unsolicited notification |

### Type IDs (implemented non-meta)

| Name | `main` | `sub` | `type` | Thread | Reply payload |
|------|--------|-------|--------|--------|---------------|
| `GET_STATUS` | 1 | 1 | `0x00000101` | main | 8 bytes: `execution_mode`, `platform_id` |
| `RESET` | 1 | 2 | `0x00000102` | main | empty |
| `READMEM` | 3 | 1 | `0x00000301` | main | `length` data bytes |
| `WRITEMEM` | 3 | 2 | `0x00000302` | main | empty |
| `KEYEVENT` | 5 | 1 | `0x00000501` | protocol (`SDL_PushEvent`) | empty |

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
| 2 | `ENSONIQ` | DOC / sound RAM | Reserved |
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
- No machine / no MMU for the domain → `E_INTERNAL` / `no machine`.

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

Main numbers 1–6 are reserved for execution, CPU, memory, breakpoints, input, and sound (up to 256 subs each). Beyond documented commands, any type outside the documented set yields `ERROR` with `E_UNKNOWN_TYPE`.

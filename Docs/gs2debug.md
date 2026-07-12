# gs2debug — Python API for agents

How to drive a running GSSquared from Python. Prefer this package over hand-rolled sockets.

Wire format (if needed): [DebugProtocol.md](DebugProtocol.md). Design notes: [DebugClient.md](DebugClient.md).

## Setup

```bash
# Terminal A — emulator (IIe example: -p 2)
./build/GSSquared --debug /tmp/gs2.sock -p 2

# Terminal B — always a separate terminal (Ctrl-C on the script must not SIGINT the emu)
PYTHONPATH=clients/python/src python3 your_script.py /tmp/gs2.sock
```

| `-p N` | Platform |
|--------|----------|
| 0 | Apple II |
| 1 | Apple II Plus |
| 2 | Apple IIe |
| 3 | Apple IIe Enhanced |
| 4 | Apple IIe 65816 |
| 5 | Apple IIgs |

Same integers as `get_status().platform_id` / `PLATFORM_*` constants. Use a distinct socket path per instance if several are running.

```python
from gs2debug import Client, MEM_MAIN, PLATFORM_APPLE_IIE, ProtocolError

with Client() as c:
    c.connect("/tmp/gs2.sock")
    c.hello()          # required before anything else
    status = c.get_status()
    assert status.platform_id == PLATFORM_APPLE_IIE
    # ... use API ...
```

Import path: `clients/python/src` on `PYTHONPATH`, or `pip install -e clients/python`.

## API (what to call)

| Method | Purpose |
|--------|---------|
| `connect(path)` / `close()` | AF_UNIX connect; also context-manager |
| `hello()` → `HelloInfo` | Handshake; sets `version`, `flags`, `max_payload` |
| `ping()` | Liveness; empty reply |
| `get_status()` → `StatusInfo` | `.execution_mode` (`0=NORMAL`, `1=STEP_INTO`, `2=PAUSED`), `.platform_id` (`PlatformId_t` / `-p N`) |
| `reset(cold_start=False)` | `computer_t::reset(cold_start)` on main thread |
| `read_mem(domain, address, length)` → `bytes` | Peek (`MEM_MAIN` / `MEM_MAIN_RAW`; `MEM_MEGAII` / `MEM_MEGAII_RAW` on IIgs) |
| `write_mem(domain, address, data)` | Poke (same domains); `data` non-empty |
| `key_event(down, scancode, mod=0)` | One SDL key down/up |
| `key_down` / `key_up` | Same, for modifiers |
| `tap_key(scancode, mod=0, hold_s=0.02)` | Down, optional hold, then up |
| `type_text(text, delay_s=0.05, hold_s=0.02)` | ASCII US layout; `\n` → Return; `hold_s` between down/up; `delay_s` after each char (**3×** after Return) |
| `request(type, payload)` | Raw framed call; raises `ProtocolError` on ERROR |
| `on_event(handler)` | Optional `handler(event_id, seq, data)` for EVENT frames |

Rules: call `hello()` first; one outstanding request at a time; never reimplement framing in agent scripts.

## Constants

From `gs2debug` / `gs2debug.keys` / `gs2debug.types`:

| Name | Value / meaning |
|------|-----------------|
| `MEM_MAIN` | `0` — CPU MMU view (`cpu->mmu->read` / `write`) |
| `MEM_MEGAII` | `1` — Mega II / IIe-view MMU (`computer->mmu`); **IIgs only** |
| `MEM_ENSONIQ` / `MEM_ADBMICRO` | Reserved (ERROR `unsupported domain`) |
| `MEM_MAIN_RAW` | `4` — physical `cpu->mmu->get_memory_base()[addr]` (no MMU/bus) |
| `MEM_MEGAII_RAW` | `5` — physical Mega II buffer; **IIgs only** |
| `PLATFORM_APPLE_II` … `PLATFORM_APPLE_IIGS` | `0`…`5` — same as `-p` / `StatusInfo.platform_id` |
| `SCANCODE_LCTRL`, `SCANCODE_F12`, `SCANCODE_RETURN`, … | SDL3 scancodes (see `keys.py`) |
| `KMOD_LCTRL`, `KMOD_CTRL`, `KMOD_LSHIFT`, … | SDL keymod bits for the **event’s** `mod` field |
| `ProtocolError.code` / `.message` | `E_*` from protocol (`E_BAD_LENGTH=2`, `E_INTERNAL=6`, …) |

Addresses are linear `uint32`. IIe / Mega II text page 1 row 0: `0x0400` (`MEM_MAIN` on II/IIe, or `MEM_MEGAII` on IIgs). IIgs bank `$E0` text via FPI: `MEM_MAIN` at `0xE00400`. `MEM_*_RAW` uses buffer offsets (IIgs FPI: banks 0–127 in 8 MB; Mega II: 128 KB including aux at `+0x10000`) — not CPU `$E0xxxx` addresses.

## Recipes

### Check platform

```python
status = c.get_status()
print(status.execution_mode, status.platform_id)
```

### Peek / poke text row

```python
row = c.read_mem(MEM_MAIN, 0x0400, 0x28)          # II / IIe
c.write_mem(MEM_MAIN, 0x0400, os.urandom(0x28))
assert c.read_mem(MEM_MAIN, 0x0400, 0x28) == data  # verify
# IIgs FPI (MAIN): use 0xE00400
# IIgs Mega II:     c.read_mem(MEM_MEGAII, 0x0400, 0x28)
# Physical buffers: c.read_mem(MEM_MAIN_RAW, 0x0400, 0x28)
#                   c.read_mem(MEM_MEGAII_RAW, 0x0400, 0x28)  # IIgs
```

### Machine reset

Prefer the protocol command (not keyboard):

```python
c.reset(cold_start=False)   # warm
c.reset(cold_start=True)    # cold (clears $3F2–$3F4)
```

Keyboard Control+Reset is still useful for testing KEYEVENT. Handlers check **Ctrl on the F12 event** (macOS/Windows Reset key), not only a prior Ctrl-down:

```python
from gs2debug import SCANCODE_LCTRL, SCANCODE_F12, KMOD_LCTRL, KMOD_CTRL
import time

c.key_down(SCANCODE_LCTRL, KMOD_LCTRL)
c.key_down(SCANCODE_F12, KMOD_CTRL)   # mod must include CTRL
time.sleep(1)
c.key_up(SCANCODE_F12, KMOD_CTRL)
c.key_up(SCANCODE_LCTRL, 0)
```

### Type BASIC / arbitrary text

```python
c.type_text('10 PRINT "HI"\nRUN\n', delay_s=0.1)  # prefer ≥0.1 so line numbers are not dropped
```

Or shell helper (stdin / `--text` / `--file`; does **not** embed a program):

```bash
PYTHONPATH=clients/python/src python3 clients/python/examples/type_to_emu.py /tmp/gs2.sock \
  --reset --wait 2 --delay 0.1 <<'EOF'
NEW
10 GR
20 END
RUN
EOF
```

`--reset` calls protocol `reset(cold_start=False)`.

### Applesoft graphics (don’t confuse modes)

| Mode | Command | Lines |
|------|---------|--------|
| Lo-res `GR` | `COLOR=` 0–15 | `HLIN` / `VLIN` (coords ~40×48) |
| Hi-res `HGR` | `HCOLOR=` 0–7 | `HPLOT x1,y TO x2,y` (280×192) — **not** `HLIN` |

IIe-only soft-switch **status** reads (`$C01A` TEXT, `$C01D` HIRES, …) are invalid on II / II Plus — use `-p 2` (or Enhanced) if you need those.

## Gotchas

1. **Separate terminal** for the emulator — Ctrl-C in the same shell/process group can kill GSSquared.
2. **Typing too fast drops keys** (e.g. line `60` becomes line `0`). Use `delay_s≥0.1` and rely on `hold_s`; Return already gets a longer pause.
3. **Shifted glyphs** (`"`, uppercase, …): library holds Shift for that tap; do not put Control/Alt in `type_text` — use `key_down` / `key_up`.
4. **MAIN / MAIN_RAW / MEGAII / MEGAII_RAW** are implemented; ENSONIQ / ADBMICRO are not.
5. **IIgs MAIN** is the FPI/banked CPU MMU; **MEGAII** is `computer->mmu`. `*_RAW` indexes `get_memory_base()` (buffer offsets, not `$E0xxxx`). Non-GS + MEGAII/_RAW → ERROR `MEGAII only on Apple IIgs`.
6. Disconnecting the client does **not** quit the emulator; it accepts a new client.

## Example scripts

| Script | Role |
|--------|------|
| `examples/hello_ping.py` | HELLO / GET_STATUS (mode + platform) / PING |
| `examples/read_text40.py` | Loop-read II/IIe `$0400` |
| `examples/read_text40_iigs.py` | Loop-read IIgs `$E0/0400` via MAIN |
| `examples/read_text40_megaii.py` | Loop-read IIgs `$0400` via MEGAII |
| `examples/write_text40.py` | Random write + readback `+/-` (II/IIe) |
| `examples/write_text40_iigs.py` | Same for `$E0/0400` |
| `examples/type_basic_iie.py` | Boot wait, protocol RESET, demo Applesoft |
| `examples/type_to_emu.py` | Generic typer tool for agents |

All under `clients/python/`. Each file’s header has concrete Usage lines.

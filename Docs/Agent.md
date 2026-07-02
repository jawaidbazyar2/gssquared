# Emulator Agent

The agent is an optional observer that runs inside GSSquared and ships a
stream of structural events out a UNIX domain socket to a separate
"compositor" process. It is the emulator-side leg of an in-progress effort
to virtually extend the Apple IIgs desktop to modern resolution by
combining sniffed Toolbox calls with sampled video memory — and to
eventually do the same on real IIgs hardware via a Raspberry Pi plugged
into a slot card. The wire protocol is shared between both paths; only the
transport differs (UNIX socket on the emulator, slot-card I/O port on
hardware).

This document covers what the agent emits today, where it hooks into the
emulator, and how to consume its output.

## Quickstart

Enable the agent by setting `GS2_AGENT_SOCKET` in the environment:

```
GS2_AGENT_SOCKET=/tmp/iigs-agent.sock build/GSSquared
```

In another terminal, connect a receiver. The included Python decoder
parses the wire format and pretty-prints each packet:

```
socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - | tools/decode_agent_stream.py
```

Or capture for later analysis:

```
socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - | xxd > capture.txt
tools/decode_agent_stream.py capture.txt
```

Without `GS2_AGENT_SOCKET`, the agent is not constructed and the emulator
behaves identically to a normal build. The hook overhead in disabled mode
is a single null check per CPU instruction (predictable branch, ~free).

## Architecture

The agent is a small library (`gs2_agent`, in `src/agent/`) plus a
handful of one-line hook insertions in the existing emulator code.

```
emulator thread                            IO thread
───────────────                            ─────────
CPU65816::execute_next ──┐
display.cpp scanner_iigs ─┤  emit_*()      accept compositor ─┐
... (future hooks)        ├─►  ▼            │                  │
                          │   build pkt     ▼                  │
                          └─► EventQueue ──► pop_blocking ──► UnixSocketTransport
                              (bounded,                          │
                               drop-oldest)                      ▼
                                                          UNIX socket
```

The emulator-thread hooks build a small `vector<uint8_t>` packet and push
it onto a bounded queue. A separate IO thread drains the queue to the
listening UNIX socket, blocking in `accept()` when no compositor is
connected and rotating cleanly through reconnects.

Source files:

| File | Role |
|---|---|
| `src/agent/Agent.{hpp,cpp}` | Public API; owns queue + IO thread; lifecycle |
| `src/agent/EventQueue.{hpp,cpp}` | Bounded thread-safe queue (drop-oldest) |
| `src/agent/Protocol.{hpp,cpp}` | Wire framing, tag enum, packet builders |
| `src/agent/UnixSocketTransport.{hpp,cpp}` | Server-side UDS, accept/write/reconnect |
| `src/computer.{hpp,cpp}` | Owns `agent::Agent *agent`; constructs from env var |
| `src/cpu.hpp` | `cpu_state` carries `agent::Agent *agent` for hot-path access |

Threading rules:

- All `Agent::emit_*` methods are called from the emulator thread and
  must be cheap and non-blocking. They build a packet and append to a
  bounded queue.
- The IO thread is the sole owner of the listen / client sockets. It
  pulls packets off the queue and writes them to the wire. If the
  compositor disconnects, the queue keeps filling under drop-oldest
  policy until a new compositor connects.
- Shutdown is initiated by the destructor calling
  `transport_->shutdown()` then `queue_.shutdown()`. The IO thread
  drains any remaining packets and exits.

## Wire protocol

All multi-byte integers are big-endian.

### Framing

```
[len: u16]   length of (tag + payload), in bytes
[tag: u8]    packet type
[payload: len-1 bytes]
```

Tag space is partitioned by direction so misdirected packets are obvious:

- `0x00`–`0x7F` — agent → compositor
- `0x80`–`0xFF` — compositor → agent (reserved; not yet emitted in either
  direction)

### Packets

| Tag | Name | Payload | Notes |
|-----|------|---------|-------|
| `0x00` | `HELLO` | `[u8 version] [u32 caps]` | First packet on every connect, including reconnect |
| `0x01` | `VBL` | `[u32 frame_seq]` | One per frame (60 Hz) at IIgs scanline 192 |
| `0x02` | `TOOL_CALL` | `[u16 X] [u16 A] [u16 Y] [u16 SP] [u8 N] [N stack bytes]` | Toolbox dispatcher entered with these registers; N (currently 16) bytes of caller's stack at SP+4 are included so the receiver can decode args per a per-tool signature table |
| `0x03` | `MEM_WRITE` | `[u32 addr (24-bit)] [u8 data]` | CPU write to video memory or video softswitch, normalized to its $E0/$E1 slow-bus address |
| `0x04` | `TOOL_RETURN` | same shape as `TOOL_CALL` | Toolbox dispatcher returned. X carries the *original* selector saved at entry. Stack capture covers the return slot (now filled by the tool) plus the popped args. Paired with the matching `TOOL_CALL` by reverse stack order — the agent maintains a small frame stack so nested calls pair correctly |
| `0x05` | `MEM_BLOB` | `[u32 addr (24-bit)] [u16 len] [len bytes]` | Opportunistic memory-block dump. The agent emits this when a hook wants the receiver to see structures pointed at by a register/arg. First user: NewWindow emits the 80-byte WindParam record (so receivers can extract `wPosition` without a follow-up request). Receivers associate by *immediate ordering* — a `MEM_BLOB` emitted right after a `TOOL_CALL` belongs to that call |
| `0x80` | `INPUT_MOUSE_MOTION` | `[u16 x] [u16 y]` | **Compositor → agent.** Synthesizes an `SDL_EVENT_MOUSE_MOTION` via `SDL_PushEvent`; keygloo's existing handler picks it up exactly as if the host user had moved the mouse. |
| `0x81` | `INPUT_MOUSE_BUTTON` | `[u8 button] [u8 down] [u16 x] [u16 y]` | **Compositor → agent.** Synthesizes `SDL_EVENT_MOUSE_BUTTON_DOWN`/`UP`. Button numbers follow SDL3 conventions (1=left, 2=middle, 3=right). |

`HELLO.caps` is a bitmask of capabilities advertised by the agent:

| Bit | Constant | Meaning |
|-----|----------|---------|
| `0x0001` | `CAP_VBL` | Emits `VBL` packets |
| `0x0002` | `CAP_TOOLBOX` | Emits `TOOL_CALL` packets |
| `0x0004` | `CAP_VIDEO_WRITES` | Emits `MEM_WRITE` packets |
| `0x0008` | `CAP_INPUT_INJECTION` | Accepts compositor → agent input (`INPUT_MOUSE_MOTION` / `INPUT_MOUSE_BUTTON`) |

The `frame_seq` on `VBL` and the per-event sequence counters carried on
future packets are monotonic across the whole session. The receiver can
detect drops by watching for gaps. Sequence numbers are **not** reset on
reconnect — fresh `HELLO` plus monotonic seq lets the compositor distinguish
a fresh connection from a stream resumption.

### Sample bytes on the wire

```
HELLO version=1 caps=0x00000003
00 06 00 01 00 00 00 03

VBL frame_seq=0
00 05 01 00 00 00 00

TOOL_CALL X=0x0902 A=0x6822 Y=0x00fb  (Memory Manager / NewHandle)
00 07 02 09 02 68 22 00 fb
```

## Hook points

The agent observes the emulator at four sites today (with two more
planned). Each site is a one-line addition guarded by a `nullptr` check
on the agent pointer.

| Source | Site | Emits |
|---|---|---|
| Toolbox dispatcher entry | `src/cpus/cpu_65816.cpp` `CPU65816::execute_next` | `TOOL_CALL` |
| Video scanner VBL | `src/display/display.cpp` (`agent_chained_scanner_iigs_handler` wrapping `scanner_iigs_handler`) | `VBL` |
| Bank $E0 video page writes | `src/mmus/mmu_iigs.cpp` `bank_e0_write` | `MEM_WRITE` |
| Bank $E1 SHR writes | `src/mmus/mmu_iigs.cpp` `bank_e1_write` | `MEM_WRITE` |
| Shadowed writes from main RAM | `src/mmus/mmu_iigs.cpp` `bank_shadow_write` (inside the `shadow_is_enabled` branch) | `MEM_WRITE` |
| Softswitch writes | `src/mmus/mmu_iigs.cpp` `MMU_IIgs::write_c0xx` | `MEM_WRITE` (addr `$C0xx`) |
| _planned_: Compositor → agent input | `src/devices/adb/keygloo.cpp` `keygloo_process_event` (synthesizing `SDL_Event`) | (consumes `INPUT_*`) |

The Toolbox hook fires when the CPU is about to execute the instruction
at `$E1/0000`, which is the IIgs Toolbox dispatcher entry point. Apps
invoke the Toolbox via `JSL $E10000` with the call selector in X, so this
fires exactly once per Toolbox call. The hot-path test is

```cpp
if (cpu->agent != nullptr && (cpu->full_pc & 0x00FFFFFFu) == 0x00E10000u) {
    uint8_t stack[agent::protocol::TOOL_CALL_STACK_BYTES];
    for (size_t i = 0; i < sizeof(stack); ++i)
        stack[i] = cpu->mmu->read((cpu->sp + 4u + i) & 0xFFFFu);
    cpu->agent->emit_tool_call(cpu->x, cpu->a, cpu->y, cpu->sp,
                               stack, sizeof(stack));
}
```

The stack bytes start at `SP+4` because `JSL` pushed three bytes of
return PC (`PCL` at `SP+1`, `PCH` at `SP+2`, `PBR` at `SP+3`); the
caller's args sit immediately above. The IIgs Toolbox follows Pascal
calling: caller pushes a return slot first (if the function has a
result), then args left-to-right; the JSL pushes the return PC last.
So in the captured 16 bytes the **last** declared argument lands at
offset 0, earlier args follow, and the return slot (if any) sits at
the bottom.

Receivers decode the bytes per a per-tool signature table — see
`tools/iigs_tool_args.py` for the current set, which covers the
common Window Manager + Menu Manager calls. Adding a tool is a single
new entry in the `TOOL_SIGS` dict.

The VBL hook installs a wrapping IRQ handler on the IIgs video scanner
when the agent is enabled. The wrapper notifies the agent of
`VS_EVENT_VBL` and then forwards to the original `scanner_iigs_handler`
so IIgs IRQ delivery is unaffected. `VS_EVENT_QTR` and
`VS_EVENT_SCB_INTERRUPT` are deliberately not forwarded to the agent —
the compositor only needs one tick per frame, and SCB-interrupt timing
isn't meaningful at the host level.

## Tool-name table

`tools/iigs_tools.tsv` is a generated mapping from X-register selector to
the canonical Apple/Roger Wagner Publishing tool name (1262 entries
across 33 tool sets). Format:

```
# x_register   name              tool_set   function
0x010E         WindBootInit      0x0E       0x01
0x090E         NewWindow         0x0E       0x09
0x0B0E         CloseWindow       0x0E       0x0B
0x150E         FrontWindow       0x0E       0x15
0x1D0E         TaskMaster        0x0E       0x1D
...
```

The table was extracted from
[`roughana/supermacs`](https://github.com/roughana/supermacs), which is a
github mirror of the Apple/Roger Wagner Merlin assembler tool macro
library (`Window.Macs.S`, `Mem.Macs.S`, etc.). Each macro file declares
entries of the form

```
_NewWindow MAC
 Tool $90E
 <<<
```

which the extraction script in `tools/` (regex on `_<name> MAC` followed
by `Tool $<NNNN>`) turns into TSV rows. Re-running the extraction
against an updated supermacs checkout is the way to refresh the table.

The C++ side of the agent intentionally does **not** carry this table.
Names are receiver-side concerns; the wire stays binary.

### Common tool-set numbers

The low byte of the X register is the tool set; the high byte is the
function within the set.

| Tool set | Name | Notes |
|---|---|---|
| `$01` | Tool Locator | Tool set boot/startup orchestration |
| `$02` | Memory Manager | `NewHandle`, `DisposeHandle`, etc. |
| `$03` | Misc Tools | `ReadTimeHex`, math helpers, etc. |
| `$04` | QuickDraw II | Drawing primitives |
| `$05` | Desk Manager | Desk accessories |
| `$06` | Event Manager | `GetNextEvent`, `EventAvail` |
| `$07` | Scheduler | |
| `$08` | Sound Manager | |
| `$0E` | Window Manager | `NewWindow`, `TaskMaster`, `FrontWindow` |
| `$0F` | Menu Manager | `MenuSelect`, `InsertMenu` |
| `$10` | Control Manager | Buttons, scrollbars |
| `$11` | QuickDraw II Aux | Polygons, pictures |
| `$14` | Dialog Manager | Modal/modeless dialogs |
| `$15` | Scrap Manager | Clipboard |
| `$16` | Standard File | `SFGetFile`, `SFPutFile` |
| `$19` | Font Manager | |
| `$1B` | ACE | Audio Compression / Expansion |
| `$1C` | Resource Manager | |
| `$1E` | TextEdit | |

## Decoder utility

`tools/decode_agent_stream.py` reads either a raw binary stream from
stdin or an `xxd`/`hexdump`-style ASCII capture file, parses the wire
protocol, and prints one human-readable line per packet. Tool calls are
named by looking up the X register in `iigs_tools.tsv`.

```
$ socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - \
    | tools/decode_agent_stream.py
HELLO              version=1 caps=0x00000003
VBL                seq=0
TOOL_CALL          ToolLocator/TLBootInit  x=0x0101 a=0x6822 y=0x00fb
TOOL_CALL          MemoryMgr/MMBootInit    x=0x0102 a=0x01de y=0x0000
TOOL_CALL          MemoryMgr/NewHandle     x=0x0902 a=0x0084 y=0x0078
...
TOOL_CALL          WindowMgr/NewWindow     x=0x090e a=0xdf91 y=0x0004
TOOL_CALL          WindowMgr/CheckUpdate   x=0x0a0e a=0x0850 y=0x17b8
TOOL_CALL          EventMgr/GetNextEvent   x=0x0a06 a=0x0000 y=0x8df9
TOOL_CALL          MenuMgr/MenuSelect      x=0x2b0f a=0x0000 y=0x17ce
```

Useful pipelines:

```
# Live tail of named events:
socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - \
    | tools/decode_agent_stream.py

# Record a session and analyze later:
socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - | xxd > /tmp/cap.txt
tools/decode_agent_stream.py /tmp/cap.txt | grep WindowMgr/NewWindow

# Histogram tool-set frequencies:
tools/decode_agent_stream.py /tmp/cap.txt \
    | awk -F/ '/TOOL_CALL/ {sub(/  .*/,"",$1); sub(/.*  /,"",$1); print $1}' \
    | sort | uniq -c | sort -rn | head
```

## Behavior notes

### Idle traffic

Even with no user interaction, the IIgs Finder issues thousands of
Toolbox calls per second. The bulk are the GS/OS event loop:

- `WindowMgr/TaskMaster` — main desktop event-loop entry, ~once per frame
- `WindowMgr/FrontWindow` — called repeatedly inside TaskMaster
- `WindowMgr/CheckUpdate` — idle update-event probe
- `WindowMgr/GetWRefCon` / `GetSysWFlag` — per-event window lookups
- `EventMgr/GetNextEvent`, `EventMgr/EventAvail`
- `QuickDrawII/HidePen`, `QuickDrawII/ShowPen` — cursor blink

User-action events (`NewWindow`, `CloseWindow`, `TrackGoAway`,
`MenuSelect`, etc.) sit on top of this idle baseline, typically firing a
single-digit number of times per action. They're easy to grep for.

### Reconnect behavior

When the compositor disconnects, the agent goes back to `accept()` while
the queue keeps filling under drop-oldest policy. On reconnect the agent
sends a fresh `HELLO` and resumes streaming. Sequence numbers carry across
the gap, so the compositor can detect dropped events by watching for
discontinuities. There is no resync packet yet — when we add the
high-volume video memory writes, a `REQUEST_FULL_STATE` compositor →
agent command will become necessary.

### Compositor-built-from-agent assumption

The agent advertises capabilities; it does not negotiate. The compositor
is expected to handle older agents gracefully (ignore unknown tags) and
the agent is expected to keep tag and payload formats backward-compatible
within a major version. Bumping `protocol::VERSION` indicates a breaking
change.

### Memory write snoop

`MEM_WRITE` packets normalize all video-memory CPU writes to their
canonical `$E0xxxx` / `$E1xxxx` form regardless of how the CPU got
there:

- **Direct writes** to bank `$E0` or `$E1` go through `bank_e0_write` /
  `bank_e1_write`; the hook checks the offset against the video page
  ranges and emits `MEM_WRITE` with the original 24-bit address.
- **Shadowed writes** from main RAM (`$00`/`$01` with the relevant bit
  in shadow register `$C035` / `$C036` enabled) go through
  `bank_shadow_write`. The hook fires only inside the existing
  `shadow_is_enabled(address)` branch — i.e., only when the IIgs
  hardware actually mirrors the byte to slow RAM. The bank-select bit
  from `address & 0x1FFFF` is folded into `$E0xxxx` / `$E1xxxx`.
- **Softswitch writes** to `$C0xx` go through `write_c0xx` and are
  emitted unconditionally with addresses in the `$C0xxxx` range. The
  receiver tracks shadow/speed/border/SHR-enable state from these.

Address ranges that pass the inline filter:

| Range | Meaning |
|---|---|
| `$E0/0400-07FF` | Text page 1 |
| `$E0/0800-0BFF` | Text page 2 (ROM 03 only) |
| `$E0/2000-3FFF` | Hires page 1 |
| `$E0/4000-5FFF` | Hires page 2 |
| `$E0/C000-C0FF` | Softswitches (every write, no further filter) |
| `$E1/2000-9FFF` | SHR (32K, including SCBs and palette) |

Softswitches are emitted at `$E0/C0xx` because that's where the slow bus
(and the megaii dispatcher in the emulator, the slot card on real
hardware) actually sees them, regardless of which program bank the CPU
was using when it issued the write.

`$E0/6000-9FFF` is intentionally **excluded**. Empirically (a 160-second
GS/OS Finder capture) it carries hundreds of thousands of writes per
session, all of them GS/OS Memory Manager / tool heap allocations
unrelated to display. Earlier defensive widening to include this range
was based on a misremember of the LINEARIZE behavior; the actual
LINEARIZE remap (NEWVIDEO bit 6, `g_aux_linear`) is bank-internal to
$E1, splitting `$E1/2000-3FFF` against `$E1/6000-9FFF` so the VGC can
fetch two bytes per cycle from two physical RAM chips. The CPU sees a
flat 32K SHR buffer at `$E1/2000-9FFF` either way, so the existing
`$E1` filter covers all displayable SHR writes.

The peak bandwidth is hardware-bounded by the IIgs slow bus to roughly
~200 KB/sec, since slow-bus stores at 1 MHz cap the rate at which the
CPU can change video memory at all. Realistic averages under desktop
GUI activity are an order of magnitude lower.

## Roadmap

In rough priority order:

1. **Compositor → agent direction.** `INPUT_MOUSE` / `INPUT_KEY` packets
   that the compositor emits when the host user interacts with a
   reconstructed window; the agent injects them as synthetic `SDL_Event`s
   via `keygloo_process_event`.
2. **Real compositor binary.** Replaces `socat` + the Python decoder for
   actual desktop reconstruction.
4. **Run-batched `MEM_WRITE`.** When consecutive writes hit consecutive
   addresses (very common in QD pixel runs), coalesce into a single
   `MEM_WRITES_RUN` packet to cut wire overhead. Not needed today; will
   matter once the slot-card transport is the bottleneck.
5. **Slot-card hardware path.** A Raspberry Pi behind a CPLD/FPGA on a
   real IIgs slot bus. Same wire protocol, different transport. Requires
   a small GS/OS init driver to install the dispatcher hook in software,
   plus the slot card's Φ2 video-write snoop and Φ1 VBL edge detection.
   See the project's design notes for the hardware envelope.
6. **MCP server.** Once the compositor maintains a coherent world model
   (windows, menus, cursor, SHR shadow), expose it through a Model
   Context Protocol server: `iigs_screenshot`, `iigs_list_windows`,
   `iigs_recent_toolcalls`, `iigs_send_click`, `iigs_send_keys`,
   `iigs_capture`. This turns the IIgs into a tool an LLM can drive
   directly — useful for testing, automation, and as the eventual
   programmable-IIgs-workbench endpoint of the project.

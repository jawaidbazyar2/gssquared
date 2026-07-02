# MCP Server (design)

> Status: **Agent ported + building; MCP module not yet written.** Branch
> `mcp-uplift` (off current `origin/main`) now carries the Agent module,
> compositor excluded, and builds clean. The `src/mcp/` module is designed
> (below) and blocked only on one dependency decision (see "Resolved
> decisions / remaining gate"). Modelled on `~/src/verilogapple`'s MCP.

## Resolved decisions / remaining gate

Settled (by verilogapple precedent + code analysis on this repo):

- **JSON + framing:** `nlohmann/json` with newline-delimited JSON-RPC 2.0
  — exactly what `verilogapple/sim/mcp/mcp_server.cpp` uses.
- **Transport:** in-process **UNIX socket** gated by `GS2_MCP_SOCKET`
  (mirrors the Agent's `GS2_AGENT_SOCKET`), plus a tiny `socat` bridge in
  `.mcp.json` for stdio. Headless-stdio (verilogapple's model) is **not**
  viable cheaply here: `computer_t` constructs `video_system` (opens the
  SDL window) in its ctor (`gs2.cpp:838,952`), so a no-window mode is a
  real refactor. Deferred.
- **Threading:** MCP IO thread parses requests; anything touching emulator
  state is marshalled to the emulator thread via a command queue drained
  by `McpServer::pump()` (called from `run_one_frame`). No CPU-state
  races. Reuses the existing debugger primitives (`execution_mode`,
  `instructions_left`; see `debug_window_t::step_one/resume`).

**Remaining gate (needs a nod):** how to bring `nlohmann/json` into
gssquared's build, since it affects every contributor's build of Baz's
repo. Options: (a) CMake `find_package(nlohmann_json CONFIG)` with a
`FetchContent` fallback [recommended — portable, matches verilogapple's
system-include use]; (b) vendor the single amalgamated header under
`vendored/`; (c) hand-roll minimal JSON (avoids the dep but diverges from
verilogapple and gets thrown away later).

## Goal

Expose the running gssquared emulator as a set of Model Context Protocol
tools an LLM (Claude Code) can drive directly — the same *sort* of thing
verilogapple's MCP does: cycle/instruction-level introspection and
control for debugging and automation.

## Scope boundary (important)

The gssquared tree carries exactly two cooperating pieces and **no
reference to the compositor**:

- **Agent** (`src/agent/`, already exists) — an in-process observer that
  (a) emits structural events (Toolbox calls, VBL, memory-write snoop,
  WindMgr window snapshots) and (b) injects input (mouse today; keys to
  be added). It also happens to speak a wire protocol over a UNIX socket,
  but that transport is for *out-of-tree* consumers only.
- **MCP** (`src/mcp/`, new) — a JSON-RPC server that drives the existing
  debugger and reads emulator state to implement MCP tools.

Anything that fans the Agent's socket stream out to multiple consumers
(the "tee"), and any desktop-reconstruction compositor, lives in a
separate clone/repo — not here.

## How this differs from verilogapple

| | verilogapple | gssquared |
|---|---|---|
| Emulator | headless Verilator model, step-driven, MCP owns the clock | real-time SDL app (`SDL_AppInit`/`SDL_AppIterate`) |
| Debugger surface | built into the MCP worker | **already exists** in `src/debugger/` + `src/debug.cpp` |
| Process model | supervisor + worker (RTL rebuilds bounce the worker) | single binary — no RTL-relink constraint, so **no supervisor needed** |
| Transport | JSON-RPC over stdio, registered in `.mcp.json` | see "Open decisions" |

The consequence: most of the verilogapple tool surface is *wrapping code
gssquared already has*, not new emulator plumbing.

## Architecture

`src/mcp/` is an in-process JSON-RPC server (its own thread inside
GSSquared). Because it shares the address space with `cpu_state`, `mmu`,
and the debugger, introspection tools call straight into existing code —
no wire protocol between MCP and the emulator.

```
  Claude Code ── MCP (stdio or socket) ──▶ src/mcp/ server thread
                                             │  (in-process)
                                             ├─▶ src/debugger/ : bp, step, resume, trace
                                             ├─▶ mmu / cpu_state : peek, regs
                                             └─▶ src/agent/ : input injection, WindMgr snapshot
```

MCP reuses the Agent **in-process** (direct method calls) for the two
capabilities it uniquely holds — input injection and the WindMgr
snapshot. MCP does **not** open or depend on the Agent's UNIX socket.

### Clock ownership

verilogapple's `step`/`until_pc` work because the sim is idle until told
to advance. gssquared runs free in real time, so the MCP debug tools
drive the emulator through the existing debugger controls:

- entering stepped tools sets `computer->execution_mode = EXEC_STEP_INTO`
  (and/or the `halt` flag);
- `resume` returns the emulator to real-time execution;
- `until_pc` / `until_write` are breakpoints (`bp`) / `MemoryWatch` +
  resume-until-hit.

## Tool mapping (verilogapple → gssquared internals)

All confirmed to have existing backing code unless marked NEW.

| MCP tool | gssquared backing |
|---|---|
| `reset` | existing reset path |
| `regs` | read `cpu_state` registers |
| `peek` | `debug_dump_memory` / mmu read |
| `step_inst` | `execution_mode = EXEC_STEP_INTO` |
| `until_pc` | breakpoint (`bp`) + resume |
| `until_write` | `MemoryWatch` (`src/debugger/MemoryWatch.*`) |
| `screen_text` | read text page via mmu |
| `mem_diff` | snapshot range + diff (small helper in `src/mcp/`) |
| `trace_capture` | `src/debugger/trace.cpp` |
| `mount_disk` | existing disk-mount path (`-d` handling) |
| `type` (keys) | **NEW**: key-injection tag in Agent (mouse-only today; keygloo already handles real keys, so push `SDL_EVENT_KEY_DOWN/UP`) |
| `screenshot` | video framebuffer read (GUI mode) |
| `list_windows` | Agent WindMgr snapshot (`emit_window_snapshot_entry` logic, called in-process) |
| `send_click` / `send_keys` | Agent input injection (`TAG_INPUT_MOUSE_*`; keys NEW) |

## Open decisions

1. **Attach model.**
   - (a) *Headless stdio* — add a `--mcp` mode: no SDL window, JSON-RPC
     over stdio, registered in `.mcp.json` like verilogapple. Requires
     making SDL video optional in `gs2.cpp` (`SDL_AppInit` currently
     always inits video; args are `getopt "sxp:d:"`, no headless flag).
     Best for deterministic debugging.
   - (b) *Socket alongside live GUI* — MCP listens on a port while the
     normal GUI runs; drive a live visible session. `.mcp.json` needs a
     tiny stdio↔socket bridge (belongs in the clone, per the tee rule).
   - **Recommended: build (a) first, structure so (b) can be added
     without rework.**

2. **v1 tool scope.**
   - **Recommended: minimal proof first** — `reset`, `regs`, `peek`,
     `step_inst` end-to-end to validate the JSON-RPC seam + `.mcp.json`,
     then expand to the full debug-parity set, then the GUI tools
     (`screenshot`, `list_windows`, `send_click`) which lean on the Agent.

## Status: minimal proof implemented

`src/mcp/McpServer.{hpp,cpp}` is in the tree and building. It exposes a
newline-delimited JSON-RPC 2.0 server over a UNIX socket, with tools:
`regs`, `peek`, `poke`, `reset`, `step`, `until_pc`, `disasm`, `pause`,
`resume`, plus the MCP `initialize` / `tools/list` / `ping` handshake.
`disasm` reuses the existing `Disassembler`; `until_pc`/`step` reuse the
debugger's `execution_mode`/`instructions_left`. Emulator-touching tools
are marshalled to the emulator thread by `pump()` (called from
`run_one_frame`); IO/parse lives on a dedicated thread.

**Verified end-to-end** against a running headless emulator (Apple II+):
the JSON-RPC handshake and every tool return correct real state — e.g.
`peek $FFFC` returns the `$FA62` reset vector, `disasm` shows the Disk II
boot loop, `poke`+`peek` roundtrips, `until_pc` bounds correctly. See the
headless note below.

### Headless mode (`GS2_HEADLESS`)

The emulator is normally a GUI app: `computer_t`'s ctor builds
`video_system`, which opens the SDL window and (on a machine with no
display) aborts. `GS2_HEADLESS=1` runs it with **no window** — the video
system renders to an offscreen software surface, and the frame-present
path is skipped — so the CPU + MCP loop runs without a display (for
MCP/CI/automation). It auto-launches a platform (default Apple II+, or
`-p`). The normal GUI path is untouched: every change is gated behind the
flag, which is false by default.

```sh
GS2_HEADLESS=1 GS2_MCP_SOCKET=/tmp/gs2-mcp.sock build/GSSquared -p 1
python3 tools/mcp_smoke.py /tmp/gs2-mcp.sock
```

## Build / run

- `src/mcp/` compiles into GSSquared (single binary; no separate worker).
  `nlohmann/json` is found via `find_package(nlohmann_json)` with a
  `FetchContent` fallback.
- Launch with the socket enabled, e.g. Apple II+ (`-p 1`):

  ```sh
  GS2_MCP_SOCKET=/tmp/gs2-mcp.sock build/GSSquared -p 1
  ```

- `.mcp.json` (repo root) registers a `socat` stdio↔socket bridge so an
  MCP client (Claude Code) can attach. The emulator must be running first.
- Manual smoke test without an MCP client (server must be running):

  ```sh
  python3 tools/mcp_smoke.py /tmp/gs2-mcp.sock
  ```

- Adding a tool: implement a `tool_*` body in `McpServer.cpp`, register it
  in `tools_catalogue()`, add a dispatch arm in `handle_tool_call()` —
  same pattern as verilogapple's `build_tool_catalogue()` /
  `handle_tools_call()`.

## Next

- Interactive end-to-end verification of the tools (needs the GUI run).
- `step_inst` semantics review; then extend toward the fuller
  verilogapple surface (`until_pc`, `screen_text`, `mem_diff`,
  `trace_capture`, `mount_disk`) and the Agent-backed GUI tools
  (`screenshot`, `list_windows`, `send_click`).

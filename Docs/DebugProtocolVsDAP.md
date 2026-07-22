# GS2 Debug Protocol vs DAP

Coverage comparison for source- and assembly-level debugging: GSSquared’s binary host protocol ([DebugProtocol.md](DebugProtocol.md), v1) against Microsoft’s [Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/specification.html) used by VS Code and similar IDEs.

## Bottom line

GS2 already covers the **assembly-debugging core** that DAP exposes optionally (run control, instruction/data breakpoints, memory R/W, stop reasons, CPU snapshot). It is intentionally weaker on **source** debugging, call stacks, variables, and expressions — those stay on the host. It is **stronger** on Apple II / IIgs machine specifics (memory domains, I/O watches, instruction trace, peripherals, key injection).

| | GS2 | DAP |
|--|-----|-----|
| Design center | Assembly-first | Source-first |
| Surface size | ~20 implemented commands | 50+ requests (capability-gated) |
| Audience | Hosts, agents, scripts | IDE UI |

---

## Design posture

### GS2 External Debug Protocol

Host-driven imperative commands over a 12-byte binary frame. Opaque payloads; no JSON, no expression language, no symbols inside the emulator.

- Binary framing
- Typed stop engine (address / access / mask / ignore-count)
- Host owns symbols and fancy predicates
- Agent / CLI first

**Split:** GS2 matches addresses / access / masks; the host maps source lines, symbols, and complex conditions via stop → inspect → `CONTINUE`.

### Debug Adapter Protocol (DAP)

JSON request/response/event protocol between IDE and a debug adapter. Source lines are the primary breakpoint model; assembly is an optional capability layer.

- JSON + `Content-Length` framing
- IDE UI contract (Variables, Call Stack, Breakpoints, Debug Console)
- Expression evaluation in the adapter
- Capabilities negotiated at `initialize`

The adapter typically wraps GDB/LLDB/a runtime and presents threads, frames, variables, and source mapping to the editor.

---

## Capability matrix

| Concern | GS2 protocol | DAP |
|---------|--------------|-----|
| Session handshake / caps | `HELLO` (version + flags) | `initialize` + `Capabilities` |
| Pause / continue | `PAUSE`, `CONTINUE` + `EVT_RUN_STATE` | `pause`, `continue` (+ `continued`) |
| Step into (instruction) | `STEP_INTO(count)` + `STOP_STEP` | `stepIn` (+ `granularity=instruction`) |
| Step over / step out | Not on wire (UI has helpers) | `next`, `stepOut` |
| Instruction breakpoints | `BP_SET` EXEC + `addr_mask` + range | `setInstructionBreakpoints` |
| Data breakpoints | `BP_SET` DATA (R/W, value, ignore) | `setDataBreakpoints` (+ bytes) |
| I/O / soft-switch watches | `BP_KIND_IO` bank whitelist | No first-class equivalent |
| Source-line breakpoints | Host maps line→addr (by design) | `setBreakpoints` (primary model) |
| Function / exception BPs | None | `setFunctionBreakpoints`, `setExceptionBreakpoints` |
| Memory read/write | `READMEM` / `WRITEMEM` + domains | `readMemory` / `writeMemory` |
| Disassembly service | None (host can disasm bytes) | `disassemble` |
| CPU regs at stop | `EVT_STOPPED` 40-byte trace blob | `scopes` → `variables` (Registers) |
| Live register query | Via stop snapshot / `GET_TRACE` | `scopes` / `variables` while stopped |
| Instruction history | `GET_TRACE` ring (100k × 40B) | No standard request |
| Call stack / frames | None | `stackTrace` (+ `instructionPointerReference`) |
| Variables / evaluate | None (no expr language in GS2) | `scopes`, `variables`, `evaluate`, `setVariable` |
| Symbols / modules / sources | Host-side only | `modules`, `loadedSources`, `source` |
| Device / peripheral state | `STATE_GET` / `STATE_SET` (Ensoniq, Disk II, Mouse) | Not in DAP core |
| Input injection | `KEYEVENT` (SDL) | Not in DAP core |
| Machine reset | `RESET` warm/cold | `restart` (session-level, optional) |

---

## Assembly debugging depth

### Where GS2 matches or exceeds DAP

**Stronger — memory domains.** `MAIN` / `MEGAII` / `RAW` / `ENSONIQ` are first-class. DAP has opaque `memoryReference` strings with no standard multi-space model.

**Stronger — I/O watches.** Soft-switch kind with fixed banks `{00,01,E0,E1}`. DAP data breakpoints cannot express this without adapter-specific logic.

**Stronger — address mask + range.** Bank-aware or bank-agnostic EXEC/DATA matches without an expression language. DAP instruction breakpoints are point addresses.

**Stronger — instruction trace.** `GET_TRACE` exposes a large ring of completed instructions with full CPU snapshots. No DAP equivalent.

**Parity — stop + CPU state.** `EVT_STOPPED` ships reason, bp id, PC/eaddr, and a 40-byte register/opcode snapshot — same role as DAP `stopped` → `stackTrace` → `scopes`.

**Parity — data BP features.** R/W access, byte value/mask, ignore-count, temporary, enable. Maps to DAP data breakpoints + hit conditions (expression form differs; GS2 uses fixed fields).

### Where DAP is richer for assembly UIs

**`disassemble`.** Server returns formatted instructions with optional symbol resolution. GS2 expects the host to `READMEM` and disassemble locally.

**`instructionPointerReference`.** Stack frames can anchor the disassembly view at the current PC. GS2 has PC in the stop blob but no frame model.

**Step over / out / granularity.** `next`, `stepOut`, and statement-vs-instruction stepping are standard. GS2 only exposes `STEP_INTO(count)` on the wire.

**Register editing UX.** `setVariable` / `setExpression` on register scopes. GS2 has no dedicated register-write command.

**Advanced navigation.** `goto`, `restartFrame`, `stepBack` — out of scope for GS2 v1.

---

## Source debugging depth

**Intentional asymmetry.** GS2 explicitly pushes symbols and source lines to the host (“Symbol → address” / “Source line breakpoints” in the host column of DebugProtocol.md). DAP makes those first-class wire concepts.

Bridging to VS Code means a thin DAP adapter that maps `setBreakpoints` → `BP_SET` EXEC after symbol lookup — **not** growing an expression language inside GS2.

| DAP area | What it provides | GS2 |
|----------|------------------|-----|
| Source BPs | `setBreakpoints` by file/line/column; conditions, hit conditions, logpoints; `breakpointLocations` | Host maps line → address |
| Stack + locals | `stackTrace`, `scopes`, `variables`, `evaluate` | None on wire; needs DWARF/PDB (or similar) in an adapter |
| Modules / sources | `modules`, `loadedSources`, `source` | No program/module concept on the wire |

---

## Mapping cheat sheet (if you wrapped GS2 in DAP)

| DAP request / event | GS2 backing | Notes |
|---------------------|-------------|-------|
| `initialize` | `HELLO` | Caps hardcoded in adapter |
| `attach` / `launch` | CLI + socket connect | Emulator already running |
| `pause` / `continue` | `PAUSE` / `CONTINUE` | Policy A = step-off EXEC bp |
| `stepIn` | `STEP_INTO(1)` | Batch count is a GS2 extra |
| `next` / `stepOut` | Temp EXEC bp / host logic | Not native yet |
| `setInstructionBreakpoints` | `BP_SET` EXEC | Replace-all semantics in adapter |
| `setDataBreakpoints` | `BP_SET` DATA | Map access + bytes |
| `setBreakpoints` | Host symbols → `BP_SET` EXEC | Adapter owns line→addr |
| `readMemory` / `writeMemory` | `READMEM` / `WRITEMEM` | Encode domain in `memoryReference` |
| `disassemble` | `READMEM` + host disasm | Or add future `DISASM` cmd |
| `stopped` | `EVT_STOPPED` | Map reason codes |
| `continued` | `EVT_RUN_STATE` | When mode → `NORMAL` |
| `stackTrace` / `scopes` | Trace blob → fake 1 frame | Enough for asm UI |
| `threads` | Single dummy thread | Required by DAP even for 1 CPU |
| `evaluate` | Host-only | Do not put expr eval in GS2 |
| `modules` / `source` | — | Leave unsupported |

---

## Gaps worth caring about

### Goal: agent / scripted asm debug

GS2 v1 is already well-aimed: run control, rich stops, multi-domain memory, EXEC/DATA/IO breakpoints, trace history, peripherals.

**Nice-to-haves on the wire:** `STEP_OVER` / `STEP_OUT`, dedicated `GET_REGS` / `SET_REGS`, optional `DISASM` for convenience (not required if the host disassembles).

### Goal: VS Code source debugging

Build a DAP adapter process — do not implement DAP inside GS2. The hard missing pieces are host-side: symbols, stack unwinding for high-level languages, and variable evaluation.

For Merlin/ca65/ORCA asm in the editor, instruction BPs + disassemble + memory + one register frame may be enough without full DWARF.

---

## Sources

- [DebugProtocol.md](DebugProtocol.md) — GS2 framing v1
- [DAP specification](https://microsoft.github.io/debug-adapter-protocol/specification.html) — requests, `Capabilities`, assembly-related `supportsInstructionBreakpoints` / `supportsDisassembleRequest` / `supportsReadMemoryRequest`

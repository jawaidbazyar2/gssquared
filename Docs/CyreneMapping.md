# Cyrene ↔ GSSquared DebugProtocol Mapping

Assessment of whether [DebugProtocol.md](DebugProtocol.md) (v1) is sufficient to build an **intermediary server** that speaks the Cyrene/KEGS IPC contract on one side and GSSquared’s debug socket on the other.

**Cyrene reference:** `/Users/bazyar/src/CyreneSourceCode/CyrenePlugInForKEGS` — see `CYRENE_KEGS_PROTOCOL.md`, `Cyrene_WindowIPC.h`, `Cyrene_Breakpoint.c`.

**Scope:** Apple IIgs only (`-p 5`). Cyrene is a 65816 / GS/OS–oriented debugger; GS2’s multi-platform commands are irrelevant to this bridge.

---

## Verdict

**Yes — for a first useful bridge.** DebugProtocol now covers the core loop Cyrene needs: session handshake, pause/continue, instruction stepping, register snapshots at stop, multi-domain memory peek/poke, typed breakpoints with stop events, and Ensoniq peripheral state.

**No — for full Cyrene parity without extra work.** Several Cyrene features have no GS2 wire equivalent (semantic stop conditions, system-call breakpoints, logpoints, system-call trace ring, battery RAM in snapshots, streaming 59-byte operation batches with Toolbox call numbers). An intermediary can **emulate some** of these (blocking acks, snapshot assembly, GOID counters, trace→operation translation) but others need **GS2 extensions** or **accept degraded behavior**.

Recommended posture: ship an intermediary that implements Cyrene IPC + shared memory on Windows (or a Cyrene-facing TCP shim), maps the **supported subset** below, and documents unsupported Cyrene breakpoint/stop forms with clear errors in the shared breakpoint region.

---

## Architecture comparison

| Aspect | Cyrene ↔ KEGS | Cyrene ↔ intermediary ↔ GS2 |
|--------|---------------|----------------------------|
| Transport | `WM_COPYDATA` between hidden HWNDs | Intermediary presents Cyrene IPC; connects to GS2 `AF_UNIX` debug socket |
| Bulk data | ~1 MB named shared memory (`Local\Cyrene_<pid>`) | Intermediary owns shared memory; fills from GS2 commands |
| Sync model | KEGS **blocks emulation** until next C2K command after K2C data | GS2 is **async** (requests + unsolicited `EVENT`s); intermediary must block Cyrene side and sequence GS2 I/O |
| Memory read | Full snapshot (~MB) per stop | Synthesize from many `READMEM` / `GET_REGS` / `STATE_GET` calls |
| Breakpoints | Text lines in shared memory | Parse lines → `BP_SET`; map hits → Cyrene stop reason `10` |
| Live progress | GOID QWORD at shared offset 0 | Intermediary maintains GOID/GCC counters (not tied to GS2 internals) |

The intermediary is therefore a **stateful adapter**, not a thin opcode translator.

---

## Command mapping (C2K → DebugProtocol)

| Cyrene (C2K) | GS2 | Notes |
|--------------|-----|-------|
| `OPEN_CONNECTION` | `HELLO` | Intermediary accepts Cyrene window name; opens GS2 socket separately |
| `CLOSE_CONNECTION` | close socket | Optional `QUIT` only if shutting down emulator |
| `GET_SNAPSHOT` | composite | See [Snapshot synthesis](#snapshot-synthesis-k2c_send_snapshot) |
| `GET_OPERATION` + `Stop=xN` | `STEP_INTO(N)` then wait `EVT_STOPPED` | Cyrene uses GOID-relative stop; GS2 uses instruction count — equivalent for fixed N |
| `GET_OPERATION` + `Stop=@addr` | temp `BP_SET` EXEC + `CONTINUE` | Remove temp BP after stop |
| `GET_OPERATION` + `Stop=JMP/JSR/RTS/VBL/INTERRUPT/TOOLBOX/OS` | **no direct map** | See [Semantic stop conditions](#gap-semantic-stop-conditions) |
| `PAUSE` | `PAUSE` | Cyrene stop reason maps to explicit pause, not Cyrene code `5` |
| `WRITE_DATA` | `WRITEMEM` | Cyrene: `(addr & 0xFF000000)==0` → RAM at `addr & 0xFFFFFF`; else DOC → GS2 `MEM_MAIN` / `MEM_ENSONIQ` |

### K2C responses (intermediary → Cyrene)

| Cyrene (K2C) | Source |
|--------------|--------|
| `SEND_SNAPSHOT` | Synthesized buffer (59-byte op + stop byte + RAM + DOC + BRAM + DOC regs) |
| `SEND_OPERATION` | Translated from `GET_TRACE` windows and/or stepped instructions |
| `CLOSE_CONNECTION` | GS2 process exit or debug socket close |

---

## Snapshot synthesis (`K2C_SEND_SNAPSHOT`)

Cyrene expects a single variable-length blob (see Cyrene spec offsets 0, 100, 101, 512, 768, 1024+).

| Snapshot region | Cyrene offset | GS2 source | Status |
|-----------------|---------------|------------|--------|
| Current operation | 0, 59 bytes | `GET_REGS` + last completed trace / live peek | **Partial** — see [Operation record](#gap-operation-record-59-bytes) |
| Stop reason | 100 | `EVT_STOPPED.reason` | **Partial** — map GS2 reasons to Cyrene 1–10 |
| Breakpoint ID | 101 | `EVT_STOPPED.bp_id` | OK when `reason=10` |
| DOC registers | 512, 227 bytes | `STATE_GET` (`DEVICE_ID_ENSONIQ`) | **Partial** — layout differs; intermediary must pack Cyrene’s 227-byte DOC block from GS2’s 784-byte blob |
| Battery RAM | 768, 256 bytes | — | **Gap** — no `READMEM` domain or `STATE_GET` for `DEVICE_ID_RTC_PRAM` |
| Fast/slow RAM | 1024+ | `READMEM` `MEM_MAIN_RAW` / banked reads | OK (multiple round-trips; mind 64 KiB/frame cap) |
| ROM | after RAM | `READMEM` `MEM_MAIN` at `$FC0000`–`$FFFFFF` | OK via CPU view |
| DOC RAM | after ROM | `READMEM` `MEM_ENSONIQ` | OK |

Stop-reason mapping (GS2 → Cyrene byte at offset 100):

| GS2 `EVT_STOPPED.reason` | Cyrene code | Cyrene meaning |
|--------------------------|-------------|----------------|
| `STOP_STEP` (4) | 1 | GOID count (if stop was `Stop=xN`) |
| `STOP_BP_EXEC` (1) | 5 or 10 | Address / breakpoint — use 10 if `bp_id != 0` |
| `STOP_BP_DATA` (2) | 10 | Breakpoint |
| `STOP_BP_IO` (3) | 10 | Breakpoint |
| `STOP_PAUSE` (5) | — | Cyrene has no direct “host paused” code; use `10` with id 0 or extend adapter convention |

Semantic stops (JMP, VBL, Toolbox, …) have **no GS2 event** today.

---

## Breakpoint mapping (shared memory text → `BP_SET`)

Cyrene writes semicolon-separated lines to shared memory; KEGS parses in `Cyrene_Breakpoint.c`. The intermediary parses the same text and issues `BP_SET` / `BP_CLEAR` / `BP_ENABLE`.

### Supported (direct or approximate)

| Cyrene source + trigger | GS2 |
|-------------------------|-----|
| `Address=BB/HHLL` + `Execute` | `BP_KIND_EXEC`, `address = (BB<<16)\|HHLL`, `addr_mask` from `*` wildcards |
| `Address=…` + `Read` / `Write` / `ReadWrite` | `BP_KIND_DATA` + `BP_ACCESS_*` |
| `Address=BB/HHLL` in `$C000`–`$C0FF` + R/W | `BP_KIND_IO` (bank whitelist matches Cyrene soft-switch banks) |
| `Read: = XX` / `Write: = XX` (byte, equality) | `FLAG_DATA_MATCH` + `data_value` / `data_mask` |
| `Repeat:Only<N>` / `Repeat:After<N>` | `ignore_count` / hit-count logic in intermediary (GS2 `ignore_count` = skip N hits before stop — map `After<N>`; `Only<N>` needs intermediary-side disable after N hits) |
| `Action:Stop` | normal BP stop → Cyrene reason 10 |

Wildcard addresses (`*` in hex digits): map to GS2 `addr_mask` nibble masks (same idea as Cyrene’s `source_address_mask`).

### Unsupported (report parse error or ignore line)

| Cyrene feature | Why |
|----------------|-----|
| `SystemCall=P8/P16/GSOS/TOOL-…` | GS2 does not instrument Toolbox / ProDOS / GS/OS calls |
| `Register=…` + `Hold:` | No register watch / compare breakpoint kind |
| `Operation=NNOOOOPPQQ` | No opcode/operand pattern breakpoint |
| `Address=src-dst` range (MVN/MVP) | GS2 range BPs are single contiguous span, not dual-address block moves |
| `Read/Write: !=, >, >=, <, <=` | GS2 data match is **byte equality with mask** only |
| 16-bit compare values in triggers | GS2 watches treat bus as **8-bit** for value match |
| `Action:Print'…'` / `Action:File'…'` | No logpoint / trace-to-ring-buffer in GS2 |
| `Symbol=…` | Cyrene resolves before send; still OK if resolved to `Address=` |

---

## Operation record (59 bytes)

Cyrene’s per-instruction record (`OPERATION_DATA_SIZE = 59`) is **not** the same as GS2’s `system_trace_entry_t` (40 bytes, documented in DebugProtocol).

| Cyrene field | GS2 trace / stop blob |
|--------------|----------------------|
| A, X, Y, PC, PB, DB, SP, D, P | Present (layout differs) |
| Opcode + 3 operand bytes | `opcode`, `operand` (32-bit) — pack into Cyrene bytes |
| Separate read / write / jump addresses | **Gap** — GS2 has one `eaddr`, `data`, `f_write` |
| Read/write value sizes, page-wrap flags | **Partial** — `f_data_sz`, no separate wrap bits |
| `call_number` (Toolbox/OS) | **Gap** — not in GS2 trace |
| Soft switches C02E/C02F/C035/C068 | **Gap** — not in trace; could `READMEM` `MEM_MAIN` at `$C0xx` when building record |
| GOID / GCC | Intermediary counters |
| ROM version / RAM bank count | From `GET_STATUS` / config, not trace |

**Streaming:** Cyrene receives up to 20,000 × 59-byte records mid-run (`K2C_SEND_OPERATION`) before the final snapshot. GS2 exposes history via `GET_TRACE` (ring buffer, 40-byte entries) and stop via `EVT_STOPPED`. An intermediary can poll `GET_TRACE` during long `STEP_INTO` runs but:

- Entries are **post-instruction** and may race if the guest is not paused.
- Ring semantics differ from Cyrene’s forward cache flush.

Practical approach: for `Stop=xN` with N ≤ a few thousand, step in chunks (`STEP_INTO` + wait event), pull `GET_TRACE` after each chunk, translate to 59-byte records.

---

## Event / execution mapping

| Cyrene behavior | GS2 |
|-----------------|-----|
| Block until Cyrene ack | Intermediary waits on Cyrene C2K after sending K2C; drives GS2 asynchronously in background |
| Break at BP | `EVT_STOPPED` + `PAUSE` semantics (`execution_mode=PAUSED`) |
| Continue after stop | `CONTINUE` (Policy A for EXEC re-hit — compatible with Cyrene “step off” if Cyrene sends another `GET_OPERATION`) |
| Step 1/10/100/… | `STEP_INTO(count)` |
| Live GOID at shm[0] | Intermediary increments on each translated operation |

GS2 **`SET_REGS`** exists but Cyrene/KEGS never had register write — not required for Cyrene compatibility.

---

## Trace / log output (shared memory ring)

Cyrene shared memory offset 5120+: 1 MB ring of `P`/`F`/`T` lines from breakpoint print actions and system-call tracing (mask at offset 28).

| Feature | GS2 |
|---------|-----|
| Breakpoint `Action:Print` / `Action:File` | **Gap** — no formatted trace output channel |
| System-call trace (`T` lines, mask bits P8/P16/GSOS/Toolbox) | **Gap** — no OS/Toolbox hook in debug protocol |
| Overflow counter at offset 24 | Intermediary can implement locally if emulating print actions |

---

## Gap summary

### Intermediary-only (no GS2 change required)

- Windows `WM_COPYDATA` + shared memory façade
- Synchronous blocking ack semantics on Cyrene side
- GOID / GCC generation
- Full snapshot assembly from many protocol round-trips (slow but correct except BRAM)
- 40 → 59 byte operation translation (with documented missing fields zeroed)
- Cyrene breakpoint text parser → `BP_SET`
- `Repeat:Only` / complex repeat modes via intermediary state
- Stop reason byte mapping for supported stop types

### Degraded (works with limitations)

- **`GET_SNAPSHOT` latency** — many `READMEM` calls vs one KEGS memcpy; cache snapshots in intermediary if Cyrene tolerates staleness on optional refresh
- **Operation streaming** — chunk stepping + `GET_TRACE` instead of true per-op flush; large `Stop=x10000` may be slow
- **Wildcard / masked address BPs** — map to `addr_mask`; verify edge cases against Cyrene’s nibble `*` rules
- **DOC register block in snapshot** — translate from Ensoniq `STATE_GET` v1 layout
- **Soft-switch bytes in operation record** — extra `READMEM` at `$C02E` etc. when synthesizing 59-byte records

### Hard gaps (need GS2 extensions or accept missing Cyrene features)

#### Semantic stop conditions

`C2K_GET_OPERATION` strings with no GS2 command:

| Payload | Needed capability |
|---------|-------------------|
| `Stop=JMP` | Stop on branch/jump opcode |
| `Stop=JSR` | Stop on JSR/JSL/JML |
| `Stop=RTS` | Stop on RTS/RTL/RTI |
| `Stop=VBL` | Vertical-blank stop |
| `Stop=INTERRUPT` | IRQ/NMI entry stop |
| `Stop=TOOLBOX` | Toolbox call stop |
| `Stop=OS` | ProDOS / GS/OS call stop |

**Possible GS2 additions:** `STEP_UNTIL` with a `stop_kind` enum, or temporary internal stops wired to existing CPU trace hooks. Until then, intermediary can only approximate with repeated small steps and host-side opcode inspection (too slow for interactive use).

#### System-call breakpoints and trace

Cyrene `SystemCall=P8-XX`, `P16-XXXX`, `GSOS-XXXX`, `TOOL-XXXX` and the system-call trace ring require **GS/OS & Toolbox call detection** in the emulator debug path (KEGS fills `call_number` in `Cyrene_Operation.c` / engine hook). GS2 trace records do not include call numbers; no `BP_KIND_SYSCALL` exists.

#### Battery RAM in snapshot

Cyrene dumps 256 bytes at snapshot offset 768. GS2 stores BRAM in the RTC device (`bram[256]`, persisted under `PrefPath/bram/<uuid>.bin`) with **no** `STATE_GET`/`READMEM` exposure. Intermediary cannot faithfully reproduce this region without a new **`STATE_GET` for `DEVICE_ID_RTC_PRAM`** (256-byte blob) or a dedicated memory domain.

#### Logpoints and file actions

`Action:Print` / `Action:File` with template expansion (`[A]`, `[GOID]`, `[@addr]`, …) — no GS2 equivalent. Would need either logpoint support in the stop engine or intermediary-side emulation by evaluating templates on each `EVT_STOPPED` (only for stops, not for non-stop prints).

#### Data breakpoint comparators

Cyrene supports `!=`, `>`, `>=`, `<`, `<=` on read/write/hold triggers (8- and 16-bit). GS2 `BP_SET` data match is **fixed byte equality with mask**. Relational watches require host-side continue filtering or a protocol extension.

#### Register hold breakpoints

Cyrene `Register=PC` + `Hold: >= XXXX` — no GS2 register watch kind. Could fake with `GET_REGS` after every stop only if combined with full instruction tracing ( impractical ).

---

## Recommended intermediary shape

```
┌─────────────┐   WM_COPYDATA +    ┌──────────────────┐   AF_UNIX    ┌───────────┐
│ Cyrene.exe  │◄──shared memory───►│ cyrene-gs2-bridge │◄────────────►│ GSSquared │
│ (Windows)   │                    │ (Windows service) │  DebugProto  │  -p 5     │
└─────────────┘                    └──────────────────┘              └───────────┘
```

Core loop on `C2K_GET_OPERATION`:

1. Parse stop string; if unsupported, set Cyrene-visible error (e.g. reject in breakpoint status or no-op with message in output ring).
2. Install/update BPs from shared memory (`UpdateBreakpointList` equivalent).
3. Issue GS2 `CONTINUE` or `STEP_INTO(N)`.
4. Wait for `EVT_STOPPED` / `EVT_RUN_STATE` (and optionally poll `GET_TRACE` mid-run).
5. Build `K2C_SEND_OPERATION` batches and final `K2C_SEND_SNAPSHOT`.
6. Block until next C2K (Cyrene ack model).

Run GS2 as: `./GSSquared --debug /tmp/cyrene-gs2.sock -p 5 --no-quit-confirm` for harness-style shutdown.

---

## Suggested GS2 protocol additions (priority order)

If the goal is **full Cyrene parity** rather than a minimal bridge, extend DebugProtocol in this order:

1. **`STATE_GET` for `DEVICE_ID_RTC_PRAM`** — 256-byte BRAM blob (+ version header). Unblocks snapshot offset 768.
2. **`STEP_UNTIL` or extended `STEP_INTO` flags** — JMP / JSR / RTS / VBL / interrupt semantic stops (matches Cyrene `Stop=` strings).
3. **System-call stop + trace event** — Toolbox / P8 / P16 / GS/OS call number in trace or dedicated `EVENT`; enables Cyrene `SystemCall=` BPs and `T` ring lines.
4. **Logpoint channel** — optional text lines on BP match without stopping (`Action:Print`), or document that intermediary fakes these locally.
5. **Richer data-watch compare ops** — optional `BP_FLAG_CMP_GT` etc., or keep relational filters host-side only.

Items 1–3 unblock the largest Cyrene/KEGS feature gaps; 4–5 are polish for power users.

---

## Implementation readiness checklist

| Cyrene workflow | Ready? |
|-----------------|--------|
| Connect / disconnect | Yes (adapter) |
| Pause | Yes (`PAUSE`) |
| Step 1 / 10 / … / 10000 | Yes (`STEP_INTO`) |
| Run to address | Yes (temp EXEC BP) |
| Exec / read / write / I/O address breakpoints | Yes (`BP_SET`, with comparator limits) |
| Memory patch | Yes (`WRITEMEM`) |
| Register view at stop | Yes (`EVT_STOPPED` / `GET_REGS`) |
| Full memory snapshot | Yes, slow (many `READMEM`; BRAM missing) |
| Per-instruction history at stop | Partial (`GET_TRACE` + translate) |
| Step JMP/JSR/RTS/VBL/INT/Toolbox/OS | **No** |
| System-call breakpoints | **No** |
| Register / opcode pattern breakpoints | **No** |
| Print / file breakpoint actions | **No** |
| System-call trace ring | **No** |
| Relational data watch values | **No** |

---

## References

- GS2: [DebugProtocol.md](DebugProtocol.md), [DebugClient.md](DebugClient.md), [gs2debug.md](gs2debug.md)
- GS2 vs IDE protocols: [DebugProtocolVsDAP.md](DebugProtocolVsDAP.md) (similar “adapter owns gaps” pattern)
- Cyrene/KEGS: `CYRENE_KEGS_PROTOCOL.md`, `Cyrene_WindowIPC.h`, `Cyrene_Breakpoint.h`, `Cyrene_Operation.h`

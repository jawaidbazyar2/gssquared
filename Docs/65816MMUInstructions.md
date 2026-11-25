# 65816 Instruction Enhancements for MMU

CPU side of the two-level MMU in [TwoLevelPageTables.md](TwoLevelPageTables.md). Page-table words, the walk, and the TLB leaf layout stay there. This document is privilege, extra registers, traps, and the new instructions.

Every new instruction is a `WDM` extension. **Signature bytes are not assigned yet.** Assemblers use the mnemonics below. Design record: [MMUReport.md](MMUReport.md).

This is a **soft 65816** (FPGA/ASIC). It is not a W65C816S with an external MMU.

## Standard vs enhanced

**Standard mode** (reset): cycle-accurate 65816. `WDM` is always 2 bytes. `PRIV` does not shift, `S`/`SSP` do not swap, user/supervisor does not apply. Phantom cycles are stock.

**Enhanced mode:** entered by `ENHN` (2-byte `WDM`, recognized in standard mode). `PRIV` is live, `WDM` uses length-class decoding (2/3/4 bytes), MMU register ops apply, no phantom cycles. Distinct from `MMU_EN`. Reset leaves enhanced mode.

`ENHN` in enhanced mode is a NOP. There is no instruction to leave enhanced mode; `/RES` does.

2-byte MMU ops (`ENHN`, `CPUID`, `L1*`, `TXUS`/`TSXU`, `STPV`/`LDPV`, `VBR*`, `MMUE`/`MMUD`, `TLBI`, `LDCS`, `LDFA`) decode in standard mode so tables, `VBR`, and `SSP` can be programmed before `ENHN` / `MMUE`. They do not change stock 65816 architected state except `ENHN` (mode) and `CPUID` (`A`). `MMUE` is a NOP until enhanced.

## Privilege

`PRIV` is an 8-bit shift register. It is not in `P`. `PHP` / `PLP` / `REP` / `SEP` cannot touch it.

In **standard mode** `PRIV` is inert (no shift, no `SSP` swap).

In **enhanced mode**, `PRIV[0]` is the current mode. 1 = supervisor, 0 = user. The walk uses `L1_Sup` when `PRIV[0] = 1` and `L1_User` when `PRIV[0] = 0`. There is no PTE user bit; privilege is which table is walked.

Reset: `PRIV = 00000001` (supervisor, empty history).

Trap (`IRQ`, `NMI`, `ABORT`, `COP`, `BRK`), enhanced mode:

```text
old         = PRIV[0]
PRIV        ← (PRIV << 1) | 1
if old == 0:
    swap S ↔ SSP
; stock 4-byte push (PBR, PCH, PCL, P), I ← 1, D ← 0, PBR ← 0, vector
```

`RTI`, enhanced mode:

```text
; stock pop of P, PC, PBR
if PRIV[0] == 1:
    to_user = (PRIV[1] == 0)
    PRIV    ← PRIV >> 1          ; high bit fills 0
    if to_user:
        swap S ↔ SSP
; else leave PRIV alone           ; user RTI cannot escalate
```

Eight bits is current plus seven saved. A deeper nest shifts the oldest bit off the top. **Kernel contract:** do not nest traps more than seven levels; do not `STPV` a value with bit 0 clear except when about to `RTI` into a user process.

GS/OS in supervisor never drops `PRIV[0]`: `old` is always 1, `SSP` is never swapped, the stacked frame is still four bytes.

`S` in this section is the 65816 stack pointer. PTE bit 30 (superpage) is a different `S`; do not mix them.

On trap, `D`, `DBR`, `M`, and `X` keep the interrupted values (stock 65816). The handler stores them in the process context and restores them before `RTI` to user.

## MMU enable

Reset: `MMU_EN = 0`. Translation is off. Identity:

```text
PA[31:24] = 0
PA[23:0]  = VA[23:0]
```

No walk, no `V`/`W`/`NX` check, no TLB, no `ABORT` from the MMU. In enhanced mode `PRIV` still shifts on traps. `WDM` still programs `L1_User`, `L1_Sup`, `SSP`, and `VBR`.

`MMUE` sets `MMU_EN` (enhanced supervisor, `E = 0` only). The next fetch is walked through `L1_Sup`. That fetch, and the live stack, must already be mapped there — usually identity for the kernel page you are standing on. Flushes the TLB. Drains prefetch.

`MMUD` clears `MMU_EN` (back to identity). Flushes the TLB. Drains prefetch. Only safe from a page that is identity in `L1_Sup`.

`MMUE` in emulation (`E = 1`) is a NOP of its length class. `MMUE` before `ENHN` is a NOP.

Both are supervisor-only in enhanced mode. In user they are `WDM` NOPs of their length class. `L1_User` may be 0 at `MMUE`; it is used only when `PRIV[0] = 0`.

Set `VBR` (and `L1_Sup`) before `MMUE`.

## Two level-1 bases

| Register | Used when | Typical writer |
|---|---|---|
| `L1_Sup` | `MMU_EN` and `PRIV[0] = 1` | boot, rarely after |
| `L1_User` | `MMU_EN` and `PRIV[0] = 0` | context switch |

Each is a 32-bit physical address of a 1024-byte level-1 table, 1024-byte aligned. `PA[9:0]` of the register is 0 (RAZ/WI). The walker is [TwoLevelPageTables.md](TwoLevelPageTables.md); substitute the selected base for `L1_Base`.

`$00:0100` in user mode and `$00:0100` in supervisor are different translations. A TLB hit must match the table that filled the entry, not only the VA.

`L1UW` / `L1SW` store `{X[15:0], C[15:0]}` into the chosen base (`X` high, `C` low). Software uses `M=0`, `X=0`; the instruction does not check. `L1UW` drops TLB rows filled from `L1_User`. `L1SW` drops rows filled from `L1_Sup`. Both drain prefetch. `L1SW` while `MMU_EN = 1` has the same “standing on a mapped page” requirement as `MMUE`.

**Kernel contract:** do not leave the previous process’s aliases in `L1_Sup` across `L1UW` (unmap or `TLBI` the copy window, or do not map user pages into `L1_Sup`). Usual IPC is the same PA in two `L1_User` tables. Kernel copyin is a window in `L1_Sup` plus `TLBI`.

## Supervisor stack pointer

`SSP` is a 16-bit pointer, bank 0 implied. User `TXS` / `TCS` / `PHA` never touch it.

On a user → supervisor crossing, `S` and `SSP` swap, then the four-byte frame is pushed. The handler sees the frame at `1,S` as usual. Nested supervisor traps do not swap (`old` was 1). `RTI` to user swaps back.

While `PRIV[0] = 1`, `SSP` holds the frozen user stack pointer. `TXUS` / `TSXU` are how the kernel reads and writes that value (first process, context switch). Live kernel `S` stays ordinary `TXS`. Software uses 16-bit `X`; the instruction does not check. In user they are `WDM` NOPs.

Reset: `SSP` is implementation-defined (kernel ROM stack is a good default). GS/OS never swaps, so `S` is the only stack it has.

## Vector base (`VBR`)

When `MMU_EN = 1`, native `COP` / `BRK` / `ABORT` / `NMI` / `IRQ` fetch the 16-bit PC from a **32-bit physical** table at `VBR`, untranslated (so the vector pull cannot page-fault). `RESET` is never this path: `/RES` clears `MMU_EN` and fetches `$00FFFC` under identity.

The table is the stock native 12 bytes, `VBR+0` = `COP`:

| Offset | Vector |
|---|---|
| `+0` | `COP` |
| `+2` | `BRK` |
| `+4` | `ABORT` |
| `+6` | `NMI` |
| `+8` | reserved (native `$FFEC`) |
| `+$A` | `IRQ` |

Little-endian 16-bit PCs. After the pull: `PBR ← 0`, `I ← 1`, `D ← 0`; the handler fetch is a normal translated VA. `VBR` is 16-byte aligned (`PA[3:0] = 0`, RAZ/WI). `VBRW` / `VBRR` move `{X,C}`. Kernel sets `VBR` before `MMUE`.

With `MMU_EN = 0`, vectors are stock `$00FFE4–$00FFEE`. The IIgs FPI does not use `VPB` to select ROM vectors; this CPU still issues **PA** for translated cycles, including the handler fetch after a `VBR` pull.

## Instructions: blocked or modified

Stock opcodes in **standard mode** are a 65816.

In **enhanced mode**, with `MMU_EN = 1`:

| Instruction | User `PRIV[0]=0` | Supervisor |
|---|---|---|
| `SEI` `CLI` `SEP` `REP` `PLP` | execute; `I` in `P` updates; **IRQ is not masked** | `I` masks IRQ as usual |
| `RTI` | stock four-byte pop; `PRIV` does not shift | pop, then shift `PRIV`; swap `SSP` if returning to user |
| `COP` `BRK` | allowed; trap into supervisor; signature latched | trap (nested); signature latched |
| `STP` | `ABORT` | halt as usual |
| `XCE` | `ABORT` if it would set `E = 1` | `ABORT` if it would set `E = 1` (`CLC; XCE` while native is fine) |
| `WAI` | do not halt; trap (not a page-fault `ABORT`; vector / `PC` TBD) | halt until IRQ/NMI/ABORT/RESET, as usual |
| `WDM` MMU ops | NOP, length still consumed | execute |
| `WDM` unassigned | NOP, length consumed | NOP, length consumed |
| everything else | unchanged | unchanged |

With `MMU_EN = 0` in enhanced mode: no translation; `WAI` is stock; `XCE` is stock; `PRIV` / `SSP` still follow traps if enhanced.

`I` is ignored in user so `SEP #$04` and `PLP` are not back doors around `SEI`. Old GS/OS uses `SEI` in supervisor, with or without the MMU.

`NMI` and `ABORT` are not masked by `I` in either mode.

`WAI` in user must not stop the clock (it would park the whole CPU). Supervisor `WAI` is the idle loop. Native-only under translation: IIe / ProDOS 8 is out of scope (`MMUD` or never `MMUE`).

## `ABORT`

Stock 65816 `/ABORT` restart: registers restore to instruction start; `PC` of the faulting instruction is on the stacked frame; `RTI` retries the whole instruction. A leading byte of a straddling store or RMW may already have committed. `MVN` / `MVP` restart from the beginning (`C`/`X`/`Y` restored); they do not span banks.

There is **no external `/ABORT` pin**. Causes are internal: not-present (`V=0`), write protect (`W=0`), `NX` on fetch, empty physical address (including during a walk), `STP` in user, `XCE` that would set `E = 1`.

Faulting-byte **VA** and **cause** go in a register (`LDFA`); bit layout TBD. `NMI` is unused on a GS and is not specified to preserve that register.

If trap entry itself fails (kernel stack or empty PA at `VBR`), hardware **halts** (double-fault), rather than looping on `ABORT`. Kernel stack pages in `L1_Sup` must always be valid and writable. `VBR` must point at populated RAM.

User `WAI` is **not** this `ABORT` (yield path TBD).

## WDM encoding (signatures later)

`WDM` is `$42` plus a signature.

**Standard mode:** always 2 bytes (stock), even if the signature would be a longer class in enhanced mode.

**Enhanced mode:** bits 7:6 of the signature are the length class of the *extension*, including that signature byte. There is no 5-byte class. Two-byte ops sit at the high end so `$FF` (Host FST) stays the same length as stock.

| Signature | Extra operand bytes | Total size | 65816 analogue |
|---|---|---|---|
| `$C0–$FF` | 0 | 2 | implied |
| `$80–$BF` | 1 | 3 | zp / imm8 |
| `$00–$7F` | 2 | 4 | abs / imm16 |

```text
; bit7 bit6 | extra | total
;  1    1   | 0     | 2   ($C0–$FF)
;  1    0   | 1     | 3   ($80–$BF)
;  0    x   | 2     | 4   ($00–$7F)
operand_bytes = signature[7] ? (signature[6] ? 0 : 1) : 2
total_bytes   = 2 + operand_bytes                   ; includes $42
```

Those extra bytes are a format, not a promise of immediates. Implied MMU ops live in `$C0–$FF`. Do not assign `$FF` (Host FST). Unknown signatures still fetch that many bytes.

Privilege is per mnemonic, not per length class. MMU mnemonics are supervisor-only except `CPUID` (runs in standard mode for discovery) and `ENHN` (standard mode). In user, MMU `WDM`s are NOPs of their length class.

## Mnemonics

Implied (`$C0–$FF` once assigned; skip `$FF`). No operand bytes. Register payload is in `C` and/or `X`. Software uses 16-bit `M`/`X` where noted; hardware does not NOP or fault on 8-bit width.

| Mnemonic | Transfer | Requires | Effect |
|---|---|---|---|
| `ENHN` | — | standard or enhanced | Enter enhanced mode. NOP if already enhanced. Flushes TLB. |
| `CPUID` | → `A` | any | Discovery: `LDA #0` / `CPUID` / `A` changed. Stock 65816 `WDM` is a NOP so `A` stays 0. Payload in `A` TBD. |
| `MMUE` | — | enhanced supervisor, `E=0` | `MMU_EN ← 1`. TLB flush. Next fetch via `L1_Sup`. NOP if `E=1` or not enhanced. |
| `MMUD` | — | enhanced supervisor | `MMU_EN ← 0`. Identity. TLB flush. |
| `L1UW` | `X,C → L1_User` | supervisor | `L1_User ← {X,C}` with `PA[9:0]=0`. Drop user-tagged TLB. |
| `L1SW` | `X,C → L1_Sup` | supervisor | `L1_Sup ← {X,C}` with `PA[9:0]=0`. Drop supervisor-tagged TLB. |
| `L1UR` | `L1_User → X,C` | supervisor | `{X,C} ← L1_User`. |
| `L1SR` | `L1_Sup → X,C` | supervisor | `{X,C} ← L1_Sup`. |
| `VBRW` | `X,C → VBR` | supervisor | `VBR ← {X,C}` with `PA[3:0]=0`. |
| `VBRR` | `VBR → X,C` | supervisor | `{X,C} ← VBR`. |
| `TXUS` | `X → SSP` | supervisor | `SSP ← X`. |
| `TSXU` | `SSP → X` | supervisor | `X ← SSP`. |
| `STPV` | `A → PRIV` | supervisor | `PRIV ← A[7:0]`. |
| `LDPV` | `PRIV → A` | supervisor | `A[7:0] ← PRIV`; if `M=0`, `B` unchanged. |
| `TLBI` | `X,C → VA` | supervisor | Invalidate TLB leaf for VA `{X,C}` (24-bit VA in low 24; match rule TBD). |
| `LDCS` | → `A` | supervisor | `A ←` latched `COP`/`BRK` signature byte `$nn`. |
| `LDFA` | → `X,C` | supervisor | Fault VA and cause (layout TBD). |

`STPV` / `LDPV` move the whole shift register. Trap and `RTI` already shift `PRIV`.

`COP` / `BRK` latch `$nn` into the signature register so the kernel does not read `(user PBR:PC - 1)` from `L1_User`.

```text
ENHN  enter enhanced               CPUID identity / features
MMUE  MMU enable                   MMUD  MMU disable
L1UW  L1 user write                L1SW  L1 supervisor write
L1UR  L1 user read                 L1SR  L1 supervisor read
VBRW  vector base write            VBRR  vector base read
TXUS  X → user stack (SSP)         TSXU  user stack → X
STPV  store A to PRIV              LDPV  load PRIV to A
TLBI  invalidate TLB at VA         LDCS  load COP/BRK signature
LDFA  load fault VA/cause
```

`TXUS` / `TSXU` are `TXS` / `TSX` aimed at `SSP` instead of `S`.

### Example: boot, then first user

```text
        ; reset: standard mode, MMU_EN=0, PRIV=00000001, identity 24-bit
        LDA #0
        CPUID                   ; A≠0 → this CPU
        ENHN                    ; enhanced; PRIV live; no phantom cycles

        ; build supervisor L1 (and L2) in RAM with ordinary STA
        LDA #l1_sup_lo
        LDX #l1_sup_hi
        L1SW
        LDA #vbr_lo
        LDX #vbr_hi
        VBRW
        MMUE                    ; PC's page is identity in that L1; VBR physical

        ; later, first user process
        LDA #l1_user_lo
        LDX #l1_user_hi
        L1UW
        LDX #user_s             ; e.g. $03FF
        TXUS
        LDA #$01
        STPV                    ; PRIV=$01: next RTI → user
        ; push P, PC, PBR for the user (stock frame) then:
        RTI
```

## Trap order (MMU on)

User running (`PRIV[0]=0`, translations from `L1_User`):

1. Shift `PRIV`, swap `S`/`SSP`.
2. Translations are now `L1_Sup`. Kernel pages apply.
3. Push four bytes on the kernel stack (translated). Failure → halt.
4. Fetch the 16-bit PC from physical `VBR + offset` (untranslated). Empty PA → halt.
5. Fetch the handler at `$00:PC` through `L1_Sup`.

Supervisor running: skip the swap; another four bytes on the same kernel stack.

`ABORT` uses this same path (`VBR+$4`). `COP` / `BRK` latch `$nn` before the walk of the handler.

## Reset

`/RES`: standard mode, `MMU_EN = 0`, `PRIV = 00000001`, TLB flush, identity fetch of `$00FFFC`. `VBR` is unused until the next `MMUE`.

## Other

**Single-core.** No CAS, no barriers, no TLB shootdown.

**Cache:** PTE `NC` and fabric uncached `$E0C000` / `$E1C000` are in [TwoLevelPageTables.md](TwoLevelPageTables.md). PA decode, motherboard RAM, slot DMA, 1 MHz, and timers are not this ISA.

## GSSquared emulator

The hardware ISA still has a TLB, `NC`, and prefetch. The emulator does not: it has no cache hierarchy and no need to memoize walks.

With `MMU_EN = 1`, every real access (fetch, load, store, including the walk’s own table loads) runs the walk in [TwoLevelPageTables.md](TwoLevelPageTables.md) against the live PTE. There is no TLB, so a PTE store is visible on the next access. `NC` is ignored. Prefetch drain is a no-op.

`ENHN` / `MMUE` / `MMUD` / `L1UW` / `L1SW` / `TLBI` / `/RES` still execute; their TLB-flush and `TLBI` effects are no-ops. Software should still issue them so the same code runs on hardware later.

Standard-mode phantom cycles stay. Enhanced mode still issues none.

## Open

- `WDM` signature bytes for every mnemonic above.
- `CPUID` payload in `A`.
- `TLBI` match (4 KB vs enclosing superpage; which table tag).
- `LDFA` bit layout (VA + not-present / `W` / `NX` / empty PA / illegal op).
- User `WAI` vector and stacked `PC` (yield, not page-fault `ABORT`).
- Exception priority (`RESET` vs `ABORT` vs `NMI` vs `IRQ`) on the same cycle.

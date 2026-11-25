# 65816 Instruction Enhancements for MMU

CPU side of the two-level MMU in [TwoLevelPageTables.md](TwoLevelPageTables.md). Page-table words, the walk, and the TLB leaf layout stay there. This document is privilege, the extra registers, how traps enter supervisor, and the new instructions.

Every new instruction is a `WDM` extension. **Signature bytes are not assigned yet.** Assemblers use the mnemonics below; the `WDM` map comes later. Length of each `WDM` instruction is still known from the signature: bits 7:6 of that byte are the extra-byte count (0..3), so an unassigned code in the same class still consumes the right number of bytes.

## Privilege

`PRIV` is an 8-bit shift register. It is not in `P`. `PHP` / `PLP` / `REP` / `SEP` cannot touch it.

`PRIV[0]` is the current mode. 1 = supervisor, 0 = user. The walk’s `user` is `PRIV[0] == 0`.

Reset: `PRIV = 00000001` (supervisor, empty history).

Trap (`IRQ`, `NMI`, `ABORT`, `COP`, `BRK`):

```text
old         = PRIV[0]
PRIV        ← (PRIV << 1) | 1
if old == 0:
    swap S ↔ SSP
; stock 4-byte push (PBR, PCH, PCL, P), I ← 1, D ← 0, PBR ← 0, vector
```

`RTI`:

```text
; stock pop of P, PC, PBR
if PRIV[0] == 1:
    to_user = (PRIV[1] == 0)
    PRIV    ← PRIV >> 1          ; high bit fills 0
    if to_user:
        swap S ↔ SSP
; else leave PRIV alone           ; user RTI cannot escalate
```

Eight bits is current plus seven saved. A deeper nest shifts the oldest bit off the top. GS/OS never leaves supervisor: `old` is always 1, `SSP` is never swapped, the stacked frame is still four bytes.

`S` in this section is the 65816 stack pointer. PTE bit 30 (superpage) is a different `S`; do not mix them.

## MMU enable

Reset: `MMU_EN = 0`. Translation is off. Identity:

```text
PA[31:24] = 0
PA[23:0]  = VA[23:0]
```

No walk, no `U`/`W`/`V` check, no TLB, no `ABORT` from the MMU. `PRIV` still shifts on traps. `WDM` still programs `L1_User`, `L1_Sup`, and `SSP`.

`MMUE` sets `MMU_EN`. The next fetch is walked through `L1_Sup`. That fetch, and the live stack, must already be mapped there — usually identity for the kernel page you are standing on.

`MMUD` clears `MMU_EN` (back to identity). Only safe from a page that is identity in `L1_Sup`.

Both are supervisor-only. In user they are `WDM` NOPs of their length class. `L1_User` may be 0 at `MMUE`; it is used only when `PRIV[0] = 0`.

## Two level-1 bases

| Register | Used when | Typical writer |
|---|---|---|
| `L1_Sup` | `MMU_EN` and `PRIV[0] = 1` | boot, rarely after |
| `L1_User` | `MMU_EN` and `PRIV[0] = 0` | context switch |

Each is a 32-bit physical address of a 1024-byte level-1 table, 1024-byte aligned. `PA[9:0]` of the register is 0 (RAZ/WI). The walker is [TwoLevelPageTables.md](TwoLevelPageTables.md); substitute the selected base for `L1_Base`.

`$00:0100` in user mode and `$00:0100` in supervisor are different translations. A TLB hit must match the table that filled the entry, not only the VA.

`L1UW` / `L1SW` store `{X[15:0], C[15:0]}` into the chosen base (`X` high, `C` low). `M` and `X` must be 16-bit (`M=0`, `X=0`); otherwise the instruction is a NOP. `L1UW` drops TLB rows filled from `L1_User`. `L1SW` drops rows filled from `L1_Sup`.

## Supervisor stack pointer

`SSP` is a 16-bit pointer, bank 0 implied. User `TXS` / `TCS` / `PHA` never touch it.

On a user → supervisor crossing, `S` and `SSP` swap, then the four-byte frame is pushed. The handler sees the frame at `1,S` as usual. Nested supervisor traps do not swap (`old` was 1). `RTI` to user swaps back.

While `PRIV[0] = 1`, `SSP` holds the frozen user stack pointer. `TXUS` / `TSXU` are how the kernel reads and writes that value (first process, context switch). Live kernel `S` stays ordinary `TXS`.

`X` must be 16-bit for `TXUS` / `TSXU`. In user they are `WDM` NOPs.

Reset: `SSP` is implementation-defined (kernel ROM stack is a good default). GS/OS never swaps, so `S` is the only stack it has.

## Instructions: blocked or modified

Stock opcodes, with `MMU_EN = 1`. With `MMU_EN = 0`, they are a 65816.

| Instruction | User `PRIV[0]=0` | Supervisor |
|---|---|---|
| `SEI` `CLI` `SEP` `REP` `PLP` | execute; `I` in `P` updates; **IRQ is not masked** | `I` masks IRQ as usual |
| `RTI` | stock four-byte pop; `PRIV` does not shift | pop, then shift `PRIV`; swap `SSP` if returning to user |
| `COP` `BRK` | allowed; trap into supervisor | trap (nested) |
| `STP` | privilege fault (`ABORT`) | halt as usual |
| `XCE` | privilege fault | privilege fault while `MMU_EN = 1` (native only under translation) |
| `WAI` | do not halt; trap (see below) | halt until IRQ/NMI/ABORT/RESET, as usual |
| `WDM` MMU ops | NOP, length still consumed | execute |
| `WDM` unassigned | NOP, length consumed | NOP, length consumed |
| everything else | unchanged | unchanged |

`I` is ignored in user so `SEP #$04` and `PLP` are not back doors around `SEI`. Old GS/OS uses `SEI` in supervisor, with or without the MMU.

`NMI` and `ABORT` are not masked by `I` in either mode.

`WAI` stops the clock until an interrupt. A user process must not do that: it would park the whole CPU so no other process can run. With `MMU_EN = 1` and `PRIV[0] = 0`, `WAI` does not halt. It traps instead, and the kernel may treat that as a yield. Vector and status code are not picked yet.

Supervisor `WAI` is stock, including with the MMU on. The idle loop is the one place it makes sense: the scheduler has already seen an empty run queue. With `MMU_EN = 0`, `WAI` is stock in both modes.

## WDM encoding (signatures later)

`WDM` is `$42` plus a signature. Bits 7:6 of the signature are the length class of the *extension*, including that signature byte:

| Signature | Extra operand bytes | Total size | 65816 analogue |
|---|---|---|---|
| `$00–$3F` | 0 | 2 | implied |
| `$40–$7F` | 1 | 3 | zp / imm8 |
| `$80–$BF` | 2 | 4 | abs / imm16 |
| `$C0–$FF` | 3 | 5 | abs.l |

```text
operand_bytes = signature[7:6]      ; 0..3
total_bytes   = 2 + operand_bytes   ; includes $42
```

Those extra bytes are a format, not a promise of immediates. An implied MMU op lives in `$00–$3F`. A later op that *addresses* memory uses a longer class. Unknown signatures still fetch that many bytes.

Privilege is per mnemonic, not per length class. Every mnemonic in this document is supervisor-only.

## Mnemonics

All of these are implied (`$00–$3F` once assigned). No operand bytes. Register payload is in `C` and/or `X`.

| Mnemonic | Transfer | Requires | Effect |
|---|---|---|---|
| `MMUE` | — | supervisor | `MMU_EN ← 1`. Next cycle translated via `L1_Sup`. |
| `MMUD` | — | supervisor | `MMU_EN ← 0`. Identity map. |
| `L1UW` | `X,C → L1_User` | supervisor, `M=0`, `X=0` | `L1_User ← {X,C}` with `PA[9:0]=0`. Drop user-tagged TLB. |
| `L1SW` | `X,C → L1_Sup` | supervisor, `M=0`, `X=0` | `L1_Sup ← {X,C}` with `PA[9:0]=0`. Drop supervisor-tagged TLB. |
| `L1UR` | `L1_User → X,C` | supervisor, `M=0`, `X=0` | `{X,C} ← L1_User`. |
| `L1SR` | `L1_Sup → X,C` | supervisor, `M=0`, `X=0` | `{X,C} ← L1_Sup`. |
| `TXUS` | `X → SSP` | supervisor, `X=0` | `SSP ← X`. User `S` while in supervisor. |
| `TSXU` | `SSP → X` | supervisor, `X=0` | `X ← SSP`. |
| `STPV` | `A → PRIV` | supervisor | `PRIV ← A[7:0]`. Context save / first drop to user. |
| `LDPV` | `PRIV → A` | supervisor | `A[7:0] ← PRIV`; if `M=0`, `B` unchanged. |

`STPV` / `LDPV` move the whole shift register, not a single trap push. Trap and `RTI` already shift `PRIV`. These two exist so a kernel can save, restore, or force `00000001` before an `RTI` into a new process.

```text
MMUE  MMU enable                   MMUD  MMU disable
L1UW  L1 user write                L1SW  L1 supervisor write
L1UR  L1 user read                 L1SR  L1 supervisor read
TXUS  X → user stack (SSP)         TSXU  user stack → X
STPV  store A to PRIV              LDPV  load PRIV to A
```

`TXUS` / `TSXU` are `TXS` / `TSX` aimed at `SSP` instead of `S`.

### Example: boot, then first user

```text
        ; reset: MMU_EN=0, PRIV=00000001, identity 24-bit
        ; build supervisor L1 (and L2) in RAM with ordinary STA
        LDA #l1_sup_lo
        LDX #l1_sup_hi
        L1SW
        MMUE                    ; PC's page is identity in that L1

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
2. Translations are now `L1_Sup`. Kernel bank 0 and `SSP` pages apply.
3. Push four bytes on the kernel stack.
4. Fetch the vector from `$00FFE4` through `L1_Sup`.

Supervisor running: skip the swap; another four bytes on the same kernel stack.

`ABORT` (not-present, protection, privilege) uses this same path. Fault VA / status CSRs are not in this mnemonic set yet.

## Open

- `WDM` signature bytes for every mnemonic above.
- Kernel copy from user space (window in `L1_Sup`, software walk of `L1_User`, or a later `WDM` load-as-user).
- Explicit TLB invalidate of one VA (writes to `L1_*` already drop that table’s rows).
- Fault address and fault-type registers for `ABORT`.
- User `WAI` trap: which vector (`ABORT` vs something else) and how the kernel tells it from a page fault. Intent is yield, not halt. Supervisor `WAI` stays a real wait.

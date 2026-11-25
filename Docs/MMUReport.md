# 65816 Two-Level MMU Design Gap Analysis

Gap analysis of [65816MMUInstructions.md](65816MMUInstructions.md) and [TwoLevelPageTables.md](TwoLevelPageTables.md) for two implementation contexts:

1. **CPU-socket accelerator card** for a physical Apple IIgs
2. **New GS-compatible computer** built around this CPU + MMU architecture

---

## 1. Core CPU / ISA / MMU Spec Gaps (Both Contexts)

### 1.1 `PRIV` shift register behavior and legacy GS/OS safety — **DECIDED**
In `65816MMUInstructions.md` (lines 14–37, 50), `PRIV` resets to `00000001`, shifts left inserting `1` on traps, and shifts right inserting `0` on `RTI` — even while `MMU_EN = 0`.

- **1.1.1 Synthetic `RTI` in legacy/stock software:** **Decision:** `PRIV` has no effect until enhanced mode is explicitly enabled (new mode-enable instruction / `MMUE`). Before that, stock 65816 behavior applies (no `SSP` swap, no privilege drop).
- **1.1.2 Deep supervisor nesting (>7 levels):** **Decision:** Kernel responsibility not to overflow the 8-bit `PRIV` shift register (non-issue; document as a kernel contract).
- **1.1.3 `STPV` clearing bit 0 directly:** **Decision:** Kernel responsibility not to write an invalid `PRIV` value (non-issue).
- **Follow-up (resolved in 1.4):** Enhanced mode is distinct from `MMU_EN`. Standard mode is a cycle-accurate 65816. `PRIV` stays inert until enhanced mode is entered. See 1.4.1.

### 1.2 65816 `/ABORT` precision, partial side effects, and double faults — **DECIDED** (WAI deferred)
- **1.2.1 Page-straddling & RMW:** **Decision:** Stock 65816 `/ABORT` (option B). Byte 1 may already have committed before byte 2 faults. Registers restore to instruction start; `RTI` retries the whole instruction. Restart of a straddling store to RAM is treated as idempotent; user mode is not expected to map side-effecting I/O.
- **1.2.2 `MVN` / `MVP`:** **Decision:** Stock `/ABORT` restart of the whole instruction (`C`/`X`/`Y` restored to instruction start). `MVN`/`MVP` cannot span a bank (source and dest banks are operands; `X`/`Y` wrap inside those banks), so bank-crossing is a non-issue. A 4 KB page fault mid-move retries the entire copy.
- **1.2.3 Double fault during trap entry:** **Decision:** Option A. Kernel bug: hardware halts (STP-like double-fault halt), rather than looping on `ABORT`. Kernel stack and vector pages in `L1_Sup` must always be valid and writable.
- **1.2.4 Exception priority & user `WAI`:** **Decision (partial):** User `WAI` is **not** a page-fault `ABORT`. Vector, `PC` (retry vs skip), and how the kernel distinguishes yield from other traps are **deferred**. Exception priority (`RESET` vs MMU `ABORT` vs external `/ABORT` vs `NMI` vs `IRQ`) still unspecified.
- **1.2.5 Fault address / cause:** **Decision:** Faulting instruction `PC` is on the stacked `ABORT` frame (stock 65816). Remaining fault info (VA of the faulting byte, cause) goes in a register. `NMI` clobber of that register is a non-issue (nothing in a GS pulls `NMI`). Register layout TBD with the mnemonic set.
- **Follow-up:** Vector-address wording in `65816MMUInstructions.md` line 185 (`$00FFE4` is specifically `COP`; should be the corresponding native vector in `$00FFE4–$00FFEE`).

### 1.3 Emulation mode (`E = 1`) edge cases — **DECIDED**
- **1.3.1 `MMUE` while `E = 1`:** **Decision:** `MMUE` is a NOP in emulation mode (same length class). A kernel will not do this; hardware still refuses rather than enabling translation with an 8-bit stack / no `PBR` on trap frames.
- **1.3.2 `XCE` while `MMU_EN = 1`:** **Decision:** An attempt to set `E = 1` under translation faults (`ABORT`). Native-only while the MMU is on, in both user and supervisor. `CLC; XCE` while already native (`E` stays 0) is not an attempt to set `E = 1`. IIe / ProDOS 8 under the MMU is out of scope; that needs `MMUD` (or never entering enhanced mode).

### 1.4 `WDM` encoding conflicts and edge cases — **DECIDED**
- **1.4.1 Length class and when it is live:** **Decision:** No 5-byte `WDM`. Length classes are 2, 3, and 4 bytes. `$00–$3F` implied (2 bytes), `$40–$7F` one extra (3 bytes), `$80–$FF` two extra (4 bytes). Other emulators' `WDM` uses are irrelevant.
  - **Standard mode:** cycle-accurate 65816, `WDM` always 2 bytes, as today. `PRIV` inert (1.1.1).
  - **Enhanced mode:** enhanced 65816 (length-class `WDM`, `PRIV`, MMU ops). Distinct from `MMU_EN`.
  - Ops needed **before** `MMU_EN = 1` (`MMUE`, `L1SW`/`L1UW`, `TXUS`, `CPUID`, enter-enhanced) are 2-byte `WDM`s so they decode correctly in standard mode and with translation still off.
  - **Follow-up:** Mnemonic/signature for *entering enhanced mode* (must be a 2-byte `WDM` recognized in standard mode). Not `MMUE` (that only sets `MMU_EN`).
- **1.4.2 Wrong `M`/`X`:** **Decision:** No width check, no NOP, no fault. The instruction executes with the live register state; garbage `L1`/`SSP` if `M` or `X` is 8-bit is the programmer's bug. Software contract remains “use with `M=0`, `X=0`.”
- **1.4.3 Feature/version discovery:** **Decision:** `CPUID`, a 2-byte `WDM`. Discovery sequence: `LDA #0` / `CPUID` / test whether `A` changed. On a stock 65816, `WDM` is a NOP so `A` stays 0. Return value in `A` (version/feature bits) TBD.

### 1.5 Bus cycle classification (`VPA`/`VDA`/`VPB`/`MLB`) & TLB lifecycle — **DECIDED**
- **1.5.1 Dummy / phantom cycles:** **Decision:** In enhanced mode there are **no phantom cycles** (`VPA=0, VDA=0` internal ops with a stale address). No walk, no fault, no physical transaction from a dummy cycle, because those cycles are not issued. (Standard mode remains cycle-accurate 65816, including phantom cycles.)
- **1.5.2 TLB invalidation / size:** **Decision:**
  - `RESET`, enter-enhanced, `MMUD`, and `MMUE` flush the TLB.
  - Superpage ↔ L2 size change without a flush: **software contract** (invalidate/flush before changing the type).
  - Mid-instruction page count: **hardware problem**. TLB is expected to be at least 32 entries.
  - Per-VA `TLBI`: **Decision:** add supervisor-only `TLBI` (2-byte `WDM`, implied; VA payload in `C`/`X` like the other MMU ops). Opcode/signature deferred with the rest of the `WDM` map. `L1SW`/`L1UW` still drop all rows for that table. Exact match rule (4 KB vs also the enclosing superpage; user-tagged vs current `PRIV`) TBD with the mnemonic sheet.
- **1.5.3 `U` and `G`:** **Decision: drop `U`.** Privilege is which L1 is walked (user never walks `L1_Sup`). Walk checks are `V` then `W` only. PTE bit 28 is **NC** (2.4): `0` = cacheable, `1` = uncached. No privilege fault type. `G` is not a PTE bit; `L1UW` drops user-tagged TLB rows, kernel-private rows survive by table tag. Kernel contract: do not leave process-A aliases in `L1_Sup` across `L1UW` (flush or unmap the copy window, or do not map user pages into `L1_Sup`). Spec updates needed in `TwoLevelPageTables.md` (flag table, walk, TLB fill) and `65816MMUInstructions.md` (privilege-fault mentions).
- **1.5.4 PTE endianness & torn writes:** **Decision:** Little-endian (everything on the 65816 is). Kernel contract: set `V` last, clear `V` first, so a walker never observes a torn 32-bit PTE. Hardware does not special-case this.
- **1.5.5 Serialization:** **Decision:** `MMUE`, `MMUD`, `L1SW`, `L1UW`, `STPV` drain prefetch; the next fetch uses the new mode/table. `L1SW` while already translating has the same “standing on a mapped identity/valid page” requirement as `MMUE`.
- **Fault CSRs:** See 1.2.5 (`PC` on stack; VA/cause in a register).

---

## 2. Gaps for Context 1: Apple IIgs CPU-Socket Accelerator Card

An accelerator card sits in the IIgs 40-pin 65C816 socket in front of the motherboard **FPI/CYA**, **Mega II**, **ROM**, and **expansion slots**.

### 2.1 Requires a custom soft-core / ASIC 65816 (cannot use a physical W65C816S) — **DECIDED**
**Decision:** Accepted. This ISA cannot wrap a W65C816S; the accelerator is a soft 65816 (FPGA/ASIC) driving the 40-pin socket as bus master. Standard mode remains cycle-accurate 65816 toward the motherboard. Several FPGA 65816 IIgs accelerators already exist; this is the same class of product.

### 2.2 No physical address map partitioning (32-bit PA vs 24-bit IIgs socket) — **DECIDED**
**Decision:** Out of ISA scope. Same as 2.1: an FPGA-core accelerator owns PA decode (onboard RAM vs socket vs MMIO), reset aliases, and RAM discovery. The CPU/MMU spec only produces 32-bit PAs; the card fabric maps them. Not an ISA gap.

### 2.3 Collision with motherboard FPI/CYA & Mega II — **PARTIAL** (scope: CPU ISA only, not full accelerator)
Section 2 describes FPGA accelerators that already have local fast RAM and optional own video. This work defines **65816 enhancements only**, not socket interface / shadow / display card behavior.

- **2.3.1 Vector pull / VBR:** **Decision:** When `MMU_EN = 1`, native `COP`/`BRK`/`ABORT`/`NMI`/`IRQ` fetch from a **32-bit physical** vector base (`VBR`), not VA `$00FFE4`. Untranslated, so a trap cannot page-fault on the vector table itself (interrupt-then-fault). Table is the stock native **12 bytes**, `VBR+0` = `COP`:

  | Offset | Vector |
  |---|---|
  | `+0` | `COP` |
  | `+2` | `BRK` |
  | `+4` | `ABORT` |
  | `+6` | `NMI` |
  | `+8` | reserved (`$FFEC`) |
  | `+$A` | `IRQ` |

  16-bit little-endian PCs as usual; `PBR ← 0` after the pull; handler fetch is then a normal translated VA. `RESET` still `$00FFFC` (MMU off on reset). 2-byte `WDM` `VBRW`/`VBRR`, payload `{X,C}` like `L1SW`. 16-byte aligned (`PA[3:0]=0`, RAZ/WI). Kernel sets `VBR` before `MMUE`. Standard mode / `MMU_EN = 0`: stock `$00FFE4`. FPI does not use `VPB` for ROM vectors.
- **2.3.2 Motherboard `$00`/`$01`/`$E0`/`$E1`:** **Decision:** Out of ISA. FPGA cards have full local fast RAM. Writes through to those banks are for display (slow) and optional; many cards have their own buffers/scanners. Interface logic, not the 65816 enhancement.
- **2.3.3 4 KB page vs legacy I/O / slot layout:** **Unresolved — later.** PTE 4 KB (and 64 KB superpage) cannot split `$C000–$C0FF` I/O from `$C100–$CFFF` slot ROM, or other sub-page legacy holes. Extra masking/decode for those regions is needed and is **not** specified yet. Note: in native mode direct page (`D`) and stack (`S`) can sit anywhere in bank 0, so ZP-at-`$0000` / stack-at-`$0100` sharing a page with text is not a native-kernel constraint.

### 2.4 Missing cache-inhibit / device-memory attribute in PTEs — **DECIDED**
**Decision:** Both fabric and PTE.

- **Fabric:** accesses to physical `$E0C000` and `$E1C000` (Mega II I/O pages) are **always uncached**, regardless of PTE. Side-effecting `$C0xx` must not depend on the OS setting a bit.
- **PTE:** non-cache bit (use freed bit 28). Polarity **`0` = cacheable RAM, `1` = uncached/device** (matches “write unused bits 0”). Covers framebuffers, DMA bounce, `mmap` of MMIO, mailboxes — same PA as ordinary RAM, or devices mapped at arbitrary VAs. Superpage and 4 KB leaf both carry it; on an L2 walk, uncached if **either** level is `1` (a whole bank can be forced device from L1). Walker copies the effective bit into the TLB attribute field.

ISA rule is the PTE bit + the `$E0C000`/`$E1C000` hardwire. Extra fabric ranges are card-specific.

### 2.5 Motherboard DMA, bus arbitration, and page-table placement — **DECIDED**
- **2.5.1 Slot DMA:** **Decision:** Product is **legacy slot-DMA incompatible** (same class as AppleSqueezer). No bounce-buffer ISA, no requirement to snoop `/DMA` into onboard RAM. Slot DMA cards that master motherboard RAM will not see accelerator memory.
- **2.5.2 Page tables / working RAM:** **Decision:** Accelerator uses **onboard card RAM for everything**. Motherboard RAM is ignored (except whatever interface logic still does for optional display writes — 2.3.2). Walker and kernel treat local RAM as the physical store; not an ISA constraint beyond “PAs the CPU uses are on the card.”

### 2.6 Timing, Warm Reset, External `/ABORT`, and Preemption Timer — **DECIDED**
- **2.6.1 1 MHz / IWM:** **Decision:** Interface logic, not ISA.
- **2.6.2 Warm reset (`/RES`):** **Decision:** `MMU_EN = 0`, enhanced mode off, `PRIV` reset, TLB flush, identity fetch of `$00FFFC`. `VBR` is unused until the next `MMUE`.
- **2.6.3 External `/ABORT`:** **Decision:** There will never be an external `/ABORT`. Fault cause need not distinguish pin vs walk.
- **2.6.4 Preemption timer:** **Decision:** Not ISA. Initially IIgs motherboard VBL; later an FPGA timer on the card.

---

## 3. Gaps for Context 2: New GS-Compatible Computer

In a standalone FPGA/ASIC GS-compatible machine built around this CPU+MMU, you control the whole memory fabric — which exposes a different set of architectural gaps.

### 3.1 Undefined 32-bit physical map & FPI/Mega II coexistence — **DECIDED**
**Decision:** Fabric/platform, not ISA (same as 2.2). The CPU produces 32-bit PAs; the machine map (legacy window vs flat RAM vs MMIO) is not part of the 65816 enhancement.

### 3.2 Legacy Apple IIe / ProDOS 8 virtualization (`E = 1`) — **DECIDED**
**Decision:** Out of scope (1.3.2). `XCE` that would set `E = 1` under `MMU_EN = 1` faults. No sandboxed IIe/P8 under translation.

### 3.3 Split `L1_Sup`/`L1_User` + 32-bit PA > 24-bit VA kernel ergonomics — **DECIDED**
- **3.3.1 copyin / sharing:** **Decision:** No load-as-user `WDM`. Kernel maps a window in `L1_Sup` when it must touch user memory (`TLBI` to drop the window). Usual microkernel path: **shared pages between two user processes** (same PA in two `L1_User` tables); kernel is not on the copy path.
- **3.3.2 `COP`/`BRK` signature:** **Decision:** Hardware **latches** the signature byte (`$nn`) into a supervisor-readable register on `COP`/`BRK`. Kernel does not fetch `(user PBR:PC-1)` from `L1_User`. Read via 2-byte `WDM` (opcode later).
- **3.3.3 High physical memory:** **Decision:** 24-bit VA / 32-bit PA is the architecture. Kernel uses windows + `TLBI`; no extra ISA.

### 3.4 Missing PTE attributes & forward-compatibility polarity — **DECIDED**
Cache-inhibit: **see 2.4** (PTE bit 28, `0` = cacheable, `1` = uncached; fabric also forces `$E0C000` / `$E1C000`). Write-through / write-combining deferred.

- **3.4.1 Execute:** **Decision:** **`NX`** = PTE bit 27. `0` = fetch allowed, `1` = instruction fetch faults. User and supervisor. L2 walk: NX if either level is `1`. Fetch only (`VPA`); data ignores `NX`.
- **3.4.2 Accessed / Dirty:** **Decision:** v1 is **RAM-resident only** (no swap / file pager). Hardware does **not** implement `A`/`D` (no PTE writeback). Bits reserved in the unused payload / TLB attr field, write `0`, not architecturally exposed. When a pager exists, **software** `A`/`D`: `W=0` and fault-on-first-write for dirty; `V=0` (or equivalent) faults for accessed. Kernel bookkeeping, then `W=1` / `V=1` + `TLBI`.
- **3.4.3 Remainder:** **Decision:** other unused payload bits write-`0`, ignored.
`TwoLevelPageTables.md` reserves 8 attribute bits in the TLB (`TLB[7:0]`) and leaves `PTE[27:20]` as `0` in leaf PTEs, deferring `X`, cacheable/device, `A`, and `D`:

- **Cacheability / ordering is mandatory for hardware:** A fast new computer with SDRAM/DDR + L1 cache + MMIO peripherals + DMA needs per-page cache/device attributes (at least *Cacheable RAM* vs *Uncached Device/MMIO*, plus ideally *Write-Through / Write-Combining* for framebuffers) — and cache maintenance instructions (or hardware DMA coherency) if DMA isn't snooped.
- **Set the `0`-polarity contract now:** Since current software writes `0` to `PTE[27:20]`, future leaf attributes placed there will only be backwards-compatible if `0` is the safe default (e.g., `0 = cacheable, 1 = uncacheable/device`; `0 = executable, 1 = NX`). Decide and freeze that convention early.
- **Clarify `A`/`D` bit strategy:** `TwoLevelPageTables.md` (line 194) implies hardware writeback of `A`/`D` into PTEs on eviction, which adds significant FPGA complexity (read-modify-write page-table updates and race conditions with software PTE edits). Software-emulated `A`/`D` (via `V=0` / `W=0` faults) is far simpler in hardware and worth choosing explicitly.

### 3.5 Missing system-level exceptions & boundaries — **DECIDED**
- **3.5.1 Unmapped PA:** **Decision:** `ABORT` (same path as not-present/protection), including during a page-table walk. Cause register distinguishes empty PA vs PTE permission as needed (layout TBD with other fault bits).
- **3.5.2 Trap-entry registers:** **Decision:** Stock 65816. `D`, `DBR`, `M`, `X` (and the rest) stay as the interrupted context. Handler stores them into the process control block and restores them before `RTI` to user. Not special-cased in hardware.
- **3.5.3 SMP:** **Decision:** **Single-core.** No CAS/LL-SC, no barriers, no TLB shootdown in this ISA.

---

## 4. Prioritized Checklist

| Priority | Item | Scope | Status |
|---|---|---|---|
| **P0** | **1.1** `PRIV` inert until enhanced mode (distinct from `MMU_EN`); nesting & `STPV` are kernel contract | Both | Decided |
| **P0** | **1.2** Stock `/ABORT` restart; `MVN`/`MVP` whole-instruction retry; double-fault halt; fault `PC` on stack + cause/VA in a register. User `WAI` not page-fault (`PC`/vector deferred). Exception priority still open. | Both | Decided (WAI deferred) |
| **P0** | **1.5** Enhanced mode: no phantom cycles. TLB flush on `RESET`/enhanced/`MMUD`/`MMUE`. Size-change = software contract. TLB ≥ 32 entries. Drop `U`. `TLBI` added (opcode deferred). PTE little-endian, `V` last/first kernel contract. Prefetch drain. | Both | Decided |
| **P0** | **2.4** Fabric uncached `PA $E0C000` and `$E1C000`. PTE bit 28: `0`=cacheable, `1`=uncached; OR of L1 and L2. | Both | Decided |
| **P1** | **2.5** Slot DMA incompatible (AppleSqueezer-class). All RAM/page tables onboard; motherboard RAM ignored. | Accel | Decided |
| **P1** | **2.6** Reset: `MMU_EN=0`, enhanced off, `PRIV` reset, TLB flush, `$00FFFC`. No external `/ABORT`. Timer is motherboard VBL then FPGA, not ISA. 1 MHz is interface. | Accel | Decided |
| **P0** | **2.1** Soft 65816 FPGA/ASIC in the IIgs CPU socket; not a W65C816S wrap. | Accel | Decided |
| **P0** | **2.2** PA decode, socket vs onboard RAM, discovery: FPGA/card fabric, not ISA. | Accel | Decided (out of ISA) |
| **P0** | **2.3** `VBR` = 32-bit PA, 12-byte native table, untranslated when `MMU_EN=1`. FPI does not use `VPB`. Shadow/display is interface logic. Legacy sub-page I/O/slot masking later. Native `D`/`S` anywhere in bank 0. | Accel | Partial |
| **P1** | **1.4** No 5-byte `WDM` (`$80–$FF` = 4 bytes). Standard = stock 65816; enhanced ≠ `MMU_EN`. Pre-`MMUE` ops are 2-byte. Wrong `M`/`X` is programmer error. `CPUID` is 2-byte (`LDA #0` / `CPUID` / `A` changed). Enter-enhanced mnemonic TBD. | Both | Decided |
| **P1** | **3.1** PA map / FPI coexistence: fabric, not ISA. | New computer | Decided (out of ISA) |
| **P1** | **3.2** `E=1` under MMU out of scope (1.3.2). | New computer | Decided |
| **P1** | **3.3** Window in `L1_Sup` + `TLBI`; usual IPC is user–user shared pages. Latch `COP`/`BRK` `$nn` in a register. 24-bit VA / 32-bit PA is the model. | Both | Decided |
| **P2** | **1.3** `MMUE` NOP if `E=1`; `XCE` that would set `E=1` under MMU faults. IIe/P8 under MMU out of scope. | Both | Decided |
| **P2** | **3.4** `NX` = bit 27. `A`/`D` reserved, not implemented; software fault-on-first-write later. Unused bits write-0. | Both | Decided |
| **P2** | **3.5** Empty PA → `ABORT`. Trap saves `D`/`DBR`/`M`/`X` in process context (stock 65816). Single-core. | Both | Decided |

---

## 5. Parked / TBD (not blocking the ISA sketch)

| Item | Notes |
|---|---|
| Enter-enhanced mnemonic | Named `ENHN` in the spec (signature still later) |
| `WDM` signatures | All mnemonics including `CPUID`, `TLBI`, `VBRW`/`VBRR`, `COPSIG` |
| `CPUID` payload in `A` | Discovery is `LDA #0` / `CPUID` / `A` changed (1.4.3) |
| `TLBI` match rule | 4 KB vs superpage; which table's tag (1.5.2) |
| User `WAI` vector / `PC` | Not page-fault `ABORT`; yield path deferred (1.2.4) |
| Exception priority | `RESET` vs MMU `ABORT` vs `NMI` vs `IRQ` (1.2.4) |
| Fault register layout | VA + cause (1.2.5); empty-PA vs `V`/`W`/`NX` (3.5.1) |
| 2.3.3 Legacy sub-page mask | `$C000` I/O vs slot ROM etc.; later |
| Spec file edits | Applied in `65816MMUInstructions.md` and `TwoLevelPageTables.md` |

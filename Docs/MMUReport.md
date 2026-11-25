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
- **1.5.3 `U` and `G`:** **Decision: drop `U`.** Privilege is which L1 is walked (user never walks `L1_Sup`). Walk checks are `V` then `W` only. PTE bit 28 is reserved, write `0` (safe polarity for a later cache-inhibit or execute bit). No privilege fault type. `G` is not a PTE bit; `L1UW` drops user-tagged TLB rows, kernel-private rows survive by table tag. Kernel contract: do not leave process-A aliases in `L1_Sup` across `L1UW` (flush or unmap the copy window, or do not map user pages into `L1_Sup`). Spec updates needed in `TwoLevelPageTables.md` (flag table, walk, TLB fill) and `65816MMUInstructions.md` (privilege-fault mentions).
- **1.5.4 PTE endianness & torn writes:** **Decision:** Little-endian (everything on the 65816 is). Kernel contract: set `V` last, clear `V` first, so a walker never observes a torn 32-bit PTE. Hardware does not special-case this.
- **1.5.5 Serialization:** **Decision:** `MMUE`, `MMUD`, `L1SW`, `L1UW`, `STPV` drain prefetch; the next fetch uses the new mode/table. `L1SW` while already translating has the same “standing on a mapped identity/valid page” requirement as `MMUE`.
- **Fault CSRs:** See 1.2.5 (`PC` on stack; VA/cause in a register).

---

## 2. Gaps for Context 1: Apple IIgs CPU-Socket Accelerator Card

An accelerator card sits in the IIgs 40-pin 65C816 socket in front of the motherboard **FPI/CYA**, **Mega II**, **ROM**, and **expansion slots**.

### 2.1 Requires a custom soft-core / ASIC 65816 (cannot use a physical W65C816S)
Unlike the earlier [MemoryProtection.md](MemoryProtection.md) concept, `65816MMUInstructions.md` requires custom `WDM` register transfers (`C`/`X`/`A`), `SSP` swapping, user-mode `I`-flag bypass, and user `WAI`/`STP`/`XCE` trapping. A real W65C816S chip cannot do any of these externally; the accelerator **must** implement the CPU core itself (e.g., in an FPGA).

### 2.2 No physical address map partitioning (32-bit PA vs 24-bit IIgs socket)
The IIgs CPU socket only has 24 address bits (`$00:0000–$FF:FFFF`), of which FPI decodes `$00–$7F` (up to 8 MB motherboard/slot RAM), `$E0–$E1` (Mega II), and `$FC–$FF` (ROM).

- **Missing routing specification:** How does `PA[31:0]` decide whether an access targets **onboard accelerator RAM/peripherals** vs the **IIgs motherboard bus**?
- **Bootstrapping & RAM discovery:** At reset (`MMU_EN = 0`), only `PA[31:24] = $00` is reachable, so initial `L1_Sup`/`L2` tables must be built in the low 16 MB. There is no register/table for the OS to discover how much onboard accelerator RAM exists above 16 MB or where motherboard vs onboard regions sit in the 4 GB PA space.

### 2.3 Collision with motherboard FPI/CYA & Mega II
Every bus cycle that leaves the CPU socket is re-interpreted by the motherboard FPI/CYA according to `$C035` (Shadow/IOLC), `$C036` (Speed/all-bank shadow), `$C068` (State/LC/ALTZP/RAMRD/RAMWRT), `$C029` (NewVideo), and `VPB`:

- **Vector pull (`VPB`) hijacking:** When `MMU_EN = 1` traps to `$00FFE4–$00FFEE`, if that translated `PA` exits onto the motherboard with `VPB=0` (and `$C035` bit 6 = 0), the FPI will override it and return **Apple IIgs ROM's vectors (`$FF/FFE4–$FF/FFEE`)** instead of the OS's vectors. The accelerator must suppress `VPB` (or serve kernel vector pages from onboard RAM).
- **I/O & Shadowing interference:** Mapping a virtual page to motherboard bank `$00` or `$01` subjects it to FPI `$C035`/`$C068` rules; mapping I/O to `$E0/C000` avoids `$C035` bit 6, but `$E0/$E1` accesses are forced to 1 MHz.
- **4 KB page size vs IIgs bank `$00` layout:** In Apple IIgs compatibility, Zero Page (`$0000–$00FF`), Stack (`$0100–$01FF`), and Text Page 1 (`$0400–$07FF`) all share the **same 4 KB page** (`$0000–$0FFF`), and I/O (`$C000–$C0FF`) shares a 4 KB page with Slot/Expansion ROMs (`$C100–$CFFF`). Fast onboard RAM vs video-shadowed writes inside `$0000–$0FFF` cannot be distinguished using 4 KB PTE flags alone — legacy shadowing still requires sub-page accelerator shadowing logic (just like TransWarp GS / ZipGSX).

### 2.4 Missing cache-inhibit / device-memory attribute in PTEs
An accelerator running at 14–100+ MHz with onboard RAM/cache **cannot** function without strict uncached/device handling for motherboard I/O:

- Reading Apple II soft switches (`$C000`, `$C010`, `$C030` speaker, `$C081`/`$C083` LC double-read, IWM `$C0E0–$C0EF`, SCC, Ensoniq) has hardware side effects.
- Caching, speculative reads, write buffering, or multi-byte burst line fills touching `$C0xx` will break IIgs hardware. Without a PTE cache-inhibit/device bit (or strict PA-range uncacheable decoding), the OS cannot safely map I/O or video buffers.

### 2.5 Motherboard DMA, bus arbitration, and page-table placement
- **Slot DMA cannot see onboard accelerator RAM:** IIgs DMA cards (e.g., Apple II High-Speed SCSI, RamFAST) assert `/DMA` and access motherboard RAM (`$00–$7F`, `$E0/$E1`) directly. The OS must know which PA range is DMA-reachable (motherboard RAM) for bounce buffers, and the accelerator must snoop or invalidate cached motherboard lines during `/DMA` (`BE=0`).
- **Page tables in motherboard RAM:** If software places `L1` or `L2` in motherboard RAM, a 32-bit PTE read takes 4 slow 8-bit socket cycles (and must stall/arbitrate properly with `PHI2`, `RDY`, DRAM refresh, and `BE`). Document whether page tables are restricted to onboard RAM or allowed anywhere.

### 2.6 Timing, Warm Reset, External `/ABORT`, and Preemption Timer
- **1 MHz sync & IWM motor-on slowdown:** When executing fast code from onboard RAM, accessing IWM floppy registers (`$C0E0–$C0EF`) still requires snooping `$C036` bits 0–3 / `$C0x8`–`$C0x9` (or an explicit speed switch) so floppy timing loops don't run too fast.
- **Warm Reset (`/RES` / Ctrl-OpenApple-Reset):** Must guarantee clearing `MMU_EN = 0`, resetting `PRIV`, flushing the TLB, and routing `$00FFFC` back to motherboard ROM so the machine recovers cleanly.
- **External `/ABORT` pin vs MMU `ABORT`:** Need a fault-status bit to distinguish a motherboard `/ABORT` assertion from an internal MMU fault.
- **Preemption timer:** A stock IIgs only has VBL (60 Hz) and 1-sec IRQs, both tightly coupled to Mega II/ROM. A multitasking OS on an accelerator wants an independent onboard timer IRQ (card-level requirement).

---

## 3. Gaps for Context 2: New GS-Compatible Computer

In a standalone FPGA/ASIC GS-compatible machine built around this CPU+MMU, you control the whole memory fabric — which exposes a different set of architectural gaps.

### 3.1 Undefined 32-bit physical map & FPI/Mega II coexistence
- **Bottom 16 MB collision:** At reset (`MMU_EN = 0`), `PA = $00_000000–$00_FFFFFF` must look like an Apple IIgs (FPI shadowing, Mega II `$E0/$E1`, `$C0xx` I/O, ROM `$FC–$FF`) to boot legacy software.
- **Unspecified `MMU_EN = 1` physical bus semantics:** When `MMU_EN = 1`, does `PA` go to a **clean, flat 32-bit physical bus** (bypassing FPI shadowing/LC/vector-pull interception), with Mega II / legacy I/O living at dedicated physical addresses?
  - If FPI stayed active on `PA = $00_000000–$00_01FFFF` with `MMU_EN = 1`, native processes allocated physical RAM in the bottom 128 KB would be corrupted by LC/shadowing/I/O rules.
  - Conversely, if FPI is completely bypassed when `MMU_EN = 1`, running a legacy GS/OS or Apple IIe environment inside a protected process requires either a dedicated physical window for Mega II/legacy I/O or a clean mode/hypervisor boundary.
- **Native peripheral & boot architecture:** No conventions yet for where native peripherals (GPU/SecondSight, storage, Ethernet, audio, timers, interrupt controller), extended DRAM, and native vs legacy boot ROMs live in the 4 GB PA space.

### 3.2 Legacy Apple IIe / ProDOS 8 virtualization (`E = 1`)
Because `XCE` faults in **both** user and supervisor mode whenever `MMU_EN = 1`, a new GS-compatible computer cannot run ProDOS 8 or Apple IIe software (or IIgs ROM routines that use `SEC; XCE`) under the multitasking OS — forcing a hard reboot/mode switch to `MMU_EN = 0`.

- To run sandboxed Apple IIe / GS/OS VMs under a native OS on this machine, either allow `E = 1` in user mode (auto-switching to `E = 0` on traps and saving `E` in `PRIV` or a CSR) or explicitly document that `E = 1` is unsupported under `MMU_EN = 1`.

### 3.3 Split `L1_Sup`/`L1_User` + 32-bit PA > 24-bit VA kernel ergonomics
This combination creates three concrete bottlenecks for a native OS on a 32-bit PA machine:

1. **`copy_from_user` / `copy_to_user`:** With separate `L1_Sup` and `L1_User` tables, the kernel cannot directly dereference user pointers passed in syscalls. Walking `L1_User` in 65816 software, temporarily mapping a page into `L1_Sup`, and calling `L1SW` (flushing the entire supervisor TLB) for every syscall buffer will dominate syscall latency. A `WDM` load/store-as-user instruction (or a fast per-VA TLB invalidate + dedicated kernel copy window) is almost mandatory.
2. **`COP #$nn` / `BRK #$nn` signature byte:** On a 65816 syscall via `COP #$nn`, the signature byte sits at `(user PBR:PC - 1)` in **`L1_User`**, not `L1_Sup`. Either the CPU should latch the `COP`/`BRK` signature byte into a supervisor CSR on trap, or the OS ABI must ignore the signature byte and pass syscall numbers in registers (`A`/`X`).
3. **Managing >16 MB physical RAM from a 24-bit kernel:** Unlike 64-bit kernels, a 24-bit `L1_Sup` cannot permanently direct-map 64 MB–4 GB of physical RAM. The kernel must frequently create/teardown temporary 4 KB mappings in `L1_Sup` to zero pages, manage page tables above 16 MB, and copy I/O buffers — making **per-VA TLB invalidation (`INVLPG`/`TLBI`)** essential so kernel page management doesn't constantly nuke the entire supervisor TLB.

### 3.4 Missing PTE attributes & forward-compatibility polarity
`TwoLevelPageTables.md` reserves 8 attribute bits in the TLB (`TLB[7:0]`) and leaves `PTE[27:20]` as `0` in leaf PTEs, deferring `X`, cacheable/device, `A`, and `D`:

- **Cacheability / ordering is mandatory for hardware:** A fast new computer with SDRAM/DDR + L1 cache + MMIO peripherals + DMA needs per-page cache/device attributes (at least *Cacheable RAM* vs *Uncached Device/MMIO*, plus ideally *Write-Through / Write-Combining* for framebuffers) — and cache maintenance instructions (or hardware DMA coherency) if DMA isn't snooped.
- **Set the `0`-polarity contract now:** Since current software writes `0` to `PTE[27:20]`, future leaf attributes placed there will only be backwards-compatible if `0` is the safe default (e.g., `0 = cacheable, 1 = uncacheable/device`; `0 = executable, 1 = NX`). Decide and freeze that convention early.
- **Clarify `A`/`D` bit strategy:** `TwoLevelPageTables.md` (line 194) implies hardware writeback of `A`/`D` into PTEs on eviction, which adds significant FPGA complexity (read-modify-write page-table updates and race conditions with software PTE edits). Software-emulated `A`/`D` (via `V=0` / `W=0` faults) is far simpler in hardware and worth choosing explicitly.

### 3.5 Missing system-level exceptions & boundaries
- **Bus Error / Unmapped PA fault:** If a PTE points to unpopulated physical memory (e.g., `PA = $80_000000` on a 64 MB machine) — either on a normal access or during a page-table walk — the spec defines no Bus Error / Machine Check behavior.
- **Trap-entry register state:** Standard 65816 traps leave `DP`, `DBR`, `M`, and `X` holding user-controlled values on kernel entry. Fine (standard 65816), just worth noting in the trap contract so kernel entry prologues don't touch `DP`/`DBR` before resetting them.
- **SMP / multicore boundary:** There is no atomic CAS/LL-SC, memory barrier, or cross-core TLB shootdown. Assuming single-core is sensible for a 65816 machine, just worth stating explicitly as a design boundary.

---

## 4. Prioritized Checklist

| Priority | Item | Scope | Status |
|---|---|---|---|
| **P0** | **1.1** `PRIV` inert until enhanced mode (distinct from `MMU_EN`); nesting & `STPV` are kernel contract | Both | Decided |
| **P0** | **1.2** Stock `/ABORT` restart; `MVN`/`MVP` whole-instruction retry; double-fault halt; fault `PC` on stack + cause/VA in a register. User `WAI` not page-fault (`PC`/vector deferred). Exception priority still open. | Both | Decided (WAI deferred) |
| **P0** | **1.5** Enhanced mode: no phantom cycles. TLB flush on `RESET`/enhanced/`MMUD`/`MMUE`. Size-change = software contract. TLB ≥ 32 entries. Drop `U`; bit 28 reserved 0. `TLBI` added (opcode deferred). PTE little-endian, `V` last/first kernel contract. Prefetch drain. Cache-inhibit is 2.4 / 3.4. | Both | Decided |
| **P0** | **2.2 / 2.3 / 3.1** Define 32-bit PA map partitioning and `MMU_EN=1` interaction with FPI/Mega II (`$C035`, `$C068`, `VPB`, `$E0/$E1`, DMA) | Accel + New Computer | Open |
| **P1** | **1.4** No 5-byte `WDM` (`$80–$FF` = 4 bytes). Standard = stock 65816; enhanced ≠ `MMU_EN`. Pre-`MMUE` ops are 2-byte. Wrong `M`/`X` is programmer error. `CPUID` is 2-byte (`LDA #0` / `CPUID` / `A` changed). Enter-enhanced mnemonic TBD. | Both | Decided |
| **P1** | **3.3** Solve kernel ↔ user data access (load/store-as-user `WDM` or fast `TLBI` + window) and `COP` signature-byte access | Both | Open |
| **P2** | **1.3** `MMUE` NOP if `E=1`; `XCE` that would set `E=1` under MMU faults. IIe/P8 under MMU out of scope. | Both | Decided |
| **P2** | **3.2 / 3.4 / 3.5** `A`/`D` strategy, Bus Error, feature-ID payload. (`3.2` E=1 VMs: out of scope per 1.3.2. `U`/`G` dropped per 1.5.3. PTE endianness per 1.5.4.) | Both | Open |

# GSSquared MMU Architecture & Specification (Phase 1)
## Reference Design for 65816-based Apple IIgs Compatible Systems

This document specifies the software design for the Memory Management Unit (MMU) of the **GSSquared** emulator. The architecture is optimized for a low-overhead software implementation while remaining strictly implementable in hardware (e.g., FPGA) to interface with a physical 65816 processor.

---

## 1. Architectural Overview & Core Parameters

The 65816 processor natively exposes a 24-bit address space (16 MB). This MMU translates 24-bit virtual addresses (VA) into a broader 32-bit physical address space (PA), facilitating expansion up to 4 GB of physical memory, I/O devices, and expanded buffers. 

Instead of rigid, hardwired memory shadowing for backward compatibility, this architecture utilizes a flexible page translation strategy to map legacy Apple II environments cleanly.

### Core Parameters
*   **Virtual Address Space:** 24-bit (16 MB total, formatted as `Bank:Address`).
*   **Physical Address Space:** 32-bit (4 GB total).
*   **Page Sizes Supported:** 
    *   **Standard Page:** 4 KB (4096 bytes).
    *   **Large Page:** 64 KB (65536 bytes) — corresponds exactly to a native 65816 memory bank.
*   **Address Translation Breakdown (4 KB Standard Page):**
    *   `VA[23:12]` (12 bits) = Virtual Page Number (VPN).
    *   `VA[11:0]` (12 bits) = Page Offset.
    *   `PA[31:12]` (20 bits) = Physical Page Number (PPN).
    *   `PA[11:0]` (12 bits) = Physical Offset (identical to Virtual Offset).

---

## 2. In-Memory Data Structures

To keep the hardware/emulator page table walker as simple as possible, the MMU uses a single-level flat page table per process. A complete 16 MB address space is fully defined by an array of 4,096 Page Table Entries (PTEs).

### 2.1 Page Table Size Overhead
*   **Entries per Table:** 4,096
*   **Size per Entry:** 4 bytes (32-bit word)
*   **Total Footprint per Process Table:** 16 KB ($4096 \times 4\text{ bytes}$).

### 2.2 Page Table Entry (PTE) Format (32-bit Word)

31                                 12 11        7 6   5   4   3   2   1   0
+-------------------------------------+-----------+---+---+---+---+---+---+
|       Physical Page Number (PPN)     | Reserved  | L |PID| X | W | R | V |
|               20 bits               |  5 bits   |Siz|Mch|Exc|Wrt|Red|Val|
+-------------------------------------+-----------+---+---+---+---+---+---+


#### Field Descriptions:
*   **PPN (Bits 31-12):** The 20-bit target base address in physical memory. For a 64 KB page alignment, the lower 4 bits of this field (`PPN[3:0]`) must be written as 0 by the kernel.
*   **Reserved (Bits 11-7):** Reserved for future hardware expansion. Must be written as 0.
*   **L / Size (Bit 6):** Large Page Identifier.
    *   `0` = Standard 4 KB Page.
    *   `1` = Large 64 KB Page.
*   **PID_MATCH (Bit 5):** Process ID Match flag.
    *   `1` = Enforces strict PID verification against the MMU's active PID register.
    *   `0` = Global Page. Visible across all process contexts (useful for shared system vectors and the GS/OS kernel).
*   **X / Execute (Bit 4):** Instruction fetch permission. Enforces that cycles where the 65816 asserts code execution signals (e.g., `VPA` & `VDA` high simultaneously) are valid.
*   **W / Write (Bit 3):** Write permission (`1` = Read/Write, `0` = Read-Only).
*   **R / Read (Bit 2):** Read permission (`1` = Readable, `0` = No-Access).
*   **V / Valid (Bit 1-0):** Validity and cache characteristics.
    *   `00` = Invalid (Triggers fault).
    *   `01` = Valid, Cacheable (Standard RAM).
    *   `10` = Valid, Non-Cacheable (Dedicated I/O pages like `$00C000–$00CFFF`).

### 2.3 Variable Page Size via PTE Splatting (Method 1)
To prevent the hardware page table walker from needing complex multi-step lookup logic, Large Pages (64 KB) are handled via **PTE Splatting**:
1.  When allocating a 64 KB Large Page (e.g., mapping a full 65816 bank), the kernel fills **16 consecutive entries** in the flat page table with the exact same 32-bit PTE value.
2.  The `L` bit (Bit 6) is set to `1` in all 16 entries.
3.  When the hardware page walker fetches a entry on a TLB miss, it reads a single entry. If `L = 1`, it instructs the TLB to load the entry as a unified 64 KB masked translation.

---

## 3. Translation Lookaside Buffer (TLB) Specification

The TLB caches active address translations within the emulator and hardware layers to avoid a 16 KB memory walk on every clock cycle.

### 3.1 TLB Entry Layout
Each entry in the TLB consists of a Tag match evaluation field and a Data translation field:

*   **Tag:** 
    *   `VPN` (12 bits)
    *   `PID` (16 bits)
    *   `Valid` (1 bit)
*   **Mask:**
    *   `Size_Mask` (12 bits): Evaluated during lookup.
        *   If `L = 0`: Mask is `0xFFF` (All 12 bits of VPN must match).
        *   If `L = 1`: Mask is `0xFF0` (Only the upper 8 bits—the Bank—must match).
*   **Data:**
    *   `PPN` (20 bits)
    *   `Flags` (X, W, R, Valid status)

### 3.2 Hardware Page Table Walker Logic
On a TLB miss, the hardware/emulator performs a single atomic memory read:
1.  Compute entry physical address: `PTE_Address = Page_Table_Root_Pointer + (VA[23:12] << 2)`.
2.  Fetch the 32-bit word from `PTE_Address`.
3.  Evaluate the entry:
    *   If `V == 00` or permissions fail, assert the processor `/ABORT` pin.
    *   If valid, inject the entry into the TLB. If `L == 1`, set the TLB entry mask to `0xFF0` so that any subsequent hits inside this 64 KB bank resolve instantly.

---

## 4. Memory Protection & Fault Handling (`/ABORT`)

The 65816 `/ABORT` pin is leveraged strictly for **Memory Protection and Application Sandboxing**. 

1.  **Violation Detection:** If an active application (`PID != 0`) attempts to execute an invalid operation (e.g., writing to a page marked Read-Only, executing from a non-X page, or accessing a non-global page assigned to a different PID), the MMU drives the `/ABORT` line low before the conclusion of the clock cycle.
2.  **Instruction Abort:** The current bus cycle is prevented from modifying memory or internal registers.
3.  **Kernel Intervention:** The 65816 traps through the hardware `/ABORT` vector. Because `PID 0` handles the exception, the system safely captures the fault, identifies the offending PID, cleans up its memory allocation tables, and terminates the application context without destabilizing the rest of the GS/OS environment.

---

## 5. Configuration & Register Interface

The MMU configuration interface is memory-mapped into a reserved window within the standard Apple II I/O space at **`$00C0B0–$00C0BF`**. 
TODO: this is a bad choice. it looks like it wants 10 bytes. 

> **Access Control Rule:** These registers are strictly privileged. The physical hardware/emulator will ignore writes or return zeroed reads for any operation attempted while the active PID register is not set to `0x0000`.

### Register Layout

| Address | Register Name | Access | Width | Description |
| :--- | :--- | :--- | :--- | :--- |
| `$00C0B0` | `MMU_CTRL` | R/W | 8-bit | Bit 0: MMU Enable (`1` = On, `0` = Off/Bypass)<br>Bits 1-7: Reserved |
| `$00C0B2` | `MMU_ACTIVE_PID` | R/W | 16-bit| Holds the currently executing Process ID. |
| `$00C0B4` | `MMU_PTR_LOW` | R/W | 16-bit| Bits [15:0] of the Physical Page Table Root Pointer. |
| `$00C0B6` | `MMU_PTR_HIGH` | R/W | 16-bit| Bits [31:16] of the Physical Page Table Root Pointer. |
| `$00C0B8` | `MMU_TLB_FLUSH` | Write | 16-bit| **TLB Invalidate Page:** Writing a 16-bit Virtual Address (`Bank:Page`) to this register instantly flushes that translation from the TLB. |
| `$00C0BA` | `MMU_TLB_PURGE` | Write | 8-bit | **Global TLB Purge:** Writing any value to this register completely flushes all non-global entries from the TLB. |

---

## 6. Legacy Apple II Compatibility Mapping

To maintain full compatibility with Apple II environments, banks `$E0` and `$E1` are explicitly retained for legacy Mega II emulation and sound graphics architectures. 

Instead of routing these through rigid alternative hardware logic blocks, the `PID 0` operating system initialization routine handles this natively via the MMU by populating the master page tables to map virtual banks `$E0` and `$E1` directly to their respective dedicated legacy controller locations within the 32-bit physical space. The `$00C000–$00CFFF` page is mapped with the `V` bits set to `10` (Valid, Non-Cacheable) to guarantee hardware soft-switches bypass any TLB caching loops.

---

## 7. Implementation Plan: GSSquared Software Realization

> **Status:** Design agreed; not yet implemented. This section is the actionable plan for adding the Advanced MMU to the emulator and reconciling it with the existing FPI/Mega II model. It supersedes the hardware-centric framing above wherever the two differ for the *software* implementation.

### 7.1 Key Conclusions

1. **The existing `MMU_IIgs` is the FPI model, not this spec.** It is a software model of the real Apple IIgs FPI (shadowing, main/aux, language card, soft switches) at 64K granularity. The MegaII is a separate instance of MMU_IIe, contained by MMU_IIgs, which has a 256-byte page granularity to support legacy Apple II semantics. Its `page_table[]` is the *decoded output* of soft-switch state. This document's Advanced MMU is a **separate, additive subsystem** for 16-bit 65816-native, OS-managed multitasking — not a replacement for the FPI model.

2. **No TLB in v1.** A hardware TLB exists to avoid a slow page-table walk and to avoid storing a full decoded map on-die. The emulator has neither constraint. Because the V→P table is **single-level and flat**, the "walk" (§3.2) is literally *one indexed load*, cheap enough to perform on every access. A TLB would be a cache in front of something already as fast as the cache, plus invalidation risk. The TLB programming model (registers in §5) is retained on paper for FPGA/ABI parity, but in the emulator those registers drive eager recompute, not a cache. A real decoded cache is a *later, profiling-driven optimization only*.

2a. HOWEVER even given 2 above, we may want to require guest management of a TLB (which we would not use in GS2 emulation) so when we do eventually implement in hardware, guest OS code will already be set up for it.

3. **Walk, don't pre-decode per PID.** Resolution reads the compact 4-byte PTE directly from emulated RAM. There is no fat host-side decoded array per process.

4. **Two-object MMU + pointer swap, not a per-access `if`.** The CPU already dispatches every access through a single `virtual` call on `cpu_state.mmu` (see `src/cpu.hpp`), and CPU cores never cache a page base pointer (verified in `src/cpus/base_6502.cpp` — all access goes through `cpu->mmu->read()/write()`). Therefore mode is selected by swapping the `MMU *` object, giving each mode a straight-line `read`/`write` with **zero per-access mode branches**.

5. **Partition by virtual-RAM-vs-physical-hardware, not by bank number.** Bank `$00` (65816 stack + direct page are always bank 0) and `$01` and an app's code/data banks are **per-process virtual RAM**. `$E0`/`$E1` (Mega II) and `$Cxxx` I/O are **shared physical hardware** potentially reachable identically from every PID. The legacy shadowed mapping is just *one address space's flavor*, not a global mode — this is what makes per-process multitasking (private bank `$00`) possible.

### 7.2 Target Architecture (Two Layers)

| Layer | Structure | Scope | Owner | Size |
| :--- | :--- | :--- | :--- | :--- |
| **Virtual → Physical** | Flat 4-byte PTE table (§2), 4,096 entries | **Per-PID** | Guest OS, in emulated RAM, rooted by `MMU_PTR_*` | 16 KB / PID |
| **Physical → Host** | Fat `page_table_entry_t` (`read_p`/`write_p`/handlers) | **Machine-wide (shared)** | Emulator | Sized to installed physical memory (a few thousand entries) |

**Per-access path (`MMU_Advanced::read`/`write`):**

```
pte   = phys_ram[ pt_root + (vpn << 2) ]        // load the 4-byte guest PTE
if (!V || permission check fails) -> assert /ABORT
pa    = (pte.PPN << 12) | (va & 0xFFF)
entry = phys_map[ pa >> 12 ]                     // PA -> host (RAM ptr or device handler)
byte  = entry.read_p ? entry.read_p[pa & 0xFFF] : entry.read_h(...)
```

- **PID switch:** set `pt_root` (offset into guest physical RAM) + active PID. One store. Zero recompose, zero per-PID host allocation.
- **Mode switch (FPI ↔ Advanced):** `cpu->set_mmu(fpi_or_advanced)`. One store; safe mid-stream (no stale page-pointer cache).
- **Shared hardware:** `$E0`/`$E1` and `$Cxxx` are ordinary entries in the shared physical map carrying the existing Mega II handlers (`bank_e0_*`, `megaii_c0xx_*`). Native PIDs simply do not map `$Cxxx` into their virtual space (or map it to a protected/abort page).

### 7.3 Phased Work Items

**Phase 1 — Mapping only (no protection):**
- [ ] Add `MMU_Advanced : public MMU`, sharing the backing store (RAM/ROM) and the same Mega II object with `MMU_IIgs`.
- [ ] Build the **shared physical map** (machine-wide `page_table_entry_t` array): RAM pages → host pointers; ROM → read-only; `$E0`/`$E1`/`$Cxxx` physical pages → Mega II handlers.
- [ ] Implement the two-load walk in `MMU_Advanced::read`/`write` reading the 4-byte PTE from emulated RAM.
- [ ] Implement `$C0Bx` register handlers (`MMU_CTRL`, `MMU_ACTIVE_PID`, `MMU_PTR_LOW/HIGH`) that call into a machine/owner object to `cpu->set_mmu()` and to set `pt_root`/PID. (Note §5 TODO: pick a better register window than `$C0B0–$C0BF`.)
- [ ] Decode `L`-bit large pages (64 KB) per §2.3 splatting semantics during the walk.

**Phase 2 — Protection & sandboxing:**
- [ ] Extend the walk with `R`/`W`/`X`/`PID_MATCH`/`V` checks; route violations to a handler that asserts the 65816 `/ABORT` (§4).
- [ ] **Prerequisite:** the CPU core must distinguish opcode fetch from data read (VPA/VDA-equivalent) to enforce the `X` bit. Confirm/extend `base_6502.cpp` to signal access type to the MMU.
- [ ] Implement `MMU_TLB_FLUSH`/`MMU_TLB_PURGE` as recompute/invalidate hooks (no-ops for the walk model in v1; wired for the optional cache in Phase 3).

**Phase 3 — Optional performance cache (only if profiling justifies):**
- [ ] Add a small decoded cache (the actual "TLB") in front of the walk, invalidated by `TLB_FLUSH`/`PURGE`. Consider a fully-decoded host map or a SIMD/struct-of-arrays associative buffer if a finite TLB is desired for fidelity.

### 7.4 Open Decisions
- **Register window:** replace `$C0B0–$C0BF` (§5 TODO) with a cleaner privileged window.
- **Global vs hybrid scope at boot:** whether the entire space resolves through PTEs once enabled (doc §6, `PID 0` replicates legacy mapping in PTEs) or legacy banks remain on the FPI model until the OS opts a region into Advanced mode.
- **Granularity reconciliation:** Advanced uses 4 KB pages; FPI uses 64KByte, MegaII 256-byte.
- **Physical map representation:** flat array sized to installed RAM vs sparse/region-allocated for a large 32-bit PA space. (Probably flat space; even allocating a 64MB or 128MB chunk to a IIgs would be powerful).
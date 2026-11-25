# Two-level page tables

24-bit virtual address, 32-bit physical address, for a 65816 with a page-table MMU.

A 65816 virtual address is 24 bits (16 MB per address space). This MMU translates it to a 32-bit physical address (4 GB). Many 16 MB address spaces can sit in physical RAM at once, and peripherals can be mapped anywhere in the 4 GB.

Translation is two levels. A 64 KB superpage may end at level 1, one superpage per 65816 bank. Otherwise the level-1 entry points at a 16-entry level-2 table of 4 KB pages.

Every entry is the same 32-bit word, **little-endian**. Bits 31:27 are the flags, in the same positions at both levels. Bits 26:0 are the payload. Each use stores a right-aligned frame number there, and the frame is shifted left by the granule: 6 for a level-2 table, 16 for a superpage, 12 for a 4 KB page.

Privilege, `WDM` mnemonics, `L1_User` / `L1_Sup`, `VBR`, and `SSP` are [65816MMUInstructions.md](65816MMUInstructions.md). An earlier single-level layout (one 16 KB flat table, flags in the low bits) is [MemoryProtection.md](MemoryProtection.md). This document does not change that one. Design record: [MMUReport.md](MMUReport.md).

## Address spaces

| | Width | Range |
|---|---|---|
| Virtual address `VA[23:0]` | 24 | 16 MB |
| Physical address `PA[31:0]` | 32 | 4 GB |
| Level-1 superpage | 64 KB | one 65816 bank, `VA[15:0]` |
| Level-2 page | 4 KB | `VA[11:0]` |

`VA[23:16]` is the bank. It indexes level 1. There is no address-space id. Two level-1 bases: `L1_Sup` while supervisor, `L1_User` while user ([65816MMUInstructions.md](65816MMUInstructions.md)). Privilege is **which table is walked**, not a PTE user bit. A context switch writes `L1_User`. The TLB caches finished translations only and tags which table filled them. Changing a base drops every entry from that table.

## The PTE word

Bit 31 is the most significant bit of the 32-bit entry. The flags do not move.

| Bit | Name | Meaning |
|---|---|---|
| 31 | `V` | 1 = present. 0 = not-present `ABORT`. |
| 30 | `S` | Level 1: 1 = 64 KB superpage, 0 = pointer to a level-2 table. Level 2: ignored; write 0. |
| 29 | `W` | 1 = writes allowed. 0 = a write takes a protection `ABORT`. |
| 28 | `NC` | 1 = uncached / device. 0 = cacheable RAM. |
| 27 | `NX` | 1 = instruction fetch `ABORT`. 0 = fetch allowed. Data reads/writes ignore `NX`. |

There is no `U` bit. User mode never walks `L1_Sup`.

`NC` and `NX` at level 1 apply to the whole 64 KB bank, including when `S = 0` (pointer). The level-2 leaf can only **raise** them (see walk). Polarity `0` is the safe default (cacheable, executable).

Accessed and dirty are **not implemented**. Those bits stay in the unused payload / TLB attr field, write 0, not architecturally exposed. A later pager uses software `A`/`D`: `W=0` fault-on-first-write for dirty, `V=0` faults for accessed, then `TLBI`.

## Three readings of bits 26:0

Bit 31 is at the left. A column is the same PTE bit in every row.

```text
               31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
L2 pointer    [V][0][W][NC][NX][0][                          L2FN = PA[31:6]                                 ]
64 KB super   [V][1][W][NC][NX][            zeros            ][               PSFN = PA[31:16]               ]
4 KB page     [V][0][W][NC][NX][      zeros      ][                     PPFN = PA[31:12]                     ]
```

The `S` column is the required value: 0 for a pointer, 1 for a superpage, 0 on a level-2 entry (ignored).

A frame number is an integer in the low bits of the payload. Its width is `32 - log2(granule)` counting from bit 0. Unused bits between the frame and `NX` are not read; write 0.

| Use | Granule | Frame | PTE bits | Shift |
|---|---|---|---|---|
| Level-2 table | 64 bytes | `PA[31:6]` (26 bits) | 25:0 | 6 |
| 64 KB superpage | 64 KB | `PA[31:16]` (16 bits) | 15:0 | 16 |
| 4 KB page | 4 KB | `PA[31:12]` (20 bits) | 19:0 | 12 |

### Level-1 pointer (`S = 0`)

A level-2 table is 16 entries by 4 bytes, so it is 64 bytes and lives on a 64-byte boundary. `PA[5:0]` of the table base is 0 and is not stored.

`PTE[25:0]` is the 26-bit table frame (L2FN), equal to `PA[31:6]`. The walker appends six zeros. The table may sit anywhere in the 4 GB. `PTE[26]` is unused; write 0. `PTE[27]` is `NX`.

```text
entry_pa = {PTE[25:0], VA[15:12], 2'b0}
```

That is the same value as `(PTE[25:0] << 6) + (VA[15:12] << 2)`. The add cannot carry, because the six appended bits are 0 and the index occupies only bits 5:2.

```text
               31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
L2 entry addr [                               PTE[25:0] << 6                               ][VA[15:12] ][ 0  ]
```

### Level-1 superpage (`S = 1`)

`PTE[15:0]` is the 16-bit superpage frame number (PSFN). It becomes `PA[31:16]`. `PTE[26:16]` is not read; write 0. There are 2^16 such frames, covering the 4 GB.

```text
PA[31:16] = PTE[15:0]
PA[15:0]  = VA[15:0]
```

### Level-2 page

`PTE[19:0]` is the 20-bit page frame number (PPFN). It becomes `PA[31:12]`. `PTE[26:20]` is not read; write 0. `PTE[30]` is not read; write 0. There are 2^20 such frames, covering the 4 GB.

```text
PA[31:12] = PTE[19:0]
PA[11:0]  = VA[11:0]
```

A 4 KB page, a 64 KB superpage, and a level-2 table can each sit anywhere in the 4 GB. The table is 64-byte aligned because the walker forces `PA[5:0]` of its base to 0.

## Virtual address

```text
               23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
64 KB super   [       L1 index       ][              offset → PA[15:0]               ]
4 KB page     [       L1 index       ][ L2 index ][        offset → PA[11:0]         ]
```

| VA bits | Width | Superpage path | 4 KB path |
|---|---|---|---|
| 23:16 | 8 | level-1 index | level-1 index |
| 15:12 | 4 | part of the 64 KB offset | level-2 index |
| 11:0 | 12 | part of the 64 KB offset | page offset, copied to `PA[11:0]` |
| 15:0 | 16 | copied to `PA[15:0]` | split, as above |

## Physical address

These columns are physical address bits, not PTE bits.

```text
               31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
superpage PA  [               PTE[15:0] << 16                ][                   VA[15:0]                   ]
4 KB PA       [                     PTE[19:0] << 12                      ][             VA[11:0]             ]
```

```text
PA_super = (PTE[15:0] << 16) | VA[15:0]
PA_4k    = (PTE[19:0] << 12) | VA[11:0]
```

An access to a PA with no backing (hole in the 4 GB map), including a walk that addresses an empty L2, is `ABORT`.

Physical `$E0C000` and `$E1C000` (Mega II I/O pages) are **always uncached** in the fabric, regardless of `NC`. Extra uncached PA ranges are platform-specific.

## Tables in memory

| Table | Entries | Index | Bytes | Placement |
|---|---|---|---|---|
| Level 1 | 256 | `VA[23:16]` | 1024 | `L1_User` or `L1_Sup`, a 32-bit physical address of entry 0. Place it on a 1024-byte boundary. |
| Level 2 | 16 | `VA[15:12]` | 64 | `PTE[25:0] << 6`. Anywhere in the 4 GB, 64-byte aligned by the shift. |

```text
L1_Base     = PRIV[0] ? L1_Sup : L1_User
L1_entry_pa = L1_Base + (VA[23:16] << 2)
```

On a 1024-byte boundary the index occupies `PA[9:2]` and `PA[1:0]` is 0, with no carry into `PA[10]`. The base is a register, not a PTE field, so it keeps a full 32-bit byte address. The level-1 table may sit anywhere in the 4 GB. With the MMU off, there is no walk; see [65816MMUInstructions.md](65816MMUInstructions.md).

One address space, if every bank is a superpage, costs the 1024-byte level-1 table. If every bank is split into 4 KB pages, it costs 1024 + 256 × 64 = 17408 bytes (17 KB).

PTEs are little-endian 32-bit words. **Kernel contract:** set `V` last, clear `V` first, so a walker never observes a torn entry. Hardware does not special-case this.

## Walk

The same tests run on a level-1 entry and, when the walk continues, on the level-2 entry. `write` is the cycle's access type. `fetch` is an instruction fetch (`VPA`). `L1_Base` is `L1_Sup` or `L1_User` as above.

In enhanced mode the CPU issues no phantom cycles (`VPA=0, VDA=0`); the walker only sees real accesses.

```text
L1 = load32(L1_Base + (VA[23:16] << 2))   ; empty PA → ABORT

if L1[31] == 0:                 not-present fault
if write and L1[29] == 0:       protection fault
if fetch and L1[27] == 1:       NX fault

if L1[30] == 1:
    PA = (L1[15:0] << 16) | VA[15:0]
    NC = L1[28]
    NX = L1[27]
    done

L2 = load32({L1[25:0], VA[15:12], 2'b0})  ; empty PA → ABORT

if L2[31] == 0:                 not-present fault
if write and L2[29] == 0:       protection fault
if fetch and L2[27] == 1:       NX fault

PA = (L2[19:0] << 12) | VA[11:0]
NC = L1[28] | L2[28]
NX = L1[27] | L2[27]
```

Level-1 `V`, `W`, `NC`, and `NX` apply to the whole 64 KB bank, including when `S = 0` and the entry is only a table pointer. A pointer with `W = 0` makes the bank read-only. A pointer with `NC = 1` or `NX = 1` forces uncached or no-execute for every page in that bank. The level-2 entry can only narrow further (`W` AND; `NC`/`NX` OR). The level-2 entry is not fetched if the level-1 check already faulted.

Check order is present, then write, then `NX` (on fetch). A write to a not-present entry is a not-present fault.

A walk that finishes installs one TLB entry. A level-2 pointer is not installed. The hit path below reproduces the physical address this walk would have produced.

## TLB

The TLB is a cache of leaves: a 64 KB superpage or a 4 KB page. The CPU does not read or write it. The walker fills it after the checks above. At least **32** entries. Entries are 32 bits, the same width as a memory PTE, with a different packing. `PA[31]` sits in bit 31 for both page sizes, so a hit is a concatenation and a 4-bit select. The shift from the right-aligned memory frame happens once, at fill.

The tag is not part of this word. It holds a valid bit, `VA[23:12]`, and which base filled the entry (`L1_User` vs `L1_Sup`). A 64 KB entry matches `VA[23:16]` only. A 4 KB entry matches `VA[23:12]`. A hit also matches `PRIV[0]`.

```text
               31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
64 KB entry   [                  PA[31:16]                   ][ from VA  ][1][W][NC][NX][     8 flags        ]
4 KB entry    [                        PA[31:12]                         ][0][W][NC][NX][     8 flags        ]
```

Bit 11 is `SZ`, shown as the required value: 1 for 64 KB, 0 for 4 KB. On a 64 KB entry, bits 15:12 are stored as 0 and are not used. The hit path takes those physical bits from the virtual address.

```text
PA[31:16] = TLB[31:16]
PA[15:12] = SZ ? VA[15:12] : TLB[15:12]
PA[11:0]  = VA[11:0]
```

`W` is the effective write permission (AND of the two levels). `NC` and `NX` are the effective attributes (OR of the two levels). There is no `G` bit; kernel leaves survive `L1UW` because they are tagged supervisor.

Bits 7:0 are eight attribute flags. The walker writes 0. Reserved for a later `A`/`D` (not implemented).

`RESET`, `ENHN`, `MMUE`, and `MMUD` flush the TLB. `L1UW` drops user-tagged rows; `L1SW` drops supervisor-tagged rows. `TLBI` drops one VA (match rule TBD). Changing a 64 KB superpage to an L2 pointer (or the reverse) without a flush is a **software contract** (invalidate first).

### Fill

```text
if L1[30] == 1:                          // superpage
    TLB[31:16] = L1[15:0]
    TLB[15:12] = 0
    TLB.SZ     = 1
    TLB.W      = L1[29]
    TLB.NC     = L1[28]
    TLB.NX     = L1[27]
else:                                    // pointer, then level-2 leaf
    TLB[31:12] = L2[19:0]
    TLB.SZ     = 0
    TLB.W      = L1[29] & L2[29]
    TLB.NC     = L1[28] | L2[28]
    TLB.NX     = L1[27] | L2[27]

TLB[7:0] = 0
tag.T    = PRIV[0]               // 1 = filled from L1_Sup
```

`TLB[31:12] = L2[19:0]` is the left shift: memory bits 19:0 move to TLB bits 31:12. `TLB[31:16] = L1[15:0]` moves memory bits 15:0 to TLB bits 31:16.

On a TLB hit, a write with `W=0` is a protection `ABORT`; a fetch with `NX=1` is an `NX` `ABORT`. Fabric still forces uncached at `$E0C000` / `$E1C000` even if `NC=0`.

## Software contract

- Write unused payload bits as 0. On a pointer that is `PTE[26]`. On a superpage that is `PTE[26:16]`. On a 4 KB page that is `PTE[26:20]` and `PTE[30]`. The walker does not read them.
- Allocate each level-2 table as 64 bytes at `L2FN << 6`. The walker supplies the six low zeros.
- Place each level-1 table on a 1024-byte boundary.
- Build both levels with one entry layout. The flag positions do not change.
- Set `V` last, clear `V` first (little-endian 32-bit stores).
- Flush or `TLBI` before changing a leaf from superpage to L2 (or the reverse).
- Do not leave process-A aliases in `L1_Sup` across `L1UW`.

## Not in this word

No accessed bit, no dirty bit, no process id, no user bit, no hardware `A`/`D` writeback. `NC` and `NX` are in the flag field. The flat entry in [MemoryProtection.md](MemoryProtection.md) carries its own execute, cache, and process-id flags. This layout does not match that one.

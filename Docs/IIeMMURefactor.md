# IIe MMU refactor

Phase 1 moves the Apple IIe auxiliary-memory and language-card map into `MMU_IIe`, behind a constructor flag so a IIgs Mega II keeps today's page tables. A characterization test records the current map first and is the gate for the move.

Unifying the IIgs composers with `calc_aux_read` / `calc_aux_write` stays out of scope. `shadow_is_enabled` is a separate change, specified at the end.

## Current split

One motherboard function is two objects.

`MMU_IIe` owns the page table, RAM and ROM bases, `cpu_aux` / RamWorks bank, and the slot-ROM switches. Its constructor registers writes for `$C006`, `$C007`, `$C00A`, and `$C00B` (`INTCXROM`, `SLOTC3ROM`).

`iiememory` (`src/devices/iiememory/iiememory.cpp`) owns `80STORE`, `RAMRD`, `RAMWRT`, `ALTZP`, the language-card flip-flops, copies of `PAGE2` and `HIRES`, and the code that publishes `$00`–`$BF` and `$D0`–`$FF`. It holds an `MMU_II *` and downcasts to reach `cpu_aux_base()`, `set_aux_bank()`, `f_intcxrom`, and `f_slotc3rom`.

`MMU_IIgs` does not use `iiememory`. It keeps its own copies of those switches (`g_80store`, `g_ramrd`, the state register at `$C068`, `LanguageCardLogic`) and writes the Mega II page table from `megaii_compose_map` and `bsr_map_memory`. `$C068` updates several switches at once and never goes through `$C000`–`$C00F`.

Display is a second consumer of `80STORE`, `PAGE2`, `HIRES`, `TEXT`, and `MIXED`. It registers its own `$C0xx` handlers. The scanner applies video switches on a delayed cycle and reads video RAM by address. That split stays.

The II+ language card stays a device. It owns a card RAM buffer. IIe language-card RAM is main and auxiliary memory, selected by `ALTZP`.

## Phase 1

### In

- Record a golden map of today's IIe page table, driven by `$C0xx` bus accesses.
- Move `iiememory`'s switches, `LanguageCardLogic`, `compose_map`, and `bsr_map_memory` onto `MMU_IIe`.
- Replace the inline `$C00x` handler body with a static callback that calls setters.
- Give `compose_map` a range helper so a region is one call, not a loop that repeats `ram_page(...)`.
- Constructor flag so the Mega II does not register those handlers and does not compose on init or reset.
- Delete `DEVICE_ID_IIE_MEMORY` / `MODULE_IIEMEMORY` once RamWorks calls `MMU_IIe::set_aux_bank` directly.
- Move the `iiememory` debug dump onto the MMU. Same lines.

### Out

- No change to `MMU_IIgs::megaii_compose_map`, `MMU_IIgs::bsr_map_memory`, `calc_aux_read`, or `calc_aux_write`.
- No change to which `$C0xx` handlers the IIgs installs, or to the order they run relative to display.
- No change to display, the video scanner, or the II+ language card.
- `TEXT`, `MIXED`, `80COL`, and alt charset do not move onto the MMU. They do not affect the CPU map. `PAGE2` and `HIRES` do, when `80STORE` is on, so the MMU observes those writes the way `iiememory` does today.
- Do not split the keyboard low bits out of `$C011`–`$C018`. Those reads keep returning `(last key & 0x7F) | flag`. The golden test does not cover the data bus.

## Constructor flag

```cpp
MMU_IIe(int page_table_size, int ram_amount, uint8_t *rom_pointer,
        bool install_memory_switches = true);
```

The flag lives on `MMU_IIe2`. Phase 2 constructs that class for the Mega II. The rules below are the same.

`install_memory_switches == true` is the IIe. The MMU registers the handlers that `init_iiememory` registers today, composes the initial map, and on `reset` clears the aux switches, resets the language card, sets the aux bank to 0, and recomposes with force.

`install_memory_switches == false` is the Mega II inside `MMU_IIgs`.

- Do not register the handlers that move over from `iiememory`.
- Do not call `compose_map` or `bsr_map_memory` from the constructor or from `reset`.
- `reset` stays what it is today: clear `INTCXROM` and `SLOTC3ROM`, then `compose_c1cf()`.

The methods may exist on the class. The IIgs code does not call them. Page-table contents on the IIgs path come only from the existing IIgs composers.

Always register `$C006`, `$C007`, `$C00A`, and `$C00B`, flag or not. On a IIgs those already occupy slot 0, and `init_c0xx_handlers` stacks `megaii_c0xx_write` in slot 1. Leaving that pair alone keeps slot-ROM behavior.

Pass `false` at the two Mega II construction sites:

- `src/gs2.cpp`, `MMU_MMU_IIGS`
- `apps/iigsmmutest/main.cpp`

Every other `new MMU_IIe2` keeps the default (`true`).

The flag has to cover reset, not only registration. `computer_t::reset` calls `mmu->reset` on the Mega II before the IIgs reset handler runs `megaii_compose_map`. That compose republishes a region only when the IIgs `m_*` flags change. An `MMU_IIe::reset` that rewrote the page table from its own flags would leave the IIgs flags saying "already published."

Each `$C0xx` address has two handler slots. On a IIgs both are already full for `$C000`/`$C001` and `$C054`–`$C057` (IIgs, then display). A third install asserts. `$C080`–`$C08F` is a single IIgs handler today. A second language-card handler would run as well, and the pre-write flip-flop would advance twice per access.

Do not register `$C00C`–`$C00F`. Those are display (80-column, alt charset).

`$C011` and `$C012` are registered twice today (`iiememory_read_C01X`, then `bsr_read_C011` / `bsr_read_C012`). Reads OR the two results. Phase 1 uses one handler.

## Setter API

`set_C0XX_write_handler` stores a C function pointer, `(void *context, uint32_t address, uint8_t value)`. A static member with that signature is a valid callback and can reach private data. A non-static member is not, because of the hidden `this`.

The handler decodes the address. The setter owns the flag and the map update that depends only on that flag.

```cpp
void MMU_IIe::c00x_write(void *context, uint32_t address, uint8_t value) {
    auto *self = static_cast<MMU_IIe *>(context);
    switch (address) {
        case 0xC006: self->set_intcxrom(false); break;
        case 0xC007: self->set_intcxrom(true); break;
        case 0xC00A: self->set_slotc3rom(false); break;
        case 0xC00B: self->set_slotc3rom(true); break;
        /* 80STORE, RAMRD, RAMWRT, ALTZP: setters, then one compose_map() */
    }
}
```

`set_intcxrom` / `set_slotc3rom` store the flag and call `compose_c1cf()`. Same shape as `MMU_IIgs::set_intcxrom`. Adopt this for the four handlers that already exist, before moving anything else. Behavior stays the same.

Aux switches interact. `set_80store`, `set_ramrd`, `set_ramwrt`, `set_altzp`, `set_page2`, and `set_hires` store the flag only. The handler calls `compose_map()` once after the switch. `compose_map` reads every flag and republishes the regions whose derived read or write bank changed. The page loops do not live in the setters.

`$C08x` is its own static handler: `LanguageCardLogic::read` or `write`, then `bsr_map_memory()`. `ALTZP` changes the language-card window's bank, so `compose_map` calls `bsr_map_memory` when the zero-page bank changes, as `iiememory_compose_map` does today.

`set_aux_bank` updates `cpu_aux` and recomposes with force. The aux base can move while every flag stays the same. RamWorks `$C073` calls `set_aux_bank` and does not look up `MODULE_IIEMEMORY`.

Public setters, private static handlers. A later IIgs phase can call `megaii->set_intcxrom(...)` without writing `$C006`.

## Map the composer publishes

Offsets are from the selected base. Language-card bank 1 is the `$C000` hole in that 64K (`base + 0xC000`). Bank 2 is `base + 0xD000`. `$E000`–`$FFFF` RAM is `base + 0xE000`. The IIe ROM image starts at `$C000`, so ROM `$D000` is `get_rom_base() + 0x1000` and ROM `$E000` is `get_rom_base() + 0x2000`.

| Pages | Read | Write |
|---|---|---|
| `$00`–`$01` | `ALTZP` | `ALTZP` |
| `$02`–`$03`, `$08`–`$1F`, `$40`–`$BF` | `RAMRD` | `RAMWRT` |
| `$04`–`$07` | `80STORE` ? `PAGE2` : `RAMRD` | `80STORE` ? `PAGE2` : `RAMWRT` |
| `$20`–`$3F` | `80STORE && HIRES` ? `PAGE2` : `RAMRD` | `80STORE && HIRES` ? `PAGE2` : `RAMWRT` |
| `$C1`–`$CF` | `INTCXROM`, `SLOTC3ROM`, slot register. Not aux. | none |
| `$D0`–`$DF` | LC read-enable ? bank RAM : ROM | LC write-enable ? bank RAM : `NONE` |
| `$E0`–`$FF` | LC read-enable ? RAM : ROM | LC write-enable ? RAM : `NONE` |

`$C0` is the handler table, not a page pointer. `$C8`–`$CF` also changes when `$C3xx` is accessed while `SLOTC3ROM` is clear (`iie_map_rom_slot3`). That is an access side effect. `$C1`–`$CF` does not share switches with the rows above it, so the characterization golden splits there. The `$C8` cases live in the `$Cxxx` suite.

Write-enable is `_FF_WRITE_ENABLE == 0` (active low). `PRE_WRITE` does not change the map.

## Characterization test

Write this against the current code and commit the golden before moving handlers. The refactor is done when the same bus sequences produce the same golden.

### Fixture

Construct an `MMU_IIe` (flag on, once the flag exists) and call `init_iiememory`. No display, no keyboard device, no SDL. After phase 1, drop `init_iiememory`. The test still only calls `read` and `write`.

Drive switches by bus access. Do not call `compose_map` and do not poke flags. `$C054`–`$C057` and `$C08x` are reads. `$C000`–`$C005`, `$C008`, `$C009`, `$C006`, `$C007`, `$C00A`, `$C00B` are writes.

### What one state records

Both suites use the same encoding. Record read and write separately:

| Pointer | Record |
|---|---|
| inside the 128K block (`main_ram` .. `main_ram + 0x20000`) | `RAM+offset` |
| inside `get_rom_base()` | `ROM+offset` |
| null | `NONE` |

`offset` is `pointer - main_ram`, 17 bits (`0x00000`–`0x1FFFF`). Bit 16 set is auxiliary bank 0, so text page 1 in aux is `RAM+0x10400` and language-card bank 1 under `ALTZP` is `RAM+0x1C000`. A pointer outside that window is not a `RAM+` span.

Run-length encode. One state is a list of spans, for example `04-07 R RAM+0x10400 W RAM+0x10400`. Store one line per state: the switch bits, then the spans.

Leave out function pointers, `read_d` / `write_d`, and page `$C0`. A handler address is not a map, and the debug tags will get renamed.

The aux/language-card suite records `$00`–`$BF` and `$D0`–`$FF`. The `$Cxxx` suite records `$C1`–`$CF`.

On mismatch, print the switch values and the first page whose read or write descriptor differs.

### Aux and language card

512 states. These axes, each on or off, except the language card which is three bits:

- `80STORE`, `RAMRD`, `RAMWRT`, `ALTZP`, `PAGE2`, `HIRES` (64)
- language-card bank, read-enable, write-enable (×8)

Reach the eight language-card map states through `$C080`–`$C08F`, using `LanguageCardLogic`'s rules:

- Address bit 3 set (`$C088`–`$C08F`) selects bank 1. Clear (`$C080`–`$C087`) selects bank 2.
- Address bits 1:0 equal 00 or 11 enable RAM read. 01 or 10 select ROM.
- Write-enable takes two successive reads of an odd address (`$C081`, `$C083`, `$C089`, `$C08B`, and the `+$4` aliases). One read leaves write protect on. `PRE_WRITE` itself is not an axis. The golden includes both the one-access and the two-access result.

`INTCXROM` and `SLOTC3ROM` are not axes. One check walks all four combinations and requires the same `$00`–`$BF` / `$D0`–`$FF` snapshot.

`TEXT`, `MIXED`, `80COL`, and alt charset are not axes. One extra check toggles them and requires the same snapshot.

### Suites kept out of the 512

- RamWorks bank 0 and bank 1, for a few flag combinations (at least `ALTZP`, `RAMRD`, and `80STORE`+`PAGE2`+`HIRES`). Bank 0 auxiliary pages record as `RAM+0x1xxxx`. Bank 1 is `aux_extra` and falls outside the 17-bit window. A separate assertion checks that `cpu_aux` moved to the selected bank.
- One reset snapshot: main RAM everywhere the aux switches control, language card reading ROM and writing RAM bank 2. `$C1`–`$CF` is not on this line.

### $Cxxx

`$C1`–`$CF` has its own lines in the same golden. `80STORE`, `RAMRD`, `RAMWRT`, `ALTZP`, `PAGE2`, `HIRES`, and the language card do not change it. One check sets those and requires the same `$C1`–`$CF` snapshot.

`INTCXROM` on publishes motherboard ROM for the whole window. The slot register, `SLOTC3ROM`, a `$C3xx` access, and `$CFFF` do not change that snapshot. One line, plus those checks.

`INTCXROM` off, with `$C8`–`$CF` still the default empty slot table: the six slot-register bits `compose_c1cf` honors, times `SLOTC3ROM` (128). Slot 3 follows `SLOTC3ROM`, not the slot-register bit. One check sets that bit and requires the same snapshot.

`$C8`–`$CF` is an access side effect, not crossed with the 64 slot-register values. With no card ROMs installed, and the slot register held at its power-on value:

- Default, and after `$CFFF`: `NONE`.
- `$C3xx` with `SLOTC3ROM` clear: motherboard ROM at `ROM+0x0800`.
- `$C3xx` with `SLOTC3ROM` set: stays `NONE`.
- An access in `$C1`, `$C2`, or `$C4`–`$C7` has no `$C8` handler, so the window stays `NONE`.
- `$C3xx` while `INTCXROM` is on still shows internal ROM. Clearing `INTCXROM` reveals the `$C8` ROM that access stored.

One `$Cxxx` reset snapshot: `INTCXROM` off, `SLOTC3ROM` off, slot register at its power-on value, `$C8`–`$CF` cleared.

### Setter companion

After the setters exist, a short second test calls them for a handful of states and requires the same snapshot as the matching `$C0xx` sequence. The golden guards the refactor. This one guards the new surface.

## Done when

- The golden file was generated from `iiememory` before the move, and the same test passes with the logic on `MMU_IIe`.
- A IIgs build still composes Mega II pages `$00`–`$BF` and `$D0`–`$FF` only through `megaii_compose_map` and `MMU_IIgs::bsr_map_memory`. The Mega II instance is `MMU_IIe2` with `install_memory_switches` false (Phase 2).
- RamWorks bank select republishes via `set_aux_bank`.
- `$C011` / `$C012` have one read handler. `$C01x` still returns the keyboard low bits ORed with the flag.
- Display still owns `TEXT`, `MIXED`, `80COL`, and alt charset, and still shares `80STORE`, `PAGE2`, and `HIRES` through the second `$C0xx` slot.

## Phase 2 — Mega II uses MMU_IIe2

Replace the embedded Mega II with `MMU_IIe2`. `MMU_IIgs` still publishes `$00`–`$BF` and `$D0`–`$FF` only through `megaii_compose_map` and `bsr_map_memory`. Unifying those composers with `MMU_IIe2::compose_map` / `calc_aux_read` / `calc_aux_write` stays out of scope.

### Construction

`install_memory_switches == false` at the two Mega II sites:

- `src/gs2.cpp`, `MMU_MMU_IIGS`
- `apps/iigsmmutest/main.cpp`

Standalone IIe stays `MMU_IIe2` with the default (`true`) and a message bus. Do not attach a message bus to the Mega II. IIgs keyboard and status reads go through IIgs handlers, not `MMU_IIe2::c01x_read`.

`MMU_IIgs::megaii` is `MMU_IIe2 *`.

With the flag false, the Mega II still installs slot-ROM writes (`$C006`, `$C007`, `$C00A`, `$C00B`) and the slot-3 `$C8` side effect. It does not install aux, language-card, or display handlers, and `reset` does not republish aux or language-card pages. IIgs `reset` still runs `megaii_compose_map` and `bsr_map_memory` after that.

### INTCXROM and SLOTC3ROM

`MMU_IIe2::set_intcxrom` and `set_slotc3rom` store the flag and call `compose_c1cf()`. On a IIgs, `MMU_IIgs::set_intcxrom` mirrors `g_intcxrom` and then calls `megaii->set_intcxrom`. Do not call `compose_c1cf()` again in that helper. `$C00A` / `$C00B` on the IIgs path call `megaii->set_slotc3rom` the same way.

A bus write can still compose twice: Mega II slot 0 (`c00x_write`) and IIgs slot 1 (`write_c0xx`) both run. Deduplicating that stack is out of scope.

### Still out

- Delegating `megaii_compose_map` to `MMU_IIe2::compose_map`.
- Removing the `iiememory` device or `MMU_IIe` (still used by `iiemmutest`, `iiememory`, and the video test apps).

### Done when

- `iigsmmutest -l` and `iigsmmutest -l -3` pass.
- A IIgs ROM built-in diagnostic still runs.
- Mega II has no language-card handlers on `$C080`–`$C08F` (`install_memory_switches` is false). IIgs owns those addresses.

## Phase 3 — page table struct-of-arrays

The main page table is an array of `page_table_entry_t`. Each record is 80 bytes: `read_p`, `write_p`, three handler pairs, and two debug strings. A CPU read uses `read_p` only. Consecutive `read_p` values are 80 bytes apart, so each one occupies its own 64-byte cache line, and the index scale does not fit an addressing mode (those stop at 8).

Store each field in its own array, length `num_pages`:

```cpp
uint8_t *read_p;
uint8_t *write_p;
read_handler_t *read_h;
write_handler_t *write_h;
write_handler_t *shadow_h;
const char **read_d;
const char **write_d;
```

A read indexes `read_p[page]` (scale 8) and, on a null pointer, `read_h[page]`. A write indexes `write_h[page]`, then `write_p[page]`, then `shadow_h[page]`. Same order as today. `page` and the byte offset still come from `page_size_bits` and `page_size_mask`. `MMU_II` is 256 pages of 256 bytes. `MMU_IIgs` is one entry per 64K bank. Both go through this table.

`page_table_entry_t` stays the value type. `get_page_table_entry` gathers one page into a struct. `set_page_table_entry` scatters a struct into the arrays. `map_page_*`, `set_page_read_h`, `set_page_write_h`, and `set_page_shadow` keep their signatures and write one field.

### Who indexes the arrays

The live methods are inline in `src/mmus/mmu.hpp`. `src/mmus/mmu.cpp` is inside `#if 0`. Leave it.

These read and write the main table by taking `&page_table[page]` today:

- `MMU::read`, `write`, `read_raw`, `write_raw`, the mappers, the dump, and `get_page_base_address` in `mmu.hpp`
- `MMU_II::read` and `write` in `mmu_ii.cpp`
- `MMU_IIe::read` and `write` in `mmu_iie.cpp`

`MMU_IIe2` does not override `read` or `write`. `MMU_IIgs::read` and `write` call `MMU::read` and `MMU::write`.

`compose_c1cf` publishes a side-table record with `page_table[page] = entry` in `mmu_ii.cpp`, `mmu_iie.cpp`, and `mmu_iie2.cpp`. Those three call `set_page_table_entry` instead.

### Side tables stay arrays of structs

`MMU_II::slot_rom_ptable[15]`, `MMU_IIe::int_rom_ptable[15]`, and `MMU_IIe2::int_rom_ptable[15]` stay `page_table_entry_t` arrays. They are filled on slot-ROM changes and copied into the main table by `compose_c1cf`. They are not on the per-cycle path.

`apps/iiemmutest/iie_mmu_oracle.hpp` keeps calling `get_page_table_entry` and reading `read_p` / `write_p` on the returned struct.

Devices, `MMU_IIgs` composers, and the CPU test apps keep calling `map_page_read`, `map_page_write`, `map_page_both`, and the `set_page_*` handlers. They do not see the storage.

### Out

- A compiled per-mode reader. Region bounds are the same in every mode; mode only changes pointers.
- Devirtualizing `MMU::read` / `write`.
- A direct `zp_base` / language-card base in front of the table.
- `alignas(64)` or a statically allocated table.
- A new page size.
- Turning the 15-entry side tables into parallel arrays.

### Baseline

`iiemmu2test --bench`, Release, array-of-structs. `MMU *` to `MMU_IIe2`, 10,000,000 reads of `i % 0xC000`, one warmup, five timed passes.

median 1.324 ns/read, 755.102 MHz. Passes: 1.319, 1.324, 1.324, 1.329, 1.331. Checksum 1275000000.

### After

Same bench, struct-of-arrays.

median 1.385 ns/read, 721.872 MHz. Passes: 1.382, 1.383, 1.385, 1.413, 1.432. Checksum 1275000000.

The median is 0.061 ns/read slower (about 4.6%). Checksum matches the baseline. Both tables fit in L1; this loop did not show a gain from the scale-8 index.

### Done when

- The IIe golden map is unchanged. Same bus sequences, same `RAM+` / `ROM+` / `NONE` spans.
- `iigsmmutest -l` and `iigsmmutest -l -3` pass. IIgs reads still go through `MMU::read`.
- `get_page_table_entry` returns the same pointers, handlers, and debug tags as the old record.

## GS MMU — shadow_is_enabled

`bank_shadow_write` is the only caller. It runs after `calc_aux_write`, so bit 16 of the address is the post-aux bank. The predicate does not see the bank number above that. `SPEED_SHADOW_ALL` only decides which banks install `bank_shadow_write`. It does not change the predicate.

The range tests rechecked six windows against `reg_shadow` on every call. Every range is a whole number of 1K blocks, and 1K is the coarsest step that stays exact: `$0000–$03FF` is not shadowed and `$0400–$07FF` is text page 1. `shadow_is_enabled` is now the mask lookup below.

### One mask table

128 bytes, one per 1K slot in the low 17 address bits. Filled in the `MMU_IIgs` constructor, after `is_rom03` is known. Nothing writes it after that. `$C035` and the `reg_shadow = 0x08` in `reset` change the register only.

```text
slot    = (address >> 10) & 0x7F
enabled = (shadow_mask[slot] & ~reg_shadow) != 0
```

The byte is the OR of the inhibit bits of every region that contains that slot. `reg_shadow` stays live. A bit set in the register inhibits that region. The AND with the inverted register is nonzero when at least one covering region is still enabled. A slot no region covers holds `0` and stays disabled for every register value.

Bits 0–5 are the bits this function reads. Bit 6 (`IOLC`) and bit 7 (reserved) are not in the table. `$C000–$FFFF` stays 0 here; `bank_shadow_write` handles that window before this call.

`is_rom03` is the only construction input. On a ROM03, slots 2 and 66 hold `TEXT2` (`0x20`). On a ROM01 those slots hold `0`.

| Slots | Addresses | Mask |
|---|---|---|
| 1 and 65 | `$x400–$x7FF` | `TEXT1` `0x01` |
| 2 and 66 | `$x800–$xBFF` | `TEXT2` `0x20` on ROM03, else `0` |
| 8–15 | even `$2000–$3FFF` | `HGR1` `0x02` |
| 16–23 | even `$4000–$5FFF` | `HGR2` `0x04` |
| 72–87 | odd `$2000–$5FFF` | `SHR\|AUXHGR` `0x18` |
| 88–103 | odd `$6000–$9FFF` | `SHR` `0x08` |

Odd `$2000–$5FFF` is the overlap. Shadow is on when either inhibit bit is clear, which is the OR already stored in `0x18`. Odd `$6000–$9FFF` is SHR only. Even `$6000–$FFFF` is 0.

Power-on `reg_shadow = 0x08` inhibits SHR and leaves the other display regions enabled. Odd `$2000` is on (`0x18 & ~0x08 = 0x10`). Odd `$6000` is off.

### Out

- No second table, and no pointer swap when `reg_shadow` changes.
- `IOLC` stays in `bank_shadow_read` / `bank_shadow_write`. A nonzero mask is a Mega II write-through, which is the wrong action for `$C000–$CFFF`.
- `calc_aux_read`, `calc_aux_write`, and the `$C0–$FF` dispatch in the shadow handlers stay as they are.
- No sub-64K page table.

### Compare

`iigsmmutest --shadow`. It does not load a ROM file and does not run the assembly suites.

Construct two `MMU_IIgs` objects with a zeroed ROM buffer and a Mega II built with `install_memory_switches` false. 128K selects ROM01. 256K selects ROM03. Same shape as the `-l` path in `apps/iigsmmutest/main.cpp`.

Copy today's range tests into the harness as the oracle. The lookup calls production `shadow_is_enabled`, which indexes `shadow_mask` on the object. The harness does not keep a second table.

Sweep, and fail on the first mismatch. Print machine, `reg_shadow`, address, oracle, and lookup.

- `reg_shadow` from `0x00` through `0xFF`, via `set_shadow_register`. Bits 6 and 7 must not change the result.
- 128 addresses, `(slot << 10)` for slot `0..127`.
- The same 128 addresses with bit 17 set (`| 0x20000`). Bank `$02` matches bank `$00` and bank `$03` matches bank `$01`.

That is 2 × 256 × 128 × 2 comparisons. 1K boundaries are the grid. The speed-test checksum below covers the bytes inside each block for the register values it times.

Snapshot the 128 mask bytes after construction. After the sweep, and after `reset`, the bytes are unchanged and `reg_shadow` is `0x08`.

### Speed

The same `--shadow` run times the oracle and the lookup after the compare passes. A compare failure skips the timing and returns non-zero. The timing does not fail the process.

Release build. 10,000,000 calls, address `i & 0x1FFFF`, one warmup, five timed passes, median of the five. Same arrangement as `iiemmu2test --bench`.

The timed loop calls a `noinline` wrapper compiled in `apps/iigsmmutest/shadow_predicate_fn.cpp`, not in the loop's file. `noinline` in the same translation unit is not enough. Clang still sees the body, rewrites the call to pass the register in a register, and deletes every timed pass but the last. A separate file hides the body. There is no link-time optimization on this target.

```cpp
[[gnu::noinline]] bool shadow_pred::oracle(MMU_IIgs *mmu, uint32_t address);
[[gnu::noinline]] bool shadow_pred::lookup(MMU_IIgs *mmu, uint32_t address);
```

The oracle is today's range tests. It reads `shadow_register()` and `is_rom03_machine()` on each call. The lookup calls `shadow_is_enabled`, so the inline mask load is compiled into the wrapper. Neither is `pure` or `const`. `reg_shadow` is not a constant argument. The timed loop also puts a memory clobber around each pass so a visible caller cannot drop the call.

Time the two pass loops separately. Inside the loop accumulate `sum += (uint64_t)result * (address | 1)`. Compare checksums after the passes, not inside them. The checksums must be equal.

Three register values, both machines:

| `reg_shadow` | What the old tests do |
|---|---|
| `0x08` | Power-on. SHR inhibited, the other display regions enabled. |
| `0x00` | Every display region enabled. Hits return at the first matching range. |
| `0x3F` | All six display inhibits set. Every range fails. |

### Release

`./build/iigsmmutest --shadow`, existing Release tree. Median of five passes.

```text
ROM01 reg 08 oracle ns/call: 1.916 1.795 1.633 1.617 1.647  median 1.647
ROM01 reg 08 lookup ns/call: 0.695 0.691 0.697 0.696 0.694  median 0.695
ROM01 reg 08 checksum 128015925248
ROM01 reg 00 oracle ns/call: 1.594 1.599 1.598 1.606 1.587  median 1.598
ROM01 reg 00 lookup ns/call: 0.697 0.699 0.689 0.692 0.707  median 0.697
ROM01 reg 00 checksum 250422493184
ROM01 reg 3F oracle ns/call: 1.816 1.735 1.791 1.737 1.747  median 1.747
ROM01 reg 3F lookup ns/call: 0.701 0.696 0.686 0.705 0.695  median 0.696
ROM01 reg 3F checksum 0
ROM03 reg 08 oracle ns/call: 1.618 1.629 1.630 1.634 1.657  median 1.630
ROM03 reg 08 lookup ns/call: 0.715 0.694 0.697 0.695 0.699  median 0.697
ROM03 reg 08 checksum 133517279232
ROM03 reg 00 oracle ns/call: 1.592 1.586 1.574 1.594 1.591  median 1.591
ROM03 reg 00 lookup ns/call: 0.694 0.698 0.710 0.694 0.704  median 0.698
ROM03 reg 00 checksum 255923847168
ROM03 reg 3F oracle ns/call: 1.777 1.789 1.783 1.772 1.787  median 1.783
ROM03 reg 3F lookup ns/call: 0.712 0.701 0.703 0.689 0.700  median 0.701
ROM03 reg 3F checksum 0
```

Lookup median is 0.69–0.70 ns/call. Oracle median is 1.59–1.78 ns/call. Checksums match the pre-swap run. The fully-inhibited case (`$3F`, checksum 0) is the one where every range test fails, and the lookup is still one load and a test. The lookup column is production `shadow_is_enabled`.

### Done when

- The compare passes for ROM01 and ROM03.
- The mask bytes survive `set_shadow_register` and `reset`.
- Each bench pair prints one checksum, shared by the oracle and the lookup.
- `iigsmmutest -l` and `iigsmmutest -l -3` still pass. The write-through behavior is unchanged.

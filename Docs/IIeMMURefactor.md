# IIe MMU refactor

Phase 1 moves the Apple IIe auxiliary-memory and language-card map into `MMU_IIe`, behind a constructor flag so a IIgs Mega II keeps today's page tables. A characterization test records the current map first and is the gate for the move.

Later phases (IIgs calling the same composer, one predicate shared with `calc_aux_read` / `calc_aux_write`) are out of scope here. The IIgs composers stay as they are.

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

Every other `new MMU_IIe` keeps the default.

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
- A IIgs build still composes Mega II pages only through `megaii_compose_map` and `MMU_IIgs::bsr_map_memory`. `install_memory_switches` is false for that `MMU_IIe`.
- RamWorks bank select republishes via `set_aux_bank`.
- `$C011` / `$C012` have one read handler. `$C01x` still returns the keyboard low bits ORed with the flag.
- Display still owns `TEXT`, `MIXED`, `80COL`, and alt charset, and still shares `80STORE`, `PAGE2`, and `HIRES` through the second `$C0xx` slot.

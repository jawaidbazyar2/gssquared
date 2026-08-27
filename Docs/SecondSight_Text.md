# Second Sight Host Text Mode

**Status:** draft v0.1 (2026-08-27)
**Card mode:** SetMode emulation flag `$04`
**Depends on:** [SecondSight.md](SecondSight.md) (classic VGA API, handshake, slot I/O)

This is a *new* scanout mode: the card renders an 80-column VGA-style text
buffer that lives in **shadowed Apple II memory**. The host writes cells with
ordinary stores. The card never requires a DMA of the screen on the hot path.

An original 1995 Second Sight silicon MAY be capable to implement this. See "Back-Porting" notes at end of this document.

Same fence as GPU mode ([SecondSight_GPU.md](SecondSight_GPU.md)): this is the API
the card should have grown, not a modeline.

GNO/ME VGA console, ANSI/curses, and IIx text are the intended clients. Classic
mode 03h (upload into card VRAM, CRTC start address) stays frozen for Cogito /
Spectrum.

---

## 1. Why a distinct mode

Classic VGA text (`SetMode` flag `$01`, mode `$03`) is a display controller:
cells live in card VRAM; the CPU pushes them through the handshake. Fine for
occasional uploads; fatal as a 1 MHz terminal.

Apple II overlay (`ss_a2_text_sync`) is free, but it is the Apple text page:
no VGA attributes, no 80×25 linear buffer, no hardware row scroll.

Host Text (`SetMode` flag `$04`) is a **scanout of host RAM**, in the same
family as PPU mode (`$02`):

- software picks a **raster** by mode number (cell size × visible rows);
- software places a **control block** and a **cell buffer** in snoopable RAM;
- `SetTextCtrl` arms the control-block pointer;
- the card latches the block at VBL and renders from those pointers.

```
  1995 VGA text                         Host Text
  ─────────────                         ─────────
  SetMode($03, VGA)                     SetMode($03, HOSTTEXT)
  UploadData → VRAM                     (blank until armed)
  CRTC start address                    SetTextCtrl(ctrl)
  CPU DMA every update                  STA into shadowed cells
```

PPU and GPU stay separate. Host Text does not draw pixels into a framebuffer
the host owns, and it does not execute a command-sequence buffer.

---

## 2. Arming: blank until SetTextCtrl

`SetMode` with flag `$04` switches the command table and the raster. It does
**not** start scanning Apple RAM.

Until a control-block address has been armed with `SetTextCtrl` (`$50`):

- output is **blank** (same pixels as `ScreenOff`);
- `ScreenOn` does not override this.

No default pointer into HGR. Arming a random address would scan leftover
hires as text.

Typical bring-up:

```
; fill cells at $2000 and control block at $2FE0 (card is not looking)
SetMode($03, HOSTTEXT)          ; raster on, still blank
SetTextCtrl($2FE0)              ; 16-bit, main; next VBL shows the buffer
```

`SetTextCtrl` is only legal once `ss_mode == HOSTTEXT`. Issued from any other
mode: handshake `$A6`. Relocating the control block later is another
`SetTextCtrl`.

Leaving Host Text (`SetMode` with flag `$00` / `$01` / `$02` / `$03`)
**unarms**. Re-entry is blank until `SetTextCtrl` again.

`ScreenOff` blanks even when armed. `ScreenOn` restores scanout only if still
armed.

Driver still must **clear the cell buffer before arming**. The card cannot tell
a finished screen from leftover SHR.

---

## 3. The VGA fence

While `ss_mode == HOSTTEXT`:

**Allowed classic commands**

| Cmd   | Name             | Role in Host Text                                      |
| ----- | ---------------- | ------------------------------------------------------ |
| `$00` | GetStatus        | Detection (unchanged 12-byte record)                   |
| `$01` | SetMode          | Enter/leave; pick raster by mode number                |
| `$04` | ScreenOff        | Blank output                                           |
| `$05` | ScreenOn         | Enable output (still blank if unarmed)                 |
| `$06` | SetPalette       | 768-byte RGB; only indices 0–15 are used for cells     |
| `$07` | SetPaletteEntry  | One of those 16 colors                                 |
| `$08` | SetBorder        | Overscan / blank-screen color                          |
| `$0F` | SetTextFont      | Same `$00`–`$03` as VGA text (Apple / Apple alt / ANSI / user) |

**Host Text command** (extension space; not part of the 1995 set). Starts at
`$50` so it does not collide with GPU host commands `$40`–`$43`.

| Cmd   | Name         | Role                          |
| ----- | ------------ | ----------------------------- |
| `$50` | SetTextCtrl  | §6 — arm the control block    |

**Fenced** (handshake `$A6` if issued in Host Text)

| Cmd           | Name                         | Why                                          |
| ------------- | ---------------------------- | -------------------------------------------- |
| `$02`         | UploadData                   | No linear VRAM contract                      |
| `$03`         | ScrollScreen                 | Use `start_line` in the control block        |
| `$0A`         | ClearScreen                  | VRAM fill; host clears its own buffer        |
| `$0C` / `$0D` | SetVGAReg / GetVGAReg        | No modeline                                  |
| `$0E`         | SetUserMode                  | No 84-byte VGA block                         |
| `$09`         | RunCode                      | Z180 path is not this mode                   |
| `$40`–`$43` GPU | UploadTexture / …          | Wrong mode                                   |

Oak / CRTC registers are ignored. Visible geometry comes only from the SetMode
mode number plus `vis_rows` / `cols` in the control block (those must fit the
raster; §4).

---

## 4. Display modes (rasters)

The mode **number** selects cell geometry and the pixel size of the scanout.
`vis_rows` in the control block must match (or be ≤) that raster.

| Mode  | Visible | Cell   | Pixels   | Notes                          |
| ----- | ------- | ------ | -------- | ------------------------------ |
| `$01` | 40×25   | 9×16   | 360×400  |                                |
| `$03` | 80×25   | 9×16   | 720×400  | v0.1 required                  |
| `$43` | 80×43   | 8×8    | 640×344  | planned; 8×8 font              |
| `$50` | 80×50   | 8×8    | 640×400  | planned                        |

`$03` is the GNO default and matches classic SS / IBM mode 03h *geometry*, not
its VRAM contract.

Graphics numbers (`$13`, `$53`, …) and Apple-emulation IDs `$FA`–`$FE` are
**invalid** in Host Text (`$A6`).

Unknown number or a `vis_rows` that does not fit the raster: `SetMode`
returns `$A6`, or the VBL latch stays blank (§7).

Coordinate space: column 0 is left, row 0 is the top *visible* row after
`start_line` / wrap is applied. Frozen rows (§8) are an exception.

---

## 5. What RAM the card can see

Addresses are **16-bit** plus a **main/aux** bit. There is no bank number: a
slot card does not see `$00` vs `$E0`. It sees the Apple II video bus —
IIe main vs aux, which on a IIgs is Mega II `$E0` vs `$E1`.

| Machine | Legal cell / ctrl regions | Not visible |
| ------- | ------------------------- | ----------- |
| IIe     | RAM the CPU writes, **main or aux** (card must track `RAMWRT` like A2DVI) | — |
| IIgs    | Mega II video shadow: HGR1/2, SHR `$2000–$9FFF`, main (`$E0`) or aux (`$E1`) | Fast banks `$00–$7F` unless those writes are actually shadowed |
| Emulator / IIx / memexp+video | Same 16-bit + aux window, unless a later capability bit extends it | — |

**Do not** put the buffer or the control block in text page 1/2. Screen holes.

Software always stores `$2000`, `$2FE0`, etc. Aux is a flag, not a bank byte.

---

## 6. SetTextCtrl (`$50`)

Command byte `$50`, then **3** DMA bytes. No bank.

| Offset | Name    | Meaning |
| ------ | ------- | ------- |
| `$00`  | addr_lo | Control block address bits 7–0 |
| `$01`  | addr_hi | Bits 15–8 |
| `$02`  | flags   | Bit 0 **aux**: 0 = main, 1 = IIe aux / IIgs Mega II `$E1`. Other bits reserved 0. |

The aux bit is part of this command’s argument block: it selects which side
the **control block** is read from. Cell / palette aux bits live in the
control block (§7.1), not here.

On success: handshake `$A5` after the usual long-command wait; the pointer is
armed; **scanout begins at the next VBL**. Fill the block (and cells) *before*
this command — there is no magic number to mark the block valid. Bad length /
reserved nonsense in the DMA packet: `$A6`, remains unarmed (or stays on the
previous armed pointer if this was a relocate that failed).

---

## 7. Control block (32 bytes)

Latched as a whole at the start of VBL. Mid-frame stores take effect next
frame. Arming (`SetTextCtrl`) is the validity gate: once armed, the card uses
whatever bytes sit at that address. No magic.

If `virt_rows < vis_rows`, `cols` is 0, or `frozen_top + frozen_bottom >=
vis_rows`: stay blank (armed pointer is kept; fix the block and wait a VBL).
Garbage cells still display as garbage — the driver must clear them before
arming.

| Off | Size | Name          | |
| --- | ---- | ------------- | --- |
| `$00` | 1  | flags         | §7.1 |
| `$01` | 1  | cols          | 40 or 80 |
| `$02` | 1  | vis_rows      | Visible rows (25 / 43 / 50) |
| `$03` | 1  | virt_rows     | Buffer height, ≥ `vis_rows` |
| `$04` | 1  | start_line    | First mapped buffer row, `0 .. virt_rows-1` |
| `$05` | 1  | cursor_x      | 0 .. cols-1 |
| `$06` | 1  | cursor_y      | 0 .. vis_rows-1 (visible space, not buffer row) |
| `$07` | 1  | frozen_top    | Rows at the top that ignore `start_line` (0 = none) |
| `$08` | 1  | frozen_bottom | Same, from the bottom |
| `$09` | 1  | reserved      | 0 |
| `$0A` | 2  | buffer_addr   | 16-bit — char plane, or interleaved buffer |
| `$0C` | 2  | attr_addr     | 16-bit — attr plane if planar; ignored if interleaved |
| `$0E` | 2  | pal_addr      | 16-bit — 16×RGB888 if `pal_from_block`; `$0000` = command palette |
| `$10` | 16 | reserved      | 0 |

Pointers are 16-bit only. Main vs aux is **`flags` bits 5–7**, not a bank
byte.

### 7.1 `flags`

| Bit | Name           | |
| --- | -------------- | --- |
| 0   | `planar`       | 0 = interleaved (char, attr, char, attr). 1 = linear chars, then linear attrs. |
| 1   | `wrap`         | 1 = circular `start_line` (required for console scroll). 0 = clamp. |
| 2   | `pal_from_block` | 1 = load 16×RGB from `pal_addr` each VBL. 0 = last `$06`/`$07` (or IBM default). |
| 3   | `blink`        | 1 = VGA blink (attr bit 7). 0 = **16 background colors** (bit 7 is bg intensity). Default 0. |
| 4   | `cursor`       | 1 = draw a hardware cursor at (`cursor_x`,`cursor_y`). |
| 5   | `buffer_aux`   | 0 = `buffer_addr` in main, 1 = aux (IIgs `$E1`). |
| 6   | `attr_aux`     | Same for `attr_addr`. |
| 7   | `pal_aux`      | Same for `pal_addr`. |

`cols` × `virt_rows` must fit the allocation behind `buffer_addr` (and
`attr_addr` if planar). The card does not clip oversize; the driver owns that.

---

## 8. Hardware scroll, freeze, page-flip

**`start_line`** is a **row index**, not a CRTC byte address.

With `wrap` set, visible row `y` (after frozen top) maps to buffer row

```
(start_line + y) % virt_rows
```

Console newline on a full screen: increment `start_line` modulo `virt_rows`,
clear the new last buffer row. No 4000-byte `memmove`.

With `wrap` clear, mapping clamps; useful only for a large linear scrollback
that the driver manages.

**Frozen rows:** `frozen_top` / `frozen_bottom` are taken from the start of
the char/attr buffer (row 0 .. N-1 and the last M rows), **not** rotated by
`start_line`. Status / clock line. Remaining `vis_rows - top - bottom` rows
scroll. If `top + bottom >= vis_rows`, the block is invalid (blank).

**Page-flip:** write `buffer_addr` (and `attr_addr` if planar), and/or the
matching aux bits. Latched at VBL. Two HGR pages, or two slices of SHR, or
main vs aux. Do not handshake.

Cursor is in *visible* coordinates so a GNO driver can place it without adding
`start_line`.

---

## 9. Cell layouts

The renderer already distinguishes these
(`vga_text_vram_layout_t` in `vga_render_text_9x16.hpp`). Host Text exposes
both. **Planar is the default for new software** — this mode is not IBM
`B800` compatible; mode 03h VGA already is.

### Interleaved (`planar = 0`)

`pitch = cols * 2`. Cell `(c, r)` is at `buffer + r*pitch + c*2`: byte 0
character, byte 1 IBM-style attribute (fg nibble, bg nibble).

65816-friendly (`LDA` char+attr as a word). Atomic cell update.

### Planar (`planar = 1`)

Char plane: `pitch = cols`. Cell `(c, r)` char at `buffer + r*cols + c`.
Attr plane: same index at `attr_addr`. If `attr_addr` is `$0000`, attrs sit
immediately after the char plane (`buffer + cols*virt_rows`) on the **same
side** as the chars (`buffer_aux`; `attr_aux` is ignored).

Use cases:

- ANSI/GNO hot path is “emit glyphs in the current SGR” — `memcpy` / `STA
  (zp),Y` a run of chars; attrs only on color change.
- 6502 row loops are 80 bytes, not stride 2.
- Curses `chgat`, selection, search highlight: rewrite attrs, leave letters.
- `clrtoeol` / erase keeping colors: spaces into the char plane only.
- Recolor a status line without rewriting text.
- Independent attr page-flip (same glyphs, other color map).
- IIe: chars in **main**, attrs in **aux**, same `$2000` offset — two 4K
  planes, `buffer_aux` clear, `attr_aux` set.

---

## 10. Palette — yes, support it

Sixteen VGA attribute colors *are* the look of this mode. A GNO console that
cannot leave IBM cyan/magenta, or cannot do a dark/amber theme, is unfinished.

**Default** (until `$06`/`$07` or a block palette is loaded): IBM CGA/VGA
text DAC, same as `vga_text_9x16_restore_ibm_palette()`:

| i | RGB | i | RGB |
| - | --- | - | --- |
| 0 | `00 00 00` | 8 | `55 55 55` |
| 1 | `00 00 AA` | 9 | `55 55 FF` |
| 2 | `00 AA 00` | 10 | `55 FF 55` |
| 3 | `00 AA AA` | 11 | `55 FF FF` |
| 4 | `AA 00 00` | 12 | `FF 55 55` |
| 5 | `AA 00 AA` | 13 | `FF 55 FF` |
| 6 | `AA 55 00` | 14 | `FF FF 55` |
| 7 | `AA AA AA` | 15 | `FF FF FF` |

Attribute byte: `fg = attr & 0x0F`, `bg = (attr >> 4) & 0x0F`. With `blink`
clear (recommended for terminals), all 16 backgrounds exist. With `blink`
set, bit 7 blinks and only eight backgrounds exist — PC VGA behavior.

Two ways to set colors; both are intentional.

1. **Commands `$06` / `$07`.** Same packets as classic SS. Only indices 0–15
   affect glyphs; 16–255 may be ignored. Used when the driver does not want a
   palette in HGR, or for a one-shot init. Takes effect next VBL (with the
   rest of the latch).
2. **Block palette.** `flags.pal_from_block` and a `pal_addr` to **48 bytes**:
   16 entries of `R, G, B` (8-bit, same order as `$06`). Reloaded every VBL,
   so a theme switch or palette page-flip is a pointer store. `pal_addr == 0`
   with the bit set is invalid (blank).

If `pal_from_block` is clear, `$06`/`$07` own the 16 colors. If the bit is
set, the block table wins that frame (commands still update the fallback for
when the bit is cleared).

`$08` SetBorder is the overscan color while scanning **and** the color of the
unarmed / `ScreenOff` blank. Default black.

---

## 11. Conventional location

Cells start at **`$2000`**. The control block sits at the **end of the 4K
page** (`$2FE0`–`$2FFF`). That is the 80×25 map.

Hires page 1 is already in the IIgs shadow window and IIe cards snoop it.
Page-aligning the buffer lets 6502/65C02 code use `STA $2000,X` / `LDA
$2000,X` without a page-cross penalty on the first 256 bytes, and every row
base that itself lands on a page boundary. A header at `$2000` would force
`STA $2020,X`, which crosses into `$2100` at `X = $E0`.

| | Address | Size | Aux flag |
| --- | --- | --- | --- |
| Cells (80×25) | `$2000` | 4000 bytes | `buffer_aux` / `attr_aux` |
| Optional palette | `$2FA0` | 48 bytes | `pal_aux` |
| Control block | `$2FE0` | 32 bytes | `SetTextCtrl` flags.aux |

```
$2000 ─ $2F9F    interleaved cells, or chars then attrs (4000)
$2FA0 ─ $2FCF    optional 16×RGB palette (48); $2FD0–$2FDF pad
$2FE0 ─ $2FFF    control block
```

Physically that is IIe main/aux, or IIgs Mega II `$E0`/`$E1`. Drivers still
write 16-bit addresses.

**Bring-up (80×25 interleaved, virt_rows=25, all main):**

```
cells at $2000                     ; 80*25*2 = 4000
ctrl  at $2FE0                     ; SetTextCtrl($2FE0), aux=0
  flags = wrap                     ; or planar | wrap
  cols=80, vis=25, virt=25, start_line=0
  buffer_addr = $2000
  attr_addr   = $0000              ; packed after chars if planar
  pal_addr    = $2FA0              ; or $0000 and use $06
```

**Planar split across aux:** chars `$2000` main (2000 bytes), attrs `$2000`
aux. Header still `$2FE0` main. `attr_aux` set. Fits the same 4K on each
side (ctrl + palette live only in main; aux `$2FA0`–`$2FFF` is unused).

**80×50 / virt_rows=50:** 8000-byte buffer does not fit in 4K. Same idea,
end of HGR1 (8K): cells `$2000`–`$3F3F`, palette `$3FA0`, ctrl `$3FE0`.
`SetTextCtrl($3FE0)`.

**Page-flip pair:** buffer A at `$2000` (4K or 8K as above), buffer B at
`$4000` (HGR2) or in aux. Control block stays at `$2FE0` (or `$3FE0`); only
`buffer_addr` / `attr_addr` (and aux bits) move.

---

## 12. Size cheat sheet (80 columns)

| virt_rows | Chars | Attrs | Interleaved | Conventional fit |
| --------- | ----- | ----- | ----------- | ---------------- |
| 25 | 2000 | 2000 | 4000 | 4K: cells `$2000`–`$2F9F`, ctrl `$2FE0` |
| 43 | 3440 | 3440 | 6880 | 8K HGR1, ctrl `$3FE0` |
| 50 | 4000 | 4000 | 8000 | 8K: cells `$2000`–`$3F3F`, ctrl `$3FE0` |
| 200 | 16000 | 16000 | 32000 | SHR `$2000` (32K, usually aux) |

80×25 in 4K is the default. virt_rows=50 (hardware scroll headroom) takes
the rest of HGR1 and moves the block to `$3FE0`.

---

## 13. Hardware cursor

If `flags.cursor` is set, the card inverts or replaces the cell at
(`cursor_x`, `cursor_y`) in *visible* space (after freeze + scroll). Blink
rate is card-defined (~2 Hz). `cursor_x >= cols` or `cursor_y >= vis_rows`
hides it.

v0.1: block cursor, full cell. Shape / underline is a later flags nibble if
anyone cares.

---

## 14. Driver sketch (GNO)

```
SetMode($03, HOSTTEXT)              ; 80×25 9×16, still blank
; clear cells at $2000; write control block at $2FE0
; load palette at $2FA0 or SetPalette
SetTextCtrl($2FE0)                  ; aux=0; next VBL is live
SetTextFont($02)                    ; PC ANSI, optional
```

Hot path: stores into planes, `INC start_line` on LF, `chgat` into attrs.
No SS commands. Alternate screen: second buffer + store `buffer_addr`.
25 ↔ 43: `SetMode` to a new raster, then rewrite `vis_rows` (keep armed).

IOCTL reports `cols` × `vis_rows`. Termcap: ANSI, 16-color SGR.

---

## 15. v0.1 vs later

**v0.1**

- Raster `$03` (80×25 9×16) only.
- Control block, `SetTextCtrl`, blank-until-armed.
- Interleaved and planar, wrap scroll, freeze rows, hardware cursor.
- `$06` / `$07` / `$08` / `$0F`.
- Conventional map in §11.

**Later**

- Rasters `$43` / `$50` / `$01`.
- WaitVBL command if polling `start_line` vs beam ever matters (latch should
  make this unnecessary).
- Cursor shape.

---

## 16. Back-porting

A real legacy SecondSight card may be capable of this mode. It depends on how quickly the Z180s DMA controller can blit from shadowed apple II RAM into VGA memory, and if it can complete during VBL.

IT might not be required to do it all during VBL,  as a beam-racing or beam-trailing approach could be taken.


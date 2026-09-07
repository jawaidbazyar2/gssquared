# Second Sight GPU Text Mode

**Status:** draft v0.1 (2026-08-28)
**Card mode:** SetMode emulation flag `$05`
**Depends on:** [SecondSight.md](SecondSight.md) (classic VGA API, handshake, slot I/O)

This is a *new* mode: the card owns a VGA-style text cell buffer in **card
VRAM** and accepts a **16-bit word stream** on `C0B1`. The host is a terminal
encoder, not a framebuffer painter. Scroll, insert/delete line, and fills run
on the card. Unique glyphs cross the 1 MHz bus once.

Same fence as GPU mode ([SecondSight_GPU.md](SecondSight_GPU.md)) and Host Text
([SecondSight_Text.md](SecondSight_Text.md)): this is the API the card should
have grown, not a modeline. Classic mode 03h VRAM upload stays frozen for
Cogito / Spectrum.

Origin of the split (host policy vs card VRAM, stream-on `STA C0B1`, GPU-ish
text primitives, why *not* to memmove a linear buffer in bank 0) is the
**27 Aug 2026** note in [DevelopLog.md](DevelopLog.md). This document is that
design after the tagging / IIe / DP refinements that followed.

```
  Host Text ($04)                       GPU Text ($05)
  ───────────────                       ──────────────
  cells in shadowed Apple RAM           cells in card VRAM
  STA into $2000                        STA C0B1 word stream
  start_line / wrap for LF              ScrollUp command (any region)
  insert/delete line = host memmove     InsertLine / DeleteLine on card
  readback is free                      host shadow (ReadCells is experimental)
```

Original 1995 Second Sight silicon is **not** required to implement this.
A write-IRQ into a ring buffer plus a consumer that blits cells in local RAM
is enough; the Z180 path is one way to get there. The emulator can execute
the stream in C++ next to the existing text renderer.

---

## 1. Why a distinct mode

Three ways to get a GNO / ANSI / IIe console on Second Sight were on the
table (DevelopLog, 27 Aug 2026):

1. **VT100/ANSI in a GS/OS or GNO process**, painting Host Text RAM (or
   classic uploads). Flexible. The 65816 still **moves** cells. A software
   scroll of an 80×25 interleaved buffer is on the order of a video frame of
   Mega II time. GNO insert/delete line cannot use `start_line`.
2. **Terminal entirely on the card.** Host sends a byte stream; the card
   parses ANSI. Fastest to prototype in the emulator. The II and the card
   fight over cursor/wrap unless the parser is perfect, and IIe firmware
   (`CH`/`CV`, text window) is a different language than VT100.
3. **GPU text commands.** Host translates *its* language (IIe `COUT`, GNO
   termcap, a VT100 state machine) into a small blit ISA. The card never
   wraps or scrolls unless told. Unique characters are cheap stores. Motion
   is a one-word command.

This mode is (3). It is not Host Text (scanout of `$2000`). It is not GPU
mode `$03` (pixel CSB). It is not classic VGA text (handshake per DMA).

The 1 MHz bus is a capacity of about two bytes per microsecond. Spend it on
**new** cells. Never spend it on **moving old ones.**

---

## 2. Arming: stream on with SetMode

`SetMode` with flag `$05` and a text raster mode number (`$03` = 80×25 9×16,
`$43` = 80×43 8×8; same numbers as Host Text) switches the command table,
allocates a card-side cell buffer, and **turns `C0B1` into a free-running
word FIFO**.

Until `SetMode($05)` succeeds:

- output is **blank** (same pixels as `ScreenOff`);
- writes to `C0B1` are classic DMA data, not this ISA.

No default cells. Driver should `Reset` (or `Erase` page) before `ScreenOn`.

Leaving GPU Text (`SetMode` with flag `$00` / `$01` / `$02` / `$03` / `$04`)
**ends the stream** and discards FIFO contents. Re-entry is blank until the
host paints again.

`ScreenOff` / `ScreenOn`, `SetPalette` / `SetPaletteEntry`, `SetBorder`,
`SetTextFont` stay on **`C0B0` handshake** (same fence as Host Text). They are
not 16-bit stream opcodes. `UploadData`, `ScrollScreen`, `ClearScreen`,
`SetVGAReg`, GPU `$40`–`$43` are **fenced** (`$A6`).

There is no per-word `$A5`. Putc does not poll. While the GPU Text stream is
armed, **`$C0B8` is the classic handshake** (§3): **`$00` = not ready** (ring
still has work), **`$01` = ready** (buffer empty). Same sense as DMA
(`$01` = ready). No opcode. Classic `SetMode` / `SetPalette` still use
`$C0B0` plus `$C0B8` as in [SecondSight.md](SecondSight.md) when those
handshake commands are in flight.

---

## 3. Card-side execution

Every implementing card (FPGA, Z180, emulator thread) does this:

1. A write to `C0B1` **interrupts** (or equivalent: the slot decoder strobes
   a “byte/word arrived” event).
2. The ISR (or decoder) **only** pushes into a **ring buffer**. It does not
   run `ScrollUp`.
3. A **consumer** (second thread, main loop, Z180 foreground) pulls words
   and executes the ISA in order.

Assume every opcode finishes in **a few microseconds** — faster than a 1 MHz
(or 2.8 MHz with 1 MHz I/O) Apple II can emit the next word. The ring exists
so the ISR stays short, not because the consumer is slow.

**Scanout.** Host selects with **`SetSync` (`$93`)** (§6.3). Default on
`SetMode` entry: **immediate**. `Reset` (`$92`) does not change this.

- **Immediate (arg `$00`):** consumer runs as soon as the ring is non-empty;
  the next VGA frame shows whatever VRAM has become. Low-latency `COUT`.
- **VBL (arg `$01`):** consumer (or present) applies queued words at the
  card’s next VBL. Tear-free. `$C0B8` = `$01` means the **ring** is empty
  (ready); scanout may still be one frame behind.

`SetSync` is **in-order**: it takes effect when the consumer executes it.
If the card is already in VBL mode, a switch to immediate sits in the ring
until that VBL, then the rest of the ring drains eager.

**Bandwidth bound.** Slot I/O is Mega II speed: at most one byte per 1 MHz
cycle. An NTSC frame is **17 030** CPU/slow cycles (262 × 65), so the bus
cannot deliver more than **17 030 bytes/frame** into the card. A real
console never hits that: each word is an `LDA` plus a `STA`, so perhaps
**~¼** of the ceiling (~4 KB/frame) is the planning number.

Size the ring for **one frame at the bound** (17 KB is the hard cap; 4–8 KB
is enough in practice). If VBL-batched, that is also the maximum command
list presented in one frame.

**`$C0B8` (no command).** Same polarity as classic SS: `$00` = not ready,
`$01` = ready.

| `$C0B8` | Meaning |
| --- | --- |
| `$00` | Not ready — data still to process (ring not empty) |
| `$01` | Ready — buffer empty |

The host may `LDA $C0B8` at any time. Ready means the consumer has taken
every enqueued word (immediate: VRAM matches the stream). Writes after a `$01`
sample start a new fill; `$C0B8` goes `$00` (not ready) again.

```
wait    LDA   $C0B8
        CMP   #$01
        BNE   wait          ; until ready — ring empty
```

Do not poll per glyph. Use it before `SetMode` away if you must not discard
queued words.

**Backpressure** is left open. If the ring would overrun (host faster than
drain for a pathological burst, or a stuck consumer), possible valves —
none required in v0.1:

- Stretch/stall the `C0B1` write until there is space (the II just sees a
  slow cycle).
- Leave `$C0B8` at `$00` (not ready; host already has that bit).
- Drop oldest or newest (hostile to a terminal; last resort).

The ISA is **one-way** (host → ring → VRAM). `$C0B8` is status, not reverse
traffic. `ReadCells` still turns `C0B1` around; see §6.5.

---

## 4. Split of responsibility

**Host owns policy.** It keeps `CH`/`CV` (or VT100 cursor), wrap vs stick,
when LF becomes `ScrollUp`, the ANSI/GNO/IIe parser, tab stops, and (if
needed) a fast-RAM shadow of the cells for curses readback.

**Card owns VRAM.** It tracks a cursor `(x,y)` used only to place the next
putc and to clip. After a putc it does `x++` and **clamps** at the right
margin. It does **not** wrap, does **not** scroll, does **not** interpret
`CR`/`LF`/`BS`. If the host would wrap or index off the bottom of the region,
the host sends `ScrollUp` / `SetY` itself.

That is the “don’t get too fancy” rule from the 27 Aug note: if the card
only advances under this clamp, the host never has to read cursor x/y back
to stay synced with Apple II land.

Coordinates are **absolute** in the raster (column 0 is left of the screen).
The host adds `WNDLFT` / `WNDTOP` when talking IIe window-relative `CH`/`CV`.
Margin registers clip scroll, insert/delete, erase, and putc clamp — they
do not rebase `(0,0)`.

---

## 5. Word stream

Every unit is a **16-bit word**, little-endian on the wire.

### 5.1 Ports

| CPU | How a word lands |
| --- | --- |
| 65816, `M=0` | `STA $C0B1` writes lo → `$C0B1`, hi → `$C0B2` |
| 6502 / 65C02 | `STA $C0B1` (lo), `STA $C0B1` (hi) — two byte writes into the same FIFO |
| 65816 hot loop | `D = $C000` (page-aligned), `STA $B1` — **4** cycles vs **5** absolute; still two 1 MHz writes. `PHD` / `TCD` / `PLD` around a burst. Do **not** set `D = $C0B1` (`DL ≠ 0` eats the saving). Restore `D`; `$C000` as DP turns `LDA $24` into I/O. |

`C0B2` as a *classic* read-data register is not used for this FIFO. 16-bit
`STA $C0B1` must not be followed by `STA $C0B2` with `M=0` (that is
`$C0B2`/`$C0B3`).

Stay in **16-bit A** on the 816. `SEP`/`REP` around 8-bit opcodes costs more
than packing commands as 16-bit words. VGA cells are 16-bit; so are opcodes.

### 5.2 Tag: bit 15

Pack **character in the low byte, attribute in the high byte** (IBM `AL` /
`AH`).

| High byte | Meaning | Low byte |
| --- | --- | --- |
| `$00`–`$7F` | **putc** — one cell, clamp-advance | CP437 |
| `$80`–`$FF` | **command** (128 opcodes) | 8-bit argument |

Bit 15 is attr bit 7. The fast path therefore has **16 fg × 8 bg**, bold
(fg intensity, attr bit 3) included, **no blink** and **no bright
backgrounds**. That matches IIe 80-column firmware (inverse is a 7-bit attr
in zp; there is no VGA blink). Linux/ANSI SGR `30–37` / `40–47` / bold /
reverse fit. SGR `100–107` and blink use **FullCell** (`$80`).

```
; 816, M=0, A = char in lo (bit 15 already clear)
ORA   zp_attr      ; 3   attr in $00–$7F
STA   $C0B1        ; 4–5
```

```
; IIe 65C02, A = glyph (COUT)
STA   $C0B1        ; 4   char
LDA   attr         ; 3   zp, $00–$7F
STA   $C0B1        ; 4   attr
```

Eleven cycles on a IIe vs BASCALC + `80STORE` + aux/main. That is the
firmware-replacement win.

**Count convention:** argument `0` means **1** (same as the 27 Aug sketch).
Repeat/scroll/ins/del never need a true zero. A 2000-cell clear is `Erase`,
not a 2000-count Repeat.

### 5.3 Quoted payload

After a command that takes a **cell operand**, the next word is **untagged**:
a full IBM `attr:char`, bit 15 allowed. It is **not** parsed as an opcode.
That is how blink / 16-bg cells exist at all.

`FullCell` (`$80`) is that quote for a single putc: opcode word, then opaque
cell, then clamp-advance. Slightly more expensive; rare.

---

## 6. Command codes

128 opcodes `$80`–`$FF`. v0.1 uses `$80`–`$93` (`$94` experimental).
Unused codes must `$NOP`-safe or `$A6` the FIFO until StreamEnd —
**reserved = ignore one word**, so a future host can skip.

Necessary and sufficient for **IIe 80-col console firmware** and a
**VT100 / linux (ANSI) emulator** on the host. The card does not parse ESC.

| Code | Name | Arg | Extra word | Effect |
| --- | --- | --- | --- | --- |
| `$80` | `FullCell` | ignored | untagged cell | Putc with 8-bit attr (blink / 16 bg). Clamp-advance. |
| `$81` | `Repeat` | count | untagged cell | Overwrite `count` cells at cursor with that cell; `x += count`, clamp. No shift. `clrtoeol` / `ECH` / tabs-as-spaces / box fill. |
| `$82` | `SetX` | column | — | Absolute column. Clipped to raster then to right margin for subsequent putc. |
| `$83` | `SetY` | row | — | Absolute row. |
| `$84` | `ScrollUp` | lines | — | Region inside margins moves up; new bottom rows filled with space + **current attr**. |
| `$85` | `ScrollDown` | lines | — | Opposite (VT100 `SD`, RI at top). |
| `$86` | `InsertLine` | count | — | IL inside margins (GNO / VT100 `IL`). |
| `$87` | `DeleteLine` | count | — | DL inside margins. |
| `$88` | `InsertChar` | count | — | ICH: shift rest of line right, fill with space + current attr. |
| `$89` | `DeleteChar` | count | — | DCH: shift rest of line left, fill at margin. |
| `$8A` | `Erase` | sub | — | See §6.1. Uses space + **current attr**. Clipped to margins unless sub says full raster. |
| `$8B` | `SetAttr` | attr `$00`–`$7F` | — | Current SGR for Erase / IL fill / scroll fill. Does not change already-written cells. |
| `$8C` | `SetAttrFull` | ignored | untagged cell | Current SGR ← payload high byte (full IBM attr). Low byte ignored. |
| `$8D` | `SetLeft` | column | — | Left margin (IIe `WNDLFT`, DEC `DECSLRM`). |
| `$8E` | `SetRight` | column | — | Inclusive right column (`WNDLFT+WNDWDTH-1`). |
| `$8F` | `SetTop` | row | — | Top of scroll region (`WNDTOP`, `DECSTBM`). |
| `$90` | `SetBottom` | row | — | Inclusive bottom row. |
| `$91` | `CursorStyle` | ignored | style word | Hardware cursor at card `(x,y)`. See §6.2. |
| `$92` | `Reset` | ignored | — | Margins = full raster, `(x,y)=(0,0)`, attr `$07`, cursor on (blink block), erase full raster. Does **not** change `SetSync`. |
| `$93` | `SetSync` | 0 / 1 | — | `$00` = process immediately; `$01` = batch until VBL. See §6.3. |
| `$94` | `ReadCells` | count | *host reads* `count` words | **Experimental.** See §6.5. |

`SetX` + `SetY` replace a packed `MoveTo`. Two words, 8-bit args, IIe `CH`/`CV`
sized. 80×43 / 80×50 fit.

Invalid margin (`left > right`, `top > bottom`, outside raster): stay at
previous margins (do not blank the screen).

### 6.1 `Erase` subcodes (low byte)

| Arg | VT100 / linux | IIe |
| --- | --- | --- |
| `$00` | `EL 0` — cursor to end of line | `CLREOL` |
| `$01` | `EL 1` — start of line to cursor | — |
| `$02` | `EL 2` — whole line | — |
| `$03` | `ED 0` — cursor to end of screen | `CLREOP` |
| `$04` | `ED 1` — home to cursor | — |
| `$05` | `ED 2` — whole **margin rectangle** | `HOME` clear (window) |
| `$06` | `ED 2` of the **full raster** | rare (ignore window) |
| `$07` | `ECH` 1 at cursor (no advance) | — |

`ECH` of N is `Repeat` of space, or N× `$07`. Prefer `Repeat`.

Linux `ED 3` (clear scrollback) is host-side if the host keeps scrollback;
the card has no scrollback buffer in v0.1.

### 6.2 `CursorStyle` (`$91`)

Hardware cursor at the card’s `(x,y)` — the same cell putc uses. It is not
a glyph in VRAM; scanout overlays it. Position follows `SetX`/`SetY` / putc
advance.

Opcode arg is ignored. The **next word is untagged** (full 16 bits, not a
putc/command tag):

| Bits | Name | |
| --- | --- | --- |
| 0 | `enable` | 0 = hidden (`DECTCEM` off). 1 = draw. |
| 1 | `blink` | 1 = hardware blink (~2 Hz, card-defined). 0 = steady. |
| 2–3 | `shape` | `00` block (full cell), `01` underline, `10` bar (insert; left of cell), `11` reserved (= block). |
| 4 | `replace` | 0 = **invert** the cell (IIe flash). 1 = draw in **current attr** (SGR / `SetAttr`). |
| 5–7 | reserved | 0 |
| 8–11 | `start` | First scanline of the glyph cell (0 = top). `$F` = default for `shape`. |
| 12–15 | `end` | Last scanline, inclusive. `$F` = default. If `start > end`, treat as default. |

Defaults when `start`/`end` are `$F`: block = full cell height (8 or 16);
underline = last two rows; bar = full height, 1–2 pixels wide (not a CRTC
scanline pair).

```
        LDA   #$9100
        STA   $C0B1
        LDA   #$0003        ; enable + blink, block, invert, default lines
        STA   $C0B1
        LDA   #$9100
        STA   $C0B1
        LDA   #$0000        ; hidden
        STA   $C0B1
```

`Reset` loads `$0003`. IIe 80-col: show = `$0003`, hide = `$0000`. Linux
`DECSCUSR` / `CSI ? 25 h/l` map to this word; the host does not send
scanlines unless it wants a custom underline.

### 6.3 `SetSync` (`$93`)

```
        LDA   #$9300        ; immediate
        STA   $C0B1
        LDA   #$9301        ; VBL batch
        STA   $C0B1
```

Arg other than `$00`/`$01`: ignore, keep previous mode.

IIe firmware / GNO `COUT`: leave **immediate**. Full-screen redraws or
scroll that must not tear: **VBL**, then optional poll `$C0B8` = `$01` after
the burst (ready / empty ring; picture updates at that VBL).

### 6.4 Ring idle (`$C0B8`)

Not an opcode. See §3. Poll `$C0B8` until `$01` (ready) if you need the
buffer empty.

### 6.5 `ReadCells` (`$94`) — experimental

**Not a v0.1 requirement.** Cards may ignore the opcode (treat as a one-word
`NOP`). Hosts must not depend on it. Prefer a **fast-RAM shadow** of cells
the host already wrote.

The rest of this ISA is **one-way**: `STA $C0B1` → ring → consumer → VRAM.
`ReadCells` turns `C0B1` around. That fights the interrupt/ring model:

- **When does the card execute it?** Immediate: microseconds later. VBL:
  not until the next VBL, so earlier words in the same ring have not hit
  VRAM yet either.
- **How does the II know it can `LDA`?** `$C0B8` = `$01` means the **write
  ring** is empty (ready), not that `C0B1` has become a read port. You would
  still `STA` `ReadCells` (which makes `$C0B8` = `$00` again) and then need a
  second ready-wait before `LDA` — and VBL-batch plus port turnaround stay
  racy.

If a future card implements it, the sketch is:

After the opcode, `C0B1` is a **read** port for exactly `count` 16-bit words
(`0` = 1). Each word is an **untagged** IBM cell (`attr:char`, bit 15 may be
set). Then `C0B1` is a write FIFO again. Starts at the current cursor; each
word consumed advances like putc (clamp, no wrap).

```
; 816, M=0 — experimental
        LDA   #$94CC
        STA   $C0B1
        ; $C0B8 empty-wait still unspecified vs port turnaround
        LDA   $C0B1
        STA   BUF
        ; …
```

Do not ship firmware that needs this.

### 6.6 Not on the card

| Host-side | Why |
| --- | --- |
| `CR` / `LF` / `BS` / `HT` / `NEL` / `IND` / `RI` | Policy: host issues `SetX`/`SetY`/`Scroll*` |
| Tab stops (`HTS`/`TBC`) | Host |
| Save/restore cursor (`DECSC`) | Host stack |
| SGR parser, UTF-8, GNO termcap | Host maps to `SetAttr` + putc |
| Bell | Speaker, not VRAM |
| Full-screen / curses readback | Host shadow in fast RAM. `ReadCells` is experimental (§6.5). |

Insert/delete **character** *is* on the card so curses does not read the
rest of the line across 1 MHz.

---

## 7. Current attribute

`SetAttr` / `SetAttrFull` load the fill/erase pen. Putc **does not** change
it (the cell’s attr is in the putc word). After `Reset`, current attr is
`$07` (IBM gray on black).

IIe inverse: host sets zp `attr` to reversed nibbles (e.g. `$70`) and uses
the fast putc path; also `SetAttr` so `HOME`/`CLREOL` fill inverse spaces if
that is what firmware would do.

---

## 8. IIe console mapping

Replacement slot-3 / 80-col firmware (or a `COUT` hook). Apple high bits
stripped to `$20`–`$7F` (or mapped to CP437). Inverse → 7-bit attr. Flash →
inverse or ignored (no `$80` FullCell required).

| Firmware | Stream |
| --- | --- |
| `COUT` glyph | putc (two `STA $C0B1` or one 16-bit store) |
| `CR` | `SetX` left margin |
| `LF` | if `CV` at bottom: `ScrollUp 1`; else `SetY CV+1`. `SetX` left if you want `NEL`. |
| `BS` | `SetX x-1` (clamp left); optional space-overwrite is host |
| `TAB` | `SetX` next stop, or `Repeat` spaces |
| `HOME` | `SetX`/`SetY` origin, `Erase $05` |
| `CLREOL` | `Erase $00` |
| `CLREOP` | `Erase $03` |
| `VT` / `ESC` | host |
| `WNDLFT/WDTH/TOP/BTM` | `SetLeft/Right/Top/Bottom` |
| Cursor on / off | `CursorStyle` extra `$0003` / `$0000` |
| Scroll at window bottom | `ScrollUp` — **not** a host copy of `$400`/`$E1` |

Window setup once when `WND*` changes. Per-character path never touches
`80STORE` or BASCALC.

---

## 9. VT100 / linux (ANSI) mapping

Host is a state machine ([`console_codes(4)`](https://man7.org/linux/man-pages/man4/console_codes.4.html),
[ANSI](https://en.wikipedia.org/wiki/ANSI_escape_code)). Card sees only the
ISA above.

| Sequence | Stream |
| --- | --- |
| printable / UTF-8 → CP437 | putc or `FullCell` |
| `CSI n C/D/A/B` | `SetX`/`SetY` |
| `CSI y;x H` / `f` | `SetX` `SetY` (absolute; host adds origin if `DECOM`) |
| `CSI n S` / `T` | `ScrollUp` / `ScrollDown` |
| `CSI n L` / `M` | `InsertLine` / `DeleteLine` |
| `CSI n @` / `P` | `InsertChar` / `DeleteChar` |
| `CSI n K` | `Erase $00/$01/$02` |
| `CSI n J` | `Erase $03/$04/$05` |
| `CSI n X` | `Repeat` space |
| `CSI n b` (REP) | `Repeat` last cell (host remembers) or N putcs |
| `CSI n;m r` `DECSTBM` | `SetTop` `SetBottom` |
| `DECSLRM` / IIe width | `SetLeft` `SetRight` |
| `SGR` | `SetAttr` / `SetAttrFull`; following putcs carry the attr in-band too |
| `RIS` / `CSI 2 J` + home | `Reset` or `Erase $06` + `SetX 0` `SetY 0` |
| `CSI ? 25 h/l` `DECTCEM` | `CursorStyle` enable bit |
| `CSI n q` `DECSCUSR` | `CursorStyle` blink + shape (0–2 block, 3–4 underline, 5–6 bar) |
| `DECSC`/`DECRC` | host |
| `IND`/`RI`/`NEL` | `SetY` ± 1 or `Scroll*` at edge |
| `ESC 7/8`, charset G0/G1 | host (map to CP437) |

16-color GNOME/linux console is in scope. 256-color / truecolor SGR is
**not** (VGA text DAC is 16 entries; use `SetPalette` for themes).

GNO insert/delete line is `$86`/`$87` — the case Host Text `start_line`
cannot accelerate (DevelopLog, 27 Aug).

---

## 10. Cost (why this beats Host Text *without* `start_line`)

| Op | Host Text, software scroll | GPU Text |
| --- | --- | --- |
| Fast putc (816) | `STA abs,X` ~2 slow writes | 1× 16-bit `STA $C0B1` ~2 slow writes |
| IIe putc | BASCALC + 80-col banks | 11 cycles, two `STA $C0B1` |
| LF at bottom | ~24–28k cycles / ~0.6–1 frame | `ScrollUp` one word |
| IL/DL in the middle | host `memmove` | one word |
| `HOME` / `ED 2` | 4000 stores | `Erase $05` one word |

A two-word `command + cell` putc was the 27 Aug sketch (~16 cycles, four
slow writes). Bit 15 tagging makes the common glyph **one word**. `Repeat`
(`$81 $10` + space cell) is `0510 00A0` from that note, as a command plus
quoted cell.

---

## 11. Bring-up

```
SetMode($03, $05)          ; 80×25 GPU Text, C0B1 is the FIFO
SetTextFont($02)           ; PC ANSI, optional
SetPalette / SetBorder     ; optional; IBM default otherwise
Reset                      ; $92: clear, attr $07, cursor home, blink block
; SetSync $9300 immediate (default) or $9301 VBL
ScreenOn
; then putc / SetAttr / ScrollUp …
```

80×43: `SetMode($43, $05)`. Same ISA. `SetBottom` 42.

The ring is sized around one frame of slot traffic (§3). Do not poll `$C0B8`
per glyph. `LDA $C0B8` until `$01` if you must know the buffer is empty
(e.g. before `SetMode` away). Leaving the mode may discard unread ring words.

---

## 12. v0.1 vs later

**v0.1**

- Rasters `$03` / `$43`.
- Bit 15 putc vs `$80`–`$93` (`$94` experimental).
- Clamp-advance, margins, no auto wrap.
- `CursorStyle` (`$91`): enable, blink, block/underline/bar, invert vs attr, scanlines.
- Write-IRQ → ring → consumer; one-way stream.
- `SetSync` (`$93`): `$00` immediate, `$01` VBL. Default immediate.
- `$C0B8`: `$00` = not ready (work in ring), `$01` = ready (empty). No idle opcode.
- IIe two-byte `C0B1` path and 816 16-bit / `D=$C000` path.

**Later**

- `ReadCells` (`$94`) if turnaround after `$C0B8` = `$01` is ever made precise.
  Until then: host shadow.
- Backpressure if a card ever drains slower than the 17 KB/frame bound (§3).
- Packed `SetXY` extra word.
- Horizontal pan (rarely needed for VT100).
- Scrollback on the card (`ED 3`).
- Flag `$05` listed in [SecondSight.md](SecondSight.md) “New Mode Definitions”
  next to `$02` PPU / `$03` GPU / `$04` Host Text.

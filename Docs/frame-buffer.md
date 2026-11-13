# Frame buffer

The CPU-drawn screen. QuickDraw writes pixels here. The scanner reads the same bytes. No command packet is involved. The GPU command path is [gpu-api.md](gpu-api.md). The same register file selects a second scanner, [PPU mode](#ppu-mode). The linear frame buffer does not scroll.

The enhanced window is banks `$D0` through `$DF`, one mebibyte. Bank `$D0` is not IIgs DRAM. Installed RAM ends at bank `$7F`. The memory map translates `$D00000`–`$DFFFFF` to a physical store the video controller scans. A slot card cannot implement this window: the slot connector carries a 16-bit Mega II address, and a store to `$D0xxxx` is never a slot cycle. The whole window is write-through or uncached.

`QDStartUp` does not reserve `$E1/2000`, and it does not touch the shadow register, for an enhanced mode. `$E12000` remains the classic super-hi-res buffer, used when the depth register is 0.

## Softswitches

Super hi-res is still bit 7 of `$C029` (`NEWVIDEO` at `$E0C029`). `$C02A` stays reserved. Depth 0 is today’s super hi-res, and any other depth is the enhanced window. Text, lo-res, and double hi-res are unchanged when `$C029` bit 7 is clear.

### `$C069`–`$C06C` — video register file

`$C069`/`$C06A` is the register number. `$C06B`/`$C06C` is the value. Both are 16-bit. A word store to `$E0C069` selects a register. A word store to `$E0C06B` writes it, and a word load reads it back. `$C068` is `STATEREG`, so a word store there must not be used to reach the index. `$C06D` is `TESTREG`, so a word store at `$E0C06C` must not be used to reach the value.

An interrupt routine that uses the file reads `$E0C069` first and writes that number back when it is done. Power-on value of every writable register is 0. Capabilities is fixed by the device.

| Number | Register |
|---:|---|
| 0 | Depth, in bits per pixel. 0 is classic. 2, 4, 8, and 16 are enhanced. Any other value behaves as 0. |
| 1 | Width, in pixels. |
| 2 | Height, in scan lines. |
| 3 | Start page. A 16-bit register. Bits 15–12 read as zero. |
| 4 | Scanner. 0 is the linear frame buffer. 1 is PPU mode. Any other value behaves as 0. |
| 5 | PPU control. |
| 6 | PPU mask. |
| 7 | PPU status. Read-only. |
| 8 | PPU scroll X, in pixels. |
| 9 | PPU scroll Y, in pixels. |
| 10 | Nametable mirror. |
| 11 | Display. 0 shows the picture. 1 suppresses it. |
| 12 | Capabilities. Read-only bitfield. |

```
LDA #11         ; display
STA $E0C069
LDA #1
STA $E0C06B     ; suppress, scanner holds its working copy

LDA #0          ; depth
STA $E0C069
LDA #2
STA $E0C06B     ; 2 bits per pixel

LDA #1          ; width
STA $E0C069
LDA #640
STA $E0C06B

LDA #2          ; height
STA $E0C069
LDA #400
STA $E0C06B

LDA #11
STA $E0C069
LDA #0
STA $E0C06B     ; show, adopted at the next frame
```

Register 12 tells the host what this device implements. Bit 0 is set when PPU mode is present. The other bits read as zero. A write does nothing. Selecting scanner 1 with bit 0 clear behaves as scanner 0.

Register 11 is the blanking switch for every scanner, classic included. One makes the output black and stops the scanner from reading memory or adopting new parameters. `$C019` and the PPU vblank latch keep running. Writing 0 adopts the current file at the start of the next frame, so a new depth, size, page, and scanner appear together. PPU scroll, control, mask, and mirror still update at each scan line once the picture is showing.

Scanner 0 and depth 0 is today’s super hi-res. Width, height, and the page are ignored. Scan lines are 200 tall. SCB bit 7 chooses 320 or 640, and the low nibble chooses the palette. The pixels stay at `$E12000`, the SCBs at `$E19D00`, the palettes at `$E19E00`. Registers 5–10 are ignored too.

An enhanced depth fixes one raster for the whole frame. The visible stride is the packed width: width/4 at 2 bits, width/2 at 4 bits, width at 8 bits, and width×2 at 16 bits. The width has to produce a whole number of bytes. These are the modes in view:

| Depth | Width | Height | Stride | Bytes |
|---:|---:|---:|---:|---:|
| 2 | 640 | 400 | 160 | 64,000 |
| 4 | 640 | 400 | 320 | 128,000 |
| 4 | 640 | 480 | 320 | 153,600 |
| 8 | 640 | 480 | 640 | 307,200 |
| 16 | 640 | 480 | 1280 | 614,400 |
| 8 | 800 | 600 | 800 | 480,000 |
| 8 | 1024 | 768 | 1024 | 786,432 |
| 8 | 320 | 200 | 320 | 64,000 |
| 16 | 320 | 200 | 640 | 128,000 |

A 640-pixel 8-bit line is 640 bytes. The 1024 above is the width of that mode. Any other size uses the same three registers, provided the bytes the scanner reads stay in `$D00000`–`$DEFFFF`.

### Start page

Register 3 is the start page, in both scanners. It is a 16-bit register. The window is one mebibyte, so bits 15–12 read as zero and the useful value is 12 bits.

```
origin = $D00000 + (page << 8)
```

The origin can be any 256-byte boundary in the window. A second buffer can begin on the 256-byte boundary after the first. Bank `$DF` is the table bank and is not a legal origin. The largest frame-buffer mode, 1024×768 at 8 bits, is 786,432 bytes and still ends at `$DC0000` when the origin is `$D00000`. A PPU image occupies `origin` through `origin+$40FF`, so the same register can point at several of them.

## Tables

Enhanced frame-buffer modes use this bank. Classic depth and PPU mode do not.

```
$DF0000                                            $DFFFFF
+------------------+------------------+-------------------+
| SCB table        | palettes         | unused            |
| $DF0000–$DF01FF  | $DF0200–$DF03FF  | $DF0400–$DFFFFF   |
| 512 bytes        | 512 bytes        |                   |
+------------------+------------------+-------------------+
```

The SCB table holds one byte per scan line. A 2-bit or 4-bit enhanced mode uses the low nibble as the palette number. Bit 7 is ignored, because the width register already fixed the width. An 8-bit or 16-bit mode does not read SCBs.

The palette block is 512 bytes, at `$DF0200`. A 2-bit or 4-bit enhanced mode reads it as 16 palettes of 16 words. An 8-bit mode reads the same bytes as one palette of 256 words. A 16-bit mode does not consult it.

A 4-bit pixel is the index into the palette the SCB names. The high nibble of a byte is the left pixel.

A 2-bit pixel is not an index by itself. The scanner applies the classic 640-mode column map. Four pixels share a byte, leftmost in bits 7–6. Column `x` and pixel value `p` (0–3) select

```
index = ((x & 3) << 2) | p
```

`x` is the column on the screen. The four pixels of a byte therefore take entries 0–3, 4–7, 8–11, and 12–15 of that palette. A color that is not constant across those four groups is a four-pixel pattern, one value per column phase. QuickDraw’s 640 pattern expansion already writes the pixels that way. The scanner is what turns them into colors in every enhanced 2-bit mode. A 4-bit mode does not do this.

Each enhanced entry is a 15-bit color in a 16-bit word. Bit 15 is zero. Bits 14–10 are red, 9–5 green, 4–0 blue. That is 32,768 colors, five bits per channel. A 16-bit pixel is this same word, so the color stored in the frame is the color stored in the palette.

Classic depth is unchanged. Its entries at `$E19E00` remain 12-bit `$0RGB`. Those words are not enhanced entries. The bit fields do not line up. Promoting a 4-bit channel `c` into a 5-bit channel is `(c << 1) | (c >> 3)`.

## Bank crossings

A packed stride other than a divisor of 64 KiB splits some scan lines across a bank. `65536 / 640 = 102.4`, so in 640×480×8, line 102 starts 256 bytes before the end of a bank and ends in the next one. The same happens for strides 160, 320, 800, and 1280 once the frame is taller than one bank. A frame of 64,000 bytes or less stays inside the origin’s bank when the origin is bank-aligned: 640×400 at 2 bits, and 320×200 at 8 bits. Classic depth stays at `$E12000`.

That split is acceptable. The scanner holds a 24-bit pointer and adds the stride with a carry into the bank.

The 65816 form that crosses a bank is a 24-bit address plus an index: absolute long indexed (`LDA long,X`) or indirect long indexed (`LDA [dp],Y`). The index is added to all 24 bits. Absolute indexed (`LDA abs,X`) adds the index in 16 bits and wraps inside the data bank. An inner loop that uses the absolute form will wrap to the start of the bank instead of continuing the line. `ClearScreen` and the slab store use the long form, or they keep a 24-bit pointer and step it.

## PPU mode

Scanner 1 composites a 256×240 picture the way the NES picture processing unit does. The CPU writes tiles, name tables, a palette, and a sprite list into ordinary memory at the start page. There is no second address and data port. Transparent pixels show the backdrop. The 256×240 picture is the output, and the area around it is the backdrop.

`$C019` still follows the beam. PPU status bit 7 is a separate latch: set when blanking starts, cleared by reading register 7 and when blanking ends. Control bit 7 arms an interrupt on the rising edge of that latch, on the same VBL interrupt `$C041` bit 3 enables. Setting the bit while the latch is already set raises the interrupt.

While the display is on, the scanner samples control, mask, both scrolls, and the mirror at the start of each scan line. A write during a line shows up on the next line. Sprite 0 hit is the way software finds the line it wants to split on.

### Memory

Addresses are offsets from the page origin. The CPU writes the canonical addresses. The scanner applies the mirrors.

```
origin+$0000   pattern table 0    4096 bytes    256 tiles
origin+$1000   pattern table 1    4096 bytes
origin+$2000   nametable 0        1024 bytes    32×30 tiles, then attributes
origin+$2400   nametable 1        1024 bytes
origin+$2800   nametable 2        1024 bytes
origin+$2C00   nametable 3        1024 bytes
origin+$3F00   palette            32 bytes
origin+$4000   OAM                256 bytes     64 sprites
```

`$3000`–`$3EFF` mirrors the nametables, and `$3F20`–`$3FFF` mirrors the palette, for the scanner only.

A tile is 16 bytes, the NES layout. Bytes 0–7 are plane 0 and bytes 8–15 are plane 1. Bit 7 of a byte is the leftmost pixel. The pixel value is the plane 0 bit plus the plane 1 bit shifted left one. Zero is transparent.

A nametable byte is a tile index. The 64 attribute bytes at offset `$3C0` color the tiles. One attribute byte covers a 32×32 block. Bits 1–0 are the top-left 16×16, bits 3–2 the top-right, bits 5–4 the bottom-left, and bits 7–6 the bottom-right. Those two bits pick one of the four background palettes.

Register 10 chooses how the four nametables alias:

| Value | Name | What the scanner reads |
|---:|---|---|
| 0 | horizontal | `$2400` reads `$2000`, `$2C00` reads `$2800` |
| 1 | vertical | `$2800` reads `$2000`, `$2C00` reads `$2400` |
| 2 | single screen | all four read `$2000` |
| 3 | single screen | all four read `$2400` |
| 4 | four screen | all four are distinct |

Any other value behaves as 0. Horizontal is the power-on value. Four physical tables are always stored, so four-screen needs no extra RAM.

### Scroll

Registers 8 and 9 are the camera, in pixels, across a map of four 256×240 screens. The scanner uses X modulo 512 and Y modulo 480. Y 240 is the first line of the lower screen. There is no dead band.

Control bits 1–0 pick the base nametable and add to that camera. Bit 0 adds 256 to X. Bit 1 adds 240 to Y. The mirror is applied after that, when the scanner turns a map coordinate into a stored nametable.

### Palette

The 32 bytes at `origin+$3F00` are NES color numbers, `$00`–`$3F`. The scanner expands them through the 2C02’s 64-color palette into the 15-bit DAC. The layout and the mirrors are the NES ones.

| Bytes | Use |
|---|---|
| `$3F00` | backdrop |
| `$3F01`–`$3F03` | background palette 0 |
| `$3F05`–`$3F07` | background palette 1 |
| `$3F09`–`$3F0B` | background palette 2 |
| `$3F0D`–`$3F0F` | background palette 3 |
| `$3F11`–`$3F13` | sprite palette 0 |
| `$3F15`–`$3F17` | sprite palette 1 |
| `$3F19`–`$3F1B` | sprite palette 2 |
| `$3F1D`–`$3F1F` | sprite palette 3 |

`$3F04`, `$3F08`, `$3F0C`, `$3F10`, `$3F14`, `$3F18`, and `$3F1C` read as the backdrop. A sprite pixel of 0 is transparent and does not use its palette entry.

Mask bit 0 forces a color onto the gray column by clearing its low four bits. Mask bits 7–5 emphasize blue, green, and red.

### Control and mask

Register 5 is the control byte in the low 8 bits. The high byte reads as zero.

| Bits | Meaning |
|---|---|
| 1–0 | base nametable |
| 2 | unused |
| 3 | sprite pattern table. 1 selects `origin+$1000` |
| 4 | background pattern table. 1 selects `origin+$1000` |
| 5 | sprite size. 0 is 8×8. 1 is 8×16 |
| 6 | unused |
| 7 | raise the VBL interrupt when status bit 7 becomes set |

Register 6 is the mask byte.

| Bits | Meaning |
|---|---|
| 0 | grayscale |
| 1 | draw background in columns 0–7 |
| 2 | draw sprites in columns 0–7 |
| 3 | draw background |
| 4 | draw sprites |
| 5 | emphasize red |
| 6 | emphasize green |
| 7 | emphasize blue |

With bit 1 clear, background pixels in columns 0–7 are backdrop. With bit 2 clear, sprite pixels in those columns are skipped. With bit 3 clear there is no background and sprite 0 cannot hit. With bit 4 clear there are no sprites.

Register 7 reads the status byte. A write does nothing.

| Bits | Meaning |
|---|---|
| 7 | vblank latch |
| 6 | sprite 0 hit |
| 5 | sprite overflow, more than eight sprites on a scan line |
| 4–0 | zero |

Bits 6 and 5 clear at the start of the picture. Bit 5 is that count. It is not the 2C02’s sprite-evaluation quirk.

### Sprites

OAM is 64 entries of four bytes, the NES order.

| Byte | Meaning |
|---|---|
| 0 | Y. The sprite’s top line is this value plus one. `$FF` hides it. |
| 1 | tile index |
| 2 | attributes |
| 3 | X |

Attribute bit 5 draws the sprite behind opaque background. Bit 6 flips it horizontally. Bit 7 flips it vertically. Bits 1–0 pick the sprite palette. An 8×16 sprite takes its pattern table from bit 0 of the tile index, and the following tile is the bottom half. Control bit 3 does not apply to 8×16 sprites.

The scanner walks OAM from entry 0. The first eight sprites that intersect the scan line are drawn, and an earlier entry wins a tie. A ninth sets the overflow flag and is not drawn.

Sprite 0 hit sets status bit 6 when a non-transparent pixel of sprite 0 overlaps a non-transparent background pixel. Both must be enabled. Column 255 does not hit.

## QuickDraw

`SetScreenMode` writes depth, width, height, and the page, and leaves the scanner at 0. It rewrites the screen `LocInfo` and posts the MessageCenter message. The Window Manager, Menu Manager, Finder, and the mouse clamp take the new bounds from that message.

| `LocInfo` field | Depth 0 | Enhanced |
|---|---|---|
| image pointer | `$E12000` | the page origin |
| byte width | 160 | the mode’s stride |
| bounds | `0, 0, 200, 320` or `640` | `0, 0, height, width` |

`XSetUp320` and `XSetUp640` still pack 4-bit and 2-bit pixels. An 8-bit line is a byte address. A 16-bit line is a word address. SCB bit 7 still means 320 versus 640, and only when the depth is 0.

These are the places that still name the old screen and have to follow the `LocInfo` instead.

| What | Where |
|---|---|
| `NumScanLines`, `ScreenBase`, `FirstSCB`, `FirstTable` | `src/GSToolbox/QD/QDEquates/qd.data.asm` |
| Screen reserve, shadow, linearization, `Port640` | `src/GSToolbox/QD/init.asm` |
| `ClearScreen`, `SetSCB`, `SetAllSCBs` | `src/GSToolbox/QD/env.asm` |
| Cursor clamp at 200 | `src/GSToolbox/QD/cursor.asm` |
| `ScreenRect` | `src/GSToolbox/WindMgr/windmgr.asm` |
| Private screen record and the menu clamp | `src/GSToolbox/MenuMgr/menumgr.asm` |
| Last scan line | `src/GSToolbox/QDAux/qdaux.asm` |
| Finder `ScreenRect` | `src/A.U.G/Finder/data.aii` |

For an enhanced 2-bit or 4-bit mode, `FirstSCB` is `$DF0000` and `FirstTable` is `$DF0200`. `ScreenBase` is the page origin.

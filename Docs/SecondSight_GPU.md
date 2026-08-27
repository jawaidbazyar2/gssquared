# Second Sight GPU Mode

**Status:** draft v0.1 (2026-08-26)  
**Card mode:** SetMode emulation flag `$03`  
**Depends on:** [SecondSight.md](SecondSight.md) (classic VGA API, handshake, slot I/O)

This is a *new* high-level drawing architecture that reuses the Second Sight
command port, handshake, and VGA *resolution numbers*. It does **not** reuse
the Oak/VGA register model, CRTC programming, or a programmer-visible linear
framebuffer.

GPU mode is the drawing backend implied by [AppleIIx.md](AppleIIx.md): the 1 MHz
Apple II bus is too slow to push pixels, so the host submits resources and a
command sequence; the card (or emulator) executes them at GPU speed.

Original 1995 Second Sight silicon is **not** expected to implement this. The
point of the fence is that we should have exposed this level of abstraction in
1995 instead of teaching every application to program VGA modelines.

---



## 1. Why a distinct mode

Classic Second Sight VGA mode (`SetMode` flag `$01`) is a *display controller*:

- software picks a mode number **or** pokes Sequencer / CRTC / GDC / Attribute /
Oak extended registers (`SetVGAReg`, `SetUserMode`);
- software uploads a linear framebuffer and (sometimes) a palette;
- the card scans that buffer out.

That model is what Cogito and the existing emulator speak. It stays frozen.

GPU mode (`SetMode` flag `$03`) is a *drawing processor*:

- software picks a resolution by the same **mode number** (size + native depth);
- software uploads **textures** (and later fonts, patterns, meshes);
- software transmits a **command sequence buffer** (CSB) of primitives;
- the card maintains off-screen surfaces and a presentable display;
- `Present` makes a completed image visible.

The host never computes a pixel address, never programs a modeline, and never
owns the scanout buffer.

```
  1995 VGA path                         GPU path
  ─────────────                         ────────
  SetMode(num, VGA)                     SetMode(num, GPU)
  SetVGAReg / SetUserMode               (illegal)
  UploadData → linear VRAM              UploadTexture → handle
  CPU writes pixels                     ExecCmdBuf(Clear, Draw*, Present)
  CRTC scans VRAM                       card composites, then presents
```

PPU mode (`$02`) stays a third, NES-style path. GPU does not replace it.

---



## 2. GPU vs QuickDraw: one ISA, two client policies

QuickDraw II desktop apps and action games want different *update models*, not
necessarily different hardware:


|              | **Frame renderer** (games)         | **Retained framebuffer** (desktop / QD)   |
| ------------ | ---------------------------------- | ----------------------------------------- |
| Typical loop | Clear → draw whole scene → Present | Draw dirty rects → Present                |
| Backbuffer   | Discarded each frame               | Survives across submits                   |
| Tear control | Double-buffer swap at VBL          | Same Present, but no mandatory Clear      |
| CPU work     | Build a small CSB of world state   | Translate QD calls to CSB opcodes         |
| Good for     | Sprites, scrolling worlds          | Windows, menus, text, incremental updates |


**v0.1 decision:** one card mode, one command ISA, two present policies
(§8.3). QuickDraw primitives (`FrameRect`, `PaintRect`, `FillRect`,
`InvertRect`, …) are opcodes in that ISA. They are not a second `SetMode` flag.

A future GS/OS QuickDraw patch is a *translator* (toolbox call → CSB), not a
second GPU. If desktop compositing later needs window surfaces, vsync cursor,
or a display server, that is software on top of render-to-texture + Present —
still this ISA.

Revisit splitting “QD mode” only if retained-framebuffer scheduling or
compatibility with classic IIgs `GrafPort` semantics fights the game path
badly. Until then, do not fork the card.

---



## 3. The VGA fence

While `ss_mode == GPU`:

**Allowed classic commands**


| Cmd   | Name      | Role in GPU mode                                |
| ----- | --------- | ----------------------------------------------- |
| `$00` | GetStatus | Detection (unchanged 12-byte record)            |
| `$01` | SetMode   | Enter/leave GPU; pick resolution by mode number |
| `$04` | ScreenOff | Blank output                                    |
| `$05` | ScreenOn  | Enable output                                   |


**GPU host commands** (start at `$40`; not part of the 1995 set)


| Cmd   | Name            | Role                                       |
| ----- | --------------- | ------------------------------------------ |
| `$40` | UploadTexture   | §7.2                                       |
| `$41` | FreeTexture     | §7.3                                       |
| `$42` | ExecCmdBuf      | §7.4                                       |
| `$43` | GetGpuInfo      | §7.1 (current GPU state, not capabilities) |
| TBD   | GetCapabilities | §7.5 (feature stream; command byte TBD)    |


**Fenced (must return handshake** `$A6` **if issued in GPU mode)**


| Cmd           | Name                         | Why                                          |
| ------------- | ---------------------------- | -------------------------------------------- |
| `$02`         | UploadData                   | Linear VRAM is not a GPU resource            |
| `$03`         | ScrollScreen                 | No CRTC start address                        |
| `$06` / `$07` | SetPalette / SetPaletteEntry | VGA DAC; use CSB `SetPalette`                |
| `$08`         | SetBorder                    | No overscan modeline                         |
| `$0A`         | ClearScreen                  | Address+length fill of VRAM; use CSB `Clear` |
| `$0C` / `$0D` | SetVGAReg / GetVGAReg        | Modeline / Oak programming                   |
| `$0E`         | SetUserMode                  | 84-byte VGA register block                   |


Leaving GPU mode (`SetMode` with flag `$00` or `$01`) drops all GPU handles
and display surfaces. Classic VGA state is restored to a defined idle
(Apple II emulation or last VGA mode — implementer choice, must be documented
when implemented).

GPU mode **ignores** CRTC registers even if old software pokes them. Scanout
size comes only from the SetMode mode number.

---



## 4. Display modes (resolution numbers)

GPU mode accepts the **graphics** entries of the existing Second Sight mode
table (`vga_modes[]` in `secondsight.hpp`). The number selects *width,
height, and native color depth only*. Timing, pitch, planar layout, and
Oak bits are not part of the contract.


| Mode  | Size    | Native depth    | Notes                                   |
| ----- | ------- | --------------- | --------------------------------------- |
| `$13` | 320×200 | 8 bpp indexed   | Classic VGA 13h geometry                |
| `$53` | 640×480 | 8 bpp indexed   |                                         |
| `$5C` | 640×480 | 16 bpp (RGB555) | 15 color bits, matching SS 16-bit modes |
| `$5F` | 640×480 | 24 bpp (RGB888) |                                         |
| `$61` | 640×400 | 8 bpp indexed   | AppleColor-sized                        |


Text modes `$01` / `$03` and the internal “emulated Apple” IDs `$FA`–`$FE`
are **invalid** in GPU mode (`$A6`).

Additional sizes (800×600, 1024×768, …) may be added later as new numbers.
They still must not expose modelines — add a row to this table, nothing else.

**Native depth** is the display surface format. Textures may use a different
format; the card converts on blit. Indexed modes have a 256-entry GPU palette
(24-bit colors), independent of the VGA DAC.

Whether the card supports GPU mode at all is a `**GetCapabilities**` question
(§7.5). `GetGpuInfo` reports heap size and current mode so software can see
if a given resolution fits (two display buffers + working set). `SetMode`
returns `$A6` if the mode is unknown or the display surfaces cannot be
allocated.

Coordinate space: origin **top-left**, **+Y down**, pixel units, signed 16-bit
coordinates (QuickDraw II `Rect` convention). A `Rect` is
`{top, left, bottom, right}` with **exclusive** `bottom` and `right`.

---



## 5. Resources and handles

Everything the GPU retains across CSB submissions is a **resource** identified
by a 16-bit **handle**.


| Handle          | Meaning                                                                                                          |
| --------------- | ---------------------------------------------------------------------------------------------------------------- |
| `$0000`         | Display target (backbuffer of the swap chain, or the retained FB — §8.3). **Never** returned by `UploadTexture`. |
| `$0001`–`$FFFE` | Allocated resources (textures in v0.1)                                                                           |
| `$FFFF`         | Invalid; error return                                                                                            |


v0.1 resource type: **texture** (2D pixel rectangle). Later types (command
buffers, fonts, index/vertex buffers) share this handle namespace; `FreeTexture`
becomes a generic `FreeResource` or keeps working for textures only.

**Texture upload parameters**


| Field  | Type | Meaning                                        |
| ------ | ---- | ---------------------------------------------- |
| width  | u16  | Pixels, ≥ 1                                    |
| height | u16  | Pixels, ≥ 1                                    |
| format | u8   | See below                                      |
| flags  | u8   | bit 0: renderable (may be a `SetRenderTarget`) |


**Formats (v0.1)**


| Value | Name       | Bytes/pixel | Layout (row-major, tightly packed, top-left origin) |
| ----- | ---------- | ----------- | --------------------------------------------------- |
| `$01` | `IDX8`     | 1           | Palette index                                       |
| `$02` | `RGB555`   | 2           | LE: `0rrrrrgg gggbbbbb` (bit 15 = 0)                |
| `$03` | `RGB888`   | 3           | `R, G, B`                                           |
| `$04` | `ARGB8888` | 4           | `B, G, R, A` in memory (LE 32-bit `AARRGGBB`)       |


Pixel byte length is always `width * height * bpp`. No row padding in v0.1.

A renderable texture is a valid `SetRenderTarget`. The display (`handle 0`)
is always renderable. Drawing to a texture, then `DrawTexture` of that handle
to the display, is the off-screen / GWorld pattern.

**GPU RAM (not the 512K VGA window)**

Classic SS VGA memory is 512K. GPU mode has a **separate heap**. `GetGpuInfo`
returns the current size and free space. Presence of GPU itself is advertised
by `GetCapabilities` (§7.5). Suggested profiles:


| Profile              | Heap     | Who                                                            |
| -------------------- | -------- | -------------------------------------------------------------- |
| Full                 | ≥ 16 MiB | GSSquared emulator, future IIx / modern cards                  |
| Minimum to claim GPU | ≥ 4 MiB  | Enough for 640×480×16 double-buffer + several 128×128 textures |


Display surfaces (front + back, or one retained FB) are allocated from this
heap at `SetMode` and are **not** handles the guest frees.

---



## 6. Host I/O (unchanged transport)

Slot 3 device-select: command `$C0B0`, data `$C0B1`/`$C0B2`, handshake `$C0B8`
(see [SecondSight.md](SecondSight.md) § Handshaking).

GPU host commands use the existing patterns:


| Pattern      | Handshake                        | Used by                                           |
| ------------ | -------------------------------- | ------------------------------------------------- |
| DMA          | `$01` ready → bytes → `$00` done | args, pixels, CSB body, info records              |
| Long-running | `$A5` success / `$A6` error      | `SetMode`, `ExecCmdBuf`, `FreeTexture` after work |


Only one host command is in flight. The CSB interpreter runs to completion
(or error) before `$A5`/`$A6`.

---



## 7. Host API commands

GPU host commands start at `$40`. `$00`–`$0F` remain the 1995 set. Host
command bytes and CSB opcodes (§8) are separate namespaces.

### 7.1 GetGpuInfo (`$43`)

Immediate DMA-out after the command byte (same shape as GetStatus). This
record is **current GPU state** (heap, live mode). It is not a capability
cookie and has no signature. Card features (GPU present, Swap/Retain, …)
come from `GetCapabilities` (§7.5).


| Offset | Size | Content                                            |
| ------ | ---- | -------------------------------------------------- |
| `$00`  | 1    | CSB ISA version (`1` for this document)            |
| `$01`  | 4    | GPU heap size in bytes (u32 LE)                    |
| `$05`  | 4    | GPU heap free (u32 LE)                             |
| `$09`  | 2    | Maximum textures                                   |
| `$0B`  | 2    | Maximum CSB bytes per `ExecCmdBuf`                 |
| `$0D`  | 2    | Native width of current GPU mode (0 if not in GPU) |
| `$0F`  | 2    | Native height                                      |
| `$11`  | 1    | Native format (`$01`–`$04`)                        |
| `$12`  | 1    | Current present policy (§8.3)                      |
| `$13`  | 1    | Active flag: 1 if currently in GPU mode            |


Handshake: DMA `$01`/`$00` (not long-running). Length = `$14` bytes.

If the card is not in GPU mode, width/height/format are 0, `active` is 0, and
heap fields still report the GPU heap (or 0 if the card has no GPU). Do not
use a zero heap as “no GPU” — ask `GetCapabilities`.

### 7.2 UploadTexture (`$40`)

1. Command `$40`.
2. DMA in header (8 bytes):

  | Offset | Size | Field          |
  | ------ | ---- | -------------- |
  | `$00`  | 2    | width          |
  | `$02`  | 2    | height         |
  | `$04`  | 1    | format         |
  | `$05`  | 1    | flags          |
  | `$06`  | 2    | reserved (`0`) |

3. DMA in `width * height * bpp` pixel bytes.
4. DMA out 2-byte handle (LE). `$FFFF` on failure.
5. Long-running `$A5` / `$A6`.

Failure cases: not in GPU mode, bad format, zero size, heap exhaustion,
too many textures.

Uploaded data may be discarded from Apple memory after `$A5`; the card owns
its copy.

### 7.3 FreeTexture (`$41`)

1. Command `$41`.
2. DMA in 2-byte handle.
3. `$A5` if freed; `$A6` if handle is `$0000`, `$FFFF`, unknown, or the
  texture is the current render target.

Freeing does not implicit-Present. Draw commands in a later CSB that cite a
freed handle error the whole buffer (§9).

### 7.4 ExecCmdBuf (`$42`) — transmit and execute

1. Command `$42`.
2. DMA in header (4 bytes):

  | Offset | Size | Field                                                   |
  | ------ | ---- | ------------------------------------------------------- |
  | `$00`  | 3    | length (u24 LE), CSB bytes that follow                  |
  | `$03`  | 1    | flags: bit 0 = abort on first error (must be 1 in v0.1) |

3. DMA in `length` bytes — the command sequence buffer.
4. Card executes the CSB in order.
5. `$A5` if every opcode succeeded; `$A6` if not (no partial-commit
  guarantee in v0.1: a failed CSB may have already mutated the current
   target; see §9).

v0.1 max `length` is 65536. `GetGpuInfo` reports the cap.

The CSB is **not** retained. To replay, the host transmits it again. A later
revision may add `UploadCmdBuf` → handle for static scenes.

### 7.5 GetCapabilities (TBD)

Command byte, record layout, and capability IDs are **to be determined**
and are out of scope for this document.

Software that needs to know what the card can do — including whether GPU
mode exists, which present policies (`Swap` / `Retain`) are implemented,
maximum advertised resolution, and later 3D / QuickDraw extras — issues
`GetCapabilities` and reads a stream of capability records. That replaces
any magic signature on `GetGpuInfo` or extra bytes on classic `GetStatus`
(Cogito keeps the 12-byte `$00` record).

Detection sketch (library, not card ISA):

1. `GetStatus` — `'G','S','V','G','A'` cookie; firmware version high enough
  that `GetCapabilities` exists.
2. `GetCapabilities` — look for a GPU capability in the stream.
3. `SetMode(..., $03)` / `GetGpuInfo` — enter GPU and read live heap/mode.

Until `GetCapabilities` is specified, GSSquared GPU mode may be assumed
present in emulator builds that compile it in; guest software should still
call through this sequence so it ports to a real capability record later.

---



## 8. Command sequence buffer (CSB)

A CSB is a packed byte stream, little-endian, 8-bit opcodes, no padding.
Execution is serial. After `End` (or end of buffer), the interpreter stops.
These opcodes are **not** host command bytes written to `$C0B0` (those start
at `$40`; see §7). A CSB `$40` would be a 3D opcode, not `UploadTexture`.

### 8.1 Opcode map (ISA v1)


| Op          | Name               | Args                                         | v0.1                                |
| ----------- | ------------------ | -------------------------------------------- | ----------------------------------- |
| `$00`       | `End`              | —                                            | required                            |
| `$01`       | `Clear`            | color:u32                                    | required                            |
| `$02`       | `Present`          | flags:u8                                     | required                            |
| `$03`       | `SetRenderTarget`  | handle:u16                                   | required                            |
| `$04`       | `DrawTexture`      | handle:u16, x:i16, y:i16                     | required                            |
| `$05`       | `DrawTextureRect`  | handle:u16, src:Rect, dst:Rect               | required                            |
| `$06`       | `DrawTextureTiled` | handle:u16, dst:Rect                         | required                            |
| `$10`       | `SetClip`          | rect:Rect                                    | required                            |
| `$11`       | `SetColor`         | color:u32                                    | required                            |
| `$12`       | `SetPattern`       | 8 bytes (1-bit 8×8)                          | required                            |
| `$13`       | `SetDrawMode`      | mode:u8                                      | required                            |
| `$14`       | `SetPalette`       | start:u8, count:u8, then `count×3` RGB bytes | 8 bpp modes                         |
| `$15`       | `SetPresentPolicy` | policy:u8                                    | required                            |
| `$20`       | `FrameRect`        | rect:Rect                                    | QD, required                        |
| `$21`       | `PaintRect`        | rect:Rect                                    | QD, required                        |
| `$22`       | `FillRect`         | rect:Rect                                    | QD, required                        |
| `$23`       | `InvertRect`       | rect:Rect                                    | QD, required                        |
| `$30`–`$3F` | *more QD*          |                                              | reserved (Line, Oval, RoundRect, …) |
| `$40`–`$5F` | *3D*               |                                              | reserved                            |
| `$E0`–`$EF` | *vendor / debug*   |                                              | reserved                            |
| `$FF`       | `End`              | alias of `$00`                               | optional                            |


`Rect` = four i16: `top, left, bottom, right` (8 bytes). Empty or inverted
rects (bottom ≤ top or right ≤ left) are no-ops, not errors.

`color:u32` is **always** `AARRGGBB` in the CSB (A=`$FF` = opaque). The card
converts to the target’s native format. In `IDX8` targets, the implementation
maps RGB to the nearest palette entry unless `SetDrawMode` bit 7 is set, in
which case the **low 8 bits** of the color are used as an index (alpha
ignored).

### 8.2 Interpreter state (reset at each ExecCmdBuf)


| State          | Reset value                                                            |
| -------------- | ---------------------------------------------------------------------- |
| Render target  | handle `$0000` (display)                                               |
| Clip           | full current target                                                    |
| Color          | opaque black `$FF000000`                                               |
| Pattern        | 8×8 all-ones (solid, so `FillRect` ≡ `PaintRect` until set)            |
| Draw mode      | `Copy` (`$00`)                                                         |
| Palette        | unchanged across CSBs (owned by the GPU mode, not the CSB)             |
| Present policy | unchanged across CSBs (set by `SetPresentPolicy` or `SetMode` default) |


Resetting clip/color/mode per submit keeps a CSB self-contained — important
when games and a QD translator share the card. Palette and present policy are
session state because they are expensive / semantic.

### 8.3 Present, targets, policies

`SetRenderTarget handle`

- `$0000` — display (the buffer that `Present` will show).
- Other — a renderable texture. Size of the target becomes the clip default
until the next `SetClip`.

Drawing is always into the **current** target. It is not visible on the
monitor until `Present`, except as noted for retain policy.

`Present` flags


| Bit | Meaning                                                                                             |
| --- | --------------------------------------------------------------------------------------------------- |
| 0   | Wait for display VBL before the new image is scanned out (should be 1 for the GS/OS animation test) |
| 1–7 | Reserved, 0                                                                                         |


**Present policy** (`SetPresentPolicy`, default `Swap` on `SetMode` GPU):


| Value | Name     | Behavior                                                                                                                                             |
| ----- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `$00` | `Swap`   | Two display buffers. `Present` queues a swap at VBL. The next CSB draws into the **other** buffer. Games should `Clear` (or fully cover) each frame. |
| `$01` | `Retain` | One display buffer. Draws accumulate. `Present` waits VBL (if flag 0) but does not discard contents. Desktop / QD.                                   |


`Clear` fills the **current target** (clipped) with `color`. It does not
Present.

### 8.4 Texture primitives

`DrawTexture handle, x, y`  
Blit the full texture to `(x,y)` on the current target, 1:1 pixels. Pixels
outside the clip / target are discarded. Negative `x`/`y` are legal (partially
off-screen).

`DrawTextureRect handle, src, dst`  
Blit `src` in texture space to `dst` in target space. If `dst` size ≠ `src`
size, the card scales (nearest-neighbor in v0.1). Empty src/dst → no-op.

`DrawTextureTiled handle, dst`  
Replicate the full texture over `dst` (modulo wrap). This is the “stamp a
texture across the display” primitive.

v0.1 blending: **copy** all pixels (including 0). Color-key and alpha come
later (`SetColorKey`, `ARGB8888` source with `SrcOver`). Indexed `$00` is
**not** automatically transparent — that is PPU-mode semantics.

### 8.5 QuickDraw II rect primitives (v0.1)

These follow IIgs QuickDraw II *shapes*, not a full `GrafPort`. Pen size is
**1×1** until a later `SetPenSize`. Pattern is the CSB 8×8 1-bit pattern
with **foreground = current color** and **background = transparent** for
`FillRect`? No: QD `FillRect` uses the pattern with both bits as colors.

v0.1 pattern model (simplest useful QD subset):

- `SetPattern`: 8 bytes, row 0 … row 7, MSB = leftmost pixel.
- `PaintRect`: fill with current **color** (ignore pattern), `Copy` or current
draw mode.
- `FillRect`: for each pixel, pattern bit 1 → current color, bit 0 → skip
(leave target pixel). That is “stencil with color,” good enough for desktop
dither once a second color exists.
- `FrameRect`: 1-pixel outline on the four edges of `rect` (inside the
exclusive bounds: top row, bottom-1 row, left col, right-1 col).
- `InvertRect`: bitwise XOR of each native pixel with all-ones (1s complement
of the stored pixel). Independent of color/pattern.

`SetDrawMode`


| Value | Name                      | Apply to                       |
| ----- | ------------------------- | ------------------------------ |
| `$00` | `Copy`                    | Replace dest with source/color |
| `$02` | `Xor`                     | Dest XOR source (QD `modeXOR`) |
| `$80` | `Copy` + indexed-as-index | See color rule in §8.1         |


`OR` / `BIC` / not-variants are a later QD increment. `InvertRect` is the
v0.1 inversion path (QD `InvertRect`).

Iterate here: round rect, oval, line, region, text, color pattern (32-byte
IIgs `Pattern`), `pnSize`, transfer modes 0–15.

---



## 9. Errors


| Situation                              | Host handshake              | CSB  |
| -------------------------------------- | --------------------------- | ---- |
| Unknown host command                   | ignore / no `$A5` (classic) | —    |
| Host GPU cmd while not in GPU          | `$A6`                       | —    |
| Fenced VGA cmd while GPU               | `$A6`                       | —    |
| Upload OOM / bad args                  | `$A6`, handle `$FFFF`       | —    |
| Unknown CSB opcode                     | `$A6`                       | stop |
| Invalid handle in CSB                  | `$A6`                       | stop |
| `SetRenderTarget` to non-renderable    | `$A6`                       | stop |
| CSB longer than cap / truncated opcode | `$A6`                       | stop |


v0.1 does **not** roll back the target on CSB error. Guest should `Clear` or
redraw after `$A6`. A later “transaction” flag can add rollback.

---



## 10. 3D (out of scope, reserved)

ISA `$40`–`$5F` is reserved for a later 3D increment (vertex upload, triangle,
depth, transform). That is **CSB opcode** space, not host `$C0B0` commands.
v0.1 implementations must `$A6` if they ever see these.
No vertex formats or matrices in this document.

---



## 11. Entering GPU mode

```
SetMode(mode_number, $03)
```

`mode_number` is a row from §4. Sequence:

1. If already GPU and same mode: `$A5`, no-op (handles preserved).
2. If already GPU and different mode: drop all handles, realloc display
  surfaces, `$A5` or `$A6`.
3. If coming from VGA/PPU/emu: fence VGA state, alloc GPU heap surfaces,
  default policy `Swap`, identity palette (index i → gray or VGA-like ramp —
   document in implementation), `$A5`.

`GetStatus` byte `$08` (`vga_active`) is **1** in GPU mode so existing “not
Apple video” checks still treat the card as owning the monitor. Byte `$09`
(`vga_mode_num`) is the GPU mode number. Do not put `$03` (the *flag*) in
`$09`.

---



## 12. Initial test: GS/OS texture bounce

Goal: prove the 1 MHz bus is only used for **handles and a tiny CSB**, while
the card moves more pixels per frame than a IIgs could blit.

### 12.1 Setup

- Apple IIgs, Second Sight in slot 3, GS/OS.
- S16 (or EXE) with a small SS library: GetStatus, GetCapabilities,
GetGpuInfo, SetMode, UploadTexture, ExecCmdBuf, FreeTexture, ScreenOn.



### 12.2 Procedure

1. `GetStatus` — `'G','S','V','G','A'` present. Firmware version after the
  cookie is high enough that `GetCapabilities` exists.
2. `GetCapabilities` — GPU capability present (record format TBD, §7.5).
3. `GetGpuInfo` — heap size (and later, that the chosen mode fits).
4. `SetMode($5C, $03)` — 640×480, RGB555, GPU. `$A5`.
5. Build three CPU-side images, **good-sized** (if heap allows, **128x128**
  `RGB555` each = 32 KiB). Distinct art (solid + checker
  - gradient is enough).
6. `UploadTexture` × 3 → handles `A, B, C`.
7. Each frame, build a CSB (~40–80 bytes):
  ```
   Clear       $FF202040
   DrawTexture A, x0, y0
   DrawTexture B, x1, y1
   DrawTexture C, x2, y2
   Present     (VBL wait)
   End
  ```
   Positions follow independent velocities; bounce on the 640×480 edges
   (account for texture size). Optional: `DrawTextureRect` with a changing
   `dst` size for a cheap “scale” stress.
8. `ExecCmdBuf` every frame. The 65816 must **not** write the 640×480
  framebuffer.
9. On quit: `FreeTexture` × 3, `SetMode` back to Apple emulation (`$00`).



### 12.3 Pass criteria

- Three textures visible, moving **every display frame** (60 Hz with VBL
Present) without tearing.
- Host per-frame traffic is the CSB plus handshake, not `width*height` pixels.
- Motion remains smooth at CPU speeds where a naive 65816 blit of one 256×256
16-bit image per frame would fail.

This is a **frame renderer** (`Swap` + `Clear`). A second test (later) should
exercise `Retain` + `FrameRect` / `PaintRect` / `FillRect` / `InvertRect`
without clearing.

---



## 13. Worked CSB example

Handles: `$0001` texture A. Bounce position (40, 80). Clear dark blue,
present with VBL.

```
00: 01                      Clear
01: 40 20 20 FF             color AARRGGBB = FF202040 (LE bytes 40 20 20 FF)
05: 04                      DrawTexture
06: 01 00                   handle 0001
08: 28 00                   x = 40
0A: 50 00                   y = 80
0C: 02                      Present
0D: 01                      flags = VBL
0E: 00                      End
```

14 bytes. The 256×256 blit is entirely on-card.

---



## 14. Reference implementation: GSSquared

Not part of the guest contract. Other cards (or a future IIx GPU) may execute
the same ISA with a real rasterizer. In GSSquared, **true GPU mode** (the
game path: textures, clear, blit, present) is a thin translator from CSB
opcodes onto **SDL3** `SDL_Renderer`. The emulator does not reimplement a
software GPU for that path, and it does not drop to the low-level `SDL_GPU`
API (Metal/Vulkan-style command buffers) for v0.1 2D.

GSSquared already has one window `SDL_Renderer`, off-screen `TARGET`
textures, and an optional CRT post-process that uses `SDL_GPU` *after* the
frame is composed. GPU mode is another guest-sized `TARGET` texture that the
existing Second Sight frame handler feeds to `video_system_t::render_frame()`,
same as VGA and PPU.

### 14.1 CSB → `SDL_Renderer`


| CSB / host op                     | SDL3                                                                      |
| --------------------------------- | ------------------------------------------------------------------------- |
| `UploadTexture`                   | `SDL_CreateTexture` + `SDL_UpdateTexture`                                 |
| `FreeTexture`                     | `SDL_DestroyTexture`                                                      |
| `SetRenderTarget`                 | `SDL_SetRenderTarget` (GPU-owned texture only)                            |
| `Clear`                           | `SDL_RenderClear` / `SDL_RenderFillRect`                                  |
| `DrawTexture` / `DrawTextureRect` | `SDL_RenderTexture`                                                       |
| `DrawTextureTiled`                | `SDL_RenderTextureTiled`                                                  |
| `SetClip`                         | `SDL_SetRenderClipRect`                                                   |
| `SetColor`                        | `SDL_SetRenderDrawColor`                                                  |
| `PaintRect` / `FrameRect`         | `SDL_RenderFillRect` / `SDL_RenderRect`                                   |
| `Present`                         | Flip which guest buffer is **front**; do **not** call `SDL_RenderPresent` |


`SDL_GPU` stays the CRT shader path. Guest CSBs are 2D blit lists; wrapping
them in GPU render passes and pipelines buys nothing visible to the IIgs.
A later 3D increment (§10) is the point at which a second backend on
`SDL_GPU` would make sense.

### 14.2 Present, threading, and renderer state

`Present` in the ISA means “this guest surface is ready,” not “flip the host
window.” Window present, host vsync, bezel, OSD, and CRT remain in
`video_system`. `ExecCmdBuf` runs on the 65816 I/O timeslice; the frame
handler runs later.

Rules:

- Draw only into GPU-owned `SDL_TEXTUREACCESS_TARGET` textures (display
backbuffers and renderable guest textures). Never set the render target to
`nullptr` (the window) from the CSB interpreter.
- Save and restore renderer target, clip, viewport, and draw color around
CSB execute so OSD / ImGui / the frame handler are not disturbed.
- Guest VBL wait (Present flag bit 0) is the emulator frame turn (same idea
as PPU WaitVBL), **not** SDL vsync. Do not busy-wait inside a `$C0B8` poll
for “GPU time.”
- `Swap` is two guest `TARGET` textures and a pointer flip. `Retain` is one.



### 14.3 Formats

The SDL renderer is effectively RGBA. Convert on upload (and on palette
change for `IDX8`):

- `RGB555` / `RGB888` / `ARGB8888` → renderer-native once at `UploadTexture`.
- `IDX8` → RGBA at upload and whenever `SetPalette` runs.

The guest still sees handles and the native *mode* depth from §4. It never
sees SDL’s pixel format.

### 14.4 QuickDraw is a different backend

`InvertRect`, 1-bit `FillRect`, XOR, pen sizes, and regions are
framebuffer raster ops. `SDL_Renderer` is a compositor. Forcing those through
blend-mode tricks will get the wrong `InvertRect` and a pile of special cases.


| Path                | GSSquared backend                                                                                  |
| ------------------- | -------------------------------------------------------------------------------------------------- |
| True GPU / games    | CSB → `SDL_Renderer` (this section)                                                                |
| QuickDraw / desktop | CSB → CPU blit into a linear or `STREAMING` surface; Present copies it to the same display texture |


Same guest ISA. The emulator may dispatch per opcode family. Split QD into a
separate `SetMode` flag only if the two interpreters get in each other’s way
(see §2, §15.1). Do not block the bounce test (§12) on solving QD.

### 14.5 Wiring notes

- Do **not** share the classic 1 MiB linear `frame_buffer` interpreters
(`vga_render_8/16/24`). GPU surfaces are SDL textures on the GPU heap.
- `SetMode(..., $03)` must set `ss_mode = SS_MODE_GPU` and must **not** fall
through to the VGA `emu_flag != 0` path that only sets `SS_MODE_VGA`.
- Debug panel: heap used, handle table (SDL texture pointer per handle), last
CSB opcode, current target, which display texture is front.

---



## 15. Open questions (iterate here)

1. **QD as a separate** `SetMode` **flag** — deferred; see §2. Reopen if `Retain`
  vs `Swap` is not enough for a real QD patch.
2. **Color pattern** — IIgs 32-byte `Pattern` vs 1-bit `SetPattern` + color.
3. **Pen size / frame inset** — QD `FrameRect` uses `pnSize`; v0.1 is 1×1.
4. **Alpha / color key** — needed before this is a sprite engine.
5. **CSB as a resource** — upload once, exec by handle, patchable commands.
6. **Paravirtual Exec** — CSB pointer in IIgs RAM read directly by the
  emulator (skip byte-at-a-time `$C0B2`). Faster tests; not 1995-accurate.
   Could be a header flag on `$42`.
7. **800×600 / 1024×768** — add mode numbers when we care; still no modelines.
8. **GetStatus version** — left at 1.4 so Cogito is undisturbed. GPU
  presence and feature bits go through `GetCapabilities` (§7.5), not extra
   `GetStatus` bytes and not a `GetGpuInfo` signature.
9. **GetCapabilities record** — command byte, stream format, and capability
  IDs TBD.

---



## 16. Document history


| Ver | Date       | Notes                                                                                                                                                                                                 |
| --- | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0.1 | 2026-08-26 | Initial ISA, host cmds `$40`–`$43`; `GetGpuInfo` has no signature; capabilities via TBD `GetCapabilities`; VGA fence, QD rect subset, GS/OS bounce test; GSSquared CSB→SDL_Renderer reference backend |



# RGB2.0 frame buffer — implementation 1.0

Emulator plan for the linear frame buffer in [frame-buffer.md](frame-buffer.md). That document is the guest-visible programming model (registers, depths, strides, SCBs, palettes, bank crossings). This document is how GSSquared installs it.

RGB2.0 is an Apple IIgs slot 3 card. It has no slot ROM and no slot I/O window. It installs a 1MB store in FPI banks `$D0`–`$DF` and a register file at `$C069`–`$C06C`. On an A2FPGA or an AppleSqueezer the same window is motherboard logic; in GSSquared the slot is how the machine turns it on. Second Sight and the Video Overlay Card already occupy slot 3 the same way, with no firmware of their own, so the built-in 80-column ROM at `$C300` stays in place.

Version 1.0 is the linear scanner (scanner 0). PPU mode (scanner 1) and the GPU command path are later. Guest toolbox changes (`SetScreenMode`, `LocInfo`, Window Manager, Finder) are GS/OS work and are listed in frame-buffer.md; they are not part of this emulator slice.

## Card

| Item | Value |
|------|-------|
| Product name | RGB2.0 |
| `.gs2` / Settings.txt card id | `rgb2` |
| `device_id` | `DEVICE_ID_RGB2` |
| Editor label | RGB2.0 |
| Platforms | `PLATFLAG_ANY_IIGS` (ROM 01 and ROM 03) |
| Slot | 3 only (`slots_allowed = 0b00001000`) |
| Firmware | none. Do not call `set_slot_rom`. Do not write the slot register or `SLOTC3ROM`. |
| Slot I/O `$C0B0`–`$C0BF` | unused |

One card per slot is already enforced, so RGB2.0, Second Sight, and the Video Overlay Card exclude each other by occupying slot 3. No extra mutex.

```toml
platform = "apple2gs"
# or "apple2gs_rom3"

[[cards]]
slot = 3
card = "rgb2"
```

`gs2_version` stays 1. Wire the id through the same tables as `second_sight` and `voc`:

- `src/Device_ID.hpp`
- `src/device_info.cpp`
- `src/devices.cpp` (`init_rgb2`)
- `src/util/SystemConfig.cpp` (parse and emit)
- `src/util/SettingsConfig.cpp` (parse)
- `Docs/SystemConfigTOML.md`, `Docs/ConfigFiles.md`

The Edit System slot picker picks the card up from `device_info`. There is no new motherboard control and no new root key in the `.gs2`.

## Power-on order

`computer->mmu` on a IIgs is the Mega II (`MMU_IIe2`). The FPI is `computer->cpu->mmu`, an `MMU_IIgs`. Slot `power_on` runs after `MMU_IIgs::init_map()` and after motherboard devices, including the game controller.

RGB2.0 `power_on`:

1. Cast `computer->cpu->mmu` to `MMU_IIgs` and map the window (below).
2. Install `$C069`–`$C06C` read and write handlers on `computer->mmu` (the Mega II). `$E0C069` and a shadowed `$xxC069` both land there.
3. Register a reset handler and a shutdown handler. Reset clears the register file. Shutdown frees the store.
4. Register a weight-1 frame processor, the same hook the Video Overlay Card uses (below).

The processor returns false while the adopted depth behaves as classic, so the Mega II scanner draws today’s picture. An enhanced depth or a blanked display returns true and replaces that frame. `$C019` and `video_cycle()` keep running either way.

## FPI banks `$D0`–`$DF`

`MMU_IIgs` is one 64KB page per bank. The window is sixteen of those pages, 1,048,576 bytes, owned by the card.

```
bank $D0 → store + 0x00000
bank $D1 → store + 0x10000
…
bank $DF → store + 0xF0000
```

Map each bank with `map_page_both` onto that pointer and a tag such as `"RGB2"`. `map_page_both` clears the read and write handlers, so the CPU hits the pointer path. Leave `shadow_h` clear. There is no cache in the emulator; a plain store is the write-through window. These banks are not Mega II `$D000` and are not language-card RAM.

`init_map()` plants `float_area_read` on banks `$80`–`$DF` before the card runs. A read with no card returns the bank number; a write is discarded. The card’s `map_page_both` replaces that for `$D0`–`$DF` only. Banks `$80`–`$CF` stay float (or shadow-all) as they are today.

### `set_ram_shadow_banks` stops at `$CF`

`$C036` bit 4 (`SPEED_SHADOW_ALL`, `0x10`) and `reset()` both call `set_ram_shadow_banks()`. That function currently rewrites every bank from the end of fast RAM through `$DF`, then `$E2`–`$EF`. The next speed-register write would put `$D0`–`$DF` back on the float or shadow handler and the window would disappear. GS/OS writes `$C036` during boot.

Change both loops that currently run `ram_banks … $DF` so they stop at `$CF`:

```cpp
for (int i = (int)ram_banks; i <= 0xCF; i++)
```

One loop is the shadow-all path (`bank_shadow_read` / `bank_shadow_write`). The other restores plain float. The `$E2`–`$EF` loops stay. `init_map()`’s one-time float install on `$80`–`$DF` stays, so a machine with no RGB2.0 still floats `$D0`–`$DF`.

This bound is unconditional. The card maps once, in `power_on`. Reset and `$C036` must not map those banks again.

With the bit set, banks `$D0`–`$DF` no longer copy a shadowed display page into Mega II, and `$D0C000`–`$DFCFFF` are no longer Mega II I/O. They are either the frame buffer or ordinary float. `Docs/AppleIIgs-Memory.md` describes bit 4 as shadowing for fast RAM banks, which end at `$7F`. Banks `$80`–`$CF` and `$E2`–`$EF` keep today’s shadow-all behavior, including the unpopulated-bank copies covered by iigsmmutest tests 33 (`$82`) and 34 (`$EE`).

The pixel store must not go through `bank_shadow_write`. A store to `$D00400` is a pixel. `$D0C000` sits inside a 640×480×8 frame, so an I/O hole there would split scan lines.

## Register file

Guest behavior is [frame-buffer.md](frame-buffer.md) § `$C069`–`$C06C`. Four Mega II addresses:

| Address | Role |
|---------|------|
| `$C069` | index, bits 7–0 |
| `$C06A` | index, bits 15–8 |
| `$C06B` | value, bits 7–0 |
| `$C06C` | value, bits 15–8 |

A 65816 word store is two byte cycles, low byte then high byte. A byte write updates that half of the 16-bit latch. A write to `$C06B` or `$C06C` then commits the latched value to the selected register. A write to `$C069` or `$C06A` only changes the index. A read of `$C069`/`$C06A` returns the index. A read of `$C06B`/`$C06C` returns the selected register.

`$C068` stays `STATEREG`. `$C06D` stays untouched (the guest calls it `TESTREG`; GSSquared has no handler there). A word store at `$E0C068` or `$E0C06C` is a guest bug and must keep hitting those neighbors.

The game controller, a motherboard device, installs IIe-style read mirrors at `$C069`–`$C06F` (`GAME_SWITCH` / `GAME_ANALOG` plus 8) on every platform, and it runs before slot cards. RGB2.0 replaces the read handlers on `$C069`–`$C06C` and adds the write handlers. `$C061`–`$C067` and the `$C070` strobe stay. The paddle/button mirrors at `$C06D`–`$C06F` stay. These four handlers are not reinstalled on reset, so they survive `MMU_IIgs::reset()`.

Power-on and reset values:

| Number | Register | 1.0 |
|---:|---|---|
| 0 | Depth | writable, power-on 0. Scanner treats 2, 4, 8, 16 as enhanced. Any other stored value behaves as classic. Readback is the stored value. |
| 1 | Width | writable, power-on 0. Ignored when the adopted depth behaves as classic. |
| 2 | Height | writable, power-on 0. Same. |
| 3 | Start page | writable. Bits 15–12 read as zero; store the masked 12-bit value. |
| 4 | Scanner | writable, power-on 0. Capabilities bit 0 is clear, so a stored 1 behaves as 0. Any other stored value behaves as 0. |
| 5–10 | PPU control, mask, status, scroll, mirror | stored and read back. Ignored by the 1.0 scanner. Status bit 7 and the sprite flags stay 0. |
| 11 | Display | writable, power-on 0. 0 shows the picture. 1 blanks it. Any other stored value behaves as 0. |
| 12 | Capabilities | read-only 0. A write does not change it. Bit 0 clear means PPU mode is absent. |

## Adoption and blanking

The register file and the picture the scanner is drawing are separate copies. Depth, width, height, page, and scanner move into the adopted copy together.

- A write of 1 to register 11 blanks the picture, stops the scanner from reading the window, and drops a pending adoption. The adopted copy is left as it was. `$C019` and the rest of the IIgs beam keep running.
- A write of 0 arms adoption. At the next frame boundary the scanner copies those five registers into the adopted set and disarms. Edits made after that wait for another write of 0.
- While the picture is showing, further writes to depth, width, height, page, or scanner do not change the adopted copy until register 11 is written as 0 again.

Reset and power-on clear the file, clear any armed adoption, and adopt classic immediately (depth behaves as 0, display 0), so the machine comes up on today’s super hi-res.

Origin and span, from frame-buffer.md:

```
origin = $D00000 + (page << 8)
```

Page `>= $0F00` is bank `$DF` and is not a legal pixel origin. The scanner draws black for any pixel whose address is outside `$D00000`–`$DEFFFF`. It does not wrap into the table bank. SCB bytes stay at `$DF0000` and palette words at `$DF0200`, fixed, for the enhanced depths that use them. Classic depth ignores width, height, and page and keeps `$E12000` / `$E19D00` / `$E19E00`.

## Video

`VideoScannerIIgs` keeps the beam. RGB2.0 does not extend it. The card registers a weight-1 frame processor, the same pattern as the Video Overlay Card: `video_system_t::update_display` runs processors in weight order and stops at the first that returns true. Weight 0 is the Mega II scanner. RGB2.0 is weight 1, so it runs first.

When the adopted depth behaves as classic and the display register is not 1, the processor returns false. Presentation stays the current super hi-res / text / lo-res / double hi-res path. `$C029` bit 7 still selects super hi-res against the older modes.

When register 11 is 1, the processor presents black at the adopted size, does not read the store, clears the Mega II scan buffer, and returns true. That blanks classic video too. `$C019` still follows the beam because `video_cycle()` is unchanged.

When the adopted depth is 2, 4, 8, or 16, the processor builds one `width` by `height` frame from the store into an `SDL_PIXELFORMAT_XRGB1555` texture and presents it with `render_frame_vga`. The 15-bit color word is that format. Stride, SCB use, the 2-bit column map, and nibble order are the rules in frame-buffer.md. The store is one contiguous allocation, so a scan line that crosses a 64KB bank is ordinary pointer arithmetic. A pixel whose address is outside `$D00000`–`$DEFFFF` is black. Width and height are clamped to 1024×768, the texture size. Classic 12-bit `$0RGB` at `$E19E00` stays on the classic path.

The card holds the store and both register copies. With no RGB2.0 in the machine, nothing reads `$D0`–`$DF` for video.

## Tests

MMU, in `apps/iigsmmutest` or next to it:

- Tests 33 and 34 still pass (`$82` and `$EE` shadow-all copies).
- With shadow-all set, a write to `$D00400` does not appear at `$E00400`, and `$D0C000` does not answer as a softswitch.
- With no card, a read of `$D00000` after `init_map` and after a `$C036` write is the bank number `$D0`.

Card, with a IIgs config that has `card = "rgb2"` in slot 3:

- A store to `$D01234` reads back, including after a write to `$C036` with bit 4 set, and after reset.
- Banks `$E0`/`$E1` language-card `$D000` are unchanged.
- Word store to `$E0C069` / `$E0C06B` selects and writes a register; a word load reads it back. Start page bits 15–12 read as 0. Capabilities reads as 0 and ignores a write.
- A byte store to `$E0C068` still updates `STATEREG` only.
- Reset returns every writable register to 0 and the adopted picture to classic.
- No `$C3` ROM is installed; internal slot 3 firmware remains when `SLOTC3ROM` is clear.

Video, once the scanner presents enhanced frames: the modes tabulated in frame-buffer.md (at least 640×400×2, 640×480×8, and 320×200×8), a line that crosses a 64KB bank in 640×480×8, blank then adopt so depth and size change together, and blanking leaving `$C019` running.

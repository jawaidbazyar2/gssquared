# SS Console

Host-side console on Second Sight **GPU Text** (`SetMode` flag `$05`).
Card ISA: [Docs/SecondSight_GPUText.md](../Docs/SecondSight_GPUText.md).
IIe bring-up driver: `testdev/ss80`.

This file is the host contract: when to park, when to leave the mode, and
what the card is *not* asked to snapshot. The stream opcodes live in the
card spec; this document is policy.

---

## 13. Display yield (park / unpark)

A Classic Desk Accessory (or any other reason to show the Apple IIgs
desktop / Mega II text) is **not** a reason to leave GPU Text.

`SetMode` away ends the stream and discards FIFO + VRAM. `SetMode($05)`
again on the same raster is **not** a clear (host `Reset` / `Erase` if
it wants a known page). A card-side snapshot that survives `SetMode`
*away* is only useful if something must **actually** leave GPU Text. A
CDA is not that.

Yield keeps the card in GPU Text. The host drains the ring, sends one
word, and Mega II is on the glass. Claim is one word. Same page. No
replay.

### 13.1 Driver

```
park:
        wait $C0B8 = $01        ; ring empty
        STA  $9500              ; Yield arg 0

unpark:
        STA  $9501              ; Yield arg 1
```

No `SetMode`. No replay. VRAM, cursor, margins, current attr, and
`SetSync` stay as they were.

Park must drain first so `$9500` is last. Words after a park still
execute (they update VRAM while Mega II is showing). That is legal; it
is not the usual path.

### 13.2 Opcode `$95` (`Yield`)

| Arg | Name | Effect |
| --- | --- | --- |
| `$00` | park / yield | Stop owning the display. Mega II scanout. GPU Text stays armed. |
| `$01` | claim / unpark | Own the display again. Same VRAM page. |
| other | — | Ignore. |

`$92` `Reset` does not change yield. `ScreenOff` is not yield: black,
we still own VGA.

### 13.3 Why not `SetMode` / snapshot

| Approach | Cost | After CDA / dialog |
| --- | --- | --- |
| `SetMode` emu, then `$05` again | Lose VRAM + ring | Unspecified until host `Reset` / paints |
| Card snapshot that survives `SetMode` | Extra VRAM copy | Only pays if something *must* leave `$05` |
| `$95` yield | One word | Same page, no replay |

Use `SetMode` when you are done with GPU Text (quit, switch to GPU
pixels, Host Text, …). Use `$95` when you only need Mega II for a
while.

### 13.4 GSSquared

1. Execute `$95` in the GPU Text consumer (`SsGpuText`). Arg `$00`
   sets a yielded flag; arg `$01` clears it. Cells, cursor, margins,
   sync are untouched.
2. After the consumer runs (immediate drain on `C0B1`, or VBL drain),
   a hook copies that flag to `vga_active` (`0` = yielded, `1` = claim).
   It must **not** call `leave_gpu_text_if_needed()`.
3. `frame()`: if GPU Text is armed and `vga_active` is 0, still drain
   a VBL-batched ring (so `$9501` can take effect), then return
   **false**. The existing path already draws Mega II when
   `vga_active` is 0.
4. `ScreenOff` (`display_enabled` = 0) with `vga_active` still 1:
   return **true** and draw nothing — black, we still own VGA.

Testdev: paint a distinctive page → park → Mega II visible → claim →
same page, no stream replay.

# RGB2.0

RGB2.0 is an Apple IIgs slot-3 video card. It adds the linear frame buffer in [frame-buffer.md](frame-buffer.md): a 1MB store in banks `$D0`–`$DF`, and a register file at `$C069`–`$C06C`. It has no slot ROM, so the built-in 80-column firmware stays in place.

Second Sight and the Video Overlay Card also use slot 3 — pick one of the three.

Version 1.0 draws the linear scanner (2, 4, 8, and 16 bits per pixel). Depth 0 remains today’s super hi-res. PPU mode is not implemented; the capabilities register reads as 0, so selecting scanner 1 behaves as the linear frame buffer.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Choose an **Apple IIgs** platform (ROM 01 or ROM 03).
3. Click **slot 3** and pick **RGB2.0**.
4. Save and launch.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "rgb2"
```

## Related

- [frame-buffer.md](frame-buffer.md) — guest programming model
- [Frame-Buffer-Impl-1.0.md](Frame-Buffer-Impl-1.0.md) — emulator implementation
- [Second Sight](Cards_SecondSight.md) — slot 3 conflict
- [Video Overlay Card](Cards_VOC.md) — slot 3 conflict
- [Writing Config Files Manually](ConfigFiles.md)

# Video Overlay Card (VOC)

The Apple IIgs Video Overlay Card sits in **slot 3** and can display **640×400 SHR** by interlacing the two 32K Super Hi-Res buffers in banks `$E0` and `$E1`.

GSSquared implements that interlace mode as a **progressive 60 Hz** composite (both fields every frame), matching KEGS. Overlay / genlock / dissolve registers are stored but not rendered. The card has **no slot ROM** — `$C300`–`$C3FF` stays the built-in IIgs 80-column firmware. Control is only via device-select `$C0B0`–`$C0BF`.

Second Sight also uses slot 3; pick one or the other in the config.

## How to enable it

### In the config editor

1. At System Select, open **+** or **Edit…**.
2. Choose an **Apple IIgs** platform (ROM 01 or ROM 03).
3. Click **slot 3** and pick **Video Overlay Card**.
4. **Save** the `.gs2` config, then launch it.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "voc"
```

## Enable 640×400 interlace

From the monitor (`CALL -151`) or guest software:

```
C029:C1       ; SHR + host linearization
C0B1:39       ; SHRSource=11, GG Bus enabled
C0B5:80       ; InterlaceEnable
```

Even lines come from `$E12000` (aux); odd lines from `$E02000` (main). Each bank has its own scan-line control bytes and palettes. Disable interlace (clear `$C0B5` bit 7, or leave `$C0B1` 5:4 other than `11`) to return to normal IIgs video.

Detection reads at `$C0B7`, `$C0B8`, and `$C0BD` return `$00` so Uthernet / Second Sight probes do not see those cards.

## Further reading

- Hardware/register spec: [VOC_SPEC.md](VOC_SPEC.md)
- Hand-editing configs: [Writing Config Files Manually](ConfigFiles.md)

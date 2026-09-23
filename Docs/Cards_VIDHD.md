# VIDHD

VIDHD is a high-density video card used with a **65816 Apple IIe** (the `apple2e_65816` platform). It is not part of the built-in `-p 4` tile unless you add it in a custom config.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Set **Platform** to **Apple IIe with 65816** (`apple2e_65816`).
3. Click a slot and pick **VIDHD**.
4. Save and launch.

### In a `.gs2` file

```toml
gs2_version = 1
name = "IIe 65816 + VIDHD"
platform = "apple2e_65816"

[[cards]]
slot = 3
card = "vidhd"
```

(Slot number depends on the software you are running; the editor only offers VIDHD on the 65816 //e platform.)

## Related

- [Selecting a System](Select.md)
- [Command Line](CommandLine.md) — `-p 4` is the 65816 //e without requiring VIDHD
- [Writing Config Files Manually](ConfigFiles.md)

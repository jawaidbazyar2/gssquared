# Videx VideoTerm

Videx VideoTerm is an 80-column text card for the Apple ][ and ][+. It belongs in **slot 3** only. It is not valid on IIe or IIgs platforms (those machines have their own 80-column hardware).

GSSquared renders Videx text with the **monochrome** display engine.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Choose **Apple ][** or **Apple ][+**.
3. Click **slot 3** and pick **Videx**.
4. Save and launch.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "videx"
```

## Using it

Boot software that talks to the Videx card (or switch the guest into 80-column mode the way that title expects). Use **Display → Monitor → Monochrome** (green / amber / white) for a typical VideoTerm look. **F2** cycles display engines.

## Related

- [Displays](Displays.md)
- [Writing Config Files Manually](ConfigFiles.md)

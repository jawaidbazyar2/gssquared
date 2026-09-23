# Second Sight

Second Sight is an Apple IIgs slot-3 video card that can drive an external VGA-style display. In GSSquared it provides that VGA output path and a **text** mode that routes Apple II 40/80-column text through the card’s font.

The Video Overlay Card also uses slot 3 — pick one or the other.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Choose an **Apple IIgs** platform (ROM 01 or ROM 03).
3. Click **slot 3** and pick **Second Sight**.
4. Save and launch.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "second_sight"
```

## Display → Second Sight Text

When the card is present, **Display → Second Sight Text** renders fullscreen Apple II 40- or 80-column text through the card’s VGA path (an Apple-ified font from the Second Sight ROM). Graphics modes and the card’s own VGA modes are unchanged.

The menu item is grayed out if the current machine has no Second Sight. The setting is remembered in app settings. See [Displays](Displays.md).

## Related

- [Displays](Displays.md)
- [Video Overlay Card](Cards_VOC.md) — slot 3 conflict
- [Writing Config Files Manually](ConfigFiles.md)

# Apple Mouse III

Apple Mouse III is a slot mouse card for the Apple //e (and compatible machines). GSSquared emulates the Mouse III hardware and firmware so mouse-aware //e software can use your host mouse.

## What it does

- Provides a Mouse III card in an expansion slot (commonly slot 4).
- Works with software that talks to the Apple mouse firmware entry points (`IN#n` / mouse toolkit calls).
- On the IIgs, you normally use the built-in **ADB** mouse instead — see [Using a Mouse](Mouse.md).

Older configs that named the card `"applemouseiii"` still work; the preferred value is `"mouse"`.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Choose an **Apple //e** or **Enhanced //e** platform (or another non-GS machine that allows the card).
3. Click a slot (often **4**) and pick **Apple Mouse** / **Mouse**.
4. Save and launch.

A sample config ships as `IIe_AppleMouseIII.gs2` (Mouse III in slot 4).

### In a `.gs2` file

```toml
[[cards]]
slot = 4
card = "mouse"
```

## How to use it

1. Boot software that supports the Apple mouse card.
2. Turn on **Mouse Capture** (**F1**, or **Machine → Capture Mouse**) so movement stays in the window.
3. Exit capture with **F1** (or open the Control Panel with **F4**, which releases capture temporarily).

For general capture behavior and IIgs GS/OS tracking, see [Using a Mouse](Mouse.md).

## Related

- [Using a Mouse](Mouse.md)
- [Creating Custom System Configs](ConfigEditor.md)

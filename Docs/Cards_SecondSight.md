# Second Sight

Second Sight is an Apple IIgs video card with its own monitor output. GSSquared offers two implementations:

* **Second Sight** is the simulated card. It provides a VGA output path and an optional text mode that uses the card's font.
* **Second Sight (with Z180)** runs the card's firmware on its Z180 processor, with the Oak video chip, Bt481 DAC and capture logic. Its firmware handles Apple II video modes and programs written for Second Sight.

The built-in IIgs machines use the simulated card. Choose one version per machine.

## How to enable it

### In the config editor

1. Open **+ New** or **Edit…** from System Select.
2. Choose an **Apple IIgs** platform (ROM 01 or ROM 03).
3. Click **slot 3** and pick **Second Sight (with Z180)**, or **Second Sight** for the simulated card.
4. Save and launch.

On a ROM 03 IIgs the real card works in slots 1–6, and on a ROM 01 IIgs only in slot 3 (Second Sight manual, p. 30). Slot 3 is the usual choice. The simulated card and the Video Overlay Card use slot 3.

The card firmware is included in the application resources, on native platforms and in the browser.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "second_sight_z180"
```

Use `card = "second_sight"` for the simulated card. Monitor and screen choices belong to the Display menu and app settings.

## Display choices for Second Sight with Z180

**Display → Second Sight → Monitor → VGA CRT** is the default. **VGA LCD** puts a current LCD monitor on the same VGA cable (see [Displays](Displays.md)). **AppleColor RGB** selects the AppleColor RGB monitor. The firmware uses this choice to select its supported modes; some VGA modes are unavailable on RGB. The ROM reads the monitor sense at each card mode change, so the choice takes effect then. **Machine → Restart** shows the change at once.

**Display → Second Sight → View → IIgs monitor** shows the IIgs output in the main window. **Second Sight monitor** shows the card output there instead. **Both** (the default) keeps the IIgs in the main window and opens a separate Second Sight window. In the browser, both pictures appear side by side. Full screen shows the selected view. **F11** cycles IIgs → Second Sight → both.

The two outputs are independent and keep running in all three views. The IIgs uses its normal renderer and **Display → Monitor** choice. The Second Sight monitor uses only the card's VRAM and DAC. When its firmware mirrors the IIgs screen, it renders the card's captured copy.

These items are grayed out without **Second Sight (with Z180)**. Monitor type and view are remembered in app settings. In the browser they last for the page session, like the other browser app settings. Closing the separate card window selects **IIgs monitor**.

## Display → Second Sight Text

For the simulated card, **Display → Second Sight Text** renders fullscreen Apple II 40- or 80-column text through the card's VGA path (an Apple-ified font from the Second Sight ROM). Graphics modes and the card's own VGA modes are unchanged.

The menu item is grayed out without the simulated card. The Z180 implementation renders text through its firmware. See [Displays](Displays.md).

## Firmware and Lattice chip options

Choose **Settings → Second Sight → Firmware** and **Settings → Second Sight → Lattice chip**.
These options are grayed out without **Second Sight (with Z180)**. They are saved
in app settings, including across browser reloads. They take effect at the next
power-on or **Machine → Restart**; Reset leaves the running card unchanged.

| Firmware option | Included ROM |
|---|---|
| 1.4-BEEF (original), the default | `ROM_BEEF.bin` |
| 1.5-CBFC | `ROM_CBFC.bin` |
| 2.0-3277 | `ROM_3277.bin` |

The Lattice chip options are **Original (BE8C)** and **v2.0 (BBB1)**. Every
firmware/chip pairing can run. Firmware 1.5 is intended for the original Lattice
chip; firmware 2.0 is intended for Lattice v2.0. Changing one option leaves the
other choice intact. The defaults are 1.4-BEEF and Original (BE8C).

The Lattice chip is the square 44-pin socketed chip at the right end of the
card, above the slot connector. The original has a white BE8C sticker.

The 1.5-CBFC and 2.0-3277 builds split uploads at the 512 KB video-memory
boundary and use level-sensed DMA for the second piece. They also keep Apple
display switches from resetting a native video mode. SetPalette preserves the
user font when font 3 is selected by using the command buffer at SRAM `$0A600`.
With fonts 0, 1 or 2, it uses `$0F000–$0F2FF`, as firmware 1.4 does. The original
1.4 image is unchanged. These builds acknowledge a mode-change command after
its pending mode change is applied, so an interrupt during the main loop cannot
leave the previous mode active.
AppleColor RGB double hires (mode `$FA`) uses 260 lines at 59.9 Hz in these
builds, correcting the original 222-line, 70.2 Hz timing
([firmware fault E30](https://github.com/Webifi/second-sight-reference/blob/master/docs/reference/errata.md#e30-double-hires-on-the-applecolor-rgb-monitor-runs-at-70-hz)).

**Settings → Second Sight → INT0 pull-up** selects **None (stock card)**, the
default, or **Added (card modification)**. The card wires the VGA chip's
vertical retrace interrupt to the Z180 INT0 input with no pull-up resistor, so
on a stock card that interrupt never reaches the Z180. A pull-up resistor from
Z180 pin 10 to +5 V makes it work. No Second Sight firmware uses it; software
that waits for the vertical retrace must read port `$3DA` bit 3 on a stock card.
Like the other hardware options, it takes effect at the next power-on or
**Machine → Restart**.

The original Lattice chip reproduces the factory BE8C map, including its
hardware faults. Lattice v2.0 corrects direct bank-1 capture with RAMWRT on,
supplies the border-write event, and selects the right capture bank with
80STORE on. Both maps flag every copied write in `$9000–$9FFF`, including
palette and scan-line control bytes. The emulator does not patch firmware
behaviour to compensate for the selected chip.

There is one card key, `second_sight_z180`, and only one Second Sight device can
be installed. Firmware images live in `assets/roms/cards/secondsight/` and use
the normal native and web resource packaging.

## Related

- [Displays](Displays.md)
- [Video Overlay Card](Cards_VOC.md)
- [Writing Config Files Manually](ConfigFiles.md)

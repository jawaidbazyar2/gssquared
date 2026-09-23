# Displays

GS2 provides a variety of display modes, emulations, and controls.

These are matters of personal preference, so you get to pick the one you like best!

Typically any mode / control can be used with any computer.


## Display Engines

GS2 supports three different Apple II Display Engines:

* Composite / NTSC
* IIgs RGB
* Monochrome

You can use any rendering mode (for the most part) with any computer type.

In the Control Panel, there are buttons to change the display engine - NTSC, RGB, and Monochrome. And, buttons to change the Monochrome color (green, amber, white).

Display engine can be selected by menu, from the Control Panel, from the hover Display picker, or by pressing **F2** to cycle through the display engines.

NTSC mode has additional controls for Hue (Color) and Saturation, just like real composite monitors.

Hue is adjusted with the keypad Plus and Minus Keys, while holding SHIFT.
Saturation is adjusted with the keypad Plus and Minus Keys, while holding OPTION / WINDOWS.

Reducing Saturation to 0.5 results in colors that are a little more subdued, similar to some other emulators and certain CRT monitors.
Reducing Saturation to 0 results in a grayscale (not monochrome) display.

## Display Configuration

In the OSD, there are buttons to change the display engine - NTSC, RGB, and Monochrome. And, buttons to change the Monochrome color (green, amber, white).

* F2 cycles through the display engines.  
* F5 toggles between pixel-blur and rectangular. pixel-blur provides a little more "analog" upscaling of Apple II dots to modern displays. Rectangular performs an exact square upscaling/downscaling.
* F3 toggles between Full-Screen and Windowed modes.

These are matters of personal preference, so you get to pick the one you like best.

## CRT Effect / Shader

GS2 has a "CRT Shader" option under the Display menu, that turns on a "CRT Emulation" effect.

The effect modifies the normal display output, applying a "shadow mask" and some other enhancements to attempt to emulate the way pixels appear on an 80s-era CRT display.

Available on macOS (Metal), Windows (D3D12), and Linux (Vulkan / SPIR-V). Works best on Retina / high-DPI displays but works pretty well on other monitors such as 27" e.g. 1440p (2560 x 1440).

The shader mode can be activated with Display > CRT Shader, or by pressing F7. The [`-g`](CommandLine.md) command-line flag enables it at boot.

## Second Sight Text

If the current machine has a **Second Sight** card, **Display → Second Sight Text** renders Apple II fullscreen 40- or 80-column text through the card’s VGA path, using an Apple-ified font from the Second Sight ROM. Graphics modes and Second Sight’s own VGA modes are unchanged.

The menu item is grayed out when no Second Sight card is present. The setting is remembered in app settings. See [Second Sight](Cards_SecondSight.md).

## Pixel Modes

GS2 has two Pixel Modes:

* Pixel blur ("Analog")
* Rectangular

Pixel-blur provides a little more "analog" upscaling of Apple II dots to modern displays, that gives it more of that old CRT feel.

Rectangular performs an exact square upscaling/downscaling, which is common in other emulators, and may be more to some people's liking.

Pixel Mode can be selected from the hover Display picker or by pressing **F5** to toggle between pixel-blur and rectangular. F5 is **not** a switch between “accurate NTSC” and a legacy renderer — that path is always the Composite engine (**F2**).

## HUD overlays

**Display → HUD → Stats** shows a small performance overlay (off by default).  
**Display → HUD → Drives** shows the drive-activity strip at the bottom of the screen (on by default).

Both are remembered in app settings. Turn them off when you want a clean picture for recording.

Hover speed and display widgets are documented in [On-Screen Display](OSD.md).

## Windowed - Full Screen

GS2 Supports windowed (default) and full-screen modes.

Use full-screen mode to make your classic Apple II games the full size of your modern monitor.

**F3** toggles between Full-Screen and Windowed modes.

## PAL timing

II / II+ / IIe configs can use European **PAL** video timing (50 Hz) instead of NTSC. Shipped examples include a PAL //e `.gs2` in `Documents/GSSquared/`. In a hand-written config, set `clock = "pal"` (not valid on IIgs). See [Writing Config Files Manually](ConfigFiles.md).

## Video fidelity notes

GSSquared runs the CPU in frame-sized chunks, then draws video. Titles that depend on **cycle-accurate mode switches** still work for the usual cases; the [debugger](UsingTheDebugger.md) Video pane can show a cycle-accurate beam view if you need to inspect timing.

**Floating-bus** reads (video data appearing on unmapped reads) are implemented so demoscene titles that sample the bus can run. That is an emulator fidelity choice, not a Display menu item.



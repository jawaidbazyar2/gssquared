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

GS2 has a "Shadow Mask" option under the Display menu, that turns on a GPU CRT shader effect.

The effect modifies the normal display output, applying a "shadow mask" and some other enhancements to attempt to emulate the way pixels appear on an 80s-era CRT display.

Available on macOS (Metal), Windows (D3D12), and Linux (Vulkan / SPIR-V). Works best on Retina / high-DPI displays but works pretty well on other monitors such as 27" e.g. 1440p (2560 x 1440).

The shader mode can be activated with Display > Shadow Mask, or by pressing F7. The [`-g`](CommandLine.md) command-line flag enables it at boot.

The GPU shader is not available in the web build. Its SDL renderer has no WebGPU backend. Second Sight uses the same Shadow Mask switch for a software scanline effect, available in either native window and in the browser.

GPU effects are optional. If a shader or its render target cannot be created, the corresponding effect is disabled and its menu item is grayed out. Composite and monochrome retain their CPU decoders. **GS Composite** requires its GPU encoder; **Monochrome persistence** also requires its history texture and blend operations. Second Sight's software scanlines do not require a GPU shader.

## Second Sight Text

If the current machine has the simulated **Second Sight** card, **Display → Second Sight Text** renders Apple II fullscreen 40- or 80-column text through the card’s VGA path, using an Apple-ified font from the Second Sight ROM. Graphics modes and Second Sight’s own VGA modes are unchanged.

The menu item is grayed out when no Second Sight card is present. The setting is remembered in app settings. See [Second Sight](Cards_SecondSight.md).

## Second Sight with Z180

Second Sight with Z180 has its own monitor output, separate from the IIgs display. **Display → Second Sight → Monitor → VGA CRT**, **VGA LCD** and **AppleColor RGB** (default) select its monitor. The ROM reads the monitor sense at each card mode change and offers different modes for VGA and AppleColor RGB. The choice of cable takes effect at the next card mode change; **Machine → Restart** shows it at once. VGA CRT and VGA LCD use the same cable, so a change between them shows at once.

**Display → Second Sight → View → IIgs monitor**, **Second Sight monitor**, and **Both** select one or both outputs. With one selected, the main window and full screen show that monitor. With both selected, the native app uses a separate card window; the browser shows both pictures side by side. **F11** cycles these three views. Both is the default.

The IIgs renderer runs normally every frame, independently of the card. The card monitor shows only its own VRAM and DAC output, including any IIgs picture captured and mirrored by its firmware. Hiding either monitor does not stop it. Closing the separate card window returns to the IIgs view.

These options are grayed out when the machine has no **Second Sight (with Z180)**. They are remembered in app settings, just like Second Sight Text. Browser settings last until the page is reloaded. See [Second Sight](Cards_SecondSight.md) for card selection and the included firmware.

The VGA CRT shows each mode as a user sets its size and position controls for it: the picture keeps the aspect of the monitor's factory raster at the signal's scan rates and fills the screen, with its border in the programmed overscan color. Border that does not fit is cropped evenly. On AppleColor RGB, a mode with the IIgs screen's shape shows the same visible time and line window as GS2's IIgs SHR display. Other modes keep the width and position that the factory sets with the IIgs signal, because those controls are inside the case; only the picture height follows the back-panel V-HEIGHT control, within its range. A signal that a monitor cannot lock to shows no picture and an on-screen message. On AppleColor RGB, a mode that locks only after the user turns H-HOLD or V-HOLD on the back panel, or that needs V-HEIGHT turned more than half way to an end, shows its picture and a short on-screen message. The CRT monitors use GS2's aspect fitting. The browser reserves space above them for its menu bar.

**VGA LCD** is a current LCD monitor, as its controller works: a Realtek RTD2556 with a 1920 x 1080 panel. It compares the signal's sync with its stored modes, samples the part of each line that the chosen mode holds with its own clock, and scales the samples to a 4:3 area of the panel with its own filter, at its factory sharpness. A 400-line mode, which can be 640 or 720 pixels wide at the same timing, gets the width setting that matches the picture. Each new mode gets the monitor's Auto adjustment, then Clock and Phase are set for a steady picture without moving bars, and a picture smaller than the mode is centered. A signal with no stored mode, or out of the monitor's range, shows no picture and an on-screen message. The panel's filter runs on the GPU where one is available. The browser version offers VGA LCD only with WebGL on a hardware GPU.

**Display → Shadow Mask** / **F7** adds scanlines to the whole CRT card picture, including its border. It is off by default; the LCD has no scanlines. The line spacing follows the signal: VGA has fine 400- or 480-line scanning, including doubled 200-line modes; AppleColor RGB has visible gaps between progressive lines, or alternating fields for interlaced modes such as $61. AppleColor RGB cannot display 480p VGA timing.

The card effect is the same in native and web builds and reuses unchanged rows. It changes only the displayed picture; it does not change the card's pixels, timing, or monitor sense. The IIgs monitor keeps its existing native GPU effect.

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



# GSSquared v1.1

I am pleased to announce the release of **GSSquared v1.1**.

GSSquared is an Apple II series computer emulator.

## Supported platforms

- Apple ][
- Apple ][ Plus
- Apple //e
- Apple //e Enhanced
- Apple //e Enhanced with 65816 CPU and Super Hires Video
- Apple IIgs ROM 01
- Apple IIgs ROM 03

## Pre-built binaries

- **macOS** (Intel and Apple Silicon)
- **Windows 10+**
- **Linux AppImage** (should run on many Linux distros; tested on Ubuntu 22)

## Play in the browser

No install: the same emulator runs at [gssquared.net/live](https://gssquared.net/live).

---

This release is about looking and sounding like the machine on your desk. Composite video moves onto the GPU, so NTSC color is both faster and closer to an AppleColor monitor. An Apple IIgs can show **GS Composite** — the IIgs RGB picture put through a composite encoder, the way a real GS looks on a composite display. Green, amber, and white monitors keep a phosphor glow. Mockingboard music is in tune, you can pick a timbre, and now has Speech synthesis. A //e can have an **8 MB RamWorks** card. Serial ports can send a print job to a network printer or to a printer app on your computer. The window is gentler on the host when you look away: it uses less of your computer's CPU when it's not the top window, and it can pause when minimized.

## Features

- **Faster, sharper composite.** NTSC and monochrome pictures are rendered with a GPU when your computer has one (macOS Metal, Windows D3D12, Linux Vulkan), especially on a high-DPI display like an Apple Retina. The GPU improves rendering performance by 200%. The existing code is unchanged if you don't have a GPU.

- **GS Composite.** If you have a GPU, an emulated Apple IIgs, **Display → Monitor → GS Composite** shows the IIgs RGB output as composite color, like a real GS on an AppleColor screen. **F2** cycles through the engines; this one is skipped on II / //e. Choose it from the Display menu (the hover color buttons do not include it yet).

- **Phosphor persistence.** Green, amber, and white monitors keep a short glow from the previous frame. **Display → Phosphor Persistence** turns it off. It is on by default when the GPU decoder is in use.

- **Scaling and shadow mask.** **Display → Scaling** is **Smooth** or **Sharp** (**F5** still toggles). The CRT effect is now **Display → Shadow Mask** (**F7**, or `-g` at launch). Hue and saturation keys are unchanged.

- **RamWorks.** On a //e, //e Enhanced, or 65816 //e, put **RamWorks** in the **Aux** slot (slot 0 in a `.gs2`). That is a fixed **8 MB** of extra auxiliary RAM (128 banks of 64K). Software that talks to `$C073` can use it as a RAM disk; the screen still comes from the first 64K of auxiliary memory. It is not the Slinky Memory Expansion card (that one stays in slots 1–7).

- **Dramatic Mockingboard fidelity improvements, and speech.** Programs that use the "volume control trick" to play samples, now work. (E.g. DigiDream) **Settings → Mockingboard → Timbre** chooses Square (default), Soft Square, Warm Sine, Organ, or Sawtooth. Now also supports speech synthesis (SSI-263 chip).

- **Printer on serial.** On a Super Serial Card or a IIgs serial port, attach **Printer**. On Mac and Windows, GSSquared now works with a companion app GS2Print, that provides Color ImageWriter II -> PDF generation. See https://github.com/jawaidbazyar2/gs2print

- **Net Printer.** Serial ports can attach **Net Printer**: type a host and TCP port (often 9100). Bytes go out as a raw TCP stream, the way a JetDirect-style port works, so if you have a PCL compatible printer, you can print directly to it from GS/OS and IIe PCL drivers.

- **Connections panel.** Serial, parallel, Net Printer, and (on a IIgs) the Host Folder configuration uses nice new UI widgets. Click the pencil on **Host Folder** to pick the folder GS/OS sees as `:Host`.

- **Pause when minimized.** **Settings → Pause when Minimized** (off by default) freezes the Apple while the window is in the dock or taskbar. The debugger window still counts as “you are here.” When you switch to another app, GSSquared sleeps between frames so it uses less CPU, even if Sleep / Busy Wait is off. The screen saver is allowed unless a machine is running and the window is focused.

- **A newer version, if there is one.** When GSSquared starts, you will be notified when a new version of GSSquared is available. A button will appear which opens the same updates page as **Docs → Check For Updates**.

- **Save in the config editor.** **Save** writes over the `.gs2` you are editing. **Save As** asks for a new name (and a new machine identity on a IIgs). Right-click a recent machine tile — or Cmd-click / Alt-click — to edit it.

- **Web player.** Audio in the browser is less likely to drop out. The page can also run on a static HTTPS host that cannot send the usual isolation headers (a small worker supplies them).

## Accuracy

- Paddle timers behave like the real NE558: a `$C070` strobe does not restart an axis that is still timing out. Games that read a long axis twice no longer see it finish early.

- Composite colorburst on and off follows an AppleColor IIe monitor. Short bursts of graphics can leave color off; a full burst holds color through vertical blank.

- On a IIgs, ROMBANK is implemented, language-card banks sit in the right places, and shadowing of auxiliary hi-res only happens when hi-res is actually on. Ludicrous speed is a bit faster because the memory map uses lookup tables.

## Bug Fixes

- GS/OS printing to the IIgs printer port no longer hangs waiting for a transmit interrupt that never stayed pending.

- **IIgs volume.** The Ensoniq sound chip follows the IIgs system volume (`$C03C`). Loud titles are closer to a real GS. If the host audio queue fills up, a frame is dropped instead of stretching.

- If you resized the System Select windows, emulation display is now sized/scaled correctly.

- Debug builds label the window **[debug]** so you can tell them from a release build.

- The debugger can save the instruction trace with **`trsave`**, and **Settings → Debug** chooses whether tracing starts on and whether a trace file is written when you leave the machine.

- Mounting a partitioned CD (APM) no longer marks your other hard-disk images write-protected.

- Inbound modem listen on Windows starts correctly, so a telnet client can ring the guest.

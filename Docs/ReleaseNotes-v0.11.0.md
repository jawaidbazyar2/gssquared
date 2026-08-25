# GSSquared v0.11.0

I am pleased to announce the release of **GSSquared v0.11.0**.

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

---

This release is about connecting the virtual machine to the real one — host serial ports, print-to-clipboard, Host FST on Windows — and about display and accuracy: CRT shaders on Windows and Linux, Second Sight Apple II text, CD-ROM / APM volumes, and a large round of IIgs timing and MMU fixes (including textfunk). The debugger can snapshot video pages, and the debug protocol now works on Windows.

## Features

- **Host Serial.** Attach a real host UART to IIgs SCC or Super Serial from the Control Panel or config editor: macOS `/dev/cu.*`, Windows `COMn`, Linux `/dev/ttyUSB*` / `ttyACM*` / `ttyAMA*` (Bluetooth/debug callouts skipped). Guest baud, data, parity, and stop bits pass through. Unplugged dongles stay attached and retry.

- **Print to clipboard.** Serial and parallel ports can attach a **Clipboard** device. Guest output is buffered, high-bit stripped, and copied to the host clipboard after idle-close (~10s) or Ctrl-Reset, with a toast for the byte count.

- **CRT shader on Windows and Linux.** The CRT GPU effect (already on macOS) is available on Windows (D3D12 / DXIL) and Linux (Vulkan / SPIR-V). Toggle with **Display → CRT Shader** or **F7**. Prebuilt shader blobs ship with the app; extra compiler SDKs are not required to build.

- **Host FST on Windows.** Share a host folder with GS/OS as `:Host` on 64-bit Windows. ProDOS type/auxtype, Finder info, and resource forks are stored in NTFS Alternate Data Streams (use an NTFS folder).

- **Second Sight Apple II text.** With a Second Sight card, **Display → Second Sight Text** renders Apple II 40/80-column text through the card’s VGA path (Apple-ified font from the Second Sight ROM).

- **CD-ROM / APM on BazFast.** Mount `.iso` CD dumps (always write-protected). If the image has an Apple Partition Map, each ProDOS and HFS partition appears as its own SmartPort unit — the usual path for 1990s Apple IIgs CD-ROMs.

- **IIgs paste.** **Edit → Paste Text** and Shift+Insert now work on IIgs ADB, matching the existing II/IIe path: one character per frame so the guest can keep up.

- **Debugger video pages.** The debugger Video pane can decode a specified display page (`hgr1`, `80text1`, SHR, …) as a static thumbnail while you step.

- **Drivers volume.** **File → Mount Drivers** still mounts `/GS2.DRIVERS`. The installer on that disk has options to install **Host FST**, **Marinetti**, and **Uthernet II**.

## Accuracy

- IIgs FPI registers (`$C035`–`$C037`, `$C02D`, `$C068`, `$C071`–`$C07F`) are billed as fast cycles instead of 1 MHz Mega II accesses — textfunk display timing is correct.

- Ludicrous speed is now fixed multiples of the 14 MHz clock so the video scanner still runs. Speaker, Ensoniq, and related devices no longer fall apart under LS.

- 65816 / 6502: WAI implementation; `PLB` is 4 cycles (was 2); `XCE` is 2 cycles (was 1); native-mode relative branches no longer take a page-cross penalty (ROM 03 beep pitch).

- ROM 03 keyboard repeat rates above default are no longer extremely slow.

- IIgs MMU: bank `$E1` language-card decode; linearization and shadow fixes; ROM 03 cold-start reset; banks `$F0`–`$FF` treated as fast ROM.

- GS/OS mouse tracking on 640-pixel SHR desktops no longer tracks as 320.

- Second Sight VBL is generated in text mode under ludicrous speed (GNO/ME no longer freezes).

## Bug Fixes

- `.2mg` images with missing or zero header parameters no longer truncate or misread; header sanity checks are stronger.

- BazFast refuses 140K floppy images (those belong on a 5.25" drive) and will not mount the same host file more than once.

- **File → Quit** now works on Windows (enabled at System Select; use **Close Emulation** while a machine is running).

- Debugger monitor pane jumps to the newest output after a command instead of staying scrolled to the top.

- Windows path handling in system settings is more reliable.

## Internals

- **Debug Protocol** listens on Windows AF_UNIX sockets (`--debug` on Win10+). New/extended commands: mount/unmount storage, linearized text-page capture, get/set CPU registers, and `PASTE_TEXT` (Python `paste_text`) so agents can fill the paste buffer in one round-trip.

- Debugger user guide: `UsingTheDebugger.md`.

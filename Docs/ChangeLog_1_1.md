# GSSquared Change Log — 1.1

Covers commits after the GitHub **v1.0.0** tag (2026-09-29) through 2026-11-20 (~111 commits, excluding the 1.0 notes commit). Version in tree is **1.1**.

This is the technical changelog. The user-facing summary is [Release_1.1](Release_1_1.md).

---

## Features

### Display

- **GPU NTSC decoder.** Composite and monochrome engines pack a 560-dot bitstream, then decode on the GPU when an SDL GPU renderer is available (Metal / D3D12 / Vulkan). The CPU path remains for the web build and software rendering. (`e41b74f6`, `f8b88dd2`, `1a255042`)
- **GS Composite monitor.** New **Display → Monitor → GS Composite** engine (IIgs only; **F2** skips it on 8-bit machines). IIgs RGB is encoded to composite in a fragment shader, matching an Apple IIgs on an AppleColor composite monitor. GPU shaders ship as Metal (macOS) and SPIR-V (Linux); Windows uses the CPU composite fallback until a DXIL blob is committed. (`2dc94731`, `38be06b6`)
- **Phosphor persistence.** **Display → Phosphor Persistence** (on by default when a GPU decoder is present). One history texture is faded per emulated frame and max-blended with the new monochrome frame. Decay is milder than the first landing. (`ed4b0306`, `c9ba2315`, `0aed0214`, `578b6bbc`)
- **Scaling menu.** **Display → Scaling → Smooth / Sharp** replaces the old “pixel-blur / rectangular” wording. **F5** still toggles the two modes. (`c9ba2315`)
- **Shadow Mask.** The CRT GPU effect is now **Display → Shadow Mask** (was “CRT Shader”). **F7** and `-g` are unchanged. (`macos/linux/windows menus`)
- **NTSC chroma-trap luma filter.** Held-dot weights use a Gaussian luma with a null at the subcarrier (on by default). Alt/Shift + keypad `/` still toggles the classic 2.0 / 1.3 / 0.6 MHz filter. (`126f580b`)
- **Held-bit NTSC.** Fixes a perceived error in the OE algorithm (and ours) for held bits. (`06fedf21`)
- **AppleColor colorburst.** Color killer / burst-gate latency matches an AppleColor IIe composite monitor: burst on horz 13–16, gate sees 15–16, integrated on/off timing for VIDSYNC phases 0–4 (issues **#217**, **#223**). (`147ffb22`, `ab7b2129`, `56ebd279`)

### Memory and cards

- **RamWorks.** Applied Engineering-style 8MB auxiliary RAM on IIe / IIe Enhanced / IIe 65816. Config slot **0** (labeled **Aux**). Always 128 × 64K banks; write `$C073` to select, read `$C071` for the bank. Video stays on AUX 0. (`6fef4ab0`, `76e0dd27`)
- **IIe MMU (`MMU_IIe2`).** Language-card / 80STORE / aux mapping lives on the MMU. The old `iiememory` device is gone. Golden `iiemmutest` / `iiemmu2test` harnesses record the map. (`b2db6677`, `67fc9251`, `9c4dae76`)
- **IIgs MMU.** Struct-of-arrays page tables; LUT for aux offsets (~15% ludicrous-speed gain on a tight JMP loop vs 1.0); LUT for `shadow_is_enabled` (~70% cheaper). Language-card bank 2 is at `$C000`, bank 1 at `$D000`. `AUXHGR` shadows only when HGR1 or HGR2 is also on. Two I/O registers force unused bits to 0. (`caa6f07b`, `b78db30f`, `c8d913c5`, `f794ac48`, `8a709ec8`, `da9496e0`)
- **ROMBANK (`$C028` / `$C068` bit 1).** Implemented for Mega II windows that alias bank `$FF` on ROM 01. ROM 03 still reads the bit; CYA pin 60 is grounded so the windows do not move. (`601627f3`, `7ce69789`)

### Audio

- **Mockingboard fixed-point synth.** Sample position is integer; clock-skew correction; additional harmonics and zero-crossing waveforms. (`f8fbc15e`, `d27275e0`, `37bd4b3f`, `6875b7b2`)
- **Mockingboard Timbre.** **Settings → Mockingboard → Timbre**: Square (hardware), Soft Square, Warm Sine, Organ, Sawtooth. Mono Helper moves under the same submenu. (`28ef8b87`)
- **Mockingboard C speech (SSI-263).** Votrax SC-02 on Mockingboard C (VIA A6). Vendored MIT core from `ssi263-speech` (Tamas Geczy) plus die-read phoneme ROM. (`19e19e73`)
- **Mockingboard pitch and samples (Webifi).** Tones and envelopes at the right pitch; second chip’s envelope keeps running; samples written to volume registers; register/generation cleanup on reset. (`#229`, `#233`; `8939f384`, `8913c156`, `b5e1b5d4`, `e64e9a44`, `578f7cf3`)
- **Ensoniq system volume.** DOC respects `$C03C`. 64× sample multiple for a more GS-like level; 20 ms volume glide; drop a host audio frame if the queue is too full (**#211**). (`78b5c458`, `4414b015`, `0f9c380b`, `c93553fa`)

### Serial, parallel, printing

- **Printer attachment.** SSC and IIgs SCC ports offer **Printer**: same file capture as File, plus a `.gs2print` ticket to a sidecar on native builds. SSC DIPSW1 is SIC P8 so firmware does not wait for ACK. Sidecar RX (e.g. `ESC ?` → `IW10C`) is fed back as serial receive. Parallel stays file/clipboard. (`51219f84`, `725f3eea`, `df54e133`, `bc3a5015`)
- **Net Printer.** Raw TCP client (`device = "netprinter"`, host + port, often 9100). Opens on the first guest byte, closes after 60 s idle. Native only. (`8d52dee2`)
- **Connections UI.** Control Panel / config editor connections container restyled (rounded cards, vector icons). Host FST folder picker lives on that row, not a separate **Host Folder…** button. (`8d52dee2`, `15839369`)
- **SSC / SCC RESET.** Cleaner reset handling on both UARTs. (`8d52dee2`)
- **Windows inbound modem listen (#196).** Serial-device worker threads start after the listen socket is bound so Windows inbound telnet on port 6502 works. (`643e6c21`)
- **SCC Tx interrupt pending.** Tx IP stays latched until the transmit buffer is written or WR0 cmd 5 (Reset Tx Int Pending), per Zilog. Fixes GS/OS printing hanging because the firmware ISR saw no Tx interrupt. (`cef686f1`)

### Workflow and UI

- **Pause when Minimized (#212).** **Settings → Pause when Minimized** (off by default). Guest freezes while the main window is minimized; a focused debugger window still counts as being in the app.
- **Power-efficient unfocus (#212).** When neither the emulator nor the debugger has input focus, the run loop sleeps between frames (same wait as Sleep / Busy Wait) regardless of the Sleep checkmark. Display sleep (screensaver) is inhibited only while a machine is running *and* a window has focus (**#205**).
- **Update hint on System Select.** Background HTTPS GET to gssquared.net; if this desktop build is behind the latest GitHub release, **Click to get new version** appears under + / Launch / Edit. Fail-closed; not on web. (`1d05a8bd`)
- **Config editor Save (#210).** **Save** overwrites the current `.gs2` with no dialog. **Save As** names a new file (and a new machine `id`). Right-click / Cmd-click / Alt-click a recent tile opens the editor.
- **IIe slot 0 labeled Aux.** OSD and config editor show **Aux** instead of **0** on IIe-family machines. (`f5032117`)
- **Modals.** Shared rounded modal chrome for quit, dirty-disk, pack fetch, launch-config. (`62d4f4e5`)
- **Debug window title.** Debug builds append `[debug]` to window titles. (`1b31bf09`)
- **HD20SC activity.** Drive-button math no longer divides size by two. (`d079f039`)
- **Recent-config tiles.** Display list sized from the candidate set instead of a fixed maximum. (`5b2101f5`)

### Web

- **Static hosting.** `isolation-worker.js` plus shell.html so GitHub Pages–style hosts that cannot send COOP/COEP headers still isolate via a service worker over HTTPS. (`17a2481f`, `#227`)
- **Web audio dropouts.** Extra buffering in Sound GLU so browser audio does not underrun. (`dca7540b`, `#228`)
- **Native web texture format.** Avoid a CPU pixel conversion on the canvas path. (`1a8eef6f`, `#230`)

### Debugger and protocol

- **CPU Trace / Write Trace File on Exit.** **Settings → Debug**. Trace-on-start is on by default; save-on-exit is off. Ring buffer is power-of-two for cheaper wrap. (`58907268`)
- **`trsave`.** Monitor command writes the current trace ring to a file. (`1977ee4b`)
- **LOADMEM.** Debug protocol loads a host file into a memory domain in one round-trip (`Client.load_mem`). (`29c99dad`)
- **COP / BRK / WDM signature.** Trace and disassembly show the signature byte. (`b2b7b6db`)

---

## Bug Fixes

| Issue | Description | Commit |
|-------|-------------|--------|
| **#196** | Windows inbound modem listen: start serial worker after bind | `643e6c21` |
| **#205** | Do not inhibit the screensaver except while a running machine has focus | `c04d50ca` |
| **#208** | After System Select / editor, refresh the emulator target rect from real output pixels (high-DPI / resize) | `5b200591` |
| **#211** | Drop an Ensoniq host audio frame if the queue is too full | `c93553fa` |
| **#212** | Pause when minimized; sleep between frames when unfocused | `d8f45e7b` |
| **#215** | Paddle NE558: `$C070` does not retrigger an axis that is still running (IIe Tech Note #6) | `c2711cf3` |
| **#223** | AppleColor colorburst latch / VIDSYNC phases | `56ebd279` |
| **#229** | Mockingboard volume-register samples | merge |
| **#233** | Mockingboard tone and envelope pitch | merge |
| APM | Mounting an APM image no longer write-protects *other* already-mounted media | `cf828d2a` |
| SCC | Tx interrupt pending no longer cleared by register reads (GS/OS printer hang) | `cef686f1` |
| Persistence | GPU mono persistence blend / availability | `0aed0214` |

---

## Internals (not user-visible, or only via tests)

- Packed `FrameBits` intermediate; LUT-based composite scan generator; monochrome renders white then a post-process multiply tints it. (`f8b88dd2`)
- `ntsc1120` / `ntscview` prototype tools for decoder comparison. (`126f580b`)
- `vsgtest` colorburst / AppleColor reference. (`ab7b2129`, `56ebd279`)
- `iiemmutest` golden map; `iigsmmutest` aux/shadow predicates and ROMBANK cases.
- `testdev/adbtest` for ADB edge cases. (`573e2709`)
- `systemconfigtest` fixtures (aliases, ParallelOutput). (`0365e1da`, `#231`)
- CONTRIBUTING.md expanded. (`73b7e073`)
- Credits in README. (`6c377d05`)

---

## Documentation written with the code

Several developer specs landed with the features (not the user-guide index): `NTSCShader.md`, `RamWorks.md`, `IIeMMURefactor.md`, `PrinterEmulation.md`, `NetPrinter.md`, `NotifyUpdate.md`, `TwoLevelPageTables.md`, `PhantomWrites.md`, `SCC8530_Serial.md` Tx-IP note.

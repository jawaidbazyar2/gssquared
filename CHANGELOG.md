# Changelog

## 2026-08-01

Covers commits from 2026-07-10 through 2026-08-01.

### Features

- **Apple IIgs ROM 03.** New ROM 03 platform with 256K ROM bank `$FF` offset derived from ROM size, text page 2 shadowing, ROM 03 identity flags, and boot-hang workarounds. (`4fdb2ea`, `f6a9a5c`, `ad5f436`, `6fc73cd`, `605a2c3`)
- **Debug Protocol / external debugger.** New `DebugProtocolServer` with a Python client library and examples: RESET, memory domains (`MEGAII`, `MAIN_RAW`, `MEGAII_RAW`), `bpd`/`bpi` breakpoints, `step_info` with count, trace-buffer access, Ensoniq introspection, and DiskII hooks (`.po` uses `vol=>0` for a2osx). (`948ff76`, `7fd2a99`, `e951d15`, `904ddd4`, `5b0d837`, `b92dccd`, `74b2ac2`, `65ea90d`, `709653d`)
- **Uthernet II.** Slirp-based Ethernet card that works out of the box on macOS, Windows, and Linux; libslirp version pinned. (`a1b2a00`, `8498cc0`)
- **Super Serial Card.** New SSC device with MOS6551 emulation. (`5e35081`)
- **IIgs Host FST and drivers volume.** Initial Host FST port plus a built-in drivers volume (à la VMware) that toggles mount/unmount of the Host FST and host driver. (`51a34a3`, `99fbb44`, `e1f62d6`, `0556e3c`)
- **Apple Mouse III.** New Mouse III card with PIA6520; Mouse II paths redirect to the III implementation. (`10ad57a`)
- **Screenshot to disk.** Save Screen writes timestamped screenshot files (#134). (`1e53380`)
- **Serial / parallel connection UI.** In-app UI for managing serial and parallel connections; `FileDevice` shows a toast with the filename on close; parallel card refactored onto `FileDevice` and a child thread. (`29b57cb`, `93d3d71`)
- **DOC stereo and drive stereo.** Ensoniq DOC stereo output (mono copied to both channels when needed); drive sound effects stereo-ified. (`160721c`, `d2a0e05`)
- **IIgs French keyboard.** French ADB keyboard layout. (`cacab7d`, `8d45c69`)
- **`.gs2` UUID / per-config BRAM.** Auto-generate and persist a UUID on `.gs2` files; BRAM files are named by that UUID so configs keep distinct battery RAM. (`a8ad06b`)
- **Custom configs in Prefs.** Custom configs live as `.gs2` files copied into Prefs on startup; Launch defaults there; empty recent-config list is seeded from distributed configs. (`bd3a75b`, `6f33a97`, `7315428`)

### User Interface

- **HUD and gamepad preferences (#139).** Menu toggles to hide HUD stats and drive icons, plus “disconnected when no gamepad.” (`78e309f`)
- **System Select / config editor polish.** Launch button; hover hints for +/Edit; restyled new/edit controls; version display; website and donate menu links. (`6d52665`, `2c19e82`, `a734321`, `e1443a0`, `235b966`, `c057046`)
- **Debugger UX.** Monitor command rewrite; scrollable debug window; buttons to toggle debug-module panes; colored shared 6502/65816 trace; GS-style `/` bank separator and 24-bit addresses; M/X width in disassembly. (`7f1e34d`, `ab94831`, `855c93b`, `b5528bb`, `1f67146`, `695f440`, `423811d`)
- **File dialog paths.** Last open/save locations tracked in `SystemSettings` instead of `Paths`. (`17175bf`)

### Bug Fixes

- **CPU / IRQ.** IRQ takes 7 cycles (was 6) with the missing phantom read on all CPU types — fixes Mad Effects 2; IRQ-pending reevaluated on reset. (`6b91d9c`, `be5c869`)
- **IIgs RAM sizing.** Correct sizing with 8MB memory expansion plus motherboard RAM (ROM 01 128K / ROM 03 1M). (`e0729c9`)
- **IWM / floppy.** Correct drive-enable model, empty-drive sense, and LSS clocking (#137). (`3d4efff`)
- **ADB / mouse.** ADB mouse cursor read from Event Manager bank globals (#135); ROM 03 mouse tracking fixed. (`d36c06c`, `90afdeb`)
- **Game controller / Joyport.** Floating GC switch behavior when no pad is connected (#143); Joyport suspend timing after reset; multi-gamepad startup fix. (`d79be3c`, `0e60636`, `5bd804a`, `cf94661`)
- **Ensoniq.** Round of DOC / SoundGlu fixes for misbehaving titles. (`6eefad5`)
- **Video soft switches.** Fixes to pass vsync and vidmodes tests (II+ / IIe solid; some GS switches still tentative). (`829bd5e`, `0d95d93`)
- **`.2mg` images.** Fixed offset error. (`ba91cf6`)
- **Host FST.** Fixed use-after-free in upstream `host_fst.c`. (`5bd8570`)
- **Trace / debugger.** Wrong effective address on `LDA [d,X]` fixed with an explicit R/W flag in the trace record; gamepad debug output no longer hardcodes `gps[0]`. (`9420cac`, `a359889`)

### Internals

- **BreakpointTable.** Dedicated class for breakpoint management in the debugger. (`16066ec`)
- **Default slotting.** Clock included by default on Enhanced IIe for A2Desktop; unenhanced IIe description corrected (no clock). (`b693acb`, `c95348f`)
- **Code cleanup / reuse.** Game-controller readability refactor; 6502/65816 trace sharing; misc dead-API and cleanup passes. (`cba9c84`, `1f67146`, `a73d3e3`, `80e02e1`)
- **Test tooling.** Python scripts for Ensoniq regression titles; arrow scancodes for input coverage. (`5283908`, `82ddc93`)
- **Docs / agent notes.** Host FST and driver docs, networking/SSC notes, expanded agent instructions. (`e1f62d6`, `d948aad`)

## 2026-05-19

Covers commits from 2026-05-01 through 2026-05-19.
```
### Features

- **3.5" floppy / WOZ support and IWM2 rewrite.** Added true 3.5" floppy WOZ read/write alongside the existing 5.25" path, with a major IWM2 rewrite that unifies the 3.5/5.25 floppy API. All WozTestImage test cases now boot and play. 3.5 writeback to WOZ and BLK images is complete; `.po` imports correctly into the internal WOZ representation. Head and read pointers gained 16× resolution, with `fast_forward` advance calculated from the mounted WOZ image (#115). Mount-time tests reject images incompatible with the target drive. (`91750ce`, `479c20d`, `13dd7bb`, `f34ea82`, `e0c6f15`, `7429b17`, `93a1539`, `29c6389`, `25be9e0`, `f3c834f`, `5d88290`)
- **Floppy source reorg and nibblizer refactor.** Moved all WOZ floppy code into `src/devices/floppy/` as its own library; eliminated the old `diskii_fmt` path in favor of `woz_nibblizer` classes; deleted legacy IWM, `ndiskii`, and `Floppy525` implementations; flattened the class tree by removing the redundant `FloppyDrive` wrapper. (`.nib` images are now read-only — lossy FF-as-8-bit conversion works for read but cannot be written back safely.) (`96539fa`, `913920d`, `77a89c2`, `77496fc`, `53bf070`)
- **BazFast hard drive (formerly pdblock3).** Renamed pdblock3 to BazFast. Added `.pmap` ("partition map") file support for mounting multiple volumes at once; `StorageDevice::mount` now accepts an array of `media_descriptor`. All 10 SmartPort units are registered. (`7f7091f`, `dc2b3b7`, `de8d525`, `2843e3f`, `9eda065`)
- **Drives HUD / Control Panel overhaul.** New AppleDisk 3.5 and HD20SC button types with matching atlas assets; `DrivesOSD` uses a height-efficient layout grid. Drive type is stored in each button so the HUD composes correctly — IIe and later show the AppleDisk icon, II+ and below show Disk II; a lone HD drive is centered. (`eae604e`, `ae9803b`, `2642468`, `5f10110`, `1014c86`, `a39151c`, `695a560`)
- **Modal dialog stack.** Implemented a modal dialog stack with window-centering; quit flow moved to a `QuitModal` that can chain into dirty-disk-save dialogs; dirty-disk save logic extracted into its own modal subclass, removing several layers of indirection from the main app event loop. (`9647640`, `bdbe63b`)
- **CLI platform auto-launch.** Added `-p PLATFORM` to launch directly into a configured platform and auto-quit when the window closes (#113). (`4f8b9d7`)

### User Interface

- **Debugger.** Enter/exit debug window is now bound to F10 to avoid conflicting with emulated keyboard layouts. (`5707dd2`)
- **System select.** Reduced CPU burn during the system-select screen by updating only when mouse-related events occur. (`90dc139`)
- **Drive activity indicator.** Active-after-access timeout restored to 1 second. (`1014c86`)

### Bug Fixes

- **Floppy / IWM2.** Fixed all known issues with the new floppy code and IWM2; see `IWM.md` and `Woz.md` for details. (`13dd7bb`)
- **Build.** Fixed a Windows build issue. (`68f1a45`)

### Internals

- **Disk controller API.** Controllers now expose `read`/`write` (instead of `read_cmd`) and take the register value rather than the full memory address. (`d1b61c1`, `eb3961f`, `ed6a946`)
- **Mount plumbing.** Added `PMap` utility class and extended `mount.cpp` for multi-volume BazFast mounts. (`dc2b3b7`)
- **Docs / notes.** Expanded `IWM.md`, `Woz.md`, and `Storage.md`; added Phasor card notes; updated Linux AppImage build instructions and copyright message. (`e21ea46`, `9eda065`, `bbce706`, `b3fd897`, `3e57e22`)

## 2026-04-30

Covers commits from 2026-03-25 through 2026-04-30.

### Features

- **WOZ disk image support (read + write).** Added full support for WOZ 1.0 / 2.0 disk images, including an internal floppy representation in WOZ format and a new `ndiskii_woz` device wired up for Apple IIe testing. Writing is now working and tested: disk-speed tests are correct, the vast majority of WOZ Test Suite images boot/work correctly, Locksmith can bit-copy a disk, and Copy II Plus can block-copy a disk without errors. Added the ability to write the internal WOZ image back out to the original block format. (`0a61790`, `02af752`, `b040e55`, `cc8ee63`)
- **`.img` disk image suffix support.** `.img` files are now accepted and treated the same as `.hdv`. (`98a5df5`)
- **Mockingboard 2.0 rewrite.** Mockingboard emulation was effectively rewritten: the 6522 and AY8910 are now broken out into their own classes, a new top-level Mockingboard class wraps them, and the C0xx interface layer is now a thin C-like shim. Added an L/R-to-mono decoupler that fixes chiptune phase-cancellation problems. The new implementation switches to cycle-by-cycle 6522 emulation and now passes all `mb-audit` 6522 tests (#94), including T1/T2 timer behavior validated against `irqtimetest` and the GS-IRQ test. (`abdc694`, `3a44ad5`, `7ef7f63`, `495370d`, `f749490`, `5be06ab`, `af14fff`)
- **Audio decorrelation.** New audio decorrelation feature in the `AudioSystem`, integrated with the AY8910s and exposed as a Settings menu toggle; ~5ms decorrelation was found to work well in practice. (`5d4e5b3`, `8631ef1`)
- **New Video Scan Generator (VSG) pipeline.** Introduced a new video model with `VSG_Comp`, `VSG_RGB`, and `VSG_Intf` classes, a new `FrameVSG` frame type, a small ring-buffer utility for HGR pixels, methods for multi-pixel insertion, and a companion `vpp2` test app. The render interface now takes a new VSG frame, GuS colors were replaced with Renée colors, and a 24-bit color cache keyed off the 12-bit values was added. (`52ea2a6`, `30be13c`, `7884597`, `64c729f`, `bdc21ef`, `9ff95d5`, `abb8e44`, `83e0d1d`, `56779f3`)
- **CPU event timer.** Added an `EventTimer` inside the IIe `NClock` with a minimal schedule/cancel API, used by the Mockingboard and by the computer to track reset assert/deassert times for a game-controller joyport fix. (`9659824`, `55583eb`, `ce1cdba`)
- **LORES7M display mode.** Implemented the LORES7M mode along with correct reset semantics in the display and game controller for the AN[0-3] switches and DBLRES. (`2479694`)
- **C021 register support.** Added (provisional) support for the C021 register and reimplemented the full-frame video update as a 17030-cycle loop. (`a975352`)
- **Full video register reset states.** Implemented all known video-register reset states for both //e and IIgs. (`4400238`)

### User Interface

- **Menu / input controls**
    - Right mouse button now temporarily accelerates the emulator to 14 MHz, with a matching on/off menu toggle. (`40f7819`, `aad9fe6`)
    - `INS` key triggers the same temporary speed-up as the right mouse button. (`578aa4d`)
    - Middle mouse click now toggles mouse capture (#109). (`4e149e5`)
    - Added "scrolly momentum" to scrolling UI. (`84fad77`)
- **Debugger UX**
    - Added buttons for step functions and reorganized the debugger layout. (`c30ced6`)
    - "Step over" on a non-`JSR`/`JSL` now behaves the same as single-step. (`6db8f03`)
    - Audio device info is now shown at the bottom of the debug view. (`5b59269`)
    - Display debug now shows hcounter/vcounter in hex. (`3bff817`)
    - Added a keyboard debug hook. (`96b9633`)
- **Keyboard handling**
    - Any code path that checks for a key-down now consumes the matching key-up, preventing leakage into the emulated keyboard. (`7c6629a`)
    - Reimplemented AKD (Any Key Down) by counting non-modifier key-up/key-down events. (`955d498`)
- **Disk mount feedback.** On mount failure, a user-visible heads-up message is now shown. (`1dfd029`)
- **Fullscreen rendering.** Fixed fullscreen; dropped `SDL_RenderSetScaleMode` in favor of rendering directly to the calculated window rect, with a new rect-calculation method that scales correctly for tall fullscreen. Also added a fake border area for Videx so it composes correctly. (`03e2ee7`, `1f05ec5`)

### Bug Fixes

- **CPU / 65816**
    - Fixed the same bad STA (ZP),Y phantom-address calculation on the 65816 that was previously fixed on the 6502. (`282d43d`)
    - Fixed a double-add in the (ZP),Y phantom read. (`f85562d`)
    - Fixed placement of the P-bit write in `RTI` on both 65816 and 6502. (`9372901`)
    - Fixed cycle-count errors for `dp,x`, `REP`/`SEP`, `PLD`/`PHD`, and `XBA` (validated against `textfunk` and `videomodes.po`). (`9987d17`)
    - `PEA` is 3 bytes, not 2. (`2c4c903`)
    - More accurate CPU interrupt handling: tightened up when IRQ fires and when IRQ-in is sampled; `irq_asserted` is now a `bool`. (`0ad763c`, `d8c5d56`)
- **Disk II / floppy**
    - Fixed a 525 floppy bug where the disk would sometimes load corrupted. (`8a5ff65`)
    - More thorough init/cleanup of variables on disk mount (remount). (`d4bf550`)
    - Odd reads now return floating bus. (`4a27e54`)
- **Soft switches / video**
    - Fixed broken `C01C` (page2 soft-switch status). (`3fc46e5`)
    - RGB display: Fixed double lores color selection on the aux byte. (`5f098aa`)
    - Fixed IIgs lores (was broken); IIgs does not force `C05E`. (`ae7c014`)
    - Screen capture was inadvertently scaling Videx output. (`9dd3964`)
    - Fixed the screen-grab path again. (`052d674`)
- **Mockingboard / 6522 / AY8910**
    - `IRA` is now set to `$FF` on reset and during bus cycles when inactive, reflecting Mockingboard pull-up behavior (passes all `mb-audit` tests). (`af14fff`)
    - Don't try to disassemble the `$C400` space (hack to stop stepping through Mockingboard ROM). (`8417765`)
- **Build**
    - Fixed Windows build issues. (`5790c4b`, `1428de8`)

### Internals

- **DiskII WOZ controller refactor.** Introduced a `sequencer_state` for LSS `READSHIFT` behavior, refactored `READSHIFT`/`READLOAD` handling, improved `data_register` updates, and added extensive comments on bit-cell accumulation and non-destructive reads. (`003ee45`, `f6b323c`)
- **Debug tooling.** Added DiskII and Floppy525 debug output and a method to directly `printf` a `DebugFormatter`; added a video-cycle callback queue; moved the debug emitter into the clock and fixed `ram_refresh` timing. (`492dff0`, `b54f0dd`, `ddb64c3`)
- **Clock / event timer plumbing.** `NClock` became a forward declaration where possible; it now uses the `EventTimer` internally; the IRQ trigger cycle number is cached for debug. (`da3285c`, `55583eb`, `9659824`)
- **Test harness.** Improved the test harness, added 6522 regression tests, added a 65816-specific cycle test and fixed cycle-delta display, and disabled tests that are no longer used. (`680896d`, `495370d`, `76d84c3`, `36f395e`)
- **Build / dependencies.** imgui source is now included directly in-tree instead of relying on a system-provided library; CMakeLists always copies resources on build to avoid stale files; `pdblock3` ROM updated to jump back into `SLOOP` so other devices get a chance to boot. (`9786602`, `0728ef6`)
- **Slow-cycle / Mega II timing.** Added a couple of missing `slow_cycle` cases for Mega II access. (`d727e78`)
- **Video pipeline plumbing.** Video no longer calls back out to clear page2 (it handles it itself); page2 is now cleared directly on reset; fixed placement of hsync/vsync and emission of blank cycles. (`f548d4a`, `bb6c771`, `e3cac80`, `8605944`)
- **Dead code / cleanup.** Multiple large passes of dead-code removal and cleanup, including removing the `full_frame_redraw` flag, the `vid_timer`, unused variables, and the deprecated pre-split Mockingboard class; dropped `scale_x/y` from `VideoSystem`; miscellaneous tidy-ups. (`c11203e`, `504df75`, `0dd5820`, `26d039f`, `cfffd4c`, `f016952`, `6454450`, `e8546d3`, `dbe8bf4`, `88f2339`, `5e38fc4`, `48719f9`)
- **Misc / docs / notes.** Docs added for `.img` files and the virtual modem; various notes files committed. (`625e49c`, `8567ace`, `d4affc3`, `8917833`, `acacf59`, `66f5052`, `5ae6d71`, `1d7d784`, `19e3a69`, `7f5d73f`)
- **Submodule debug integration.** Tied in submodule debug and suppressed `generate_frame` when in step mode. (`011d012`)

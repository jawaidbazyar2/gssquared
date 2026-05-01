# Changelog

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

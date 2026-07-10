# GS2 Change Log — Release 0.9

Covers development from **2026-05-19** through **2026-07-09** (85 commits).

Release 0.9 focuses on configurable hardware profiles (including an in-app editor), GS/OS mouse tracking, CRT post-processing effects, SecondSight card emulation, and groundwork for a web build. The 0.9 roadmap items are complete.

---

## Major Changes

### TOML and Settings.txt configuration

A new configuration system replaces ad-hoc platform setup with declarative config files.

- **TOML config files (`.gs2`).** Full parsing, loading, and execution of machine profiles: platform, slots, storage, video, and aliases. Integrated into the main app and platform menus. Documented in `SystemConfigTOML.md` and `ConfigFiles.md`.
- **Settings.txt loader.** Reader/loader for Mike's legacy `Settings.txt` format, with conversion into the new system config layer.
- **Test harness.** `apps/systemconfigtest` with fixture files for both TOML and Settings.txt validation.

### Config editor and custom system profiles

In-app create / edit / save / launch of custom hardware configs — the remaining 0.9 roadmap item.

- **Edit System Configuration.** New editor UI (`EditSystem`, `ConfigDraft`) for name, description, platform, slot cards, and pre-mounted storage. Opened from System Select via **+** (new) or **Edit...** (existing `.gs2` / Settings.txt).
- **Dynamic slots and drives UI.** `SlotsPanel` and `StorageButtonFactory` build slot/drive buttons from the live config; OSD button constructors cleaned up around asset IDs.
- **Launch Config…** Menu item (macOS / Windows / Linux) loads a config and boots immediately from System Select (renamed from "Open Config…"). Also supports drag/drop and Open With for profile packs.
- **macOS `.gs2` document type.** Info.plist UTI + UTI-aware save dialog so `.gs2` files are recognized in Finder and save/open panels (not grayed out).
- **Device metadata.** Card placement rules centralized in `device_info` (`slots_allowed`, `multipleInstances`, platform flags); editor and config load reject duplicate cards when not allowed.
- Documented in `ConfigEditor.md`.

### GS/OS mouse tracking (KeyGloo)

Mouse synchronization for GS/OS via the KeyGloo ADB path.

- Content-area tracking: the display pipeline now computes and remembers the inset "content" rectangle so mouse coordinates map correctly when the CRT bezel or letterboxing is present.
- Closed-loop injection support for accurate pointer sync without writing into guest memory.
- Helper routines to detect when the Control Panel or a modal dialog is open, so mouse handling can defer appropriately.

### CRT GPU shader pipeline

Post-processing CRT effects rendered on the GPU.

- New GPU shader pipeline with HLSL and Metal backends; shader source loaded from files at runtime (`assets/shaders/`).
- Resolution-aware parameter scaling; option to enable the shader at boot with fallback when the GPU renderer is unavailable.
- Documented in `Displays.md`.

### SecondSight card emulation

Initial emulation of the SecondSight VGA card for Apple IIgs.

- Core device with debug handler and nearly complete API command set (except `run_code`).
- VGA text mode (9×16) and 8/16/24-bit graphics renderers, refactored into separate files.
- PPU (Picture Processing Unit) mode with a Mario test harness (`apps/ssppu`).
- Handshake timing, multibank framebuffer upload, and interleaved char/attribute data matching real VGA layout.

### Emscripten / web build (proof of concept)

Initial work toward running GS2 in the browser.

- CMake and build scaffolding for Emscripten.
- Web shell HTML, local serve script, and a web file-dialog implementation.
- Documented in `Emscripten.md`; PPU mode spec added in `PPU.md`.

### High-DPI display support

- Retina / high-DPI mode with correct scaling of the OSD and ImGui elements.
- Display metrics logging; borderless-window fullscreen instead of exclusive mode.

---

## Bug Fixes

| Issue | Description |
|-------|-------------|
| **#119** | `cold_start` now propagates from `computer::reset` to all registered reset handlers; KeyGloo forces ADB Micro RAM `0x51` to 0 so the GS ROM runs its power-on reset routine. |
| **#120** | Renamed `modal_stack` to `mstack` to avoid name conflicts. |
| **#121** | AudioSystem now reports the correct SpeakerFX volume setting; plumbing added for audio device change events. |
| **#122** | On audio device change, clear the audio buffer to prevent a runaway "too much data" reset loop. |
| **#127** | Ensoniq `soundctl` register: low 4 bits are write-only and now return `0xF` on read. |
| **#129** | KeyGloo mouse sync detects 320-mode more accurately. |
| **#102, #107** | Ensoniq interrupt handling and change-during-frame behavior corrected. |
| — | Fixed DHGR regression introduced by the II+ delay-color fix in `VideoScannerII`. |
| — | II/II+ video scanner now ignores the `dblres` flag (not applicable on those machines). |
| — | Fixed PDBLOCK3 ROM checksum write using `,X` instead of `,Y`, which stomped guest memory. |
| — | Fixed double-lo-res switch display label. |
| — | `Floppy525_woz` phase-settle events use a unique `instanceID` so concurrent Disk II and IWM drives do not collide on a shared timer. |

---

## Other Improvements

### Audio

- Audio device change events routed through `AudioSystem`; buffer management on hot-plug.

### Video / display

- `cycle_handler` support added to `clock_iigs` for tighter timing integration.
- Drive HUD: AppleDisk icon on IIe and later, Disk II on II+ and below; fractional track display made more readable.

### UI / UX

- Drive icons repositioned slightly in the HUD.
- System badge and related Select System polish for custom configs.

### Documentation

- New or expanded docs: `ConfigFiles.md`, `SystemConfigTOML.md`, `ConfigEditor.md`, `SecondSight.md`, `Emscripten.md`, `PPU.md`, `ExternalDebugInterface.md`, `MemoryProtection.md`, `Mouse.md`, `CompatibilityGS.md`.
- Ongoing developer notes in `DevelopLog.md` and roadmap updates; **0.9 roadmap marked complete**.

### Internals / tooling

- `apps/vgatext` shader and VGA text rendering test harnesses with ARM NEON optimizations.
- Deprecated KeyGloo mouse-sync routines removed after the closed-loop rewrite.
- macOS agent build notes: prefer `cmake --build … --parallel`.
- Version bumped to **0.8** on 2026-05-19 (`CMakeLists.txt`); 0.9 features landed on top of that baseline.

---

## Commit Timeline (summary)

| Period | Theme |
|--------|-------|
| **May 19–21** | 0.8 release baseline; drive HUD polish; audio and reset bug fixes (#119–#122) |
| **May 26–Jun 7** | SecondSight card emulation, VGA renderers, PPU mode |
| **Jun 26–29** | Emscripten PoC; high-DPI; Ensoniq fixes |
| **Jun 30–Jul 4** | CRT GPU shaders; KeyGloo mouse sync |
| **Jul 6–7** | TOML config system; Settings.txt loader; config documentation; #129 |
| **Jul 9** | Config editor UI; `.gs2` UTI / Launch Config; device_info; multi-drive floppy events; 0.9 roadmap complete |

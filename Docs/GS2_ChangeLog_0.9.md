# GS2 Change Log — Release 0.9

Covers development from **2026-05-19** through **2026-07-07** (67 commits).

Release 0.9 focuses on configurable hardware profiles, GS/OS mouse tracking, CRT post-processing effects, SecondSight card emulation, and groundwork for a web build.

---

## Major Changes

### TOML and Settings.txt configuration

A new configuration system replaces ad-hoc platform setup with declarative config files.

- **TOML config files (`.gs2`).** Full parsing, loading, and execution of machine profiles: platform, slots, storage, video, and aliases. Integrated into the main app and platform menus. Documented in `SystemConfigTOML.md` and `ConfigFiles.md`.
- **Settings.txt loader.** Reader/loader for Mike's legacy `Settings.txt` format, with conversion into the new system config layer.
- **Test harness.** `apps/systemconfigtest` with fixture files for both TOML and Settings.txt validation.

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
| **#102, #107** | Ensoniq interrupt handling and change-during-frame behavior corrected. |
| — | Fixed DHGR regression introduced by the II+ delay-color fix in `VideoScannerII`. |
| — | II/II+ video scanner now ignores the `dblres` flag (not applicable on those machines). |
| — | Fixed PDBLOCK3 ROM checksum write using `,X` instead of `,Y`, which stomped guest memory. |
| — | Fixed double-lo-res switch display label. |

---

## Other Improvements

### Audio

- Audio device change events routed through `AudioSystem`; buffer management on hot-plug.

### Video / display

- `cycle_handler` support added to `clock_iigs` for tighter timing integration.
- Drive HUD: AppleDisk icon on IIe and later, Disk II on II+ and below; fractional track display made more readable.

### UI / UX

- Drive icons repositioned slightly in the HUD.

### Documentation

- New or expanded docs: `ConfigFiles.md`, `SystemConfigTOML.md`, `SecondSight.md`, `Emscripten.md`, `PPU.md`, `ExternalDebugInterface.md`, `MemoryProtection.md`, `Mouse.md`, `CompatibilityGS.md`.
- Ongoing developer notes in `DevelopLog.md` and roadmap updates for 0.9 milestones.

### Internals / tooling

- `apps/vgatext` shader and VGA text rendering test harnesses with ARM NEON optimizations.
- Deprecated KeyGloo mouse-sync routines removed after the closed-loop rewrite.
- Version bumped to **0.8** on 2026-05-19 (`CMakeLists.txt`); 0.9 features landed on top of that baseline.

---

## Commit Timeline (summary)

| Period | Theme |
|--------|-------|
| **May 19–21** | 0.8 release baseline; drive HUD polish; audio and reset bug fixes (#119–#122) |
| **May 26–Jun 7** | SecondSight card emulation, VGA renderers, PPU mode |
| **Jun 26–29** | Emscripten PoC; high-DPI; Ensoniq fixes |
| **Jun 30–Jul 4** | CRT GPU shaders; KeyGloo mouse sync |
| **Jul 6–7** | TOML config system; Settings.txt loader; config documentation |

# GSSquared feature catalog

A catalog of **user-visible** capabilities, **host integrations**, and **emulator-specific** behavior. It is not a list of normal Apple II hardware fidelity (see baseline below).

**Audience:** casual users and power users (custom configs, debug protocol, automation).

**Maintainers:** table columns (`id`, `hosts`, `verify`, `automation`) are designed so a release checklist or automated tests can be filtered from this doc later. See [Suggested smoke tiers](#appendix-suggested-smoke-tiers).

---

## How to read this document

### Status

| Value | Meaning |
|-------|---------|
| Complete | Shipped and intended for regular use |
| Partial | Usable with known gaps |
| Planned | Documented intent; not shipped or incomplete |
| By choice (omitted) | Intentionally out of scope |

### Catalog table columns

| Column | Purpose |
|--------|---------|
| `id` | Stable slug for tests and checklists (do not reuse when behavior changes) |
| `feature` | Short name |
| `status` | See above |
| `hosts` | `all`, `macOS`, `win`, `linux`, `web`, or combinations |
| `preconditions` | Machine state, platform, or hardware needed |
| `verify` | Observable confirmation (human or automated) |
| `automation` | `manual`, `cli`, `debug-socket`, or `both` (cli + debug-socket) |
| `notes` | Deviations, limits, links |

### Inclusion rubric (what belongs here)

Include host integration, GSSquared workflow (System Select, menus, OSD, speed, shaders), automation surfaces, per-device **user** capabilities (formats, mounts, attachments), and deviations from real hardware.

Exclude generic “Apple II has text mode” fidelity unless GSSquared adds an angle (engine choice, optional card, known limit).

### Fidelity baseline

GSSquared models standard Apple II / IIgs behavior for supported machines and video modes. For CPU, memory, and mode matrices see [Documentation.md](Documentation.md).

---

## At a glance

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `platform.host.macos` | macOS app bundle (DMG) | Complete | macOS | — | Launch `.app`; ROMs/assets included | manual | [README.md](../README.md) |
| `platform.host.win` | Windows portable ZIP | Complete | win | — | Run `GSSquared.exe` | manual | MSYS2 build; no installer required |
| `platform.host.linux` | Linux AppImage | Complete | linux | — | Run AppImage | manual | |
| `platform.host.web` | Browser WebAssembly build | Complete | web | COOP/COEP server | Load `/live` or local `serve.py` | manual | [Web.md](Web.md), [Emscripten.md](Emscripten.md) |
| `machine.apple2` | Apple ][ | Complete | all | — | Boot built-in tile or `-p 0` | cli | |
| `machine.apple2plus` | Apple II Plus | Complete | all | — | Boot tile or `-p 1` | cli | |
| `machine.apple2e` | Apple IIe | Complete | all | — | `-p 2` | cli | |
| `machine.apple2e-enh` | Apple IIe Enhanced | Complete | all | — | `-p 3` | cli | |
| `machine.apple2e-65816` | Apple IIe with 65816 | Complete | all | — | `-p 4` | cli | Optional VIDHD card in custom configs |
| `machine.apple2gs` | Apple IIgs (ROM 01 / ROM 03 tiles) | Complete | all | — | `-p 5`; pick ROM tile | cli | Separate ROM 01 vs ROM 03 platforms |
| `diff.8-and-16-bit` | 8-bit and IIgs in one app | Complete | all | — | Switch machines via System Select | manual | |
| `diff.ready-to-run` | ROMs and assets bundled | Complete | all | — | No separate ROM download | manual | |

---

## Getting started and machines

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `start.system-select` | System Select tile row | Complete | all | App launch | “Choose your retro experience” | manual | [Select.md](Select.md) |
| `start.custom-plus` | Create custom machine (+ tile) | Complete | all | — | Opens config editor | manual | [ConfigEditor.md](ConfigEditor.md) |
| `start.recent-configs` | Recent custom config tiles | Complete | all | Prior `.gs2` use | Tile appears after open | manual | `Documents/GSSquared/` |
| `start.launch-config-menu` | File → Launch Config… | Complete | all | Machine off | Picks `.gs2` / Settings.txt; boots | manual | |
| `start.launch-config-arg` | CLI: config path argument | Complete | all | — | `GSSquared Foo.gs2` skips selector | cli | Closes to quit, not selector; [CommandLine.md](CommandLine.md) |
| `start.cli-platform` | CLI: `-p N` auto-launch | Complete | all | — | `get_status` → `platform_id` | both | `-p 0`…`5`; [CommandLine.md](CommandLine.md) |
| `start.cli-mount` | CLI: `-dsXdY=path` mount | Complete | all | Config with slot X | Disk mounted at boot; `MOUNT` protocol | both | Overrides same slot/drive in `.gs2`; [CommandLine.md](CommandLine.md) |
| `start.cli-sleep` | CLI: `-s` sleep vs busy-wait | Complete | all | — | Lower CPU when idle | cli | Same as Settings → Sleep/Busy Wait; [CommandLine.md](CommandLine.md) |
| `start.cli-crt-boot` | CLI: `-g` CRT shader at boot | Complete | all | Native GPU | Shader on when guest starts | cli | Same as F7 at boot; [CommandLine.md](CommandLine.md) |
| `start.cli-debug-socket` | CLI: `--debug PATH` | Complete | macOS, linux, win | AF_UNIX path | `HELLO` on socket | debug-socket | [CommandLine.md](CommandLine.md), [DebugProtocol.md](DebugProtocol.md) |
| `start.cli-no-quit-confirm` | CLI: `--no-quit-confirm` | Complete | all | — | SIGTERM exits without Quit modal | cli | For harnesses; [CommandLine.md](CommandLine.md) |
| `start.power-off-on` | Power off / on virtual machine | Complete | all | — | System Select vs running | manual | File → Close Emulation; [Select.md](Select.md) |
| `start.shipped-gs2-seed` | Default `.gs2` copied to Documents | Complete | macOS, win, linux | First run | Files in `Documents/GSSquared/` | manual | Migration from old prefs path |

---

## Emulator workflow (menus, OSD, hover)

Native menus: [Menus.md](Menus.md). Control Panel / hover strip / drives: [OSD.md](OSD.md). CLI flags: [CommandLine.md](CommandLine.md).

### File

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.file.new-disk-525-unfmt` | New Disk Image → 5.25 Unformatted | Complete | macOS, win, linux | — | `.woz` on disk; not auto-mounted | manual | [BlankFloppy.md](BlankFloppy.md) |
| `workflow.file.new-disk-525-dos33` | New Disk → 5.25 DOS 3.3 | Complete | macOS, win, linux | — | Formatted template file | manual | |
| `workflow.file.new-disk-525-prodos` | New Disk → 5.25 ProDOS | Complete | macOS, win, linux | — | Formatted template | manual | |
| `workflow.file.new-disk-35-prodos` | New Disk → 3.5 ProDOS | Complete | macOS, win, linux | — | 800K template | manual | |
| `workflow.file.new-disk-hdv-empty` | New Disk → 32M HD unformatted | Complete | macOS, win, linux | — | 32M `.hdv` zeros | manual | |
| `workflow.file.new-disk-hdv-prodos` | New Disk → 32M HD ProDOS | Complete | macOS, win, linux | — | Formatted 32M volume | manual | |
| `workflow.file.drives-submenu` | File → Drives (per-drive mount) | Complete | all | Emulation running | Same as OSD mount | manual | Dynamic drive list; [Menus.md](Menus.md) |
| `workflow.file.open-disk` | File → Drives mount picker | Complete | all | Emulation running | Mount via picker | manual | Same as Drives submenu (no “Open Disk Image” item) |
| `workflow.file.open-system` | Launch Config / System Select | Complete | all | Machine off | Loads saved config | manual | Not a File → Open System item; [Select.md](Select.md) |
| `workflow.file.save-system` | Config editor Save / Save As | Complete | all | — | `.gs2` written; mounts only | manual | No CPU/RAM state; [ConfigEditor.md](ConfigEditor.md) |
| `workflow.file.mount-drivers` | File → Mount Drivers | Complete | all | IIgs + BazFast + free drive | `/GS2.DRIVERS` RO volume | manual | [Storage.md](Storage.md) |
| `workflow.file.screenshot` | File → Save Screenshot | Complete | macOS, win, linux | — | PNG on Desktop | manual | Shift+PrintScreen |
| `workflow.file.close-emulation` | File → Close Emulation | Complete | all | Machine running | Returns to System Select | manual | Win: required before Quit |
| `workflow.file.quit` | Quit application | Complete | all | Win: machine off | Process exits | manual | Dirty-disk prompts unless `--no-quit-confirm` |

### Edit

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.edit.copy-screen` | Copy Screen to clipboard | Complete | all | — | Paste image in host app | manual | PrintScreen |
| `workflow.edit.paste-text` | Paste Text into guest | Complete | all | II / IIe / IIgs ADB | Characters appear; one/frame | both | Shift+Insert; `PASTE_TEXT` |

### Machine

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.machine.reset` | Reset (Ctrl-Reset) | Complete | all | Running | Guest reboot | both | `reset` / key injection |
| `workflow.machine.restart` | Restart (Ctrl-OA-Reset) | Complete | all | Running | Hard reset | both | |
| `workflow.machine.pause` | Pause / Resume emulator | Complete | all | — | Execution frozen | both | `pause` / `continue_exec` |
| `workflow.machine.capture-mouse` | Capture Mouse | Complete | all | — | Relative mouse mode | manual | [Mouse.md](Mouse.md) |

### Settings

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.settings.speed` | System Speed presets | Complete | all | — | 1.0 / 2.8 / 7.1 / 14.3 / Unlimited | manual | Menu is 1.0–14.3; Unlimited via F9 / OSD / hover; [Menus.md](Menus.md) |
| `workflow.settings.sleep` | Sleep / busy-wait | Complete | all | — | CPU usage changes | cli | `-s` |
| `workflow.settings.gamepad-mode` | Game Controller modes | Complete | all | — | Joyport / mouse-as-stick / gamepad | manual | [Joysticks.md](Joysticks.md) |
| `workflow.settings.joyport-select` | Joyport Controller Select L/C/R | Complete | all | Joyport mode | AN0 behavior | manual | |
| `workflow.settings.no-gamepad-float` | Disconnected When No Gamepad | Complete | all | — | Paddles float vs centered | manual | Total Replay visibility |
| `workflow.settings.modifier-swap` | OA/CA modifier mapping swap | Planned | all | — | — | — | No host menu; mappings are fixed in [KeyboardShortcuts.md](KeyboardShortcuts.md) |
| `workflow.settings.mono-helper` | Mono Helper (audio) | Complete | all | — | Menu toggle | manual | Settings → Mono Helper; [Menus.md](Menus.md) |
| `workflow.settings.rmb-accel` | Right Mouse Button Accelerate | Complete | all | — | Faster CPU while RMB held | manual | Settings menu; [Mouse.md](Mouse.md) |

### Keyboard shortcuts (emulator)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `input.shortcut.f1-release-mouse` | F1 release captured mouse | Complete | all | Capture on | Cursor free | manual | [KeyboardShortcuts.md](KeyboardShortcuts.md) |
| `input.shortcut.f2-display-cycle` | F2 cycle display engine | Complete | all | — | Composite/RGB/mono | manual | |
| `input.shortcut.f3-fullscreen` | F3 fullscreen toggle | Complete | all | — | Window borderless | manual | |
| `input.shortcut.f4-osd` | F4 Control Panel | Complete | all | — | Panel opens | manual | |
| `input.shortcut.f5-ntsc-mode` | F5 pixel-blur vs rectangular | Complete | all | — | Picture changes | manual | Scaling, not NTSC vs legacy; [Displays.md](Displays.md) |
| `input.shortcut.ctrl-f5-filter` | Ctrl+F5 linear vs nearest | Planned | all | — | — | — | Not implemented; F5 is the scale-mode toggle |
| `input.shortcut.f6-joyport-cycle` | F6 joystick mode cycle | Complete | all | — | Joyport/gamepad/mouse | manual | |
| `input.shortcut.f7-crt` | F7 CRT shader | Complete | all | — | Shader toggles | manual | |
| `input.shortcut.f9-speed` | F9 / Shift+F9 speed | Complete | all | — | Speed changes | manual | |
| `input.shortcut.f10-debugger` | F10 debugger | Complete | all | — | Debugger window | manual | |
| `input.shortcut.gs-french-layout` | IIgs French AZERTY via guest CP | Complete | all | Guest set to French | French characters | manual | Host menu does not pick layout |

### Display and HUD

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `display.engine.composite` | Composite (NTSC) monitor | Complete | all | — | Color artifacting | manual | Cycle-accurate path |
| `display.engine.rgb` | GS RGB monitor | Complete | all | IIgs default RGB | Exact GS palette mapping | manual | [Displays.md](Displays.md) |
| `display.engine.mono` | Monochrome green/amber/white | Complete | all | — | HUD + menu switch | manual | |
| `display.hud.stats` | HUD → Stats overlay | Complete | all | — | FPS/stats visible | manual | Off by default |
| `display.hud.drives` | HUD → Drives strip | Complete | all | — | Activity strip | manual | On by default |
| `display.fullscreen` | Full Screen | Complete | all | — | Borderless window | manual | |
| `display.second-sight-text` | Second Sight Text mode | Complete | all | IIgs + Second Sight | Menu enabled | manual | [Cards_SecondSight.md](Cards_SecondSight.md) |
| `display.crt-shader` | CRT GPU shader | Complete | all | Native GL/D3D12 | F7 toggles | cli | `-g` at boot; [Displays.md](Displays.md) |
| `display.hover-controls` | Hover speed/display widgets | Complete | all | Mouse not captured | Face buttons + pickers | manual | Automatic left strip; [OSD.md](OSD.md) |
| `display.pal-timing` | PAL video timing | Complete | all | PAL `.gs2` / clock | 50 Hz behavior | manual | [Displays.md](Displays.md), [ConfigFiles.md](ConfigFiles.md) |
| `display.floating-bus` | Floating-bus reads | Complete | all | — | Demoscene titles | debug-socket | [Displays.md](Displays.md) |
| `display.cycle-accurate-video` | Cycle-accurate mode switches | Complete | all | — | Titles that count cycles | manual | Frame-organized CPU; [Displays.md](Displays.md) |

### Sound menu

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `audio.menu.fx-toggle` | Drive seek sound effects | Complete | all | — | Drive seek sounds | manual | Always on; stereo pan by drive; no Sound menu |
| `audio.menu.volume` | Master volume | By choice (omitted) | all | — | — | — | Use host OS volume; no in-app slider |
| `audio.menu.mb-decorrelate` | Mockingboard decorrelation | Complete | all | Mockingboard | Wider stereo image | manual | Settings → Mono Helper |

### Debug menu and OSD

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `debug.ui.window` | Debugger window (F10 / OSD) | Complete | all | — | ImGui debugger opens | manual | [UsingTheDebugger.md](UsingTheDebugger.md) |
| `debug.trace-on-quit` | Auto instruction trace on quit | Complete | macOS, win, linux | — | `gssquared-trace.bin` in Documents | manual | |
| `debug.osd-button` | OSD Debug button | Complete | all | Hover/OSD visible | Same as F10 | manual | |

### OSD / Control Panel (F4)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.osd.open` | Open Control Panel | Complete | all | — | F4 or tab button | manual | |
| `workflow.osd.mount-drive` | Mount/unmount via drive icons | Complete | all | — | Picker; dirty save prompt | both | `MOUNT` / `UNMOUNT` |
| `workflow.osd.drag-drop-disk` | Drag disk image onto window | Complete | all | — | Panel opens; drop on drive | manual | |
| `workflow.osd.serial-buttons` | Serial/parallel attachment UI | Complete | all | Ports present | File/modem/clipboard/serial | manual | [SerialConnections.md](SerialConnections.md) |
| `workflow.osd.host-folder` | Host Folder… (Host FST) | Complete | macOS, win, linux | IIgs running | `:Host` path changes | manual | Not on web |
| `workflow.osd.slot-view` | Slot card display | Complete | all | — | Cards listed | manual | Editable when configuring |

### Docs menu

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `workflow.docs.online` | Online Documentation | Complete | all | Network | Browser opens docs | manual | [Menus.md](Menus.md) |
| `workflow.docs.donate` | Donate link | Complete | all | Network | Browser opens | manual | |
| `workflow.docs.check-updates` | Check For Updates | Complete | all | Network | Releases page | manual | Docs menu |

---

## Input (host mapping and emulation choices)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `input.keyboard.iiplus` | II+ keyboard mapping | Complete | all | II+ platform | Keys match doc | manual | No REPT key; host autorepeat |
| `input.keyboard.iie` | IIe keyboard (incl. Delete→0x7F) | Complete | all | IIe | OA/CA mapped | manual | [KeyboardShortcuts.md](KeyboardShortcuts.md) |
| `input.keyboard.gs-adb` | IIgs ADB keyboard | Complete | all | IIgs | GS/OS input | manual | Full intl keyboard: Planned |
| `input.deviation.iiplus-hard-reset` | Ctrl-OA-Reset on II+ | Complete | all | II+ | Hard reset works | manual | Not on real II+ game ports |
| `input.mouse.gs-tracking` | GS/OS mouse tracking | Complete | all | IIgs | Pointer moves | manual | ADB path |
| `input.mouse.card-iie` | Apple Mouse III card | Complete | all | Card in slot | [Cards_AppleMouse.md](Cards_AppleMouse.md) | manual | |
| `input.joystick.gamepad` | USB gamepad as joystick | Complete | all | — | Paddle/button lines | manual | |
| `input.joystick.mouse-as-stick` | Mouse emulates joystick | Complete | all | — | Stick from mouse | manual | |
| `input.joystick.sirius-joyport` | Sirius / Atari Joyport (F6) | Complete | all | — | Two sticks via AN0 | manual | [Joyport.md](Joyport.md) |

---

## Storage and disk images

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `storage.writeback-unmount` | Buffered writes until unmount | Complete | all | Writable image | Save/discard dialog | manual | Host workflow |
| `storage.drag-drop` | OS drag-drop onto emulator | Complete | all | — | Mount on chosen drive | manual | Web: in-memory only |
| `storage.smartport-panel` | BazFast / SmartPort drive panel | Complete | all | bazfast3 card | Six drive icons | manual | “Hard drive” CP |
| `storage.iso-apm` | ISO with APM → multiple units | Complete | all | ISO + APM | Partitions mount | manual | Read-only ISO |
| `storage.pmap` | Partition map sidecar mount | Complete | all | `.pmap` | Multiple HD images | manual | [Storage.md](Storage.md) |
| `storage.format.woz21` | WOZ 2.1 images | Planned | all | — | — | — | [Storage.md](Storage.md) |

### Supported image formats (summary)

| Format | R/W | Typical size | `id` | notes |
|--------|-----|--------------|------|-------|
| `.do` / `.dsk` | RW | 140K | `storage.format.dsk` | |
| `.po` | RW | 140K | `storage.format.po` | |
| `.nib` | RO | 140K | `storage.format.nib` | No safe write path |
| `.woz` 1.0/2.0 | RW | 140K/800K | `storage.format.woz` | Copy protection, half/quarter tracks |
| `.2mg` | RW | varies | `storage.format.2mg` | |
| `.hdv` / `.img` / `.hda` | RW | any | `storage.format.block` | Block devices |
| `.iso` | RO | any | `storage.format.iso` | |

WOZ-focused floppy emulation speeds with host CPU throttle (disk “runs faster” in fast modes).

---

## Serial, parallel, and networking

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `conn.file-capture` | Serial/parallel → file | Complete | macOS, win, linux | Port | Bytes in file | manual | |
| `conn.clipboard` | Serial/parallel → clipboard | Complete | macOS, win, linux | Port | Text on host clipboard | manual | Strip bit 7; CR→LF |
| `conn.modem` | Hayes modem (SCC / SSC) | Complete | macOS, win, linux | Not web | TCP via SDL_net | manual | [Documentation.md](Documentation.md); web: `web.no-modem` |
| `conn.host-serial` | Attach host UART | Complete | macOS, win, linux | `device=serial` | Data to real port | manual | [SerialPortSpec.md](SerialPortSpec.md) |
| `conn.gs-scc-ab` | IIgs built-in SCC ports A/B | Complete | all | IIgs | OSD buttons slot 1/2 | manual | |
| `net.uthernet2-slirp` | Uthernet II via SLIRP | Complete | macOS, win, linux | Card + driver | TCP/IP in guest | manual | [Cards_UthernetII.md](Cards_UthernetII.md) |

`.gs2` `[[connections]]`: `file`, `modem`, `echo`, `clipboard`, `serial`, `none` — [SystemConfigTOML.md](SystemConfigTOML.md).

---

## Audio (emulator angles)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `audio.speaker.fx` | Drive/speaker sound effects | Complete | all | FX on | Seek clicks | manual | Filtered speaker path |
| `audio.ensoniq` | IIgs Ensoniq DOC | Complete | all | IIgs | Sound in GS titles | manual | [Documentation.md](Documentation.md) |
| `audio.mockingboard` | Mockingboard AY/6522 | Complete | all | Card(s) | mb-audit; games | manual | [Cards_Mockingboard.md](Cards_Mockingboard.md) |
| `audio.ensoniq-stereo` | Stereo when guest uses stereo | Complete | all | IIgs | Pan in headphones | manual | Mono → both channels |

---

## Time, NVRAM, and configuration

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `config.editor` | Visual config editor (+ tile) | Complete | all | — | Slots, disks, connections | manual | [ConfigEditor.md](ConfigEditor.md) |
| `config.gs2-toml` | `.gs2` TOML on disk | Complete | all | — | Load/save round trip | manual | [SystemConfigTOML.md](SystemConfigTOML.md) |
| `config.bram-in-gs2` | IIgs battery RAM in `.gs2` | Complete | all | IIgs ran | `bram=` hex on close | manual | Replaces sidecar `.bram` import |
| `config.neil-settings` | Import Neil Settings.txt | Complete | all | — | Opens in editor / launch | manual | |
| `clock.thunderclock` | Thunderclock Plus card | Complete | all | Card | ProDOS time / TIME SET | manual | [Cards_Clock.md](Cards_Clock.md) |
| `clock.prodos-generic` | Generic ProDOS clock card | Complete | all | Card | Read-only time | manual | [Cards_Clock.md](Cards_Clock.md) |
| `clock.gs-rtc` | IIgs realtime clock | Complete | all | IIgs | Matches host TZ | manual | [ConfigFiles.md](ConfigFiles.md) |
| `state.save-restore` | Full machine save states | Planned | all | — | — | — | Design: [SaveAndRestore.md](SaveAndRestore.md); not shipped |

---

## Debugger, tracing, and automation

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `debug.protocol.hello` | Debug protocol handshake | Complete | macOS, linux, win | `--debug PATH` | `HELLO` reply | debug-socket | [DebugProtocol.md](DebugProtocol.md) |
| `debug.protocol.run-control` | Pause / step / continue | Complete | macOS, linux, win | Socket | `EVT_STOPPED` | debug-socket | |
| `debug.protocol.mem-regs` | READMEM / WRITEMEM / GET_REGS | Complete | macOS, linux, win | Socket | Bytes match | debug-socket | |
| `debug.protocol.breakpoints` | Breakpoints | Complete | macOS, linux, win | Socket | Stop at PC | debug-socket | |
| `debug.protocol.keys` | KEYEVENT / PASTE_TEXT | Complete | macOS, linux, win | Socket | Guest sees input | debug-socket | |
| `debug.protocol.video-text` | VIDEO_TEXT snapshot | Complete | macOS, linux, win | Socket | 40-col text bytes | debug-socket | Not bitmap |
| `debug.protocol.mount` | MOUNT / UNMOUNT | Complete | macOS, linux, win | Socket | `status` code | debug-socket | |
| `debug.protocol.state-get` | STATE_GET (e.g. Ensoniq) | Complete | macOS, linux, win | Socket | Device blob | debug-socket | |
| `debug.python-client` | In-tree Python `gs2debug` | Complete | macOS, linux, win | PYTHONPATH | Examples run | debug-socket | [gs2debug.md](gs2debug.md) |
| `debug.mcp-server` | Go MCP sidecar (external) | Complete | macOS, linux, win | Separate binary | MCP tools → protocol | debug-socket | Not in app bundle; [UsingTheDebugger.md](UsingTheDebugger.md), [McpServer.md](McpServer.md) |
| `debug.tracing-ui` | CPU tracing in debugger | Complete | all | — | Trace panes | manual | [Tracing.md](Tracing.md) |

Use `c.quit()` / protocol `QUIT` in harnesses — avoid SIGKILL without `--no-quit-confirm` ([AGENTS.md](../AGENTS.md)).

---

## Web build vs native

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `web.play-in-browser` | Run emulator in browser | Complete | web | COOP/COEP | Canvas runs | manual | [Web.md](Web.md), [Emscripten.md](Emscripten.md) |
| `web.mount-picker` | File picker instead of native dialog | Complete | web | — | Mount from picker | manual | |
| `web.no-persist-mounts` | Mounts lost on reload | Complete | web | — | Refresh clears | manual | MEMFS |
| `web.no-modem` | No SCC modem / SDL_net | Complete | web | — | Modem option absent | manual | Compiled out |
| `web.no-host-fst-picker` | No Host Folder… | Complete | web | — | Control absent | manual | |
| `web.audio-gesture` | Audio after user gesture | Complete | web | — | Click to start overlay | manual | Browser policy |
| `web.deploy-live` | gssquared.net/live deploy | Complete | web | Server | `/live` loads | manual | `scripts/deploy-web.sh` |

Future catalog/play: [arqyv-gs2pack.md](arqyv-gs2pack.md) (planned product shape).

---

## Slot cards and devices

Each subsection: hardware role, how to use it in GSSquared, then a **capabilities table**. TOML `card` strings from [SystemConfigTOML.md](SystemConfigTOML.md).

### Disk II (`disk_ii`)

Woz-based 5.25″ controller card. Mount via OSD, File → Drives, drag-drop, `-dsXdY=`, or `[[storage]]`.

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.diskii.mount-525` | Mount 140K images | Complete | all | Slot 6 typical | Boot guest | both | Multiple controllers allowed |
| `device.diskii.woz-protected` | WOZ copy protection | Complete | all | `.woz` | Protected titles run | manual | Weak bits, half tracks |
| `device.diskii.no-half-track` | Half/quarter track seek | Partial | all | — | Some images only | manual | [Storage.md](Storage.md) |

### Integrated Woz Machine — IWM (`iwm` motherboard, IIgs)

Built-in Apple IIgs floppy controller (5.25″ and 3.5″).

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.iwm.mount-525-woz` | 5.25″ WOZ/DSK | Complete | all | IIgs | Boot 5.25 | both | |
| `device.iwm.mount-35` | 3.5″ 800K | Complete | all | IIgs | Boot 3.5 | both | |
| `device.iwm.apple-disk-ui` | AppleDisk drive icons on GS | Complete | all | IIgs | HUD drives | manual | |

Deep dive: [src/devices/iwm/IWM2.md](../src/devices/iwm/IWM2.md).

### BazFast 3 / SmartPort (`bazfast3`, aliases `smartport`, `pdblock3`)

DMA SmartPort-style block storage (up to six drives).

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.bazfast.multi-drive` | Up to six SmartPort units | Complete | all | Card | CP shows 6 icons | manual | |
| `device.bazfast.block-images` | HDV/IMG/HDA/2MG | Complete | all | — | Format/partition | both | |
| `device.bazfast.mount-drivers-vol` | `/GS2.DRIVERS` RO volume | Complete | all | Free drive | Installer runs | manual | File → Mount Drivers |

### ProDOS block (`prodos_block`, `prodos_block2`)

Generic ProDOS block devices (older/smaller interfaces).

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.pdblock.mount` | Block image mount | Complete | all | Card in slot | ProDOS sees volume | both | Multiple `prodos_block` allowed |

### Language Card (`language_card`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.language-card.slot0` | 16K language card | Complete | all | Slot 0 only | LC programs | manual | Standard hardware; listed for config |

### Memory Expansion — Slinky (`mem_expansion`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.memexpansion.bank` | Up to 1MB expansion | Complete | all | Card | RAM tests / programs | manual | |

### Thunderclock Plus (`thunder_clock`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.thunderclock.prodos-time` | ProDOS clock + TIME SET | Complete | all | Card | Timestamps | manual | [Cards_Clock.md](Cards_Clock.md) |

### Generic ProDOS clock (`prodos_clock`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.prodos-clock.readonly` | Read-only ProDOS clock | Complete | all | Card | Date in ProDOS | manual | |

### Videx VideoTerm (`videx`)

80-column card for II / II+ (slot 3).

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.videx.80col` | 80-column text | Complete | all | II/II+ | 80-col software | manual | [Cards_Videx.md](Cards_Videx.md) |

### Mockingboard (`mockingboard`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.mockingboard.dual` | Two cards / four voices | Complete | all | Slots | Ultima V etc. | manual | [Cards_Mockingboard.md](Cards_Mockingboard.md) |
| `device.mockingboard.decorrelate` | Menu decorrelation | Complete | all | Card | Wider stereo | manual | Settings → Mono Helper |

### Apple Mouse III (`mouse`, `applemouseiii`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.mouseiii.iie` | Mouse card for //e | Complete | all | IIe + card | [Cards_AppleMouse.md](Cards_AppleMouse.md) | manual | |

### Parallel Interface (`parallel`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.parallel.capture` | Printer data to file/clipboard | Partial | all | `[[connections]]` | Raw bytes captured | manual | No ImageWriter emulation |
| `device.parallel.no-printer-emul` | ImageWriter / Epson emulation | Planned | all | — | — | — | Capture only; [Cards_Parallel.md](Cards_Parallel.md) |

See [Cards_Parallel.md](Cards_Parallel.md).

### Super Serial Card (`super_serial`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.ssc.connections` | Same virtual devices as SCC | Complete | all | Card + `[[connections]]` | Modem/file/serial | manual | [Cards_SuperSerial.md](Cards_SuperSerial.md) |

### Uthernet II (`uthernet2`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.uthernet2.slirp` | Ethernet via user-space stack | Complete | macOS, win, linux | Card + Marinetti/driver | Ping / telnet | manual | Not raw TAP |

### Video Overlay Card — VOC (`voc`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.voc.interlace` | 640×400 SHR interlace | Complete | all | IIgs slot 3 | Desktop patterns | manual | [Cards_VOC.md](Cards_VOC.md) |

### Second Sight (`second_sight`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.secondsight.vga` | VGA output path | Complete | all | IIgs slot 3 | External monitor modes | manual | [Cards_SecondSight.md](Cards_SecondSight.md) |
| `device.secondsight.text-menu` | Display → Second Sight Text | Complete | all | Card | Text via VGA | manual | [Cards_SecondSight.md](Cards_SecondSight.md) |

### VIDHD (`vidhd`)

65816 //e high-density video (custom platform variant).

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.vidhd.65816e` | VIDHD on //e 65816 | Complete | all | `apple2e_65816` | HD modes | manual | [Cards_VIDHD.md](Cards_VIDHD.md) |

### Host FST (`host_fst` motherboard, IIgs)

Share host folder as GS/OS `:Host`.

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.hostfst.install` | Install FST from `/GS2.DRIVERS` | Complete | macOS, win, linux | GS/OS + BazFast | Installer | manual | One-time per boot volume |
| `device.hostfst.pick-folder` | Host Folder… | Complete | macOS, win, linux | IIgs running | Finder shows `:Host` | manual | Default: Documents |
| `device.hostfst.windows-ads` | NTFS ADS metadata on Windows | Complete | win | NTFS folder | Forks preserved | manual | [HostFST.md](HostFST.md) |

### Motherboard: display (`display`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.display.engines` | NTSC / RGB / mono engines | Complete | all | — | Menu switch | manual | Not a slot card |
| `device.display.scanner-pal` | PAL scanner variant | Complete | all | PAL config | 50 Hz | manual | |

### Motherboard: speaker (`speaker`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.speaker.toggle` | $C030 speaker bit | Complete | all | — | Click in BASIC | manual | Standard; FX are GS2 addition |

### Motherboard: game port (`game_controller`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.gameport.paddles` | Paddles / buttons | Complete | all | — | Games respond | manual | Settings control source |

### Motherboard: keyboards (`keyboard_iiplus`, `keyboard_iie`)

Composed per platform; mapping docs are the user surface ([KeyboardShortcuts.md](KeyboardShortcuts.md)). No separate slot config.

### Motherboard: IIe memory (`iie_memory`)

Aux / main memory layout for //e family — platform-composed, not user-pluggable.

### Motherboard: cassette (`cassette`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.cassette.omitted` | Cassette load/save | By choice (omitted) | all | — | — | — | [Unimplemented.md](Unimplemented.md) |

### Motherboard: IIgs ADB (`adb` / Keygloo)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.adb.keyboard-mouse` | ADB keyboard + mouse | Complete | all | IIgs | GS/OS input | manual | |

### Motherboard: Ensoniq (`ensoniq`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.ensoniq.doc` | DOC sound synthesis | Complete | all | IIgs | GS audio titles | manual | No more known bugs per status doc |

### Motherboard: SCC8530 (`scc8530`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.scc.printer-port` | GS printer on serial | Partial | all | IIgs | Print to file | manual | Not full printer |
| `device.scc.modem-b` | Modem on port B | Complete | macOS, win, linux | IIgs | Hayes TCP | manual | Absent on web |

### Motherboard: RTC / PRAM (`rtc_pram`)

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `device.rtc.host-sync` | Clock synced to host TZ | Complete | all | IIgs | Control Panel clock | manual | BRAM also in `.gs2` |

---

## File associations and host launch

| `id` | feature | status | hosts | preconditions | verify | automation | notes |
|------|---------|--------|-------|---------------|--------|------------|-------|
| `host.open.gs2-macos-drop` | macOS Open → existing instance | Complete | macOS | Installed `.app` | `DROP_FILE` | manual | [ProtocolHandlers.md](ProtocolHandlers.md) |
| `host.open.gs2-windows-argv` | Windows double-click → argv | Complete | win | — | Second instance | manual | [ProtocolHandlers.md](ProtocolHandlers.md) |
| `host.open.gs2-associations` | Full OS file-type registration | Planned | all | — | — | — | Roadmap 1.0 |
| `host.url.gssquared-scheme` | `gssquared:` URL handler | Planned | all | — | — | — | Spec in ProtocolHandlers |

---

## Intentionally not offered

User-language summary of [Unimplemented.md](Unimplemented.md):

| Topic | User impact |
|-------|-------------|
| Cassette tape | No load/save of tape images or WAV |
| REPT key | Use host keyboard repeat |
| IIgs ROMBANK softswitch | No effect; matches unused ROM 01 behavior |
| IIgs DMABank register | Unimplemented |
| IIgs diagnostic register | Unimplemented |
| RAMfast SCSI | Not emulated |

---

## Planned and partial (product roadmap)

From [Roadmap.md](Roadmap.md) and status docs:

| `id` | feature | status | notes |
|------|---------|--------|-------|
| `roadmap.hud-cleanup` | HUD button polish | Planned | Roadmap 1.0 |
| `roadmap.file-associations` | File/URL associations | Planned | [ProtocolHandlers.md](ProtocolHandlers.md) |
| `roadmap.intl-keyboard-gs` | Full GS international keyboard | Planned | Post-1.0 |
| `roadmap.save-state` | Save/load full machine state | Planned | [SaveAndRestore.md](SaveAndRestore.md) |
| `roadmap.printer-emulation` | ImageWriter-class printing | Planned | Parallel/serial dump only today |
| `roadmap.appletalk-host-folder` | AppleTalk-shaped host server | Planned | Post-1.0 |

---

## Appendix: doc vs code (maintainer)

Rows flagged during catalog authoring — reconcile in [Documentation.md](Documentation.md) or code before treating as Complete in a release matrix.

| Topic | Catalog says | Other doc / note |
|-------|----------------|------------------|
| IWM 3.5″ | Complete in catalog | Documentation.md still lists “3.5 Pending” — reconcile when convenient |
| WOZ 2.1 | Planned | Documentation.md ❌ |
| 5.25 half/quarter track | Partial limit | Documentation.md footnote |
| SystemConfigTOML header | Implemented schema | Banner updated to match shipped `.gs2` / editor |
| Save states | Planned | ControlOverlay.md mentions save state; no implementation in tree |

---

## Appendix: suggested smoke tiers

**Not a release checklist** — rules for deriving one later:

| Tier | Filter on catalog rows |
|------|-------------------------|
| Tier 1 automation | `status=Complete` AND `automation` ∈ {`debug-socket`, `cli`, `both`} AND `hosts` contains target OS |
| Tier 1 manual smoke | `status=Complete` AND `hosts=all` OR includes target OS AND user-visible workflow |
| Tier 2 | `status=Partial` — manual only before release |
| Exclude | `Planned`, `By choice (omitted)` |

Example Tier 1 automated script: launch with `--debug` + `--no-quit-confirm`, `HELLO`, `-p 3`, `MOUNT`, `video_text`, `quit`.

---

## See also

- [index.md](index.md) — how-to guides
- [CommandLine.md](CommandLine.md) — CLI flags
- [Web.md](Web.md) — browser build
- [Documentation.md](Documentation.md) — fidelity and project status matrices
- [Menus.md](Menus.md) — menu reference

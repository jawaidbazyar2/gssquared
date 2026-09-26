# Saved Config Files

GSSquared lets you save and reload a complete virtual Apple II setup: which model you are emulating, what cards are in the slots, which disk images are mounted, and (optionally) serial-port attachments. That setup lives in a **saved config file**.

This guide is written for people who want to **create or edit their own configs**. If you need every field, validation rule, and enum value, see [SystemConfigTOML.md](SystemConfigTOML.md).

---

## Two file formats (pick one)

| Format | Filename | Best for |
|--------|----------|----------|
| **GS2 native** | `Something.gs2` | Configs you write yourself. Clear structure, easy to read. |
| **A2Fusion Profiles** | `Something Settings.txt` | Profile packs from the community (arqyv / A2Fusion). GS2 can load these, but you usually do not hand-edit them. |

Both formats describe the same thing internally. GS2 converts either one into the same machine setup at boot time.

To keep a `.gs2` and the disk images it mounts in a single file, put them in a [GS2 pack](Gs2Pack.md) (`.gs2pack`). The config inside that archive is still a `.gs2`, named `machine.gs2`.

There is also **`Profiles.txt`**, which is a **catalog** listing available profiles in a pack. You cannot boot from `Profiles.txt` directly — open a specific `… Settings.txt` file (or let the Profiles browser pick one for you).

---

## Opening a config

- **System Select screen** — use **Launch Config…** (or the folder icon) and choose a `.gs2` or `… Settings.txt` file.
- **macOS Finder** — double-click a `.gs2` file (or Open With GSSquared). If the System Select screen is showing, GSSquared loads and launches that config. If emulation is already running, a prompt asks whether to stop the current machine and launch the new config (Cancel keeps the current machine). See [File types and URL protocols](ProtocolHandlers.md).
- **Command line** — pass the file path as a positional argument:

  ```bash
  gssquared ~/Documents/MyIIe.gs2
  gssquared "Choplifter Settings.txt"
  ```

When you launch with a config file path, GSSquared skips the System Select screen and boots straight into that configuration. See [Command Line](CommandLine.md).

To write a config from the UI, use **+** or **Edit…** on System Select and click **Save** (or **Save As**). That writes a `.gs2` with platform, slots, pre-mounted disks, and serial/parallel attachments — not CPU/RAM state. **Save As** to a new path mints a new machine `id` (IIgs BRAM is copied). Full machine **save states** (CPU, RAM, devices) are [planned and not shipped](SaveAndRestore.md).

---

## Your first `.gs2` file

A `.gs2` file is plain text in **TOML** format. TOML uses `key = value` lines and `[[sections]]` for repeating blocks. Comments start with `#`.

Every file must start with a version line and a name:

```toml
gs2_version = 1
name = "My Enhanced IIe"
platform = "apple2e_enhanced"
```

Add a Disk II in slot 6 (the usual boot-floppy location on most setups):

```toml
[[cards]]
slot = 6
card = "disk_ii"
```

Save as `MyIIe.gs2`, put a disk image next to it, and mount it:

```toml
[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"
```

That is a valid, bootable config. Everything else is optional.

---

## Top-level settings

These go at the start of the file (before any `[[cards]]` blocks).

| Key | Required? | What it does |
|-----|-----------|--------------|
| `gs2_version` | **Yes** | Must be `1`. Future GS2 versions may accept newer numbers. |
| `id` | No | Stable UUID for this machine. Minted automatically if missing. See [Machine identity](#machine-identity-id). |
| `name` | **Yes** | Short title shown on the System Select tile. |
| `platform` | **Yes** | Which Apple II model. See [Platforms](#platforms). |
| `description` | No | Subtitle or tooltip text. |
| `clock` | No | `"ntsc"` (default) or `"pal"`. Not valid on IIgs. |
| `scanner` | No | Video timing. Usually omitted — GS2 picks a sensible default from `platform` and `clock`. |
| `speed` | No | Host CPU speed at boot: `"1.024mhz"`, `"2.8mhz"`, `"7.159mhz"`, `"14.3mhz"`, `"ludicrous"`. Omitted: 1.024 MHz (II/IIe) or 2.8 MHz (IIgs). F9 / OSD changes are session-only. |
| `display` | No | Monitor: `"composite"`, `"rgb"`, `"green"`, `"amber"`, `"white"`. Omitted: composite, or RGB on IIgs. |

### Platforms

| Value | Machine |
|-------|---------|
| `"apple2"` | Original Apple ][ |
| `"apple2plus"` | Apple ][+ |
| `"apple2e"` | Apple IIe |
| `"apple2e_enhanced"` | Enhanced //e |
| `"apple2e_65816"` | //e with 65816 accelerator |
| `"apple2gs"` | Apple IIgs ROM 01 |
| `"apple2gs_rom3"` | Apple IIgs ROM 03 |

### Machine identity (`id`)

`id` is a UUID that identifies the *machine*, not the filename. On IIgs platforms, battery RAM (Control Panel / NVRAM) is stored in the `.gs2` as a hex `bram` field. Closing the machine writes the file, including any NVRAM changes. Older installs used `PrefPath/bram/<id>.bin` or a `.bram` sidecar; those are imported into the `.gs2` when present.

| Action | BRAM result |
|--------|-------------|
| Copy or move the `.gs2` | BRAM travels with the file |
| Save As to a new file | New `id`; BRAM bytes are copied |

If `id` is missing when you load a writable `.gs2`, GSSquared assigns one and may rewrite the file. You rarely need to set it yourself.

### Clock and video

- **`clock = "ntsc"`** — North American timing (default).
- **`clock = "pal"`** — European timing. Use with II, II+, and IIe family only.
- **`scanner`** — Advanced. Values: `"apple2"`, `"apple2e"`, `"apple2e_pal"`, `"apple2gs"`. If you are building a PAL //e, set `clock = "pal"` and you can skip `scanner`; GS2 will use `"apple2e_pal"`.
- **`speed`** — How fast the emulator runs the CPU (not the IIgs Control Panel `$C036` bit). Same choices as the config editor and F4 speed row.
- **`display`** — Composite (NTSC), RGB, or a monochrome phosphor. Same choices as the config editor monitor row.

Motherboard devices (keyboard, built-in IIgs sound, built-in floppy controller on the GS, and so on) are added automatically from `platform`. You only list **expansion cards** in `[[cards]]`.

---

## Expansion cards — `[[cards]]`

Each card occupies one slot. Slots are numbered **0 through 7**.

```toml
[[cards]]
slot = 6
card = "disk_ii"

[[cards]]
slot = 7
card = "bazfast3"
```

Rules that bite people:

- **One card per slot.** Do not list the same slot twice.
- **Some cards only fit certain slots or machines.** Example: Videx 80-column card is slot 3 only, and only on Apple II / II+. Second Sight and the Video Overlay Card are IIgs-only and slot 3 only.
- **Most cards allow only one instance.** Mockingboard and Disk II are exceptions — you can have two Mockingboards in different slots.

### Card types

| `card` value | What it is |
|--------------|------------|
| `"language_card"` | Language card (slot 0, II / II+ only) — 16K bank-switched RAM |
| `"disk_ii"` | Disk II controller (two 5.25" drives) |
| `"prodos_clock"` | [Generic ProDOS clock](Cards_Clock.md) (read-only) |
| `"thunder_clock"` | [Thunderclock Plus](Cards_Clock.md) |
| `"parallel"` | [Parallel Interface](Cards_Parallel.md) |
| `"mockingboard"` | [Mockingboard](Cards_Mockingboard.md) |
| `"mouse"` | [Apple Mouse III](Cards_AppleMouse.md); `"applemouseiii"` is an alias |
| `"videx"` | [Videx VideoTerm](Cards_Videx.md) 80-column (II / II+ only, slot 3) |
| `"mem_expansion"` | Slinky-style RAM expansion (up to 1 MB) |
| `"prodos_block"` | Deprecated - do not use; prefer `"bazfast3"` |
| `"prodos_block2"` | Deprecated - do not use; prefer `"bazfast3"` |
| `"bazfast3"` | SmartPort / hard-disk controller (also accepts `"smartport"` or `"pdblock3"`) |
| `"vidhd"` | [VIDHD](Cards_VIDHD.md) (65816 //e only) |
| `"second_sight"` | [Second Sight](Cards_SecondSight.md) (IIgs only) |
| `"voc"` | [Video Overlay Card](Cards_VOC.md) (IIgs only, slot 3) |
| `"super_serial"` | [Super Serial Card](Cards_SuperSerial.md) |
| `"uthernet2"` | [Uthernet II](Cards_UthernetII.md) (IIe + IIgs; slots 1–7) |

Empty slots are simply omitted — you do not need to say “empty.”

### Parallel and serial attachments

Do **not** put paths on the card entry. Use `[[connections]]` (below) for parallel file/clipboard capture and serial modem/file/clipboard/host-UART attachments. See also [Parallel Interface](Cards_Parallel.md) and [Serial & Parallel Connections](SerialConnections.md).

---

## Disk images — `[[storage]]`

Pre-mount disks so they are ready when the machine boots. Each row is one drive:

```toml
[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"

[[storage]]
slot = 6
drive = 2
image = "disks/empty.woz"
```

| Property | Meaning |
|----------|---------|
| `slot` | Slot number of the **controller** (Disk II card, SmartPort card, or built-in IIgs drive). |
| `drive` | Drive number **1–6**, matching what you see in the Control Panel (1 = first drive). |
| `image` | Path to the disk image file. |

### Where to put image files

- **Relative paths** (recommended) — resolved from the folder containing your `.gs2` file. Keep images in a subfolder next to the config so you can zip the whole bundle and share it.
- **Absolute paths** — used as-is (`/Users/you/disks/game.po`).

Supported image types include `.po`, `.dsk`, `.woz`, `.2mg`, `.hdv`, and others GS2 recognizes.

### Which slot and drive?

| Hardware | Typical `slot` | `drive` values |
|----------|----------------|----------------|
| Disk II card | `6` (often) | `1`, `2` |
| SmartPort / BazFast (`bazfast3`) | card slot (often `7`) | `1`–`6` |
| IIgs built-in 3.5" (IWM) | `5` | `1`, `2` |
| IIgs built-in 5.25" (IWM) | `6` | `1`, `2` |

On a **IIgs**, slot `6` is the **built-in** 5.25" drive — not a Disk II card. On II / II+ / IIe, slot `6` is usually a Disk II card you listed in `[[cards]]`. The `[[storage]]` syntax is the same either way; only the hardware behind that slot differs.

Multiple SmartPort volumes are multiple `[[storage]]` rows on the same slot:

```toml
[[storage]]
slot = 7
drive = 1
image = "volumes/GSOS.po"

[[storage]]
slot = 7
drive = 2
image = "volumes/Games.po"
```

---

## Serial / parallel ports — `[[connections]]`

Optional. Describes what is “plugged into” each serial or parallel port — file capture, clipboard, a virtual modem, or a real host UART. Full UI walkthrough: [Serial & Parallel Connections](SerialConnections.md).

**IIgs built-in ports** (no `slot` — these are on the motherboard):

```toml
[[connections]]
port = "a"
device = "file"
path = "captures/printer.bin"

[[connections]]
port = "b"
device = "modem"
```

**Slot cards** (Super Serial, Parallel, etc.):

```toml
[[connections]]
slot = 2
device = "modem"

[[connections]]
slot = 1
device = "file"
path = "printouts/session.bin"
```

| Property | Meaning |
|----------|---------|
| `port` | IIgs SCC only: `"a"` or `"b"`. Defaults to `"a"` if omitted. |
| `slot` | Expansion-slot card (SSC, parallel, …). Use instead of `port`. |
| `device` | `"none"`, `"file"`, `"clipboard"`, `"echo"`, `"modem"`, or `"serial"`. Parallel allows `"none"` / `"file"` / `"clipboard"` only. **`echo` is TOML-only** — it is not offered in the Control Panel or config editor. |
| `path` | Host file when `device = "file"` (relative paths work like disk images). Host port name when `device = "serial"` (stored as-is, e.g. `cu.usbserial-…`, `/dev/cu.usbserial-…`, `COM3`, or `/dev/ttyUSB0`). |

If you omit `[[connections]]` entirely, GS2 uses platform defaults (IIgs: file + modem on native builds; SSC → modem; parallel → file).

---

## Complete examples

### Minimal Enhanced //e with one boot disk

```toml
gs2_version = 1
name = "Blank IIe"
platform = "apple2e_enhanced"

[[cards]]
slot = 6
card = "disk_ii"

[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"
```

### Apple ][+ “daily driver” style

```toml
gs2_version = 1
name = "Apple ][+"
description = "Disk II, clock, parallel, VIDEX, Mockingboard, SmartPort"
platform = "apple2plus"
clock = "ntsc"

[[cards]]
slot = 0
card = "language_card"

[[cards]]
slot = 1
card = "parallel"

[[cards]]
slot = 2
card = "prodos_clock"

[[cards]]
slot = 3
card = "videx"

[[cards]]
slot = 4
card = "mockingboard"

[[cards]]
slot = 5
card = "bazfast3"

[[cards]]
slot = 6
card = "disk_ii"

[[cards]]
slot = 7
card = "mem_expansion"

[[storage]]
slot = 5
drive = 1
image = "volumes/ProDOS_32MB.po"

[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"
```

### Apple IIgs with SmartPort volumes

```toml
gs2_version = 1
name = "My GS Workstation"
platform = "apple2gs"

[[cards]]
slot = 3
card = "second_sight"

[[cards]]
slot = 7
card = "bazfast3"

[[storage]]
slot = 5
drive = 1
image = "images/SystemTools_800k.2mg"

[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"

[[storage]]
slot = 7
drive = 1
image = "volumes/GSOS.po"

[[storage]]
slot = 7
drive = 2
image = "volumes/Games.po"
```

---

## A2Fusion Profiles and `Settings.txt`

Community **profile packs** (published through arqyv / A2Fusion) ship as folders containing disk images and one or more **`… Settings.txt`** files. These follow Michael Neil’s **Profiles Specification** — a simple `key value` format (space-separated, `#` comments).

GS2 can load any file whose name ends with **`Settings.txt`** (case-insensitive). You do not need to convert them to `.gs2` unless you want to edit by hand in TOML.

### Example profile file

```text
# Choplifter on Apple IIgs
profile.name Choplifter
machine A2GS

smartport.disk1 disks/zaxxon.dsk
floppy.disk1 disks/zork.woz
floppy.disk2 disks/zork2.woz

machine.speed 2800000
video.mode MONO
```

### How Settings keys map to `.gs2` concepts

| Settings.txt | Same idea in `.gs2` |
|--------------|---------------------|
| `profile.name` | `name` |
| `machine` | `platform` (see tokens below) |
| `gssquared.description` | `description` |
| `gssquared.clock` | `clock` |
| `gssquared.scanner` | `scanner` |
| `machine.speed` | `speed` |
| `video.mode` | `display` |
| `slot6 disk_ii` | `[[cards]]` with `slot = 6`, `card = "disk_ii"` |
| `smartport.disk1 path` | `[[storage]]` on the SmartPort slot, `drive = 1` |
| `floppy.disk1 path` | `[[storage]]` on the floppy controller, `drive = 1` |

**Machine tokens** in `machine` (comma-separated; first supported one wins):

| Token | GS2 platform |
|-------|----------------|
| `APPLE2` | `apple2` |
| `APPLE2PLUS` | `apple2plus` |
| `APPLE2E` | `apple2e` |
| `APPLE2E_ENHANCED` | `apple2e_enhanced` |
| `APPLE2E_65816` | `apple2e_65816` |
| `A2GS` | `apple2gs` |

Card names in `slotN` lines use the same names as the `card` column in the table above (`disk_ii`, `mockingboard`, `smartport`, etc.).

**SmartPort behavior:** If a Settings file mentions `smartport.diskN` but does not define slot 7, GS2 automatically installs a SmartPort card (`bazfast3`) in slot 7 — matching how many published profiles expect things to work.

**Paths in Settings files:** Relative paths are resolved from the Settings file’s folder. Some packs use an `sd:` prefix on paths; GS2 strips that and treats the remainder as a relative path.

### Preference keys (display, speed, audio)

Lines like `machine.speed`, `video.mode`, and `video.scanlines` are **preferences** — how you want to run the machine, not which hardware is installed.

- **`machine.speed`** — Hertz value (for example `2800000`) or a `.gs2` speed name. Mapped to host CPU speed at boot.
- **`video.mode`** — `COLOR` / `NTSC` / `COMPOSITE`, `RGB`, `MONO` / `GREEN`, `AMBER`, or `WHITE`. Mapped to the monitor type at boot.
- Other preference keys (`video.scanlines`, `sound`, …) are stored for compatibility but **not applied at boot yet**.

### `Profiles.txt` and `Global Settings.txt`

- **`Profiles.txt`** — index inside a profile pack (“Choplifter → Choplifter Settings.txt”, and so on). Open an individual Settings file, or use the Profiles browser when it is available — not the catalog itself.
- **`Global Settings.txt`** — app-wide defaults in the Profiles spec (networking, UI, paths). These belong to the **application**, not to one virtual machine. Per-machine hardware still comes from each profile’s Settings file or your `.gs2` file.

### When to use which format

| You want to… | Use |
|--------------|-----|
| Write your own config from scratch | `.gs2` |
| Share a config with another GS2 user | `.gs2` + relative disk paths |
| Run a curated arqyv / A2Fusion pack | The pack’s `… Settings.txt` as-is |
| Match every validation rule and enum | [SystemConfigTOML.md](SystemConfigTOML.md) |

---

## Clocks

| Clock | How you get it | What it does |
|-------|----------------|--------------|
| **Thunderclock Plus** | Slot card `"thunder_clock"` | ProDOS timestamps, TIME SET, host-synced counter. See [Clock cards](Cards_Clock.md). |
| **Generic ProDOS clock** | Slot card `"prodos_clock"` | Read-only date/time for ProDOS. |
| **IIgs realtime clock** | Built into IIgs platforms | Matches the **host time zone**. Battery RAM / Control Panel NVRAM is stored in the `.gs2` as `bram` when you close the machine. |

PAL (`clock = "pal"`) is video/timing, not these clock cards. It runs II / II+ / IIe at 50 Hz. See [Displays](Displays.md).

---

## Common mistakes

| Problem | Fix |
|---------|-----|
| `Unsupported gs2_version` | Set `gs2_version = 1`. |
| `Duplicate [[cards]] slot` | Each slot number may appear only once in `[[cards]]`. |
| `Card videx is not allowed on platform apple2gs` | Videx is II / II+ only. Check platform vs. cards. |
| `clock=pal is not valid for platform apple2gs` | Remove `clock = "pal"` on IIgs configs. |
| `Duplicate storage entry` | Only one `image` per `slot` + `drive` pair. |
| Disk not found at boot | Check relative paths — they are relative to the **config file’s directory**, not the app. |
| `Not a .gs2 or Settings.txt file` | Rename to end in `.gs2` or `Settings.txt`. |
| TOML syntax error | Strings need quotes; table headers are exactly `[[cards]]`, `[[storage]]`, `[[connections]]`. |

---

## Organizing your configs

A portable layout that works well:

```text
MyConfigs/
  MyIIe.gs2
  disks/
    BASIC SYSTEM.woz
  volumes/
    ProDOS_32MB.po
```

Default user configs (and builtin IIgs BRAM) live in `Documents/GSSquared/`:

| Platform | Typical folder |
|----------|----------------|
| macOS | `~/Documents/GSSquared/` |
| Windows | `%USERPROFILE%\Documents\GSSquared\` (or OneDrive Documents) |
| Linux | XDG documents dir, usually `~/Documents/GSSquared/` |

App settings and leftover PrefPath BRAM stay in the per-user prefs directory (`SDL_GetPrefPath`: Application Support / `%APPDATA%` / `~/.local/share`). You can also keep `.gs2` files anywhere and open them with Launch Config… or a path on the command line.

---

## Further reading

- **[SystemConfigTOML.md](SystemConfigTOML.md)** — full schema, validation rules, connection port registry, load/save behavior, and design notes.
- **Fixture examples in the repo** — `apps/systemconfigtest/fixtures/*.gs2` and `* Settings.txt` are real files GS2’s tests load successfully; useful as copy-paste starting points.

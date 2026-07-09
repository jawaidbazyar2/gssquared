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

There is also **`Profiles.txt`**, which is a **catalog** listing available profiles in a pack. You cannot boot from `Profiles.txt` directly — open a specific `… Settings.txt` file (or let the Profiles browser pick one for you).

---

## Opening a config

- **System Select screen** — use **Launch Config…** (or the folder icon) and choose a `.gs2` or `… Settings.txt` file.
- **macOS Finder** — double-click a `.gs2` file (or Open With GSSquared). If the System Select screen is showing, GSSquared loads and launches that config. If emulation is already running, it shows a short message asking you to quit emulation first.
- **Command line** — pass the file path as a positional argument:

  ```bash
  gssquared ~/Documents/MyIIe.gs2
  gssquared "Choplifter Settings.txt"
  ```

When you launch with a config file path, GSSquared skips the System Select screen and boots straight into that configuration.

**Save System** (writing your current session back to disk) is planned but not fully wired up yet. For now, create configs by editing a `.gs2` file in a text editor.

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
| `name` | **Yes** | Short title shown on the System Select tile. |
| `platform` | **Yes** | Which Apple II model. See [Platforms](#platforms). |
| `description` | No | Subtitle or tooltip text. |
| `clock` | No | `"ntsc"` (default) or `"pal"`. Not valid on IIgs. |
| `scanner` | No | Video timing. Usually omitted — GS2 picks a sensible default from `platform` and `clock`. |

### Platforms

| Value | Machine |
|-------|---------|
| `"apple2"` | Original Apple ][ |
| `"apple2plus"` | Apple ][+ |
| `"apple2e"` | Apple IIe |
| `"apple2e_enhanced"` | Enhanced //e |
| `"apple2e_65816"` | //e with 65816 accelerator |
| `"apple2gs"` | Apple IIgs |

### Clock and video

- **`clock = "ntsc"`** — North American timing (default).
- **`clock = "pal"`** — European timing. Use with II, II+, and IIe family only.
- **`scanner`** — Advanced. Values: `"apple2"`, `"apple2e"`, `"apple2e_pal"`, `"apple2gs"`. If you are building a PAL //e, set `clock = "pal"` and you can skip `scanner`; GS2 will use `"apple2e_pal"`.

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
- **Some cards only fit certain slots or machines.** Example: Videx 80-column card is slot 3 only, and only on Apple II / II+. Second Sight is IIgs-only.
- **Most cards allow only one instance.** Mockingboard and Disk II are exceptions — you can have two Mockingboards in different slots.

### Card types

| `card` value | What it is |
|--------------|------------|
| `"language_card"` | Language card (slot 0, II / II+ only) |
| `"disk_ii"` | Disk II controller (two 5.25" drives) |
| `"prodos_clock"` | ProDOS real-time clock |
| `"thunder_clock"` | Thunder Clock Plus |
| `"parallel"` | Parallel printer interface |
| `"mockingboard"` | Mockingboard sound |
| `"mouse"` | Apple Mouse II |
| `"videx"` | Videx 80-column (II / II+ only, slot 3) |
| `"mem_expansion"` | RAM expansion (Slinky-style) |
| `"prodos_block"` | Generic ProDOS block device |
| `"prodos_block2"` | ProDOS block device (variant 2) |
| `"bazfast3"` | SmartPort / hard-disk controller (also accepts `"smartport"` or `"pdblock3"`) |
| `"vidhd"` | VIDHD (65816 //e only) |
| `"second_sight"` | Second Sight (IIgs only) |

Empty slots are simply omitted — you do not need to say “empty.”

### Parallel printer output

If you install a parallel card, you can send printer output to a file on your Mac:

```toml
[[cards]]
slot = 1
card = "parallel"
output = "printouts/session.txt"
```

If `output` is omitted, GS2 uses a default file name.

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

## Serial ports — `[[connections]]`

Optional. Describes what is “plugged into” serial ports — file capture, loopback test, or a virtual modem.

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

| Property | Meaning |
|----------|---------|
| `port` | `"a"` or `"b"`. Defaults to `"a"` if omitted. |
| `device` | `"none"`, `"file"`, `"echo"`, or `"modem"`. |
| `path` | Host file when `device = "file"`. Relative paths work like disk images. |
| `slot` | Only for a serial card in an expansion slot (future). Omit for IIgs built-in SCC. |

If you omit `[[connections]]` entirely, GS2 uses platform defaults (on IIgs, port A is typically a file device and port B a modem on native builds).

Omit this section unless you care about serial setup — most configs do not need it.

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

Lines like `machine.speed`, `video.mode`, and `video.scanlines` are **preferences** — how you want to run the machine, not which hardware is installed. GS2 reads and stores these for compatibility with profile packs, but **does not apply all of them at boot yet**. Hardware and disk mounts from the same file still load normally.

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

On macOS, user configs will eventually live under Application Support (for example `~/Library/Application Support/GSSquared/systems/`). Until Save System is implemented, you can keep files anywhere and open them with the folder icon or `-c`.

---

## Further reading

- **[SystemConfigTOML.md](SystemConfigTOML.md)** — full schema, validation rules, connection port registry, load/save behavior, and design notes.
- **Fixture examples in the repo** — `apps/systemconfigtest/fixtures/*.gs2` and `* Settings.txt` are real files GS2’s tests load successfully; useful as copy-paste starting points.

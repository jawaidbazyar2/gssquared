# System Config TOML (`.gs2`)

Proposed on-disk format for user-created system configurations. Maps to `SystemConfig_t` in `src/systemconfig.hpp`, slot device IDs in `src/Device_ID.hpp` / `src/devices.cpp`, and initial disk mounts handled by `Mounts::mount_media()` in `src/util/mount.cpp`.

This is a **design proposal** — not yet implemented.

## File identity

| Item | Value |
|------|-------|
| Extension | `.gs2` |
| Format | TOML 1.0 |
| Default location | User config directory (e.g. `~/Library/Application Support/GSSquared/systems/` on macOS) |
| Related formats | Project X files (arqyv-generated) are a separate, forward-compatible format |

Every file MUST begin with a format version so future loaders can migrate or reject old files:

```toml
gs2_version = 1
```

Unknown `gs2_version` values are a hard error. Loaders MAY accept a range of versions if they implement migration.

TODO: on the first invocation of GS2 copy some default extra configs into the local user systems folder.

---

## Top-level schema

Corresponds to `SystemConfig_t` plus per-card configuration and disk mounts:

```c
struct SystemConfig_t {
    const char *name;
    PlatformId_t platform_id;
    bool builtin;
    clock_set_t clock_set;
    video_scanner_t scanner_type;
    const char *description;
    device_id slot_devices[NUM_SLOTS];  // slots 0–7, derived from [[cards]]
};

typedef struct {
    uint16_t slot;
    uint16_t drive;
    std::string filename;
} disk_mount_t;
```

| TOML key | Type | Required | Maps to | Notes |
|----------|------|----------|---------|-------|
| `gs2_version` | integer | yes | — | Format version (currently `1`) |
| `name` | string | yes | `name` | Display name on System Select tile |
| `description` | string | no | `description` | Short subtitle / tooltip text |
| `platform` | string | yes | `platform_id` | See [Platform](#platform) |
| `clock` | string | no | `clock_set` | Default from platform if omitted |
| `scanner` | string | no | `scanner_type` | Default from platform if omitted |
| `builtin` | boolean | no | `builtin` | Always `false` for user files; omit or set explicitly |
| `[[cards]]` | array of tables | no | `slot_devices[]` + per-card config | Hardware only; see [Cards](#cards) |
| `[[storage]]` | array of tables | no | `disk_mount_t[]` | All pre-mounted disks; see [Storage](#storage) |
| `[[connections]]` | array of tables | no | serial port attachments | Virtual devices on serial ports; see [Connections](#connections) |

Motherboard-resident devices (keyboard, display, speaker, IIe memory, IIgs ADB/RTC/Ensoniq/SCC/IWM, etc.) are composed automatically from `platform` during `transition_to_emulation()`, matching current built-in behavior. Only slot cards appear in `[[cards]]`.

**Separation of concerns:**

- `[[cards]]` — what hardware is installed.
- `[[storage]]` — what disk images are mounted (`slot` + `drive`), matching `Mounts`.
- `[[connections]]` — what virtual device is attached to each serial port (`port` or `slot` + `port`), matching however serial ports register at runtime (same pattern as `Mounts` for disks).

There is no storage- or serial-attachment data on card entries (except `parallel` `output`, which is a printer sink rather than a serial connection).

---

## Platform

String enum → `PlatformId_t` (`src/PlatformIDs.hpp`):

| TOML value | C enum | Typical use |
|------------|--------|-------------|
| `"apple2"` | `PLATFORM_APPLE_II` | Original Apple ][ |
| `"apple2plus"` | `PLATFORM_APPLE_II_PLUS` | Apple ][+ |
| `"apple2e"` | `PLATFORM_APPLE_IIE` | Apple IIe |
| `"apple2e_enhanced"` | `PLATFORM_APPLE_IIE_ENHANCED` | Enhanced //e |
| `"apple2e_65816"` | `PLATFORM_APPLE_IIE_65816` | //e with 65816 accelerator |
| `"apple2gs"` | `PLATFORM_APPLE_IIGS` | Apple IIgs |

Loaders SHOULD reject `clock = "pal"` on `"apple2gs"` (PAL clock is not applicable to GS in current code).

---

## Clock

String enum → `clock_set_t` (`src/NClock.hpp`):

| TOML value | C enum |
|------------|--------|
| `"ntsc"` | `CLOCK_SET_US` |
| `"pal"` | `CLOCK_SET_PAL` |

If omitted, use `"ntsc"`.

---

## Video scanner

String enum → `video_scanner_t` (`src/devices/displaypp/VideoScanner.hpp`):

| TOML value | C enum | Typical platform |
|------------|--------|------------------|
| `"apple2"` | `Scanner_AppleII` | II, II+ |
| `"apple2e"` | `Scanner_AppleIIe` | IIe, Enhanced //e |
| `"apple2e_pal"` | `Scanner_AppleIIePAL` | PAL //e |
| `"apple2gs"` | `Scanner_AppleIIgs` | IIgs, 65816 //e |

If omitted, derive from `platform` and `clock` (e.g. PAL //e → `"apple2e_pal"`).

---

## Card types

String enum → `device_id` (`src/Device_ID.hpp`). These are the values allowed in `[[cards]].card` and validated against `Devices[]` in `src/devices.cpp` (`slots_allowed`, `platform_flags`, `multipleInstances`).

The TOML key **`device`** is reserved for `[[connections]]` — the virtual peripheral attached to a serial port (`file`, `modem`, `echo`, `none`). It MUST NOT be used as the card-type field.

### Slot-assignable card types

| TOML value | C enum | Human name | Notes |
|------------|--------|------------|-------|
| `"language_card"` | `DEVICE_ID_LANGUAGE_CARD` | II/II+ Language Card | Slot 0 only |
| `"prodos_block"` | `DEVICE_ID_PRODOS_BLOCK` | Generic ProDOS Block | Multiple instances allowed |
| `"prodos_clock"` | `DEVICE_ID_PRODOS_CLOCK` | Generic ProDOS Clock | |
| `"disk_ii"` | `DEVICE_ID_DISK_II` | Disk II Controller | Multiple instances allowed |
| `"mem_expansion"` | `DEVICE_ID_MEM_EXPANSION` | Memory Expansion (Slinky) | |
| `"thunder_clock"` | `DEVICE_ID_THUNDER_CLOCK` | Thunder Clock Plus | |
| `"prodos_block2"` | `DEVICE_ID_PD_BLOCK2` | Generic ProDOS Block 2 | |
| `"parallel"` | `DEVICE_ID_PARALLEL` | Apple II Parallel Interface | |
| `"videx"` | `DEVICE_ID_VIDEX` | Videx VideoTerm | Slot 3 only; II / II+ only |
| `"mockingboard"` | `DEVICE_ID_MOCKINGBOARD` | Mockingboard | Multiple instances allowed |
| `"mouse"` | `DEVICE_ID_MOUSE` | Apple Mouse II | |
| `"vidhd"` | `DEVICE_ID_VIDHD` | VIDHD | 65816 //e only |
| `"bazfast3"` | `DEVICE_ID_PD_BLOCK3` | BazFast 3 (DMA Storage) | Multiple instances allowed |
| `"second_sight"` | `DEVICE_ID_SECOND_SIGHT` | Second Sight | IIgs only |

Aliases (optional, loader MAY accept):

| Alias | Canonical |
|-------|-----------|
| `"pdblock3"`, `"smartport"` | `"bazfast3"` |
| `"pdblock2"` | `"prodos_block2"` |

### Motherboard devices (not in `[[cards]]`)

Present automatically per platform; listed for reference:

| TOML value | C enum | Platform(s) |
|------------|--------|-------------|
| `"keyboard_iiplus"` | `DEVICE_ID_KEYBOARD_IIPLUS` | II, II+ |
| `"keyboard_iie"` | `DEVICE_ID_KEYBOARD_IIE` | IIe family |
| `"speaker"` | `DEVICE_ID_SPEAKER` | All |
| `"display"` | `DEVICE_ID_DISPLAY` | All |
| `"game_controller"` | `DEVICE_ID_GAMECONTROLLER` | All |
| `"iie_memory"` | `DEVICE_ID_IIE_MEMORY` | IIe family |
| `"cassette"` | `DEVICE_ID_CASSETTE` | All |
| `"rtc_pram"` | `DEVICE_ID_RTC_PRAM` | IIgs |
| `"adb"` | `DEVICE_ID_KEYGLOO` | IIgs |
| `"ensoniq"` | `DEVICE_ID_ENSONIQ` | IIgs |
| `"scc8530"` | `DEVICE_ID_SCC8530` | IIgs |
| `"iwm"` | `DEVICE_ID_IWM` | IIgs |

---

## Cards

Expansion hardware is described as an array of card entries. Each entry occupies one slot and may carry type-specific properties (parallel output path, etc.). **Disk images and serial attachments are not specified here** — use `[[storage]]` and `[[connections]]`.

```toml
[[cards]]
slot = 6
card = "disk_ii"

[[cards]]
slot = 7
card = "bazfast3"
```

### Common properties

Every `[[cards]]` entry:

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `slot` | integer | yes | Expansion slot `0`–`7` |
| `card` | string | yes | Card type; see [Card types](#card-types) |

Validation (loader responsibility, mirroring `Devices[]`):

- Each `slot` appears at most once.
- Card type must be allowed on the chosen `platform` (`platform_flags`).
- Card type must be legal in that slot number (`slots_allowed` bitmask).
- At most one instance unless `multipleInstances` is true (e.g. two `"mockingboard"` cards in slots 4 and 7).

The loader builds `slot_devices[NUM_SLOTS]` from `[[cards]]`: unlisted slots are empty (`DEVICE_ID_NONE`).

---

## Per-card-type properties

Each card type defines which additional keys are valid on its `[[cards]]` entry. Unknown keys for a card type SHOULD produce a warning; loaders MAY reject them.

Storage card types (`disk_ii`, `prodos_block`, `prodos_block2`, `bazfast3`) have **no** card-level storage properties. Mount disks via `[[storage]]` after the card is installed.

Serial-capable slot cards (e.g. future Super Serial Card) have **no** connection properties on the card entry. Attach virtual devices via `[[connections]]`.

### `parallel`

Parallel printer interface. Optional output destination:

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `output` | string | `"parallel.out"` | File that receives printed bytes |

```toml
[[cards]]
slot = 1
card = "parallel"
output = "printouts/session.txt"
```

### `mockingboard`, `mouse`, `videx`, `language_card`, clocks, storage cards, serial cards, etc.

No additional v1 properties beyond `slot` and `card`. Card presence and slot are sufficient.

---

## Connections

All serial port attachments use `[[connections]]`, whether the port is on a slot card or built into the IIgs motherboard (SCC8530). Each serial port registers with the runtime the same way storage devices register with `Mounts`; the config file only names the port and what is plugged into it.

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `device` | string | yes | Virtual peripheral: `file`, `modem`, `echo`, `none` |
| `port` | string | yes* | Port channel: `"a"` or `"b"` |
| `slot` | integer | no | Slot number when the port is on a slot card; omit for IIgs built-in SCC |
| `path` | string | no | Host file when `device = "file"` |
| `remote_url` | string | no | Remote URL when `device = "modem"` (future) e.g. telnet://1.2.3.4:5566 |

\* For a single-port slot card, `port` MAY be omitted and defaults to `"a"`.

### `device` values

Aligned with `src/serial_devices/`:

| Value | Description |
|-------|-------------|
| `"none"` | No attachment |
| `"file"` | Read/write through a host file (`path` required) |
| `"echo"` | Loopback for testing |
| `"modem"` | Hayes-compatible virtual modem (native builds only) |

### Port addressing

**IIgs built-in SCC** — omit `slot`, set `port` to `"a"` (modem/printer) or `"b"`:

```toml
[[connections]]
port = "a"
device = "file"
path = "captures/port_a.bin"

[[connections]]
port = "b"
device = "modem"
```

These map to `SCC_CHANNEL_A` / `SCC_CHANNEL_B` after `init_scc8530_slot()` composes the motherboard. Same attachment model as today’s hard-coded `FileDevice` / `ModemDevice` setup in `scc8530.cpp`, but driven from config.

**Slot serial card** (future Super Serial Card, etc.) — set `slot` to the card’s slot; `port` defaults to `"a"` for single-port cards:

```toml
[[cards]]
slot = 2
card = "super_serial"

[[connections]]
slot = 2
device = "modem"
```

### Rules

- Each `(slot, port)` pair appears at most once. Built-in SCC ports use `(no slot, "a"|"b")`.
- Omit `[[connections]]` entirely to use platform defaults (IIgs: port A → file, port B → modem on native builds).
- After hardware is composed, the loader SHOULD warn on or reject entries whose port is not registered.
- Built-in `port = "a"|"b"` entries are valid only when `platform = "apple2gs"`.

### What stays on cards

**Parallel** printer output is not a serial connection — it remains a card property (`output` on the `parallel` card entry). If other non-serial sinks appear later, they can stay on the card or gain their own top-level section; serial and serial-like comm ports use `[[connections]]`.

---

## Storage

All pre-mounted disk images use `[[storage]]`, regardless of whether the drive belongs to a slot card or to built-in IIgs IWM hardware. Every storage controller — Disk II, block devices, and IIgs IWM — registers drives with `Mounts` using the same `slot`/`drive` keys, so one format covers all of them.

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `slot` | integer | yes | Slot number of the storage controller |
| `drive` | integer | yes | Drive unit `1`–`6` (1-based, matching Control Panel / CLI convention) |
| `image` | string | yes | Path to disk image file |

Each entry maps directly to `Mounts::mount_media({ slot, drive - 1, image })`.

Rules:

- One `image` per `slot`/`drive` pair. Each `slot`/`drive` combination appears at most once.
- Omit `[[storage]]` entirely for configs with no pre-mounted disks.
- After hardware is composed, the loader SHOULD warn on or reject `[[storage]]` entries whose `slot`/`drive` has no registered storage device.

### Path resolution

- **Absolute paths** are used as-is.
- **Relative paths** are resolved relative to the directory containing the `.gs2` file (recommended for portable configs).

Supported image types are whatever `identify_media()` accepts (`.po`, `.dsk`, `.woz`, `.2mg`, `.hdv`, `.pmap` etc.).

### Drive map (reference)

| Hardware | `slot` | `drive` values |
|----------|--------|----------------|
| Disk II controller | card slot (typically `6`) | `1`, `2` |
| ProDOS Block / Block 2 / BazFast 3 | card slot | `1`–`6` (BazFast registers six units) |
| IIgs built-in 5.25" (IWM) | `6` | `1`, `2` |
| IIgs built-in 3.5" (IWM) | `5` | `1`, `2` |

On Apple IIgs, slot `6` is the motherboard IWM 5.25" drives — not a Disk II card. On II/II+/IIe, slot `6` is typically a Disk II card. That difference is invisible to `[[storage]]`: IWM registers its drives with `Mounts` the same way slot cards do, and images mount with the same `{ slot, drive, image }` triple. Validation only needs to check that the composed system has a registered device at that address.

Multi-volume BazFast mounts are multiple rows on the same slot:

```toml
[[storage]]
slot = 7
drive = 1
image = "volumes/GSOS.po"

[[storage]]
slot = 7
drive = 2
image = "volumes/Games1.po"

[[storage]]
slot = 7
drive = 3
image = "volumes/Games2.po"
```

---

## Complete examples

### Apple ][+ — matches built-in "Apple ][+"

`Apple2Plus.gs2`:

```toml
gs2_version = 1
name = "Apple ][+"
description = "64K RAM (incl Lang Card); Disk II; Clock; Parallel Port; VIDEX 80-col; Mockingboard"
platform = "apple2plus"
clock = "ntsc"
scanner = "apple2"

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

[[storage]]
slot = 6
drive = 2
image = "disks/empty.woz"
```

### Enhanced //e with dual Mockingboard

`DualMock.gs2`:

```toml
gs2_version = 1
name = "Apple IIe Enhanced Dual Mockingboard"
description = "128K RAM; Disk II; Clock; DUAL Mockingboard"
platform = "apple2e_enhanced"
clock = "ntsc"
scanner = "apple2e"

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
card = "mockingboard"

[[storage]]
slot = 6
drive = 1
image = "disks/BASIC SYSTEM.woz"
```

### Apple IIgs with Second Sight and SmartPort volumes

`MyGS.gs2`:

```toml
gs2_version = 1
name = "My GS Workstation"
description = "Apple IIgs 8MB RAM; Second Sight; boot + games volumes"
platform = "apple2gs"
clock = "ntsc"
scanner = "apple2gs"

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
image = "volumes/Games1.po"

[[storage]]
slot = 7
drive = 3
image = "volumes/Games2.po"

[[connections]]
port = "a"
device = "file"
path = "captures/printer.bin"

[[connections]]
port = "b"
device = "modem"
```

### PAL Enhanced //e

`IIe_PAL.gs2`:

```toml
gs2_version = 1
name = "Apple IIe Enhanced / PAL"
description = "PAL Video; 128K RAM; Disk II; Clock; Mockingboard"
platform = "apple2e_enhanced"
clock = "pal"
scanner = "apple2e_pal"

[[cards]]
slot = 2
card = "mem_expansion"

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
card = "mouse"
```

---

## Load / save behavior (proposed)

1. **Parse** TOML → in-memory `SystemConfig_t` + per-card config + `[[storage]]` + `[[connections]]`.
2. **Validate** platform, cards, slot assignments, and device compatibility.
3. **Select** config on System Select screen or via File → Open System.
4. **Compose** system: platform motherboard devices + slot cards (existing `transition_to_emulation()` path).
5. **Apply** card-specific settings (parallel `output`, etc.) and `[[connections]]` during or after device init.
6. **Mount** disks from `[[storage]]` after storage devices register (same order as CLI `-dsXdY=` handling in `gs2.cpp`).

**Save System** / **Save As System** (from `Docs/UserInterface.md`):

- Writes `[[cards]]` and metadata from the current config.
- Writes one `[[storage]]` row per mounted drive (slot, drive, image path).
- Writes one `[[connections]]` row per configured serial port (port or slot+port, device, paths).
- Sets `builtin = false`.
- Omits `[[storage]]` rows for empty drives; omits `[[connections]]` rows that match platform defaults (optional).

---

## Minimal valid file

```toml
gs2_version = 1
name = "Blank IIe"
platform = "apple2e_enhanced"

[[cards]]
slot = 6
card = "disk_ii"
```

All other fields use platform defaults; no disks pre-mounted.

---

## Open questions

- **Serial port registry:** Implement a `Connections` (or extend device init) so SCC and slot serial cards register addressable ports the same way `Mounts` registers drives — then loader, OSD, and save/load all share one model.
- **Super Serial Card:** Not yet in `devices.cpp`; `[[connections]]` with `slot` is specified for when it is added.
- **Built-in migration:** Ship current `BuiltinSystemConfigs[]` as `.gs2` files and load all configs through one code path?
- **Profile preferences:** Add `[display]`, `[speed]`, `[input]`, `[audio]` (or equivalent) for menu-controlled settings; map Neil `video.*` / `machine.speed` on import.

---

## Architecture: profiles, TOML, and arqyv

This section records design intent from ongoing work with **Profiles Specification v0.81** (Michael Neil, June 2026) and the arqyv curated-profile effort. It is not part of the v1 `.gs2` file format itself.

### One internal model, several entry points

**Profiles are a UI and packaging concept**, not a second machine-configuration system inside GS2. When the user launches a configuration — from any source — the loader fills the **same in-memory structures** this document describes, then `transition_to_emulation()` runs as today.

```
Profiles browser (arqyv) ──┐
Open .gs2 (folder icon)  ──┼──► Profile loader ──► Profile struct ──► compose + mount + apply prefs
Built-in select tiles    ──┘         ▲
Save System / editor     ──────────────┘ (TOML serialize / deserialize)
```

- **`.gs2` TOML** — native save/edit format for local user configs (Save System, Create Profile editor, shipped built-ins on disk).
- **Profiles browser** — System Select UI for discovering curated packs published via arqyv (catalog + assets); selecting a profile invokes the loader on the backing Settings bundle.
- **Mike’s Profiles `Settings.txt`** — interchange format arqyv (and hardware) may ship; GS2 parses it into the same struct shape as `.gs2`, not a parallel runtime path.

### Canonical `Profile` struct (planned)

The loader targets one aggregate type (names provisional):

| Section | Contents | This doc |
|---------|----------|----------|
| **Metadata** | `name`, `description`, tile image, help URL, source URL | top-level `name`, `description`; image not in v1 TOML yet |
| **Machine** | `platform`, `clock`, `scanner`, cards | `platform`, `clock`, `scanner`, `[[cards]]` |
| **Storage** | Pre-mounted disks | `[[storage]]` |
| **Connections** | Serial port attachments | `[[connections]]` |
| **Preferences** | Display, CPU speed, joystick mode, audio | not in v1 TOML yet; fields exist scattered in `video_system_t`, `NClock`, `gamec_state_t`, `AudioSystem` |
| **Extensions** | Unmapped Neil / `gssquared.*` keys | round-trip for forward compatibility |

Built-in System Select tiles, user `.gs2` files, and arqyv profile packages all converge on this struct.

### Machine config vs preferences

**In the profile (hardware truth):** `platform`, `clock_set`, `scanner`, `[[cards]]`, `[[storage]]`, `[[connections]]` — what Mike’s `slotN`, `smartport.disk*`, and slot serial settings describe, plus GS2-specific platform/scanner fields.

**In preferences (how the user runs it):** monitor type (NTSC / RGB / mono), mono color, CRT shader, emulated CPU speed, game-controller mode, volume, etc. — Mike’s `machine.speed`, `video.*`, and similar. These apply to motherboard/host-facing subsystems after compose, analogous to how `[[storage]]` applies after devices register.

**App-global (not per profile):** e.g. sleep/busy-wait, CRT-at-boot default, paths — Mike’s `Global Settings.txt` layer; lives in `gs2_app_t`, not in `.gs2`.

### arqyv collaboration

arqyv publishes **profile packages**: catalog (`Profiles.txt`), Settings file(s), disk images, and tile artwork. GS2’s Profiles button is the front door; **launch** parses the package into `Profile`, then boots. Curation and publishing stay in arqyv/Mike’s format; GS2 implements import mapping and the shared internal model.

Collaboration with Mike should define: card-type names, `smartport.diskN` / `floppy*.diskN` → `[[storage]] slot`/`drive` mapping, required keys for a minimal playable profile, and optional `gssquared.*` extensions.

### Implementation phasing (GS2)

1. **Load** — parse `.gs2` and Neil `Settings.txt` into `Profile`; folder icon + Profiles browser stub.
2. **Save** — Save System writes `.gs2` from current `Profile` (including mounted disks and connections).
3. **Editor + catalog** — Create Profile UI; arqyv sync, recent tiles, `profile.image` on System Select.

Phase 1 validates the loader and mapping with real arqyv packs before investing in the full editor.

# Blank Disk Images — implementation spec

Developer spec for **File → New Disk Image**. User guide: [Blank Disk Images](BlankFloppy.md).

## Goal

Give the user a way to create a new image file on disk, then mount it later like any other image. Creating an image does **not** mount it.

The menu is available whether or not a machine is running. It is pure host file I/O.

## User flow

1. Choose **File → New Disk Image →** one of the types below.
2. The native save-file dialog opens, with a suggested filename and the appropriate extension filter.
3. Cancel: nothing is written.
4. Confirm a path:
   - Floppy types: copy the matching pre-baked `.woz` from shipped resources to that path.
   - 32M HD: create a new 33,554,432-byte file of zeros at that path, with a default .hdv suffix
   - 32M HD ProDOS: the same file, then write ProDOS volume structures over its first 22 blocks
5. Done. The user mounts the file later via **File → Drives**, the Control Panel, or drag-and-drop. See [Storage](Storage.md).

```
File → New Disk Image → type
        │
        ▼
  native save dialog
        │
   ┌────┴────┐
 cancel    path chosen
   │         │
   ▼         ▼
 nothing   floppy → copy resources/floppyimages/*.woz
           HD     → create 32M zero file
                    └→ ProDOS variant: format the new file
                    │
                    ▼
              file on disk (not mounted)
```

## Menu

**File → New Disk Image →**

| Menu label | Meaning |
|---|---|
| 5.25 Unformatted | Empty 5.25″ WOZ2: valid file structure, no track data |
| 5.25 Formatted DOS 3.3 | 5.25″ WOZ2 with a DOS 3.3 filesystem |
| 5.25 Formatted ProDOS | 5.25″ WOZ2 with a ProDOS filesystem |
| 3.5 Formatted ProDOS | 3.5″ / 800K WOZ2 with a ProDOS filesystem |
| 32M HD Unformatted | Raw 32M block image, all zeros |
| 32M HD Formatted ProDOS | Raw 32M block image carrying an empty ProDOS volume |

“ProDOS” in the menu labels means a **ProDOS-formatted** volume.

Suggested default filenames in the save dialog:

| Type | Suggested name |
|---|---|
| 5.25 Unformatted | `Blank 5.25 Unformatted.woz` |
| 5.25 Formatted DOS 3.3 | `Blank 5.25 DOS 3.3.woz` |
| 5.25 Formatted ProDOS | `Blank 5.25 ProDOS.woz` |
| 3.5 Formatted ProDOS | `Blank 3.5 ProDOS.woz` |
| 32M HD Unformatted | `Blank 32M HD.hdv` |
| 32M HD Formatted ProDOS | `Blank 32M ProDOS.hdv` |

Filters: `.woz` for the four floppy types; `.hdv` for the HDs (raw 512-byte blocks, same class as the shipped `drivers.hdv`).

On macOS, use the existing NSSavePanel helper pattern (`gs2_show_save_file_dialog` / `src/platform-specific/macos/gs2_save_dialog.mm`) so the suggested filename actually appears. Do not patch vendored SDL. Other platforms use `SDL_ShowSaveFileDialog` with `default_location` set to the last disk-dialog directory plus the suggested filename.

Remember the chosen directory via the existing `FileDialogKind::Disk` last-path machinery.

## Image catalog

| Menu item | On-disk result | How it is produced |
|---|---|---|
| 5.25 Unformatted | WOZ2, `disk_type = 1`, TMAP all `0xFF`, empty TRKS (no bit blocks) | Bake once with `wozutil create-blank`, commit under `assets/floppyimages/` |
| 5.25 Formatted DOS 3.3 | Pre-baked WOZ2 | Created with CiderPress2 (`cp2`), dropped into `assets/floppyimages/` |
| 5.25 Formatted ProDOS | Pre-baked WOZ2 | Same |
| 3.5 Formatted ProDOS | Pre-baked WOZ2 | Same |
| 32M HD Unformatted | 65,536 × 512 = 33,554,432 zero bytes | Generated at save time; **not** shipped |
| 32M HD Formatted ProDOS | Same file, first 22 blocks written with an empty ProDOS volume | Generated at save time by `src/util/ProDOSFormat.cpp` |

The formatted floppies are ordinary WOZ2 images (INFO + TMAP + populated TRKS). Volume name and other filesystem details are whatever `cp2` wrote when the templates were baked. They are not generated at runtime.

The 32M HDs are **not** WOZ and **not** 2MG. They are raw `.hdv` files. BazFast write-through already handles this size. Do not ship a 32M image in the distribution — both variants are generated at save time.

ProDOS 8 has no built-in volume format command, so an unformatted HD is only useful to someone who owns a formatting utility (Copy II Plus, System Utilities, GS/OS Advanced Disk Utility). The ProDOS variant exists so the common case needs no utility at all.

## Assets

Shipped templates live under **`assets/floppyimages/`** and are copied into `resources/floppyimages/` at build/install time (same pattern as `assets/vdisk/` → `resources/vdisk/`).

| Resource file | Menu item |
|---|---|
| `blank-525-unformatted.woz` | 5.25 Unformatted |
| `blank-525-dos33.woz` | 5.25 Formatted DOS 3.3 |
| `blank-525-prodos.woz` | 5.25 Formatted ProDOS |
| `blank-35-prodos.woz` | 3.5 Formatted ProDOS |
| `prodos-boot.bin` | Block 0 of a formatted ProDOS HD (512-byte ProDOS boot loader) |

Runtime path: `Paths::get_base_path()` + `floppyimages/<file>`.

CMake `assemble_resources` globs and copies `assets/floppyimages/` into `build/resources/floppyimages/`, with a per-directory `.assembled.stamp` and a Linux FHS install rule next to `vdisk`.

## Unformatted 5.25 WOZ

`wozutil create-blank` writes an empty 5.25″ WOZ2 via default `Woz()` plus `Woz::save()`.

### File layout

WOZ 2.x, little-endian chunks, CRC32 over all bytes after the 12-byte header (Gary S. Brown 1986 table, already in `src/util/woz.cpp`).

| Offset | Size | Contents |
|---|---|---|
| 0 | 8 | `WOZ2` magic: `57 4F 5A 32 FF 0A 0D 0A` |
| 8 | 4 | CRC32 of bytes 12…end |
| 12 | 8 + 60 | `INFO` chunk (id + size + 60-byte payload) |
| 80 | 8 + 160 | `TMAP` chunk |
| 248 | 8 + 1280 | `TRKS` chunk: 160 × 8-byte zero TRK descriptors, **no** bit blocks |
| 1536 | — | end of file (`WOZ_TRACK_DATA_OFFSET`) |

Expected file size: **1536 bytes** (slightly larger if META is present).

### INFO fields

Use the `woz_info_t` defaults already in `src/util/woz.hpp`, plus the constructor’s creator string:

| Field | Value |
|---|---|
| version | 2 |
| disk_type | 1 (5.25″) |
| write_protected | 0 |
| synchronized | 0 |
| cleaned | 1 |
| creator | `"GS2 WOZ"` space-padded to 32 bytes (no NUL), as `Woz::Woz()` already sets |
| disk_sides | 1 |
| boot_sector_format | 0 (unknown) |
| optimal_bit_timing | 32 (4 µs) |
| compatible_hardware | 0 |
| required_ram | 0 |
| largest_track | 0 (no tracks) |
| flux_block / largest_flux_track | 0 |

### TMAP / TRKS

- TMAP: 160 bytes of `0xFF` (no quarter-track mapped).
- TRKS: 160 descriptors, all zeros. `Woz::build_trks_chunk()` skips tracks with `bit_count == 0`, so no bitstream payload is appended.
- Optional META is allowed (e.g. title/subtitle identifying a blank 5.25″). Not required for a valid image.

Reads of unmapped tracks synthesize random bits (empty-disk noise). Writes to empty tracks allocate a track (other branch).

## `wozutil create-blank`

```
wozutil create-blank <output.woz>
```

Development tool only. The emulator copies the committed template; it does not call `wozutil` at runtime.

The three formatted templates are **not** produced by `wozutil`. Create them with `cp2` and place the resulting `.woz` files in `assets/floppyimages/`.

## 32M HD at save time

When the user confirms a path for **32M HD Unformatted**:

- Create (or overwrite) the file at that path.
- Size: 33,554,432 bytes (65,536 blocks × 512).
- Contents: zeros. `ftruncate` / equivalent is fine; holes read as zeros.
- Extension: `.hdv` by default.

Do not generate this file at build time or ship it in resources.

## 32M HD Formatted ProDOS

Same file, then `prodos_format_image()` (`src/util/ProDOSFormat.cpp`) writes the first 22 blocks. Nothing else is touched, so the file stays sparse (~16K on disk on APFS).

`total_blocks` is **65,535**, not 65,536: the field is 16 bits, so the last block of a 32M file lies outside the volume. `pdblock3::get_drive_info()` already reports 65,535 blocks for a 65,536-block image, so guest and volume header agree. ProDOS 8 catalogs the result as 65,513 free + 22 used.

| Blocks | Contents |
|---|---|
| 0 | ProDOS boot loader, copied from `resources/floppyimages/prodos-boot.bin` |
| 1 | Zeros (second half of the boot area, unused by the loader) |
| 2–5 | Volume directory; block 2 holds the volume header, blocks 3–5 are empty and linked prev/next |
| 6–21 | Volume bitmap: 65,536 bits = 8,192 bytes = 16 blocks |

Volume header fields (block 2): `storage_type` `$F` with the name length in the low nibble, volume name, creation and modification date/time, `version` 0, `min_version` 0, `access` `$C3`, `entry_length` `$27`, `entries_per_block` `$0D`, `file_count` 0, `bitmap_pointer` 6, `total_blocks` 65535.

Bitmap: `1` = free, MSB of each byte is the lowest-numbered block. Blocks 0–21 are marked used, and so are the bits past `total_blocks` (the final bit, for the nonexistent block 65,535).

Dates use the ProDOS packed format (year in bits 15–9, month 8–5, day 4–0; minute and hour in the time word). Per Apple II Technical Note #28 the 7-bit year is 0–39 for 20xx and 40–99 for 19xx, so 2026 is stored as 26. (CiderPress2 writes `year - 1900` here instead, which reads as 2026 as well but overflows after 2027.)

If `prodos-boot.bin` is missing, block 0 stays zeroed: the volume is still valid, just not bootable, and a warning goes to stderr. The image is otherwise byte-identical to `cp2 create-disk-image <file> 32M prodos`, apart from the volume name, timestamps, and the `version` byte (cp2 writes 5).

### Volume name

Derived from the filename so no extra UI is needed: take the stem, upper-case it, and collapse each run of characters ProDOS disallows into a single period (`My Games.hdv` → `MY.GAMES`). If the result is empty, longer than 15 characters, or does not start with a letter, use `BLANK`.

Do not generate this file at build time or ship it in resources.

## Save-dialog implementation notes

Mirror the `.gs2` save path in `EditSystem::begin_save()`:

- Floppy: filter `WOZ images` / `woz`.
- HD (both variants): filter `Hard disk images` / `hdv`.
- `default_location` = last disk-dialog directory + suggested filename.
- macOS: titled NSSavePanel with `setNameFieldStringValue` for the suggested name. Existing files appearing gray in the save panel is Apple behavior; overwrite still works.
- Callback: copy the resource file (`SDL_CopyFile`), or create the 32M zero file and optionally format it. On failure, surface `system_diag` and leave nothing usable behind (a partly formatted HD is removed). On success, no further UI.

Menu events are handled in `gs2.cpp` `SDL_AppEvent` for all phases (`MenuInterface::newDiskImage`).

## Out of scope

- Auto-mount after create.
- Empty-track allocation on write (other branch).
- Generating formatted DOS/ProDOS bitstreams at runtime (the ProDOS HD is a block image, not a bitstream).
- Shipping a 32M image of any kind.
- HD sizes other than 32M, HFS volumes, and prompting for a volume name.
- 2MG headers, `.po` / `.do` blanks, or unformatted 3.5″.
- Changing vendored SDL save-dialog behavior.

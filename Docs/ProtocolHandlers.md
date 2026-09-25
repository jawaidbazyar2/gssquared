# File types and URL protocols

How GSSquared is opened from the host: Finder / Explorer, drag-and-drop, the command line, and (later) web links. Implementation notes live here; user-facing launch steps are in [Creating Custom System Configs](ConfigEditor.md) and [Writing Config Files Manually](ConfigFiles.md).

**Windows:** double-clicking a `.gs2` while GSSquared is already running starts a **second process** (the path goes to `argv`). macOS delivers the open to the existing instance. Disk-image associations and the `gssquared:` URL scheme are [planned](Roadmap.md), not shipped.

Roadmap 1.0 lists “file type and URL associations.” This document is the spec for that work.

## How the host talks to GS2

SDL3 does **not** have a separate “document open” event. On macOS, Finder Open, Open With, and custom URL schemes all become `SDL_EVENT_DROP_FILE`. SDL documents that event as “the system requests a file open.”

| How the user opened something | What GS2 sees |
|-------------------------------|---------------|
| Finder double-click / Open With / `open Foo.gs2` | `SDL_EVENT_DROP_FILE` with `window == NULL`. No hover target. |
| Explorer / file-manager double-click of `.gs2` | Not a drop. The shell starts `GSSquared` with the path in `argv`. A second click while GS2 is already running starts a **second process** (unlike macOS, which delivers `DROP_FILE` to the existing instance). |
| Drag onto the GS2 window | `DROP_BEGIN` → `DROP_POSITION` (OSD hover) → `DROP_FILE` with a window → `DROP_COMPLETE`. |
| Terminal `GSSquared file.gs2` | Not a drop. [`src/gs2.cpp`](../src/gs2.cpp) `SDL_AppInit` reads `argv` and auto-launches. |
| Click `gssquared:https://…` (future) | Same `DROP_FILE`, but `event.drop.data` is the **URL string**, not a local path. |

On macOS, SDL’s Cocoa delegate maps:

* `application:openFile:` → `SDL_SendDropFile(NULL, NULL, filename)`
* `kAEGetURL` (`handleURLEvent`) → `SDL_SendDropFile(NULL, NULL, urlString)`

A Finder open of a `.app` bundle does **not** put the file in `argv`. Launch Services delivers an Apple Event after startup; SDL queues it as `DROP_FILE`. Associations apply to the **installed app bundle**, not the loose `build/GSSquared` executable.

Window drops can target a drive button (`DROP_POSITION` + hover). A system open cannot. That is why a later disk-image association cannot reuse “drop on a drive.”

## macOS `.gs2` document type

Declared in [`assets/Info.plist.in`](../assets/Info.plist.in):

* Exported UTI `com.bazyar.gs2.config` (extension `gs2`)
* `CFBundleDocumentTypes` role **Editor**, `LSHandlerRank` **Owner**
* Conforms to `public.text` and `public.data` (the file is TOML)

### Icon

Without `UTTypeIconFile` / `CFBundleTypeIconFile`, Finder uses the generic text-file icon (because the UTI conforms to `public.text`).

Both keys point at `gs2-config.icns` (source art and `build.sh` live in `~/src/gs2_icons/`; the committed `.icns` is in `assets/img/`). The files still conform to `public.text`; the custom icon overrides the generic fallback.

Finder caches UTI icons. After installing a bundle that changes the icon, refresh with:

```
lsregister -f /path/to/GSSquared.app
```

or log out. Existing `.gs2` files may keep the old generic icon until Launch Services rebuilds its database.

## Windows `.gs2` document type

The zip has no installer. [`src/FileAssociations.cpp`](../src/FileAssociations.cpp) rewrites the per-user association on every launch (skipped when `--debug` is set so smoke tests do not steal the handler).

HKCU (no admin):

* `Software\Classes\.gs2` → ProgID `GSSquared.gs2`, `Content Type` `application/x-gs2-config`
* `Software\Classes\GSSquared.gs2` — “GS2 System Configuration”
* `DefaultIcon` = `"<exe>",1` (document icon embedded in the EXE)
* `shell\open\command` = `"<exe>" "%1"`

`<exe>` is `GetModuleFileNameW`, so a moved zip keeps working. `SHChangeNotify(SHCNE_ASSOCCHANGED)` runs only when the written values changed.

### Icon

[`assets/windows/gssquared.rc.in`](../assets/windows/gssquared.rc.in) embeds two icons (MinGW `windres`):

* Index 0 — `gs2.ico` (application)
* Index 1 — `gs2-config.ico` (`.gs2` document)

Source art and `build.sh` live in `~/src/gs2_icons/`; the committed `.ico` files are in `assets/img/`.

## Linux `.gs2` document type

MIME type **`application/x-gs2-config`**, glob `*.gs2`. We do not claim `*.txt`.

Packaged (FHS / AppImage AppDir / RPM):

* [`assets/GSSquared.desktop`](../assets/GSSquared.desktop) — `Exec=GSSquared %f`, `MimeType=application/x-gs2-config;`
* [`assets/mime/gssquared.xml`](../assets/mime/gssquared.xml)
* Mimetype icons from `assets/img/gs2-config.iconset/` → `share/icons/hicolor/<size>/mimetypes/application-x-gs2-config.png`

The AppImage / zip-style launch also rewrites the **user** tree on every launch so a moved AppImage stays associated:

* `~/.local/share/applications/GSSquared.desktop` with absolute `Exec=` (`$APPIMAGE` or `/proc/self/exe`)
* `~/.local/share/mime/packages/gssquared.xml`
* Mimetype PNGs copied from `resources/img/mimetype-gs2-*.png`
* `xdg-mime default GSSquared.desktop application/x-gs2-config`

`update-desktop-database` and `update-mime-database` run only when those files actually changed.

## Opening a config: UX

Applies to `.gs2` and to `… Settings.txt` once the file has been handed to GS2 (see below).

| Situation | Behavior |
|-----------|----------|
| App not running | Launch GS2 and boot that profile. Closing the machine quits the app (`auto_launched`). |
| System Select | Load and launch immediately. Same `auto_launched` document-session as a cold open. |
| Config editor | Ignore. Do not discard an unsaved draft. |
| Emulation running | Modal: launch this config? Current machine will stop. **Cancel** / **Launch**. Launch walks dirty disks (same chain as quit / power-off), tears down the machine, and boots the new config **without** flashing System Select and **without** exiting the process. The new session is a document-open (`auto_launched`). |

`auto_launched` is the trap: a cold-opened `.gs2` sets it so power-off exits the app. An in-session switch must halt without taking that exit path, then set `auto_launched` again on the new machine.

**File → Launch Config…** from System Select still returns to the selector on power-off (the user is already inside the app). Finder Open / Open With / CLI path are document sessions.

## `… Settings.txt` is not a Finder type

Launch Services matches **one filename extension** — the token after the last `.`. For `Apple IIe Settings.txt` that is only `txt`. There is no UTI / `CFBundleDocumentTypes` way to claim the long suffix ` Settings.txt` (or `Profiles.txt`) without claiming **all** `.txt` files.

We will not declare a `.txt` document type. Double-click stays with the user’s text editor.

GS2 still recognizes those names **inside the app** via `detect_config_file_kind` (basename ends with `Settings.txt` or `.gs2`). Paths that work today:

* Open With → GSSquared
* Drag onto the window
* File → Launch Config…
* CLI argv

A Finder Open With of a Settings file arrives as the same `SDL_EVENT_DROP_FILE` as a `.gs2`, so the already-running prompt applies. The way a **web page** launches a Settings pack is the custom URL scheme, not a Finder association.

## Disk-image associations (future)

Wanted types include `.woz`, `.dsk`, `.do`, `.po`, `.2mg`, `.hdv`, `.img`, `.hda`, `.nib`, `.iso`, and `.pmap`.

* **`.pmap`** is GS2-specific — export a UTI and claim **Owner**.
* Shared types (`.woz`, `.dsk`, …) should be `LSHandlerRank` **Alternate** so we do not steal them from CiderPress, Virtual II, or similar.

Finder Open of a disk image has **no hover target**. That needs an explicit policy, not the current “mount on the highlighted drive button” drop path. Candidates: first empty compatible drive, a drive picker, or (cold start) pick/last-used machine then mount. Until that policy exists, do not register disk types — an association that lands on System Select and does nothing is worse than no association.

## URL protocol (future)

A custom scheme is how a web page launches GS2. Finder suffix matching is irrelevant, which is why this is the path for `… Settings.txt` packs.

### Scheme

Register **`gssquared`** in `CFBundleURLTypes` (matches the app, low collision risk). Optional later alias: `gs2`.

Recommended form — our scheme, then a normal https config URL:

```
gssquared:https://example.com/pack/Choplifter%20Settings.txt
```

A page uses `<a href="gssquared:https://…">Play in GSSquared</a>`. The browser asks “Open GSSquared?” and Launch Services hands us the string.

An equivalent query form is `gssquared://open?config=https%3A%2F%2F…`. Do **not** silently rewrite `gssquared://example.com/…` to https unless that rewrite is documented and stable.

### After the click

1. `DROP_FILE` data is the full URL. Do not treat `gssquared:…` as a filesystem path.
2. Parse out the https config URL.
3. Prompt: “Download *N* files from example.com and launch?”
4. Download the config into a cache directory.
5. Walk `image =` (and Settings disk lines, and `.pmap` members). Resolve those paths against the **config URL’s directory**.
6. Download each image next to the config, then launch as a local profile (same already-running modal as a local `.gs2`).

`detect_config_file_kind` still applies after download (basename ends with `.gs2` or `Settings.txt`).

### Must-haves before shipping the scheme

* HTTPS only (or an explicit, documented exception).
* Reject `file:`, unexpected schemes, and absolute image URLs off-host.
* Size cap.
* Confirm prompt before any download.
* Same switch-while-running modal as local configs.
* Windows and Linux protocol registration (registry / `.desktop` `x-scheme-handler`), not only macOS `CFBundleURLTypes`.

## `.gs2pack`

A **pack** is one file that holds a machine config plus its disk images. Extension **`.gs2pack`**. Opening it (Finder, Explorer, Steam, or the CLI) launches that machine with those disks mounted. System Select is skipped. Loose `.gs2` stays the editable text format. A pack is “this machine and its disks, as one document.”

Format is **uncompressed POSIX ustar** (the `ustar` tar dialect: magic `ustar`, version `00`). One file on every OS — no macOS directory-package illusion required. Stock `tar` lists and extracts it (`tar -tf pack.gs2pack`). Composing with stock `tar` must pass `--format ustar`. A plain `tar -cf` writes pax on macOS and Windows and GNU tar on Linux; those extended headers are not a pack.

Typical members:

```
machine.gs2
disks/boot.woz
disks/data.hdv
```

ustar limits a member name to 100 bytes plus a 155-byte prefix. `machine.gs2` and `disks/boot.woz` fit. Each member is a 512-byte header, then the raw bytes, then padding to 512. The header checksum covers the header only. Two 512-byte zero blocks end the archive. Disk-image members are stored raw. Their length does not change when the guest writes.

### Runtime: extract, mount, rewrite

GSSquared mounts the temporary-extracted archive members. Floppies write back with `fopen`; SmartPort / `.hdv` write **through** to a real file (or to MEMFS). Gzip is not part of the format and is not accepted.

1. Open the `.gs2pack` path.
2. Extract to a **local** working directory (not the cloud folder) Local disk or MEMFS.
3. Launch `machine.gs2` from that tree. Relative `image =` paths work as they do today.
4. On power-off, config switch, or app quit: rewrite the ustar if anything changed (always rewriting is fine — packs are ≤200 MB).
5. Atomic replace: write `name.gs2pack.tmp`, then `rename` over the old file.

Do not update the archive per sector. Do not extract into a synced folder - extract into a TEMP folder.

### In-place reader (RP2350)

An RP2350-class reader (520 KB SRAM, no PSRAM on Pico 2) treats the ustar as the filesystem and does not extract. A member starts on a 512-byte boundary. A disk image that cannot grow is a fixed span after its header, so a sector write is a write to that offset on the SD card. The pack itself is uncompressed ustar. GSSquared on desktop and web still extracts and rewrites; it does not poke the archive.

So, this works for pretty much any image - except .WOZ where tracks are added. read/write to .WOZ where all tracks are already specified and stored should work fine.

### Composing a pack as a stream

A writer that already knows each member’s size can emit the archive in one forward pass: 512-byte header, member bytes, pad to 512, and two zero blocks at the end. A generated `machine.gs2` is small enough to finish in memory, count, and then emit. Catalog disk blobs already have sizes. No temp file is required to assemble the pack. arQyv’s compose path is in [arqyv-gs2pack.md](arqyv-gs2pack.md).

### Cloud-backed store (Steam as the model)

Steam Cloud (and the same idea on iCloud / Dropbox / OneDrive) syncs **files in a known folder**, usually at session boundaries (Steam) or continuously (generic cloud).

**Only the `.gs2pack` lives in the synced tree.** The extract cache is machine-local (`~/Library/Caches/…`, `%LOCALAPPDATA%`, …). If the workdir is inside the cloud folder, every HD write-through becomes a sync event and conflict bait.

That matches Steam AutoCloud: download the pack before launch, upload the pack after exit. GS2 packs on quit so the file Steam uploads is complete. A crash loses work since the last successful rewrite — same as any other document; periodic rewrite is optional, not required for size.

**Conflicts** are last-write-wins on a binary blob. Two machines that both extract, play, and pack will drop one session. Acceptable if we treat a pack like a save file. Optional later: notice the pack’s mtime/size changed while we had it open, and warn before overwriting.

**Quota:** Steam Cloud is **per app, per user**, set by us in Steamworks (`Byte quota per user`), not something a player can buy more of. Valve’s published ceiling is on the order of **10 GB** per game (and a 100 MiB cap on a single `FileWrite`; larger packs need the stream write API or AutoCloud). A few 200 MB packs still eat that budget fast.

**Intended product split**

* **Steam** distributes **GSSquared** (the player). Steam Cloud on that app, if enabled at all, is tiny — settings, not a pack library. Players cannot buy more Steam Cloud, and one 10 GB quota cannot be “my whole disk collection.”
* **[arqyv.net](https://arqyv.net)** is the paid **collection**: subscribe for cloud-backed `.gs2pack` storage and the same library on every computer. Open via `gssquared:https://arqyv.net/…` (or an in-app signed-in browser). Same extract / mount / rewrite rules; the ustar that arqyv stores is the document. Curated `… Settings.txt` packs are the current arqyv shape; `.gs2pack` is the later unit.
* Optional extra channel: a **third-party Steam title** that *is* one pack (“Choplifter for GSSquared”) can use *that* app’s Steam Cloud for its own pack. DLC on the GS2 app does not get a separate quota.

`gssquared:` downloads can land as a cache `.gs2pack` (ustar of the fetched config + images) and then follow the same extract / launch / rewrite path. If that cache is not in a cloud folder, nothing uploads until the user saves a copy there.

## Phasing

| Slice | Status |
|-------|--------|
| Spec (this file) | Now |
| Local `.gs2` / Settings open while emulating (prompt, dirty-disk, switch) | Now |
| `.gs2` Finder document icon | Now |
| Windows / Linux `.gs2` file-type and document icon | Now |
| Disk-image UTIs + Finder-open-to-mount policy | Later |
| `gssquared:` URL fetch + cache + confirm | Later |
| `.gs2pack` ustar (extract / mount / atomic rewrite, Simple Browser) | Now |
| Cloud folder / Steam AutoCloud (the pack only in the synced tree) | Later |
| Windows / Linux `gssquared:` protocol registration | Later |

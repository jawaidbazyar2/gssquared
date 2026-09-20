# File types and URL protocols

How GSSquared is opened from the host: Finder / Explorer, drag-and-drop, the command line, and (later) web links. Implementation notes live here; user-facing launch steps are in [Creating Custom System Configs](ConfigEditor.md) and [Writing Config Files Manually](ConfigFiles.md).

Roadmap 1.0 lists “file type and URL associations.” This document is the spec for that work.

## How the host talks to GS2

SDL3 does **not** have a separate “document open” event. On macOS, Finder Open, Open With, and custom URL schemes all become `SDL_EVENT_DROP_FILE`. SDL documents that event as “the system requests a file open.”

| How the user opened something | What GS2 sees |
|-------------------------------|---------------|
| Finder double-click / Open With / `open Foo.gs2` | `SDL_EVENT_DROP_FILE` with `window == NULL`. No hover target. |
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

## `.gs2pack` (future)

A **pack** is one file that holds a machine config plus its disk images. Extension **`.gs2pack`**. Format is a **zip** (one file on every OS — no macOS directory-package illusion required).

Typical layout inside the zip:

```
machine.gs2
disks/boot.woz
disks/data.hdv
```

Loose `.gs2` stays the editable text format. A pack is “this machine and its disks, as one document.”

### Runtime: extract, mount, rewrite

GS2 cannot mount zip members. Floppies write back with `fopen`; SmartPort / `.hdv` write **through** to a real file.

1. Open the `.gs2pack` path (Finder / Steam / CLI).
2. Extract to a **local** working directory (not the cloud folder).
3. Launch `machine.gs2` from that tree. Relative `image =` paths work as they do today.
4. On power-off, config switch, or app quit: rewrite the zip if anything changed (always rewriting is fine — packs are ≤200 MB).
5. Atomic replace: write `name.gs2pack.tmp`, then `rename` over the old file.

Do not update the zip per sector. Do not extract into a synced folder.

### Cloud-backed store (Steam as the model)

Steam Cloud (and the same idea on iCloud / Dropbox / OneDrive) syncs **files in a known folder**, usually at session boundaries (Steam) or continuously (generic cloud).

**Only the `.gs2pack` zip lives in the synced tree.** The extract cache is machine-local (`~/Library/Caches/…`, `%LOCALAPPDATA%`, …). If the workdir is inside the cloud folder, every HD write-through becomes a sync event and conflict bait.

That matches Steam AutoCloud: download zips before launch, upload zips after exit. GS2 packs on quit so the file Steam uploads is complete. A crash loses work since the last successful rewrite — same as any other document; periodic rewrite is optional, not required for size.

**Conflicts** are last-write-wins on a binary blob. Two machines that both extract, play, and pack will drop one session. Acceptable if we treat a pack like a save file. Optional later: notice the zip’s mtime/size changed while we had it open, and warn before overwriting.

**Quota:** Steam Cloud is **per app, per user**, set by us in Steamworks (`Byte quota per user`), not something a player can buy more of. Valve’s published ceiling is on the order of **10 GB** per game (and a 100 MiB cap on a single `FileWrite`; larger packs need the stream write API or AutoCloud). A few 200 MB packs still eat that budget fast.

**Intended product split**

* **Steam** distributes **GSSquared** (the player). Steam Cloud on that app, if enabled at all, is tiny — settings, not a pack library. Players cannot buy more Steam Cloud, and one 10 GB quota cannot be “my whole disk collection.”
* **[arqyv.net](https://arqyv.net)** is the paid **collection**: subscribe for cloud-backed `.gs2pack` storage and the same library on every computer. Open via `gssquared:https://arqyv.net/…` (or an in-app signed-in browser). Same extract / mount / rewrite rules; the zip that arqyv stores is the document. Curated `… Settings.txt` packs are the current arqyv shape; `.gs2pack` is the later unit.
* Optional extra channel: a **third-party Steam title** that *is* one pack (“Choplifter for GSSquared”) can use *that* app’s Steam Cloud for its own zip. DLC on the GS2 app does not get a separate quota.

`gssquared:` downloads can land as a cache `.gs2pack` (zip the fetched config + images) and then follow the same extract / launch / rewrite path. If that cache is not in a cloud folder, nothing uploads until the user saves a copy there.

## Phasing

| Slice | Status |
|-------|--------|
| Spec (this file) | Now |
| Local `.gs2` / Settings open while emulating (prompt, dirty-disk, switch) | Now |
| `.gs2` Finder document icon | Now |
| Disk-image UTIs + Finder-open-to-mount policy | Later |
| `gssquared:` URL fetch + cache + confirm | Later |
| `.gs2pack` zip (extract / mount / atomic rewrite) | Later |
| Cloud folder / Steam AutoCloud (zip only in the synced tree) | Later |
| Windows / Linux file-type and protocol registration | Later |

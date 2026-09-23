# arQyv-served GS2 packs

Status: spec. Not implemented.

This is how the Emscripten build plays an [arQyv](https://arqyv.net) **Title** or **Collection** item: fetch the pack, boot it, let the user swap disks inside that pack, and store disk writes back to that user’s arQyv cloud storage.

The document model is the `.gs2pack` in [ProtocolHandlers.md](ProtocolHandlers.md): one config plus its disk images, extracted to a working tree, mounted as ordinary files, rewritten as a unit when the session saves. This file is the web session of that model. Native `gssquared:` URLs, Finder open, and Steam Cloud stay in ProtocolHandlers. Web disk mounting as it works today is in [Emscripten.md](Emscripten.md).

## Mode

**arQyv-served-gs2pack** is a session mode, not a second emulator.

| | Ordinary web session | arQyv-served-gs2pack |
|--|----------------------|----------------------|
| How it starts | System Select. User mounts disks themselves. | A Title or Collection item. Shell fetches the pack and auto-starts that config. |
| Drive “open” | Browser file picker (this computer). | Simple Browser (this pack’s images). |
| Where writes go | The local disk image the user opened. | Pack images: the user’s arQyv cloud storage. A disk opened with the local picker: that local file. |

The local picker still exists in pack mode, as a secondary action (“Open from this computer…”). The drive button’s primary dialog is the Simple Browser. A disk opened that way writes back to the local image, in either mode. It is not stored in the Collection copy.

## Where the player runs

Host the player on arqyv.net, on a path that sends the same COOP/COEP headers the web build needs for pthreads (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`).

Title disk files already live on arqyv.net under `/catalog/<id>/disks/…`. Same-origin fetches are allowed under `require-corp`. The Collection session cookie is first-party there.

`gssquared.net/live` cannot do this as it stands. That page is cross-origin isolated, and catalog responses send neither CORS nor `Cross-Origin-Resource-Policy`, so a `fetch()` of a disk (and an iframe of `/titles` or `/collection`) is blocked. `Cross-Origin-Opener-Policy: same-origin` also clears `window.opener`, so a popup on arqyv.net cannot post a selection back.

If the player stays on `gssquared.net/live` later:

- Catalog disk URLs and a play manifest need CORS and `Cross-Origin-Resource-Policy: cross-origin`.
- Collection needs a sign-in redirect that returns a short-lived token for that one item. A credentialed cross-site fetch of the arqyv session cookie is not the plan.

## Titles and Collection

Both resolve to a pack. The shell does not crawl the archive.

- **Title** — public curated catalog entry (`/titles/<slug>`). Metadata the player needs: platform badge, and the ordered disk list (role, filename, URL). Example: 221B Baker Street is two WOZs; Choplifter is one WOZ and the page says it will not run on later models. `Profiles.txt` is a catalog index and is not a boot file.
- **Collection** — the signed-in user’s saved titles (`/collection`, login required). A row is a title plus that user’s saved images, when they have any.

Public catalog objects are immutable. Playing a Title downloads that snapshot into the working tree. The first save creates or updates the user’s Collection copy. Nothing writes back onto `/catalog/<id>/`.

Playing a Collection item that already has saved images downloads the catalog snapshot, then overlays the user’s copies.

## Working tree and auto-start

GSSquared cannot mount zip members. The shell unpacks into the in-memory filesystem (MEMFS) before the emulator opens anything:

```
/packs/<id>/machine.gs2
/packs/<id>/disks/boot.woz
/packs/<id>/disks/data.woz
```

`[[storage]]` `image` paths are resolved relative to the config file. With that layout, the paths in the config are the MEMFS paths `mount_media` already opens.

The emulator already auto-starts when a config path is on `argv` (`SDL_AppInit` loads it and skips System Select). The web shell does not set `Module.arguments` today. Pack mode does, from `preRun`, after the writes:

```javascript
Module.arguments = ['/packs/<id>/machine.gs2'];
```

`preRun` may be asynchronous so the runtime waits while the pack downloads. The click that chooses Play is the user gesture that unlocks audio.

A stored `machine.gs2` (or a `… Settings.txt`) ships with the title when the machine is not “floppies in slot 6”: IIgs, hard disk, a card the badge does not imply. Otherwise the shell writes a minimal config from the badge and the disk list:

- Platform from the badge (`apple2` / `apple2plus` / `apple2e` / `apple2e_enhanced` / `apple2gs`, per [ConfigFiles.md](ConfigFiles.md)). Choplifter’s badge is Apple II or ][+, not a IIe.
- A Disk II in slot 6.
- `[[storage]]` entries in listed order, drive 1 then drive 2, `image` relative to the config (`disks/<filename>`).

Drive 1 is the boot disk. Further disks stay in the tree, unmounted, for the Simple Browser.

## Two ways to pick a disk image

Mounting only needs a path string. `mount_media` opens whatever path it is given. The two pickers differ in where that path comes from.

### Local computer

The drive button (outside pack mode) and “Open from this computer…” (inside it) open a file picker for a disk image on this computer. Guest writes go back to that file, the same way a desktop mount writes through to the image path.

That write-through needs a writable file handle (`showOpenFilePicker`). The dialog the web build uses now does not provide one. `<input type="file">` only supplies bytes; the shell copies them to `/uploads/<name>` and mounts that MEMFS file, so later writes stay in the copy and never reach the disk the user picked. Drag-and-drop works the same way. The local picker in this spec replaces that dialog.

### Simple Browser

A modal we draw. It is not a filesystem explorer. It lists the disk images that belong to this pack: filename, and which drive currently has it mounted, if any.

Choosing one mounts that existing MEMFS path on the drive whose button was clicked. No download, no `<input type="file">`.

The list is the pack’s images, not every file under MEMFS (`/resources`, `/uploads`, the config). A directory walk of `/packs/<id>/disks` is enough; the shell can also pass the list in when the session starts.

In pack mode the drive button opens this modal. “Open from this computer…” on the modal opens the local picker and mounts that file on the same drive. Writes to it go to the local image. They are not copied into the pack and they are not uploaded with the Collection save.

## Saving back to arQyv

Guest writes already land in the mounted file: floppies via `fopen`, SmartPort / `.hdv` write-through. For a pack image that file is in MEMFS, and Save uploads the dirty images. For a local-picker image that file is the one on this computer, and the write is the save. Do not upload pack images per sector, and do not upload on every block.

Save points:

1. An explicit **Save** in the player.
2. Leaving the title (power-off back to the picker, or launching a different item), after any dirty-disk prompt the desktop build already uses.
3. An attempt to close the tab, when the pack’s MEMFS images are dirty.

The shell captures that close attempt. `beforeunload` cannot show our own dialog and cannot wait for an upload, and `fetch` keepalive / `sendBeacon` cannot carry a disk image, so the handler cancels the close and the shell then asks: save the MEMFS pack back to the user’s Collection, or close without saving. Save runs to completion, then the tab may close. Choosing not to save closes with the Collection copy unchanged.

A killed tab or a crash never delivers that prompt. Changes since the last successful upload are dropped — same rule as a native pack that never finished its rewrite.

What gets uploaded is the user’s document, not the catalog:

- Dirty disk images, and a config that changed (mounted set, BRAM on a IIgs).
- The server keeps the user’s pack. Unchanged images stay references to the catalog blobs; the client does not re-upload a pristine WOZ.
- A disk opened from this computer is not part of the pack. Its writes go to that local file and are not uploaded.
- The public `/catalog/<id>/` object is never the PUT target.

Relaunch overlays those saved images on a fresh catalog snapshot.

Conflicts are last-write-wins on the user’s pack, as in ProtocolHandlers. Two sessions that both save will drop one of them. Warning when the server copy changed while we had it open can come later.

Packs stay within the size already assumed for `.gs2pack` (on the order of 200 MB). The web heap is 256 MB to start and 1 GB maximum, and MEMFS holds the whole working tree plus the guest RAM, so a pack that fits the native cap can still be tight here. The shell should refuse to start a pack it cannot fetch and write, with a clear error, rather than booting a machine with missing images.

## Phasing

| Slice | |
|-------|--|
| Player page on arqyv.net, COOP/COEP, same-origin fetch of one Title’s disks | First |
| Synthesized `.gs2`, `Module.arguments`, auto-start | First |
| Simple Browser as the drive-open dialog in this mode; local picker secondary | First |
| Save to the user’s Collection copy, including the tab-close prompt; overlay on next launch | First |
| Stored `machine.gs2` for titles the badge cannot describe | When a title needs it |
| Collection picker (signed-in list) launching the same way | After Save exists |
| Token handoff so `gssquared.net/live` can launch a pack | Later, only if the player must stay on that host |

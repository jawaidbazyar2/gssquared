# arQyv-served GS2 packs

Status: arQyv Title/Collection fetch and cloud save are not implemented. Local `.gs2pack` open, extract, rewrite, and the Simple Browser are implemented (see ProtocolHandlers).

This is how GSSquared, desktop and web, plays an [arQyv](https://arqyv.net) **Title** or **Collection** item: fetch the pack, boot it, let the user swap disks inside that pack, and store disk writes back to that user’s arQyv cloud storage.

The document model is the `.gs2pack` in [ProtocolHandlers.md](ProtocolHandlers.md): one uncompressed ustar holding a config plus its disk images. This file is the arQyv session of that model, on both hosts. The player unpacks the ustar into a working tree, mounts those files, and on save rebuilds a ustar. Opening a `.gs2pack` the user already has (Finder, Explorer, or a web drop), `gssquared:` URLs, and Steam Cloud stay in ProtocolHandlers. Web disk mounting as it works today is in [Emscripten.md](Emscripten.md).

## Mode

**arQyv-served-gs2pack** is a session mode, not a second emulator.

| | Ordinary session | arQyv-served-gs2pack |
|--|-----------------|----------------------|
| How it starts | System Select. User mounts disks themselves. | A Title or Collection item. The player fetches the pack and auto-starts that config. |
| Drive “open” | This computer’s file picker. | Simple Browser (this pack’s images). |
| Where writes go | The local disk image the user opened. | Pack images: the user’s arQyv cloud storage. A disk opened with the local picker: that local file. |

The local picker still exists in pack mode, as a secondary action (“Open from this computer…”). The drive button’s primary dialog is the Simple Browser. A disk opened that way writes back to the local image, in either mode. It is not stored in the Collection copy.

## Where the player runs

Desktop GSSquared fetches the same ustar over HTTPS and then follows the extract / launch / rewrite path in ProtocolHandlers. A Collection fetch sends the user’s arQyv credentials. A Title fetch does not need them.

The web player is hosted on arqyv.net, on a path that sends the same COOP/COEP headers the web build needs for pthreads (`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`).

Title disk files already live on arqyv.net under `/catalog/<id>/disks/…`. Same-origin fetches are allowed under `require-corp`. The Collection session cookie is first-party there.

`gssquared.net/live` cannot do this as it stands. That page is cross-origin isolated, and catalog responses send neither CORS nor `Cross-Origin-Resource-Policy`, so a `fetch()` of a disk (and an iframe of `/titles` or `/collection`) is blocked. `Cross-Origin-Opener-Policy: same-origin` also clears `window.opener`, so a popup on arqyv.net cannot post a selection back.

If the player stays on `gssquared.net/live` later:

- Catalog disk URLs and a play manifest need CORS and `Cross-Origin-Resource-Policy: cross-origin`.
- Collection needs a sign-in redirect that returns a short-lived token for that one item. A credentialed cross-site fetch of the arqyv session cookie is not the plan.

## Titles and Collection

Both resolve to one ustar. The player does not crawl the catalog. The server builds that archive as a forward stream: for each member a 512-byte ustar header (the size is already known), the bytes, then padding to 512, and two zero blocks at the end. A generated `machine.gs2` is finished in memory so its size is known before its header. Catalog disk blobs already have sizes. Nothing is staged in a temp file. The stream is uncompressed ustar. Gzip is not accepted.

- **Title** — public curated catalog entry (`/titles/<slug>`). Metadata the player needs: platform badge, and the ordered disk list (role, filename, URL). Example: 221B Baker Street is two WOZs; Choplifter is one WOZ and the page says it will not run on later models. `Profiles.txt` is a catalog index and is not a boot file.
- **Collection** — the signed-in user’s saved titles (`/collection`, login required). A row is a title plus that user’s saved images, when they have any.

Public catalog objects are immutable. Playing a Title streams the catalog snapshot as that ustar. The first save creates or updates the user’s Collection copy. Nothing writes back onto `/catalog/<id>/`.

Playing a Collection item streams the same way, with the user’s saved images already in place of the catalog members they replace.

## Working tree and auto-start

GSSquared cannot mount archive members. Both hosts unpack the ustar into a working tree before the emulator opens anything, then launch `machine.gs2` from that tree. Relative `image` paths resolve from the config file, and `mount_media` opens those paths.

Desktop extracts to a local temp directory, not a cloud folder, and passes that `machine.gs2` on `argv`. `SDL_AppInit` already loads a config on `argv` and skips System Select. On close it rewrites the ustar, as in ProtocolHandlers.

The web shell unpacks into MEMFS, the Emscripten in-memory filesystem:

```
/packs/<id>/machine.gs2
/packs/<id>/disks/boot.woz
/packs/<id>/disks/data.woz
```

The web shell does not set `Module.arguments` today. Pack mode does, from `preRun`, after the writes:

```javascript
Module.arguments = ['/packs/<id>/machine.gs2'];
```

`preRun` may be asynchronous so the runtime waits while the pack downloads. The click that chooses Play is the user gesture that unlocks audio. A `.gs2pack` the user opens in the page (picker or drop) takes this same unpack path.

The streamed ustar always contains `machine.gs2`. The GS2 file should have full title and description filled out for display purposes. A stored config (and a `… Settings.txt`) ships with the title when the machine is not “floppies in slot 6”: IIgs, hard disk, a card the badge does not imply. Otherwise the server writes a minimal config into the archive from the badge and the disk list:

- Platform from the badge (`apple2` / `apple2plus` / `apple2e` / `apple2e_enhanced` / `apple2gs`, per [ConfigFiles.md](ConfigFiles.md)). Choplifter’s badge is Apple II or ][+, not a IIe.
- A Disk II in slot 6.
- `[[storage]]` entries in listed order, drive 1 then drive 2, `image` relative to the config (`disks/<filename>`).

Drive 1 is the boot disk. Further disks stay in the tree, unmounted, for the Simple Browser.

## Two ways to pick a disk image

Mounting only needs a path string. `mount_media` opens whatever path it is given. The two pickers differ in where that path comes from.

### Local computer

The drive button (outside pack mode) and “Open from this computer…” (inside it) open a file picker for a disk image on this computer. Guest writes go back to that file.

Desktop already does this: the native dialog returns a path, and the mount writes through to it.

On the web, write-through needs a writable file handle (`showOpenFilePicker`). The dialog the web build uses now does not provide one. `<input type="file">` only supplies bytes; the shell copies them to `/uploads/<name>` and mounts that MEMFS file, so later writes stay in the copy and never reach the disk the user picked. Drag-and-drop works the same way. The local picker in this spec replaces that dialog.

### Simple Browser

A modal we draw. It is not a filesystem explorer. It lists the disk images that belong to this pack: filename, and which drive currently has it mounted, if any.

Choosing one mounts that existing working-tree path on the drive whose button was clicked. No download, and on the web no `<input type="file">`.

The list is the pack’s images, not every file in the working tree (on the web: `/resources`, `/uploads`, the config). A directory walk of the pack’s `disks/` directory is enough; the player can also pass the list in when the session starts.

In pack mode the drive button opens this modal. “Open from this computer…” on the modal opens the local picker and mounts that file on the same drive. Writes to it go to the local image. They are not copied into the pack and they are not uploaded with the Collection save.

## Saving back to arQyv

Guest writes already land in the mounted file: floppies via `fopen`, SmartPort / `.hdv` write-through. For a pack image that file is in the working tree (a temp directory on desktop, MEMFS on the web). On save the player rebuilds a ustar from that tree. The upload is the dirty members and a config that changed; the server streams a new ustar for the user’s pack and fills unchanged members from catalog blobs. For a local-picker image that file is the one on this computer, and the write is the save. Do not upload pack images per sector, and do not upload on every block.

Save points:

1. An explicit **Save** in the player.
2. Leaving the title (power-off back to the picker, or launching a different item), after the dirty-disk prompt.
3. Quitting the desktop app, after that same prompt. The rewrite is the one in ProtocolHandlers; the upload to arQyv finishes before the process exits.
4. An attempt to close the browser tab, when the pack’s working-tree images are dirty.

On the web, the shell captures that close attempt. `beforeunload` cannot show our own dialog and cannot wait for an upload, and `fetch` keepalive / `sendBeacon` cannot carry a disk image, so the handler cancels the close and the shell then asks: save the pack back to the user’s Collection, or close without saving. Save runs to completion, then the tab may close. Choosing not to save closes with the Collection copy unchanged.

A killed process or a crash never delivers that prompt. Changes since the last successful upload are dropped — same rule as a local pack that never finished its rewrite.

What gets uploaded is the user’s document, not the catalog:

- Dirty disk images, and a config that changed (mounted set, BRAM on a IIgs).
- The server keeps the user’s pack. Unchanged images stay references to the catalog blobs; the client does not re-upload a pristine WOZ.
- A disk opened from this computer is not part of the pack. Its writes go to that local file and are not uploaded.
- The public `/catalog/<id>/` object is never the PUT target.

Relaunch overlays those saved images on a fresh catalog snapshot.

Conflicts are last-write-wins on the user’s pack, as in ProtocolHandlers. Two sessions that both save will drop one of them. Warning when the server copy changed while we had it open can come later.

Packs stay within the size already assumed for `.gs2pack` (on the order of 200 MB). On the web the heap is 256 MB to start and 1 GB maximum, and MEMFS holds the whole working tree plus the guest RAM, so a pack that fits the desktop cap can still be tight there. Either host should refuse to start a pack it cannot fetch and write, with a clear error, rather than booting a machine with missing images.

## Phasing

| Slice | |
|-------|--|
| Fetch one Title’s ustar, unpack, auto-start (desktop `argv`, web `Module.arguments`) | First |
| Web player page on arqyv.net, COOP/COEP, same-origin fetch | First |
| Synthesized `machine.gs2` in the streamed ustar | First |
| Simple Browser as the drive-open dialog in this mode; local picker secondary | Local pack: now. arQyv session: first |
| Save to the user’s Collection copy, including desktop quit and the web tab-close prompt; overlay on next launch | First |
| Stored `machine.gs2` for titles the badge cannot describe | When a title needs it |
| Collection picker (signed-in list) launching the same way | After Save exists |
| Token handoff so `gssquared.net/live` can launch a pack | Later, only if the player must stay on that host |

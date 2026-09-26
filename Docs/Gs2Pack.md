# GS2 Packs (`.gs2pack`)

A **GS2 pack** is one file that holds a virtual Apple II and the disk images that machine uses. The extension is **`.gs2pack`**. Open the file and that machine boots with those disks mounted. System Select is skipped.

The pack filename is the document name (`Choplifter.gs2pack`, `MyIIe.gs2pack`). Inside, the machine description is always a file named `machine.gs2`, and the disks stay in their own image formats.

A loose `.gs2` file is still the format you edit by hand or from the config editor. A pack is that same config, plus the images it names, stored as a single document you can copy, mail, or download.

---

## Purpose and use cases

Use a pack when the thing you want to keep or hand to someone is **a machine together with its media**, as one file.

* **Launch a ready-made machine.** Double-click the pack, choose it with **File → Launch Config…**, or pass the path on the [command line](CommandLine.md). GSSquared boots `machine.gs2` and mounts the images it names.

* **Give someone a title or a setup.** A pack travels as one file: the computer (model, cards, display, which drives are mounted) and the disk images those drives use. The recipient opens that file. Relative paths inside the pack stay valid because the images travel with the config.

* **Keep guest writes with the machine.** Saving a game, copying a file in ProDOS, or changing which pack disk is in a drive updates the pack when you close the machine. The `.gs2pack` you opened is replaced with a complete copy that includes those changes. Copy that file and the saved disks come with it.

* **Swap disks that belong to the title.** While a pack is running, the drive picker lists the images stored in the pack. Mounting one of them records that choice in `machine.gs2`, and the next time the pack opens, that disk is the one in the drive.

* **Open a pack from a link.** A `gssquared:https://…/name.gs2pack` link asks you to confirm, downloads that one pack, and launches it. The downloaded file is the same ustar document described here. Details of the link itself are in [File types and URL protocols](ProtocolHandlers.md).

* **Look inside, or build one, with ordinary tools.** A pack is an uncompressed ustar archive. `tar -tf pack.gs2pack` lists the contents. `tar -xf pack.gs2pack` extracts `machine.gs2` and the disk images. Publishers and tinkerers can assemble a pack with `tar`, as [below](#building-a-pack).

A pack records configuration and media. CPU registers and guest RAM stay outside it. Full machine save states are a separate, [planned](SaveAndRestore.md) feature.

---

## A container for metadata and disk images

The files a virtual Apple II reads and writes stay the disk-image formats in [Storage & Disks](Storage.md): `.woz`, `.dsk`, `.do`, `.po`, `.2mg`, `.hdv`, and the others. Each of those describes tracks or blocks. GSSquared mounts one in a Disk II, the IIgs disk port, or a SmartPort drive.

A pack is the layer around those files: **metadata plus a container**. It has no track layout, block size, or filesystem of its own, and a drive mounts an image, not the `.gs2pack`. Open a pack to boot a machine. Mount a disk image to put media in a drive.

The archive holds these members:

| Piece | Role |
|-------|------|
| `machine.gs2` | Which computer this is, which cards are installed, and which images start in which drives |
| Other members | The disk images themselves, stored byte for byte in their own formats |

Extract `disks/boot.woz` from a pack and you still have a `.woz` file. Another program that reads WOZ can use it. Nothing in the container converts, compresses, or wraps the image.

If you only want to boot a disk you already have, mount that image on a machine you already use. If you want “this IIe, these cards, these disks” to stay one file, put them in a pack.

---

## `machine.gs2`

Every pack contains a member named **`machine.gs2`** at the top of the archive. That file is an ordinary GSSquared config. GSSquared boots the pack by loading it. The name is always `machine.gs2`, whatever the `.gs2pack` file is called.

`machine.gs2` is the metadata: platform, slot cards, display, speed, serial attachments, and the `[[storage]]` entries that mount images. On an Apple IIgs it also carries battery RAM (Control Panel / NVRAM) with the machine, the same way a loose `.gs2` does. Paths in that file are relative to the config. After the pack is opened, that directory is the root of the archive, so an image stored as `disks/boot.woz` is named like this:

```toml
[[storage]]
slot = 6
drive = 1
image = "disks/boot.woz"
```

How to write the rest of the file — platforms, cards, displays, and every field — is the `.gs2` config format. The field-by-field reference is [System Config TOML (`.gs2`)](SystemConfigTOML.md). A walkthrough for writing a config by hand is [Writing Config Files Manually](ConfigFiles.md).

The config editor saves a loose `.gs2`. To change cards or the platform inside a pack, extract it, edit `machine.gs2` with those guides, and build the archive again. Drive changes you make in the Control Panel while the pack is running are written back into `machine.gs2` for you.

---

## What a pack contains

A typical pack looks like this when you list it:

```text
machine.gs2
disks/boot.woz
disks/data.hdv
```

`machine.gs2` is required. Disk images conventionally live under `disks/`. The Control Panel’s pack disk list shows the regular files in that directory. Other regular files may sit beside them; GSSquared will extract those too, and `machine.gs2` can refer to them with a relative path.

Member names are portable relative paths: forward slashes, no leading slash, and no `..`. A name fits ustar’s limits when it is at most 100 bytes, or a directory prefix of at most 155 bytes plus a final component of at most 100 bytes. `machine.gs2` and `disks/boot.woz` fit. A pack may be at most **200 MB**.

---

## File format

A `.gs2pack` is an **uncompressed POSIX ustar** archive. ustar is the classic `tar` dialect. Stock `tar` can list and extract a pack because the file really is that archive; the `.gs2pack` extension tells GSSquared to boot it as a machine.

Each member is stored like this:

1. A **512-byte header**. The header’s magic is the text `ustar` and the version is `00`. The header names the member and records its size in octal. A checksum covers the header only.
2. The **member bytes**, unchanged. A disk image is the same file you would mount on its own.
3. **Padding** with zero bytes out to the next multiple of 512.

Two 512-byte blocks of zeros end the archive. Member bytes are stored uncompressed; a gzip wrapper around the tar is a different file, and GSSquared opens the ustar bytes themselves. On macOS, Windows, and Linux the pack is that single archive file.

List and extract with the `tar` you already have:

```bash
tar -tf Choplifter.gs2pack
tar -xf Choplifter.gs2pack
```

### Writing ustar on purpose

Building a pack with `tar` has to request the ustar dialect:

```bash
tar --format ustar -cf Choplifter.gs2pack machine.gs2 disks/boot.woz disks/data.hdv
```

Run that from the directory that contains `machine.gs2` and `disks/`, so the names stored in the archive are `machine.gs2` and `disks/boot.woz`.

A plain `tar -cf` does not write this format. On macOS and Windows it writes a pax archive. On Linux, GNU tar writes its own dialect. Those archives add extended headers GSSquared does not accept. Pass `--format ustar`.

Directory entries inside the archive (tar’s marker for the `disks` folder itself) are ignored. The files are what matter. GSSquared accepts normal file members and that directory marker, and it requires `machine.gs2`.

---

## Opening a pack

* **Finder, Explorer, or Open With** — open the `.gs2pack`. On macOS this is delivered to GSSquared if it is already running. On Windows, opening a document starts GSSquared with the path on its command line. Association details are in [File types and URL protocols](ProtocolHandlers.md).
* **File → Launch Config…** — on the System Select screen, pick a `.gs2pack` as well as a `.gs2` or `… Settings.txt`. The pack boots immediately.
* **Command line** — `GSSquared Choplifter.gs2pack` skips System Select and boots the pack. Closing that session quits the app, the same as launching a `.gs2` from the command line. See [Command line](CommandLine.md).
* **A `gssquared:` link** — confirms, downloads the pack, and launches it. See [File types and URL protocols](ProtocolHandlers.md).

**Edit…** on System Select edits a loose config. A pack launches a machine: use Launch Config, or extract it and edit `machine.gs2`.

GSSquared unpacks the archive into a temporary working directory, loads `machine.gs2` from that directory, and mounts images from the relative paths in the config. The `.gs2pack` file itself stays put while you play.

When you power off, switch to another config, or quit, GSSquared writes the working files back into the pack: `machine.gs2` (including drive changes and IIgs battery RAM) and the disk images, including bytes the guest wrote. It writes a temporary file beside the pack and then replaces the original, so a reader never sees a half-written archive. Quit or switch away to keep your progress. A crash before that rewrite leaves the pack as it was when the session started.

---

## Disks while a pack is open

Open the Control Panel and click a drive, the same as any other machine. GSSquared lists the files in the pack’s `disks/` directory instead of starting in your system file picker. Choose one to mount it. That mount is saved into `machine.gs2`, so the pack remembers the drive the next time it opens.

**Open from this computer…** still mounts an image that lives outside the pack. That image stays where it is. It is not copied into the archive when the pack is saved. Disks you want to travel inside the document belong in the archive, under `disks/`, with a matching `image` path in `machine.gs2`.

---

## Building a pack

1. Write `machine.gs2` as a normal config. Follow [Writing Config Files Manually](ConfigFiles.md), and use [System Config TOML (`.gs2`)](SystemConfigTOML.md) for the fields. Point every `image` path at a file you will put in the archive, usually under `disks/`.
2. Place the images next to that config in those relative paths.
3. Create the archive with `tar --format ustar`, as in [Writing ustar on purpose](#writing-ustar-on-purpose).
4. Open the `.gs2pack` in GSSquared.

To change the contents later, extract with `tar -xf`, edit, and build the archive again. Do that while GSSquared does not have the pack open. Closing the machine rewrites the pack from the files it unpacked, and that rewrite would replace an archive you edited underneath it.

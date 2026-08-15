# Host FST

Host FST lets GS/OS on an Apple IIgs see a folder on your real computer as a GS/OS volume named **`:Host`** (also `/Host`). Copy files between the host and the emulated machine without packing them into a disk image first.

Host FST is built into every Apple IIgs configuration in GSSquared (ROM 01 and ROM 03). It is **not** a slot card.

## One-time setup: install the drivers into GS/OS

You only need to do this once per GS/OS boot volume (or after you rebuild that System folder).

1. Boot **GS/OS** on an Apple IIgs config that has **BazFast** available.
2. Choose **File → Mount Drivers**.  
   That mounts the write-protected `/GS2.DRIVERS` volume on an empty BazFast drive.  
   (Grayed out if there is no BazFast, emulation is not running, or every BazFast icon already has media — free a drive and try again.)
3. On `/GS2.DRIVERS`, open the **`HOST.FST`** folder. Copy these two files into your GS/OS System folder:

   | File on drivers volume | Copy to |
   |------------------------|---------|
   | **Host.FST** | `System/FSTs` |
   | **Host.Driver** | `System/Drivers` |

4. Restart GS/OS (or otherwise let it load the new FST/driver) so Host FST becomes active.
5. You can uncheck **File → Mount Drivers** when you no longer need the drivers volume mounted.

## Choose the host-side folder

1. With the IIgs running, press **F4** (or open the Control Panel tab).
2. Click **Host Folder…**.
3. Pick the folder on your Mac / PC / Linux machine that you want GS/OS to see as `:Host`.

If you never pick a folder, GSSquared shares your **Documents** folder. The choice is remembered in app settings.

Changing the host folder while GS/OS is running remounts `:Host` to the new path.

## Using `:Host`

After the FST is installed and loaded, the shared folder appears in the GS/OS Finder (and to apps) as **`:Host`**. Copy files to and from it like any other volume.

## Notes

- Requires an **Apple IIgs** platform and **GS/OS** (not plain ProDOS 8 on a //e).
- On **Windows**, Host FST stores ProDOS type/auxtype, Finder info, and resource forks in NTFS Alternate Data Streams (`:AFP_AfpInfo` and `:AFP_Resource`, the same convention as CiderPress and Services for Macintosh). Use an **NTFS** folder; FAT/exFAT and some network shares will not keep that metadata.
- The **Host Folder…** picker is not available in the web (Emscripten) build.
- Mount Drivers only supplies the installer disk; the Host FST itself stays built into the emulator once the two System files are installed.

## Related

- [Storage & Disks](Storage.md) — BazFast, Mount Drivers overview
- [OSD / Control Panel](OSD.md)
- [Menus](Menus.md)

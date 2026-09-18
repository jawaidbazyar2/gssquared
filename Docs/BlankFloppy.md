# Blank Disk Images

**File → New Disk Image** creates a new disk image file on your computer. It does **not** put the disk in a drive — mount it afterwards like any other image.

The menu works at the system picker and while a machine is running.

## Create an image

1. Choose **File → New Disk Image**, then one of the types below.
2. Pick a folder and filename in the save dialog. GSSquared suggests a name; you can change it.
3. Cancel writes nothing. Confirm writes the file and closes the dialog.

Then mount it: **File → Drives**, the Control Panel (**F4**), or drag-and-drop onto a drive. See [Storage & Disks](Storage.md).

## What to pick

| Menu item | File | What you get |
|---|---|---|
| 5.25 Unformatted | `.woz` | Empty 5.25″ disk — no filesystem. Format it from DOS, ProDOS, Copy II Plus, or similar. |
| 5.25 Formatted DOS 3.3 | `.woz` | 5.25″ disk already formatted for DOS 3.3. Ready to `SAVE` / `INIT` usage as a data disk. |
| 5.25 Formatted ProDOS | `.woz` | 5.25″ disk already formatted for ProDOS. |
| 3.5 Formatted ProDOS | `.woz` | 800K 3.5″ disk already formatted for ProDOS. |
| 32M HD Unformatted | `.hdv` | 32 megabyte hard-disk image, all zeros. Format it from ProDOS or GS/OS (BazFast). |

“ProDOS” here means a **ProDOS-formatted** volume, not a particular ProDOS version.

**Formatted** floppies can be catalogued and written immediately. **Unformatted** images are blank media: the guest OS has to format them first.

The 32M HD is a raw block file (not WOZ, not 2MG). Mount it on **BazFast**. ProDOS 8’s usual size limit is 32M.

## After you save

The new file is just a file on your Mac, PC, or Linux machine. Insert it the same way you would a disk you downloaded:

- **File → Drives** — pick the slot/drive
- Control Panel (**F4**) — click an empty drive
- Drag the file onto GSSquared, then onto a drive

5.25″ images go on a 5.25″ drive. 3.5″ / 800K images go on a 3.5″ drive. The 32M `.hdv` goes on BazFast (800K or larger block images only).

## Related

- [Menus](Menus.md) — File menu
- [Storage & Disks](Storage.md) — formats, mounting, write-back
- [OSD / Control Panel](OSD.md)

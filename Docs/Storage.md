# Storage & Disks

GS2 supports the following virtual disk media formats:

| File Type | Sizes | Description |
|--|--|--|
| .do / .po | 140K | DOS33 or ProDOS ordered |
| .dsk | 140K | DOS33 ordered |
| .nib | 140K | Nibblized 5.25" |
| .2mg | 140K, 800K | 5.25 and 3.5 |
| .woz | 140K, 800K | 5.25 and 3.5 |
| .hdv, .img | any | Can be any size, raw block device |

Woz format is the heart of GS2 floppy emulation. GS2 supports copy-protected 5.25 and 3.5 disks in Woz format, even ones with half tracks, quarter tracks, spiral tracks, weak bits, etc etc. Virtually any copy-protected Woz image should work fine in GS2.

## Mounting disks

There are several ways to mount disks depending on your preference.

### Control Panel

Open the Control Panel - F4 or the OSD Button.

Click a disk drive that you want to mount a disk on. If a disk is already mounted there, clicking will unmount that disk.

If the disk is a 5.25 and has been modified, you will be asked if you want to Save or Discard changes, or Cancel the unmount.

Clicking on an unmounted drive, will present your system's file picker, and you can then select a disk image.

If the disk image is not compatible with that drive, you will see an error at the top of the screen.

Once done, Close the OSD (F4 again or the OSD Button).

### Menus

You can choose a storage device from the menu, and mount/unmount images to the device the same as above.

### Drag and Drop

If you drag a file from your operating system Finder / Explorer / Linuxything on top of GS2, the Control Panel will automatically open, and then you can drag the disk image on top of the drive you want to mount on.

## Drive-specific details:

* BazFast

BazFast is a SmartPort hard drive device - you can mount any kind of media 800K or larger to the BazFast device. You can mount a 140K media, however, ProDOS assumes a 140K media is a Disk II and will usually crash trying to use such a disk.

You can have up to 10 media mounted on BazFast. The upper limit on volume size under ProDOS 8 is 32M. The limit on GS/OS is 2 GiB.

* 5.25" Drives

You can mount any exactly 140K disk image to a 5.25" drive. Changes written to these drives are held in memory until you unmount them. At that time the system will ask if you want to save changes back to the original disk image file.

* 3.5" Drives

You can mount any 800K image onto a 3.5 drive. As with a 5.25 floppy, changes are hald in memory until you unmount, and you'll be asked then if you want to save changes back to the original disk image file.

## Write-protection

If the disk image permission in the host operating system is Read-Only (i.e., writes not allowed) then the image will be mounted into GS2 as Write Protected.

## Floppy Emulation Speeds

In a real Apple II or IIe or IIgs, the 5.25 Floppy disk is clocked to the I/O bus clock - exactly 1.0205MHz no matter what.

In GS2, however, in Apple II / IIe mode, the Floppy disk is clocked to the CPU clock, even if you have the CPU speed set to 2.8, 7.1, or 14.3MHz. This allows us to accelerate floppy reads/writes safely. This is especially useful with the right-mouse or INS "accelerate" mode.

In IIgs mode, the Floppy disk is clocked to the bus clock - always 1MHz. This is because the IIgs speed-shifts on its own. Don't worry, the 5.25 and 3.5 floppies still perform quite well, because lots of the floppy processing on a IIgs can happen at the fast CPU speed.


## Some Notes about Disk Formats

Whatever disk image format you mount, GS2 will always write back to the same format on disk - it does not convert your disk images, but assumes you want to update / keep the same format.

Floppy disk drives always convert whatever format is used on disk, to Woz format *internally*. This is so the virtual floppy disk always operates exactly the same way in the emulator. But when you unmount and choose to Save changes, GS2 converts the internal Woz format back to whatever format you had on the disk. GS2 thus cannot be used to convert other formats to Woz or vice-versa.

If you want to convert a file from a block format to Woz format or vice-versa, use a program like CiderPress2, or AppleSauce.

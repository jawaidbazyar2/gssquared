# Storage & Disks

GS2 supports the following virtual disk media formats:

| File Type | Sizes | Description |
|--|--|--|
| .do / .po | 140K | DOS33 or ProDOS ordered |
| .dsk | 140K | DOS33 ordered |
| .nib | 140K | Read only Nibblized 5.25" |
| .2mg | 140K, 800K | 5.25 and 3.5 |
| .woz 1.0 | 140K, 800K | 5.25 and 3.5 |
| .woz 2.0 | 140K, 800K | 5.25 and 3.5 |
| .hdv, .img | any | Can be any size, raw block device |
| .pmap | any .2mg, .hdv, .img, .po | "Partition Map" to mount multiple hard drive images at once |

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

You can choose a storage device from the menu (**File → Drives**), and mount/unmount images to the device the same as above.

### Mount Drivers

**File → Mount Drivers** mounts GSSquared’s built-in drivers disk (`/GS2.DRIVERS`) onto an empty BazFast drive. Check the menu item to mount; uncheck it to unmount that same drive.

The image is write-protected. It currently includes the Host FST driver (see [Host FST](#host-fst) below). The menu item is grayed out if the current machine has no BazFast card, or if emulation is not running. If all six BazFast icons already have media mounted, Mount Drivers does nothing until you free a drive.

### Drag and Drop

If you drag a file from your operating system Finder / Explorer / Linuxything on top of GS2, the Control Panel will automatically open, and then you can drag the disk image on top of the drive you want to mount on.

## Drive-specific details:

* 5.25" Drives

You can mount any exactly 140K disk image to a 5.25" drive. Changes written to these drives are held in memory until you unmount them. At that time the system will ask if you want to save changes back to the original disk image file.

Because .nib format loses information about which FF nybbles are 10-bit sync bytes and which are regular data bytes, it is not possible to safely/accurately convert .nib to .woz. We can load them, however, we cannot write them. So they are mounted readonly (write protected).

* 3.5" Drives

You can mount any 800K image onto a 3.5 drive. As with a 5.25 floppy, changes are hald in memory until you unmount, and you'll be asked then if you want to save changes back to the original disk image file.


* BazFast

BazFast is a SmartPort hard drive device - you can mount any kind of media 800K or larger to the BazFast device. You can mount a 140K media, however, ProDOS assumes a 140K media is a Disk II and will usually crash trying to use such a disk.

BazFast transfers data into the emulated machine using a kind of "Super DMA" that takes no processor cycles. It's about as fast as it can get.

You can have up to 32 media mounted on BazFast. The upper limit on volume size under ProDOS 8 is 32M. The limit on GS/OS is 2 GiB.

BazFast presents as six hard disk icons in the Control Panel. Each one of these icons can mount any number of images, up to the limit of 32 total across all six icons. This lets each device operate similarly to a single hard disk with partitions. Each "partition" or image gets a unique SmartPort unit number.

Unlike the other drive types that *replace* any existing image when mounting a new one, BazFast is additive - if you drag a new image over a device that has a mount, the new image is ADDED to the images there.

Clicking on a BazFast drive that has images mounted, will unmount -all- the images under that icon.

In addition to .hdv, .2mg, .po, .img, BazFast can mount a .pmap file. .pmap is a text file that contains a list of filenames of image files. For example:

wita2gs.pmap:
```
A_Boot.po
B_Action Games.hdv
C_Adventure and Sim Games.hdv
D_Board Games and RPGs.hdv
E_Sports and Unreleased Games.hdv
F_Demos.hdv
G_Games with Path Mods.hdv
```

If you mount this pmap on a BazFast device, all 7 images will be mounted at once, a big time-saver for those with complex setups, multi-disk collections like What Is the AppleIIgs or Golden Orchard.

## Host FST

Host FST lets GS/OS on an Apple IIgs see a folder on your real computer as a GS/OS volume named **`:Host`** (or `/Host` under ProDOS 16). Copy files between the host and the emulated machine without packing them into a disk image first.

Host FST is built into every Apple IIgs configuration in GSSquared (it is not a slot card). To use it:

1. Boot GS/OS with BazFast available.
2. Use **File → Mount Drivers** to attach `/GS2.DRIVERS`, then install or copy the Host FST from that volume into your GS/OS setup (see the `HOST.FST` folder on the drivers disk).
2a. Host.FST should go into System/FSTs
2b. Host.Driver should go into System/Drivers
3. Open the Control Panel (**F4**) and click **Host Folder…** to choose which host directory is shared. If you never pick one, GSSquared uses your Documents folder. The choice is remembered in settings.
4. After the FST is active under GS/OS, the shared folder appears as the `:Host` volume.

Changing the host folder while GS/OS is running remounts the Host volume to the new path. Host FST is IIgs / GS/OS oriented; the folder picker is not available in the web (Emscripten) build.

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

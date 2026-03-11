# Storage & Disks

GS2 supports the following virtual disk media formats:

| File Type | Sizes | Description |
|--|--|--|
| .do / .po | 140K | DOS33 or ProDOS ordered |
| .dsk | 140K | DOS33 ordered |
| .nib | 140K | Nibblized 5.25" |
| .2mg | 140K, 800K | 5.25 and 3.5 |
| .hdv | 32M | Can be any size, raw block device |

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

On systems that have system menus defined, you can choose the storage device from the menu, and mount/unmount otherwise the same as above.

### Drag and Drop

If you drag a file from your operating system Finder / Explorer / Linuxything on top of GS2, the Control Panel will automatically open, and then you can drag the disk image on top of the drive you want to mount on.

## Drive-specific details:

* 3.5" Drives

You can mount any 800K or bigger disk image file onto the 3.5 drives, including hard drive images, up to 32MB. Changes to these images are written immediately whenever an emulated program writes to disk.

* 5.25" Drives

You can mount any exactly 140K disk image to a 5.25" drive. Changes written to these drives are held in memory until you unmount them. At that time the system will ask if you want to save changes back to the original disk image file.

## Write-protection

If the disk image is set in the host operating system as Read-Only (i.e., writes not allowed) then the image will be mounted into GS2 as Write Protected.

## Display Configuration

In the OSD, there are buttons to change the display engine - NTSC, RGB, and Monochrome. And, buttons to change the Monochrome color (green, amber, white).

* F2 cycles through the display engines.  
* F5 toggles between pixel-blur and rectangular. pixel-blur provides a little more "analog" upscaling of Apple II dots to modern displays. Rectangular performs an exact square upscaling/downscaling.
* F3 toggles between Full-Screen and Windowed modes.

These are matters of personal preference, so you get to pick the one you like best.


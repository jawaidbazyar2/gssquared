/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * 
   * $C0S0 - Phase 0 Off
   * $C0S1 - Phase 0 On
   * $C0S2 - Phase 1 Off
   * $C0S3 - Phase 1 On
   * $C0S4 - Phase 2 Off
   * $C0S5 - Phase 2 On
   * $C0S6 - Phase 3 Off
   * $C0S7 - Phase 3 On
   * $C0S8 - Turn Motor Off
   * $C0S9 - Turn Motor On
   * $C0SA - Select Drive 1
   * $C0SB - Select Drive 2
   * $C0SC - Q6L
   * $C0SD - Q6H
   * $C0SE - Q7L
   * $C0SF - Q7H

Q6 and Q7 are 1-bit flags, states. if you trigger Q6L, then Q6 goes to low.

Q6  Q7
 L   L   READ
 H   L   Sense Write Protect or Prewrite
 L   H   Write
 H   H   Write Load

But these also serve multiple purposes, Q6L is the read register.
Q7H is where you write data to be shifted.

Q7L - Status Register.

Read Q6H, then Q7L:7 == write protect sense (1 = write protect, 0 = not write protect).
   (Reads the "notch" in the side of the diskette).

Read Q7L to go into Read mode.
then read Q6L for data. Each read shifts in another bit on the right.

Read Q6H then Q7L to go into Prewrite state. These are same bits that we use to sense
write-protect. Then we store the byte to write into Q7H. Then we read Q6L
exactly 8 times to shift the data out. 32 cycles, 8 times = 4 cycles per bit. 
We can ignore the read part, and just write whenever they STA Q7H. 

load Q7L then Q6L to go into read mode.

So, the idea I have for this:
each Disk II track is 4kbyte. I can build a complex state machine to run when
$C0xx is referenced. Or, when loading a disk image, I can convert the disk image
into a pre-encoded format that is stored in RAM. Then we just play this back very
simply, in a circle. The $C0xx handler is a simple state machine, keeping track
of these values:
   * Current track position. track number, and phase.
   * Current read/write pointer into the track
   

Each pre-encoded track will be a fixed number of bytes.

If we write to a track, we need to know which sector, so we can update the image
file on the real disk.

Done this way, the disk ought to be emulatable at any emulated MHz speed. Our 
pretend disk spins faster along with the cpu clock! Ha ha.

### Track Encoding

At least 5 all-1's bytes in a row.

Followed by :

Mark Bytes for Address Field: D5 AA 96
Mark Bytes for Data Field: D5 AA AD

### Head Movement

to step in a track from an even numbered track (e.g. track 0):
LDA C0S3        # turn on phase 1
(wait 11.5ms)
LDA C0S5        # turn on phase 2
(wait 0.1ms)
LDA C0S4        # turn off phase 1
(wait 36.6 msec)
LDA C0S6        # turn off phase 2

Moving phases 0,1,2,3,0,1,2,3 etc moves the head inward towards center.
Going 3,2,1,0,3,2,1,0 etc moves the head inward.
Even tracks are positioned under phase 0,
Odd tracks are positioned under phase 2.

If track is 0, and we get:
Ph 1 on, Ph 2 on, Ph 1 off
Then we move in one track to track. 1.

So we'll want to debug with printing the track number and phase.

Beneath Apple DOS

GAP 1:
    HEX FF sync bytes - typically 40 to 95 of them.

Address Field:
  Bytes 1-3: PROLOGUE (D5 AA 96)
  Bytes 4-5: Disk Volume
  Bytes 6-7: Track Address
  Bytes 8-9: Sector Address
  Bytes 10-11: Checksum
  Bytes 12-14: Epilogue (DE AA EB)

GAP 2: 
    HEX FF SYNC BYTES - typically 5-10

DATA Field: 
   Bytes 1-3: Prologue (D5 AA AD)
   Bytes 4 to 345 : 342 bytes of 6-2 encoded user data.
   346: Checksum
   Bytes 347-349: Epilogue (DE AA EB)

GAP 3: 
    HEX FF Sync bytes - typically 14 - 24 bytes

Address fields are never rewritten.

Volume, Track, Sector and Checksum are all single byte values, 
"odd-even" encoded:
   XX - 1 D7 1 D5 1 D3 1 D1
   YY - 1 D6 1 D4 1 D2 1 D0

Checksum is the XOR of the Volume, Track, Sector bytes. 

Page 3-20 to 21 of Beneath Apple DOS contains the write translate table and the nibblization
layout.

"50000 or so bits on a track"

Image file formats and meaning of file suffixes: do, po, dsk, hdv.
http://justsolve.archiveteam.org/wiki/DSK_(Apple_II)

In DOS at $B800 lives the "prenibble routine" . I could perhaps steal that. hehe.

 */

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "util/strndup.h"
#include "cpu.hpp"
#include "ndiskii.hpp"
#include "util/Event.hpp"
#include "util/media.hpp"
#include "util/ResourceFile.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "debug.hpp"
#include "util/mount.hpp"
#include "util/SoundEffectKeys.hpp"
#include "util/SoundEffect.hpp"
#include "util/printf_helper.hpp"
 
 
#define DEBUG_PH(slot, drive, phase, onoff) fprintf(stdout, "PH: slot %d, drive %d, phase %d, onoff %d \n", slot, drive, phase, onoff)
#define DEBUG_MOT(slot, drive, onoff) fprintf(stdout, "MOT: slot %d, drive %d, motor %d \n", slot, drive, onoff)
#define DEBUG_DS(slot, drive, drives) fprintf(stdout, "DS:slot %d, drive %d, drive_select %d \n", slot, drive, drives)
 
 /**
  * State Table:
 
  * if a phase turns on, whatever last phase turned on indicates our direction of motion.
  *         Pha  Lph  Step
  *          3    0    -1
  *          2    3    -1
  *          1    2    -1
  *          0    1    -1
  * 
  *          0    3    +1
  *          1    0    +1
  *          2    1    +1
  *          3    2    +1
  * 
  * These steps are half-tracks. So the actual tracks are this track number / 2.
  * (some images might be half tracked or even quarter tracked. I don't handle 1/4 track images yet.)
  */
 

uint8_t ndiskII_read_C0xx(void *context, uint32_t address) {
    ndiskII_controller *diskII_d = (ndiskII_controller *)context;
    return diskII_d->dc->read_cmd(address);
}

void ndiskII_write_C0xx(void *context, uint32_t address, uint8_t data) {
    ndiskII_controller *diskII_d = (ndiskII_controller *)context;
    diskII_d->dc->write_cmd(address, data);
}

void ndiskii_reset(ndiskII_controller *ndiskII_d) {
    printf("diskii_reset\n");

    ndiskII_d->dc->reset();
}
 
 
void init_slot_ndiskII(computer_t *computer, SlotType_t slot) {
   
    ndiskII_controller *diskII_d = new ndiskII_controller();
    diskII_d->computer = computer;
    diskII_d->clock = computer->clock;

    //diskII_d->drive[0] = new Floppy525(computer->sound_effect, computer->clock);
 
    // set in CPU so we can reference later
    diskII_d->id = DEVICE_ID_DISK_II;
    
    fprintf(stdout, "diskII_register_slot %d\n", slot);
 
    ResourceFile *rom = new ResourceFile("roms/cards/diskii/diskii_firmware.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load diskii/diskii_firmware.rom\n");
        return;
    }
    rom->load();
 
    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = (uint8_t *)(rom->get_data());
 
    uint16_t slot_base = 0xC080 + (slot * 0x10);
 
    for (uint16_t i = 0; i < 16; i++) {
        computer->mmu->set_C0XX_read_handler(slot_base + i, { ndiskII_read_C0xx, diskII_d });
    }
    for (uint16_t i = 8; i < 16; i++) {
        computer->mmu->set_C0XX_write_handler(slot_base + i, { ndiskII_write_C0xx, diskII_d });
    }
 
    computer->mmu->set_slot_rom(slot, rom_data, "DISK2_ROM");
 
    // register drives with mounts for status reporting
    uint64_t key = (slot << 8) | 0;

    DiskII_Controller *dc = new DiskII_Controller(computer->sound_effect, computer->clock);
    diskII_d->dc = dc;

    computer->mounts->register_storage_device(key, dc, DRIVE_TYPE_DISKII);
    computer->mounts->register_storage_device(key + 1, dc, DRIVE_TYPE_DISKII);
    
    computer->register_reset_handler(
        [diskII_d]() {
            ndiskii_reset(diskII_d/* , cpu */);
            return true;
        });
 
    computer->device_frame_dispatcher->registerHandler(
        [diskII_d]() {
            // motor off timer check. WAY easier to do here than in the drive.
            diskII_d->dc->check_motor_off_timer();

            if (diskII_d->computer->execution_mode == EXEC_NORMAL) {
                diskII_d->dc->soundeffects_update();
            }
            return true;
        });
 
    /* computer->dispatch->registerHandler(SDL_EVENT_DROP_FILE,
        [diskII_d](const SDL_Event &event) {
            printf("SDL_EVENT_DROP_FILE\n");
            const char *filename = event.drop.data;
            printf("filename: %s\n", filename);
            // x and y coordinates are where in my window the file was dropped.
            printf("x: %6.1f, y: %6.1f\n", event.drop.x, event.drop.y);
            disk_mount_t dm;
            dm.filename = strndup(filename, 1024);
            dm.slot = 6;
            dm.drive = 0;   
            int retval = diskII_d->computer->mounts->mount_media(dm);
            if (retval == 0) {
                diskII_d->computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Failed to mount media"));
                return false;
            }
            diskII_d->computer->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_CLOSE));
            diskII_d->computer->event_queue->addEvent(new Event(EVENT_REFOCUS, 0, (uint64_t)0));
            diskII_d->computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Disk Mounted Slot 6, Drive 1"));

            return true;
        }); */
}
 

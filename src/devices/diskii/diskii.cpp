/*
 *   Copyright (c) 2025 Jawaid Bazyar

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
#include "diskii.hpp"
#include "util/Event.hpp"
#include "util/media.hpp"
#include "util/ResourceFile.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "debug.hpp"
#include "util/mount.hpp"
#include "util/soundeffects.hpp"

/* uint8_t diskII_firmware[256] = {
 0xA2,  0x20,  0xA0,  0x00,   0xA2,  0x03,  0x86,  0x3C,   0x8A,  0x0A,  0x24,  0x3C,   0xF0,  0x10,  0x05,  0x3C,  
 0x49,  0xFF,  0x29,  0x7E,   0xB0,  0x08,  0x4A,  0xD0,   0xFB,  0x98,  0x9D,  0x56,   0x03,  0xC8,  0xE8,  0x10,  
 0xE5,  0x20,  0x58,  0xFF,   0xBA,  0xBD,  0x00,  0x01,   0x0A,  0x0A,  0x0A,  0x0A,   0x85,  0x2B,  0xAA,  0xBD,  
 0x8E,  0xC0,  0xBD,  0x8C,   0xC0,  0xBD,  0x8A,  0xC0,   0xBD,  0x89,  0xC0,  0xA0,   0x50,  0xBD,  0x80,  0xC0,  
 0x98,  0x29,  0x03,  0x0A,   0x05,  0x2B,  0xAA,  0xBD,   0x81,  0xC0,  0xA9,  0x56,   0x20,  0xA8,  0xFC,  0x88,  
 0x10,  0xEB,  0x85,  0x26,   0x85,  0x3D,  0x85,  0x41,   0xA9,  0x08,  0x85,  0x27,   0x18,  0x08,  0xBD,  0x8C,  
 0xC0,  0x10,  0xFB,  0x49,   0xD5,  0xD0,  0xF7,  0xBD,   0x8C,  0xC0,  0x10,  0xFB,   0xC9,  0xAA,  0xD0,  0xF3,  
 0xEA,  0xBD,  0x8C,  0xC0,   0x10,  0xFB,  0xC9,  0x96,   0xF0,  0x09,  0x28,  0x90,   0xDF,  0x49,  0xAD,  0xF0,  
 0x25,  0xD0,  0xD9,  0xA0,   0x03,  0x85,  0x40,  0xBD,   0x8C,  0xC0,  0x10,  0xFB,   0x2A,  0x85,  0x3C,  0xBD,  
 0x8C,  0xC0,  0x10,  0xFB,   0x25,  0x3C,  0x88,  0xD0,   0xEC,  0x28,  0xC5,  0x3D,   0xD0,  0xBE,  0xA5,  0x40,  
 0xC5,  0x41,  0xD0,  0xB8,   0xB0,  0xB7,  0xA0,  0x56,   0x84,  0x3C,  0xBC,  0x8C,   0xC0,  0x10,  0xFB,  0x59,  
 0xD6,  0x02,  0xA4,  0x3C,   0x88,  0x99,  0x00,  0x03,   0xD0,  0xEE,  0x84,  0x3C,   0xBC,  0x8C,  0xC0,  0x10,  
 0xFB,  0x59,  0xD6,  0x02,   0xA4,  0x3C,  0x91,  0x26,   0xC8,  0xD0,  0xEF,  0xBC,   0x8C,  0xC0,  0x10,  0xFB,  
 0x59,  0xD6,  0x02,  0xD0,   0x87,  0xA0,  0x00,  0xA2,   0x56,  0xCA,  0x30,  0xFB,   0xB1,  0x26,  0x5E,  0x00,  
 0x03,  0x2A,  0x5E,  0x00,   0x03,  0x2A,  0x91,  0x26,   0xC8,  0xD0,  0xEE,  0xE6,   0x27,  0xE6,  0x3D,  0xA5,  
 0x3D,  0xCD,  0x00,  0x08,   0xA6,  0x2B,  0x90,  0xDB,   0x4C,  0x01,  0x08,  0x00,   0x00,  0x00,  0x00,  0x00,  
}; */

#define DEBUG_PH(slot, drive, phase, onoff) fprintf(stdout, "PH: slot %d, drive %d, phase %d, onoff %d \n", slot, drive, phase, onoff)
#define DEBUG_MOT(slot, drive, onoff) fprintf(stdout, "MOT: slot %d, drive %d, motor %d \n", slot, drive, onoff)
#define DEBUG_DS(slot, drive, drives) fprintf(stdout, "DS:slot %d, drive %d, drive_select %d \n", slot, drive, drives)

uint8_t read_nybble(diskII& disk, bool motor)
{
/**
 * causes a shift of the read register.
 * We load the data into low byte of read_shift_register.
 * then we shift it left 1 bit at a time.
 * The returned data value is the high byte of the read_shift_register.
 */

    if (!motor) { // return the same data every time if motor is off.
        return (disk.read_shift_register >> 8) & 0xFF;
    }

    // Accurate version. Require the caller to shift each bit out one by one.
    if (disk.bit_position == 0) {
        // get next value from head_position to read_shift_register, increment head position.
        if (disk.track <= 68) { // 68 is the last legal track on a normal 35 track disk
            disk.read_shift_register = disk.nibblized.tracks[disk.track/2].data[disk.head_position];

            // "spin" the virtual diskette a little more
            disk.head_position++;
            if (disk.head_position >= disk.nibblized.tracks[disk.track/2].size) {
                disk.head_position = 0;
            }
            /* if (disk.head_position >= 0x1A00) { // rotated around back to start.
                disk.head_position = 0;
            } */
            if (disk.read_shift_register == 0xFF) { // for sync bytes simulate that they are 10 bits. (with two trailing zero bits)
                disk.bit_position = 8; // at 10 c600 boot code never syncs 
            } else {
                disk.bit_position = 8;
            }
            disk.read_shift_register <<= 1; // "pre-shift" 6 bits to Accelerate. This may not work for some copy-protected disks.
            disk.bit_position--;
            disk.read_shift_register <<= 1;
            disk.bit_position--;
            disk.read_shift_register <<= 1;
            disk.bit_position--;
            disk.read_shift_register <<= 1;
            disk.bit_position--;
            disk.read_shift_register <<= 1;
            disk.bit_position--;
            disk.read_shift_register <<= 1;
            disk.bit_position--;
        } 
#if 0
        else { // provide random data for track out of bounds conditions.
            disk.read_shift_register = disk.random_track[disk.head_position];   
            disk.head_position++;
            if (disk.head_position >= 0x1A00) {
                disk.head_position = 0;
            }
            disk.bit_position = 8;     
        }
#endif
    }

    //uint8_t shiftedbyte = (disk.read_shift_register >> (disk.bit_position-1) );
    disk.read_shift_register <<= 1;
    disk.bit_position--;
/*     if (disk.bit_position == 0) { // end of byte? Trigger move to next byte.
        disk.bit_position = 0;
    }; */
    //printf("read_nybble from track%d head position %d read_shift_register %02X ", disk->track, disk->head_position, disk->read_shift_register);
    //printf("shifted byte %02X\n", shiftedbyte);
    return (disk.read_shift_register >> 8) & 0xFF;
}

void write_nybble(diskII& disk) { // cause a shift.

    //printf("write_nybble: track %d, head_position %d, write_shift_register %02X\n", disk.track, disk.head_position, disk.write_shift_register);

    disk.bit_position = 0;

    // get next value from head_position to read_shift_register, increment head position.
    disk.head_position++;
    //disk.head_position %= 0x1A00;
    if (disk.head_position >= disk.nibblized.tracks[disk.track/2].size) {
        disk.head_position = 0;
    }
    disk.nibblized.tracks[disk.track/2].data[disk.head_position] = disk.write_shift_register;
    
    // "spin" the virtual diskette a little more
/*     if (disk.head_position >= 0x1A00) { // rotated around back to start.
        disk.head_position = 0;
    }
 */
    disk.modified = true;
    return;
}

void mount_diskII(cpu_state *cpu, uint8_t slot, uint8_t drive, media_descriptor *media) {
    diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);

    if (media->data_size != 560 * 256) {
        fprintf(stderr, "Disk image is not 140K\n");
        return;
    }

    if (diskII_d->drive[drive].is_mounted) {
        fprintf(stderr, "A disk already mounted, unmounting it.\n");
        unmount_diskII(cpu, slot, drive);
    }

    if (media->data_size != 140 * 1024) {
        fprintf(stderr, "Disk image is not 140K\n");
        return;
    }

    // Detect DOS 3.3 or ProDOS and set the interleave accordingly done by identify_media
    // if filename ends in .po, use po_phys_to_logical and po_logical_to_phys.
    // if filename ends in .do, use do_phys_to_logical and do_logical_to_phys.
    // if filename ends in .dsk, use do_phys_to_logical and do_logical_to_phys.
    
    if (media->media_type == MEDIA_PRENYBBLE) {
        // Load nib format image directly into diskII structure.
        load_nib_image(diskII_d->drive[drive].nibblized, media->filename);
        std::cout << "Mounted pre-nibblized disk " << media->filestub << std::endl;
        /* printf("Mounted pre-nibblized disk %s\n", media->filestub); */
    } else {
        if (media->interleave == INTERLEAVE_PO) {
            memcpy(diskII_d->drive[drive].nibblized.interleave_phys_to_logical, po_phys_to_logical, sizeof(interleave_t));
            memcpy(diskII_d->drive[drive].nibblized.interleave_logical_to_phys, po_logical_to_phys, sizeof(interleave_t));
        } else if (media->interleave == INTERLEAVE_DO) {
            memcpy(diskII_d->drive[drive].nibblized.interleave_phys_to_logical, do_phys_to_logical, sizeof(interleave_t));
            memcpy(diskII_d->drive[drive].nibblized.interleave_logical_to_phys, do_logical_to_phys, sizeof(interleave_t));
        }

        //load_disk_image(diskII_d->drive[drive].media, media->filename); // pull this into diskii stuff somewhere.
        load_disk_image(media, diskII_d->drive[drive].media); // pull this into diskii stuff somewhere.
        emit_disk(diskII_d->drive[drive].nibblized, diskII_d->drive[drive].media, media->dos33_volume);
        std::cout << "Mounted disk " << media->filestub << " volume " << media->dos33_volume << std::endl;
        /* printf("Mounted disk %s volume %d\n", media->filestub, media->dos33_volume); */
    }
    diskII_d->drive[drive].write_protect = media->write_protected;
    diskII_d->drive[drive].is_mounted = true;
    diskII_d->drive[drive].media_d = media;
    diskII_d->drive[drive].modified = false;
}

void writeback_diskII_image(cpu_state *cpu, uint8_t slot, uint8_t drive) {
    diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);
    diskII &disk = diskII_d->drive[drive];

    if (disk.media_d->media_type == MEDIA_PRENYBBLE) {
        std::cout << "writing back pre-nibblized disk image " << disk.media_d->filename << std::endl;
        write_nibblized_disk(disk.media_d, disk.nibblized);
    } else {
        std::cout << "writing back block disk image " << disk.media_d->filename << std::endl;
        media_interleave_t id = disk.media_d->interleave;
        disk_image_t new_disk_image;
        denibblize_disk_image(new_disk_image, disk.nibblized, id);
        write_disk_image_po_do(disk.media_d, new_disk_image);
    }
    disk.modified = false;
}

void unmount_diskII(cpu_state *cpu, uint8_t slot, uint8_t drive) {
    diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);

    // we used to write disk image here, but, moved to mount.cpp

    // reset all the track parameters to default to prepare for loading a new image.
    for (int i = 0; i < 35; i++) {
        diskII_d->drive[drive].nibblized.tracks[i].size = 0;
        diskII_d->drive[drive].nibblized.tracks[i].position = 0;
        memset(diskII_d->drive[drive].nibblized.tracks[i].data, 0, 0x1A00); // clear the track data. REALLY unmounted.
    }
    diskII_d->drive[drive].is_mounted = false;
    diskII_d->drive[drive].media_d = nullptr;
    diskII_d->drive[drive].modified = false;
}

drive_status_t diskii_status(cpu_state *cpu, uint64_t key) {
    uint8_t slot = key >> 8;
    uint8_t drive = key & 0xFF;
    diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);
    diskII &seldrive = diskII_d->drive[drive];

    if (diskII_d->motor == 1 && diskII_d->mark_cycles_turnoff != 0 && ((cpu->cycles > diskII_d->mark_cycles_turnoff))) {
        if (DEBUG(DEBUG_DISKII)) printf("motor off: %llu %llu cycles\n", cpu->cycles, diskII_d->mark_cycles_turnoff);
        diskII_d->motor = 0;
        diskII_d->mark_cycles_turnoff = 0;
    }
    const char *fname = nullptr;
    if (seldrive.media_d) {
        fname = seldrive.media_d->filestub.c_str(); // TODO: this could be a string_view instead of this hack.
        /* printf("diskii_status: %s\n", fname); */
    }
    bool motor = (diskII_d->drive_select == drive) ? diskII_d->motor : false;

    return {seldrive.is_mounted, fname, motor, seldrive.track, seldrive.modified};
}

bool any_diskii_motor_on(cpu_state *cpu) {

    // iterate slots. if a slot is a diskII, check if motor is on.
    // all slot data structures must start with a device ID so I know what to cast to.

    for (int i = 0; i < 8; i++) {
        diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)i);
        if ((diskII_d != nullptr) && (diskII_d->id == DEVICE_ID_DISK_II)) {
            if (diskII_d->motor == 1) {
                return true;
            }
        }
    }
    return false;
}

int diskii_tracknumber_on(cpu_state *cpu) {
    for (int i = 0; i < 8; i++) {

        diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)i);

        if ((diskII_d != nullptr) && (diskII_d->id == DEVICE_ID_DISK_II)) {
            uint8_t selected = diskII_d->drive_select;
            if (diskII_d->motor == 1) {
                return diskII_d->drive[selected].track;
            }        
        }
    }
    return -1;
}


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

uint8_t diskII_read_C0xx(void *context, uint32_t address) {
    //cpu_state *cpu = (cpu_state *)context;
    diskII_controller *thisSlot = (diskII_controller *)context;
    cpu_state *cpu = thisSlot->computer->cpu;

    int reg = address & 0x0F;
    uint8_t slot = (address >> 4) & 0x7;
    //diskII_controller *thisSlot = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);
    int drive = thisSlot->drive_select;

    diskII &seldrive = thisSlot->drive[drive];

    if (thisSlot->motor == 1 && thisSlot->mark_cycles_turnoff != 0 && ((cpu->cycles > thisSlot->mark_cycles_turnoff))) {
        if (DEBUG(DEBUG_DISKII)) printf("motor off: %llu %llu cycles\n", cpu->cycles, thisSlot->mark_cycles_turnoff);
        thisSlot->motor = 0;
        thisSlot->mark_cycles_turnoff = 0;
    }

    int8_t last_phase_on = seldrive.last_phase_on;
    int8_t cur_track = seldrive.track;

    //uint8_t read_value = 0xEE;
    int8_t cur_phase = cur_track % 4;

    // if more than X cycles have elapsed since last read, set bit_position to 0 and move head X bytes forward.
    /* if ((cpu->cycles - seldrive.last_read_cycle) > 64) {
        seldrive.bit_position = 0;
        seldrive.head_position = (seldrive.head_position +  ((cpu->cycles - seldrive.last_read_cycle) / 32) ) % 0x1A00;
    } */
    seldrive.last_read_cycle = cpu->cycles; // always update this.

    switch (reg) {
        case DiskII_Ph0_Off:    
            //if (DEBUG(DEBUG_DISKII))  DEBUG_PH(slot, drive, 0, 0);
            seldrive.phase0 = 0;
            break;
        case DiskII_Ph0_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 0, 1);
            if (cur_phase == 1) {
                seldrive.track--;
            } else if (cur_phase == 3) {
                seldrive.track++;
            }
            seldrive.phase0 = 1;
            seldrive.last_phase_on = 0;
            break;
        case DiskII_Ph1_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 1, 0);
            seldrive.phase1 = 0;
            break;
        case DiskII_Ph1_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 1, 1);
            if (cur_phase == 2) {
                seldrive.track--;
            } else if (cur_phase == 0) {
                seldrive.track++;
            }
            seldrive.phase1 = 1;
            seldrive.last_phase_on = 1;
            break;
        case DiskII_Ph2_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 2, 0);
            seldrive.phase2 = 0;
            break;
        case DiskII_Ph2_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 2, 1);
            if (cur_phase == 3) {
                seldrive.track--;
            } else if (cur_phase == 1) {
                seldrive.track++;
            }
            seldrive.phase2 = 1;
            seldrive.last_phase_on = 2;
            break;
        case DiskII_Ph3_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 3, 0);
            seldrive.phase3 = 0;
            break;
        case DiskII_Ph3_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 3, 1);
            if (cur_phase == 0) {
                seldrive.track--;
            } else if (cur_phase == 2) {
                seldrive.track++;
            }
            seldrive.phase3 = 1;
            seldrive.last_phase_on = 3;
            break;
        case DiskII_Motor_Off: // only one drive at a time is motorized.
            //if (DEBUG(DEBUG_DISKII)) DEBUG_MOT(slot, drive, 0);
            // if motor already off, do nothing. otherwise schedule a motor off.
            if (thisSlot->motor == 1) {
                thisSlot->mark_cycles_turnoff = cpu->cycles + 1000000;
                if (DEBUG(DEBUG_DISKII)) printf("schedule motor off at %llu (is now %llu)\n", thisSlot->mark_cycles_turnoff, cpu->cycles);
            }
            break;
        case DiskII_Motor_On: // only one drive at a time is motorized.
            //if (DEBUG(DEBUG_DISKII)) DEBUG_MOT(slot, drive, 1);
            thisSlot->motor = 1;
            thisSlot->mark_cycles_turnoff = 0; // if we turn motor on, reset this and don't stop it!
            break;
        case DiskII_Drive1_Select:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 0);
            thisSlot->drive_select = 0;
            break;
        case DiskII_Drive2_Select:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 1);
            thisSlot->drive_select = 1;
            break;
        case DiskII_Q6L:
            seldrive.Q6 = 0; 
            /**
             * when Q6L is read, and Q7L was previously read, then cycle another bit read of a nybble from the disk
             * TODO: only do this if the motor is on. if motor is off, return the same byte over and over,
             *   whatever the last byte was. 
             */
            /* if (seldrive.Q7 == 0) {
                return read_nybble(seldrive);
            } */
           /**
            * when Q6L is read, and Q7H was previously set (written) then we need to write the byte to the disk.
            */
            if (seldrive.Q7 == 1 || seldrive.Q6 == 1) {
                write_nybble(seldrive);
                //seldrive.Q7 = 0;
            }
            break;
        case DiskII_Q6H:
            seldrive.Q6 = 1;
            break;
        case DiskII_Q7L:
            seldrive.Q7 = 0;
            if (seldrive.Q6 == 1) { // Q6H then Q7L is a write protect sense.
                uint8_t xwp = seldrive.write_protect << 7;
                //printf("wp: Q7: %d, Q6: %d, wp: %d %02X\n", seldrive.Q7, seldrive.Q6, seldrive.write_protect, xwp);
                return xwp; // write protect sense. Return hi bit set (write protected)
            }
            break;
        case DiskII_Q7H:
            seldrive.Q7 = 1;
            break;
    }

    /* ANY even address read will get the contents of the current nibble. */
    if (((reg & 0x01) == 0) && (seldrive.Q7 == 0 && seldrive.Q6 == 0)) {
        //seldrive.last_read_cycle = cpu->cycles;
        uint8_t x = read_nybble(seldrive, thisSlot->motor);
        //printf("read_nybble: %02X\n", x);
        return x;
    }

    if (seldrive.track != cur_track) {
        uint8_t halftrack = seldrive.track % 2;
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "new (internal track): %d, realtrack %d, halftrack %d\n", seldrive.track, seldrive.track/2, halftrack);
    }
    if (seldrive.track < 0) {
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "track < 0, CHUGGA CHUGGA CHUGGA\n");
        seldrive.track = 0;
    }
    if (seldrive.track > 68) {
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "track > 71, CHUGGA CHUGGA CHUGGA\n");
        seldrive.track = 68;
    }
    return 0xEE;
}

void diskII_write_C0xx(void *context, uint32_t address, uint8_t value) {
    //cpu_state *cpu = (cpu_state *)context;
    diskII_controller *diskII_d = (diskII_controller *)context;
    cpu_state *cpu = diskII_d->computer->cpu;
    
    uint16_t addr = address - 0xC080;
    int reg = addr & 0x0F;
    uint8_t slot = addr >> 4;
    //diskII_controller *diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)slot);
    int drive = diskII_d->drive_select;

    diskII &seldrive = diskII_d->drive[drive];

    // store the value being written into the write_shift_register. It will be stored in the disk image when Q6L is tweaked in read.
    switch (reg) {
        case DiskII_Motor_Off: // only one drive at a time is motorized.
            if (DEBUG(DEBUG_DISKII)) DEBUG_MOT(slot, drive, 0);
            // if motor already off, do nothing. otherwise schedule a motor off.
            if (diskII_d->motor == 1) {
                diskII_d->mark_cycles_turnoff = cpu->cycles + 1000000;
                if (DEBUG(DEBUG_DISKII)) printf("schedule motor off at %llu (is now %llu)\n", diskII_d->mark_cycles_turnoff, cpu->cycles);
            }
            break;
        case DiskII_Motor_On: // only one drive at a time is motorized.
            if (DEBUG(DEBUG_DISKII)) DEBUG_MOT(slot, drive, 1);
            diskII_d->motor = 1;
            diskII_d->mark_cycles_turnoff = 0; // if we turn motor on, reset this and don't stop it!
            break;
        case DiskII_Drive1_Select:
            if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 0);
            diskII_d->drive_select = 0;
            break;
        case DiskII_Drive2_Select:
            if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 1);
            diskII_d->drive_select = 1;
            break;
        case DiskII_Q6H:
            //printf("Q6H set write_shift_register=%02X\n", value);
            seldrive.write_shift_register = value;
            seldrive.Q6 = 1;
            break;
        case DiskII_Q7H:
            //printf("Q7H set write_shift_register=%02X\n", value);
            seldrive.write_shift_register = value;
            seldrive.Q7 = 1;
            break;
    }
    return;
}

void diskII_init(diskII_controller * diskII_d, SlotType_t slot) {
    // clear out and reset all potential slots etc to sane states.
    //diskII_controller * diskII_d = (diskII_controller *)get_slot_state(cpu, slot);

    for (int j = 0; j < 2; j++) {
        diskII_d->drive[j].track = 0;
        diskII_d->drive[j].phase0 = 0;
        diskII_d->drive[j].phase1 = 0;
        diskII_d->drive[j].phase2 = 0;
        diskII_d->drive[j].phase3 = 0;
        diskII_d->drive[j].last_phase_on = 0;
        diskII_d->drive[j].image_index = 0;
        diskII_d->drive[j].write_protect = 0 /* 1 */ ;
        diskII_d->drive[j].bit_position = 0; // how many bits left in byte.
        diskII_d->drive[j].read_shift_register = 0; // when bit position = 0, this is 0. As bit_position increments, we shift in the next bit of the byte at head_position.
        diskII_d->drive[j].head_position = 0; // index into the track
    }
#if 0
    // create random data for "track out of bounds"
    for (int drive = 0; drive < 2; drive++) {
        for (int i = 0; i < 0x1A00; i++) { 
            uint8_t value = 0;
            for (int j = 0; j < 8; j++) {
                value = (value << 1) | ((random() % 100) < 30 /* random1Percentage */);
            }
            diskII_d->drive[drive].random_track[i] = value;
        }
    }
#endif

    diskII_d->drive_select = 0;
    diskII_d->motor = 0;
    diskII_d->mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.
}

#if 0
void diskii_reset(cpu_state *cpu) {
    printf("diskii_reset\n");
    // TODO: this should be a callback from the CPU reset handler.
    for (int i = 0; i < 8; i++) {
        SlotData *slot_data = get_slot_state(cpu, (SlotType_t)i);
        
        if ((slot_data != nullptr) && (slot_data->id == DEVICE_ID_DISK_II)) {
            diskII_controller *diskII_d = (diskII_controller *)slot_data;
            diskII_d->motor = 0;
            diskII_d->mark_cycles_turnoff = 0;
        }
    }
}
#endif
void diskii_reset(diskII_controller *diskII_d) {
    printf("diskii_reset\n");
    // this is a callback from the computer reset handler.
    //for (int i = 0; i < 8; i++) {
        //SlotData *slot_data = get_slot_state(cpu, (SlotType_t)i);
        
        //if ((slot_data != nullptr) && (slot_data->id == DEVICE_ID_DISK_II)) {
            //diskII_controller *diskII_d = (diskII_controller *)slot_data;
            diskII_d->motor = 0;
            diskII_d->mark_cycles_turnoff = 0;
        //}
    //}
}


void init_slot_diskII(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    diskII_controller *diskII_d = new diskII_controller();
    diskII_d->computer = computer;

    // set in CPU so we can reference later
    diskII_d->id = DEVICE_ID_DISK_II;
    set_slot_state(cpu, slot, diskII_d); // lots of stuff is still dependent on this (for now). this is where MessageBus comes in..
    
    fprintf(stdout, "diskII_register_slot %d\n", slot);

    ResourceFile *rom = new ResourceFile("roms/cards/diskii/diskii_firmware.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load diskii/diskii_firmware.rom\n");
        return;
    }
    rom->load();

    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = (uint8_t *)(rom->get_data());

    // clear out and reset all potential slots etc to sane states.
    diskII_init(diskII_d, slot);

    uint16_t slot_base = 0xC080 + (slot * 0x10);

    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph0_Off, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph0_On, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph1_Off, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph1_On, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph2_Off, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph2_On, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph3_Off, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Ph3_On, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Motor_Off, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Motor_On, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Drive1_Select, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Drive2_Select, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Q6L, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Q6H, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Q7L, { diskII_read_C0xx, diskII_d });
    computer->mmu->set_C0XX_read_handler(slot_base + DiskII_Q7H, { diskII_read_C0xx, diskII_d });

    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Motor_Off, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Motor_On, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Drive1_Select, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Drive2_Select, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Q6L, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Q6H, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Q7L, { diskII_write_C0xx, diskII_d });
    computer->mmu->set_C0XX_write_handler(slot_base + DiskII_Q7H, { diskII_write_C0xx, diskII_d });

    computer->mmu->set_slot_rom(slot, rom_data, "DISK2_ROM");

    // register drives with mounts for status reporting
    uint64_t key = (slot << 8) | 0;
    computer->mounts->register_drive(DRIVE_TYPE_DISKII, key);
    computer->mounts->register_drive(DRIVE_TYPE_DISKII, key + 1);

    computer->register_reset_handler(
        [diskII_d,cpu]() {
            diskii_reset(diskII_d/* , cpu */);
            return true;
        });

    computer->device_frame_dispatcher->registerHandler(
        [diskII_d,cpu]() {
            //diskii_frame_event(diskII_d, cpu);
            bool diskii_run = any_diskii_motor_on(cpu);
            if (cpu->execution_mode == EXEC_NORMAL) {
                soundeffects_update(diskii_run, diskii_tracknumber_on(cpu));
            }
            return true;
        });

    computer->dispatch->registerHandler(SDL_EVENT_DROP_FILE,
        [diskII_d,cpu](const SDL_Event &event) {
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
        });
}

void debug_dump_disk_images(cpu_state *cpu) { // only dump slot 6.
    uint8_t diskii_slot = 6;
    diskII_controller * diskII_d = (diskII_controller *)get_slot_state(cpu, (SlotType_t)diskii_slot);
    if ((diskII_d != nullptr) && (diskII_d->id == DEVICE_ID_DISK_II)) {
        printf("debug_dump_disk_images slot 6 drive 1\n");
       /*  write_nibblized_disk(diskII_d->drive[0].nibblized, "/tmp/disk1.nib");
        write_nibblized_disk(diskII_d->drive[1].nibblized, "/tmp/disk2.nib"); */
    }
}

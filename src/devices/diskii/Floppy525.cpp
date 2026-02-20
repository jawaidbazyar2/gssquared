#include <iostream>
#include <cstdint>
#include "util/printf_helper.hpp"
#include "Floppy525.hpp"
#include "devices/diskii/diskii.hpp"
#include "util/SoundEffectKeys.hpp"
#include "debug.hpp"


void Floppy525::set_track(int track_num) {
    track = track_num;
 }

void Floppy525::move_head(int direction) {
    track += direction;
}

void Floppy525::write_nybble(uint8_t nybble) {

    //printf("write_nybble: track %d, head_position %d, write_shift_register %02X\n", disk.track, disk.head_position, disk.write_shift_register);

    bit_position = 0;

    // get next value from head_position to read_shift_register, increment head position.
    head_position++;
    //disk.head_position %= 0x1A00;
    if (head_position >= nibblized.tracks[track/2].size) {
        head_position = 0;
    }
    nibblized.tracks[track/2].data[head_position] = write_shift_register;
    
    // "spin" the virtual diskette a little more
    /*     if (disk.head_position >= 0x1A00) { // rotated around back to start.
            disk.head_position = 0;
        }
    */
    modified = true;
    return;
}

uint8_t Floppy525::read_nybble() {
   /**
    * causes a shift of the read register.
    * We load the data into low byte of read_shift_register.
    * then we shift it left 1 bit at a time.
    * The returned data value is the high byte of the read_shift_register.
    */

    if (!enable) { // return the same data every time if motor is off.
        return (read_shift_register >> 8) & 0xFF;
    }

    // Accurate version. Require the caller to shift each bit out one by one.
    if (bit_position == 0) {
        // get next value from head_position to read_shift_register, increment head position.
        if (track <= 68) { // 68 is the last legal track on a normal 35 track disk
            read_shift_register = nibblized.tracks[track/2].data[head_position];

            // "spin" the virtual diskette a little more
            head_position++;
            if (head_position >= nibblized.tracks[track/2].size) {
                head_position = 0;
            }
            /* if (disk.head_position >= 0x1A00) { // rotated around back to start.
                disk.head_position = 0;
            } */
            if (read_shift_register == 0xFF) { // for sync bytes simulate that they are 10 bits. (with two trailing zero bits)
                bit_position = 8; // at 10 c600 boot code never syncs 
            } else {
                bit_position = 8;
            }
            read_shift_register <<= 1; // "pre-shift" 6 bits to Accelerate. This may not work for some copy-protected disks.
            bit_position--;
            read_shift_register <<= 1;
            bit_position--;
            read_shift_register <<= 1;
            bit_position--;
            read_shift_register <<= 1;
            bit_position--;
            read_shift_register <<= 1;
            bit_position--;
            read_shift_register <<= 1;
            bit_position--;
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
    read_shift_register <<= 1;
    bit_position--;
    /*     if (disk.bit_position == 0) { // end of byte? Trigger move to next byte.
        disk.bit_position = 0;
    }; */
    //printf("read_nybble from track%d head position %d read_shift_register %02X ", disk->track, disk->head_position, disk->read_shift_register);
    //printf("shifted byte %02X\n", shiftedbyte);
    return (read_shift_register >> 8) & 0xFF;
}


bool Floppy525::mount(uint64_t key, media_descriptor *media_in) {

    if (media_in->data_size != 560 * 256) {
        fprintf(stderr, "Disk image is not 140K\n");
        return false;
    }

    if (is_mounted) {
        fprintf(stderr, "A disk already mounted, unmounting it.\n");
        unmount(key);
    }

    if (media_in->data_size != 140 * 1024) {
        fprintf(stderr, "Disk image is not 140K\n");
        return false;
    }

    // Detect DOS 3.3 or ProDOS and set the interleave accordingly done by identify_media
    // if filename ends in .po, use po_phys_to_logical and po_logical_to_phys.
    // if filename ends in .do, use do_phys_to_logical and do_logical_to_phys.
    // if filename ends in .dsk, use do_phys_to_logical and do_logical_to_phys.
    
    if (media_in->media_type == MEDIA_PRENYBBLE) {
        // Load nib format image directly into diskII structure.
        load_nib_image(nibblized, media_in->filename);
        std::cout << "Mounted pre-nibblized disk " << media_in->filestub << std::endl;
        /* printf("Mounted pre-nibblized disk %s\n", media->filestub); */
    } else {
        if (media_in->interleave == INTERLEAVE_PO) {
            memcpy(nibblized.interleave_phys_to_logical, po_phys_to_logical, sizeof(interleave_t));
            memcpy(nibblized.interleave_logical_to_phys, po_logical_to_phys, sizeof(interleave_t));
        } else if (media_in->interleave == INTERLEAVE_DO) {
            memcpy(nibblized.interleave_phys_to_logical, do_phys_to_logical, sizeof(interleave_t));
            memcpy(nibblized.interleave_logical_to_phys, do_logical_to_phys, sizeof(interleave_t));
        }

        //load_disk_image(diskII_d->drive[drive].media, media->filename); // pull this into diskii stuff somewhere.
        load_disk_image(media_in, media); // pull this into diskii stuff somewhere.
        emit_disk(nibblized, media, media_in->dos33_volume);
        std::cout << "Mounted disk " << media_in->filestub << " volume " << media_in->dos33_volume << std::endl;
        /* printf("Mounted disk %s volume %d\n", media->filestub, media->dos33_volume); */
    }
    write_protect = media_in->write_protected;
    is_mounted = true;
    media_d = media_in;
    modified = false;
    play_sound(SE_SHUGART_CLOSE); 
    return true;
}

bool Floppy525::unmount(uint64_t key) {
    // we used to write disk image here, but, moved to mount.cpp

    // reset all the track parameters to default to prepare for loading a new image.
    for (int i = 0; i < 35; i++) {
        nibblized.tracks[i].size = 0;
        nibblized.tracks[i].position = 0;
        memset(nibblized.tracks[i].data, 0, 0x1A00); // clear the track data. REALLY unmounted.
    }
    is_mounted = false;
    media_d = nullptr;
    modified = false;

    play_sound(SE_SHUGART_OPEN); // so much easier here.

    return true;
}

bool Floppy525::writeback() {

    if (media_d->media_type == MEDIA_PRENYBBLE) {
        std::cout << "writing back pre-nibblized disk image " << media_d->filename << std::endl;
        write_nibblized_disk(media_d, nibblized);
    } else {
        std::cout << "writing back block disk image " << media_d->filename << std::endl;
        media_interleave_t id = media_d->interleave;
        disk_image_t new_disk_image;
        denibblize_disk_image(new_disk_image, nibblized, id);
        write_disk_image_po_do(media_d, new_disk_image);
    }
    modified = false;

    return true;
}

void Floppy525::nibblize() {}

drive_status_t Floppy525::status() {
    if (is_mounted) return {is_mounted, media_d->filestub, enable, track, modified};
    else return {is_mounted, "", enable, track, modified};
}

void Floppy525::reset() {
    enable = false;
}

/* void Floppy525::motor(bool onoff) {
    // TODO: these will have to use a timer because we can't rely on being called back here periodically to shut off the drive..
    // fix in DiskII first and then see.
    if (motor_on && onoff==false && mark_cycles_turnoff == 0) {
        // delay motor off by 1 second
        mark_cycles_turnoff = clock->get_c14m() + clock->get_c14m_per_second();
        if (DEBUG(DEBUG_DISKII)) printf("schedule motor off at %llu (is now %llu)\n", u64_t(mark_cycles_turnoff), u64_t(clock->get_cycles()));
        return;
    }
    mark_cycles_turnoff = 0;
    motor_on = onoff;
    printf("Motor %s\n", onoff ? "on" : "off");
} */

uint8_t Floppy525::read_cmd(uint16_t address) {
    uint16_t reg = address & 0x0F;

    if (enable && mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
        if (DEBUG(DEBUG_DISKII)) printf("motor off: %llu %llu cycles\n", u64_t(clock->get_c14m()), u64_t(mark_cycles_turnoff));
        enable = false;
        mark_cycles_turnoff = 0;
    }

    int8_t last_phase_on = last_phase_on;
    int8_t cur_track = track;

    int8_t cur_phase = cur_track % 4;

    // if more than X cycles have elapsed since last read, set bit_position to 0 and move head X bytes forward.
    /* if ((cpu->cycles - seldrive.last_read_cycle) > 64) {
        seldrive.bit_position = 0;
        seldrive.head_position = (seldrive.head_position +  ((cpu->cycles - seldrive.last_read_cycle) / 32) ) % 0x1A00;
    } */
    //last_read_cycle = clock->get_cycles(); // always update this. NOT USED ANY MORE

    switch (reg) {
        case DiskII_Ph0_Off:    
            //if (DEBUG(DEBUG_DISKII))  DEBUG_PH(slot, drive, 0, 0);
            phase0 = 0;
            break;
        case DiskII_Ph0_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 0, 1);
            if (cur_phase == 1) {
                track--;
            } else if (cur_phase == 3) {
                track++;
            }
            phase0 = 1;
            last_phase_on = 0;
            break;
        case DiskII_Ph1_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 1, 0);
            phase1 = 0;
            break;
        case DiskII_Ph1_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 1, 1);
            if (cur_phase == 2) {
                track--;
            } else if (cur_phase == 0) {
                track++;
            }
            phase1 = 1;
            last_phase_on = 1;
            break;
        case DiskII_Ph2_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 2, 0);
            phase2 = 0;
            break;
        case DiskII_Ph2_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 2, 1);
            if (cur_phase == 3) {
                track--;
            } else if (cur_phase == 1) {
                track++;
            }
            phase2 = 1;
            last_phase_on = 2;
            break;
        case DiskII_Ph3_Off:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 3, 0);
            phase3 = 0;
            break;
        case DiskII_Ph3_On:
            //if (DEBUG(DEBUG_DISKII)) DEBUG_PH(slot, drive, 3, 1);
            if (cur_phase == 0) {
                track--;
            } else if (cur_phase == 2) {
                track++;
            }
            phase3 = 1;
            last_phase_on = 3;
            break;
/*         case DiskII_Motor_Off: // only one drive at a time is motorized.
            //if (DEBUG(DEBUG_DISKII)) DEBUG_MOT(slot, drive, 0);
            // if motor already off, do nothing. otherwise schedule a motor off.
            
            motor(false);
            break;
        case DiskII_Motor_On: // only one drive at a time is motorized.
            
            motor(true);
            break; */

        case DiskII_Q6L:
            Q6 = 0; 
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
            if (Q7 == 1 || Q6 == 1) {
                write_nybble(write_shift_register);
                //write_nybble(seldrive);
                //seldrive.Q7 = 0;
            }
            break;
        case DiskII_Q6H:
            Q6 = 1;
            break;
        case DiskII_Q7L:
            Q7 = 0;
            if (Q6 == 1) { // Q6H then Q7L is a write protect sense.
                uint8_t xwp = write_protect << 7;
                //printf("wp: Q7: %d, Q6: %d, wp: %d %02X\n", seldrive.Q7, seldrive.Q6, seldrive.write_protect, xwp);
                return xwp; // write protect sense. Return hi bit set (write protected)
            }
            break;
        case DiskII_Q7H:
            Q7 = 1;
            break;
    }

    // handled by caller
    /* ANY even address read will get the contents of the current nibble. */
    /* if (((reg & 0x01) == 0) && (Q7 == 0 && Q6 == 0)) {
        //seldrive.last_read_cycle = cpu->cycles;
        uint8_t x = read_nybble();
        //printf("read_nybble: %02X\n", x);
        return x;
    } */

    if (track != cur_track) {
        uint8_t halftrack = track % 2;
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "new (internal track): %d, realtrack %d, halftrack %d\n", seldrive.track, seldrive.track/2, halftrack);
    }
    if (track < 0) {
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "track < 0, CHUGGA CHUGGA CHUGGA\n");
        track = 0;
    }
    if (track > 68) {
        //if (DEBUG(DEBUG_DISKII)) fprintf(stdout, "track > 71, CHUGGA CHUGGA CHUGGA\n");
        track = 68;
    }
    return 0;
}

void Floppy525::write_cmd(uint16_t address, uint8_t data) {
    uint16_t reg = address & 0x0F;

    // store the value being written into the write_shift_register. It will be stored in the disk image when Q6L is tweaked in read.
    switch (reg) {
        case DiskII_Q6H:
            //printf("Q6H set write_shift_register=%02X\n", value);
            write_shift_register = data;
            Q6 = 1;
            break;
        case DiskII_Q7H:
            //printf("Q7H set write_shift_register=%02X\n", value);
            write_shift_register = data;
            Q7 = 1;
            break;
    }
    return;

}
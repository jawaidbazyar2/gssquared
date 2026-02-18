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


 #pragma once

 //#include "slots.hpp"
 #include "computer.hpp"
 #include "NClock.hpp"
 #include "devices/diskii/diskii_fmt.hpp"
 #include "util/media.hpp"
 #include "util/mount.hpp"
 #include "util/SoundEffectKeys.hpp"
 #include "devices/diskii/Floppy525.hpp"
 
 #define DiskII_Ph0_Off 0x00
 #define DiskII_Ph0_On 0x01
 #define DiskII_Ph1_Off 0x02
 #define DiskII_Ph1_On 0x03
 #define DiskII_Ph2_Off 0x04
 #define DiskII_Ph2_On 0x05
 #define DiskII_Ph3_Off 0x06
 #define DiskII_Ph3_On 0x07
 #define DiskII_Motor_Off 0x08
 #define DiskII_Motor_On 0x09
 #define DiskII_Drive1_Select 0x0A
 #define DiskII_Drive2_Select 0x0B
 #define DiskII_Q6L 0x0C
 #define DiskII_Q6H 0x0D
 #define DiskII_Q7L 0x0E
 #define DiskII_Q7H 0x0F
 

class DiskII_Controller : public StorageDevice {
    computer_t *computer;
    SoundEffect *sound_effect;
    SoundEffectContainer_t sounds[5];
    NClock *clock;
    Floppy525 drives[2];
    uint8_t drive_select;
    bool motor;
    uint64_t mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.

    /* Load our sound effects */
    const char *sound_files[5] = {
        "sounds/shugart-drive.wav",
        "sounds/shugart-stop.wav",
        "sounds/shugart-head.wav",
        "sounds/shugart-open.wav",
        "sounds/shugart-close.wav",
    };
   
    // schedule a motor off.
    void request_motor_off() {
        mark_cycles_turnoff = clock->get_c14m() + 14318180;
        //if (DEBUG(DEBUG_DISKII)) printf("request_motor_off: %llu\n", u64_t(mark_cycles_turnoff));
    }

    // motor on is always immediate.
    void request_motor_on() {
        mark_cycles_turnoff = 0;
        motor = 1;
        drives[drive_select].set_enable(true);
        //if (DEBUG(DEBUG_DISKII)) printf("request_motor_on: %llu\n", u64_t(mark_cycles_turnoff));
    }

    public:
    DiskII_Controller(SoundEffect *sound_effect, NClockII *clock) : StorageDevice(), drives{Floppy525(sound_effect, clock), Floppy525(sound_effect, clock)} {
        this->sound_effect = sound_effect;
        this->clock = clock;
        drive_select = 0;
        motor = 0;
        mark_cycles_turnoff = 0;

        for (int i = 0; i < SDL_arraysize(sounds); i++) {
            sounds[i].fname = sound_files[i];
            sounds[i].si = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
        }
    }

    void reset() {
        drive_select = 0;
        motor = 0;
        mark_cycles_turnoff = 0;
        
        drives[0].reset();
        drives[1].reset();
    }

    bool get_motor() { return motor; }
    uint8_t get_track() { return drives[drive_select].get_track(); }

    bool diskii_running_last = false;
    int tracknumber_last = 0;
    void soundeffects_update() {
        int tracknumber = drives[drive_select].get_track();

        //printf("diskii_running: %d, tracknumber: %d / %d\n", diskii_running, tracknumber, tracknumber_last);
    
        /* If less than a full copy of the audio is queued for playback, put another copy in there.
            This is overkill, but easy when lots of RAM is cheap. One could be more careful and
            queue less at a time, as long as the stream doesn't run dry.  */
    
        /* If sound state changed, reset the stream */
        if (diskii_running_last && !motor) {
            diskii_running_last = false;
            /* Clear the audio stream when transitioning to disabled state */
            sound_effect->flush(SE_SHUGART_DRIVE);
        }
        
        /* Only queue audio data if sound is enabled */
        static int running_chunknumber = 0;
        if (motor) {
            int dl = (int) sounds[SE_SHUGART_DRIVE].si->wav_data_len / 10;
            if (SDL_GetAudioStreamQueued(sounds[SE_SHUGART_DRIVE].si->stream) < dl) {
                SDL_PutAudioStreamData(sounds[SE_SHUGART_DRIVE].si->stream, sounds[SE_SHUGART_DRIVE].si->wav_data + dl * running_chunknumber, dl);
                running_chunknumber++;
                if (running_chunknumber > 8) {
                    running_chunknumber = 0;
                }
            }
        }
        // minimum track movement is 2. We're called every 1/60th. That's 735 samples.
        static int start_track_movement = -1;
        if (tracknumber >= 0 && (tracknumber_last != tracknumber)) {
            // if we have a track movement, play the head movement sound
            // head can move 16.7 / 2.5 tracks per second, about 7.
            int ind = 200 * 2 * std::abs(start_track_movement-tracknumber);
    
            int len = ((int) (200 * 2) * std::abs(tracknumber_last-tracknumber));
            if (ind + len > sounds[SE_SHUGART_HEAD].si->wav_data_len) {
                len = sounds[SE_SHUGART_HEAD].si->wav_data_len - ind;
            }
            sound_effect->play_specific(SE_SHUGART_HEAD, ind, len);

            if (start_track_movement == -1) start_track_movement = tracknumber_last;
            tracknumber_last = tracknumber;
        } else {
            // if head did not move on this update, reset the start_track_movement
            start_track_movement = -1;
        }
    }

    void check_motor_off_timer() {
        if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
            drives[drive_select].set_enable(false);
            motor = 0;
            mark_cycles_turnoff = 0;
        }
    }

    uint8_t read_cmd(uint16_t address) {
        uint16_t reg = address & 0x0F;
        
        check_motor_off_timer();

        switch (reg) {
            case DiskII_Motor_Off:
                request_motor_off();
                break;
            case DiskII_Motor_On:
                request_motor_on();
                break;
            case DiskII_Drive1_Select:
                //if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 0);
                // on a select change with motor on, change motor status immediately.
                if (motor == 1) {
                    drives[1].set_enable(false);
                    drives[0].set_enable(true);
                }
                drive_select = 0;
                break;
            case DiskII_Drive2_Select:
                //if (DEBUG(DEBUG_DISKII)) DEBUG_DS(slot, drive, 1);
                if (motor == 1) {
                    drives[0].set_enable(false);
                    drives[1].set_enable(true);
                }
                drive_select = 1;
                break;
            default:
                break;
        }
        Floppy525 &sel = drives[drive_select];

        if (((reg & 0x01) == 0) && (sel.get_Q7() == 0 && sel.get_Q6() == 0)) {
            //seldrive.last_read_cycle = cpu->cycles;
            uint8_t x = sel.read_nybble();
            //printf("read_nybble: %02X\n", x);
            return x;
        }
        // should this be before, or after, a switch?
        return sel.read_cmd(address);
    }

    void write_cmd(uint16_t address, uint8_t data) {
        uint16_t reg = address & 0x0F;

        check_motor_off_timer();

        switch (reg) {
            case DiskII_Motor_Off:
                request_motor_off();
                break;
            case DiskII_Motor_On:
                request_motor_on();
                break;
            case DiskII_Drive1_Select:
                drive_select = 0;
                break;
            case DiskII_Drive2_Select:
                drive_select = 1;
                break;
            default:
                break;
        }
        drives[drive_select].write_cmd(address, data);
    }
    bool mount(uint64_t key, media_descriptor *media) {
        uint32_t drivesel = key & 0x01;
        return drives[drivesel].mount(key, media);
    }
    bool unmount(uint64_t key) {
        uint32_t drivesel = key & 0x01;
        return drives[drivesel].unmount(key);
    }
    bool writeback(uint64_t key) {
        uint32_t drivesel = key & 0x01;
        return drives[drivesel].writeback();
    }
    drive_status_t status(uint64_t key) {
        uint32_t drivesel = key & 0x01;
        return drives[drivesel].status();
    }
};

class ndiskII_controller : public SlotData {
public:
    computer_t *computer;
    NClockII *clock;
    DiskII_Controller *dc;
};

void init_slot_ndiskII(computer_t *computer, SlotType_t slot);
//void ndiskii_reset(cpu_state *cpu);
void debug_dump_disk_images(cpu_state *cpu);
bool any_ndiskii_motor_on(cpu_state *cpu);
int ndiskii_tracknumber_on(cpu_state *cpu);
 

/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "computer.hpp"
#include "NClock.hpp"
#include "util/DebugFormatter.hpp"
#include "util/media.hpp"
#include "util/SoundEffectKeys.hpp"
#include "devices/diskii/Floppy525_woz.hpp"
#include "util/ResetController.hpp"

// Soft-switch offsets within the slot's I/O page (same as ndiskii.hpp)
#define DiskII_Ph0_Off       0x00
#define DiskII_Ph0_On        0x01
#define DiskII_Ph1_Off       0x02
#define DiskII_Ph1_On        0x03
#define DiskII_Ph2_Off       0x04
#define DiskII_Ph2_On        0x05
#define DiskII_Ph3_Off       0x06
#define DiskII_Ph3_On        0x07
#define DiskII_Motor_Off     0x08
#define DiskII_Motor_On      0x09
#define DiskII_Drive1_Select 0x0A
#define DiskII_Drive2_Select 0x0B
#define DiskII_Q6L           0x0C
#define DiskII_Q6H           0x0D
#define DiskII_Q7L           0x0E
#define DiskII_Q7H           0x0F


class DiskII_WOZ_Controller : public StorageDevice {
    computer_t   *computer;
    SoundEffect  *sound_effect;
    SoundEffectContainer_t sounds[5];
    NClock       *clock;
    Floppy525_woz drives[2];
    uint8_t       drive_select = 0;
    bool          motor = false;
    uint64_t      mark_cycles_turnoff = 0;

    int running_chunknumber  = 0;
    int start_track_movement = -1;

    const char *sound_files[5] = {
        "sounds/shugart-drive.wav",
        "sounds/shugart-stop.wav",
        "sounds/shugart-head.wav",
        "sounds/shugart-open.wav",
        "sounds/shugart-close.wav",
    };

    void request_motor_off() {
        mark_cycles_turnoff = clock->get_c14m() + 14318180;
    }

    void request_motor_on() {
        mark_cycles_turnoff = 0;
        motor = true;
        drives[drive_select].set_enable(true);
    }

public:
    DiskII_WOZ_Controller(SoundEffect *sound_effect, NClockII *clock)
        : StorageDevice(),
          drives{Floppy525_woz(sound_effect, clock),
                 Floppy525_woz(sound_effect, clock)}
    {
        this->sound_effect = sound_effect;
        this->clock        = clock;

        for (int i = 0; i < SDL_arraysize(sounds); i++) {
            sounds[i].fname = sound_files[i];
            sounds[i].si    = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
        }
    }

    void reset() {
        drive_select        = 0;
        motor               = false;
        mark_cycles_turnoff = 0;
        drives[0].reset();
        drives[1].reset();
    }

    bool get_motor()   { return motor; }
    uint8_t get_track() { return drives[drive_select].get_track(); }

    bool diskii_running_last = false;
    int  tracknumber_last    = 0;

    void soundeffects_update() {
        int tracknumber = drives[drive_select].get_track();

        if (diskii_running_last && !motor) {
            diskii_running_last = false;
            sound_effect->flush(SE_SHUGART_DRIVE);
        }

        if (motor) {
            int dl = (int) sounds[SE_SHUGART_DRIVE].si->wav_data_len / 10;
            if (SDL_GetAudioStreamQueued(sounds[SE_SHUGART_DRIVE].si->stream) < dl) {
                SDL_PutAudioStreamData(sounds[SE_SHUGART_DRIVE].si->stream,
                    sounds[SE_SHUGART_DRIVE].si->wav_data + dl * running_chunknumber, dl);
                running_chunknumber++;
                if (running_chunknumber > 8) running_chunknumber = 0;
            }
        }

        if (tracknumber >= 0 && (tracknumber_last != tracknumber)) {
            int ind = 200 * 2 * std::abs(start_track_movement - tracknumber);
            int len = ((int)(200 * 2) * std::abs(tracknumber_last - tracknumber));
            if (ind + len > sounds[SE_SHUGART_HEAD].si->wav_data_len)
                len = sounds[SE_SHUGART_HEAD].si->wav_data_len - ind;
            sound_effect->play_specific(SE_SHUGART_HEAD, ind, len);

            if (start_track_movement == -1) start_track_movement = tracknumber_last;
            tracknumber_last = tracknumber;
        } else {
            start_track_movement = -1;
        }
    }

    void check_motor_off_timer() {
        if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
            drives[drive_select].set_enable(false);
            motor               = false;
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
                if (motor) {
                    drives[1].set_enable(false);
                    drives[0].set_enable(true);
                }
                drive_select = 0;
                break;
            case DiskII_Drive2_Select:
                if (motor) {
                    drives[0].set_enable(false);
                    drives[1].set_enable(true);
                }
                drive_select = 1;
                break;
            default:
                break;
        }

        Floppy525_woz &sel = drives[drive_select];

        if (((reg & 0x01) == 0) && (sel.get_Q7() == 0 && sel.get_Q6() == 0)) {
            return sel.read_nybble();
        }
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

    bool mount(storage_key_t key, media_descriptor *media) {
        return drives[key.drive].mount(key, media);
    }
    bool unmount(storage_key_t key) {
        return drives[key.drive].unmount(key);
    }
    bool writeback(storage_key_t key) {
        return drives[key.drive].writeback();
    }
    drive_status_t status(storage_key_t key) {
        return drives[key.drive].status();
    }

    DebugFormatter *debug() {
        DebugFormatter *f = new DebugFormatter();
        f->addLine("Drive Select: %d", drive_select);
        f->addLine("Motor: %d", motor);
        f->addLine("Mark Cycles Turnoff: %llu", mark_cycles_turnoff);
        drives[drive_select].debug(f);
        return f;
    }
};


class ndiskII_woz_controller : public SlotData {
public:
    computer_t          *computer;
    NClockII            *clock;
    DiskII_WOZ_Controller *dc;
    ResetController     *reset_control;

    int powerup_reset_cycles = 6;
};

void init_slot_ndiskII_woz(computer_t *computer, SlotType_t slot);

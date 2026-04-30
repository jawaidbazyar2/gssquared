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
    uint64_t      mark_cycles_turnoff = 0;

    // 8 switches in the addressible latch
    uint8_t phase0 = 0; // C0x0, C0x1
    uint8_t phase1 = 0; // C0x2, C0x3
    uint8_t phase2 = 0; // C0x4, C0x5
    uint8_t phase3 = 0; // C0x6, C0x7
    uint8_t enable = 0; // Cx08, Cx09
    uint8_t drive_select = 0; // C0x0A, C0x0B
    uint8_t Q6 = 0; // C0x0C, C0x0D - shift = 0, load = 1
    uint8_t Q7 = 0; // C0x0E, C0x0F - READ=0, WRITE=1

    int running_chunknumber  = 0;
    int start_track_movement = -1;

    uint8_t data_register = 0;
    // LSS QA-hold sub-state: tracks whether we are in the first or second
    // bit-cell after bit 7 of data_register went high.  Mirrors OpenEmulator's
    // `sequencerState` in the SEQUENCER_READSHIFT case.
    bool sequencer_state = false;

    FILE *dbglog = nullptr;

    const char *sound_files[5] = {
        "sounds/shugart-drive.wav",
        "sounds/shugart-stop.wav",
        "sounds/shugart-head.wav",
        "sounds/shugart-open.wav",
        "sounds/shugart-close.wav",
    };

    /** These two methods implement the 555 timer based "delay motor off for one second" mechanism. */
    void request_motor_off() {
        mark_cycles_turnoff = clock->get_c14m() + 14318180;
    }

    void request_motor_on() {
        mark_cycles_turnoff = 0;
        enable = true;
        drives[drive_select].set_enable(true);
    }

public:
    DiskII_WOZ_Controller(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer)
        : StorageDevice(),
          drives{Floppy525_woz(sound_effect, clock, event_timer),
                 Floppy525_woz(sound_effect, clock, event_timer)}
    {
        this->sound_effect = sound_effect;
        this->clock        = clock;

        for (int i = 0; i < SDL_arraysize(sounds); i++) {
            sounds[i].fname = sound_files[i];
            sounds[i].si    = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
        }

        dbglog = fopen("ndiskii_woz_debug.log", "w");
        drives[0].set_dbglog(dbglog);
        drives[1].set_dbglog(dbglog);
    }

    void reset() {
        drive_select        = 0;
        enable              = 0;
        mark_cycles_turnoff = 0;
        phase0 = 0;
        phase1 = 0;
        phase2 = 0;
        phase3 = 0;
        for (int i = 0; i < 4; i++) {
            drives[0].set_phase(i, false);
            drives[1].set_phase(i, false);
        }
        drives[0].reset();
        drives[1].reset();
    }

    bool get_motor()   { return enable; }
    uint8_t get_track() { return drives[drive_select].get_track(); }

    bool diskii_running_last = false;
    int  tracknumber_last    = 0;

    void soundeffects_update() {
        int tracknumber = drives[drive_select].get_track();

        if (diskii_running_last && !enable) {
            diskii_running_last = false;
            sound_effect->flush(SE_SHUGART_DRIVE);
        }

        if (enable) {
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

    void frameUpdate(bool soundEffects) {
        check_motor_off_timer();

        if (soundEffects) {
            soundeffects_update();
        }
    }

    void check_motor_off_timer() {
        if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
            drives[drive_select].set_enable(false);
            enable               = 0;
            mark_cycles_turnoff = 0;
        }
    }

    inline void set_phase(uint8_t phase, bool onoff) {
        Floppy525_woz &sel = drives[drive_select];
        sel.set_phase(phase, onoff);
    }

    void fast_forward() {
        uint64_t now     = clock->get_cycles();

        if (!enable /* || !cur_track_ptr || cur_track_ptr->bit_count == 0 */) {
            // Motor off or no track: still update bit_fp so position is consistent
            // when the track becomes available, but don't shift any bits.
            return;
        }

        // this updates the sim and tells us how many bits to update through the LSS.
        uint64_t bits_to_sim = drives[drive_select].fast_forward(now);

        for (uint64_t i = 0; i < bits_to_sim; i++) {
            uint8_t bit = drives[drive_select].read_pulse();

            // LSS READSHIFT behavior (mirrors OpenEmulator's
            // AppleDiskIIInterfaceCard::updateSequencer SEQUENCER_READSHIFT case):
            // while QA (bit 7) is 0, shift incoming RP bits left into the data
            // register so the CPU can observe partial-nibble accumulation
            // (e.g. 1F, 3F, 7F, FF).  Once QA goes high the byte holds for two
            // more bit cells, then the LSS preloads `0x02 | bit` for the next
            // nibble's leading bits.
            if (data_register & 0x80) {
                if (!sequencer_state) {
                    sequencer_state = bit;
                } else {
                    sequencer_state = false;
                    data_register = 0x02 | bit;
                }
            } else {
                data_register = (data_register << 1) | bit;
            }
        }
    }

    // ─── Nybble I/O ───────────────────────────────────────────────────────────────

    uint8_t read_nybble() {
        // Reads of Q6L are non-destructive on real hardware: the CPU just
        // samples whatever the LSS data register currently holds.  bit-cell
        // accumulation (and the QA-hold reset) happens in fast_forward().
        return data_register;
    }

    void write_nybble(uint8_t /* nybble */) {
        // WOZ write-back requires bit-level splicing into the bitstream.
        // Deferred: stub only.
    }

    void decode(uint8_t reg) {
        Floppy525_woz &sel = drives[drive_select];
        switch (reg) {
            case DiskII_Ph0_Off:
                phase0 = 0;
                set_phase(0, false);
                break;
            case DiskII_Ph0_On:
                phase0 = 1;
                set_phase(0, true);
                break;
            case DiskII_Ph1_Off:
                phase1 = 0;
                set_phase(1, false);
                break;
            case DiskII_Ph1_On:
                phase1 = 1;
                set_phase(1, true);
                break;
            case DiskII_Ph2_Off:
                phase2 = 0;
                set_phase(2, false);
                break;
            case DiskII_Ph2_On:
                phase2 = 1;
                set_phase(2, true);
                break;
            case DiskII_Ph3_Off:
                phase3 = 0;
                set_phase(3, false);
                break;
            case DiskII_Ph3_On:
                phase3 = 1;
                set_phase(3, true);
                break;
            case DiskII_Motor_Off:
                // this is correct, this does live in the controller.
                request_motor_off();
                break;
            case DiskII_Motor_On:
                request_motor_on();
                break;
            case DiskII_Drive1_Select:
                if (enable) {
                    drives[1].set_enable(false);
                    drives[0].set_enable(true);
                }
                drive_select = 0;
                break;
            case DiskII_Drive2_Select:
                if (enable) {
                    drives[0].set_enable(false);
                    drives[1].set_enable(true);
                }
                drive_select = 1;
                break;

            case DiskII_Q6L:  
                Q6 = 0;
                break;
            case DiskII_Q6H:
                Q6 = 1;
                break;
            case DiskII_Q7L:
                Q7 = 0;
                break;
            case DiskII_Q7H:
                Q7 = 1;
                break;
        }
    }
    
    uint8_t read_cmd(uint16_t address) {
        uint16_t reg = address & 0x0F;
        Floppy525_woz &sel = drives[drive_select];

        uint8_t cur_track = sel.get_track();

        fast_forward();

        decode(reg);

        switch (reg) {
            case DiskII_Q6L:
                /** when Q6=0 and Q7=0, then cycle another bit read of a nybble from the disk */
                /** when Q6L is read, and Q7H was previously set (written) then we need to write the byte to the disk. */    
                if (Q7 == 1 || Q6 == 1) {
                    write_nybble(data_register);
                }
                break;

            case DiskII_Q7L:

                if (Q6 == 1) { // Q6H then Q7L is a write protect sense.
                    uint8_t xwp = sel.get_write_protect() << 7;
                    //printf("wp: Q7: %d, Q6: %d, wp: %d %02X\n", seldrive.Q7, seldrive.Q6, seldrive.write_protect, xwp);
                    return xwp; // write protect sense. Return hi bit set (write protected)
                }
                break;

        }

        fprintf(dbglog, "read_cmd: %lld reg:%X sl:%d  track=%d, cur_track=%d, Q7=%d, Q6=%d en:%d ph [ %d %d %d %d ]\n", 
            clock->get_cycles(), reg, drive_select, sel.get_track(), cur_track, Q7, Q6, enable, phase0, phase1, phase2, phase3);


        if (((reg & 0x01) == 0) && (Q7 == 0 && Q6 == 0)) {
            return read_nybble();
        }
        return 0; // sel.read_cmd used to do this..
    }

    void write_cmd(uint16_t address, uint8_t data) {
        uint16_t reg = address & 0x0F;
        Floppy525_woz &sel = drives[drive_select];
        uint8_t cur_track = sel.get_track();

        decode(reg);

        switch (reg) {
            case DiskII_Q6H:
                data_register = data;
                break;
            case DiskII_Q7H:
                data_register = data;
                break;
        }

        fprintf(dbglog, "read_cmd: %lld reg:%X sl:%d  track=%d, cur_track=%d, Q7=%d, Q6=%d en:%d ph [ %d %d %d %d ]\n", 
            clock->get_cycles(), reg, drive_select, sel.get_track(), cur_track, Q7, Q6, enable, phase0, phase1, phase2, phase3);

        sel.write_cmd(address, data);
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
        f->addLine("Enable: %d", enable);
        f->addLine("Mark Cycles Turnoff: %llu", mark_cycles_turnoff);
        drives[drive_select].debug(f);
        return f;
    }
};

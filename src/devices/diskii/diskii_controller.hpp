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

#include <cstring>
#include <string>
#include <vector>

#include "NClock.hpp"
#include "util/DebugFormatter.hpp"
#include "util/media.hpp"
#include "util/SoundEffectKeys.hpp"
#include "devices/floppy/Floppy525_woz.hpp"

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

#define DISKII_SWITCH_COUNT 8

class DiskII_WOZ_Controller : public StorageDevice {
    SoundEffect  *sound_effect;
    SoundEffectContainer_t sounds[5];
    NClock       *clock;
    Floppy525_woz drives[2];
    uint64_t      mark_cycles_turnoff = 0;

    // Effective "motor is physically spinning" state.  Distinct from the
    // diskii_enable latch bit below: writing $C0E8 (Motor_Off) clears the
    // latch immediately but motor_on stays 1 until the 555 timer in
    // check_motor_off_timer() expires (~1 second of 14M cycles).  Apple II
    // software (especially ProDOS) relies on this grace window so the motor
    // doesn't have to re-spin-up between closely-spaced accesses.
    // "enable" out the cable is not the same as "enable" in the controller addressible latch on 5.25.
    uint8_t       motor_on = 0;

    // 8 switches in the addressible latch (74LS259 on the card).  These
    // reflect exactly what the CPU last wrote to the $C0x0-$C0xF soft-switch
    // pairs; they are the instantaneous latch state, NOT any derived
    // "physical" state.  See motor_on above for the actual motor state.
    union {
        uint8_t switches[DISKII_SWITCH_COUNT];
        struct {
            uint8_t diskii_phase0; // C0x0, C0x1
            uint8_t diskii_phase1; // C0x2, C0x3
            uint8_t diskii_phase2; // C0x4, C0x5
            uint8_t diskii_phase3; // C0x6, C0x7
            uint8_t diskii_enable; // C0x8, C0x9 -- latch bit only; use motor_on for "is motor running"
            uint8_t diskii_select; // C0x0A, C0x0B
            uint8_t diskii_q6; // C0x0C, C0x0D - SHIFT=0, LOAD=1
            uint8_t diskii_q7; // C0x0E, C0x0F - READ=0, WRITE=1
        };        
    };

    // controller data register
    uint8_t data_register = 0;
    // LSS QA-hold sub-state: tracks whether we are in the first or second
    // bit-cell after bit 7 of data_register went high.  Mirrors OpenEmulator's
    // `sequencerState` in the SEQUENCER_READSHIFT case. This is a helper to fast-forward lss state.
    bool sequencer_state = false;

    // Soundeffects simulation
    int running_chunknumber  = 0;
    int start_track_movement = -1;

    const char *sound_files[5] = {
        "sounds/shugart-drive.wav",
        "sounds/shugart-stop.wav",
        "sounds/shugart-head.wav",
        "sounds/shugart-open.wav",
        "sounds/shugart-close.wav",
    };

    /** These two methods implement the 555 timer based "delay motor off for one second" mechanism. */
    void request_motor_off() {
        mark_cycles_turnoff = clock->get_c14m() + clock->get_c14m_per_second();
    }

    void request_motor_on() {
        mark_cycles_turnoff = 0;
        motor_on = 1;
        drives[diskii_select].set_enable(true);
    }

public:
    DiskII_WOZ_Controller(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer,
                          uint16_t slot)
        : StorageDevice(),
          drives{Floppy525_woz(sound_effect, clock, event_timer, slot, 0),
                 Floppy525_woz(sound_effect, clock, event_timer, slot, 1)}
    {
        this->sound_effect = sound_effect;
        this->clock        = clock;

        for (int i = 0; i < SDL_arraysize(sounds); i++) {
            sounds[i].fname = sound_files[i];
            sounds[i].si    = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
        }

        memset(switches, 0, sizeof(switches));

    }

    void reset() {
        memset(switches, 0, sizeof(switches));
        motor_on            = 0;
        mark_cycles_turnoff = 0;

        for (int i = 0; i < 4; i++) {
            drives[0].set_phase(i, false);
            drives[1].set_phase(i, false);
        }
        drives[0].set_enable(false);
        drives[1].set_enable(false);
    }

    bool diskii_running_last = false;
    int  tracknumber_last    = 0;

    static SoundChannel channel_for_drive(uint8_t drive_select) {
        return drive_select == 0 ? SoundChannel::Left : SoundChannel::Right;
    }

    void soundeffects_update() {
        int tracknumber = drives[diskii_select].get_track();
        const SoundChannel ch = channel_for_drive(diskii_select);

        if (diskii_running_last && !motor_on) {
            diskii_running_last = false;
            sound_effect->flush(SE_SHUGART_DRIVE);
        }

        if (motor_on) {
            // Mono chunk size from WAV; stream queues stereo (2×) after expand.
            int dl = (int) sounds[SE_SHUGART_DRIVE].si->wav_data_len / 10;
            if (sound_effect->get_queued(SE_SHUGART_DRIVE) < dl * 2) {
                sound_effect->play_specific(SE_SHUGART_DRIVE, dl * running_chunknumber, dl, ch);
                running_chunknumber++;
                if (running_chunknumber > 8) running_chunknumber = 0;
            }
        }

        if (tracknumber >= 0 && (tracknumber_last != tracknumber)) {
            int ind = 200 * 2 * std::abs(start_track_movement - tracknumber);
            int len = ((int)(200 * 2) * std::abs(tracknumber_last - tracknumber));
            if (ind + len > sounds[SE_SHUGART_HEAD].si->wav_data_len)
                len = sounds[SE_SHUGART_HEAD].si->wav_data_len - ind;
            sound_effect->play_specific(SE_SHUGART_HEAD, ind, len, ch);

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
            drives[diskii_select].set_enable(false);
            motor_on            = 0;
            mark_cycles_turnoff = 0;
        }
    }

    void fast_forward() {
        if (!motor_on) {
            // Motor off: the LSS state machine is stopped.
            return;
        }

        uint64_t now     = clock->get_cycles();

        // this updates the sim and tells us how many bits to update through the LSS.
        uint64_t bits_to_sim = drives[diskii_select].fast_forward(/* now */);

        for (uint64_t i = 0; i < bits_to_sim; i++) {
            if (diskii_q7 == 0 && diskii_q6 == 0) {
                // READSHIFT (mirrors OpenEmulator's
                // AppleDiskIIInterfaceCard::updateSequencer SEQUENCER_READSHIFT
                // case): while QA (bit 7) is 0, shift incoming RP bits left
                // into the data register so the CPU can observe partial-nibble
                // accumulation (e.g. 1F, 3F, 7F, FF).  Once QA goes high the
                // byte holds for two more bit cells, then the LSS preloads
                // `0x02 | bit` for the next nibble's leading bits.
                uint8_t bit = drives[diskii_select].read_pulse();
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
            } else if (diskii_q7 == 0 && diskii_q6 == 1) {
                // READLOAD (SEQUENCER_READLOAD): the LSS executes SR every step,
                // shifting the write-protect sense bit right into bit 7 of the
                // data register.  Repeated reads while LOAD is held therefore
                // saturate the register (FF for WP=1, 00 for WP=0).
                drives[diskii_select].read_pulse(); // advance but discard.
                uint8_t wp = drives[diskii_select].get_write_protect() & 1;
                data_register = (data_register >> 1) | (wp << 7);
                sequencer_state = false;
            } else if (diskii_q7 == 1) {
                // WRITESHIFT / WRITELOAD: per OE both Q6 sub-modes behave
                // identically — shift QA (bit 7) of data_register out to disk,
                // then shift data_register left.  The actual CPU-initiated load
                // happens out-of-band in write_cmd() (any odd-address write
                // while motor enabled loads data_register).
                uint8_t out_bit = (data_register >> 7) & 0x01;
                drives[diskii_select].write_pulse(out_bit);
                data_register   = static_cast<uint8_t>(data_register << 1);
                sequencer_state = false;
            }
        }
    }

    // ─── Softswitch Update ───────────────────────────────────────────────────────────────

    inline void decode(uint8_t reg) {
        // Update the switch state
        switches[reg>>1] = reg & 0x01;
        
        switch (reg) {
            case DiskII_Ph0_Off:
            case DiskII_Ph0_On:
            case DiskII_Ph1_Off:
            case DiskII_Ph1_On:
            case DiskII_Ph2_Off:
            case DiskII_Ph2_On:
            case DiskII_Ph3_Off:
            case DiskII_Ph3_On:
                drives[diskii_select].set_phase(reg>>1,reg&0x01);
                break;
            case DiskII_Motor_Off:
                // this is correct, this does live in the controller.
                request_motor_off();
                break;
            case DiskII_Motor_On:
                request_motor_on();
                break;
            case DiskII_Drive1_Select:
                if (motor_on) {
                    drives[1].set_enable(false);
                    drives[0].set_enable(true);
                }
                break;
            case DiskII_Drive2_Select:
                if (motor_on) {
                    drives[0].set_enable(false);
                    drives[1].set_enable(true);
                }
                break;
        }
    }
    
    uint8_t read(uint16_t reg) {
        //uint16_t reg = address & 0x0F;

        fast_forward();

        decode(reg);

        // Per-bit-cell write (and WP shift) handling is now entirely inside
        // fast_forward(); reads of $C0EC no longer need an explicit
        // "trigger write" hook here.

        if ((reg & 0x01) == 0) {
            return data_register;
        }
        return 0; // odd-address reads return floating bus (simplified to 0)
    }

    void write(uint16_t reg, uint8_t data) {
        //uint16_t reg = address & 0x0F;

        // Drain LSS work using the OLD Q6/Q7 state before the switch flips.
        fast_forward();

        decode(reg);

        // Hardware behavior: any odd-address write while the drive is enabled
        // captures the CPU value into data_register (matches OE's
        // AppleDiskIIInterfaceCard::write).  This subsumes the previous
        // Q6H/Q7H-only special case.
        if (motor_on && (reg & 0x01)) {
            data_register = data;
        }
    }

    bool mount(storage_key_t key, std::vector<media_descriptor *> media_list) {
        if (media_list.size() > 1) return false;
        media_descriptor *media = media_list[0];
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
        f->addLine("Drive Select: %d", diskii_select);
        f->addLine("Motor On: %d (latch=%d)", motor_on, diskii_enable);
        f->addLine("Mark Cycles Turnoff: %llu", mark_cycles_turnoff);
        drives[diskii_select].debug(f);
        return f;
    }

    /** Pack Disk II STATE_GET v1 blob (60 bytes). See Docs/DebugProtocol.md. */
    bool pack_state(std::vector<uint8_t> &out, std::string &err) {
        (void)err;
        constexpr uint32_t kVersion = 1;
        constexpr size_t kBlobSize = 60;
        constexpr size_t kDriveSize = 16;
        out.assign(kBlobSize, 0);

        std::memcpy(out.data() + 0, &kVersion, 4);
        out[4] = diskii_select;
        out[5] = motor_on;
        out[6] = diskii_enable;
        out[7] = diskii_q6;
        out[8] = diskii_q7;
        out[9] = data_register;
        out[10] = sequencer_state ? 1 : 0;
        out[11] = 0;
        std::memcpy(out.data() + 12, &mark_cycles_turnoff, 8);
        const uint64_t cycles = clock ? clock->get_cycles() : 0;
        std::memcpy(out.data() + 20, &cycles, 8);

        for (int d = 0; d < 2; ++d) {
            uint8_t *rec = out.data() + 28 + static_cast<size_t>(d) * kDriveSize;
            const int16_t track = static_cast<int16_t>(drives[d].get_track());
            const int16_t max_t = drives[d].get_max_tracks();
            std::memcpy(rec + 0, &track, 2);
            std::memcpy(rec + 2, &max_t, 2);
            rec[4] = drives[d].get_phase(0);
            rec[5] = drives[d].get_phase(1);
            rec[6] = drives[d].get_phase(2);
            rec[7] = drives[d].get_phase(3);
            rec[8] = drives[d].get_enable() ? 1 : 0;
            rec[9] = drives[d].get_write_protect();
            rec[10] = drives[d].get_is_mounted() ? 1 : 0;
            // 11–15 pad
        }
        return true;
    }
};

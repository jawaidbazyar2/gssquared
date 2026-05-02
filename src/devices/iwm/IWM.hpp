#pragma once

#include "NClock.hpp"
#include "computer.hpp"
#include "SlotData.hpp"
#include "devices/diskii/Floppy525.hpp"
#include "util/SoundEffect.hpp"
#include "debug.hpp"

#include "IWM_Drive.hpp"
#include "IWM_525.hpp"
#include "IWM_35.hpp"
//#include "devices/diskii/Floppy525_woz.hpp"

#include "util/SoundEffectKeys.hpp"

class IWM /* : public StorageDevice */ {
    protected:

        SoundEffect *sound_effect;
        SoundEffectContainer_t sounds[5];
        NClockII *clock;

        IWM_Drive *drives_525[2];
        IWM_Drive *drives_35[2];
        //Floppy525_woz drives_525[2];

        uint64_t mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.
        bool motor = false;
        //int drive_select = 0;

        int running_chunknumber = 0;
        int start_track_movement = -1;

        uint8_t any_enabled = 0;
        uint8_t any_drive_on = 0;

        /* You can access the switch states either by array index, or by individual switch name */
        union {
            uint8_t switches[IWM_SWITCH_COUNT];
            struct {
                uint8_t iwm_ca0;
                uint8_t iwm_ca1;
                uint8_t iwm_ca2;
                uint8_t iwm_lstrb;
                uint8_t iwm_enable;
                uint8_t iwm_select;
                uint8_t iwm_q6;
                uint8_t iwm_q7;
            };        
        };

        uint32_t drive_selected = 0; // 0 = 5.25 1, 1 = 5.25 2, 2 = 3.5 1, 3 = 3.5 2

        uint8_t sense_input = 0;
        union {
            struct {
                uint8_t dr_reserved: 6;
                uint8_t dr_enable35 : 1;
                uint8_t dr_sel : 1;
            };
            uint8_t disk_register;
        };

        union {
            struct {
                uint8_t mr_latch : 1;
                uint8_t mr_hsprotocol : 1;
                uint8_t mr_motorofftimer : 1;
                uint8_t mr_bitcelltime : 1;
                uint8_t mr_clockspeed : 1;
                uint8_t mr_reserved : 3;
            };
            uint8_t reg_mode; // write only
        };
        union {
            struct {
                uint8_t hr_reserved : 6;
                uint8_t hr_underrun : 1;
                uint8_t hr_register_ready : 1;
            };
            uint8_t reg_handshake;
        };

        // controller data register
        uint8_t data_register = 0;
        // LSS QA-hold sub-state: tracks whether we are in the first or second
        // bit-cell after bit 7 of data_register went high.  Mirrors OpenEmulator's
        // `sequencerState` in the SEQUENCER_READSHIFT case. This is a helper to fast-forward lss state.
        bool sequencer_state = false;


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
            if (dr_enable35) {
                // TODO: 3.5 drive motor off instantly.
            } else {
                mark_cycles_turnoff = clock->get_c14m() + clock->get_c14m_per_second();
            }
            //if (DEBUG(DEBUG_DISKII)) printf("request_motor_off: %llu\n", u64_t(mark_cycles_turnoff));
        }

        // motor on is always immediate.
        void request_motor_on() {
            if (dr_enable35) { // is a 3.5 drive
                // TODO: 3.5 drive motor on instantly.
            } else { // is a 5.25 drive
                mark_cycles_turnoff = 0;
                motor = true;
                drives_525[drive_selected]->set_enable(true);
            }
            //if (DEBUG(DEBUG_DISKII)) printf("request_motor_on: %llu\n", u64_t(mark_cycles_turnoff));
        }

    public:
        IWM(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer) 
           /*  : StorageDevice(), 
              drives_525{Floppy525_woz(sound_effect, clock, event_timer), Floppy525_woz(sound_effect, clock, event_timer)} */ {
            /* for (uint32_t i = 0; i < IWM_SWITCH_COUNT; i++) {
                switches[i] = 0;
            } */
            drives_525[0] = new IWM_Drive_525(sound_effect, clock);
            drives_525[1] = new IWM_Drive_525(sound_effect, clock);
            drives_35[0] = new IWM_Drive_35(sound_effect, clock);
            drives_35[1] = new IWM_Drive_35(sound_effect, clock);
            reset();

            this->sound_effect = sound_effect;
            this->clock = clock;

            for (int i = 0; i < SDL_arraysize(sounds); i++) {
                sounds[i].fname = sound_files[i];
                sounds[i].si = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
            }

            memset(switches, 0, sizeof(switches));
        };

        ~IWM() {
            for (uint32_t i = 0; i < 2; i++) {
                if (drives_525[i] != nullptr) {
                    delete drives_525[i];
                    drives_525[i] = nullptr;
                }
                if (drives_35[i] != nullptr) {
                    delete drives_35[i];
                    drives_35[i] = nullptr;
                }
            }
        };

        void reset() {
            disk_register = 0;
            for (uint32_t i = 0; i < 2; i++) {
                drives_525[i]->set_enable(false);
                drives_35[i]->set_enable(false);
            }
            mark_cycles_turnoff = 0;
            drive_selected = 0;
            reg_mode = 0;
            reg_handshake = 0;
            motor = false;
        }

        void check_motor_off_timer() {
            if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
                drives_525[drive_selected]->set_enable(false);
                motor = false;
                mark_cycles_turnoff = 0;
            }
        }

        IWM_Drive *get_drive(int index) {
            return drives_525[index];
        }
        
        // utility functions
        inline bool address_odd(uint32_t address) { return address & 0x01; }
        inline bool address_even(uint32_t address) { return (address & 0x01) == 0; }

        uint8_t read_disk_register() { return disk_register; }
        void write_disk_register(uint8_t data) { disk_register = data; }

        uint8_t read_status_register() {
            // TODO: change any_drive_on and sense_input to query selected disk statuses later
            return (reg_mode & 0b000'11111) | any_drive_on << 5 | sense_input << 7;
        }

        inline void decode(uint16_t reg) {
            // Update the switch state
            switches[reg>>1] = reg & 0x01;

            if (dr_enable35) {
                // TODO: 3.5 drive motor on/off instantly.
                switch (reg) {
                    case IWM_ENABLE_ON:
                        any_enabled = true;
                        drives_35[drive_selected]->set_enable(true);
                        break;
                    case IWM_ENABLE_OFF:
                        any_enabled = false;
                        drives_35[drive_selected]->set_enable(false);
                        break;
                    case IWM_SELECT_ON:
                        drives_35[drive_selected]->set_enable(false); // de-select     
                        drive_selected = 1;
                        drives_35[drive_selected]->set_enable(any_enabled);
                        break;
                    case IWM_SELECT_OFF:
                        drives_35[drive_selected]->set_enable(false); // de-select 
                        drive_selected = 0;
                        drives_35[drive_selected]->set_enable(any_enabled);
                        break;
                    default:
                        break;
                }
            } else {
                // 5.25 drive motor on/off handled differently.
                switch (reg) {
                    case IWM_ENABLE_ON:
                        any_enabled = true;
                        request_motor_on();
                        break;
                    case IWM_ENABLE_OFF:
                        any_enabled = false;
                        request_motor_off();
                        break;
                    case IWM_SELECT_ON:
                        drives_525[drive_selected]->set_enable(false); // de-select     
                        drive_selected = 1;
                        drives_525[drive_selected]->set_enable(any_enabled);
                        break;
                    case IWM_SELECT_OFF:
                        drives_525[drive_selected]->set_enable(false); // de-select 
                        drive_selected = 0;
                        drives_525[drive_selected]->set_enable(any_enabled);
                        break;
                    default:
                        break;
                }
            }
           
        }

        uint8_t read(uint16_t reg) {
            assert(reg < IWM_ADDRESS_MAX && "IWM: read address out of bounds");
            //access(address);
            
            if (dr_enable35) {
                //drives_35[drive_selected]->read_cmd(reg);
            } else {
                drives_525[drive_selected]->read_cmd(reg);
            }

            decode(reg);

            /* Read Status Register 
            To access it, turn Q7 off and Q6 on, and read from any even-numbered address in the
            $C0E0...$C0EF range.
            */
            if (address_even(reg) && !iwm_q7 && iwm_q6) {
                return read_status_register();
            }
            /* The handshake register is a read-only register used when writing to the
            disk in asynchronous mode (when bit 1 of the mode register is on). It
            indicates whether the IWM is ready to receive the next data byte. To
            read the handshake register, turn switches Q6 off and Q7 on, and read
            from any even-numbered address  */
            if (address_even(reg) && !iwm_q6 && iwm_q7) {
                return reg_handshake;                
            }
            /* The data register is the register that you read to get the actual data
            from the disk and write to store data on the disk. To read it, turn Q6
            and Q7 off and read from any even-numbered address in the $C0E0...$C0EF
            range. */
            if (address_even(reg) && !iwm_q6 && !iwm_q7) {
                if (dr_enable35) {
                    //return drives_35[drive_selected]->read_data_register();
                } else {
                    return drives_525[drive_selected]->read_data_register();
                }
            }
            return 0;
        }

        void write(uint16_t reg, uint8_t data) { 
            assert(reg < IWM_ADDRESS_MAX && "IWM: write address out of bounds");
            //access(address);

            if (dr_enable35) {
                //drives_35[drive_selected]->write_cmd(reg, data);
            } else {
                drives_525[drive_selected]->write_cmd(reg, data);
            }

            decode(reg);

            
            /*
            Note that the drive may remain active for a second or two after the ENABLE
            access, and that the write to the mode register will fail unless the drive
            is fully deactivated.
            This means that the mode register must be repeatedly
            written until the status register (see below) indicates that the desired
            changes have taken effect.
            */
            if (address_odd(reg) && !any_enabled && !any_drive_on && iwm_q6 && iwm_q7) { // write to mode register.
                reg_mode = data;
            }
            /* To write it, turn Q6 and Q7 on and write to any odd-numbered
            address in the $C0E0...$C0EF range. */
            // TODO: I'm disabling this to, I think, make 5.25" floppy work, but unsure how this will interact with 3.5" drive.
/*             if (address_odd(address) && iwm_q6 && iwm_q7) {
                drives[drive_selected]->write_data_register(data);
            } */
        }

        void debug_output(DebugFormatter *df) {
            df->addLine("CA0: %d, CA1: %d, CA2: %d, LSTRB: %d, ENABLE: %d, SELECT: %d, Q6: %d, Q7: %d",
                iwm_ca0, iwm_ca1, iwm_ca2, iwm_lstrb, iwm_enable, iwm_select, iwm_q6, iwm_q7);
            df->addLine("Disk Register: %02X", disk_register);
            df->addLine("Mode: %02X  ClkSpd: %d  BitCell: %d  MotorOff: %d  HSProtocol: %d", reg_mode, mr_clockspeed, mr_bitcelltime, mr_motorofftimer, mr_hsprotocol);
            df->addLine("Handshake: %02X  Status: %02X", reg_handshake, read_status_register());
            df->addLine(  "         5.25/1     5.25/2     3.5/1     3.5/2");
            df->addLine("Drive Selected: %d", drive_selected);
            df->addLine("  Motor:   %d          %d          %d          %d      ", 
                drives_525[0]->get_motor_on(), 
                drives_525[1]->get_motor_on(), 
                drives_35[2]->get_motor_on(), 
                drives_35[3]->get_motor_on());
            df->addLine("  Sense:   %d          %d          %d          %d      ", 
                drives_525[0]->get_sense_input(), 
                drives_525[1]->get_sense_input(), 
                drives_35[0]->get_sense_input(), 
                drives_35[1]->get_sense_input());
            df->addLine("  LED:     %d          %d          %d          %d      ", 
                drives_525[0]->get_led_status(), 
                drives_525[1]->get_led_status(), 
                drives_35[0]->get_led_status(), 
                drives_35[1]->get_led_status());
            df->addLine("  Track / Side:  %d          %d          %d          %d      ", 
                drives_525[0]->get_track(),
                drives_525[1]->get_track(),
                drives_35[0]->get_track(),
                drives_35[1]->get_track());
        }

  
    bool diskii_running_last = false;
    int tracknumber_last = 0;
    void soundeffects_update() {
        int tracknumber = drives_525[drive_selected]->get_track(); /* drives[drive_select].get_track() */;

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
        //static int running_chunknumber = 0;
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
        //static int start_track_movement = -1;
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

    bool get_motor() { return motor; }

   
        /*
        * Handles state changes on "access" to registers C0E0-C0EF, which apply to either reads or writes.
        */
        /* void access(uint32_t address) { // address must be between 0 and 0x0F
            assert(address <= 0x0F && "IWM: access address out of bounds");
            uint32_t switch_index = (address >> 1);
            bool onoff = (address & 0x01) == 1;
            set_switch(switch_index, onoff); 
        } */

};



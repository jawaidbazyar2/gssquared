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

#include "util/SoundEffectKeys.hpp"

class IWM {
    protected:
        NClockII *clock;

        uint64_t mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.
        bool motor = false;
        //int drive_select = 0;

        SoundEffect *sound_effect;
        SoundEffectContainer_t sounds[5];
        int running_chunknumber = 0;
        int start_track_movement = -1;

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
            if (drive_selected < 2) { // is a 5.25 
                mark_cycles_turnoff = clock->get_c14m() + 14318180;
            } else { // 3.5, drive on/off handled differently.
            }
            //if (DEBUG(DEBUG_DISKII)) printf("request_motor_off: %llu\n", u64_t(mark_cycles_turnoff));
        }

        // motor on is always immediate.
        void request_motor_on() {
            if (drive_selected < 2) { // is a 5.25 
                mark_cycles_turnoff = 0;
                motor = true;
                drives[drive_selected]->set_enable(true);
            }
            //if (DEBUG(DEBUG_DISKII)) printf("request_motor_on: %llu\n", u64_t(mark_cycles_turnoff));
        }

    public:
        IWM(SoundEffect *sound_effect, NClockII *clock) {
            for (uint32_t i = 0; i < IWM_SWITCH_COUNT; i++) {
                switches[i] = 0;
            }
            drives[0] = new IWM_Drive_525(sound_effect, clock);
            drives[1] = new IWM_Drive_525(sound_effect, clock);
            drives[2] = new IWM_Drive_35(sound_effect, clock);
            drives[3] = new IWM_Drive_35(sound_effect, clock);
            reset();

            this->sound_effect = sound_effect;
            this->clock = clock;

            for (int i = 0; i < SDL_arraysize(sounds); i++) {
                sounds[i].fname = sound_files[i];
                sounds[i].si = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
            }
/*             disk_register = 0;
            for (uint32_t i = 0; i < 4; i++) {
                drives[i]->set_enable(false);
            }
            drive_selected = 0;
            reg_mode = 0;
            reg_handshake = 0; */
        };

        void check_motor_off_timer() {
            if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
                drives[drive_selected]->set_enable(false);
                motor = false;
                mark_cycles_turnoff = 0;
            }
        }
      
        IWM_Drive *get_drive(int index) {
            return drives[index];
        }
        
        void mount(media_descriptor *media) {
            drives[0]->mount(0x600, media);  // key = slot 6, drive 0
        }

        void reset() {
            disk_register = 0;
            for (uint32_t i = 0; i < 4; i++) {
                drives[i]->set_enable(false);
            }
            mark_cycles_turnoff = 0;
            drive_selected = 0;
            reg_mode = 0;
            reg_handshake = 0;
            motor = false;
        }

        // utility functions
        inline bool address_odd(uint32_t address) { return address & 0x01; }
        inline bool address_even(uint32_t address) { return (address & 0x01) == 0; }
        //uint32_t register_index() { }

        ~IWM() {};
        void set_switch(uint32_t switch_index, bool onoff) { 
            assert(switch_index < IWM_SWITCH_COUNT && "IWM: switch index out of bounds");
            switches[switch_index] = onoff; 
        }
        bool get_switch(uint32_t switch_index) { 
            assert(switch_index < IWM_SWITCH_COUNT && "IWM: switch index out of bounds");
            return switches[switch_index]; 
        }

        uint8_t read_disk_register() { return disk_register; }
        void write_disk_register(uint8_t data) { disk_register = data; }

        uint8_t read_status_register() {
            // TODO: change any_drive_on and sense_input to query selected disk statuses later
            return (reg_mode & 0b000'11111) | any_drive_on << 5 | sense_input << 7;
        }

        inline void handle_switch(uint32_t address) {
            switch (address) {
                case IWM_ENABLE_ON:
                    any_enabled = true;
                    if (drive_selected < 2) { // is a 5.25 
                        request_motor_on();
                    } else drives[drive_selected]->set_enable(true);
                    break;
                case IWM_ENABLE_OFF:
                    any_enabled = false;
                    if (drive_selected < 2) { // is a 5.25 
                        request_motor_off();
                    } else drives[drive_selected]->set_enable(false);
                    break;
                case IWM_SELECT_ON:
                    drives[drive_selected]->set_enable(false); // de-select     
                    drive_selected = 1;
                    drives[drive_selected]->set_enable(any_enabled);
                    break;
                case IWM_SELECT_OFF:
                    drives[drive_selected]->set_enable(false); // de-select 
                    drive_selected = 0;
                    drives[drive_selected]->set_enable(any_enabled);
                    break;
                default:
                    break;
            }
        }

        uint8_t read(uint32_t address) {
            assert(address < IWM_ADDRESS_MAX && "IWM: read address out of bounds");
            access(address);
            
            drives[drive_selected]->read_cmd(address);

            handle_switch(address);           

            /* Read Status Register 
            To access it, turn Q7 off and Q6 on, and read from any even-numbered address in the
            $C0E0...$C0EF range.
            */
            if (address_even(address) && !iwm_q7 && iwm_q6) {
                return read_status_register();
            }
            /* The handshake register is a read-only register used when writing to the
            disk in asynchronous mode (when bit 1 of the mode register is on). It
            indicates whether the IWM is ready to receive the next data byte. To
            read the handshake register, turn switches Q6 off and Q7 on, and read
            from any even-numbered address  */
            if (address_even(address) && !iwm_q6 && iwm_q7) {
                return reg_handshake;                
            }
            /* The data register is the register that you read to get the actual data
            from the disk and write to store data on the disk. To read it, turn Q6
            and Q7 off and read from any even-numbered address in the $C0E0...$C0EF
            range. */
            if (address_even(address) && !iwm_q6 && !iwm_q7) {
                return drives[drive_selected]->read_data_register();
            }
            return 0;
        }

        void write(uint32_t address, uint8_t data) { 
            assert(address < IWM_ADDRESS_MAX && "IWM: write address out of bounds");
            access(address);

            drives[drive_selected]->write_cmd(address, data);

            handle_switch(address);

            
            /*
            Note that the drive may remain active for a second or two after the ENABLE
            access, and that the write to the mode register will fail unless the drive
            is fully deactivated.
            This means that the mode register must be repeatedly
            written until the status register (see below) indicates that the desired
            changes have taken effect.
            */
            if (address_odd(address) && !any_enabled && !any_drive_on && iwm_q6 && iwm_q7) { // write to mode register.
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
                drives[0]->get_motor_on(), 
                drives[1]->get_motor_on(), 
                drives[2]->get_motor_on(), 
                drives[3]->get_motor_on());
            df->addLine("  Sense:   %d          %d          %d          %d      ", 
                drives[0]->get_sense_input(), 
                drives[1]->get_sense_input(), 
                drives[2]->get_sense_input(), 
                drives[3]->get_sense_input());
            df->addLine("  LED:     %d          %d          %d          %d      ", 
                drives[0]->get_led_status(), 
                drives[1]->get_led_status(), 
                drives[2]->get_led_status(), 
                drives[3]->get_led_status());
            df->addLine("  Track / Side:  %d          %d          %d          %d      ", drives[0]->get_track(),
                drives[1]->get_track(),
                drives[2]->get_track(),
                drives[3]->get_track());
        }

  
    bool diskii_running_last = false;
    int tracknumber_last = 0;
    void soundeffects_update() {
        int tracknumber = drives[drive_selected]->get_track(); /* drives[drive_select].get_track() */;

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

    private:
        /* You can access the switch states either by array index, or by individual switch name */
        union {
            uint32_t switches[IWM_SWITCH_COUNT];
            struct {
                uint32_t iwm_ca0;
                uint32_t iwm_ca1;
                uint32_t iwm_ca2;
                uint32_t iwm_lstrb;
                uint32_t iwm_enable;
                uint32_t iwm_select;
                uint32_t iwm_q6;
                uint32_t iwm_q7;
            };        
        };

        IWM_Drive *drives[4];

        uint32_t drive_selected = 0; // 0 = 5.25 1, 1 = 5.25 2, 2 = 3.5 1, 3 = 3.5 2
        uint8_t any_enabled = 0;
        uint8_t any_drive_on = 0;
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

        /*
        * Handles state changes on "access" to registers C0E0-C0EF, which apply to either reads or writes.
        */
        void access(uint32_t address) { // address must be between 0 and 0x0F
            assert(address <= 0x0F && "IWM: access address out of bounds");
            uint32_t switch_index = (address >> 1);
            bool onoff = (address & 0x01) == 1;
            set_switch(switch_index, onoff); 
        }

};



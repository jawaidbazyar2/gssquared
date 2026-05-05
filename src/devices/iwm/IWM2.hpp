#pragma once

#include "NClock.hpp"
#include "computer.hpp"
#include "SlotData.hpp"
#include "devices/diskii/Floppy525.hpp"
#include "util/SoundEffect.hpp"
#include "debug.hpp"

#include "IWM_Drive.hpp"  // iwm_switch_t constants shared by 5.25 and 3.5 decode
#include "devices/diskii/Floppy525_woz.hpp"
#include "devices/diskii/Floppy35_woz.hpp"

#include "util/SoundEffectKeys.hpp"

class IWM : public StorageDevice {
    protected:

        SoundEffect *sound_effect;
        SoundEffectContainer_t sounds[5];
        NClockII *clock;

        Floppy525_woz drives_525[2];
        Floppy35_woz  drives_35[2];

        uint64_t mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.
        bool motor = false;
        //int drive_select = 0;

        int running_chunknumber = 0;
        int start_track_movement = -1;

        //uint8_t any_enabled = 0;
        //uint8_t any_drive_on = 0;

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

        //uint32_t drive_selected = 0; // 0 = 5.25 1, 1 = 5.25 2, 2 = 3.5 1, 3 = 3.5 2

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

        // controller data register (CPU-visible on data-register reads)
        uint8_t data_register = 0;
        // 3.5 latch mode (L=1): READSHIFT runs against this; data_register is
        // derived so partial assembly never sets bit 7 until the internal
        // byte has MSB set (Neil Parker: poll LDA Q6 / BPL until valid).
        uint8_t internal_data_register = 0;

        // 3.5 async write (H=1): CPU loads bytes via odd Q6+Q7 writes; the IWM
        // serializes at disk bit rate and raises handshake bit 7 when ready
        // for the next byte (Neil Parker write example).
        uint8_t async_shift_reg      = 0;
        uint8_t async_bits_remaining = 0;
        uint8_t async_buffer_register = 0;

        // LSS QA-hold sub-state: tracks whether we are in the first or second
        // bit-cell after bit 7 of the working shift register went high.
        // Mirrors OpenEmulator's `sequencerState` in the SEQUENCER_READSHIFT case.
        bool sequencer_state = false;

        void update_handshake_ready(bool ready, bool no_underrun) {
            hr_register_ready = ready ? 1 : 0;
            hr_underrun         = no_underrun ? 1 : 0;
        }


        /* Load our sound effects */
        const char *sound_files[5] = {
            "sounds/shugart-drive.wav",
            "sounds/shugart-stop.wav",
            "sounds/shugart-head.wav",
            "sounds/shugart-open.wav",
            "sounds/shugart-close.wav",
        };

        // 5.25 ENABLE'-off schedules spindle-off ~1s later (MC14536 timer).
        // 3.5 has no equivalent latch: its spindle is commanded directly by
        // CONT35 $4/$C strobes inside Floppy35_woz, so on 3.5 we simply do
        // nothing and let the drive itself handle spindle state.
        void request_motor_off() {
            if (dr_enable35) return;
            mark_cycles_turnoff = clock->get_c14m() + clock->get_c14m_per_second();
        }

        // 5.25 ENABLE'-on starts the spindle immediately; cancels any pending
        // off-timer. 3.5 ENABLE' just lights the LED / locks the disk — the
        // drive's own set_enable() does that — and never spins the motor.
        void request_motor_on() {
            if (dr_enable35) return;
            mark_cycles_turnoff = 0;
            motor = true;
            drives_525[iwm_select].set_enable(true);
        }

        // Unified "LSS should advance" predicate. For 5.25 this is the
        // request_motor_on/off latched `motor` flag; for 3.5 it's the
        // currently-selected drive's spindle state.
        bool lss_active() {
            if (dr_enable35) return drives_35[iwm_select].get_motor_on();
            return motor;
        }

    public:
        IWM(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer) : StorageDevice(),
            drives_525{Floppy525_woz(sound_effect, clock, event_timer),
                       Floppy525_woz(sound_effect, clock, event_timer)},
            drives_35 {Floppy35_woz (sound_effect, clock, event_timer),
                       Floppy35_woz (sound_effect, clock, event_timer)}
        {
            reset();

            this->sound_effect = sound_effect;
            this->clock = clock;

            for (int i = 0; i < SDL_arraysize(sounds); i++) {
                sounds[i].fname = sound_files[i];
                sounds[i].si = sound_effect->load(sound_files[i], SE_SHUGART_DRIVE + i);
            }

            memset(switches, 0, sizeof(switches));
        };

        ~IWM() = default;

        void reset() {
            memset(switches, 0, sizeof(switches));
            disk_register = 0;
            
            drives_525[0].set_enable(false);
            drives_525[1].set_enable(false);
            drives_35[0].set_enable(false);
            drives_35[1].set_enable(false);
        
            mark_cycles_turnoff = 0;
            reg_mode = 0;
            reg_handshake = 0;
            internal_data_register = 0;
            async_shift_reg          = 0;
            async_bits_remaining     = 0;
            sequencer_state          = false;
            motor = false;
        }

        void check_motor_off_timer() {
            // 5.25 MC14536 off-timer: unchanged. 3.5 never arms this timer
            // (request_motor_off() is a no-op in 3.5 mode).
            if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
                drives_525[iwm_select].set_enable(false);
                motor = false;
                mark_cycles_turnoff = 0;
            }
        }

        /* Floppy525_woz *get_drive_525(int index) {
            return drives_525[index];
        } */
        
        // utility functions
        inline bool address_odd(uint32_t address) { return address & 0x01; }
        inline bool address_even(uint32_t address) { return (address & 0x01) == 0; }

        uint8_t read_disk_register() { return disk_register; }

        // DISKREG side-effects: bit 7 (SEL / "HDSEL") is the 4th selector bit
        // for the 3.5 status/control switch; bit 6 (EN35) picks 3.5 vs. 5.25.
        // When either changes we push the updated SEL into the currently
        // selected 3.5 drive so its sense output tracks.
        void write_disk_register(uint8_t data) {
            const uint8_t prev_sel     = dr_sel;
            const uint8_t prev_en35    = dr_enable35;
            disk_register = data;
            if (dr_sel != prev_sel || dr_enable35 != prev_en35) {
                // Keep both 3.5 drives' sense lines consistent with the new
                // SEL value — cheap, and avoids stale sense if the program
                // toggles the drive-select switch after a DISKREG write.
                drives_35[0].set_hdsel(dr_sel);
                drives_35[1].set_hdsel(dr_sel);
            }
        }

        uint8_t read_status_register() {
            // In 3.5 mode bit 7 is the selected drive's status-selector
            // output; in 5.25 mode it's the WP-sense line (already mirrored
            // into sense_input by the LSS's READLOAD path).
            uint8_t sense = dr_enable35 ? drives_35[iwm_select].read_sense()
                                        : sense_input;
            uint8_t motor_bit = lss_active() ? 1 : 0;
            return (reg_mode & 0b000'11111) | (motor_bit << 5) | (sense << 7);
        }

        void fast_forward() {
            if (!lss_active()) {
                // Motor off: the LSS state machine is stopped.
                return;
            }

            

            // Dispatch the head-advance + LSS loop against whichever drive
            // family is currently selected by dr_enable35.
            if (dr_enable35) {
                uint64_t now = clock->get_vid_cycles();
                fast_forward_impl<Floppy35_woz>(now, drives_35[iwm_select]);
            } else {
                uint64_t now = clock->get_cycles();
                fast_forward_impl<Floppy525_woz>(now, drives_525[iwm_select]);
            }
        }

    private:
        // Shared LSS loop. Templated on the concrete drive type so the
        // Floppy_woz virtuals dispatch directly (read_pulse/write_pulse/
        // fast_forward/get_write_protect) without an extra indirection.
        template <typename Drive>
        void fast_forward_impl(uint64_t now, Drive &drive) {
            uint64_t bits_to_sim = drive.fast_forward(now);

            const bool latch_read = dr_enable35 && mr_latch;
            const bool async_wr   = dr_enable35 && mr_hsprotocol;

            for (uint64_t i = 0; i < bits_to_sim; i++) {
                if (iwm_q7 == 0 && iwm_q6 == 0) {
                    // READSHIFT (mirrors OpenEmulator's SEQUENCER_READSHIFT):
                    // while QA (bit 7) is 0, shift incoming RP bits left.
                    // 5.25 / sync: expose partial nibbles on data_register.
                    // 3.5 latch (L=1): accumulate in internal_data_register;
                    // CPU-visible data_register hides MSB until valid byte.
                    uint8_t bit = drive.read_pulse();
                    if (latch_read) {
                        internal_data_register = static_cast<uint8_t>((internal_data_register << 1) | bit);
                        if (internal_data_register & 0x80) {
                            data_register = internal_data_register;
                            internal_data_register = 0;
                        }
                    } else {
                        uint8_t &shift_reg = data_register;
                        if (shift_reg & 0x80) {
                            if (!sequencer_state) {
                                sequencer_state = bit;
                            } else {
                                sequencer_state = false;
                                shift_reg       = static_cast<uint8_t>(0x02 | bit);
                            }
                        } else {
                            shift_reg = static_cast<uint8_t>((shift_reg << 1) | bit);
                        }
                    }
                } else if (iwm_q7 == 0 && iwm_q6 == 1) {
                    // READLOAD (SEQUENCER_READLOAD): shift the WP-sense bit
                    // right into bit 7 of the shift register.
                    drive.read_pulse(); // advance but discard.
                    uint8_t wp = drive.get_write_protect() & 1;
                    if (latch_read) {
                        internal_data_register = static_cast<uint8_t>((internal_data_register >> 1) | (wp << 7));
                    } else {
                        data_register = static_cast<uint8_t>((data_register >> 1) | (wp << 7));
                    }
                    sequencer_state = false;
                } else if (iwm_q7 == 1) {
                    if (async_wr) {
                        if (async_bits_remaining == 0) {
                            if (hr_register_ready == 0) { // there is data, transfer it.
                                async_shift_reg = async_buffer_register;
                                async_bits_remaining = 8;
                                update_handshake_ready(true, true);
                            } else {
                                update_handshake_ready(true, false); // there was no data, flag underrun
                            }
                        }
                        // Async write: IWM clocks bits out at disk rate; CPU
                        // loads bytes via write() when handshake says ready.                        
                        if (async_bits_remaining > 0) {
                            uint8_t out_bit =
                                static_cast<uint8_t>((async_shift_reg >> 7) & 0x01);
                            drive.write_pulse(out_bit);
                            async_shift_reg = static_cast<uint8_t>(async_shift_reg << 1);
                            async_bits_remaining--;
                        } else {
                            // Waiting for next STA Q6+1 / underrun gap: keep
                            // media timing aligned without flipping bits.
                            // there's nothing left to write, so we emit 0's..
                            drive.write_pulse(0);
                        }
                    } else {
                        // WRITESHIFT / WRITELOAD (sync): per OE both Q6 sub-modes
                        // behave identically — shift QA out to disk, then shift
                        // data_register left.
                        uint8_t out_bit =
                            static_cast<uint8_t>((data_register >> 7) & 0x01);
                        drive.write_pulse(out_bit);
                        data_register =
                            static_cast<uint8_t>(data_register << 1);
                        sequencer_state = false;
                    }
                }
            }
        }

    public:

        inline void decode(uint16_t reg) {
            // Remember the pre-write SELECT state so we can detect a change
            // and flip which physical drive is "selected" below.
            const uint8_t prev_select = iwm_select;

            // Update the switch state.
            switches[reg>>1] = reg & 0x01;

            if (dr_enable35) {
                switch (reg) {
                    case IWM_CA0_OFF:
                    case IWM_CA0_ON:
                    case IWM_CA1_OFF:
                    case IWM_CA1_ON:
                    case IWM_CA2_OFF:
                    case IWM_CA2_ON:
                    case IWM_LSTRB_OFF:
                    case IWM_LSTRB_ON:
                        // CA0-CA2 and LSTRB are state/control selectors for
                        // the 3.5 drive. The drive decodes them internally;
                        // IWM just forwards the raw bit.
                        drives_35[iwm_select].set_phase(reg >> 1, reg & 0x01);
                        break;
                    case IWM_ENABLE_ON:
                        // 3.5 ENABLE' = LED on / disk locked. Does NOT start
                        // the spindle (that's CONT35 $4).
                        drives_35[iwm_select].set_enable(true);
                        break;
                    case IWM_ENABLE_OFF:
                        drives_35[iwm_select].set_enable(false);
                        break;
                    case IWM_SELECT_ON:
                    case IWM_SELECT_OFF:
                        if (iwm_select != prev_select) {
                            // De-select the previously-active drive and
                            // hand the LED + CA/SEL state to the new one.
                            drives_35[prev_select].set_enable(false);
                            // Re-push HDSEL so the newly-selected drive's
                            // sense output reflects the current SEL line.
                            drives_35[iwm_select].set_hdsel(dr_sel);
                            drives_35[iwm_select].set_enable(iwm_enable);
                        }
                        break;
                    default:
                        break;
                }
            } else {
                // 5.25 drive motor on/off handled differently.
                switch (reg) {
                    case IWM_CA0_OFF:
                    case IWM_CA0_ON:
                    case IWM_CA1_OFF:
                    case IWM_CA1_ON:
                    case IWM_CA2_OFF:
                    case IWM_CA2_ON:
                    case IWM_LSTRB_OFF:
                    case IWM_LSTRB_ON:
                        drives_525[iwm_select].set_phase(reg>>1,reg&0x01);
                        break;
                    case IWM_ENABLE_ON:
                        request_motor_on();
                        break;
                    case IWM_ENABLE_OFF:
                        request_motor_off();
                        break;
                    case IWM_SELECT_ON:
                        drives_525[1-iwm_select].set_enable(false); // de-select
                        drives_525[iwm_select].set_enable(motor);
                        break;
                    case IWM_SELECT_OFF:
                        drives_525[1-iwm_select].set_enable(false); // de-select
                        drives_525[iwm_select].set_enable(motor);
                        break;
                    default:
                        break;
                }
            }
        }

        uint8_t read(uint16_t reg) {
            assert(reg < IWM_ADDRESS_MAX && "IWM: read address out of bounds");

            // Roll the LSS forward to "now" using whichever drive family is
            // currently selected. fast_forward() itself is a no-op when the
            // LSS is inactive (5.25 motor off or 3.5 spindle off).
            fast_forward();

            decode(reg);

            /* Read Status Register
            To access it, turn Q7 off and Q6 on, and read from any
            even-numbered address in the $C0E0...$C0EF range. */
            if (address_even(reg) && iwm_q6 && !iwm_q7) {
                return read_status_register();
            }
            /* The handshake register is a read-only register used when
            writing to the disk in asynchronous mode (when bit 1 of the
            mode register is on). Q6 off, Q7 on, even address. */
            if (address_even(reg) && !iwm_q6 && iwm_q7) {
                return reg_handshake;
            }
            /* Data register: Q6 off, Q7 off, even address. Same path for
            5.25 and 3.5 — the LSS above has already latched the result in
            data_register. */
            if (address_even(reg) && !iwm_q6 && !iwm_q7) {
                uint8_t val = data_register;
                // In Latch mode (L=1), reading the data register clears it so that
                // subsequent polls will wait for the next full byte to assemble.
                if (dr_enable35 && mr_latch) {
                    data_register = 0;
                }
                return val;
            }
            return 0;
        }

        void write(uint16_t reg, uint8_t data) {
            assert(reg < IWM_ADDRESS_MAX && "IWM: write address out of bounds");

            fast_forward();

            decode(reg);

            
            /*
            Note that the drive may remain active for a second or two after
            the ENABLE access, and that the write to the mode register will
            fail unless the drive is fully deactivated. The mode register
            must be repeatedly written until the status register indicates
            the desired changes have taken effect. See the ROM's SELIWM loop.
            */
            if (!lss_active() && address_odd(reg) && iwm_q6 && iwm_q7) {
                reg_mode = data;
            }
            /* Data-register write: Q6+Q7 on, odd address, motor running.
            5.25 sync: LSS consumes data_register each WRITESHIFT step.
            3.5 async (H=1): bytes queue into async_shift_reg; handshake
            clears until another byte is needed (Neil Parker write sequence). */
            else if (lss_active() && address_odd(reg) && iwm_q6 && iwm_q7) {
                data_register = data;
                if (dr_enable35 && mr_hsprotocol) {
                    async_buffer_register = data;
                    /* async_shift_reg      = data;
                    async_bits_remaining = 8; */
                    update_handshake_ready(false, true); // mark busy (i.e. loaded)
                }
            }
        }

        void debug_output(DebugFormatter *df) {
            df->addLine("CA0: %d, CA1: %d, CA2: %d, LSTRB: %d, ENABLE: %d, SELECT: %d, Q6: %d, Q7: %d",
                iwm_ca0, iwm_ca1, iwm_ca2, iwm_lstrb, iwm_enable, iwm_select, iwm_q6, iwm_q7);
            df->addLine("Disk Register: %02X  (EN35=%d SEL=%d)",
                disk_register, dr_enable35, dr_sel);
            df->addLine("Mode: %02X  ClkSpd: %d  BitCell: %d  MotorOff: %d  HSProtocol: %d Ltch: %d",
                reg_mode, mr_clockspeed, mr_bitcelltime, mr_motorofftimer, mr_hsprotocol, mr_latch);
            df->addLine("Handshake: %02X  Status: %02X", reg_handshake, read_status_register());
            df->addLine("Active: %s drive %d   LSS: %s",
                dr_enable35 ? "3.5" : "5.25", iwm_select,
                lss_active() ? "running" : "stopped");
            df->addLine("               5.25/0    5.25/1     3.5/0     3.5/1");
            df->addLine("  Enable   :   %-9d %-9d %-9d %-9d",
                (int)drives_525[0].get_enable(),
                (int)drives_525[1].get_enable(),
                (int)drives_35[0].get_enable(),
                (int)drives_35[1].get_enable());
            df->addLine("  Motor    :   %-9d %-9d %-9d %-9d",
                (int)drives_525[0].get_motor_on(),
                (int)drives_525[1].get_motor_on(),
                (int)drives_35[0].get_motor_on(),
                (int)drives_35[1].get_motor_on());
            df->addLine("  Selected:    %-9d %-9d %-9d %-9d",
                (int)(iwm_select == 0 && !dr_enable35),
                (int)(iwm_select == 1 && !dr_enable35),
                (int)(iwm_select == 0 && dr_enable35),
                (int)(iwm_select == 1 && dr_enable35));
            df->addLine("  Track:       %-9d %-9d %-9d %-9d",
                drives_525[0].get_track(),
                drives_525[1].get_track(),
                drives_35[0].get_track(),
                drives_35[1].get_track());
            df->addLine("  Side:        %-9s %-9s %-9d %-9d",
                "-", "-",
                drives_35[0].get_side(),
                drives_35[1].get_side());
            drives_525[0].debug(df);
            drives_525[1].debug(df);
            drives_35[0].debug(df);
            drives_35[1].debug(df);
        }

  
    bool diskii_running_last = false;
    int tracknumber_last = 0;
    void soundeffects_update() {
        // Shugart sound effects are 5.25-specific. When the IWM is in
        // 3.5 mode, skip queueing spindle / head sounds so we don't play
        // the wrong sound over a 3.5 disk access.
        if (dr_enable35) {
            if (diskii_running_last) {
                diskii_running_last = false;
                sound_effect->flush(SE_SHUGART_DRIVE);
            }
            return;
        }
        int tracknumber = drives_525[iwm_select].get_track();

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


    bool mount(storage_key_t key, media_descriptor *media) {
        if (key.slot == 6) {
            return drives_525[key.drive].mount(key, media);
        } else if (key.slot == 5) {
            return drives_35[key.drive].mount(key, media);
        } else {
            assert(false && "IWM: invalid key mount()");
            return false;
        }
    }
    bool unmount(storage_key_t key) {
        if (key.slot == 6) {
            return drives_525[key.drive].unmount(key);
        } else if (key.slot == 5) {
            return drives_35[key.drive].unmount(key);
        } else {
            assert(false && "IWM: invalid key unmount()");
            return false;
        }
    }
    bool writeback(storage_key_t key) {
        if (key.slot == 6) {
            return drives_525[key.drive].writeback();
        } else if (key.slot == 5) {
            return drives_35[key.drive].writeback();
        } else {
            assert(false && "IWM: invalid key writeback()");
            return false;
        }
    }
    drive_status_t status(storage_key_t key) {
        if (key.slot == 6) {
            return drives_525[key.drive].status();
        } else if (key.slot == 5) {
            return drives_35[key.drive].status();
        } else {
            assert(false && "IWM: invalid key status()");
            return {false, "", false, 0, false, false};
        }
    }

};



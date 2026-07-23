#pragma once

#include "NClock.hpp"
#include "computer.hpp"
#include "util/SoundEffect.hpp"

#include "devices/floppy/Floppy525_woz.hpp"
#include "devices/floppy/Floppy35_woz.hpp"

#include "util/SoundEffectKeys.hpp"
#include "debug.hpp"


static const uint32_t IWM_SWITCH_COUNT = 8;
static const uint32_t IWM_ADDRESS_MAX = IWM_SWITCH_COUNT * 2;

enum iwm_switch_t {
    IWM_CA0_OFF = 0,
    IWM_CA0_ON = 1,
    IWM_CA1_OFF = 2,
    IWM_CA1_ON = 3,
    IWM_CA2_OFF = 4,
    IWM_CA2_ON = 5,
    IWM_LSTRB_OFF = 6,
    IWM_LSTRB_ON = 7,
    IWM_ENABLE_OFF = 8,
    IWM_ENABLE_ON = 9,
    IWM_SELECT_OFF = 10,
    IWM_SELECT_ON = 11,
    IWM_Q6_OFF = 12,
    IWM_Q6_ON = 13,
    IWM_Q7_OFF = 14,
    IWM_Q7_ON = 15,
};

class IWM : public StorageDevice {
    protected:

        SoundEffect *sound_effect;
        SoundEffectContainer_t sounds[5];
        NClockII *clock;

        /* Floppy525_woz *drives_525[2];
        Floppy35_woz  *drives_35[2]; */
        Floppy_woz *drives[2][2] = { {nullptr, nullptr}, {nullptr, nullptr} };

        // ── /ENBL model ─────────────────────────────────────────────────────
        //
        // Sources (per the IWM Device Specification as transcribed in
        // Docs/IWM.md, and the 3.5-interface PAL equations there):
        //
        //   "The drive we 'select' is determined by: dr_enable35 and
        //    iwm_select and iwm_enable AND the 555 timer. This makes the
        //    motor_on signal."
        //
        // i.e. the IWM asserts exactly ONE hardware /ENBL output at a time,
        // derived combinationally from raw inputs:
        //
        //   /ENBL is asserted  <=>  iwm_enable == 1
        //                           OR the MC14536 motor-off timer is still
        //                           running (MODE.M == 0 arms a ~1 s holdover
        //                           started when iwm_enable transitions 1->0;
        //                           MODE.M == 1 means no holdover)
        //
        //   /ENBL is routed to      drives[dr_enable35][iwm_select]
        //                           (DISKREG bit 6 picks the family, the
        //                           SELECT softswitch picks the unit).
        //                           Every other drive sees /ENBL deasserted.
        //
        // What /ENBL *means* differs per family and is the drive's business,
        // not the IWM's: on a 5.25 Shugart /ENBL is also the spindle motor;
        // on a 3.5 Sony it is only "selected" (LED / disk locked) and the
        // spindle is a drive-internal latch commanded by CONT35 $4/$C (with
        // its own in-drive ~0.5 s spin-down, see Floppy35_woz).
        //
        // Representation: the raw inputs below are the ONLY stored state.
        //   - iwm_enable            (switches[4], written only by decode())
        //   - mark_cycles_turnoff   (14M deadline; nonzero = holdover armed)
        //   - dr_enable35 / iwm_select (routing)
        // Everything else is derived on demand:
        //   - enbl_asserted()       = the /ENBL line level (combinational)
        //   - sync_drive_enables()  = pushes the routing + line level into
        //     the four Floppy_woz::enable latches; called after ANY mutation
        //     of the raw inputs. Consumers (lss_active(), the status
        //     register's bit 5, the MODE-register write gate, sound effects)
        //     all read enbl_asserted() or the synced drive state — there is
        //     no separately-maintained "enable_asserted" latch to fall out
        //     of step. (A previous latch of that name desynced from the
        //     per-drive flags whenever SELECT/EN35 changed during a
        //     holdover window, which is exactly what hung the IIgs ROM03
        //     POST disk self-test — see system-settings-gs/docs/HARNESS.md.)
        uint64_t mark_cycles_turnoff = 0; // 14M deadline while the MC14536
                                          // /ENBL holdover runs; 0 = idle.
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
                uint8_t dr_hdsel : 1;
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
        // True only after a CPU data-register write in async mode. Without this,
        // reset leaves hr_register_ready=0 ("busy"), and the LSS treats that as
        // a queued byte — shifting stale async_buffer_register onto a mounted
        // track during the ROM03 write-handshake self-test and marking the
        // disk dirty even though firmware never wrote data.
        bool async_buffer_valid = false;

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

        // The /ENBL line level, derived combinationally from the raw inputs
        // (see the model comment at mark_cycles_turnoff above). This is the
        // ONLY definition of "enable is asserted" — every consumer calls it.
        bool enbl_asserted() {
            if (iwm_enable) return true;
            return mark_cycles_turnoff != 0 &&
                   clock->get_c14m() <= mark_cycles_turnoff;
        }

        // Push the /ENBL routing + level into the four drives' enable
        // latches: exactly the selected drive sees the line level, everyone
        // else sees deasserted. Idempotent (only edges are forwarded, so
        // Floppy35_woz's own motor-off scheduling still keys off real 1->0
        // transitions). Call after ANY mutation of iwm_enable /
        // mark_cycles_turnoff / dr_enable35 / iwm_select.
        void sync_drive_enables() {
            const bool level = enbl_asserted();
            for (int fam = 0; fam < 2; fam++) {
                for (int unit = 0; unit < 2; unit++) {
                    const bool want =
                        (fam == dr_enable35 && unit == iwm_select) ? level : false;
                    if (drives[fam][unit]->get_enable() != want) {
                        drives[fam][unit]->set_enable(want);
                    }
                }
            }
        }

        // ENABLE softswitch edge handling (decode() calls this after
        // updating switches[]): a 1->0 transition with MODE.M == 0 arms the
        // MC14536 ~1 s /ENBL holdover; any assertion cancels it. Repeated
        // accesses at the same level are not transitions and leave a
        // running holdover untouched.
        void enable_switch_changed(uint8_t prev_enable) {
            if (iwm_enable) {
                mark_cycles_turnoff = 0;  // asserted: cancel any holdover
            } else if (prev_enable && !mr_motorofftimer) {
                mark_cycles_turnoff =
                    clock->get_c14m() + clock->get_c14m_per_second();
            } else if (prev_enable) {
                mark_cycles_turnoff = 0;  // MODE.M=1: off immediately
            }
            sync_drive_enables();
        }

        // Unified "LSS should advance" predicate. For 5.25 the spindle IS
        // the /ENBL line; for 3.5 it's the currently-selected drive's
        // internal spindle latch (CONT35 $4/$C). Note the drive-level
        // fast_forward() additionally gates on its own (synced) enable, so
        // 3.5 bits flow only when selected AND spinning.
        bool lss_active() {
            if (dr_enable35) return drives[1][iwm_select]->get_motor_on();
            return enbl_asserted();
        }

    public:
        IWM(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer) : StorageDevice()/* ,
            drives_525{Floppy525_woz(sound_effect, clock, event_timer),
                       Floppy525_woz(sound_effect, clock, event_timer)},
            drives_35 {Floppy35_woz (sound_effect, clock, event_timer),
                       Floppy35_woz (sound_effect, clock, event_timer)} */
        {
            // Slot 6 is the IWM's 5.25" pair (matches Mounts registration).
            drives[0][0] = new Floppy525_woz(sound_effect, clock, event_timer, 6, 0);
            drives[0][1] = new Floppy525_woz(sound_effect, clock, event_timer, 6, 1);
            drives[1][0] = new Floppy35_woz(sound_effect, clock, event_timer,0 );
            drives[1][1] = new Floppy35_woz(sound_effect, clock, event_timer,1 );
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
            mark_cycles_turnoff = 0;
            reg_mode = 0;
            // Handshake bit7=1 means buffer empty / ready for CPU data.
            // Reset must start ready; ready=0 is only set when the CPU loads
            // the buffer (see write() below). Starting at 0 made the LSS
            // believe a byte was queued and emit write_pulse during POST.
            update_handshake_ready(true, true);
            internal_data_register = 0;
            async_shift_reg          = 0;
            async_bits_remaining     = 0;
            async_buffer_register    = 0;
            async_buffer_valid       = false;
            sequencer_state          = false;
            // iwm_enable == 0 and no holdover => /ENBL deasserted everywhere.
            sync_drive_enables();
        }

        // Frame-time tidy-up for the MC14536 /ENBL holdover: when the
        // deadline passes, clear it and re-sync the drive latches.
        // (enbl_asserted() already compares against the live clock, so the
        // status register / LSS see the drop immediately; this just brings
        // the per-drive enable flags along at frame granularity.)
        void check_motor_off_timer() {
            if (mark_cycles_turnoff != 0 && (clock->get_c14m() > mark_cycles_turnoff)) {
                mark_cycles_turnoff = 0;
                sync_drive_enables();
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
        // When EN35 changes the IWM physically reroutes /ENBL1//ENBL2 to the
        // newly-selected drive family — the previously-selected drive in the
        // old family loses /ENBL, and the new family's iwm_select drive
        // inherits whatever /ENBL state is currently asserted.
        void write_disk_register(uint8_t data) {
            const uint8_t prev_sel     = dr_hdsel;
            const uint8_t prev_en35    = dr_enable35;
            disk_register = data;

            if (dr_enable35 != prev_en35) {
                // EN35 is a routing input to the /ENBL model: re-syncing
                // moves the asserted /ENBL (if any) from the old family's
                // selected drive to the new family's. 3.5 spindle state is
                // internally latched in the Sony mechanism (CONT35 $4/$C),
                // so losing /ENBL doesn't hard-stop a spinning motor — the
                // drive's own set_enable(false) schedules its ~0.5 s
                // spin-down, same as an explicit SELECT change would.
                sync_drive_enables();
            }

            if (dr_hdsel != prev_sel || dr_enable35 != prev_en35) {
                // Push HDSEL into both 3.5 drives so whichever is selected
                // sees the right sense bit.
                drives[1][0]->set_hdsel(dr_hdsel);
                drives[1][1]->set_hdsel(dr_hdsel);
            }
        }

        uint8_t read_status_register() {
            // In 3.5 mode bit 7 is the selected drive's status-selector
            // output; in 5.25 mode it's the WP-sense line (already mirrored
            // into sense_input by the LSS's READLOAD path).
            /* uint8_t sense = dr_enable35 ? drives[dr_enable35][iwm_select]->read_sense()
                                        : drives[0][iwm_select]->read_sense(); */
            uint8_t sense = drives[dr_enable35][iwm_select]->read_sense();
            // Bit 5 is the IWM's own /ENBL output state (incl. the MC14536
            // holdover): the ROM's SELIWM loop polls it to know when the
            // MODE register becomes writable.
            return (reg_mode & 0b000'11111) | ((enbl_asserted() ? 1 : 0) << 5) | (sense << 7);
        }

        void fast_forward() {
            Floppy_woz *drive = drives[dr_enable35][iwm_select];
            if (!lss_active()) {
                // LSS state machine is stopped (5.25 ENABLE'-off / 3.5
                // motor-off). The drive itself still needs to tick: 3.5
                // status flags like disk_stepping / disk_ready are
                // time-based and only get re-evaluated inside the drive's
                // fast_forward() -> refresh_sense() -> update_timers()
                // chain. If we early-return without giving the drive a
                // chance to run that chain, firmware that strobes a
                // CONT35 step and then polls disk-is-stepping with the
                // motor off (legal on real Sony hardware) will spin
                // forever on the cached sense_out. Calling fast_forward()
                // is cheap when not spinning -- the base returns 0 bits
                // and the LSS bit loop is skipped below.
                drive->fast_forward();
                return;
            }

            // Dispatch the head-advance + LSS loop against whichever drive
            // family is currently selected by dr_enable35.
            /* if (dr_enable35) {
                uint64_t now = clock->get_vid_cycles();
                fast_forward_impl<Floppy35_woz>(now, drives[1][iwm_select]);
            } else {
                uint64_t now = clock->get_cycles();
                fast_forward_impl<Floppy525_woz>(now, drives[0][iwm_select]);
            } */
            fast_forward_impl(drive);
            //fast_forward_impl(/* dr_enable35 ? clock->get_vid_cycles() : clock->get_cycles(), */ drives[dr_enable35][iwm_select]);
        }

    private:
        // Shared LSS loop. Templated on the concrete drive type so the
        // Floppy_woz virtuals dispatch directly (read_pulse/write_pulse/
        // fast_forward/get_write_protect) without an extra indirection.
        //template <typename Drive>
        void fast_forward_impl(/* uint64_t now, */ Floppy_woz *drive) {
            uint64_t bits_to_sim = drive->fast_forward(/* now */);

            // L (latch) and H (handshake protocol) are independent,
            // firmware-settable bits of the IWM's MODE register ($C03F
            // write) — see Docs/IWM.md's transcription of the IWM Device
            // Specification: "Should be 0 for 5.25 and 1 for 3.5", i.e.
            // firmware chooses these per the drive it intends to talk to;
            // they are not physically wired to DISKREG's EN35 (drive
            // family select) bit. Previously both were gated by
            // `dr_enable35 &&`, so a self-test/probe that set MODE.H=1
            // (async/handshake) before flipping EN35 to 3.5 would never
            // see `reg_handshake` update — the IIgs ROM03 power-on disk
            // self-test does exactly this, and hangs forever polling a
            // handshake bit that can now never be set. Confirmed via
            // runtime trace (mr_hsprotocol=1, dr_enable35=0) while
            // gssquared spun on this loop under MCP control; see M0 boot
            // notes in system-settings-gs/docs/HARNESS.md.
            const bool latch_read = mr_latch;
            const bool async_wr   = mr_hsprotocol;

            for (uint64_t i = 0; i < bits_to_sim; i++) {
                if (iwm_q7 == 0 && iwm_q6 == 0) {
                    // READSHIFT (mirrors OpenEmulator's SEQUENCER_READSHIFT):
                    // while QA (bit 7) is 0, shift incoming RP bits left.
                    // 5.25 / sync: expose partial nibbles on data_register.
                    // 3.5 latch (L=1): accumulate in internal_data_register;
                    // CPU-visible data_register hides MSB until valid byte.
                    uint8_t bit = drive->read_pulse();
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
                    drive->read_pulse(); // advance but discard.
                    uint8_t wp = drive->read_sense(); // drive->get_write_protect() & 1;
                    if (latch_read) {
                        internal_data_register = static_cast<uint8_t>((internal_data_register >> 1) | (wp << 7));
                    } else {
                        data_register = static_cast<uint8_t>((data_register >> 1) | (wp << 7));
                    }
                    sequencer_state = false;
                } else if (iwm_q7 == 1) {
                    if (async_wr) {
                        if (async_bits_remaining == 0) {
                            if (hr_register_ready == 0 && async_buffer_valid) {
                                // CPU has queued a byte; transfer to shift reg.
                                async_shift_reg = async_buffer_register;
                                async_bits_remaining = 8;
                                async_buffer_valid = false;
                                update_handshake_ready(true, true);
                            } else if (hr_register_ready == 0) {
                                // Busy with no valid buffer (should not happen
                                // after reset init); do not mutate the surface.
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
                            drive->write_pulse(out_bit);
                            async_shift_reg = static_cast<uint8_t>(async_shift_reg << 1);
                            async_bits_remaining--;
                        } else {
                            // Waiting for next STA Q6+1 / underrun gap: keep
                            // media timing aligned without flipping bits.
                            // there's nothing left to write, so we emit 0's..
                            //drive->write_pulse(0);
                            drive->tick_no_write();
                        }
                    } else if (!dr_enable35) {
                        // 5.25 WRITESHIFT / WRITELOAD (sync): per OE both Q6
                        // sub-modes behave identically — shift QA out to disk,
                        // then shift data_register left.
                        uint8_t out_bit =
                            static_cast<uint8_t>((data_register >> 7) & 0x01);
                        drive->write_pulse(out_bit);
                        data_register =
                            static_cast<uint8_t>(data_register << 1);
                        sequencer_state = false;
                    } else {
                        // 3.5 drive with Q7=1 but mr_hsprotocol=0: the firmware
                        // has not set up async write mode (3.5 is async-only by
                        // spec). On real hardware /WRREQ would be high but the
                        // bits clocked through would be undefined; firmware
                        // never writes in this configuration. Treat it as an
                        // idle gap — keep angular timing aligned without
                        // mutating the surface. Without this guard, any stray
                        // Q7=1 window (IIgs ROM probes, GS/OS background
                        // checks, the very first fast_forward() inside a
                        // write() that turns Q7 off) shifts whatever happens
                        // to be in data_register — usually the last byte
                        // assembled by the latch reader (D5/AA/96/...) —
                        // straight onto the disk surface.
                        drive->tick_no_write();
                    }
                }
            }
        }

    public:

        inline void decode(uint16_t reg) {
            // Remember pre-write ENABLE/SELECT so transitions can be
            // detected after the switch-state update below.
            const uint8_t prev_select = iwm_select;
            const uint8_t prev_enable = iwm_enable;

            // Update the switch state.
            switches[reg>>1] = reg & 0x01;

            switch (reg) {
                case IWM_CA0_OFF:
                case IWM_CA0_ON:
                case IWM_CA1_OFF:
                case IWM_CA1_ON:
                case IWM_CA2_OFF:
                case IWM_CA2_ON:
                case IWM_LSTRB_OFF:
                case IWM_LSTRB_ON:
                    // CA0-CA2 and LSTRB go to the selected drive. The drive
                    // decodes them itself (5.25: stepper phases; 3.5: the
                    // 16-way status/control selector + strobe).
                    drives[dr_enable35][iwm_select]->set_phase(reg >> 1, reg & 0x01);
                    break;
                case IWM_ENABLE_ON:
                case IWM_ENABLE_OFF:
                    // /ENBL level input changed (or was re-written at the
                    // same level): update the holdover timer state and
                    // re-sync. What /ENBL means (5.25 spindle vs. 3.5
                    // LED/lock) is the drive's business.
                    enable_switch_changed(prev_enable);
                    break;
                case IWM_SELECT_ON:
                case IWM_SELECT_OFF:
                    // Routing input changed: /ENBL (if asserted) moves to
                    // the newly-selected unit; sync handles de-selecting
                    // the old one. HDSEL is re-pushed so the newly-selected
                    // 3.5 drive's sense output reflects the current SEL
                    // line.
                    if (iwm_select != prev_select) {
                        drives[1][0]->set_hdsel(dr_hdsel);
                        drives[1][1]->set_hdsel(dr_hdsel);
                        sync_drive_enables();
                    }
                    break;
                default:
                    break;
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
            if (!enbl_asserted() && address_odd(reg) && iwm_q6 && iwm_q7) {
                reg_mode = data;
            }
            /* Data-register write: Q6+Q7 on, odd address, motor running.
            5.25 sync: LSS consumes data_register each WRITESHIFT step.
            3.5 async (H=1): bytes queue into async_shift_reg; handshake
            clears until another byte is needed (Neil Parker write sequence). */
            else if (enbl_asserted() && address_odd(reg) && iwm_q6 && iwm_q7) {
                data_register = data;
                if (dr_enable35 && mr_hsprotocol) {
                    async_buffer_register = data;
                    async_buffer_valid = true;
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
                disk_register, dr_enable35, dr_hdsel);
            df->addLine("Mode: %02X  ClkSpd: %d  BitCell: %d  MotorOff: %d  HSProtocol: %d Ltch: %d",
                reg_mode, mr_clockspeed, mr_bitcelltime, mr_motorofftimer, mr_hsprotocol, mr_latch);
            df->addLine("Handshake: %02X  Status: %02X", reg_handshake, read_status_register());
            df->addLine("Active: %s drive %d   LSS: %s",
                dr_enable35 ? "3.5" : "5.25", iwm_select,
                lss_active() ? "running" : "stopped");
            df->addLine("               5.25/0    5.25/1     3.5/0     3.5/1");
            df->addLine("  Enable   :   %-9d %-9d %-9d %-9d",
                (int)drives[0][0]->get_enable(),
                (int)drives[0][1]->get_enable(),
                (int)drives[1][0]->get_enable(),
                (int)drives[1][1]->get_enable());
            df->addLine("  Motor    :   %-9d %-9d %-9d %-9d",
                (int)drives[0][0]->get_motor_on(),
                (int)drives[0][1]->get_motor_on(),
                (int)drives[1][0]->get_motor_on(),
                (int)drives[1][1]->get_motor_on());
            df->addLine("  Selected:    %-9d %-9d %-9d %-9d",
                (int)(iwm_select == 0 && !dr_enable35),
                (int)(iwm_select == 1 && !dr_enable35),
                (int)(iwm_select == 0 && dr_enable35),
                (int)(iwm_select == 1 && dr_enable35));
            df->addLine("  Track:       %-9d %-9d %-9d %-9d",
                drives[0][0]->get_track(),
                drives[0][1]->get_track(),
                drives[1][0]->get_track(),
                drives[1][1]->get_track());
            df->addLine("  Side:        %-9s %-9s %-9d %-9d",
                "-", "-",
                drives[1][0]->get_side(),
                drives[1][1]->get_side());
            drives[0][0]->debug(df);
            drives[0][1]->debug(df);
            drives[1][0]->debug(df);
            drives[1][1]->debug(df);
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
        int tracknumber = drives[0][iwm_select]->get_track();

        //printf("diskii_running: %d, tracknumber: %d / %d\n", diskii_running, tracknumber, tracknumber_last);
    
        /* If less than a full copy of the audio is queued for playback, put another copy in there.
            This is overkill, but easy when lots of RAM is cheap. One could be more careful and
            queue less at a time, as long as the stream doesn't run dry.  */
    
        /* If sound state changed, reset the stream */
        if (diskii_running_last && !enbl_asserted()) {
            diskii_running_last = false;
            /* Clear the audio stream when transitioning to disabled state */
            sound_effect->flush(SE_SHUGART_DRIVE);
        }

        /* Only queue audio data if sound is enabled */
        //static int running_chunknumber = 0;
        if (enbl_asserted()) {
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

    bool get_motor() { return enbl_asserted(); }


    bool mount(storage_key_t key, std::vector<media_descriptor *> media_list) {
        if (media_list.size() > 1) {
            return false;;
        }
        if (key.slot == 6) {
            return drives[0][key.drive]->mount(key, media_list[0]);
        } else if (key.slot == 5) {
            return drives[1][key.drive]->mount(key, media_list[0]);
        } else {
            assert(false && "IWM: invalid key mount()");
            return false;
        }
    }
    bool unmount(storage_key_t key) {
        if (key.slot == 6) {
            return drives[0][key.drive]->unmount(key);
        } else if (key.slot == 5) {
            return drives[1][key.drive]->unmount(key);
        } else {
            assert(false && "IWM: invalid key unmount()");
            return false;
        }
    }
    bool writeback(storage_key_t key) {
        if (key.slot == 6) {
            return drives[0][key.drive]->writeback();
        } else if (key.slot == 5) {
            return drives[1][key.drive]->writeback();
        } else {
            assert(false && "IWM: invalid key writeback()");
            return false;
        }
    }
    drive_status_t status(storage_key_t key) {
        if (key.slot == 6) {
            return drives[0][key.drive]->status();
        } else if (key.slot == 5) {
            return drives[1][key.drive]->status();
        } else {
            assert(false && "IWM: invalid key status()");
            return {false, "", false, 0, false, false};
        }
    }

};



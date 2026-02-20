#pragma once

#include <cstdint>
#include <cassert>
#include "util/DebugFormatter.hpp"
#include "util/InterruptController.hpp"
#include "util/EventTimer.hpp"
#include "NClock.hpp"
#include "serial_devices/SerialDevice.hpp"

constexpr bool SCDEBUG = 0;
 
/**
 * this code is LITTLE-ENDIAN SPECIFIC.
 * IF you have a big endian machine, you will need to swap the bytes. Or cry.
 */

/* Timing Clock fed into SCC by the IIgs */
constexpr uint64_t SCC_RX_CLOCK = 3'686'400;

/* Emulator master clock (14M clock) */
constexpr uint64_t MASTER_CLOCK = 14'318'180;

/* Maximum baud rate to emulate timing for */
constexpr float MAX_TIMED_BAUD = 115'200.0f;

enum scc_channel_t {
    SCC_CHANNEL_A,
    SCC_CHANNEL_B,
    SCC_CHANNEL_COUNT,
};

enum scc_register_t {
    WR0 = 0,
    WR1 = 1,
    WR2 = 2,
    WR3 = 3,
    WR4 = 4,
    WR5 = 5,
    WR6 = 6,
    WR7 = 7,
    WR8 = 8,
    WR9 = 9,
    WR10 = 10,
    WR11 = 11,
    WR12 = 12,
    WR13 = 13,
    WR14 = 14,
    WR15 = 15,
    RR0 = 0,
    RR1 = 1,
    RR2 = 2,
    RR3 = 3,
    RR4 = 4,
    RR5 = 5,
    RR6 = 6,
    RR7 = 7,
    RR8 = 8,
    RR9 = 9,
    RR10 = 10,
    RR11 = 11,
    RR12 = 12,
    RR13 = 13,
    RR14 = 14,
    RR15 = 15,
};

class Z85C30 {
    InterruptController *irq_control = nullptr;
    EventTimer *event_timer = nullptr;
    NClockII *clock = nullptr;

    /* FILE *data_files[SCC_CHANNEL_COUNT] = { NULL, NULL }; */

    inline char ch_name(scc_channel_t channel) {
        return (uint8_t) channel + 'A';
    }
    inline char ch_name(int channel) {
        return (char)('A' + channel);
    }
    
    SerialDevice *serial_devices[SCC_CHANNEL_COUNT] = { nullptr, nullptr };

    struct scc_channel_state_t {
        uint8_t char_rx;
        uint8_t char_tx;
        bool tx_in_progress = false;  // TX character being transmitted
        bool rx_in_progress = false;  // RX character being received
        union {
            uint8_t r_reg_0;
            struct {
                uint8_t r0_rx_char_available: 1;
                uint8_t r0_zero_count: 1;               
                uint8_t r0_tx_buffer_empty: 1;
                uint8_t r0_dcd: 1;
                uint8_t r0_sync_hunt: 1;
                uint8_t r0_cts: 1;
                uint8_t r0_tx_underrun_eom: 1;
                uint8_t r0_break_abort: 1;
            };
        };
        union {
            uint8_t r_reg_1;
            struct {
                uint8_t r1_all_sent: 1;
                uint8_t r1_residue_code: 3;               
                uint8_t r1_parity_err: 1;
                uint8_t r1_rx_overrun_err: 1;
                uint8_t r1_crc_framing_err: 1;
                uint8_t r1_end_of_frame: 1;
            };
        };
        //uint8_t r_reg_2;
        union {
            uint8_t r_reg_3;
            struct {
                uint8_t r3_b_ext_pending: 1;
                uint8_t r3_b_tx_pending: 1;
                uint8_t r3_b_rx_pending: 1;
                uint8_t r3_a_ext_pending: 1;
                uint8_t r3_a_tx_pending: 1;
                uint8_t r3_a_rx_pending: 1;
                uint8_t r3_unused: 2;
            };
        };
        union {
            uint8_t r_reg_10;
            struct {
                uint8_t r10_un1: 1;
                uint8_t r10_on_loop: 1;
                uint8_t r10_un2: 2;
                uint8_t r10_loop_sending: 1;
                uint8_t r10_un3: 1;
                uint8_t r10_two_clock_missing: 1;
                uint8_t r10_one_clock_missing: 1;
            };
        };
        // RR15 is the same as WR15 with a mask of 0b1111'1010;

        bool tx_irq_condition = false;
        union {
            uint8_t w_regs[16];
            struct {
                /* Reg 0 */
                uint8_t r0_reg_num: 3;
                uint8_t r0_cmd: 3;
                uint8_t r0_reset_cmd: 2;
                
                /* Reg 1 */
                uint8_t r1_ext_int_enable: 1;
                uint8_t r1_tx_int_enable: 1;
                uint8_t r1_partity_spcond: 1;
                uint8_t r1_rx_int_mode: 2;
                uint8_t r1_dma_req_on_rx: 1;
                uint8_t r1_dma_req_func: 1;
                uint8_t r1_dma_req: 1;

                uint8_t r2_int_vector: 8;

                /* Reg 3 */
                uint8_t r3_rx_enable: 1;
                uint8_t r3_sync_char_load_inhibit: 1;
                uint8_t r3_address_search_mode: 1;
                uint8_t r3_rx_crc_enable: 1;
                uint8_t r3_hunt_mode: 1;
                uint8_t r3_auto_enables: 1;
                uint8_t r3_rx_bits_per_char: 2;
               
                /* Reg 4 */
                uint8_t r4_parity_enable: 1;
                uint8_t r4_parity_even_odd: 1;
                uint8_t r4_stop_bits: 2;
                uint8_t r4_sync_char_mode: 2;
                uint8_t r4_clock_mode: 2;

                /* Reg 5 */
                uint8_t r5_tx_crc_enable: 1;
                uint8_t r5_rts: 1;
                uint8_t r5_sdlc_crc16: 1;
                uint8_t r5_tx_enable: 1;
                uint8_t r5_send_break: 1;
                uint8_t r5_tx_bits_per_char: 2;
                uint8_t r5_dtr: 1;

                /* Reg 6 */
                uint8_t r6: 8;

                /* Reg 7 */
                uint8_t r7_auto_tx_flag: 1;
                uint8_t r7_auto_eom_reset: 1;
                uint8_t r7_auto_rts_deactivate: 1;
                uint8_t r7_force_txd_high: 1;
                uint8_t r7_dtr_fast_mode: 1;
                uint8_t r7_complete_crc: 1;
                uint8_t r7_extended_read_enable: 1;
                uint8_t r7_reserved: 1;

                /* Reg 8 */
                uint8_t r8: 8;

                /* Reg 9 */
                uint8_t r9_vis: 1;
                uint8_t r9_nv: 1;
                uint8_t r9_dlc: 1;
                uint8_t r9_mie: 1;
                uint8_t r9_status_high_low: 1;
                uint8_t r9_software_intack_enable: 1;
                uint8_t r9_reset: 2;

                /* Reg 10 */
                uint8_t r10_6bit_8bit_sync: 1;
                uint8_t r10_loop_mode: 1;
                uint8_t r10_abort_underrun: 1;
                uint8_t r10_mark_flag_idle: 1;
                uint8_t r10_active_on_poll: 1;
                uint8_t r10_encoding: 2;
                uint8_t r10_crc_preset: 1;

                /* Reg 11 */
                uint8_t r11_txrc_out: 2;
                uint8_t r11_txrc_01: 1;
                uint8_t r11_tx_clock: 2;
                uint8_t r11_rx_clock: 2;
                uint8_t r11_txc_no_xtal: 1;

                /* Reg 12 - 13 */
                uint16_t r12_time_constant: 16;

                /* Reg 14 */
                uint8_t r14_br_gen_enable: 1;
                uint8_t r14_br_gen_source: 1;
                uint8_t r14_dtr_request_func: 1;
                uint8_t r14_auto_echo: 1;
                uint8_t r14_local_loopback: 1;
                uint8_t r14_command: 3;

                /* Reg 15 */
                uint8_t r15_unused: 1;
                uint8_t r15_zero_count_ie: 1;
                uint8_t r15_sdlc_fifo_enable: 1;
                uint8_t r15_dcd_ie: 1;
                uint8_t r15_sync_hunt_ie: 1;
                uint8_t r15_cts_ie: 1;
                uint8_t r15_tx_underrun_eom_ie: 1;
                uint8_t r15_break_abort_ie: 1;
            };
        };
    };

    protected:
        scc_channel_state_t registers[SCC_CHANNEL_COUNT];
        uint16_t reg_select[SCC_CHANNEL_COUNT];
        float baud_rate[SCC_CHANNEL_COUNT];
        uint32_t clock_mode[SCC_CHANNEL_COUNT];

        // Event timer instance IDs (must be unique across the system)
        uint64_t tx_timer_id[SCC_CHANNEL_COUNT];
        uint64_t rx_timer_id[SCC_CHANNEL_COUNT];

        const uint16_t clock_mode_table[4] = { 1, 16, 32, 64 };

        inline uint8_t get_bits_per_char(scc_channel_t channel, bool is_tx) {
            // Start bit (always 1)
            uint8_t bits = 1;
            
            // Data bits (5, 6, 7, or 8)
            uint8_t bits_code = is_tx ? registers[channel].r5_tx_bits_per_char : registers[channel].r3_rx_bits_per_char;
            switch (bits_code) {
                case 0b00: bits += 5; break;
                case 0b01: bits += 7; break;
                case 0b10: bits += 6; break;
                case 0b11: bits += 8; break;
            }
            
            // Parity bit (if enabled)
            if (registers[channel].r4_parity_enable) {
                bits += 1;
            }
            
            // Stop bits (1, 1.5, or 2)
            switch (registers[channel].r4_stop_bits) {
                case 0b00: bits += 1; break;      // 1 stop bit
                case 0b01: bits += 1; break;      // 1 stop bit (sync mode, but treat as async)
                case 0b10: bits += 2; break;      // 1.5 stop bits (round up to 2)
                case 0b11: bits += 2; break;      // 2 stop bits
            }
            
            return bits;
        }

        inline uint64_t get_cycles_per_char(scc_channel_t channel, bool is_tx) {
            if (baud_rate[channel] <= 0.0f || baud_rate[channel] > MAX_TIMED_BAUD) {
                return 0; // No timing for invalid or very high baud rates
            }
            
            uint8_t bits = get_bits_per_char(channel, is_tx);
            float chars_per_second = baud_rate[channel] / (float)bits;
            uint64_t cycles = (uint64_t)(MASTER_CLOCK / chars_per_second);
            
            return cycles;
        }

        inline void update_timing_sources(scc_channel_t channel) {
            uint16_t time_constant = registers[channel].r12_time_constant;
            uint16_t clock_mode_index = registers[channel].r4_clock_mode;
            uint16_t new_mode = clock_mode_table[clock_mode_index];
            clock_mode[channel] = new_mode;
            float baud_rate = (float)SCC_RX_CLOCK / (2.0f * (float)new_mode * ((float)time_constant + 2.0f));
            this->baud_rate[channel] = baud_rate;
            if (SCDEBUG) printf("SCC: Ch %d: Clock Mode: %d, Time Constant: %d, Baud Rate: %08.2f\n", channel, new_mode, time_constant, baud_rate);
        }

        inline bool update_tx_ip(scc_channel_t channel) {
            bool ip = false;

            if (registers[channel].r1_tx_int_enable && registers[channel].tx_irq_condition) {
                registers[channel].tx_irq_condition = false; // we took account of this, so reset it
                ip = true;
            }
            if (channel == SCC_CHANNEL_A) {
                registers[SCC_CHANNEL_A].r3_a_tx_pending = ip;
            } else {
                registers[SCC_CHANNEL_A].r3_b_tx_pending = ip;
            }
            return ip;
        }

        inline bool update_rx_ip(scc_channel_t channel) {
            bool ip = false;

            if ((registers[channel].r1_rx_int_mode == 0b10) && registers[channel].r0_rx_char_available) {
                ip = true;
            }
            if (channel == SCC_CHANNEL_A) {
                registers[SCC_CHANNEL_A].r3_a_rx_pending = ip;
            } else {
                registers[SCC_CHANNEL_A].r3_b_rx_pending = ip;
            }
            return ip;
        }

        inline uint8_t calculate_modified_vector(scc_channel_t channel) {
            uint8_t vector = registers[channel].w_regs[WR2];
            uint8_t vectormod = 0;
            // add A spcond
            if (registers[SCC_CHANNEL_A].r3_a_rx_pending) vectormod = 0b110;
            else if (registers[SCC_CHANNEL_A].r3_a_ext_pending) vectormod = 0b101;
            else if (registers[SCC_CHANNEL_A].r3_a_tx_pending) vectormod = 0b100;
            // add B spcond
            else if (registers[SCC_CHANNEL_B].r3_b_rx_pending) vectormod = 0b010;
            else if (registers[SCC_CHANNEL_B].r3_b_ext_pending) vectormod = 0b001;
            else if (registers[SCC_CHANNEL_B].r3_b_tx_pending) vectormod = 0b000;

            // VIS WR9[0] only affects vector placed on bus.
            // NV WR9[1] only affects vector placed on bus.

            if (registers[SCC_CHANNEL_A].r9_status_high_low) {
                // reverse vectormod bits.
                // then stuff into bits 6-4
                vectormod = ((vectormod & 0b001) << 2) | (vectormod & 0b010) | ((vectormod & 0b100) >> 2);
                vector = (vector & 0b1000'1111) | (vectormod << 4);
            } else {
                // stuff into bits 3-1
                vector = (vector & 0b0000'1110) | (vectormod << 1);
            }
            return vector;
        }

        inline void update_interrupts(scc_channel_t channel) {
            bool interrupt_pending = false;
            
            if (update_tx_ip(channel)) interrupt_pending = true;
            if (update_rx_ip(channel)) interrupt_pending = true;

            // if MIE reset, no interrupt asserted.
            if (registers[SCC_CHANNEL_A].r9_mie == 0) interrupt_pending = false;
            
            irq_control->set_irq(IRQ_ID_SCC, interrupt_pending);
        }

        inline void print_read_register(scc_channel_t channel, uint8_t reg_num, uint8_t retval) {
            if (SCDEBUG) printf("SCC: READ  %c :: %2d <== %02X\n", ch_name(channel), reg_num, retval);
        }

        inline void print_write_register(scc_channel_t channel, uint8_t reg_num, uint8_t data) {
            if (SCDEBUG) printf("SCC: WRITE %c :: %2d ==> %02X\n", ch_name(channel), reg_num, data);
        }


        /* Writes */

        inline void write_register_0(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR0, data);
            {
                registers[channel].w_regs[WR0] = data;
                uint16_t new_register = registers[channel].r0_reg_num;
                // TODO: need to process the various commands here. (resetting interrupts, etc.)
                if (registers[channel].r0_cmd == 0b001) new_register += 8;
                else if (registers[channel].r0_cmd == 0b010) { // reset ext/status interrupt 
                    registers[channel].r3_a_ext_pending = 0;
                } else if (registers[channel].r0_cmd == 0b101) { // reset Tx int pending
                    if (channel == SCC_CHANNEL_A) {
                        registers[SCC_CHANNEL_A].r3_a_tx_pending = 0;
                    } else {
                        registers[SCC_CHANNEL_A].r3_b_tx_pending = 0;
                    }   
                } else if (registers[channel].r0_cmd != 0) {
                    if (SCDEBUG) printf("SCC: Unimplemented WRITE %c r0_cmd: Reg %d = %02X\n", ch_name(channel), new_register, registers[channel].r0_cmd);
                }

                if (registers[channel].r0_reset_cmd == 0b01) { // reset Rx CRC Checker
                    // this is for SDLC etc mode
                } else if (registers[channel].r0_reset_cmd == 0b10) { // reset Tx CRC Generator
                    // this is for SDLC etc mode
                } else if (registers[channel].r0_reset_cmd == 0b11) { // reset Tx Underrun/EOM Latch
                    registers[channel].r0_tx_underrun_eom = 0; // ??? and we never set it anywhere. that's fine.
                }
                
                /* else {
                    printf("SCC: Unimplemented WRITE %c r0_reset_cmd command: Reg %d = %02X\n", ch_name(channel), new_register, registers[channel].r0_reset_cmd);
                } */
                reg_select[channel] = new_register;
                if (SCDEBUG) printf("SCC: Register Select %c -> New Reg %d\n", ch_name(channel), new_register);
            }
            update_interrupts(channel); // might have changed interrupt status
        }

        inline void write_register_1(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR1, data);
            registers[channel].w_regs[WR1] = data;
            update_interrupts(channel); // might have changed interrupt enables
        }

        // partially shared between channels. Write: identical. Read: different
        inline void write_register_2(uint8_t data) {
            print_write_register(SCC_CHANNEL_A, WR2, data); // we store this in Ch A
            registers[SCC_CHANNEL_A].w_regs[WR2] = data;
        }

        inline void write_register_3(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR3, data);
            registers[channel].w_regs[WR3] = data;
        }

        inline void write_register_4(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR4, data);
            registers[channel].w_regs[WR4] = data;
            update_timing_sources(channel);
        }

        inline void write_register_5(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR5, data);
            registers[channel].w_regs[WR5] = data;
        }

        inline void write_register_6(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR6, data);
            registers[channel].w_regs[WR6] = data;
        }

        inline void write_register_7(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR7, data);
            registers[channel].w_regs[WR7] = data;
        }

        inline void write_register_9(uint8_t data) {
            print_write_register(SCC_CHANNEL_A, WR9, data);
            
            // store new value first. "bits in wr9 may be written at same time as the reset command
            // because these bits are affected only by a hardware reset. (pg 3-8)"
            registers[SCC_CHANNEL_A].w_regs[WR9] = data;
            uint8_t reset_cmd = registers[SCC_CHANNEL_A].r9_reset;
            if (reset_cmd == 0x01) { // Channel Reset B
                soft_reset_channel(SCC_CHANNEL_B); // software reset for Ch B
            }
            if (reset_cmd == 0x02) { // Channel Reset A
                soft_reset_channel(SCC_CHANNEL_A);
            }
            if (reset_cmd == 0x03) { // Force HW Reset - same as external chip reset (pg 3-8)
                reset();
            }
        }

        inline void write_register_10(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR10, data);
            registers[channel].w_regs[WR10] = data;
        }

        inline void write_register_11(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR11, data);
            registers[channel].w_regs[WR11] = data;
            update_timing_sources(channel);
        }
        inline void write_register_12(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR12, data);
            registers[channel].w_regs[WR12] = data;
            update_timing_sources(channel);
        }
        inline void write_register_13(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR13, data);
            registers[channel].w_regs[WR13] = data;
            update_timing_sources(channel);
        }
        inline void write_register_14(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR14, data);
            registers[channel].w_regs[WR14] = data;
        }
        inline void write_register_15(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR15, data);
            registers[channel].w_regs[WR15] = data;
        }

        /* Reads */
        inline uint8_t read_register_0(scc_channel_t channel) {
            //printf("SCC: READ  register 0: Ch %d\n", channel);
            //registers[channel].r0_tx_buffer_empty = 1; // duh, don't override this.
            registers[channel].r0_dcd = 1; // these belong in update_queues()
            registers[channel].r0_cts = 1;
            uint8_t retval = registers[channel].r_reg_0;
            print_read_register(channel, RR0, retval);
            return retval; // TODO: fix. for now fake CTS=1, DCD=1, Tx Buffer Empty=1
        }

        inline uint8_t read_register_1(scc_channel_t channel) {
            registers[channel].r1_all_sent = registers[channel].r0_tx_buffer_empty; // this is basically always the case since our transmits are instant
            uint8_t retval = registers[channel].r_reg_1;
            print_read_register(channel, RR1, retval);
            return retval; // TODO: this is reasonable, we won't have errors.. what is residue code?
        }
        inline uint8_t read_register_2(scc_channel_t channel) {
            // Channel A: return the interrupt vector exactly as-is.
            // Channel B: return the "modified interrupt vector". 
            uint8_t retval;
            if (channel == SCC_CHANNEL_B) {
                 // when vector is read from Channel B, it always includes the status regardless of the VIS bit
                 // the status given will decode the highest priority interrupt pending at the time it is read.
                retval = calculate_modified_vector(channel);
            } else {
                retval = registers[SCC_CHANNEL_A].w_regs[WR2];
            }
            print_read_register(channel, RR2, retval);
            return retval;
        }
        inline uint8_t read_register_3(scc_channel_t channel) {
            uint8_t retval = 0x00;
            if (channel == SCC_CHANNEL_A) {
                retval = registers[SCC_CHANNEL_A].r_reg_3; 
            }
            print_read_register(channel, RR3, retval);
            return retval; // B is always 0
        }
        inline uint8_t read_register_10(scc_channel_t channel) {
            uint8_t retval = registers[channel].r_reg_10; // TODO: need to return the actual miscellaneous status;
            print_read_register(channel, RR10, retval);
            return retval;
        }
        inline uint8_t read_register_12(scc_channel_t channel) {
            uint8_t retval = registers[channel].w_regs[RR12];
            print_read_register(channel, RR12, retval);
            return retval;
        }
        inline uint8_t read_register_13(scc_channel_t channel) {
            uint8_t retval = registers[channel].w_regs[RR13];
            print_read_register(channel, RR13, retval);
            return retval;
        }
        inline uint8_t read_register_15(scc_channel_t channel) {
            uint8_t retval = registers[channel].w_regs[RR15] & 0b1111'1010; // two bits always 0
            print_read_register(channel, RR15, retval);
            return retval;
        }

    public:
        Z85C30(InterruptController *irq_control, EventTimer *event_timer = nullptr, NClockII *clock = nullptr, uint64_t base_instance_id = 0x5CC0000) 
            : irq_control(irq_control), event_timer(event_timer), clock(clock) {
            // Set up unique instance IDs for each channel's TX and RX timers
            tx_timer_id[SCC_CHANNEL_A] = base_instance_id + 0;
            tx_timer_id[SCC_CHANNEL_B] = base_instance_id + 1;
            rx_timer_id[SCC_CHANNEL_A] = base_instance_id + 2;
            rx_timer_id[SCC_CHANNEL_B] = base_instance_id + 3;
            
            // clear the data structures on boot.
            memset(reg_select, 0, sizeof(reg_select));
            memset(registers, 0, sizeof(registers));
            reset();
        };

        ~Z85C30() {}

       /*  void set_data_file(scc_channel_t channel, FILE *data_file) {
            this->data_files[channel] = data_file;
        } */

       
        inline void set_bits_by_mask(uint8_t &reg, uint8_t mask, uint8_t value) {
            reg = (reg & ~mask) | (value & mask);
        }

        /* HW Reset according to Technical Manual 
           use full set_bits_by_mask even if we're setting all bits, for consistency. The optimizer will probably optimize it out.
        */
        void hw_reset_channel(scc_channel_t channel) {
            // Cancel any pending TX/RX events
            if (event_timer) {
                event_timer->cancelEvents(tx_timer_id[channel]);
                event_timer->cancelEvents(rx_timer_id[channel]);
            }
            
            registers[channel].tx_in_progress = false;
            registers[channel].rx_in_progress = false;
            registers[channel].tx_irq_condition = false;
            set_bits_by_mask(registers[channel].w_regs[WR0], 0b1111'1111, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR1], 0b1101'1011, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR3], 0b0000'0001, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR4], 0b0000'0100, 0b000'0100);
            set_bits_by_mask(registers[channel].w_regs[WR5], 0b1001'1110, 0b0000'0000);
            // only one WR9 exists in the SCC and it can be accessed from either channel. (pg 7-10)
            set_bits_by_mask(registers[SCC_CHANNEL_A].w_regs[WR9], 0b1111'1100, 0b1100'0000);
            set_bits_by_mask(registers[channel].w_regs[WR10], 0b1111'1111, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR11], 0b1111'1111, 0b0000'1000);
            set_bits_by_mask(registers[channel].w_regs[WR14], 0b0011'1111, 0b0010'0000);
            set_bits_by_mask(registers[channel].w_regs[WR15], 0b1111'1111, 0b1111'1000);
            set_bits_by_mask(registers[channel].r_reg_0, 0b1100'0111, 0b0100'0100);
            set_bits_by_mask(registers[channel].r_reg_1, 0b1111'1111, 0b0000'0110);
            set_bits_by_mask(registers[channel].r_reg_3, 0b1111'1111, 0b0000'0000);
            set_bits_by_mask(registers[channel].r_reg_10, 0b1111'1111, 0b0000'0000);

            update_timing_sources(channel);
            update_interrupts(channel);
            // need to update interrupts
        }

        /* Channel Reset according to Technical Manual
            it's identical with 3 differences */
        void soft_reset_channel(scc_channel_t channel) {
            // Cancel any pending TX/RX events
            if (event_timer) {
                event_timer->cancelEvents(tx_timer_id[channel]);
                event_timer->cancelEvents(rx_timer_id[channel]);
            }
            
            registers[channel].tx_in_progress = false;
            registers[channel].rx_in_progress = false;
            registers[channel].tx_irq_condition = false;
            set_bits_by_mask(registers[channel].w_regs[WR0], 0b1111'1111, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR1], 0b1101'1011, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR3], 0b0000'0001, 0b0000'0000);
            set_bits_by_mask(registers[channel].w_regs[WR4], 0b0000'0100, 0b000'0100);
            set_bits_by_mask(registers[channel].w_regs[WR5], 0b1001'1110, 0b0000'0000);
            // only one WR9 exists in the SCC and it can be accessed from either channel. (pg 7-10)
            set_bits_by_mask(registers[SCC_CHANNEL_A].w_regs[WR9], 0b00100000, 0b0000'0000); // different than hw reset
            set_bits_by_mask(registers[channel].w_regs[WR10], 0b1001'1111, 0b0000'0000); // different than hw reset
            set_bits_by_mask(registers[channel].w_regs[WR11], 0b1111'1111, 0b0000'1000);
            set_bits_by_mask(registers[channel].w_regs[WR14], 0b0011'1100, 0b0010'0000); // different than hw reset
            set_bits_by_mask(registers[channel].w_regs[WR15], 0b1111'1111, 0b1111'1000);
            set_bits_by_mask(registers[channel].r_reg_0, 0b1100'0111, 0b0100'0100);
            set_bits_by_mask(registers[channel].r_reg_1, 0b1111'1111, 0b0000'0110);
            set_bits_by_mask(registers[channel].r_reg_3, 0b1111'1111, 0b0000'0000);
            set_bits_by_mask(registers[channel].r_reg_10, 0b1111'1111, 0b0000'0000);

            update_timing_sources(channel);
            update_interrupts(channel);
            // need to update interrupts
        }


        void reset() {
            // the channel reset and HW reset are not identical.
            hw_reset_channel(SCC_CHANNEL_A);
            hw_reset_channel(SCC_CHANNEL_B);
        }
        
        void writeCmd(scc_channel_t channel, uint8_t data) {
            // current reg_select for channel
            update_queues();

            uint16_t current_reg_select = reg_select[channel];
            switch (current_reg_select) {
                case WR0: write_register_0(channel, data); break;
                case WR1: write_register_1(channel, data);  reg_select[channel] = 0; break;
                case WR2: write_register_2(data);  reg_select[channel] = 0; break;
                case WR3: write_register_3(channel, data);  reg_select[channel] = 0; break;
                case WR4: write_register_4(channel, data);  reg_select[channel] = 0; break;
                case WR5: write_register_5(channel, data);  reg_select[channel] = 0; break;
                case WR6: write_register_6(channel, data);  reg_select[channel] = 0; break;
                case WR7: write_register_7(channel, data);  reg_select[channel] = 0; break;
                case WR8: writeData(channel, data);  reg_select[channel] = 0; break;
                case WR9: write_register_9(data);  reg_select[channel] = 0; break;
                case WR10: write_register_10(channel, data);  reg_select[channel] = 0; break;
                case WR11: write_register_11(channel, data);  reg_select[channel] = 0; break;
                case WR12: write_register_12(channel, data);  reg_select[channel] = 0; break;
                case WR13: write_register_13(channel, data);  reg_select[channel] = 0; break;
                case WR14: write_register_14(channel, data);  reg_select[channel] = 0; break;
                case WR15: write_register_15(channel, data);  reg_select[channel] = 0; break;
                default:
                    if (SCDEBUG) printf("SCC: Unimplemented WRITE %c register: Reg %d = %02X\n", ch_name(channel), current_reg_select, data);
                    reg_select[channel] = 0;
                    break;
            }
            // redundant: printf("SCC: WRITE register: Ch %d / Reg %d = %02X\n", channel, current_reg_select, data);
             // after any other write, reset the register select
        }

        /* 
            if write is in progress and they wrote anyway, what do we do?
            if write not in progress, 
                go ahead and chuck the character.
                schedule a timer based on the baud rate and the number of bits - char bits + start bits + stop bits.
                set a flag to indicate that write is in progress.
                when the timer is called, clear that flag.
        */
        void writeData(scc_channel_t channel, uint8_t data) {
            if (SCDEBUG) printf("SCC: WRITE %c DATA = %02X (tx_enable=%d)\n", ch_name(channel), data, registers[channel].r5_tx_enable);

            update_queues();

            // Check if TX is enabled
            if (!registers[channel].r5_tx_enable) {
                if (SCDEBUG) printf("SCC: Ch %c TX disabled, dropping char %02X\n", ch_name(channel), data);
                return;
            }

            // Store the byte to transmit
            registers[channel].char_tx = data;

            // If TX buffer was empty, accept the character and mark buffer full
            if (registers[channel].r0_tx_buffer_empty) {
                registers[channel].r0_tx_buffer_empty = false;
                registers[channel].tx_in_progress = true;
                
                // Calculate timing and schedule TX completion
                uint64_t cycles_per_char = get_cycles_per_char(channel, true);
                
                if (cycles_per_char > 0 && event_timer && clock) {
                    // Schedule the TX completion event
                    uint64_t trigger_cycle = clock->get_c14m() + cycles_per_char;
                    event_timer->scheduleEvent(trigger_cycle, tx_complete_callback, tx_timer_id[channel], this);
                    
                    if (SCDEBUG) printf("SCC: Ch %c TX scheduled for %llu cycles (baud: %.2f)\n", 
                        ch_name(channel), cycles_per_char, baud_rate[channel]);
                } else {
                    // High baud rate or no timer - complete immediately
                    tx_complete(channel);
                }
            } else {
                // TX buffer full - this is an overrun error in real hardware
                // For now, just drop the character
                if (SCDEBUG) printf("SCC: Ch %c TX overrun - buffer not empty\n", ch_name(channel));
            }

            update_interrupts(channel);
        }

        uint8_t readCmd(scc_channel_t channel) {
            
            update_queues();

            uint8_t retval = 0x00;
            uint16_t current_reg_select = reg_select[channel];
            switch (current_reg_select) {
                case RR0: case RR4: retval = read_register_0(channel); break;
                case RR1: case RR5: retval = read_register_1(channel); break;
                case RR2: case RR6: retval = read_register_2(channel); break;
                case RR3: case RR7: retval = read_register_3(channel);  break;
                case RR8: retval = readData(channel); break;
                case RR10: case RR14:  retval = read_register_10(channel); break;
                case RR12: retval = read_register_12(channel); break;
                case RR9: case RR13: retval = read_register_13(channel); break;
                case RR11: case RR15: retval = read_register_15(channel); break;
                default:
                    if (SCDEBUG) printf("SCC: Unimplemented READ %c register: Reg %d\n", ch_name(channel), current_reg_select);
            }
            //printf("SCC: READ  %c register: Reg %d = %02X\n", ch_name(channel), current_reg_select, retval);
            reg_select[channel] = 0;
            update_interrupts(channel);
            return retval;
        }

        // this should be called before reads or writes; and also based on baud timer.
        // this used to rely on the IP flag but that's not right! now uses rx_char_available
        void update_queues() {
            for (int channel = 0; channel < SCC_CHANNEL_COUNT; channel++) {
                if (serial_devices[channel] != nullptr) {
                    // Only check for new data if RX buffer is empty and no RX in progress
                    if (registers[channel].r0_rx_char_available == 0 && !registers[channel].rx_in_progress) {
                        SerialMessage msg = serial_devices[channel]->q_dev.get();
                        if (msg.type == MESSAGE_DATA) {
                            schedule_rx_char((scc_channel_t)channel, (uint8_t)msg.data);
                        } else {
                            if (msg.type != MESSAGE_NONE && SCDEBUG) printf("SCC: READ %c: got unexpected message type: %d\n", ch_name(channel), msg.type);
                        }
                    }
                }
            }
        }

        uint8_t readData(scc_channel_t channel) { 
            uint8_t retval = 0x00;

            update_queues();

            if (registers[channel].r0_rx_char_available) {
                retval = registers[channel].char_rx;

                // signal the rx buffer is empty
                registers[channel].r0_rx_char_available = 0;
                
                // do this instead in update_interrupts()
                /* if (channel == SCC_CHANNEL_A) {
                    registers[SCC_CHANNEL_A].r3_a_rx_pending = 0;
                } else {
                    registers[SCC_CHANNEL_A].r3_b_rx_pending = 0;
                } */
            }
            
            if (SCDEBUG) printf("SCC: READ  %c DATA = %02X\n", ch_name(channel), retval);

            update_interrupts(channel);
            return retval;
        }

        void set_device_channel(scc_channel_t channel, SerialDevice *device) {
            serial_devices[channel] = device;
        }
        
        void debug_output(DebugFormatter *df) {
            df->addLine("Reg Select A: %02X  B: %02X", reg_select[SCC_CHANNEL_A], reg_select[SCC_CHANNEL_B]);
            df->addLine("Clock Mode A: %dX   B: %dX", clock_mode[SCC_CHANNEL_A], clock_mode[SCC_CHANNEL_B]);
            df->addLine("Rx Data A: %02X  B: %02X", registers[SCC_CHANNEL_A].char_rx, registers[SCC_CHANNEL_B].char_rx);
            df->addLine("Tx Data A: %02X  B: %02X", registers[SCC_CHANNEL_A].char_tx, registers[SCC_CHANNEL_B].char_tx);
            df->addLine("r_reg_0 A: %02X  B: %02X", registers[SCC_CHANNEL_A].r_reg_0, registers[SCC_CHANNEL_B].r_reg_0);
            df->addLine("r_reg_1 A: %02X  B: %02X", registers[SCC_CHANNEL_A].r_reg_1, registers[SCC_CHANNEL_B].r_reg_1);
            //df->addLine("r_reg_2 A: %02X  B: %02X", registers[SCC_CHANNEL_A].r_reg_2, registers[SCC_CHANNEL_B].r_reg_2);
            df->addLine("r_reg_3 A: %02X  B: 00", registers[SCC_CHANNEL_A].r_reg_3) ;
            df->addLine("r_reg_10 A: %02X  B: %02X", registers[SCC_CHANNEL_A].r_reg_10, registers[SCC_CHANNEL_B].r_reg_10);
            df->addLine("Baud Rate A: %08.2f  B: %08.2f", baud_rate[SCC_CHANNEL_A], baud_rate[SCC_CHANNEL_B]);
            for (int i = 0; i < 16; i++) {
                df->addLine("WReg[%2d]   A: %02X  B: %02X", i, registers[SCC_CHANNEL_A].w_regs[i], registers[SCC_CHANNEL_B].w_regs[i]);
            }
        }

        // TX completion - called when character transmission is complete
        void tx_complete(scc_channel_t channel) {
            if (SCDEBUG) printf("SCC: Ch %c TX complete, char=%02X, loopback=%d\n", 
                ch_name(channel), registers[channel].char_tx, registers[channel].r14_local_loopback);
            
            registers[channel].tx_in_progress = false;
            registers[channel].r0_tx_buffer_empty = true;
            registers[channel].tx_irq_condition = true;
            
            // Now actually send the byte to the device
            uint8_t data = registers[channel].char_tx;
            
            if (registers[channel].r14_local_loopback) {
                // Local loopback - in loopback mode, RX happens instantaneously
                // The character doesn't actually go out on the wire, it's looped back internally
                if (SCDEBUG) printf("SCC: Ch %c looping back %02X (instant)\n", ch_name(channel), data);
                
                // Check if RX is enabled
                if (registers[channel].r3_rx_enable) {
                    if (registers[channel].r0_rx_char_available) {
                        // RX buffer full - overrun error
                        registers[channel].r1_rx_overrun_err = 1;
                        if (SCDEBUG) printf("SCC: Ch %c RX overrun in loopback\n", ch_name(channel));
                    } else {
                        // Instantaneous loopback - character appears immediately in RX buffer
                        registers[channel].char_rx = data;
                        registers[channel].r0_rx_char_available = 1;
                        if (SCDEBUG) printf("SCC: Ch %c loopback RX complete (instant): %02X\n", ch_name(channel), data);
                    }
                } else {
                    if (SCDEBUG) printf("SCC: Ch %c RX disabled, dropping loopback char\n", ch_name(channel));
                }
            } else if (serial_devices[channel] != nullptr) {
                serial_devices[channel]->q_host.send(SerialMessage{MESSAGE_DATA, data});
            }
            
            update_interrupts(channel);
        }

        // RX completion - called when character reception is complete
        void rx_complete(scc_channel_t channel) {
            if (SCDEBUG) printf("SCC: Ch %c RX complete: %02X\n", ch_name(channel), registers[channel].char_rx);
            
            registers[channel].rx_in_progress = false;
            registers[channel].r0_rx_char_available = 1;
            
            update_interrupts(channel);
        }

        // Schedule reception of a character
        void schedule_rx_char(scc_channel_t channel, uint8_t data) {
            // Check if RX is enabled
            if (!registers[channel].r3_rx_enable) {
                if (SCDEBUG) printf("SCC: Ch %c RX disabled, dropping char %02X\n", ch_name(channel), data);
                return;
            }
            
            if (registers[channel].r0_rx_char_available) {
                // RX buffer full - overrun error
                registers[channel].r1_rx_overrun_err = 1;
                if (SCDEBUG) printf("SCC: Ch %c RX overrun\n", ch_name(channel));
                return;
            }
            
            if (SCDEBUG) printf("SCC: Ch %c scheduling RX of %02X\n", ch_name(channel), data);
            
            registers[channel].char_rx = data;
            registers[channel].rx_in_progress = true;
            
            uint64_t cycles_per_char = get_cycles_per_char(channel, false);
            
            if (cycles_per_char > 0 && event_timer && clock) {
                uint64_t trigger_cycle = clock->get_c14m() + cycles_per_char;
                event_timer->scheduleEvent(trigger_cycle, rx_complete_callback, rx_timer_id[channel], this);
                
                if (SCDEBUG) printf("SCC: Ch %c RX scheduled for %llu cycles\n", 
                    ch_name(channel), cycles_per_char);
            } else {
                // High baud rate or no timer - complete immediately
                if (SCDEBUG) printf("SCC: Ch %c RX completing immediately\n", ch_name(channel));
                rx_complete(channel);
            }
        }

        // Static callback wrappers for EventTimer
        static void tx_complete_callback(uint64_t instanceID, void* userData) {
            Z85C30* scc = static_cast<Z85C30*>(userData);
            // Determine channel from instance ID
            if (instanceID == scc->tx_timer_id[SCC_CHANNEL_A]) {
                scc->tx_complete(SCC_CHANNEL_A);
            } else if (instanceID == scc->tx_timer_id[SCC_CHANNEL_B]) {
                scc->tx_complete(SCC_CHANNEL_B);
            }
        }

        static void rx_complete_callback(uint64_t instanceID, void* userData) {
            Z85C30* scc = static_cast<Z85C30*>(userData);
            // Determine channel from instance ID
            if (instanceID == scc->rx_timer_id[SCC_CHANNEL_A]) {
                scc->rx_complete(SCC_CHANNEL_A);
            } else if (instanceID == scc->rx_timer_id[SCC_CHANNEL_B]) {
                scc->rx_complete(SCC_CHANNEL_B);
            }
        }

};

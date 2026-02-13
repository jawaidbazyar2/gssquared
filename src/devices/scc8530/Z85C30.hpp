#pragma once

#include <cstdint>
#include <cassert>
#include "util/DebugFormatter.hpp"
#include "util/InterruptController.hpp"
#include "serial_devices/SerialDevice.hpp"

constexpr bool SCDEBUG = 0;

/**
 * this code is LITTLE-ENDIAN SPECIFIC.
 * IF you have a big endian machine, you will need to swap the bytes. Or cry.
 */

/* Timing Clock fed into SCC by the IIgs */
constexpr uint64_t SCC_RX_CLOCK = 3'686'400;

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

    /* FILE *data_files[SCC_CHANNEL_COUNT] = { NULL, NULL }; */

    inline char ch_name(scc_channel_t channel) {
        return (uint8_t) channel + 'A';
    }
    
    SerialDevice *serial_devices[SCC_CHANNEL_COUNT] = { nullptr, nullptr };

    struct scc_channel_state_t {
        uint8_t char_rx;
        uint8_t char_tx;
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

        const uint16_t clock_mode_table[4] = { 1, 16, 32, 64 };

        inline void update_timing_sources(scc_channel_t channel) {
            // TODO: implement
            uint16_t time_constant = registers[channel].r12_time_constant;
            uint16_t clock_mode_index = registers[channel].r4_clock_mode;
            uint16_t new_mode = clock_mode_table[clock_mode_index];
            clock_mode[channel] = new_mode;
            float baud_rate = (float)SCC_RX_CLOCK / (2.0f * (float)new_mode * ((float)time_constant + 2.0f));
            this->baud_rate[channel] = baud_rate;
            printf("SCC: Ch %d: Clock Mode: %d, Time Constant: %d, Baud Rate: %08.2f\n", channel, new_mode, time_constant, baud_rate);
        }

        inline void update_interrupts(scc_channel_t channel) {
            // TODO: implement
            bool interrupt_pending = false;
            if (registers[channel].r1_tx_int_enable && registers[channel].r0_tx_buffer_empty) interrupt_pending = true;
            //if (registers[channel].r1_rx_int_mode == 0b01 && registers[channel].r0_rx_char_available) interrupt_pending = true;
            if ((registers[channel].r1_rx_int_mode == 0b10) && registers[channel].r0_rx_char_available) interrupt_pending = true;
            
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
                else if (registers[channel].r0_cmd != 0) {
                    printf("SCC: Unimplemented WRITE %c r0_cmd: Reg %d = %02X\n", ch_name(channel), new_register, registers[channel].r0_cmd);
                }
                if (registers[channel].r0_reset_cmd != 0) {
                    printf("SCC: Unimplemented WRITE %c r0_reset_cmd command: Reg %d = %02X\n", ch_name(channel), new_register, registers[channel].r0_reset_cmd);
                }
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
        inline void write_register_2(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR2, data);
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

        inline void write_register_9(scc_channel_t channel, uint8_t data) {
            print_write_register(channel, WR9, data);
            registers[SCC_CHANNEL_A].w_regs[WR9] = data;
            uint8_t reset_cmd = registers[SCC_CHANNEL_A].r9_reset;
            if (reset_cmd == 0x01) { // Channel Reset B
                reset_channel(SCC_CHANNEL_B);
            }
            if (reset_cmd == 0x02) { // Channel Reset A
                reset_channel(SCC_CHANNEL_A);
            }
            if (reset_cmd == 0x03) { // Force HW Reset
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
            registers[channel].r0_dcd = 1;
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
                retval = registers[SCC_CHANNEL_A].w_regs[WR2];        // do what?
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
        Z85C30(InterruptController *irq_control) : irq_control(irq_control) {
            reset();
        };

        ~Z85C30() {}

       /*  void set_data_file(scc_channel_t channel, FILE *data_file) {
            this->data_files[channel] = data_file;
        } */

        void reset_channel(scc_channel_t channel) {
            for (int i = 0; i < 16; i++) {
                registers[channel].w_regs[i] = 0;
            }
            registers[channel].r_reg_0 = 0;
            //registers[channel].r_reg_2 = 0;
            registers[channel].r_reg_1 = 0;
            registers[channel].r_reg_3 = 0;
            registers[channel].r_reg_10 = 0;

            reg_select[channel] = 0;
            clock_mode[channel] = 1;
            baud_rate[channel] = 0.0f;
            update_timing_sources(channel);
        }
        
        void reset() {
            reset_channel(SCC_CHANNEL_A);
            reset_channel(SCC_CHANNEL_B);
        }
        
        void writeCmd(scc_channel_t channel, uint8_t data) {
            // current reg_select for channel
            update_queues();

            uint16_t current_reg_select = reg_select[channel];
            switch (current_reg_select) {
                case WR0: write_register_0(channel, data); break;
                case WR1: write_register_1(channel, data);  reg_select[channel] = 0; break;
                case WR2: write_register_2(channel, data);  reg_select[channel] = 0; break;
                case WR3: write_register_3(channel, data);  reg_select[channel] = 0; break;
                case WR4: write_register_4(channel, data);  reg_select[channel] = 0; break;
                case WR5: write_register_5(channel, data);  reg_select[channel] = 0; break;
                case WR6: write_register_6(channel, data);  reg_select[channel] = 0; break;
                case WR7: write_register_7(channel, data);  reg_select[channel] = 0; break;
                case WR8: writeData(channel, data);  reg_select[channel] = 0; break;
                case WR9: write_register_9(SCC_CHANNEL_A, data);  reg_select[channel] = 0; break;
                case WR10: write_register_10(channel, data);  reg_select[channel] = 0; break;
                case WR11: write_register_11(channel, data);  reg_select[channel] = 0; break;
                case WR12: write_register_12(channel, data);  reg_select[channel] = 0; break;
                case WR13: write_register_13(channel, data);  reg_select[channel] = 0; break;
                case WR14: write_register_14(channel, data);  reg_select[channel] = 0; break;
                case WR15: write_register_15(channel, data);  reg_select[channel] = 0; break;
                default:
                    printf("SCC: Unimplemented WRITE %c register: Reg %d = %02X\n", ch_name(channel), current_reg_select, data);
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
            printf("SCC: WRITE %c DATA = %02X\n", ch_name(channel), data);

            update_queues();

            // send byte to device (if one was assigned)
            registers[channel].char_tx = data;
            if (registers[SCC_CHANNEL_A].r14_local_loopback) { // Local Loopback enabled
                // need to stuff this into read channel now.
                registers[channel].char_rx = data;
                registers[channel].r0_rx_char_available = 1;
                registers[SCC_CHANNEL_A].r3_a_rx_pending = 1;
            } else if (serial_devices[channel] != nullptr) {
                serial_devices[channel]->q_host.send(SerialMessage{MESSAGE_DATA, data});
            }
            
            // signal the tx buffer is empty (instant send for now)
            if (channel == SCC_CHANNEL_A) {
                registers[channel].r0_tx_buffer_empty = 1;
                registers[SCC_CHANNEL_A].r3_a_tx_pending = 0;
            } else {
                registers[channel].r0_tx_buffer_empty = 1;
                registers[SCC_CHANNEL_A].r3_a_tx_pending = 0;
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
                    printf("SCC: Unimplemented READ %c register: Reg %d\n", ch_name(channel), current_reg_select);
            }
            //printf("SCC: READ  %c register: Reg %d = %02X\n", ch_name(channel), current_reg_select, retval);
            reg_select[channel] = 0;
            return retval;
        }

        // this should be called before reads or writes; and also based on baud timer.
        void update_queues() {
            for (int channel = 0; channel < SCC_CHANNEL_COUNT; channel++) {
                if (serial_devices[channel] != nullptr) {
                    if ((channel == SCC_CHANNEL_A) && (registers[SCC_CHANNEL_A].r3_a_rx_pending == 0)) {
                        SerialMessage msg = serial_devices[channel]->q_dev.get();
                        if (msg.type == MESSAGE_DATA) {
                            registers[channel].char_rx = msg.data;
                            registers[channel].r0_rx_char_available = 1;
                            registers[SCC_CHANNEL_A].r3_a_rx_pending = 1;
                        }
                    }
                    if ((channel == SCC_CHANNEL_B) && (registers[SCC_CHANNEL_A].r3_b_rx_pending == 0)) {
                        SerialMessage msg = serial_devices[channel]->q_dev.get();
                        if (msg.type == MESSAGE_DATA) {
                            registers[channel].char_rx = msg.data;
                            registers[channel].r0_rx_char_available = 1;                            
                            registers[SCC_CHANNEL_A].r3_b_rx_pending = 1;
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
                if (channel == SCC_CHANNEL_A) {
                    registers[SCC_CHANNEL_A].r3_a_rx_pending = 0;
                } else {
                    registers[SCC_CHANNEL_A].r3_b_rx_pending = 0;
                }
            }
            printf("SCC: READ  %c DATA = %02X\n", ch_name(channel), retval);
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

};

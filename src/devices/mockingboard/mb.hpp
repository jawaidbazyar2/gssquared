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

#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "util/InterruptController.hpp"
#include "util/DebugFormatter.hpp"

#define MB_6522_DDRA 0x03
#define MB_6522_DDRB 0x02
#define MB_6522_ORA  0x01
#define MB_6522_ORB  0x00

#define MB_6522_T1C_L 0x04
#define MB_6522_T1C_H 0x05
#define MB_6522_T1L_L 0x06
#define MB_6522_T1L_H 0x07

#define MB_6522_T2C_L 0x08
#define MB_6522_T2C_H 0x09
#define MB_6522_SR 0x0A
#define MB_6522_ACR 0x0B
#define MB_6522_PCR 0x0C
#define MB_6522_IFR 0x0D
#define MB_6522_IER 0x0E
#define MB_6522_ORA_NH 0x0F

#define MB_6522_1 0x00
#define MB_6522_2 0x80

#define AY_8913_REGISTER_COUNT 16

union ifr_t {
    uint8_t value;
    struct {
        uint8_t ca2 : 1;
        uint8_t ca1 : 1;
        uint8_t shift_register : 1;
        uint8_t cb2 : 1;
        uint8_t cb1 : 1;
        uint8_t timer2 : 1;
        uint8_t timer1 : 1;
        uint8_t irq : 1;
    } bits;
};

union ier_t {
    uint8_t value;
    struct {
        uint8_t ca2 : 1;
        uint8_t ca1 : 1;
        uint8_t shift_register : 1;
        uint8_t cb2 : 1;
        uint8_t cb1 : 1;
        uint8_t timer2 : 1;
        uint8_t timer1 : 1;
    } bits;
};

struct mb_6522_regs {
   
    uint8_t ora; /* 0x00 */
    uint8_t ira; /* 0x00 */
    uint8_t orb; /* 0x01 */
    uint8_t irb; /* 0x01 */
    uint8_t ddra; /* 0x02 */
    uint8_t ddrb; /* 0x03 */

    uint8_t sr; /* 0x0A */
    uint8_t acr; /* 0x0B */
    uint8_t pcr; /* 0x0C */
    ifr_t ifr; /* 0x0D */
    ier_t ier; /* 0x0E */

    uint16_t t1_latch;
    uint16_t t1_counter;  
    uint16_t t2_latch;
    uint16_t t2_counter;
    uint16_t t1_oneshot_pending = 0;
    uint16_t t2_oneshot_pending = 0;

    uint64_t t1_triggered_cycles; // used to calculate what t1 and t2 are at any given cycle. ((cycle_now - triggered_cycles) % latch)
    uint64_t t2_triggered_cycles;

    uint8_t reg_num;

};

class MockingboardEmulator; // forward declaration

struct mb_cpu_data: public SlotData {
    computer_t *computer;
    NClock *clock;
    AudioSystem *audio_system;
    MockingboardEmulator *mockingboard;
    mb_6522_regs d_6522[2];
    std::vector<float> audio_buffer;
    SDL_AudioStream *stream;
    uint64_t last_cycle;
    uint8_t slot;
    EventTimer *event_timer;
    InterruptController *irq_control = nullptr;

    float samples_accumulated = 0.0f;
    float frame_rate;
    float samples_per_frame;
    uint32_t samples_per_frame_int;
    float samples_per_frame_remainder;
    uint64_t c14m_rate;
};

void init_slot_mockingboard(computer_t *computer, SlotType_t slot);
void mb_write_Cx00(cpu_state *cpu, uint16_t addr, uint8_t data);
uint8_t mb_read_Cx00(cpu_state *cpu, uint16_t addr);
void generate_mockingboard_frame(cpu_state *cpu, SlotType_t slot);
void mb_reset(cpu_state *cpu);
DebugFormatter *debug_registers_6522(mb_cpu_data *mb_d);

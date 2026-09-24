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
#include "W6522.hpp"
#include "AY8910-2.hpp"
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

class Mockingboard; // forward declaration

struct mb_cpu_data: public SlotData {
    computer_t *computer;
    NClock *clock;
    AudioSystem *audio_system;
    Mockingboard *mockingboard;
    //mb_6522_regs d_6522[2];
    std::vector<float> audio_buffer;
    //SDL_AudioStream *stream;

    uint8_t slot;
    VidRail *vid;
    InterruptController *irq_control = nullptr;

};

void init_slot_mockingboard(computer_t *computer, SlotType_t slot);
void mb_write_Cx00(cpu_state *cpu, uint16_t addr, uint8_t data);
uint8_t mb_read_Cx00(cpu_state *cpu, uint16_t addr);
void generate_mockingboard_frame(cpu_state *cpu, SlotType_t slot);
void mb_reset(cpu_state *cpu);
DebugFormatter *debug_registers_6522(mb_cpu_data *mb_d);

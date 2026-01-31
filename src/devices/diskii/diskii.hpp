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

#include "devices.hpp"
#include "slots.hpp"
#include "computer.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "util/media.hpp"
#include "util/mount.hpp"

#define DiskII_Ph0_Off 0x00
#define DiskII_Ph0_On 0x01
#define DiskII_Ph1_Off 0x02
#define DiskII_Ph1_On 0x03
#define DiskII_Ph2_Off 0x04
#define DiskII_Ph2_On 0x05
#define DiskII_Ph3_Off 0x06
#define DiskII_Ph3_On 0x07
#define DiskII_Motor_Off 0x08
#define DiskII_Motor_On 0x09
#define DiskII_Drive1_Select 0x0A
#define DiskII_Drive2_Select 0x0B
#define DiskII_Q6L 0x0C
#define DiskII_Q6H 0x0D
#define DiskII_Q7L 0x0E
#define DiskII_Q7H 0x0F

struct diskII {
    uint8_t rw_mode; // 0 = read, 1 = write
    int8_t track;
    uint8_t phase0;
    uint8_t phase1;
    uint8_t phase2;
    uint8_t phase3;
    uint8_t last_phase_on;
    uint8_t Q7 = 0;
    uint8_t Q6 = 0;
    uint8_t write_protect = 0; // 1 = write protect, 0 = not write protect
    uint16_t image_index = 0;
    uint16_t head_position = 0; // index into the track
    uint8_t bit_position = 0; // how many bits left in byte.
    uint16_t read_shift_register = 0; // when bit position = 0, this is 0. As bit_position increments, we shift in the next bit of the byte at head_position.
    uint8_t write_shift_register = 0; 
    uint64_t last_read_cycle = 0;
    bool is_mounted = false;
    bool modified = false;
    disk_image_t media;
    nibblized_disk_t nibblized;
    media_descriptor *media_d;
#if 0
    uint8_t random_track[0x1A00];
#endif
};

struct diskII_controller : public SlotData {
    computer_t *computer;
    diskII drive[2];
    uint8_t drive_select;
    bool motor;
    uint64_t mark_cycles_turnoff = 0; // when DRIVES OFF, set this to current cpu cycles. Then don't actually set motor=0 until one second (1M cycles) has passed. Then reset this to 0.
};


void init_slot_diskII(computer_t *computer, SlotType_t slot);
void mount_diskII(cpu_state *cpu, uint8_t slot, uint8_t drive, media_descriptor *media);
void unmount_diskII(cpu_state *cpu, uint8_t slot, uint8_t drive);
void writeback_diskII_image(cpu_state *cpu, uint8_t slot, uint8_t drive);
drive_status_t diskii_status(cpu_state *cpu, uint64_t key);
void diskii_reset(cpu_state *cpu);
void debug_dump_disk_images(cpu_state *cpu);
bool any_diskii_motor_on(cpu_state *cpu);
int diskii_tracknumber_on(cpu_state *cpu);

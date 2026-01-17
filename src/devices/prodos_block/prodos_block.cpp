/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <stdio.h>

#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "devices/prodos_block/prodos_block.hpp"
#include "util/media.hpp"
#include "pd_block_firmware.hpp"

typedef struct media_t {
    FILE *file;
    media_descriptor *media;
/*     uint16_t block_size;
    uint16_t block_count;
 */} media_t;

media_t prodosblockdevices[8][2];

void pv_return(cpu_state *cpu) {
    // pop two bytes off the stack.
    // TODO: These need to do proper bounds checking
    uint8_t pcl = read_memory(cpu, 0x0100 + cpu->sp+1);
    uint8_t pch = read_memory(cpu, 0x0100 + cpu->sp+2);
    cpu->sp += 2;
    cpu->pc = (pcl | (pch << 8)) + 1;
    printf("ParaVirtual Return PC: %04X SP: A: %02X C: %02X\n", cpu->pc, cpu->a_lo, cpu->C);
}

uint8_t status(cpu_state *cpu, uint8_t slot, uint8_t drive) {
    if (prodosblockdevices[slot][drive].file == nullptr) {
        return 0x01; // device not ready
    }
    return 0x00; // device ready
}

/**
 * These two routines read and write a block to the media.
 * They take into account the media descriptor and the data offset.
 * They read and write memory to the cpu using the memory functions
 * that take into account the memory map, so, we can write data into
 * any bank selected as the CPU (or a 'DMA' device like us) would see it.
 */
void read_block(cpu_state *cpu, uint8_t slot, uint8_t drive, uint16_t block, uint16_t addr) {
    // TODO: read the block into the address.
    static uint8_t block_buffer[512];
    FILE *fp = prodosblockdevices[slot][drive].file;
    
    media_descriptor *media = prodosblockdevices[slot][drive].media;

    fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);
    fread(block_buffer, 1, media->block_size, fp);
    for (int i = 0; i < media->block_size; i++) {
        // TODO: for dma we want to simulate the memory map but do not want to burn cycles.
        // the CPU would halt during a DMA and not tick cycles even though the rest of the bus
        // is following the system clock.
        // TODO: So we need a dma_write_memory and dma_read_memory set of routines that do that.
        write_memory(cpu, addr + i, block_buffer[i]); 
    }
    //debug_dump_memory(cpu, addr, addr + media[slot][drive].block_size);
}

void write_block(cpu_state *cpu, uint8_t slot, uint8_t drive, uint16_t block, uint16_t addr) {
    // TODO: read the block into the address.
    static uint8_t block_buffer[512];
    FILE *fp = prodosblockdevices[slot][drive].file;
    media_descriptor *media = prodosblockdevices[slot][drive].media;
    
    for (int i = 0; i < media->block_size; i++) {
        // TODO: for dma we want to simulate the memory map but do not want to burn cycles.
        block_buffer[i] = read_memory(cpu, addr + i); 
    }
    fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);
    fwrite(block_buffer, 1, media->block_size, fp);

    //debug_dump_memory(cpu, addr, addr + media[slot][drive].block_size);
}

void prodos_block_pv_trap(cpu_state *cpu) {
    uint16_t block = raw_memory_read(cpu, PD_BLOCK_LO) | (raw_memory_read(cpu, PD_BLOCK_HI) << 8);
    uint16_t addr = raw_memory_read(cpu, PD_ADDR_LO) | (raw_memory_read(cpu, PD_ADDR_HI) << 8);
    uint8_t unit = raw_memory_read(cpu, PD_DEV);
    uint8_t slot = (unit >> 4) & 0b0111;
    uint8_t drive = (unit >> 7) & 0b1;
    uint8_t cmd = raw_memory_read(cpu, PD_CMD);
    printf("ParaVirtual Trap PC: %04X, Unit: %02X, Block: %04X, Addr: %04X, CMD: %02X\n", cpu->pc, unit, block, addr, cmd);

    if (cmd == 0x00) {
        uint8_t st = status(cpu, slot, drive);
        if (st) {
            cpu->C = 1; // set error flag.
            cpu->a_lo = PD_ERROR_NO_DEVICE;
        } else {
            cpu->C = 0; // clear error flag.
            cpu->a_lo = 0x00;
            cpu->x_lo = 0x40;
            cpu->y_lo = 0x06;
        }
    } else if (cmd == 0x01) {
        uint8_t st = status(cpu, slot, drive);
        if (st) {
            cpu->C = 1; // set error flag.
            cpu->a_lo = PD_ERROR_NO_DEVICE;
        } else {
            read_block(cpu, slot, drive, block, addr);
            cpu->C = 0; // clear carry
            cpu->a_lo = 0x00;
        }
    } else if (cmd == 0x02) {
        uint8_t st = status(cpu, slot, drive);
        if (st) {
            cpu->C = 1; // set error flag.
            cpu->a_lo = PD_ERROR_NO_DEVICE;
        } else {
            write_block(cpu, slot, drive, block, addr);
            cpu->C = 0; // clear carry
            cpu->a_lo = 0x00;
        }
    } else if (cmd == 0x03) { // 
        cpu->C = 1; // set error flag. // format not implemented, TODO
        cpu->a_lo = PD_ERROR_NO_DEVICE;
    }
    pv_return(cpu);
}

void mount_prodos_block(cpu_state *cpu, uint8_t slot, uint8_t drive, media_descriptor *media) {
    //printf("Mounting ProDOS block device %s slot %d drive %d\n", media->filename, slot, drive);
    //std::cout << std::format("Mounting ProDOS block device {} slot {} drive {}\n", media->filename, slot, drive) << std::endl;
    std::cout << "Mounting ProDOS block device " << media->filename << " slot " << slot << " drive " << drive << std::endl;
    FILE *fp = fopen(media->filename.c_str(), "r+b");
    if (fp == nullptr) {
        //fprintf(stderr, "Could not open ProDOS block device file: %s\n", media->filename);
        std::cerr << "Could not open ProDOS block device file: " << media->filename << std::endl;
        return;
    }
    prodosblockdevices[slot][drive].file = fp;
    prodosblockdevices[slot][drive].media = media;
}

void init_prodos_block(cpu_state *cpu, SlotType_t slot)
{

    //printf("Prodos Block Firmware:\n");
    for (int i = 0; i < 256; i++)
    {
        /* printf("%02X ", pd_block_firmware[i]);
        if (i % 16 == 0x0f)
        {
            printf("\n");
        } */

        // load the firmware into the slot memory
        for (int i = 0; i < 256; i++) {
            raw_memory_write(cpu, 0xC000 + (slot * 0x0100) + i, pd_block_firmware[i]);
        }

        /* for (int slot = 0; slot <= 7; slot++) {
            for (int drive = 0; drive <= 1; drive++) {
                media[slot][drive].file = NULL;
                media[slot][drive].block_size = 0;
                media[slot][drive].block_count = 0;
            }
        } */
    }

}

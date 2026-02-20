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

//#include <stdio.h>
#include <iostream>
#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "devices/pdblock2/pdblock2.hpp"
#include "util/media.hpp"
#include "util/ResourceFile.hpp"
#include "util/mount.hpp"

void pdblock2_print_cmdbuffer(pdblock_cmd_buffer *pdb) {
    std::cout << "PD_CMD_BUFFER: ";
    for (int i = 0; i < pdb->index; i++) {
        std::cout << std::hex << (int)pdb->cmd[i] << " ";
    }
    std::cout << std::endl;
}

drive_status_t pdblock2_osd_status(pdblock2_data *pdblock_d, uint64_t key) {
    uint8_t slot = key >> 8;
    uint8_t drive = key & 0xFF;

    media_t seldrive = pdblock_d->prodosblockdevices[slot][drive];

    bool motor = false;

    uint64_t curtime = SDL_GetTicksNS();
    if (curtime - seldrive.last_block_access_time < 1000000000) {
        motor = true;
    }
    // 3.5 drives turn off immediately.
    std::string fname;
    bool mounted = false;
    if (seldrive.media) {
        fname = seldrive.media->filestub;
        mounted = true;
    }

    return {mounted, fname, motor, seldrive.last_block_accessed};
}

uint8_t pdblock2_status(cpu_state *cpu, pdblock2_data *pdblock_d, uint8_t slot, uint8_t drive) {
    
    if (pdblock_d->prodosblockdevices[slot][drive].file == nullptr) {
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
 * slot and drive here might be virtual as one physical slot can map drives
 * to a different virtual slot.
 */
void pdblock2_read_block(cpu_state *cpu, pdblock2_data *pdblock_d, uint8_t slot, uint8_t drive, uint16_t block, uint16_t addr) {

    // TODO: read the block into the address.
    static uint8_t block_buffer[512];
    FILE *fp = pdblock_d->prodosblockdevices[slot][drive].file;
    
    media_descriptor *media = pdblock_d->prodosblockdevices[slot][drive].media;

    fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);
    fread(block_buffer, 1, media->block_size, fp);
    for (int i = 0; i < media->block_size; i++) {
        // TODO: for dma we want to simulate the memory map but do not want to burn cycles.
        // the CPU would halt during a DMA and not tick cycles even though the rest of the bus
        // is following the system clock.
        // TODO: So we need a dma_write_memory and dma_read_memory set of routines that do that.
        //write_memory(cpu, addr + i, block_buffer[i]); 
        cpu->mmu->write(addr + i, block_buffer[i]); 
    }
    pdblock_d->prodosblockdevices[slot][drive].last_block_accessed = block;
    pdblock_d->prodosblockdevices[slot][drive].last_block_access_time = SDL_GetTicksNS();
    //debug_dump_memory(cpu, addr, addr + media[slot][drive].block_size);
}

void pdblock2_write_block(cpu_state *cpu, pdblock2_data *pdblock_d, uint8_t slot, uint8_t drive, uint16_t block, uint16_t addr) {

    // TODO: read the block into the address.
    static uint8_t block_buffer[512];
    FILE *fp = pdblock_d->prodosblockdevices[slot][drive].file;
    media_descriptor *media = pdblock_d->prodosblockdevices[slot][drive].media;

    if (media->write_protected) {
        pdblock_d->cmd_buffer.error = PD_ERROR_WRITE_PROTECTED;
        return;
    }

    for (int i = 0; i < media->block_size; i++) {
        // TODO: for dma we want to simulate the memory map but do not want to burn cycles.
        //block_buffer[i] = read_memory(cpu, addr + i); 
        block_buffer[i] = cpu->mmu->read(addr + i); 
    }
    fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);
    fwrite(block_buffer, 1, media->block_size, fp);
    pdblock_d->prodosblockdevices[slot][drive].last_block_accessed = block;
    pdblock_d->prodosblockdevices[slot][drive].last_block_access_time = SDL_GetTicksNS();
}

void pdblock2_execute(cpu_state *cpu, pdblock2_data *pdblock_d) {
    uint8_t cmd, dev, unit, slot, drive;
    uint16_t block, addr;

    //pdblock2_data * pdblock_d = (pdblock2_data *)get_module_state(cpu, MODULE_PD_BLOCK2);

    if (DEBUG(DEBUG_PD_BLOCK)) pdblock2_print_cmdbuffer(&pdblock_d->cmd_buffer);

    uint8_t cksum = 0;
    for (int i = 0; i < pdblock_d->cmd_buffer.index; i++) {
        cksum ^= pdblock_d->cmd_buffer.cmd[i];
    }
    if (cksum != 0x00) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pdblock2_execute: Checksum error" << std::endl;
        pdblock_d->cmd_buffer.error = 0x01;
        return;
    }

    uint8_t version = pdblock_d->cmd_buffer.cmd[0] ;
    if (version == 0x01) {
        // TODO: version 1 command
        pdblock_cmd_v1 *cmdbuf = (pdblock_cmd_v1 *)pdblock_d->cmd_buffer.cmd;
        cmd = cmdbuf->cmd;
        dev = cmdbuf->dev;
        block = cmdbuf->block_lo | (cmdbuf->block_hi << 8);
        addr = cmdbuf->addr_lo | (cmdbuf->addr_hi << 8);        
        slot = (dev >> 4) & 0b0111;
        drive = (dev >> 7) & 0b1;
    } else {
        // TODO: return some kind of error
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pdblock2_execute: Version not supported" << std::endl;
        pdblock_d->cmd_buffer.error = 0x01;
        return;
    }

    if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pdblock2_execute: Unit: " << std::hex << (int)unit 
        << ", Block: " << std::hex << (int)block << ", Addr: " << std::hex << (int)addr << ", CMD: " 
        << std::hex << (int)cmd << std::endl;

    uint8_t st = pdblock2_status(cpu, pdblock_d, slot, drive);
    if (st) {
        pdblock_d->cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
        return;
    }
    if (cmd == 0x00) {
        media_descriptor *media = pdblock_d->prodosblockdevices[slot][drive].media;
        pdblock_d->cmd_buffer.error = 0x00;
        pdblock_d->cmd_buffer.status1 = media->block_count & 0xFF;
        pdblock_d->cmd_buffer.status2 = (media->block_count >> 8) & 0xFF;
    } else if (cmd == 0x01) {
        pdblock2_read_block(cpu, pdblock_d, slot, drive, block, addr);
        pdblock_d->cmd_buffer.error = 0x00;
        pdblock_d->cmd_buffer.status1 = 0x00;
        pdblock_d->cmd_buffer.status2 = 0x00;
    } else if (cmd == 0x02) {
        pdblock2_write_block(cpu, pdblock_d, slot, drive, block, addr);
        pdblock_d->cmd_buffer.error = 0x00;
        pdblock_d->cmd_buffer.status1 = 0x00;
        pdblock_d->cmd_buffer.status2 = 0x00;
    } else if (cmd == 0x03) { // not implemented
        pdblock_d->cmd_buffer.error = PD_ERROR_NO_DEVICE;
    }
}

//bool mount_pdblock2(cpu_state *cpu, uint8_t slot, uint8_t drive, media_descriptor *media) {
bool mount_pdblock2(pdblock2_data *pdblock_d, uint8_t slot, uint8_t drive, media_descriptor *media) {
    //pdblock2_data * pdblock_d = (pdblock2_data *)get_slot_state(cpu, (SlotType_t)slot);
    if (pdblock_d == nullptr) {
        //std::cerr << "pdblock2_mount: pdblock_d is nullptr" << std::endl; 
        return false;
    }
    //if (DEBUG(DEBUG_PD_BLOCK)) printf("Mounting ProDOS block device %s slot %d drive %d\n", media->filename, slot, drive);
    if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "Mounting ProDOS block device " << media->filename << " slot " << slot << " drive " << drive << std::endl;

    FILE *fp = fopen(media->filename.c_str(), "r+b");
    if (fp == nullptr) {
        //fprintf(stderr, "Could not open ProDOS block device file: %s\n", media->filename);
        std::cerr << "Could not open ProDOS block device file: " << media->filename << std::endl;
        return false;
    }
    pdblock_d->prodosblockdevices[slot][drive].file = fp;
    pdblock_d->prodosblockdevices[slot][drive].media = media;
    return true;
}

bool unmount_pdblock2(pdblock2_data *pdblock_d, uint64_t key) {
    uint8_t slot = key >> 8;
    uint8_t drive = key & 0xFF;
    //pdblock2_data * pdblock_d = (pdblock2_data *)get_slot_state(cpu, (SlotType_t)slot);
    if (pdblock_d->prodosblockdevices[slot][drive].file) {
        fclose(pdblock_d->prodosblockdevices[slot][drive].file);
        pdblock_d->prodosblockdevices[slot][drive].file = nullptr;
        pdblock_d->prodosblockdevices[slot][drive].media = nullptr;
    }
    return true;
}

void pdblock2_write_C0x0(void *context, uint32_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    SlotType_t slot = (SlotType_t)((addr - 0xC080) / 0x10);
    pdblock2_data * pdblock_d = (pdblock2_data *)get_slot_state(cpu, slot);

    if ((addr & 0xF) == 0x00) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_CMD_RESET: " << std::hex << (int)data << std::endl;
        pdblock_d->cmd_buffer.index = 0;
    } else if ((addr & 0xF) == 0x01) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_CMD_PUT: " << std::hex << (int)data << std::endl;
        if (pdblock_d->cmd_buffer.index < MAX_PD_BUFFER_SIZE) {
            pdblock_d->cmd_buffer.cmd[pdblock_d->cmd_buffer.index] = data;
            pdblock_d->cmd_buffer.index++;
        }
    } else if ((addr & 0xF) == 0x02) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_CMD_EXECUTE: " << std::hex << (int)data << std::endl;
        pdblock2_execute(cpu, pdblock_d);
        pdblock_d->cmd_buffer.index = 0;
    } 
}

uint8_t pdblock2_read_C0x0(void *context, uint32_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    SlotType_t slot = (SlotType_t)((addr - 0xC080) / 0x10);
    pdblock2_data * pdblock_d = (pdblock2_data *)get_slot_state(cpu, slot);
    uint8_t val;

    if ((addr & 0xF) == 0x03) {
        val = pdblock_d->cmd_buffer.error;
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_ERROR_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else if ((addr & 0xF) == 0x04) {
        val = pdblock_d->cmd_buffer.status1;
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_STATUS1_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else if ((addr & 0xF) == 0x05) {
        val = pdblock_d->cmd_buffer.status2;
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD_STATUS2_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else return 0xE0;
}

void init_pdblock2(computer_t *computer, SlotType_t slot)
{
    cpu_state *cpu = computer->cpu;
    
    if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "Initializing ProDOS Block2 slot " << slot << std::endl;
    pdblock2_data * pdblock_d = new pdblock2_data;
    pdblock_d->id = DEVICE_ID_PD_BLOCK2;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 2; j++) {
            pdblock_d->prodosblockdevices[i][j].file = nullptr;
            pdblock_d->prodosblockdevices[i][j].media = nullptr;
        }
    }

    // set in CPU so we can reference later
    set_slot_state(cpu, slot, pdblock_d);

    ResourceFile *rom = new ResourceFile("roms/cards/pdblock2/pdblock2.rom", READ_ONLY);
    if (rom == nullptr) {
        std::cerr << "Failed to load pdblock2.rom" << std::endl;
        return;
    }
    rom->load();
    pdblock_d->rom = (uint8_t *)(rom->get_data());

    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = (uint8_t *)(rom->get_data());

    computer->mmu->set_slot_rom(slot, rom_data, "PDBLK_ROM");

    // load the firmware into the slot memory -- refactor this
    /* for (int i = 0; i < 256; i++) {
        raw_memory_write(cpu, 0xC000 + (slot * 0x0100) + i, rom_data[i]);
    } */

    // register drives with mounts for status reporting
    uint64_t key = (slot << 8) | 0;
    // TODO: FIX
#if 0
    computer->mounts->register_drive(DRIVE_TYPE_PRODOS_BLOCK, key);
    computer->mounts->register_drive(DRIVE_TYPE_PRODOS_BLOCK, key + 1);
#endif
    PDBlockThunk *thunk = new PDBlockThunk(pdblock_d);
    computer->mounts->register_storage_device(key, thunk, DRIVE_TYPE_PRODOS_BLOCK);
    computer->mounts->register_storage_device(key + 1, thunk, DRIVE_TYPE_PRODOS_BLOCK);

    // register.. uh, registers.
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_RESET, { pdblock2_write_C0x0, cpu });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_PUT, { pdblock2_write_C0x0, cpu });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_EXECUTE, { pdblock2_write_C0x0, cpu });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_ERROR_GET, { pdblock2_read_C0x0, cpu });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS1_GET, { pdblock2_read_C0x0, cpu });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS2_GET, { pdblock2_read_C0x0, cpu });

/* register_C0xx_memory_write_handler((slot * 0x10) + PD_CMD_RESET, pdblock2_write_C0x0);
    register_C0xx_memory_write_handler((slot * 0x10) + PD_CMD_PUT, pdblock2_write_C0x0);
    register_C0xx_memory_write_handler((slot * 0x10) + PD_CMD_EXECUTE, pdblock2_write_C0x0);
    register_C0xx_memory_read_handler((slot * 0x10) + PD_ERROR_GET, pdblock2_read_C0x0);
    register_C0xx_memory_read_handler((slot * 0x10) + PD_STATUS1_GET, pdblock2_read_C0x0);
    register_C0xx_memory_read_handler((slot * 0x10) + PD_STATUS2_GET, pdblock2_read_C0x0); */
}

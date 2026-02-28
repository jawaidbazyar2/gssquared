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
#include "mmus/mmu_ii.hpp"
#include "debug.hpp"
#include "devices/pdblock3/pdblock3.hpp"
#include "util/media.hpp"
#include "util/ResourceFile.hpp"
#include "util/mount.hpp"

class PDBlock3 : public StorageDevice {
private:
    MMU *mmu;
    pdblock_cmd_buffer cmd_buffer;
    media_t drives[PDB3_MAX_UNITS];
    uint8_t _slot;

public:
    PDBlock3(uint8_t slot, MMU *mmu) : _slot(slot), mmu(mmu) {
        for (int j = 0; j < PDB3_MAX_UNITS; j++) {
            drives[j].file = nullptr;
            drives[j].media = nullptr;
        }
        cmd_buffer.index = 0;
        cmd_buffer.error = 0x00;
        cmd_buffer.status1 = 0x00;
        cmd_buffer.status2 = 0x00;
    }

    ~PDBlock3() {
        for (int j = 0; j < PDB3_MAX_UNITS; j++) {
            if (drives[j].file) {
                fclose(drives[j].file);
                drives[j].file = nullptr;
            }
        }
    }

    void read_from_memory(uint32_t addr, uint8_t *cb, uint8_t size) {
        for (int i = 0; i < size; i++) {
            cb[i] = mmu->read(addr + i);
        }
    }

    void write_to_memory(uint32_t addr, uint8_t *data, uint8_t size) {
        for (int i = 0; i < size; i++) {
            mmu->write(addr + i, data[i]);
        }
    }

    uint8_t internal_status(uint8_t drive) {
        if (drive >= PDB3_MAX_UNITS) return 0x01;
        if (drives[drive].file == nullptr) {
            return 0x01; // device not ready
        }
        return 0x00; // device ready
    }


    /**
    * These two routines read and write a block to the media.
    * They take into account the media descriptor and the data offset.
    * They read and write memory to the MMU using the MMU functions
    * that take into account the memory map, so, we can write data into
    * any bank selected as the CPU (or a 'DMA' device like us) would see it.
    * slot and drive here might be virtual as one physical slot can map drives
    * to a different virtual slot.
    */
    void read_block(uint8_t drive, uint32_t block, uint32_t addr) {
        uint8_t block_buffer[512];
        if (drive >= PDB3_MAX_UNITS) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return;
        }
        FILE *fp = drives[drive].file;
        media_descriptor *media = drives[drive].media;
        if (fp == nullptr || media == nullptr) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return;
        }
        if (media->block_size == 0 || media->block_size > 512) {
            cmd_buffer.error = PD_ERROR_IO;
            return;
        }
        if (block >= media->block_count) {
            cmd_buffer.error = PD_ERROR_IO;
            return;
        }
        if (fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET) < 0) {
            cmd_buffer.error = PD_ERROR_IO;
        }
        size_t bytes_read = fread(block_buffer, 1, media->block_size, fp);
        if (bytes_read != media->block_size) {
            cmd_buffer.error = PD_ERROR_IO;
        }
        for (int i = 0; i < media->block_size; i++) {
            mmu->write(addr + i, block_buffer[i]); 
        }
        drives[drive].last_block_accessed = block;
        drives[drive].last_block_access_time = SDL_GetTicksNS();
    }

    void write_block(uint8_t drive, uint32_t block, uint32_t addr) {

        uint8_t block_buffer[512];
        if (drive >= PDB3_MAX_UNITS) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return;
        }
        FILE *fp = drives[drive].file;
        media_descriptor *media = drives[drive].media;

        if (fp == nullptr || media == nullptr) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return;
        }
        if (media->block_size == 0 || media->block_size > 512) {
            cmd_buffer.error = PD_ERROR_IO;
            return;
        }

        if (block >= media->block_count) {
            cmd_buffer.error = PD_ERROR_IO;
            return;
        }
        if (media->write_protected) {
            cmd_buffer.error = PD_ERROR_WRITE_PROTECTED;
            return;
        }

        for (int i = 0; i < media->block_size; i++) {
            block_buffer[i] = mmu->read(addr + i); 
        }
        fseek(fp, media->data_offset + (block * media->block_size), SEEK_SET);
        fwrite(block_buffer, 1, media->block_size, fp);
        drives[drive].last_block_accessed = block;
        drives[drive].last_block_access_time = SDL_GetTicksNS();
    }

    void sp_execute() {
        // TODO: execute the command
        uint8_t version = cmd_buffer.cmd[0];
        pdblock_cmd_v2 *c = (pdblock_cmd_v2 *)cmd_buffer.cmd;
        uint32_t cb_addr = c->cmd_blk_hi << 8 | c->cmd_blk_lo;
        uint8_t cmdnum = mmu->read(cb_addr);

        cmd_buffer.error = 0x00;
        cmd_buffer.status1 = 0x00;
        cmd_buffer.status2 = 0x00;

        if (cmdnum & 0x40) {
            // extended command
            switch (cmdnum) {
    
            }
        } else {
            // standard command
            // default status codes
            uint16_t clist_addr = mmu->read(cb_addr + 1) | (mmu->read(cb_addr + 2) << 8);
            switch (cmdnum) {
                case 0x00: { // Status (pg 122)
                        
                        sp_cmd_status_st cmdlist;
                        read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                        uint32_t slptr = cmdlist.status_p_1 << 8 | cmdlist.status_p_0;
                        switch (cmdlist.status_code) {
                            case 0x00: { // Status 00 Statcode 00 pg 122
                                if (cmdlist.unit == 0) { // SmartPort Driver Status (pg 125)
                                    sp_cmd0_statcode_00_driver s;
                                    s.num_devices = PDB3_MAX_UNITS;
                                    memset(s.reserved, 0x00, sizeof(s.reserved));
                                    write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                                    cmd_buffer.status1 = sizeof(s);
                                } else if (cmdlist.unit > PDB3_MAX_UNITS) {
                                    cmd_buffer.error = 0x21; // invalid unit
                                    break;
                                } else if (drives[cmdlist.unit-1].media == nullptr)  { // drive offline.
                                    /* GS/OS wants the full response back, not just an error */
                                    sp_cmd0_statcode_00 s;
                                    s.status = 0b1110'0000; // device offline.
                                    s.blk_count_0 = 0x00;
                                    s.blk_count_1 = 0x00;
                                    s.blk_count_2 = 0x00;
                                    write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                                    cmd_buffer.status1 = sizeof(s);
                                } else { // drive online
                                    sp_cmd0_statcode_00 s;
                                    uint32_t blkcount = drives[cmdlist.unit-1].media->block_count;
                                    if (blkcount == 0x1'0000) { // special case nonsense for 32MB drives
                                        blkcount = 0xFFFF;
                                    }
                                    bool wp = drives[cmdlist.unit-1].media->write_protected;
                                    s.status = wp ? 0b1011'0100 : 0b1111'0000;
                                    s.blk_count_0 = blkcount & 0xFF;
                                    s.blk_count_1 = (blkcount >> 8) & 0xFF;
                                    s.blk_count_2 = (blkcount >> 16) & 0xFF;
                                    write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                                    cmd_buffer.status1 = sizeof(s);
                                }
                                break; //  return device status
                            }
                            case 0x01: cmd_buffer.error = 0x21; break; //  return device control block
                            case 0x02: cmd_buffer.error = 0x21; break; // return newline status
                            case 0x03: {
                                if (cmdlist.unit == 0 || cmdlist.unit > PDB3_MAX_UNITS) {
                                    cmd_buffer.error = 0x21; // invalid unit
                                    break;
                                }
                                if (drives[cmdlist.unit-1].media == nullptr)  {
                                    cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE; // device offline
                                    return;
                                }
                                sp_cmd0_statcode_03 s;
                                uint32_t blkcount = drives[cmdlist.unit-1].media->block_count;
                                if (blkcount == 0x1'0000) { // special case nonsense for 32MB drives
                                    blkcount = 0xFFFF;
                                }
                                bool wp = drives[cmdlist.unit-1].media->write_protected;
                                s.status = wp ? 0b1011'0100 : 0b1111'0000;
                                s.blk_count_0 = blkcount & 0xFF;
                                s.blk_count_1 = (blkcount >> 8) & 0xFF;
                                s.blk_count_2 = (blkcount >> 16) & 0xFF;

                                // id string is ".pdblock3" plus a letter corresponding to the unit (a-...)
                                s.id_str_length = 9;
                                memcpy(s.id_str, "PDBLOCK3        ", 16);
                                s.id_str[8] = 'A' + cmdlist.unit - 1;
                                s.device_type = 0x02; // nonremovable hard disk
                                //s.device_subtype = 0b0010'0000; // supports extended smartport = no; no disk-switch errors; no removable media
                                s.device_subtype = 0; // test to match virtualII 
                                s.version_1 = 0x03; // version 3
                                s.version_0 = 0x00; // .0
                                
                                write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                                cmd_buffer.status1 = sizeof(s);
                                break; // return device information block (DIB)
                            }
                            default: cmd_buffer.error = 0x21; break; // BADCTL Invalid Status Code
                        }
                    }
                    break;
                case 0x01: { // ReadBlock
                        sp_cmd_rw_st cmdlist;
                        read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                        if (cmdlist.unit == 0 || cmdlist.unit > PDB3_MAX_UNITS) {
                            cmd_buffer.error = 0x21;
                            break;
                        }
                        uint32_t block = cmdlist.block_2 << 16 | cmdlist.block_1 << 8 | cmdlist.block_0;
                        uint16_t addr = cmdlist.addr_hi << 8 | cmdlist.addr_lo;
                        read_block(cmdlist.unit - 1, block, addr);
                    }
                    break;
                case 0x02: { // WriteBlock 
                        sp_cmd_rw_st cmdlist;
                        read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                        if (cmdlist.unit == 0 || cmdlist.unit > PDB3_MAX_UNITS) {
                            cmd_buffer.error = 0x21;
                            break;
                        }
                        uint32_t block = cmdlist.block_2 << 16 | cmdlist.block_1 << 8 | cmdlist.block_0;
                        uint16_t addr = cmdlist.addr_hi << 8 | cmdlist.addr_lo;
                        write_block(cmdlist.unit - 1, block, addr);
                    }
                    break;
                case 0x03:  // Format 
                    // TODO: format the drive. well basically just return "success".
                    assert(false); // not implemented
                    break;
                case 0x04: // Control
                    // TODO: control the drive. well basically just return "success".
                    //assert(false); // not implemented GS/OS going to P8 triggers this.
                    break;
                default: 
                    cmd_buffer.error = 0x21;
                    break;
                
            }

        }
    }
    
    /* Version 1 command - ProDOS Block Device */
    void pd_execute() {
        uint8_t cmd, dev, unit, slot, drive;
        uint16_t block, addr;
    
        if (DEBUG(DEBUG_PD_BLOCK)) print_cmdbuffer();
    
        uint8_t cksum = 0;
        for (int i = 0; i < cmd_buffer.index; i++) {
            cksum ^= cmd_buffer.cmd[i];
        }
        if (cksum != 0x00) {
            if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pd_execute: Checksum error" << std::endl;
            cmd_buffer.error = 0x01;
            return;
        }
    
        // Version 1 command
        pdblock_cmd_v1 *cmdbuf = (pdblock_cmd_v1 *)cmd_buffer.cmd;
        cmd = cmdbuf->cmd;
        dev = cmdbuf->dev;
        block = cmdbuf->block_lo | (cmdbuf->block_hi << 8);
        addr = cmdbuf->addr_lo | (cmdbuf->addr_hi << 8);        
        slot = (dev >> 4) & 0b0111;
        drive = (dev >> 7) & 0b1;
    
        if (slot != (uint8_t)_slot) {
            cmd_buffer.error = PD_ERROR_IO;
            return;
        }
    
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pdblock2_execute: Unit: " << std::hex << (int)unit 
            << ", Block: " << std::hex << (int)block << ", Addr: " << std::hex << (int)addr << ", CMD: " 
            << std::hex << (int)cmd << std::endl;
    
        uint8_t st = internal_status(drive);
        if (st) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return;
        }
        if (cmd == 0x00) {
            media_descriptor *media = drives[drive].media;
            if (media == nullptr) {
                cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
                return;
            }
            cmd_buffer.error = 0x00;
            cmd_buffer.status1 = media->block_count & 0xFF;
            cmd_buffer.status2 = (media->block_count >> 8) & 0xFF;
        } else if (cmd == 0x01) {
            read_block(drive, block, addr);
            cmd_buffer.error = 0x00;
            cmd_buffer.status1 = 0x00;
            cmd_buffer.status2 = 0x00;
        } else if (cmd == 0x02) {
            write_block(drive, block, addr);
            cmd_buffer.error = 0x00;
            cmd_buffer.status1 = 0x00;
            cmd_buffer.status2 = 0x00;
        } else if (cmd == 0x03) { // not implemented
            cmd_buffer.error = PD_ERROR_NO_DEVICE;
        }
    }
        
    /* Implementations of the StorageDevice interface */

    bool mount(storage_key_t key, media_descriptor *media) {
        if (key.drive >= PDB3_MAX_UNITS) return false;
        //if (DEBUG(DEBUG_PD_BLOCK)) printf("Mounting ProDOS block device %s slot %d drive %d\n", media->filename, slot, drive);
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "Mounting PDB3 device " << media->filename << " slot: " << _slot << " drive " << key.drive << std::endl;
    
        const char *mode = media->write_protected ? "rb" : "r+b";
        FILE *fp = fopen(media->filename.c_str(), mode);
        if (fp == nullptr) {
            std::cerr << "Could not open PDB3 device file: " << media->filename << std::endl;
            return false;
        }
        drives[key.drive].file = fp;
        drives[key.drive].media = media;
        return true;
    }
    
    bool unmount(storage_key_t key) {
        if (key.drive >= PDB3_MAX_UNITS) return true;
        if (drives[key.drive].file) {
            fclose(drives[key.drive].file);
            drives[key.drive].file = nullptr;
            drives[key.drive].media = nullptr;
        }
        return true;
    }

    bool writeback(storage_key_t key) {
        return true;
    }

    drive_status_t status(storage_key_t key) {
        if (key.drive >= PDB3_MAX_UNITS) return {false, "", false, 0, false};
        media_t seldrive = drives[key.drive];

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

    void print_cmdbuffer() {
        std::cout << "PD3_CMD_BUFFER: ";
        for (int i = 0; i < cmd_buffer.index; i++) {
            std::cout << std::hex << (int)cmd_buffer.cmd[i] << " ";
        }
        std::cout << std::endl;
    }
    
    void reset() {
        cmd_buffer.index = 0;
    }
    void put(uint8_t data) {
        if (cmd_buffer.index < MAX_PD_BUFFER_SIZE) {
            cmd_buffer.cmd[cmd_buffer.index] = data;
            cmd_buffer.index++;
        }
    }

    /* Execute command - inspect command buffer version and dispatch to appropriate handler */

    void execute() {
        uint8_t version = cmd_buffer.cmd[0];
        if (version == 0x01) {
            pd_execute();
        } else if (version == 0x02) {
            sp_execute();
        } else {
            // TODO: return some kind of error
            if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "pdblock3_execute: Version not supported" << std::endl;
            cmd_buffer.error = 0x01;
        }
        reset();
    }

    /* Getters for command status */
    uint8_t get_error() {
        return cmd_buffer.error;
    }
    uint8_t get_status1() {
        return cmd_buffer.status1;
    }
    uint8_t get_status2() {
        return cmd_buffer.status2;
    }

};

void pdblock3_write_C0x0(void *context, uint32_t addr, uint8_t data) {
    pdblock3_data * pdblock_d = (pdblock3_data *)context;
    uint16_t adr = addr & 0xF;

    if (adr == 0x00) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_CMD_RESET: " << std::hex << (int)data << std::endl;
        pdblock_d->pdb->reset();
    } else if (adr == 0x01) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_CMD_PUT: " << std::hex << (int)data << std::endl;
        pdblock_d->pdb->put(data);
    } else if (adr == 0x02) {
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_CMD_EXECUTE: " << std::hex << (int)data << std::endl;
        pdblock_d->pdb->execute();
    } 
}

uint8_t pdblock3_read_C0x0(void *context, uint32_t addr) {
    pdblock3_data * pdblock_d = (pdblock3_data *)context;
    uint8_t val;

    if ((addr & 0xF) == 0x03) {
        val = pdblock_d->pdb->get_error();
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_ERROR_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else if ((addr & 0xF) == 0x04) {
        val = pdblock_d->pdb->get_status1();
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_STATUS1_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else if ((addr & 0xF) == 0x05) {
        val = pdblock_d->pdb->get_status2();
        if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "PD3_STATUS2_GET: " << std::hex << (int)val << std::endl;
        return val;
    } else return 0xE0;
}


void map_rom_pdblock3(void *context, SlotType_t slot) {
    pdblock3_data * pdblock_d = (pdblock3_data *)context;

    uint8_t *dp = pdblock_d->rom->get_data();
    for (uint8_t page = 0; page < 8; page++) {
        pdblock_d->megaii->map_c1cf_page_read_only(page + 0xC8, dp + (page * 0x100), "PDB3_ROM");
    }
    if (DEBUG(DEBUG_PD_BLOCK)) {
        printf("mapped in PDB3 $C800-$CFFF\n");
    }
}

void init_pdblock3(computer_t *computer, SlotType_t slot)
{
    if (DEBUG(DEBUG_PD_BLOCK)) std::cout << "Initializing PDB3 slot " << slot << std::endl;
    pdblock3_data * pdblock_d = new pdblock3_data;
    pdblock_d->id = DEVICE_ID_PD_BLOCK2;
    
    pdblock_d->mmu = computer->cpu->mmu;
    pdblock_d->megaii = computer->mmu; // these could be the same (iie) or different (iigs)
    pdblock_d->_slot = slot;

    pdblock_d->rom = new ResourceFile("roms/cards/pdblock3/pdblock3.rom", READ_ONLY);
    if (pdblock_d->rom == nullptr) {
        std::cerr << "Failed to load pdblock3.rom" << std::endl;
        return;
    }
    pdblock_d->rom->load();

    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = (uint8_t *)(pdblock_d->rom->get_data());

    // register slot ROM
    computer->mmu->set_slot_rom(slot, rom_data, "PDBLK_ROM");

    // register drives with mounts for status reporting
    PDBlock3 *pd3 = new PDBlock3(slot, pdblock_d->mmu);
    pdblock_d->pdb = pd3;

    storage_key_t key;
    key.slot = (uint16_t)slot;
    key.drive = 0;
    key.partition = 0;
    key.subunit = 0;
    computer->mounts->register_storage_device(key, pd3, DRIVE_TYPE_PRODOS_BLOCK);

    key.drive = 1;
    computer->mounts->register_storage_device(key, pd3, DRIVE_TYPE_PRODOS_BLOCK);

    // register.. uh, registers.
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_RESET, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_PUT, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_EXECUTE, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_ERROR_GET, { pdblock3_read_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS1_GET, { pdblock3_read_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS2_GET, { pdblock3_read_C0x0, pdblock_d });
    
    computer->mmu->set_C8xx_handler(slot, map_rom_pdblock3, pdblock_d);

}

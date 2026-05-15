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


class PDBlock3; // forward declaration

struct pdblock3_data: public SlotData {
    ResourceFile *rom;
    MMU *mmu;
    MMU_II *megaii;
    PDBlock3 *pdb;
};

class PDBlock3 : public StorageDevice {
private:
    MMU *mmu;
    pdblock_cmd_buffer cmd_buffer;
    bool disk_switched[PDB3_MAX_UNITS];
    media_t drives[PDB3_MAX_UNITS];
    uint8_t _slot;

public:
    PDBlock3(uint8_t slot, MMU *mmu) : _slot(slot), mmu(mmu) {
        for (int j = 0; j < PDB3_MAX_UNITS; j++) {
            drives[j].file = nullptr;
            drives[j].media = nullptr;
            disk_switched[j] = false;
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
    // drive is unit 0-based
    void read_block(uint8_t drive, uint32_t block, uint32_t addr) {
        uint8_t block_buffer[512];
        if (!check_valid_unit(drive)) return;
        if (!check_online(drive)) return;

        FILE *fp = drives[drive].file;
        media_descriptor *media = drives[drive].media;

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
        if (!check_valid_unit(drive)) return;
        if (!check_online(drive)) return;

        FILE *fp = drives[drive].file;
        media_descriptor *media = drives[drive].media;

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

    struct DriveInfo { uint8_t status; uint32_t blk_count; };

    DriveInfo get_drive_info(uint8_t unit_index) {
        uint32_t blkcount = drives[unit_index].media->block_count;
        if (blkcount == 0x1'0000) blkcount = 0xFFFF; // 32 MB quirk
        bool wp = drives[unit_index].media->write_protected;
        uint8_t st = wp ? 0b1011'0100 : 0b1111'0000;
        if (disk_switched[unit_index]) {
            st |= 0b0000'0001;
            disk_switched[unit_index] = false;
        }
        return {st, blkcount};
    }

    // 0-based unit (already had 1 subtracted)
    bool check_valid_unit(uint8_t unit) {
        if (unit >= PDB3_MAX_UNITS) {
            cmd_buffer.error = PD_ERROR_NO_DEVICE; // invalid unit
            return false;
        }
        return true;
    }
    bool check_unit_nonzero(uint8_t unit) {
        if (unit == 0) {
            cmd_buffer.error = PD_ERROR_NO_DEVICE; // invalid unit
            return false;
        }
        return true;
    }

    bool check_online(uint8_t unit) {
        if (drives[unit].file == nullptr || drives[unit].media == nullptr) {
            cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
            return false;
        }
        return true;
    }

    // Single implementation for both standard and extended SmartPort commands.
    // CmdStatus / CmdRW / CmdControl select the right command-list struct;
    // Stat00 / Stat03 select the right response struct (3-byte vs 4-byte blk_count).
    template<typename CmdStatus, typename CmdRW, typename CmdControl,
             typename Stat00,    typename Stat03>
    void sp_execute_impl(uint8_t cmdnum, uint32_t clist_addr) {
        switch (cmdnum) {
            case 0x00: { // Status (pg 122)
                CmdStatus cmdlist;
                read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                uint32_t slptr = cmdlist.status_p;
                switch (cmdlist.status_code) {
                    case 0x00: { // device status (pg 122)
                        if (cmdlist.unit == 0) { // SmartPort Driver Status (pg 125)
                            sp_cmd0_statcode_00_driver s;
                            s.num_devices = PDB3_MAX_UNITS;
                            memset(s.reserved, 0x00, sizeof(s.reserved));
                            write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                            cmd_buffer.status1 = sizeof(s);
                        } else if (cmdlist.unit > PDB3_MAX_UNITS) {
                            cmd_buffer.error = 0x21; // invalid unit
                            break;
                        } else if (drives[cmdlist.unit-1].media == nullptr) { // drive offline
                            /* GS/OS wants the full response back, not just an error */
                            Stat00 s;
                            s.status    = 0b1110'0000; // device offline
                            s.blk_count = 0;
                            write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                            cmd_buffer.status1 = sizeof(s);
                        } else { // drive online
                            Stat00 s;
                            auto [st, blkcount] = get_drive_info(cmdlist.unit - 1);
                            s.status    = st;
                            s.blk_count = blkcount;
                            write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                            cmd_buffer.status1 = sizeof(s);
                        }
                        break;
                    }
                    case 0x01: cmd_buffer.error = 0x21; break; // return device control block
                    case 0x02: cmd_buffer.error = 0x21; break; // return newline status
                    case 0x03: { // device information block (DIB)
                        uint8_t effunit = cmdlist.unit - 1;
                        if (!check_valid_unit(effunit)) break;
                        if (!check_online(effunit)) break;

                        Stat03 s;
                        auto [st, blkcount] = get_drive_info(effunit);
                        s.status    = st;
                        s.blk_count = blkcount;
                        s.id_str_length = 9;
                        memcpy(s.id_str, "BAZFAST3        ", 16);
                        s.id_str[8]      = 'A' + effunit;
                        s.device_type    = 0x02;        // hard disk
                        s.device_subtype = 0b1100'0000; // extended smartport + disk-switch errors
                        s.version_1      = 0x03;        // version 3
                        s.version_0      = 0x00;        // .0
                        write_to_memory(slptr, (uint8_t *)&s, sizeof(s));
                        cmd_buffer.status1 = sizeof(s);
                        break;
                    }
                    default: cmd_buffer.error = 0x21; break; // BADCTL
                }
                break;
            }
            case 0x01: { // ReadBlock
                CmdRW cmdlist;
                read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                if (!check_unit_nonzero(cmdlist.unit)) break;
                read_block(cmdlist.unit - 1, cmdlist.block, cmdlist.addr);
                break;
            }
            case 0x02: { // WriteBlock
                CmdRW cmdlist;
                read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                if (!check_unit_nonzero(cmdlist.unit)) break;
                write_block(cmdlist.unit - 1, cmdlist.block, cmdlist.addr);
                break;
            }
            case 0x03: // Format
                assert(false); // not implemented
                break;
            case 0x04: { // Control / Eject
                /* SmartPort TN #2: Before May 1988, control code $04 was device-specific.
                   It is now defined as EJECT. Devices without removable media return success. */
                CmdControl cmdlist;
                read_from_memory(clist_addr, (uint8_t *)&cmdlist, sizeof(cmdlist));
                if (!check_unit_nonzero(cmdlist.unit)) break;
                if (!check_valid_unit(cmdlist.unit)) break;
                if (cmdlist.code == 0x04) {
                    storage_key_t key;
                    key.drive = cmdlist.unit - 1;
                    key.slot  = _slot;
                    unmount(key);
                }
                break;
            }
            default:
                cmd_buffer.error = 0x21;
                break;
        }
    }

    void sp_execute(uint8_t version, pdblock_cmd_v2 *c, uint8_t cmdnum, uint32_t cb_addr) {
        cmd_buffer.error = cmd_buffer.status1 = cmd_buffer.status2 = 0x00;
        uint32_t clist_addr = mmu->read(cb_addr + 1) | (mmu->read(cb_addr + 2) << 8);
        sp_execute_impl<sp_cmd_status_st, sp_cmd_rw_st, sp_cmd_control_st,
                        sp_cmd0_statcode_00, sp_cmd0_statcode_03>(cmdnum, clist_addr);
    }

    void sp_execute_extended(uint8_t version, pdblock_cmd_v2 *c, uint8_t cmdnum, uint32_t cb_addr) {
        cmd_buffer.error = cmd_buffer.status1 = cmd_buffer.status2 = 0x00;
        uint32_t clist_addr = mmu->read(cb_addr + 1) | (mmu->read(cb_addr + 2) << 8) | (mmu->read(cb_addr + 3) << 16);
        sp_execute_impl<sp_cmd_status_ex, sp_cmd_rw_ex, sp_cmd_control_ex,
                        sp_cmd0_statcode_00_ex, sp_cmd0_statcode_03_ex>(cmdnum & 0x3F, clist_addr);
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
        block = cmdbuf->block;
        addr = cmdbuf->addr;
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
        cmd_buffer.error = 0x00;
        cmd_buffer.status1 = 0x00;
        cmd_buffer.status2 = 0x00;
        if (cmd == 0x00) {
            media_descriptor *media = drives[drive].media;
            if (media == nullptr) {
                cmd_buffer.error = PD_ERROR_DEVICE_OFFLINE;
                return;
            }
            // TODO: check and handle disk switched? 
            cmd_buffer.status1 = media->block_count & 0xFF;
            cmd_buffer.status2 = (media->block_count >> 8) & 0xFF;
        } else if (cmd == 0x01) {
            read_block(drive, block, addr);
        } else if (cmd == 0x02) {
            write_block(drive, block, addr);
        } else if (cmd == 0x03) { // not implemented
            cmd_buffer.error = PD_ERROR_NO_DEVICE;
        }
    }
        
    /* Implementations of the StorageDevice interface */

    bool mount(storage_key_t key, std::vector<media_descriptor *> media_list) {
        if (media_list.size() > 1) return false;
        media_descriptor *media = media_list[0];

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
        disk_switched[key.drive] = true;
        return true;
    }
    
    bool unmount(storage_key_t key) {
        if (key.drive >= PDB3_MAX_UNITS) return true;
        if (drives[key.drive].file) {
            fclose(drives[key.drive].file);
            drives[key.drive].file = nullptr;
            drives[key.drive].media = nullptr;
            disk_switched[key.drive] = true;
        }
        return true;
    }

    bool writeback(storage_key_t key) {
        return true;
    }

    drive_status_t status(storage_key_t key) {
        if (key.drive >= PDB3_MAX_UNITS || drives[key.drive].media == nullptr) return {false, "", false, 0, false, false};
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
        
        return {mounted, fname, motor, seldrive.last_block_accessed, seldrive.media->write_protected};
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
            pdblock_cmd_v2 *c = (pdblock_cmd_v2 *)cmd_buffer.cmd;
            uint32_t cb_addr = c->cmd_blk;
            uint8_t cmdnum = mmu->read(cb_addr);
            if (cmdnum & 0x40) sp_execute_extended(version, c, cmdnum, cb_addr);
            else sp_execute(version, c, cmdnum, cb_addr);
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
    for (key.drive = 0; key.drive < 6; key.drive++) {
        computer->mounts->register_storage_device(key, pd3, DRIVE_TYPE_PRODOS_BLOCK);
    }

    // register.. uh, registers.
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_RESET, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_PUT, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_write_handler((slot * 0x10) + PD_CMD_EXECUTE, { pdblock3_write_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_ERROR_GET, { pdblock3_read_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS1_GET, { pdblock3_read_C0x0, pdblock_d });
    computer->mmu->set_C0XX_read_handler((slot * 0x10) + PD_STATUS2_GET, { pdblock3_read_C0x0, pdblock_d });
    
    computer->mmu->set_C8xx_handler(slot, map_rom_pdblock3, pdblock_d);

}

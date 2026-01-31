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

#include <time.h>
#include <cstdint>
#include <stdio.h>

#include "debug.hpp"

#include "cpu.hpp"

#include "thunderclockplus.hpp"

#include "util/ResourceFile.hpp"

/*

https://retrocomputing.stackexchange.com/questions/5512/is-the-apple-ii-no-slot-clock-compatible-with-the-thunderclock

*/

#define THUNDERCLOCK_CMD_REG_BASE 0xC080

#define TCP_IN   0b00000001
#define TCP_CLK  0b00000010
#define TCP_STB  0b00000100
#define TCP_CMD  0b00111000
#define TCP_INTR 0b01000000
#define TCP_INTR_ASSERT 0b00100000
#define TCP_OUT  0b10000000

#define TCP_CMD_READ_TIME 0b00011000
#define TCP_CMD_SET_TIME *unknown*

#define HZ64 64
#define HZ256 256
#define HZ1024 1024

uint8_t thunderclock_command_register = 0;
uint64_t thunderclock_time_register = 0;
uint8_t thunderclock_interrupt_asserted = 0;
uint8_t thunderblock_interrupt_rate = HZ64;


// Returns 40 bits of time data in Thunderclock Plus format
// the LSB of our 40-bit register is the LSB of the seconds-units field.
uint64_t get_thunderclock_time() {
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    
    // First collect nibbles in order
    uint8_t nibbles[10] = {
        (uint8_t)(tm->tm_mon + 1),    // month (1-12 binary)
        (uint8_t)tm->tm_wday,         // day of week (0-6 BCD)
        (uint8_t)(tm->tm_mday / 10),  // date tens
        (uint8_t)(tm->tm_mday % 10),  // date units
        (uint8_t)(tm->tm_hour / 10),  // hour tens
        (uint8_t)(tm->tm_hour % 10),  // hour units
        (uint8_t)(tm->tm_min / 10),   // minute tens
        (uint8_t)(tm->tm_min % 10),   // minute units
        (uint8_t)(tm->tm_sec / 10),   // second tens
        (uint8_t)(tm->tm_sec % 10)    // second units
    };

    // Pack nibbles into result, LSB first
    uint64_t result = 0;
    for (int i = 0; i < 10; i++) {
        result = (result << 4) | (nibbles[i] & 0x0F);
    }
    
    return result;
}

/**
 * The first bit we fetch is the LSB of the seconds-units field. 
 */
uint8_t thunderclock_read_register(void *context, uint32_t address) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (address - 0xC080) >> 4;
    thunderclock_state * thunderclock_d = (thunderclock_state *)get_slot_state(cpu, (SlotType_t)slot);

    fprintf(stderr, "Thunderclock Plus read register %04X => %02X\n", address, thunderclock_command_register);
    
    uint8_t bit = (thunderclock_time_register & 0x01) << 7;
    uint8_t reg = thunderclock_command_register;
    reg = (reg & (~TCP_OUT)) | bit;
    return reg;
}

void thunderclock_write_register(void *context, uint32_t address, uint8_t value) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (address - 0xC080) >> 4;
    thunderclock_state * thunderclock_d = (thunderclock_state *)get_slot_state(cpu, (SlotType_t)slot);
    fprintf(stderr, "Thunderclock Plus write register %X value %X\n", address, value);
    // check for strobe HI to LO transition. Then perform commmand.
    if ((thunderclock_command_register & TCP_STB) && ((value & TCP_STB) == 0)) {
        // read the command register.
        if ((value & TCP_CMD) == TCP_CMD_READ_TIME) {
            thunderclock_time_register = get_thunderclock_time();
            fprintf(stderr, "Thunderclock Plus read time: %llX\n", thunderclock_time_register);
        }
    }
    if ((thunderclock_command_register & TCP_CLK) && ((value & TCP_CLK) == 0)) {
        // shift the time register right on a 1 to 0 transition of the clock bit.
        fprintf(stderr, "Thunderclock Plus CLK tick - shift time right\n");
        thunderclock_time_register >>= 1;
    }

    // remember the value.
    thunderclock_command_register = value;

}

void map_rom_thunderclock(void *context, SlotType_t slot) {
    cpu_state *cpu = (cpu_state *)context;
    thunderclock_state * thunderclock_d = (thunderclock_state *)get_slot_state(cpu, slot);
    uint8_t *dp = thunderclock_d->rom->get_data();
    for (uint8_t page = 0; page < 8; page++) {
        cpu->mmu->map_page_read_only(page + 0xC8, dp + (page * 0x100), "TCP_ROM");
    }
    if (DEBUG(DEBUG_THUNDERCLOCK)) {
        printf("mapped in thunderclock $C800-$CFFF\n");
    }
}

void init_slot_thunderclock(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    uint16_t thunderclock_cmd_reg = THUNDERCLOCK_CMD_REG_BASE + (slot << 4);
    fprintf(stderr, "Thunderclock Plus init at SLOT %d address %X\n", slot, thunderclock_cmd_reg);

    thunderclock_state * thunderclock_d = new thunderclock_state;
    thunderclock_d->id = DEVICE_ID_THUNDER_CLOCK;
    set_slot_state(cpu, slot, thunderclock_d);

    ResourceFile *rom = new ResourceFile("roms/cards/tcp/tcp.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load tcp.rom\n");
        return;
    }
    rom->load();
    thunderclock_d->rom = rom;

    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = thunderclock_d->rom->get_data();

    // load the firmware into the slot memory -- refactor this
    /* for (int i = 0; i < 256; i++) {
        raw_memory_write(cpu, 0xC000 + (slot * 0x0100) + i, rom_data[i]);
    } */
    computer->mmu->set_slot_rom(slot, rom_data, "TCP_ROM");

    computer->mmu->set_C0XX_read_handler(thunderclock_cmd_reg, { thunderclock_read_register, cpu });
    computer->mmu->set_C0XX_write_handler(thunderclock_cmd_reg, { thunderclock_write_register, cpu });

    computer->mmu->set_C8xx_handler(slot, map_rom_thunderclock, cpu );


    /* register_C0xx_memory_read_handler(thunderclock_cmd_reg, thunderclock_read_register);
    register_C0xx_memory_write_handler(thunderclock_cmd_reg, thunderclock_write_register);

    register_C8xx_handler(cpu, slot, map_rom_thunderclock); */
}

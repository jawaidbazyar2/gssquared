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

#include "gs2.hpp"
#include "cpu.hpp"
#include "memexp.hpp"
#include "debug.hpp"

void memexp_write_C0x0(void *context, uint32_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    uint8_t old_lo = memexp_d->addr_low;
    uint8_t old_med = memexp_d->addr_med;

    memexp_d->addr_low = data;
    if (((old_lo & 0x80) != 0) && ((data & 0x80) == 0)) {
        memexp_d->addr_med++;
        if (((old_med & 0x80) != 0) && ((memexp_d->addr_med & 0x80) == 0)) {
            memexp_d->addr_high++;
        }        
    }
}

void memexp_write_C0x1(void *context, uint32_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);
    
    uint8_t old_lo = memexp_d->addr_low;
    uint8_t old_med = memexp_d->addr_med;
    
    memexp_d->addr_med = data;
    if (((old_med & 0x80) != 0) && ((memexp_d->addr_med & 0x80) == 0)) {
        memexp_d->addr_high++;
    }
}

void memexp_write_C0x2(void *context, uint32_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    memexp_d->addr_high = data;
}

uint8_t memexp_read_C0x0(void *context, uint32_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    return memexp_d->addr_low;
}

uint8_t memexp_read_C0x1(void *context, uint32_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    return memexp_d->addr_med;
}

uint8_t memexp_read_C0x2(void *context, uint32_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    return memexp_d->addr_high | 0xF0; // hi nybble here is always 0xF if card has 1MB or less.
}

uint8_t memexp_read_C0x3(void *context, uint32_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);

    uint8_t data = memexp_d->data[memexp_d->addr];
    if (DEBUG(DEBUG_MEMEXP)) {
        printf("memexp_read_C0x3 %x => %x\n", memexp_d->addr, data);
    }
    memexp_d->addr++;
    return data;
}

void memexp_write_C0x3(void *context, uint32_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    uint8_t slot = (addr - 0xC080) >> 4;
    memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, (SlotType_t)slot);
    memexp_d->data[memexp_d->addr] = data;
    if (DEBUG(DEBUG_MEMEXP)) {
        printf("memexp_write_C0x3 %x => %x\n", data, memexp_d->addr);
    }
    memexp_d->addr++;
}

void map_rom_memexp(void *context, SlotType_t slot) {
    //cpu_state *cpu = (cpu_state *)context;
    //memexp_data * memexp_d = (memexp_data *)get_module_state(cpu, MODULE_MEMEXP);
    //memexp_data * memexp_d = (memexp_data *)get_slot_state(cpu, slot);
    memexp_data * memexp_d = (memexp_data *)context;

    uint8_t *dp = memexp_d->rom->get_data();
    for (uint8_t page = 0; page < 8; page++) {
        //memexp_d->mmu->map_page_read_only(page + 0xC8, dp + 0x800 + (page * 0x100), "MEMEXP_ROM");
        memexp_d->mmu->map_c1cf_page_read_only(page + 0xC8, dp + 0x800 + (page * 0x100), "MEMEXP_ROM");
    }
    if (DEBUG(DEBUG_MEMEXP)) {
        printf("mapped in memexp $C800-$CFFF\n");
    }
}

void init_slot_memexp(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    memexp_data * memexp_d = new memexp_data;
    memexp_d->mmu = computer->mmu;

    // set in CPU so we can reference later
    memexp_d->id = DEVICE_ID_MEM_EXPANSION;
    memexp_d->data = new uint8_t[MEMEXP_SIZE];
    memexp_d->addr = 0;

    ResourceFile *rom = new ResourceFile("roms/cards/memexp/memexp.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load memexp.rom\n");
        return;
    }
    rom->load();
    memexp_d->rom = rom;

    set_slot_state(cpu, slot, memexp_d);

    fprintf(stdout, "init_slot_memexp %d\n", slot);

    uint16_t slot_base = 0xC080 + (slot * 0x10);

    computer->mmu->set_C0XX_write_handler(slot_base + MEMEXP_ADDR_LOW, { memexp_write_C0x0, cpu });
    computer->mmu->set_C0XX_write_handler(slot_base + MEMEXP_ADDR_MED, { memexp_write_C0x1, cpu });
    computer->mmu->set_C0XX_write_handler(slot_base + MEMEXP_ADDR_HIGH, { memexp_write_C0x2, cpu });
    
    computer->mmu->set_C0XX_read_handler(slot_base + MEMEXP_ADDR_LOW, { memexp_read_C0x0, cpu });
    computer->mmu->set_C0XX_read_handler(slot_base + MEMEXP_ADDR_MED, { memexp_read_C0x1, cpu });
    computer->mmu->set_C0XX_read_handler(slot_base + MEMEXP_ADDR_HIGH, { memexp_read_C0x2, cpu });
    
    computer->mmu->set_C0XX_write_handler(slot_base + MEMEXP_DATA, { memexp_write_C0x3, cpu });
    computer->mmu->set_C0XX_read_handler(slot_base + MEMEXP_DATA, { memexp_read_C0x3, cpu });

    /* register_C0xx_memory_write_handler(slot_base + MEMEXP_ADDR_LOW, memexp_write_C0x0);
    register_C0xx_memory_write_handler(slot_base + MEMEXP_ADDR_MED, memexp_write_C0x1);
    register_C0xx_memory_write_handler(slot_base + MEMEXP_ADDR_HIGH, memexp_write_C0x2);
    
    register_C0xx_memory_read_handler(slot_base + MEMEXP_ADDR_LOW, memexp_read_C0x0);
    register_C0xx_memory_read_handler(slot_base + MEMEXP_ADDR_MED, memexp_read_C0x1);
    register_C0xx_memory_read_handler(slot_base + MEMEXP_ADDR_HIGH, memexp_read_C0x2);
    
    register_C0xx_memory_write_handler(slot_base + MEMEXP_DATA, memexp_write_C0x3);
    register_C0xx_memory_read_handler(slot_base + MEMEXP_DATA, memexp_read_C0x3); */
    
    // memory-map the page. Refactor to have a method to get and set memory map.
    uint8_t *rom_data = memexp_d->rom->get_data();
    /* memory_map_page_both(cpu, slot + 0xC0, rom_data + (slot*GS2_PAGE_SIZE), MEM_ROM); */

    computer->mmu->set_slot_rom(slot, rom_data+(slot * 0x0100), "MEMX_ROM");

    // load the firmware into the slot memory -- refactor this
/*     for (int i = 0; i < 256; i++) {
        raw_memory_write(cpu, 0xC000 + (slot * 0x0100) + i, rom_data[i + (slot * 0x100)]);
    }
 */
    /* register_C8xx_handler(cpu, slot, map_rom_memexp); */
    computer->mmu->set_C8xx_handler(slot, map_rom_memexp, memexp_d);
}
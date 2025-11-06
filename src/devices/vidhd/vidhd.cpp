
#include "computer.hpp"

#include "vidhd.hpp"


void init_slot_vidhd(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;

    vidhd_data *vidhd_d = new vidhd_data();
    vidhd_d->computer = computer;
    vidhd_d->cpu = cpu;

    vidhd_d->rom[0] = 0x24;
    vidhd_d->rom[1] = 0xEA;
    vidhd_d->rom[2] = 0x4C; // how did the AI know this? wild.
   
    computer->mmu->set_slot_rom(slot, vidhd_d->rom, "VIDHD_ROM");
}
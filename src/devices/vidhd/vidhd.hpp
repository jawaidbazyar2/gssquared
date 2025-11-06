#pragma once

#include "computer.hpp"

struct vidhd_data: public SlotData {
    computer_t *computer = nullptr;
    cpu_state *cpu = nullptr;
    uint8_t *rom = nullptr;

    vidhd_data() {
        id = DEVICE_ID_VIDHD;
        rom = new uint8_t[256];
        memset(rom, 0, 256);
    }
};

void init_slot_vidhd(computer_t *computer, SlotType_t slot);

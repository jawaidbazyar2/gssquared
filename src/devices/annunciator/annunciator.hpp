#pragma once

#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"

struct annunciator_state_t {
    uint8_t annunciators[4] = {0, 0, 0, 0};
    MMU_II *mmu = nullptr;
};

uint8_t read_annunciator(cpu_state *cpu, uint8_t id);
void annunciator_write_C0xx_anc0(cpu_state *cpu, uint16_t addr, uint8_t data);
void init_annunciator(computer_t *computer, SlotType_t slot);

#pragma once

#include "computer.hpp"

#include "mmus/mmu_ii.hpp"
#include "devices/adb/ADB_Micro.hpp"
#include "util/InterruptController.hpp"

struct keygloo_state_t {
    KeyGloo *kg = nullptr;
    MMU_II *mmu = nullptr;
    computer_t *computer = nullptr;
    InterruptController *irq_control = nullptr;
};

void init_slot_keygloo(computer_t *computer, SlotType_t slot);
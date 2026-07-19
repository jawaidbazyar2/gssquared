#pragma once

#include <cstdint>

#include "computer.hpp"
#include "MouseController.hpp"
#include "NClock.hpp"
#include "util/EventTimer.hpp"
#include "util/InterruptController.hpp"
#include "util/ResourceFile.hpp"

struct applemouseiii_state_t : public SlotData {
    computer_t *computer = nullptr;
    NClock *clock = nullptr;
    EventTimer *event_timer = nullptr;
    InterruptController *irq_control = nullptr;
    ResourceFile *rom_file = nullptr;
    uint8_t *rom = nullptr;
    MouseController controller;
    uint64_t vbl_cycle = 0;
    bool vbl_timer_armed = false;
};

void init_applemouseiii(computer_t *computer, SlotType_t slot);

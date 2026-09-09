/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstdint>

#include "computer.hpp"
#include "NClock.hpp"
#include "util/EventTimer.hpp"
#include "util/InterruptController.hpp"
#include "util/ResourceFile.hpp"

struct thunderclock_state : public SlotData {
    computer_t *computer = nullptr;
    NClock *clock = nullptr;
    EventTimer *event_timer = nullptr;
    InterruptController *irq_control = nullptr;
    ResourceFile *rom = nullptr;
    MMU_II *mmu = nullptr;

    uint8_t last_write = 0;
    uint8_t latched_cmd = 0;
    uint64_t shift_reg = 0;
    uint64_t time_counter = 0;
    bool irqen = false;
    bool irq_status = false;
    uint32_t tp_hz = 0;
    bool time_set_active = false;
};

void init_slot_thunderclock(computer_t *computer, SlotType_t slot);

#pragma once

#include "computer.hpp"
#include "SlotData.hpp"

struct iwm_state_t {
    uint8_t status = 0;
    uint8_t control = 0;
    uint8_t data = 0;
};

void init_iwm_slot(computer_t *computer, SlotType_t slot);

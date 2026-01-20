#pragma once

#include "computer.hpp"
#include "SlotData.hpp"

struct scc8530_state_t {
    uint8_t cmd_b = 0;
    uint8_t cmd_a = 0;
    uint8_t data_b = 0;
    uint8_t data_a = 0;
};

void init_scc8530_slot(computer_t *computer, SlotType_t slot);

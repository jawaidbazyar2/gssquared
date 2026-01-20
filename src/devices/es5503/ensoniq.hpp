#pragma once

#include "computer.hpp"
#include "SlotData.hpp"

struct ensoniq_state_t {
    uint8_t soundctl = 0;
    uint8_t sounddata = 0;
    uint8_t soundadrl = 0;
    uint8_t soundadrh = 0;
};

void init_ensoniq_slot(computer_t *computer, SlotType_t slot);

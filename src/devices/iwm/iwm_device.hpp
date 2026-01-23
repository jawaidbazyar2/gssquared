#pragma once

#include <cstdint>
#include "IWM.hpp"

struct iwm_state_t {
    uint8_t status = 0;
    uint8_t control = 0;
    uint8_t data = 0;
    
    IWM *iwm;   
};

void init_iwm_slot(computer_t *computer, SlotType_t slot);

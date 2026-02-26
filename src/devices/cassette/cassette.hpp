#pragma once

#include "gs2.hpp"
#include "computer.hpp"

// This basically does nothing right now. Just return floating bus state.

struct cassette_state_t {
    MMU_II *mmu = nullptr;
};

void init_mb_cassette(computer_t *computer, SlotType_t slot);
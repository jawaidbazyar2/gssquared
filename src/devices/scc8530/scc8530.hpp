#pragma once

#include "computer.hpp"
#include "Z85C30.hpp"
#include "util/InterruptController.hpp"

struct scc8530_state_t {
    Z85C30 *scc;
    FILE *data_file_a = NULL;
    FILE *data_file_b = NULL;
    //char *data_filename = NULL;
    InterruptController *irq_control = nullptr;
};

void init_scc8530_slot(computer_t *computer, SlotType_t slot);

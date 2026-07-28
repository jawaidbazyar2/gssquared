#pragma once

#include "gs2.hpp"
#include "computer.hpp"

#include "serial_devices/SerialDevice.hpp"
#include "util/ResourceFile.hpp"

#define PARALLEL_DEV 0x00

struct parallel_data: public SlotData {
    ResourceFile *rom = nullptr;
    SerialDevice *serial_device = nullptr;
    char port_id[16] = {};
};

void init_slot_parallel(computer_t *computer, SlotType_t slot);
void parallel_reset(parallel_data *parallel_d);

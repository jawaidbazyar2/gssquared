#pragma once

#include <string>

#include "devices/adb/keygloo_state.hpp"
#include "gs2.hpp"

void keygloo_start_paste(keygloo_state_t *kb_state, const std::string &text);
void init_slot_keygloo(computer_t *computer, SlotType_t slot);

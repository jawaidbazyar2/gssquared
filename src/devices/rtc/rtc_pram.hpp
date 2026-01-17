#pragma once

#include "computer.hpp"
#include "SlotData.hpp"
#include "RTC.hpp"


struct rtc_pram_state_t {
    RTC *rtc = nullptr;

    /* write_handler_t display_wr_handler = { nullptr, nullptr };
    read_handler_t display_rd_handler = { nullptr, nullptr }; */

};

void init_slot_rtc_pram(computer_t *computer, SlotType_t slot);

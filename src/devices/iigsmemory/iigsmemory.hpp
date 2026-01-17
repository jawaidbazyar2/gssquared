#pragma once

#include "computer.hpp"
#include "display/display.hpp"
#include "mmus/mmu_iigs.hpp"

struct iigsmemory_state_t {
    //uint8_t switch_state;
    //display_state_t *display_state;
    computer_t *computer;
    //MMU_II *mmu; // megaii
    MMU_IIgs *mmu_iigs; // iigs
    /* display_state_t *display_state; */

    /* read_handler_t c050_rh = { nullptr, nullptr };
    write_handler_t c050_wh = { nullptr, nullptr };
    read_handler_t c051_rh = { nullptr, nullptr };
    write_handler_t c051_wh = { nullptr, nullptr };
    read_handler_t c052_rh = { nullptr, nullptr };
    write_handler_t c052_wh = { nullptr, nullptr };
    read_handler_t c053_rh = { nullptr, nullptr };
    write_handler_t c053_wh = { nullptr, nullptr };
    read_handler_t c054_rh = { nullptr, nullptr };
    write_handler_t c054_wh = { nullptr, nullptr };
    read_handler_t c055_rh = { nullptr, nullptr };
    write_handler_t c055_wh = { nullptr, nullptr };
    read_handler_t c056_rh = { nullptr, nullptr };
    write_handler_t c056_wh = { nullptr, nullptr };
    read_handler_t c057_rh = { nullptr, nullptr };
    write_handler_t c057_wh = { nullptr, nullptr }; */
};

void init_iigsmemory(computer_t *computer, SlotType_t slot);

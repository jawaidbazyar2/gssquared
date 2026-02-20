#pragma once

#include <cstdint>

#include "computer.hpp"
#include "display/display.hpp"
#include "mbus/KeyboardMessage.hpp"
#include "devices/languagecard/LanguageCardLogic.hpp"

struct iiememory_state_t {
    //uint8_t switch_state;
    //display_state_t *display_state;
    computer_t *computer;
    uint8_t *ram;
    MMU_II *mmu;
    MessageBus *mbus;

    bool f_80store = false;
    bool f_ramrd = false;
    bool f_ramwrt = false;
    bool f_altzp = false;
    //bool f_altcharset = false;

    // summary memory mapping flags
    bool m_zp = false; // this is both read and write.
    bool m_text1_r = false; // 
    bool m_text1_w = false; // 
    bool m_hires1_r = false; // 
    bool m_hires1_w = false; // 
    bool m_all_r = false; // 
    bool m_all_w = false; //

    bool s_hires = false;
    bool s_page2 = false;
    bool s_text = false;
    bool s_mixed = false;

    LanguageCardLogic ll;
   /*  bool FF_BANK_1;
    bool FF_READ_ENABLE;
    bool FF_PRE_WRITE;
    bool _FF_WRITE_ENABLE; */
    
    // BSRBANK2 == !FF_BANK_1
    // BSRREADRAM == FF_READ_ENABLE

};

void init_iiememory(computer_t *computer, SlotType_t slot);

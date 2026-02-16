/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* 
 * Language Card Logic
 * 
 * This class implements the logic for the language card and all its many derivatives.
 * It is responsible for updating LC state based on reading and writing the language card registers.
 * 
 * The caller is responsible for updating their own memory map based on the language card state.
 */

#pragma once

#include <cstdio>
#include "debug.hpp"

#define LANG_A3             0b00001000
#define LANG_A0A1           0b00000011

class LanguageCardLogic {

public:
    LanguageCardLogic() { reset(); };
    ~LanguageCardLogic() {};
    uint16_t FF_BANK_1;
    uint16_t FF_READ_ENABLE;
    uint16_t FF_PRE_WRITE;
    uint16_t _FF_WRITE_ENABLE;

    void read(uint32_t address) {
    
        if (DEBUG(DEBUG_LANGCARD)) printf("languagecard read %04X ", address);

        /** Bank Select */    
        
        if (address & LANG_A3 ) {
            // 1 = any access sets Bank_1
            FF_BANK_1 = 1;
        } else {
            // 0 = any access resets Bank_1
            FF_BANK_1 = 0;
        }
    
        /** Read Enable */
        
        if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) {
            // 00, 11 - set READ_ENABLE
            FF_READ_ENABLE = 1;
        } else {
            // 01, 10 - reset READ_ENABLE
            FF_READ_ENABLE = 0;
        }
    
        int old_pre_write = FF_PRE_WRITE;

        /* PRE_WRITE */
        if ((address & 0b00000001) == 1) {  // read 1 or 3
            // 00000001 - set PRE_WRITE
            FF_PRE_WRITE = 1;
        } else {                            // read 0 or 2
            // 00000000 - reset PRE_WRITE
            FF_PRE_WRITE = 0;
        }

        /** Write Enable */
        if ((old_pre_write == 1) && ((address & 0b00000001) == 1)) { // PRE_WRITE set, read 1 or 3
            // 00000000 - reset WRITE_ENABLE'
            _FF_WRITE_ENABLE = 0;
        }
        if ((address & 0b00000001) == 0) { // read 0 or 2, set _WRITE_ENABLE
            // 00000001 - set WRITE_ENABLE'
            _FF_WRITE_ENABLE = 1;
        }
    }
 
    /* 
    Update state based on write to C08X 
    Caller is responsible for updating their memory map 
    */
    
    void write(uint32_t address) {
    
        if (DEBUG(DEBUG_LANGCARD)) printf("languagecard write %04X\n", address);  
    
        /** Bank Select */
    
        if (address & LANG_A3 ) {
            // 1 = any access sets Bank_1 - read or write.
            FF_BANK_1 = 1;
        } else {
            // 0 = any access resets Bank_1
            FF_BANK_1 = 0;
        }
    
        /** READ_ENABLE  */
        
        if (((address & LANG_A0A1) == 0) || ((address & LANG_A0A1) == 3)) { // write 0 or 3
            // 00, 11 - set READ_ENABLE
            FF_READ_ENABLE = 1;
        } else {
            // 01, 10 - reset READ_ENABLE
            FF_READ_ENABLE = 0;
        }
        
        /** PRE_WRITE */ // any write, reests PRE_WRITE
        FF_PRE_WRITE = 0;
    
        /** WRITE_ENABLE */
        if ((address & 0b00000001) == 0) { // write 0 or 2
            _FF_WRITE_ENABLE = 1;
        }
    
        /* This means there is a feature of RAM card control
        not documented by Apple: write access to odd address in C08X
        range controls the READ ENABLE flip-flip without affecting the WRITE enable' flip-flop.
        STA $C081: no effect on write enable, disable read, bank 2 */
    
        if (DEBUG(DEBUG_LANGCARD)) printf("FF_BANK_1: %d, FF_READ_ENABLE: %d, FF_PRE_WRITE: %d, _FF_WRITE_ENABLE: %d\n", 
            FF_BANK_1, FF_READ_ENABLE, FF_PRE_WRITE, _FF_WRITE_ENABLE);   
    }
 
    /** At power up, the RAM card is disabled for reading and enabled for writing.
    * the pre-write flip-flop is reset, and bank 2 is selected. 
    * the RESET .. has no effect on the RAM card configuration.
    */
    void reset() {
        // this needs to take action or not - on II+, no reset occurs.
        // on IIe and IIgs, it does. That's fine, II+ just won't register this reset.
        FF_BANK_1 = 0;
        FF_READ_ENABLE = 0;
        FF_PRE_WRITE = 0;
        _FF_WRITE_ENABLE = 0;
    }
 
};

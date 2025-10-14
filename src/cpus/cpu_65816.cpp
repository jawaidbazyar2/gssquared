/*
 *   Copyright (c) 2025 Jawaid Bazyar

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
 
 #include "cpu_traits.hpp"
 #include "base_6502.cpp"  // Include the template implementation
 #include <memory>

class CPU65816 : public BaseCPU {
private:
    std::unique_ptr<BaseCPU> emulation_core;
    std::unique_ptr<BaseCPU> native_8_8_core;
    std::unique_ptr<BaseCPU> native_16_8_core;
    std::unique_ptr<BaseCPU> native_8_16_core;
    std::unique_ptr<BaseCPU> native_16_16_core;

    BaseCPU *current_core;

    // Helper method to update current CPU based on processor state
    inline void update_current_core_if_needed(cpu_state* cpu) {
        BaseCPU *old_core = current_core;

        if (cpu->E == 1) {    // Check emulation mode flag (E flag)
            current_core = emulation_core.get();
            
        } else {
            // Native mode - check M and X flags
            bool m_flag = cpu->_M; // M flag: 1 = 8-bit A, 0 = 16-bit A
            bool x_flag = cpu->_X; // X flag: 1 = 8-bit X/Y, 0 = 16-bit X/Y
            
            if (!m_flag && !x_flag) {
                current_core = native_16_16_core.get(); // 16-bit A, 16-bit X/Y
            } else if (!m_flag && x_flag) {
                current_core = native_16_8_core.get();  // 16-bit A, 8-bit X/Y
            } else if (m_flag && !x_flag) {
                current_core = native_8_16_core.get();  // 8-bit A, 16-bit X/Y
            } else {
                current_core = native_8_8_core.get();   // 8-bit A, 8-bit X/Y
            }
        }
        /* if (old_core != current_core) {
            printf("Switching to core: %s\n", current_core->get_name());
        } */
    }

public:
    CPU65816() {
        emulation_core = std::make_unique<CPU6502Core<CPU65816_E_8_8_Traits>>();
        native_8_8_core = std::make_unique<CPU6502Core<CPU65816_N_8_8_Traits>>();
        native_16_8_core = std::make_unique<CPU6502Core<CPU65816_N_16_8_Traits>>();
        native_8_16_core = std::make_unique<CPU6502Core<CPU65816_N_8_16_Traits>>();
        native_16_16_core = std::make_unique<CPU6502Core<CPU65816_N_16_16_Traits>>();
        
        current_core = emulation_core.get(); // Start in emulation mode
    }

    int execute_next(cpu_state* cpu) override {
        // Check for mode changes (REP/SEP instructions, CLC/XCE)
        update_current_core_if_needed(cpu); // these should be done inside the cores in REP/SEP/XCE somehow rather than impact every instruction.
        return current_core->execute_next(cpu);
    }
    void reset(cpu_state* cpu) override {
        // fill in with register reset logic.
        current_core->reset(cpu);
    }

    const char *get_name() override { return current_core->get_name(); }
};


// Factory function for creating 65816 instances
std::unique_ptr<BaseCPU> create65816() {
    return std::make_unique<CPU65816>();
}


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
 
 #include "cpu_traits.hpp"
 #include "base_6502.cpp"  // Include the template implementation
 #include <memory>
 #include <typeinfo>

// Second Sight runs alongside this host CPU. Bind the exact IIgs clock type
// at construction so each host cycle can call it without virtual dispatch.
template<bool DirectIIgsClock>
class CPU65816 : public BaseCPU {
private:
    std::unique_ptr<BaseCPU> emulation_core;
    std::unique_ptr<BaseCPU> native_8_8_core;
    std::unique_ptr<BaseCPU> native_16_8_core;
    std::unique_ptr<BaseCPU> native_8_16_core;
    std::unique_ptr<BaseCPU> native_16_16_core;

    // These cores share cpu_state with the traced cores. Ordinary execution
    // avoids collecting instruction history; debugging selects the traced set.
    std::unique_ptr<BaseCPU> emulation_untraced;
    std::unique_ptr<BaseCPU> native_8_8_untraced;
    std::unique_ptr<BaseCPU> native_16_8_untraced;
    std::unique_ptr<BaseCPU> native_8_16_untraced;
    std::unique_ptr<BaseCPU> native_16_16_untraced;

    BaseCPU *current_core;

    // Helper method to update current CPU based on processor state
    template<bool Untraced>
    inline void update_current_core_if_needed(cpu_state* cpu) {

        if (cpu->E == 1) {    // Check emulation mode flag (E flag)
            current_core = (Untraced ? emulation_untraced : emulation_core).get();
            
        } else {
            // Native mode - check M and X flags
            bool m_flag = cpu->_M; // M flag: 1 = 8-bit A, 0 = 16-bit A
            bool x_flag = cpu->_X; // X flag: 1 = 8-bit X/Y, 0 = 16-bit X/Y
            
            if (!m_flag && !x_flag) {
                current_core = (Untraced ? native_16_16_untraced : native_16_16_core).get(); // 16-bit A, 16-bit X/Y
            } else if (!m_flag && x_flag) {
                current_core = (Untraced ? native_16_8_untraced : native_16_8_core).get();  // 16-bit A, 8-bit X/Y
            } else if (m_flag && !x_flag) {
                current_core = (Untraced ? native_8_16_untraced : native_8_16_core).get();  // 8-bit A, 16-bit X/Y
            } else {
                current_core = (Untraced ? native_8_8_untraced : native_8_8_core).get();   // 8-bit A, 8-bit X/Y
            }
        }
    }

public:
    CPU65816(NClock *clock) : BaseCPU(clock) {
        emulation_core = std::make_unique<CPU6502Core<CPU65816_E_8_8_Traits, TraceEnabled, DirectIIgsClock>>(clock);
        native_8_8_core = std::make_unique<CPU6502Core<CPU65816_N_8_8_Traits, TraceEnabled, DirectIIgsClock>>(clock);
        native_16_8_core = std::make_unique<CPU6502Core<CPU65816_N_16_8_Traits, TraceEnabled, DirectIIgsClock>>(clock);
        native_8_16_core = std::make_unique<CPU6502Core<CPU65816_N_8_16_Traits, TraceEnabled, DirectIIgsClock>>(clock);
        native_16_16_core = std::make_unique<CPU6502Core<CPU65816_N_16_16_Traits, TraceEnabled, DirectIIgsClock>>(clock);
        
        emulation_untraced = std::make_unique<CPU6502Core<CPU65816_E_8_8_Traits, TraceDisabled, DirectIIgsClock>>(clock);
        native_8_8_untraced = std::make_unique<CPU6502Core<CPU65816_N_8_8_Traits, TraceDisabled, DirectIIgsClock>>(clock);
        native_16_8_untraced = std::make_unique<CPU6502Core<CPU65816_N_16_8_Traits, TraceDisabled, DirectIIgsClock>>(clock);
        native_8_16_untraced = std::make_unique<CPU6502Core<CPU65816_N_8_16_Traits, TraceDisabled, DirectIIgsClock>>(clock);
        native_16_16_untraced = std::make_unique<CPU6502Core<CPU65816_N_16_16_Traits, TraceDisabled, DirectIIgsClock>>(clock);

        current_core = emulation_core.get(); // Start in emulation mode
    }

    int execute_next(cpu_state* cpu) override {
        // Check for mode changes (REP/SEP instructions, CLC/XCE)
        update_current_core_if_needed<false>(cpu); // these should be done inside the cores in REP/SEP/XCE somehow rather than impact every instruction.
        return current_core->execute_next(cpu);
    }
    // Run one host instruction with history collection compiled out. Select
    // its E/M/X mode afresh because the preceding instruction may change it.
    int execute_next_untraced(cpu_state* cpu) override {
        update_current_core_if_needed<true>(cpu);
        return current_core->execute_next(cpu);
    }
    void reset(cpu_state* cpu) override {
        cpu->clock_stopped = false;
        // fill in with register reset logic.
        current_core->reset(cpu);
    }

    const char *get_name() override { return current_core->get_name(); }
};


// Factory function for creating 65816 instances
std::unique_ptr<BaseCPU> create65816(NClock *clock) {
    // Only the exact IIgs clock type permits direct calls; subclasses may
    // override timing and must retain virtual dispatch.
    if (clock && typeid(*clock) == typeid(NClockIIgs))
        return std::make_unique<CPU65816<true>>(clock);
    return std::make_unique<CPU65816<false>>(clock);
}


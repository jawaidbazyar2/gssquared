#include "cpu_implementations.hpp"
#include "cpu.hpp"

// Note: The actual CPU classes will be defined in their respective compilation units
// This file just provides the factory interface

// Factory function to create CPU instances
std::unique_ptr<BaseCPU> createCPU(const processor_type cpuType, NClock *clock) {
    // These will be implemented elsewhere and linked in
    extern std::unique_ptr<BaseCPU> create6502(NClock *clock);
    extern std::unique_ptr<BaseCPU> create65C02(NClock *clock);
    extern std::unique_ptr<BaseCPU> create65816(NClock *clock);
    
    if (cpuType == PROCESSOR_6502) {
        return create6502(clock);
    } else if (cpuType == PROCESSOR_65C02) {
        return create65C02(clock);
    } else if (cpuType == PROCESSOR_65816) {
        return create65816(clock);
    } else {
        return nullptr;
    }
} 
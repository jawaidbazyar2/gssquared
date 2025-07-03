#include "cpu_implementations.hpp"

// Note: The actual CPU classes will be defined in their respective compilation units
// This file just provides the factory interface

// Factory function to create CPU instances
std::unique_ptr<BaseCPU> createCPU(const std::string& cpuType) {
    // These will be implemented elsewhere and linked in
    extern std::unique_ptr<BaseCPU> create6502();
    extern std::unique_ptr<BaseCPU> create65C02();
    
    if (cpuType == "6502") {
        return create6502();
    } else if (cpuType == "65C02") {
        return create65C02();
    } else {
        return nullptr;
    }
} 
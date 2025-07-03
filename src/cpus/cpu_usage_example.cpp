/*
 * Example demonstrating polymorphic CPU usage
 * This shows how to use the BaseCPU interface to call execute_next
 * without knowing the specific CPU type at compile time.
 */

#include "cpu_implementations.hpp"
#include <iostream>

void demonstratePolymorphicCPUUsage() {
    // Create different CPU instances polymorphically
    std::unique_ptr<BaseCPU> cpu6502 = createCPU("6502");
    std::unique_ptr<BaseCPU> cpu65c02 = createCPU("65C02");
    
    // You can now call execute_next on any CPU type without knowing the specific type
    // cpu_state* state = ...; // Initialize your CPU state
    
    // Both calls use the same interface, but execute different implementations
    // cpu6502->execute_next(state);   // Executes 6502-specific code
    // cpu65c02->execute_next(state);  // Executes 65C02-specific code
    
    std::cout << "CPU instances created successfully!" << std::endl;
}

// Example of how you might use this in a CPU selection scenario
BaseCPU* selectCPUForSystem(const std::string& systemType) {
    if (systemType == "Apple II") {
        return createCPU("6502").release();
    } else if (systemType == "Apple IIe") {
        return createCPU("65C02").release();
    } else {
        return nullptr;
    }
} 
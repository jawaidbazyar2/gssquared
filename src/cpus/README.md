# Polymorphic CPU Architecture

This directory contains the implementation of a polymorphic CPU architecture that allows calling `execute_next()` on any CPU type without knowing the specific implementation at compile time.

## Architecture Overview

### Base Interface
- `BaseCPU` (in `cpu_traits.hpp`) - Pure virtual base class that defines the interface
- `virtual int execute_next(cpu_state *cpu) = 0` - The polymorphic method

### Template Implementation
- `CPU6502Core<CPUTraits>` (in `base_6502.cpp`) - Template class that contains the actual CPU implementation
- Inherits from `BaseCPU` and implements `execute_next()` as `override`
- Uses traits to differentiate between CPU variants at compile time

### Concrete CPU Classes
- `CPU6502` (in `cpu_6502.cpp`) - Instantiates `CPU6502Core<CPU6502Traits>`
- `CPU65C02` (in `cpu_65c02.cpp`) - Instantiates `CPU6502Core<CPU65C02Traits>`

### Factory Pattern
- `createCPU(const std::string& cpuType)` - Factory function to create CPU instances
- `create6502()` and `create65C02()` - Individual factory functions for each CPU type

## Usage Examples

### Basic Polymorphic Usage
```cpp
#include "cpu_implementations.hpp"

// Create CPU instances polymorphically
std::unique_ptr<BaseCPU> cpu = createCPU("6502");  // or "65C02"

// Call execute_next without knowing the specific CPU type
cpu_state* state = ...; // Your CPU state
cpu->execute_next(state);
```

### CPU Selection Based on System Type
```cpp
BaseCPU* selectCPUForSystem(const std::string& systemType) {
    if (systemType == "Apple II") {
        return createCPU("6502").release();
    } else if (systemType == "Apple IIe") {
        return createCPU("65C02").release();
    }
    return nullptr;
}
```

## Benefits

1. **Polymorphism**: Call `execute_next()` on any CPU type without compile-time knowledge
2. **Type Safety**: Virtual function dispatch ensures correct implementation is called
3. **Maintainability**: Easy to add new CPU types by following the same pattern
4. **Performance**: Template-based implementation means no runtime overhead for CPU-specific logic
5. **Flexibility**: Factory pattern allows runtime CPU selection

## Adding New CPU Types

To add a new CPU type (e.g., 65C816):

1. Add traits in `cpu_traits.hpp`:
   ```cpp
   struct CPU65C816Traits {
       // Define CPU-specific traits
   };
   ```

2. Create implementation file `cpu_65c816.cpp`:
   ```cpp
   #include "cpu_traits.hpp"
   #include "base_6502.cpp"
   #include <memory>
   
   class CPU65C816 : public CPU6502Core<CPU65C816Traits> {
       // 65C816-specific overrides
   };
   
   std::unique_ptr<BaseCPU> create65C816() {
       return std::make_unique<CPU65C816>();
   }
   ```

3. Update the factory function in `cpu_implementations.cpp`

## File Structure
```
src/cpus/
├── cpu_traits.hpp          # Base interface and CPU traits
├── base_6502.cpp           # Template implementation
├── cpu_6502.cpp            # 6502 concrete class
├── cpu_65c02.cpp           # 65C02 concrete class
├── cpu_implementations.hpp # Factory interface
├── cpu_implementations.cpp # Factory implementation
└── cpu_usage_example.cpp   # Usage examples
``` 
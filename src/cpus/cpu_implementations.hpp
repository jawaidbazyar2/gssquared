#pragma once

#include <memory>

#include "cpu_traits.hpp"
#include "processor_type.hpp"


// Forward declarations of concrete CPU classes
class CPU6502;
class CPU65C02;
class CPU65816;

// Factory function to create CPU instances
std::unique_ptr<BaseCPU> createCPU(const processor_type cpuType, NClock *clock); 
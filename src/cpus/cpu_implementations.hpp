#pragma once

#include "cpu_traits.hpp"
#include <memory>
#include <string>

// Forward declarations of concrete CPU classes
class CPU6502;
class CPU65C02;

// Factory function to create CPU instances
std::unique_ptr<BaseCPU> createCPU(const std::string& cpuType); 
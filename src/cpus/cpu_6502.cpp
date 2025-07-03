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

class CPU6502 : public CPU6502Core<CPU6502Traits> {
    // 6502-specific overrides if needed
};

// Factory function for creating 6502 instances
std::unique_ptr<BaseCPU> create6502() {
    return std::make_unique<CPU6502>();
}

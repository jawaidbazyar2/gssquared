/*
 *   Copyright (c) 2025 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>
#include <string>
#include <cstdio>

class DebugFormatter {
private:
    std::vector<std::string> lines;
    
public:
    // Printf-style formatting for adding a line
    template<typename... Args>
    void addLine(const char* format, Args... args) {
        constexpr size_t buffer_size = 1024;
        char buffer[buffer_size];
        snprintf(buffer, buffer_size, format, args...);
        lines.push_back(std::string(buffer));
    }
    
    // Add a plain string line
    void addLine(const std::string& line) {
        lines.push_back(line);
    }
    
    // Add a plain C-string line
    void addLine(const char* line) {
        lines.push_back(std::string(line));
    }
    
    // Get the vector of formatted lines
    const std::vector<std::string>& getLines() const {
        return lines;
    }
    
    // Clear all lines
    void clear() {
        lines.clear();
    }
    
    // Get number of lines
    size_t size() const {
        return lines.size();
    }
    
    // Check if empty
    bool empty() const {
        return lines.empty();
    }
};

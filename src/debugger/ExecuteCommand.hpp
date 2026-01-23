#pragma once 

#include "debugger/MonitorCommand.hpp"
#include "mmus/mmu.hpp"
#include <sstream>
#include <vector>
#include "debugger/MemoryWatch.hpp"
#include "debugger/disasm.hpp"
#include "debugger/trace.hpp"

class ExecuteCommand {
  private:
    MonitorCommand *cmd = nullptr;
    MMU *mmu = nullptr;
    std::vector<std::string> output_buffer;
    MemoryWatch *memory_watches = nullptr;
    MemoryWatch *breaks = nullptr;
    Disassembler *disasm = nullptr;
    std::vector<std::string> *debug_displays;
    system_trace_buffer *trace_buffer = nullptr;
    
    void addOutput(const std::vector<std::string>& lines) {
        output_buffer.insert(output_buffer.end(), lines.begin(), lines.end());
    }
    void addOutput(const std::string& line) {
        output_buffer.push_back(line);
    }
    
    template<typename... Args>
    void addFormattedOutput(const char* format, Args... args) {
        std::ostringstream oss;
        // For printf-style formatting, we'll use a temporary buffer
        constexpr size_t buffer_size = 1024;
        char buffer[buffer_size];
        snprintf(buffer, buffer_size, format, args...);
        output_buffer.push_back(std::string(buffer));
    }

    public:
        ExecuteCommand(MMU *mmu, MonitorCommand *cmd, MemoryWatch *watches, MemoryWatch *breaks, Disassembler *disasm, std::vector<std::string> *debug_displays, system_trace_buffer *trace_buffer);
        ~ExecuteCommand();
        const std::vector<std::string>& getOutput() const;
        void clearOutput();
        void execute();
};
#pragma once

#include <cstring>
#include <fstream>
#include <unordered_map>
#include <string>

#include "gs2.hpp"
#include "cpus/processor_type.hpp"
#include "debugger/trace_opcodes.hpp"

/* #define TRACE_FLAG_IRQ 0x01
#define TRACE_FLAG_X   0x10
#define TRACE_FLAG_M   0x20 */

// minimum of 8 byte chonkiness
struct system_trace_entry_t {
    uint64_t cycle;
    
    uint32_t operand; // up to 3 bytes per opcode for 65816. 
    uint8_t opcode;
    uint8_t p;
    uint8_t db;

    uint8_t pb;
    uint16_t pc;
    
    uint16_t a;
    uint16_t x;
    uint16_t y;
    uint16_t sp;
    uint16_t d;
    uint16_t data; // data read or written
    //uint8_t address_mode; // decoded address mode for instruction so we don't have to do it again.
    
    uint32_t eaddr; // the effective memory address used.
    union {
        struct {
            uint8_t f_irq: 1;
            uint8_t f_op_sz : 2; // 0 = 0 byte (implied), otherwise # bytes of operand.
            uint8_t f_data_sz : 1; // 0 = 8bit, 1 = 16bit
        };
        uint16_t flags;
    };
    uint16_t unused;
};

struct system_trace_buffer {
    system_trace_entry_t *entries;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    processor_type cpu_type;
    int16_t cpu_mask;
    std::unordered_map<uint32_t, std::string> labels;

    system_trace_buffer(size_t capacity, processor_type cpu_type);
    ~system_trace_buffer();

    void add_entry(const system_trace_entry_t &entry);

    void save_to_file(const std::string &filename);

    void read_from_file(const std::string &filename);

    system_trace_entry_t *get_entry(size_t index);

    char *decode_trace_entry(system_trace_entry_t *entry);

    bool load_labels_from_file(const std::string &filename);

    void clear_labels();

    void set_cpu_type(processor_type cpu_type) { 
        this->cpu_type = cpu_type;
        if (cpu_type == PROCESSOR_65816) cpu_mask = CPU_65816;
        if (cpu_type == PROCESSOR_65C02) cpu_mask = CPU_65C02;
        if (cpu_type == PROCESSOR_6502) cpu_mask = CPU_6502;
    }
    const char *get_label(uint32_t address);

private:
    char *decode_trace_entry_6502(system_trace_entry_t *entry);
    char *decode_trace_entry_65816(system_trace_entry_t *entry);
};



#pragma once

#include <cstring>
#include <fstream>

#include "gs2.hpp"

#define TRACE(SETTER) { SETTER }
//#define TRACE(SETTER) 

#define TRACE_FLAG_IRQ 1

// minimum of 8 byte chonkiness
struct system_trace_entry_t {
    uint64_t cycle;
    
    uint32_t operand; // up to 3 bytes per opcode for 65816. 
    uint8_t opcode;
    uint8_t p;
    uint8_t db;
    uint8_t pb;

    uint32_t pc;
    uint16_t a;
    uint16_t x;
    uint16_t y;
    uint16_t sp;
    uint16_t d;
    uint16_t data; // data read or written
    //uint8_t address_mode; // decoded address mode for instruction so we don't have to do it again.
    
    uint32_t eaddr; // the effective memory address used.
    uint16_t flags;
    uint16_t unused;
};

struct system_trace_buffer {
    system_trace_entry_t *entries;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;

    system_trace_buffer(size_t capacity);
    ~system_trace_buffer();

    void add_entry(const system_trace_entry_t &entry);

    void save_to_file(const std::string &filename);

    void read_from_file(const std::string &filename);

    system_trace_entry_t *get_entry(size_t index);

    char *decode_trace_entry(system_trace_entry_t *entry);
};



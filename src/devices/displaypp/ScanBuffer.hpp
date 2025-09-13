#pragma once

#include "Scan.hpp"

class ScanBuffer {
    Scan_t buffer[32768];
    uint16_t write_pos;
    uint16_t read_pos;
    uint16_t count;

public:
    ScanBuffer() { write_pos = 0; read_pos = 0; count = 0; };
    ~ScanBuffer() {};

    inline void push(Scan_t scan) { buffer[write_pos] = scan; write_pos = (write_pos + 1) % 32768; count++;};
    inline Scan_t pull() { Scan_t scan = buffer[read_pos]; read_pos = (read_pos + 1) % 32768; count--; return scan;};
    inline Scan_t peek() { return buffer[read_pos];};
    inline uint16_t get_count() { return count;};
    inline void clear() { write_pos = 0; read_pos = 0; count = 0;};
};
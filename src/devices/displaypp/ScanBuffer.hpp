#pragma once

#include "Scan.hpp"

class ScanBuffer {
    constexpr static uint32_t BUFFER_SIZE = 32768;
    constexpr static uint32_t BUFFER_MASK = BUFFER_SIZE - 1;
    alignas(64) Scan_t buffer[BUFFER_SIZE];
    uint32_t write_pos;
    uint32_t read_pos;

public:
    ScanBuffer() { clear(); };
    ~ScanBuffer() {};

    inline void push(Scan_t scan) noexcept { buffer[write_pos] = scan; write_pos = (write_pos + 1) & BUFFER_MASK; };
    inline Scan_t pull() noexcept { Scan_t scan = buffer[read_pos]; read_pos = (read_pos + 1) & BUFFER_MASK; return scan;};
    inline Scan_t peek() const noexcept { return buffer[read_pos];};
    inline uint32_t get_count() const noexcept { return (write_pos - read_pos) & BUFFER_MASK; };
    inline void clear() noexcept { write_pos = 0; read_pos = 0; };
    inline Scan_t get(uint32_t index) const noexcept { return buffer[(read_pos + index) & BUFFER_MASK]; };

    // Delay-0 softswitches: revise flags on the most recently pushed sample.
    // Bus order is softswitch then video_cycle, so that sample is this CPU cycle's
    // video byte. Do not rewrite mode — turning VM_BLANK into TEXT/HIRES overflows
    // the visible Frame560 during generate_frame.
    inline void update_last_flags(uint8_t flags) noexcept {
        if (write_pos == read_pos) {
            return;
        }
        buffer[(write_pos - 1) & BUFFER_MASK].flags = flags;
    }

    void saveToFile(const char *filename);
};
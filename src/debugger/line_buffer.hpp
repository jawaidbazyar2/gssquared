#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>

class line_buffer {
    char buffer[256];
    int cursor = 0;
    int length_ = 0;

    char hex_table[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    char hex(uint8_t b) { return hex_table[b]; };

public:
    line_buffer() { reset(); };
    ~line_buffer() {};

    void check() { 
        if (cursor > length_) length_ = cursor;
    };
    void pos(int cursor) { 
        this->cursor = cursor; 
        check();
    };
    int pos() { return cursor; };
    int length() { return length_; };
    void put(uint64_t u, int precision = 16) {
        // Emit u as decimal characters at the current cursor position.
        // We'll use a temporary buffer to hold the decimal string, then copy it to buffer.
        char temp[32];
        int n = snprintf(temp, sizeof(temp), "%-*llu", precision, (unsigned long long)u);
        for (int i = 0; i < n; ++i) {
            buffer[cursor++] = temp[i];
        }
        check();
    };
    void put(uint8_t b) { 
        buffer[cursor++] = hex((b & 0xF0) >> 4); 
        buffer[cursor++] = hex(b & 0x0F); 
        check(); 
    };
    void put(uint16_t w) { 
        buffer[cursor++] = hex((w & 0xF000) >> 12); 
        buffer[cursor++] = hex((w & 0x0F00) >> 8); 
        buffer[cursor++] = hex((w & 0x00F0) >> 4); 
        buffer[cursor++] = hex(w & 0x000F); 
        check(); 
    };
    void put(uint32_t u) { // this does only 24-bit because that's the max on a IIgs.
        buffer[cursor++] = hex((u & 0x00F00000) >> 20); 
        buffer[cursor++] = hex((u & 0x000F0000) >> 16);
        put('/');
        buffer[cursor++] = hex((u & 0x0000F000) >> 12); 
        buffer[cursor++] = hex((u & 0x00000F00) >> 8); 
        buffer[cursor++] = hex((u & 0x000000F0) >> 4); 
        buffer[cursor++] = hex(u & 0x0000000F); 
        check();
    };
    void put(const char *s) { 
        while (*s) { 
            buffer[cursor++] = *s++; 
        };
        check();
    };
    void put(char *s) { 
        while (*s) { 
            buffer[cursor++] = *s++; 
        };
        check();
    };
    void put(char c) { 
        buffer[cursor++] = c; 
        check(); 
    };
    char *get() { 
        buffer[length_] = 0; // nul-terminate string at the longest extent.
        return buffer; 
    };
    void reset() { 
        memset(buffer, ' ', sizeof(buffer)); 
        cursor = 0; 
        length_ = 0; 
    };
};
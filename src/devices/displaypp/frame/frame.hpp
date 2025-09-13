#pragma once

#include <cstdint>
#include <cstdio>

/* enum color_mode_t {
    COLORBURST_OFF = 0,
    COLORBURST_ON,
}; */

struct color_mode_t {
    uint8_t colorburst:1;
    uint8_t mixed_mode:1;
};

template<typename bs_t, uint16_t HEIGHT, uint16_t WIDTH>
class Frame {
private:
    uint16_t hloc;
    uint16_t scanline;
    uint16_t f_width; // purely informational, for consumers
    uint16_t f_height;
    alignas(64) bs_t stream[HEIGHT][WIDTH];
    color_mode_t line_mode[HEIGHT];

public:
    Frame(uint16_t width, uint16_t height);  // pixels
    ~Frame() = default;
    void print();

    inline bs_t *data() {
        return stream[0];
    }

    inline void push(bs_t bit) { 
        stream[scanline][hloc++] = bit;
    }

    inline bs_t pull() { 
        return stream[scanline][hloc++];
    }
    
    inline void set_line(int line) { 
        scanline = line;
        hloc = 0;
    }

    inline void set_color_mode(int line, color_mode_t mode) {
        if (line < 0 || line >= HEIGHT) {
            printf("set_color_mode: line out of bounds: %d\n", line);
            return;
        }
        line_mode[line] = mode;
    }

    inline color_mode_t get_color_mode(int line) {
        return line_mode[line];
    }

    inline uint16_t width() { return f_width; }
    inline uint16_t height() { return f_height; }
    void clear();
    
    // Getters for template parameters
    static constexpr uint16_t max_width() { return WIDTH; }
    static constexpr uint16_t max_height() { return HEIGHT; }
};

// Template implementation
template<typename bs_t, uint16_t HEIGHT, uint16_t WIDTH>
Frame<bs_t, HEIGHT, WIDTH>::Frame(uint16_t width, uint16_t height) {
    f_width = width;
    f_height = height;
    scanline = 0;
    hloc = 0;
    for (int i = 0; i < HEIGHT; i++) {
        line_mode[i] = {0, 0};
    }
}

template<typename bs_t, uint16_t HEIGHT, uint16_t WIDTH>
void Frame<bs_t, HEIGHT, WIDTH>::clear() {
    // Implementation would go here - clearing the bitstream array
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; ++j) {
            stream[i][j] = bs_t{};
        }
    }
    scanline = 0;
    hloc = 0;
}

template<typename bs_t, uint16_t HEIGHT, uint16_t WIDTH>
void Frame<bs_t, HEIGHT, WIDTH>::print() {
    printf("Frame: %u x %u\n", WIDTH, HEIGHT);
    for (int i = 0; i < HEIGHT; i++) {
        set_line(i);
        uint16_t linepos = 0;
        for (int j = 0; j < WIDTH / 64; j++) {
            uint64_t wrd = 0;
            for (int b = 0; b < 64; b++) {
                wrd = (wrd << 1) | pull();
                if (linepos++ >= WIDTH) break; // don't go past the end of the line
            }
            printf("%016llx ", wrd);
            if (linepos >= WIDTH) break; // don't go past the end of the line
        }
        printf("\n");
    }
}
#pragma once

#include <cstdint>
#include <cstring>

class Metrics {
    public:
        Metrics() { memset(samples, 0, sizeof(samples)); write_pos = 0; };
        ~Metrics() {};

        void record(uint64_t value) { samples[write_pos] = value; write_pos = (write_pos + 1) % 60; };
        uint64_t getMin();
        uint64_t getMax();
        uint64_t getAverage();

    private:
        uint64_t samples[60];
        uint64_t write_pos = 0;
};

#define MEASURE(metric, measurablecode) { uint64_t start_time = SDL_GetTicksNS(); measurablecode; uint64_t end_time = SDL_GetTicksNS(); metric.record(end_time - start_time); }
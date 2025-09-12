#include <algorithm>

#include "Metrics.hpp"


uint64_t Metrics::getMin() {
    uint64_t min = samples[0];
    for (int i = 1; i < 60; i++) {
        if (samples[i] < min) {
            min = samples[i];
        }
    }
    return min;    
}

uint64_t Metrics::getMax() {
    uint64_t max = samples[0];
    for (int i = 1; i < 60; i++) {
        if (samples[i] > max) {
            max = samples[i];
        }
    }
    return max;
}

uint64_t Metrics::getAverage() {
    uint64_t sum = 0;
    for (int i = 0; i < 60; i++) {
        sum += samples[i];
    }
    return sum / 60;
}
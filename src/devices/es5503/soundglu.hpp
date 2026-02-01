#pragma once

#include <cstdint>
#include "ensoniq.hpp"

class NClockII;

// Legacy structure for compatibility
struct ensoniq_state_t {
    ES5503 *chip = nullptr;
    uint8_t *doc_ram = nullptr;  // 64KB DOC RAM
    uint8_t soundctl = 0;
    uint8_t sounddata = 0;
    uint8_t soundadrl = 0;  // DOC register address (only low byte used)
    uint8_t soundadrh = 0;  // High byte (stored but not used for DOC addressing)
    int16_t *audio_buffer = nullptr;
    SDL_AudioStream *stream = nullptr;
    double frame_rate = 59.9227;  // Apple II frame rate
    float samples_per_frame = 0.0f;
    float samples_accumulated = 0.0f;
    computer_t *computer = nullptr;  // For IRQ handling
    NClockII *clock = nullptr;
    uint64_t doc_read_complete_time = 0;
};

void init_ensoniq_slot(computer_t *computer, SlotType_t slot);

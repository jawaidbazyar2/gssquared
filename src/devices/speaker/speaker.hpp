/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "cpu.hpp"
//#include "slots.hpp"
#include "LowPass.hpp"
#include "NEventBuffer.hpp"
#include "SpeakerFX.hpp"

#include "computer.hpp"

#define AMPLITUDE_DECAY_RATE (0x20)
#define AMPLITUDE_PEAK (0x2000)

#define SAMPLE_BUFFER_SIZE (4096)

/* #define SAMPLE_RATE (51000)
#define SAMPLES_PER_FRAME (850)
 */
// with change to 1021800 hz..
/* #define SAMPLE_RATE (51060)
#define SAMPLES_PER_FRAME (851) */
// this is wrong, but it's consistently wrong. (based on 850 samples * 59.923hz)
#define SAMPLE_RATE (44100)
#define SAMPLES_PER_FRAME (735)

#define EVENT_BUFFER_SIZE 128000

#if 0
struct EventBuffer {
    uint64_t events[EVENT_BUFFER_SIZE];
    int write_pos;
    int read_pos;
    int count;

    EventBuffer() : write_pos(0), read_pos(0), count(0) {}

    bool add_event(uint64_t cycle) {
        if (count >= EVENT_BUFFER_SIZE) {
            return false; // Buffer full
        }
        
        events[write_pos] = cycle;
        write_pos = (write_pos + 1) % EVENT_BUFFER_SIZE;
        count++;
        return true;
    }

    bool peek_oldest(uint64_t& cycle) {
        if (count == 0) {
            return false; // Buffer empty
        }
        cycle = events[read_pos];
        return true;
    }

    bool pop_oldest(uint64_t& cycle) {
        if (count == 0) {
            return false; // Buffer empty
        }
        
        cycle = events[read_pos];
        read_pos = (read_pos + 1) % EVENT_BUFFER_SIZE;
        count--;
        return true;
    }
};
#endif

    

typedef struct speaker_state_t {
    FILE *speaker_recording = NULL;
    //SDL_AudioDeviceID device_id = 0;
    //SDL_AudioStream *stream = NULL;

    bool first_time = true;

    int device_started = 0;
    double polarity = 1.0f;
    double target_polarity = 1.0f;
    double amplitude = AMPLITUDE_PEAK; // suggested 50%

    LowPassFilter *preFilter;
    LowPassFilter *postFilter;

    int16_t *working_buffer;
    EventBufferBase<event_wdata_t> *event_buffer;
    SpeakerFX *sp;
    computer_t *computer;
    NClock *clock;
    AudioSystem *audio_system;

    double frame_rate;
    double samples_per_frame;
    int32_t samples_per_frame_int;
    double samples_per_frame_remainder;
    double samples_accumulated = 0.0;
    clock_mode_t last_clock_mode = CLOCK_FREE_RUN;
    uint64_t samples_added = 0;
    uint64_t sample_frames = 0;
    uint64_t end_frame_c14M = 0;
} speaker_state_t;

void init_mb_speaker(computer_t *computer, SlotType_t slot);
void toggle_speaker_recording(cpu_state *cpu);
void dump_full_speaker_event_log();
void dump_partial_speaker_event_log(uint64_t cycles_now);
void speaker_start(cpu_state *cpu);
void speaker_stop();
uint64_t audio_generate_frame(computer_t *computer, cpu_state *cpu, uint64_t end_frame_c14M );
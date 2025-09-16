#pragma once

#include <stdint.h>
#include "EventBuffer.hpp"

#define SYNTHESIZE_SECONDS 3


class speaker_config_t {
    public:
        double frame_rate;
        double input_rate;
        double output_rate;
        double contribution_max;
        int samples_per_frame;
        double cycle_per_sample;
        double cycles_per_frame;
    
        speaker_config_t(double frame_rate, double input_rate, double output_rate,  int samples_per_frame) {
            reconfigure(frame_rate, input_rate, output_rate, samples_per_frame);
        }
        void reconfigure(double frame_rate, double input_rate, double output_rate,  int samples_per_frame) {
            this->frame_rate = frame_rate;
            this->input_rate = input_rate;
            this->output_rate = output_rate;
            this->samples_per_frame = samples_per_frame;
            this->contribution_max = input_rate / output_rate;   // contribution_max;
            this->cycles_per_frame = input_rate / frame_rate;
            this->cycle_per_sample = input_rate / output_rate;
        }
        void print() {
            printf("frame_rate: %f\n", frame_rate);
            printf("input_rate: %f\n", input_rate);
            printf("output_rate: %f\n", output_rate);
            printf("contribution_max: %f\n", contribution_max);
            printf("cycles_per_frame: %f\n", cycles_per_frame);
            printf("cycle_per_sample: %f\n", cycle_per_sample);
            printf("samples_per_frame: %d\n", samples_per_frame);
        }
};

class Speaker {
    public:
        Speaker(speaker_config_t *config, EventBufferBase *event_buffer) {
            this->config = config;
            this->event_buffer = event_buffer;
        }
    
        inline void getCheckPolarity(uint64_t cycle) {
        
            if (cycle >= event_buffer->peek()) {
        
                event_buffer->pop();
                /* polarity = -polarity; */
                polarity_impulse = -polarity_impulse;
                polarity = polarity_impulse;
            }
            
        }
        uint64_t generate_buffer_int(int16_t *buffer, uint64_t num_samples);
        uint64_t generate_fill_buffer(int16_t *buffer, uint64_t num_samples);
        void synthesize_event_data(int frequency);
        bool load_event_data(const char *filename);
        
        void fast_forward(uint64_t cycle) {
            cycle_index = cycle;
        }
        void setup_demo(void) {
            event_buffer->add_event(100);
            event_buffer->add_event(200);
            event_buffer->add_event(300);
            event_buffer->add_event(400);
            event_buffer->add_event(500);
            event_buffer->add_event(LAST_SAMPLE);
        }
        inline double get_polarity() { return polarity; }
        inline double get_polarity_impulse() { return polarity_impulse; }

    private:
        double polarity = 1.0f;
        double polarity_impulse = 1.0f;
        double leftover = 0.0f;
        double frac = 0.0f;
    
        /** we're always going to have the same fractional part. so what if we ALWAYS track whole cycles and just a fractional part? */
    
    public:
        speaker_config_t *config;
        uint64_t cycle_index = 0; // whole part of cycle count
        uint64_t sample_index = 0;
        uint64_t event_index = 0;
        EventBufferBase *event_buffer = NULL;
    
    };
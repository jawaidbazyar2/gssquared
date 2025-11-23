#pragma once

#include "devices/speaker/EventBuffer.hpp"
#include <SDL3/SDL.h>

typedef uint64_t speaker_t;
#define FRACTION_BITS 20
#define FRACTION_MASK ((1ULL << FRACTION_BITS) - 1)

/* 
typedef uint32_t speaker_t;
#define FRACTION_BITS 6
#define FRACTION_MASK ((1ULL << FRACTION_BITS) - 1)
 */
class SpeakerFX {
    private:
        //speaker_config_t *config;
        SDL_AudioStream *stream;
        SDL_AudioDeviceID device_id;
        uint16_t bufsize;
        int device_started = 0;
        uint64_t first_event = 0;
        uint64_t last_event = 0;
        //constexpr static speaker_t decay_coeff = (uint64_t)(0.9990f * (1 << FRACTION_BITS));
        constexpr static speaker_t volume_scale = 5120ULL;
        speaker_t decay_coeff = static_cast<speaker_t>(0.9990f * (1ULL << FRACTION_BITS));

    public:
        //uint64_t cycle_index = 0; // whole part of cycle count
        //uint64_t sample_index = 0;
        //uint64_t event_index = 0;
        int16_t *working_buffer;
        EventBufferBase *event_buffer;

        double frame_rate;
        speaker_t input_rate = 1020484;
        speaker_t output_rate = 44100;
        
        //int samples_per_frame = 735;
        
        speaker_t sample_scale;
        speaker_t cycles_per_sample;
        speaker_t rect_remain = 0ULL;
        speaker_t sample_remain = 0ULL;
        speaker_t polarity_impulse = 1ULL;
        speaker_t polarity = 1ULL << FRACTION_BITS;
        uint64_t hold_counter = 0;
        uint64_t hold_counter_value = 0;
        uint64_t last_event_time = 0;
        uint32_t last_event_fake = 1;

        FILE *speaker_recording = nullptr;

        SpeakerFX(uint32_t input_rate, uint32_t output_rate, uint64_t min_event_buffer_size, uint32_t min_sample_buffer_size) {
            this->event_buffer = new EventBufferRing(next_power_of_2(min_event_buffer_size));

            this->output_rate = (speaker_t)output_rate;
            configure((speaker_t)input_rate);
            polarity = polarity_impulse << FRACTION_BITS;

            //this->input_rate = (speaker_t)input_rate;
            //cycles_per_sample = this->input_rate / this->output_rate;

            //samples_per_frame = output_rate / 60;

            // make sure we allocate plenty of room for extra samples for catchup in generate.
            working_buffer = new int16_t[min_sample_buffer_size];
        
            // Initialize SDL audio - is this right, to do this again here?
            SDL_Init(SDL_INIT_AUDIO);
            
            SDL_AudioSpec desired = {};
            //desired.freq = SAMPLE_RATE;
            desired.freq = output_rate;
            desired.format = SDL_AUDIO_S16LE;
            desired.channels = 1;
        
            device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
            if (device_id == 0) {
                SDL_Log("Couldn't open audio device: %s", SDL_GetError());
                return;
            }
            
            stream = SDL_CreateAudioStream(&desired, NULL);
            if (!stream) {
                SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
                return;
            } else if (!SDL_BindAudioStream(device_id, stream)) {  /* once bound, it'll start playing when there is data available! */
                SDL_Log("Failed to bind speaker stream to device: %s", SDL_GetError());
                return;
            }
        
            SDL_PauseAudioDevice(device_id);        
       
        }

        ~SpeakerFX() {
            SDL_DestroyAudioStream(stream);
            SDL_CloseAudioDevice(device_id);
            delete[] working_buffer;
            delete event_buffer;
        }

        void print() {
            printf("===== SpeakerFX Configuration =====\n");
            printf("frame_rate: %f\n", frame_rate);
            printf("input_rate: %llu\n", input_rate);
            printf("output_rate: %llu\n", output_rate);
            printf("cycles_per_sample: %llu::%02llu\n", cycles_per_sample >> FRACTION_BITS, cycles_per_sample & FRACTION_MASK);
            printf("sample_scale: %llu::%02llu\n", sample_scale >> FRACTION_BITS, sample_scale & FRACTION_MASK);

            //printf("samples_per_frame: %d\n", samples_per_frame);

            printf("first_event: %llu last_event: %llu\n", first_event, last_event);
            printf("========================================\n");
        }

        void start() {
            
            // Start audio playback
        
            if (!SDL_ResumeAudioDevice(device_id)) {
                std::cerr << "Error resuming audio device: " << SDL_GetError() << std::endl;
            }
        
            device_started = 1;
        }

        void stop() {
            SDL_PauseAudioDevice(device_id);  // 1 means pause
            device_started = 0;
        }

        void reset(uint64_t cycle) {
            last_event_time = cycle;
            rect_remain = 0;
        }

        /* 
        Returns a whole number of samples worth of converted audio data.
        Has several optimizations over previous code.
        The gist of this routine is it integrates the waveform in chunks of whole samples.
        ("Integrate" means, calculate the area under the waveform in this sample.)
        This is then scaled into an output sample based on a volume factor. (5120 currently).
        Uses *only* addition, multiplication, and subtraction. Any necessary division is done inside the
        configure() method at setup time or during runtime.
        Previous versions were O(input frequency). This version is now O(state changes), regardless of input frequency (e.g. 14.31818MHz).
        */
        uint64_t generate_samples(int16_t *buffer, uint64_t num_samples, uint64_t frame_next_cycle_start) {

            for (uint64_t i = 0; i < num_samples; i++) {
                sample_remain = cycles_per_sample;
                speaker_t contrib = 0;
        
                uint64_t event_time;
                while (sample_remain > 0) {
                    if (rect_remain == 0) { // if there is nothing left in current rect, get next event and calc new rectangle.
                        if (event_buffer->peek_oldest(event_time)) {
                            event_buffer->pop();
                            // if this is a very large span of time (exceeding blah) then we overflow. instead of returning huge number, we need to somehow track this.
                            rect_remain = (event_time - last_event_time) << FRACTION_BITS;
                            if (last_event_fake == 0) {
                                polarity_impulse = (polarity_impulse == 0) ? 1 : 0; // if GS, use 2 instead of 1 here.. and apply DC offset of -1 to contrib when we generate the sample.
                                polarity = ( polarity_impulse << FRACTION_BITS);
                                hold_counter = hold_counter_value;
                            }
                            last_event_time = event_time;
                            last_event_fake = 0;
                        } else {
                            event_time = frame_next_cycle_start;
                            rect_remain = (event_time - last_event_time) << FRACTION_BITS;
                            if (last_event_fake == 0) {
                                polarity_impulse = (polarity_impulse == 0) ? 1 : 0; // if GS, use 2 instead of 1 here.. and apply DC offset of -1 to contrib when we generate the sample.
                                polarity = ( polarity_impulse << FRACTION_BITS);
                                hold_counter = hold_counter_value;
                            }
                            last_event_time = event_time;
                            last_event_fake = 1;
                        }
                    }
                    if (rect_remain == 0) {   // If no pending events, immediately finish sample based on current polarity.
                        contrib += ((sample_remain * polarity) >> FRACTION_BITS);
                        sample_remain = 0;
                    } else if (rect_remain >= sample_remain) { // if current rect is larger or equal than remaining sample, finish out sample.
                        contrib += ((sample_remain * polarity) >> FRACTION_BITS);
                        rect_remain -= sample_remain;
                        sample_remain = 0;
                    } else {  // current rect is smaller than sample - consume it and loop.
                        contrib += ((rect_remain * polarity) >> FRACTION_BITS); 
                        sample_remain -= rect_remain; 
                        rect_remain = 0;
                    }
                }
                // can also do >> fraction_bits * 2 but have to do << fraction_bits*2 in config so why bother?
                uint32_t contrib32 = (uint32_t)((contrib * sample_scale) >> FRACTION_BITS);
                //if (contrib32 > 16000) printf("barf %08X\n", contrib32);
                buffer[i] = (int16_t)(contrib32);  // use precalculated scaling factor.; we shift twice to get only the whole part.
                if (hold_counter) hold_counter--; // implement 30ms hold time.
                else polarity = (polarity * decay_coeff) >> FRACTION_BITS; 
            }
            return num_samples;
        }
        
        void configure(uint64_t input_rate) {
            this->input_rate = input_rate;
            cycles_per_sample = (input_rate << FRACTION_BITS) / output_rate;
            sample_scale = (volume_scale << (FRACTION_BITS)) / cycles_per_sample;
            hold_counter_value = (0.030f / (1.0f / output_rate));
        }
        
        inline uint32_t next_power_of_2(uint32_t value) { // Utility function to round up to the next power of 2
            if (value == 0) return 1;
            if ((value & (value - 1)) == 0) return value; // Already a power of 2
            
            value--;
            value |= value >> 1;
            value |= value >> 2;
            value |= value >> 4;
            value |= value >> 8;
            value |= value >> 16;
            value++;
            
            return value;
        }

        int get_queued_samples() {
            int samp = SDL_GetAudioStreamAvailable(stream) / sizeof(int16_t);
            return samp;
        }

        uint64_t generate_and_queue(int num_samples, uint64_t frame_next_cycle_start) {

            int samples_generated = generate_samples(working_buffer, num_samples, frame_next_cycle_start);
        
            SDL_PutAudioStreamData(stream, working_buffer, num_samples * sizeof(int16_t));
            
            return samples_generated;
        }

        void synthesize_events(int frequency, int seconds) {
            event_buffer->synthesize_event_data(frequency, input_rate, seconds);
            first_event = event_buffer->first_event;
            last_event = event_buffer->last_event;
        }

        bool load_events(const char *filename) {

            // load 'recording' file into the log
            FILE *recording = fopen(filename, "r");
            if (!recording) {
                std::cerr << "Error: Could not open recording file\n";
                return false;
            }
            uint64_t event;

            // skip any long silence, start playback / reconstruction at first event.
        
            while (fscanf(recording, "%llu", &event) != EOF) {
                if (!event_buffer->add_event(event)) {
                    std::cerr << "Error: Event buffer full\n";
                    fclose(recording);
                    return false;
                }
                if (first_event == 0) {
                    first_event = event;
                }
                last_event = event;
            }
            fclose(recording);
            return true;
        }
        uint64_t get_first_event() {
            return first_event;
        }
        uint64_t get_last_event() {
            return last_event;
        }
        void fast_forward(uint64_t cycles) {
            last_event_time += cycles;
        }
        SDL_AudioDeviceID get_device_id() { return device_id; }
        bool started() { return (device_started == 1); }
};

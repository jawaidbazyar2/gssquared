
#include "devices/speaker/EventBuffer.hpp"
#include <SDL3/SDL.h>

#define DEB(x) 

typedef double speaker_t;

class SpeakerFP {
    private:
        //speaker_config_t *config;
        SDL_AudioStream *stream;
        SDL_AudioDeviceID device_id;
        uint16_t bufsize;
        int device_started = 0;
        uint64_t first_event = 0;
        uint64_t last_event = 0;
        constexpr static speaker_t decay_coeff = 0.9990f;
        uint64_t ring_buffer_size = 0;

    public:
        //uint64_t cycle_index = 0; // whole part of cycle count
        //uint64_t sample_index = 0;
        //uint64_t event_index = 0;

        int16_t *working_buffer;
        EventBufferBase *event_buffer;

        double frame_rate;
        speaker_t input_rate = 1020484.0f;
        speaker_t output_rate = 44100.0f;
        
        int samples_per_frame = 735;
        
        speaker_t sample_scale;
        speaker_t cycles_per_sample;
        speaker_t rect_remain = 0.0f;
        speaker_t sample_remain = 0.0f;
        speaker_t polarity_impulse = 0.0f; // starts at 0.
        speaker_t polarity = 0.0f; // starts at 0.
        uint64_t hold_counter = 0;
        uint64_t hold_counter_value = 0;
        uint64_t last_event_time = 0;
        uint32_t last_event_fake = 1;

        FILE *speaker_recording = nullptr;

        SpeakerFP(uint32_t input_rate, uint32_t output_rate, uint64_t min_event_buffer_size, uint32_t min_sample_buffer_size) {
            ring_buffer_size = next_power_of_2(min_event_buffer_size);
            this->event_buffer = new EventBufferRing(ring_buffer_size);

            this->output_rate = (speaker_t)output_rate;
            configure((speaker_t)input_rate);
            //this->input_rate = (speaker_t)input_rate;
            //cycles_per_sample = this->input_rate / this->output_rate;
            polarity = polarity_impulse;

            samples_per_frame = output_rate / 60;

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

        void print() {
            printf("===== SpeakerFP Configuration =====\n");
            printf("ring_buffer_size: %llu\n", ring_buffer_size);
            printf("frame_rate: %f\n", frame_rate);
            printf("input_rate: %f\n", input_rate);
            printf("output_rate: %f\n", output_rate);
            printf("cycles_per_sample: %f\n", cycles_per_sample);
            printf("samples_per_frame: %d\n", samples_per_frame);

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

            for (uint32_t i = 0; i < num_samples; i++) {
                sample_remain = cycles_per_sample;
                speaker_t contrib = 0.0f;
        
                uint64_t event_time;
                while (sample_remain > 0.0f) {
                    if (rect_remain == 0.0f) { // if there is nothing left in current rect, get next event and calc new rectangle.
                        if (event_buffer->peek_oldest(event_time)) {
                            event_buffer->pop();

                            rect_remain = event_time - last_event_time;
                            DEB(printf("+/- %llu %llu\n", event_time, last_event_time);)

                            if (last_event_fake == 0) {
                                polarity_impulse = (polarity_impulse == 1.0f) ? 0.0f : 1.0f; // if GS, use -1 instead of 0 here.
                                polarity = polarity_impulse;
                                hold_counter = hold_counter_value;
                            }    
                            last_event_time = event_time;
                            last_event_fake = 0;
                        } else {
                            // no events pending, pretend one from now until end of frame.
                            event_time = frame_next_cycle_start;
                            rect_remain = event_time - last_event_time;
                            DEB(printf("  0ev %llu %llu %llu\n", event_time, frame_next_cycle_start, last_event_time);)
                            
                            if (last_event_fake == 0) {
                                polarity_impulse = (polarity_impulse == 1.0f) ? 0.0f : 1.0f; // if GS, use -1 instead of 0 here.
                                polarity = polarity_impulse;
                                hold_counter = hold_counter_value;
                            }
                            last_event_time = event_time;
                            last_event_fake = 1;
                        }
                    }
                    if (rect_remain == 0.0f) {   // If no pending events, immediately finish sample based on current polarity.
                        DEB(printf("  rr == 0 %f %f\n", rect_remain, sample_remain);)
                        contrib += (sample_remain * polarity);
                        sample_remain = 0.0f;
                    } else if (rect_remain >= sample_remain) { // if current rect is larger or equal than remaining sample, finish out sample.
                        DEB(printf("  rr >= sr %f %f\n", rect_remain, sample_remain);)
                        contrib += (sample_remain * polarity);
                        rect_remain -= sample_remain;
                        sample_remain = 0.0f;
                    } else {  // current rect is smaller than sample - consume it and loop.
                        DEB(printf("  rr < sr %f %f\n", rect_remain, sample_remain);)
                        contrib += (rect_remain * polarity); 
                        sample_remain -= rect_remain; 
                        rect_remain = 0.0f;
                    }
                }

                buffer[i] = (int16_t)(contrib * sample_scale);  // use precalculated scaling factor.
                DEB(printf("[%d] (%f - %f) %d\n", i, rect_remain, sample_remain, buffer[i]);)
                if (hold_counter) hold_counter--; // implement 30ms hold time.
                else polarity *= decay_coeff; 
            }
            return num_samples;
        }
        
        void configure(uint64_t input_rate) {
            this->input_rate = input_rate;
            cycles_per_sample = input_rate / output_rate;
            sample_scale = (5120.0f / cycles_per_sample);
            hold_counter_value = (0.030f / (1 / output_rate));
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

            uint64_t queued_samples = get_queued_samples();

            int16_t addsamples = 0;
        
            int samples_generated = generate_samples(working_buffer, num_samples, frame_next_cycle_start);
        
            SDL_PutAudioStreamData(stream, working_buffer, samples_generated * sizeof(int16_t));
            
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
                //event *= 14; // convert from 1mhz to 14.31818
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
};

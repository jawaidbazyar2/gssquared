#pragma once

#include <vector>

#include <SDL3/SDL.h>

/* The goal here */

struct audio_stream_t {
    SDL_AudioStream *stream;
    int sample_rate;
    int channels;
    int sample_format;
    bool apply_volume;
};

class AudioSystem {
private:
    // List to track allocated audio streams
    std::vector<audio_stream_t> allocated_streams;
    SDL_AudioDeviceID device_id;
    float gain = 1.0f;
    
public:
    AudioSystem();
    ~AudioSystem();

    SDL_AudioStream *create_stream(int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void update_stream(SDL_AudioStream *stream, int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void destroy_stream(SDL_AudioStream *stream);
    int get_stream_available(SDL_AudioStream *stream) { return SDL_GetAudioStreamAvailable(stream); }

    void pause();
    void resume();

    SDL_AudioDeviceID get_audio_device_id();

    uint16_t get_stream_count();
    
    inline bool put_stream_data(SDL_AudioStream *stream, const void *data, uint32_t len) {
        return SDL_PutAudioStreamData(stream, data, len);
    }
    void set_volume(float volume); // Apply volume to all streams marked "apply_volume=true"
    float get_volume();

/*     void set_mute(bool mute); // TODO: leave here for ideas.
    bool get_mute();
    void set_balance(float balance);
    float get_balance();
    void set_pan(float pan); */
};
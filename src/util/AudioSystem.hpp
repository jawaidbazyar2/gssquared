#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "DebugFormatter.hpp"

// forward declare.
class computer_t;

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
    uint16_t volume_setting = 6;
    float gain = 1.0f;
    bool decorrelation_enabled = true;
    void printSpec(SDL_AudioSpec spec);

public:
    AudioSystem(computer_t *computer);
    ~AudioSystem();

    SDL_AudioStream *create_stream(int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void update_stream(SDL_AudioStream *stream, int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void destroy_stream(SDL_AudioStream *stream);
    int get_stream_available(SDL_AudioStream *stream) { return SDL_GetAudioStreamAvailable(stream); }

    void pause();
    void resume();
    void flush_stream(SDL_AudioStream *stream) { SDL_FlushAudioStream(stream); }

    SDL_AudioDeviceID get_audio_device_id();

    uint16_t get_stream_count();
    
    inline bool put_stream_data(SDL_AudioStream *stream, const void *data, uint32_t len) {
        return SDL_PutAudioStreamData(stream, data, len);
    }
    void set_volume(uint16_t volume); // Apply volume to all streams marked "apply_volume=true"
    inline float get_gain() { return gain; }
    inline uint16_t get_volume() { return volume_setting; }
    void getCurrentAudioFormat(DebugFormatter *df);

    // Shared R-channel decorrelation toggle. Audio generators that emit
    // correlated stereo content (e.g. Mockingboard's two AY chips) can
    // consult this and apply a short R-channel delay to avoid L+R
    // cancellation when the host sums the channels on a mono speaker.
    inline bool get_decorrelation() const { return decorrelation_enabled; }
    inline void set_decorrelation(bool v) { decorrelation_enabled = v; }
    inline void toggle_decorrelation()    { decorrelation_enabled = !decorrelation_enabled; }

    /*     void set_mute(bool mute); // TODO: leave here for ideas.
    bool get_mute();
    void set_balance(float balance);
    float get_balance();
    void set_pan(float pan); */
};
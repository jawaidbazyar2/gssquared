#pragma once

#include <functional>
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

    // Callbacks invoked when the audio device format changes (e.g. user
    // switches default output device).  Each audio generator registers one
    // to reset its own timing state after the stream has been cleared.
    using DeviceResetCallback = std::function<void()>;
    std::vector<DeviceResetCallback> device_reset_callbacks;

public:
    AudioSystem(computer_t *computer);
    ~AudioSystem();

    SDL_AudioStream *create_stream(int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void update_stream(SDL_AudioStream *stream, int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume = false);
    void destroy_stream(SDL_AudioStream *stream);
    int get_stream_available(SDL_AudioStream *stream) { return SDL_GetAudioStreamAvailable(stream); }
    int get_stream_queued(SDL_AudioStream *stream) { return SDL_GetAudioStreamQueued(stream); }

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

    // Register a callback to be invoked when the audio device format changes.
    // The callback should reset any timing state that depends on the audio
    // clock (e.g. last_event_time in SpeakerFX) so generation resumes cleanly
    // after AudioSystem has cleared all streams.
    void register_device_reset_callback(DeviceResetCallback cb) {
        device_reset_callbacks.push_back(std::move(cb));
    }

    // Clear all streams — discards buffered audio that accumulated while the
    // device was paused during a format/device change.
    void clear_all_streams() {
        for (auto &streamr : allocated_streams) {
            SDL_ClearAudioStream(streamr.stream);
        }
    }

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
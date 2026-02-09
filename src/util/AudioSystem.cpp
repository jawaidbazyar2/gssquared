#include <cstdio>
#include <SDL3/SDL.h>

#include "AudioSystem.hpp"

AudioSystem::AudioSystem() {
    // Initialize SDL audio
    SDL_Init(SDL_INIT_AUDIO);

    // for info purposes, print out the available devices and their formats.
    int num_devices = 0;
    SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&num_devices);
    for (int i = 0; i < num_devices; i++) {
        SDL_AudioSpec spec;
        SDL_GetAudioDeviceFormat(devices[i], &spec, NULL);
        printf("AudioDevice %d: %s %d\n", i, SDL_GetAudioDeviceName(devices[i]), spec.freq);

    }
    SDL_free(devices);

    device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (device_id == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return;
    }

    gain = 1.0f * 6.0f / 16.0f;
    
}

AudioSystem::~AudioSystem() {
    // Clean up all streams that haven't been deallocated yet.
    for (auto &stream : allocated_streams) {
        SDL_DestroyAudioStream(stream.stream);
    }
    SDL_CloseAudioDevice(device_id);
}

SDL_AudioDeviceID AudioSystem::get_audio_device_id() {
    return device_id;
}

/* returns true if successful, false if not */
SDL_AudioStream *AudioSystem::create_stream(int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume) {
    SDL_AudioSpec spec = {
        sample_format,
        channels,
        sample_rate
    };
    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, nullptr);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return nullptr;
    }
    if (!SDL_BindAudioStream(device_id, stream)) {  /* once bound, it'll start playing when there is data available! */
        SDL_Log("Failed to bind speaker stream to device: %s", SDL_GetError());
        return nullptr;
    }
    audio_stream_t streamr = {
        stream,
        sample_rate,
        channels,
        sample_format,
        apply_volume
    };
    allocated_streams.push_back(streamr);
    return stream;
}

void AudioSystem::update_stream(SDL_AudioStream *stream, int sample_rate, int channels, SDL_AudioFormat sample_format, bool apply_volume) {
    for (auto &streamr : allocated_streams) {
        if (streamr.stream == stream) {
            streamr.sample_rate = sample_rate;
            streamr.channels = channels;
            streamr.sample_format = sample_format;
            streamr.apply_volume = apply_volume;
            SDL_AudioSpec spec = {
                sample_format,
                channels,
                sample_rate
            };
            SDL_SetAudioStreamFormat(stream, &spec, nullptr);
            return;
        }
    }
}

void AudioSystem::destroy_stream(SDL_AudioStream *stream) {
    for (auto it = allocated_streams.begin(); it != allocated_streams.end(); ++it) {
        if (it->stream == stream) {
            SDL_DestroyAudioStream(it->stream);
            allocated_streams.erase(it);
            return;
        }
    }
}

uint16_t AudioSystem::get_stream_count() {
    return allocated_streams.size();
}

void AudioSystem::pause() {
    SDL_PauseAudioDevice(device_id);
}

void AudioSystem::resume() {
    SDL_ResumeAudioDevice(device_id);
}

void AudioSystem::set_volume(uint16_t volume) {
    if (volume > 15) volume = 15;
    gain = (float)volume / 16.0f;
    for (auto &streamr : allocated_streams) {
        if (streamr.apply_volume) {
            SDL_SetAudioStreamGain(streamr.stream, gain);
        }
    }
}

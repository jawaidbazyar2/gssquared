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

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstdio>
#include "gs2.hpp"
#include "SoundEffect.hpp"
#include "util/AudioSystem.hpp"
 
const char *sounds_to_load[] = {
    "sounds/shugart-drive.wav",
    "sounds/shugart-stop.wav",
    "sounds/shugart-head.wav",
    "sounds/shugart-open.wav",
    "sounds/shugart-close.wav"
};
 
 
/* Optimize this a bit by returning a pointer to an existing record if we find that we already loaded the same key - 
e.g. if there are multiple disk II cards in a system */

SoundInfo_t *SoundEffect::find(uint64_t key)
{
    for (auto &si : streams) {
        if (si->key == key) {
            return si;
        }
    }
    return nullptr;
}

void SoundEffect::put_mono_as_stereo(SoundInfo_t *si, const Uint8 *mono, Uint32 mono_len, SoundChannel ch)
{
    const int bytes_per_sample = SDL_AUDIO_BYTESIZE(si->wav_format);
    if (bytes_per_sample <= 0 || (mono_len % bytes_per_sample) != 0) {
        return;
    }

    const Uint32 num_samples = mono_len / bytes_per_sample;
    const Uint32 stereo_len = mono_len * 2;
    expand_scratch.resize(stereo_len);

    Uint8 *dst = expand_scratch.data();
    for (Uint32 i = 0; i < num_samples; i++) {
        const Uint8 *src = mono + i * bytes_per_sample;
        Uint8 *left = dst + (i * 2) * bytes_per_sample;
        Uint8 *right = left + bytes_per_sample;

        switch (ch) {
            case SoundChannel::Left:
                SDL_memcpy(left, src, bytes_per_sample);
                SDL_memset(right, 0, bytes_per_sample);
                break;
            case SoundChannel::Right:
                SDL_memset(left, 0, bytes_per_sample);
                SDL_memcpy(right, src, bytes_per_sample);
                break;
            case SoundChannel::Both:
            default:
                SDL_memcpy(left, src, bytes_per_sample);
                SDL_memcpy(right, src, bytes_per_sample);
                break;
        }
    }

    audio_system->put_stream_data(si->stream, expand_scratch.data(), stereo_len);
}

SoundInfo_t *SoundEffect::load(const char *fname, uint64_t key)
{
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    SoundInfo_t *si = new SoundInfo_t();
    si->key = key;

    /* Load the .wav files from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%s%s", gs2_app_values.base_path.c_str(), fname);  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &si->wav_data, &si->wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        SDL_free(wav_path);
        delete si;
        return nullptr;
    }

    si->wav_channels = spec.channels;
    si->wav_format = spec.format;

    if (spec.channels != 1) {
        SDL_Log("Sound effect '%s' is not mono (%d channels); stereo pan requires mono PCM",
                fname, spec.channels);
        SDL_free(si->wav_data);
        SDL_free(wav_path);
        delete si;
        return nullptr;
    }

    /* Stream source format is stereo: we expand mono → L/R on every put. */
    si->stream = audio_system->create_stream(spec.freq, 2, spec.format, false);

    if (!si->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        SDL_free(si->wav_data);
        SDL_free(wav_path);
        delete si;
        return nullptr;
    }
    streams.push_back(si);

    SDL_free(wav_path);  /* done with this string. */
    return si;
}
 
 /* This function runs once at startup. */
SoundEffect::SoundEffect(AudioSystem *audio_system) {
    this->audio_system = audio_system;
    
}

SoundEffect::~SoundEffect() {
    for (auto &si : streams) {
        if (si->stream) audio_system->destroy_stream(si->stream);
        if (si->wav_data) SDL_free(si->wav_data);
        delete si;
    }
}

// one-time soundeffect play - plays the entire sound effect.
void SoundEffect::play(uint64_t key, SoundChannel ch)
{
    SoundInfo_t *si = find(key);
    if (si) {
        put_mono_as_stereo(si, si->wav_data, si->wav_data_len, ch);
    }
}

// play only a specific chunk of a sound effect.
void SoundEffect::play_specific(uint64_t key, int start, int length, SoundChannel ch)
{
    SoundInfo_t *si = find(key);
    if (!si || start < 0 || length <= 0) {
        return;
    }
    if ((Uint32)start >= si->wav_data_len) {
        return;
    }
    if ((Uint32)start + (Uint32)length > si->wav_data_len) {
        length = (int)(si->wav_data_len - (Uint32)start);
    }
    put_mono_as_stereo(si, si->wav_data + start, (Uint32)length, ch);
}

void SoundEffect::flush(uint64_t key)
{
    SoundInfo_t *si = find(key);
    if (si) {
        audio_system->flush_stream(si->stream);
    }
}

int SoundEffect::get_queued(uint64_t key)
{
    SoundInfo_t *si = find(key);
    if (!si || !si->stream) {
        return 0;
    }
    return audio_system->get_stream_queued(si->stream);
}

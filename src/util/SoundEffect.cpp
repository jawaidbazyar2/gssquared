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

SoundInfo_t *SoundEffect::load(const char *fname, uint64_t key)
{
    bool retval = false;
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    SoundInfo_t *si = new SoundInfo_t();
    si->key = key;

    /* Load the .wav files from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%s%s", gs2_app_values.base_path.c_str(), fname);  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &si->wav_data, &si->wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return nullptr;
    }

    /* Create an audio stream. Set the source format to the wav's format (what
    we'll input), leave the dest format NULL here (it'll change to what the
    device wants once we bind it). */
    si->stream = audio_system->create_stream(spec.freq, spec.channels, spec.format, false);

    if (!si->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
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
void SoundEffect::play(uint64_t key)
{
    for (auto &si : streams) {
        if (si->key == key) {
            audio_system->put_stream_data(si->stream, si->wav_data, si->wav_data_len);
            return;
        }
    }
}

// play only a specific chunk of a sound effect.
void SoundEffect::play_specific(uint64_t key, int start, int length)
{
    for (auto &si : streams) {
        if (si->key == key) {
            audio_system->put_stream_data(si->stream, si->wav_data + start, length);
            return;
        }
    }
}

void SoundEffect::flush(uint64_t key)
{
    for (auto &si : streams) {
        if (si->key == key) {
            audio_system->flush_stream(si->stream);
            return;
        }
    }
}


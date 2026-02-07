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

#include <SDL3/SDL.h>
#include "AudioSystem.hpp"

/* things that are playing sound (the audiostream itself, plus the original data, so we can refill to loop. */
struct SoundInfo_t {
    uint64_t key = 0;
    SDL_AudioStream *stream = nullptr;
    Uint8 *wav_data = nullptr;
    Uint32 wav_data_len = 0;
};

struct SoundEffectContainer_t {
    const char *fname = nullptr;
    SoundInfo_t *si = nullptr;
};

class SoundEffect {
protected:
    std::vector<SoundInfo_t *> streams;
    AudioSystem *audio_system;

public:
    SoundEffect(AudioSystem *audio_system);
    ~SoundEffect();


    SoundInfo_t *load(const char *fname, uint64_t key);
    void play(uint64_t key);
    void play_specific(uint64_t key, int start, int length);
    void flush(uint64_t key);       // clear pending data from an audio stream.
};

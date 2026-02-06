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
//#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <cstdio>
#include "gs2.hpp"
#include "cpu.hpp"
#include "devices/speaker/speaker.hpp"
#include "soundeffects.hpp"
#include "util/AudioSystem.hpp"



/* things that are playing sound (the audiostream itself, plus the original data, so we can refill to loop. */
typedef struct SoundEffect {
    Uint8 *wav_data;
    Uint32 wav_data_len;
    SDL_AudioStream *stream;
} SoundEffect;

static SoundEffect soundeffects[5];

const char *sounds_to_load[] = {
    "sounds/shugart-drive.wav",
    "sounds/shugart-stop.wav",
    "sounds/shugart-head.wav",
    "sounds/shugart-open.wav",
    "sounds/shugart-close.wav"
};


static bool load_soundeffect(SDL_AudioDeviceID audio_device, const char *fname, SoundEffect *soundeffect)
{
    bool retval = false;
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    /* Load the .wav files from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%s%s", gs2_app_values.base_path.c_str(), fname);  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &soundeffect->wav_data, &soundeffect->wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return false;
    }

    /* Create an audio stream. Set the source format to the wav's format (what
       we'll input), leave the dest format NULL here (it'll change to what the
       device wants once we bind it). */
    soundeffect->stream = SDL_CreateAudioStream(&spec, NULL);
    if (!soundeffect->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(audio_device, soundeffect->stream)) {  /* once bound, it'll start playing when there is data available! */
        SDL_Log("Failed to bind '%s' stream to device: %s", fname, SDL_GetError());
    } else {
        retval = true;  /* success! */
    }

    SDL_free(wav_path);  /* done with this string. */
    return retval;
}

/* This function runs once at startup. */
bool soundeffects_init(computer_t *computer)
{
    //speaker_state_t *speaker_state = (speaker_state_t *)get_module_state(computer->cpu, MODULE_SPEAKER);

    SDL_SetAppMetadata("Example Audio Multiple Streams", "1.0", "com.example.audio-multiple-streams");

    for (int i = 0; i < SDL_arraysize(sounds_to_load); i++) {
        if (!load_soundeffect(computer->audio_system->get_audio_device_id(), sounds_to_load[i], &soundeffects[i])) {
            printf("Failed to load sound effect: %s\n", sounds_to_load[i]);
            return false;
        }
    }
    computer->register_shutdown_handler([]() {
        for (int i = 0; i < SDL_arraysize(soundeffects); i++) {
            SDL_DestroyAudioStream(soundeffects[i].stream);
            SDL_free(soundeffects[i].wav_data);
        }
        return true;
    });
    return true;
}

void soundeffects_play(int index)
{
    SDL_PutAudioStreamData(soundeffects[index].stream, soundeffects[index].wav_data, soundeffects[index].wav_data_len);
}

/* This function runs once per frame, and is the heart of the program. */
void soundeffects_update(bool diskii_running, int tracknumber)
{
    static bool diskii_running_last = false;
    static int tracknumber_last = 0;

    //printf("diskii_running: %d, tracknumber: %d / %d\n", diskii_running, tracknumber, tracknumber_last);

    /* If less than a full copy of the audio is queued for playback, put another copy in there.
        This is overkill, but easy when lots of RAM is cheap. One could be more careful and
        queue less at a time, as long as the stream doesn't run dry.  */

    /* If sound state changed, reset the stream */
    if (diskii_running_last && !diskii_running) {
        diskii_running_last = false;

        /* Clear the audio stream when transitioning to disabled state */
        SDL_FlushAudioStream(soundeffects[SE_SHUGART_DRIVE].stream);
    }
    
    /* Only queue audio data if sound is enabled */
    static int running_chunknumber = 0;
    if (diskii_running) {
        int dl = (int) soundeffects[SE_SHUGART_DRIVE].wav_data_len / 10;
        if (SDL_GetAudioStreamQueued(soundeffects[SE_SHUGART_DRIVE].stream) < dl) {
            SDL_PutAudioStreamData(soundeffects[SE_SHUGART_DRIVE].stream, soundeffects[SE_SHUGART_DRIVE].wav_data + dl * running_chunknumber, dl);
            running_chunknumber++;
            if (running_chunknumber > 8) {
                running_chunknumber = 0;
            }
        }
    }
    // minimum track movement is 2. We're called every 1/60th. That's 735 samples.
    static int start_track_movement = -1;
    if (tracknumber >= 0 && (tracknumber_last != tracknumber)) {
        // if we have a track movement, play the head movement sound
        // head can move 16.7 / 2.5 tracks per second, about 7.
        int ind = 200 * 2 * std::abs(start_track_movement-tracknumber);

        int len = ((int) (200 * 2) * std::abs(tracknumber_last-tracknumber));
        if (ind + len > soundeffects[SE_SHUGART_HEAD].wav_data_len) {
            len = soundeffects[SE_SHUGART_HEAD].wav_data_len - ind;
        }
        SDL_PutAudioStreamData(
            soundeffects[SE_SHUGART_HEAD].stream, 
            soundeffects[SE_SHUGART_HEAD].wav_data + ind,
            len
        );
        if (start_track_movement == -1) start_track_movement = tracknumber_last;
        tracknumber_last = tracknumber;
    } else {
        // if head did not move on this update, reset the start_track_movement
        start_track_movement = -1;
    }

}

/* This function runs once at shutdown. */
void soundeffects_shutdown(SDL_AudioDeviceID audio_device)
{
    int i;

    SDL_CloseAudioDevice(audio_device);

    for (i = 0; i < SDL_arraysize(soundeffects); i++) {
        if (soundeffects[i].stream) {
            SDL_DestroyAudioStream(soundeffects[i].stream);
        }
        SDL_free(soundeffects[i].wav_data);
    }

    /* SDL will clean up the window/renderer for us. */
}
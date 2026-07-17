#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "util/AudioSystem.hpp"
#include "ensoniq.hpp"
#include "util/InterruptController.hpp"

class NClockII;

// Legacy structure for compatibility
struct ensoniq_state_t {
    ES5503 *chip = nullptr;
    uint8_t *doc_ram = nullptr;  // 64KB DOC RAM
    uint8_t soundctl = 0;
    uint8_t sounddata = 0;
    uint8_t soundadrl = 0;  // DOC register address (only low byte used)
    uint8_t soundadrh = 0;  // High byte (stored but not used for DOC addressing)
    int16_t *audio_buffer = nullptr;
    // DOC samples accumulate here; flushed to SDL once per video frame after
    // resampling to the device rate (avoids SDL chunk-resampling 27117→48k).
    int16_t *sdl_staging = nullptr;
    uint32_t sdl_staging_count = 0;
    static constexpr uint32_t SDL_STAGING_CAP = 16384; // >1 frame even at max DOC rate
    int16_t *sdl_resample_buf = nullptr;
    uint32_t sdl_device_rate = 48000;
    double resample_pos = 0.0;       // fractional DOC-sample index carried across frames
    AudioSystem *audio_system = nullptr;
    SDL_AudioStream *stream = nullptr;
    double frame_rate = 59.9227;  // Apple II frame rate
    float samples_per_frame = 0.0f;
    float samples_accumulated = 0.0f;
    computer_t *computer = nullptr;  // For IRQ handling
    InterruptController *irq_control = nullptr;
    NClockII *clock = nullptr;
    uint64_t doc_read_complete_time = 0;
    // Address latched when a deferred DOC/RAM read is started. Completion must
    // use this, not the live pointer — callers (e.g. NinjaTrackerPlus) change
    // $C03E between the pipeline priming read and the data read.
    uint16_t doc_read_latched_addr = 0;

    // Fast-forward / catch-up state. Samples and oscillator IRQs are advanced to
    // the current 14M-clock time on every register/RAM access and on the periodic
    // cycle handler, rather than only once per video frame.
    uint64_t last_catchup_c14m = 0;  // 14M timestamp of the last generated sample boundary
    uint64_t c14m_accum = 0;         // fractional-c14m remainder carried between catch-ups

    // Smoothed $C03C volume nibble (0..15), slewed with ~20ms time constant.
    // Firmware/demos flip the nibble 15→5→15 for 0.6–7.7ms around DOC accesses;
    // applying that instantly chops the waveform (audible LF distortion), while
    // the real analog volume path smooths such transients. <0 = uninitialized.
    float vol_smooth = -1.0f;
};

void init_ensoniq_slot(computer_t *computer, SlotType_t slot);

/** Pack Ensoniq STATE_GET v1 blob (784 bytes). Returns false on error. */
bool pack_ensoniq_state(ensoniq_state_t *st, std::vector<uint8_t> &out, std::string &err);

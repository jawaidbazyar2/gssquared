/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * Minimal SSI-263 speech synthesizer for the Mockingboard.
 *
 * The register and ready-bit behaviour follows the Appletini SSI-263 bus
 * wrapper.  The audio path is intentionally lightweight: it uses a voiced /
 * noise excitation source and three resonant formants.  It is not intended to
 * be a transistor-level model, but it makes SSI phoneme streams audible and
 * recognisably speech-like while keeping the emulation inexpensive.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

class SSI263 {
public:
    static constexpr uint32_t kSampleRate = 44100;

    SSI263() { reset(); }

    void reset() {
        registers_ = {0xC0, 0x00, 0x00, 0x80, 0xFF};
        ready_ = false;
        active_ = false;
        completion_pending_ = false;
        interrupts_enabled_ = false;
        current_function_ = 0;
        phoneme_ = 0;
        samples_remaining_ = 0;
        samples_total_ = 0;
        phase_ = 0.0f;
        pitch_hz_ = pitch_target_hz_ = pitchFromRegisters();
        pitch_start_hz_ = pitch_hz_;
        noise_lfsr_ = 0x1ACEu;
        profile_ = profileFor(0);
        previous_profile_ = profile_;
        transition_samples_ = 1;
        formant_update_countdown_ = 0;
        for (auto &filter : formants_) {
            filter.reset();
        }
    }

    // In Mockingboard-compatible mode the primary SSI socket is selected by
    // A6 over offsets $40-$7F.  A2..A0 select the register, so registers 0..7
    // repeat throughout the window (4..7 are filter-frequency aliases).
    static constexpr bool isMockingboardWriteOffset(uint8_t offset) {
        return offset >= 0x40 && offset <= 0x7F;
    }

    static constexpr uint8_t registerForOffset(uint8_t offset) {
        return offset & 0x07;
    }

    // SSI registers: 0 duration/phoneme, 1 inflection, 2 rate/inflection,
    // 3 control/articulation/amplitude, and 4 filter frequency.
    void write(uint8_t reg, uint8_t value) {
        if (reg > 7) {
            return;
        }

        // The hardware only decodes three register-address bits. Registers
        // 4..7 all reach the filter-frequency latch.
        if (reg >= 4) {
            registers_[4] = value;
            return;
        }

        const uint8_t old_control = registers_[3];
        const bool was_ready = ready_;
        registers_[reg] = value;

        // The A/!R ready indication clears on writes to the first three
        // registers, matching the canonical Appletini bus wrapper.
        if (reg <= 2) {
            ready_ = false;
            completion_pending_ = false;
        }

        switch (reg) {
            case 0:
                phoneme_ = value & 0x3F;
                if ((registers_[3] & 0x80) == 0) {
                    startPhoneme(phoneme_);
                }
                break;

            case 1:
            case 2:
                // Clearing A/!R after a completed phoneme repeats the current
                // phone on the real part.  Some speech drivers depend on it.
                if (was_ready && !active_ && (old_control & 0x80) == 0) {
                    startPhoneme(phoneme_);
                } else if (active_) {
                    // INFLECT/RATEINF are live inputs to the canonical voice
                    // core, so pitch changes during an active phone as well.
                    pitch_target_hz_ = pitchFromRegisters();
                }
                break;

            case 3:
                if (value & 0x80) {
                    active_ = false;
                    ready_ = false;
                    completion_pending_ = false;
                    samples_remaining_ = 0;
                } else if (old_control & 0x80) {
                    // Dropping the control bit starts the phoneme which was
                    // staged while the device was held in reset. This edge
                    // also latches the duration function and IRQ-enable mode.
                    latchModeAndInterrupts();
                    startPhoneme(registers_[0] & 0x3F);
                } else {
                    // Articulation and amplitude are live while CTL remains
                    // low. Recompute the compact backend's settling window.
                    transition_samples_ = articulationSamples();
                }
                break;

            default:
                break;
        }
    }

    // This is the SSI's native D7 state. Mockingboard mode routes completion
    // to a 6522 CA1 interrupt and must not return this from slot-page reads;
    // a future Phasor-native bus decoder may use it directly.
    uint8_t read(uint8_t /*reg*/) const { return ready_ ? 0x80 : 0x00; }

    bool ready() const { return ready_; }
    bool active() const { return active_; }
    bool interruptsEnabled() const { return interrupts_enabled_; }
    uint8_t phoneme() const { return phoneme_; }
    uint32_t samplesRemaining() const { return samples_remaining_; }
    uint32_t samplesTotal() const { return samples_total_; }
    float pitchHz() const { return pitch_target_hz_; }

    bool takeCompletion() {
        const bool pending = completion_pending_;
        completion_pending_ = false;
        return pending;
    }

    // Mix mono speech into an interleaved stereo Mockingboard frame.  The AY
    // generator creates the frame first, so speech can be added in-place.
    void mixSamples(std::vector<float> &stereo, uint32_t sample_count) {
        const size_t needed = static_cast<size_t>(sample_count) * 2;
        if (stereo.size() < needed) {
            stereo.resize(needed, 0.0f);
        }

        for (uint32_t i = 0; i < sample_count; ++i) {
            float speech = active_ ? generateSample() : 0.0f;
            const size_t index = static_cast<size_t>(i) * 2;
            stereo[index] = clampAudio(stereo[index] + speech);
            stereo[index + 1] = clampAudio(stereo[index + 1] + speech);
        }
    }

private:
    struct Profile {
        float f1;
        float f2;
        float f3;
        float voiced;
        float noise;
        uint16_t duration_ms;
    };

    class Resonator {
    public:
        void reset() { z1_ = z2_ = 0.0f; }

        void configure(float frequency, float q) {
            frequency = std::clamp(frequency, 80.0f,
                                   static_cast<float>(kSampleRate) * 0.45f);
            q = std::max(q, 0.5f);
            constexpr float pi = 3.14159265358979323846f;
            const float omega = 2.0f * pi * frequency /
                                static_cast<float>(kSampleRate);
            const float alpha = std::sin(omega) / (2.0f * q);
            const float a0 = 1.0f + alpha;

            // Constant-skirt-gain band-pass biquad.
            b0_ = alpha / a0;
            b1_ = 0.0f;
            b2_ = -alpha / a0;
            a1_ = (-2.0f * std::cos(omega)) / a0;
            a2_ = (1.0f - alpha) / a0;
        }

        float process(float input) {
            const float output = b0_ * input + z1_;
            z1_ = b1_ * input - a1_ * output + z2_;
            z2_ = b2_ * input - a2_ * output;
            return output;
        }

    private:
        float b0_ = 0.0f;
        float b1_ = 0.0f;
        float b2_ = 0.0f;
        float a1_ = 0.0f;
        float a2_ = 0.0f;
        float z1_ = 0.0f;
        float z2_ = 0.0f;
    };

    static Profile profileFor(uint8_t phone) {
        // SSI-263 phoneme groups.  Vowels get their characteristic formant
        // centres; stops and fricatives use short noise-rich profiles.  The
        // mapping covers all 64 codes and gives the common SSI vocabulary
        // (IY, AY, EH, AE, AH, O, UH, ER, L, W, consonants) distinct sounds.
        switch (phone) {
            case 0x00: return {500, 1500, 2500, 0.0f, 0.0f, 45}; // pause
            case 0x01: case 0x02: return {270, 2290, 3010, 1.0f, 0.02f, 95}; // IY
            case 0x03: case 0x04: return {300, 2200, 3000, 1.0f, 0.02f, 90}; // Y
            case 0x05: case 0x06: return {650, 1200, 2500, 1.0f, 0.02f, 125}; // AY
            case 0x07: return {390, 1990, 2550, 1.0f, 0.02f, 90};             // I
            case 0x08: case 0x09: return {730, 1090, 2440, 1.0f, 0.02f, 105}; // A
            case 0x0A: case 0x0B: return {530, 1840, 2480, 1.0f, 0.02f, 95};  // EH
            case 0x0C: case 0x0D: return {660, 1720, 2410, 1.0f, 0.02f, 100}; // AE
            case 0x0E: case 0x0F: return {640, 1190, 2390, 1.0f, 0.02f, 95};  // AH
            case 0x10: return {570, 840, 2410, 1.0f, 0.02f, 120};             // AW
            case 0x11: case 0x12: return {570, 840, 2410, 1.0f, 0.02f, 105};  // O/OU
            case 0x13: case 0x14: case 0x15: return {300, 870, 2240, 1.0f, 0.02f, 95};
            case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B:
                return {440, 1020, 2240, 1.0f, 0.02f, 90};                    // U/UH
            case 0x1C: case 0x1D: case 0x1E: case 0x1F:
                return {490, 1350, 1690, 0.95f, 0.03f, 100};                 // ER/R
            case 0x20: case 0x21: case 0x22:
                return {400, 1200, 2600, 0.85f, 0.04f, 80};                  // L
            case 0x23: return {300, 800, 2200, 0.9f, 0.03f, 80};             // W
            case 0x24: return {250, 900, 2200, 0.45f, 0.35f, 65};            // B
            case 0x25: return {300, 1700, 2700, 0.4f, 0.45f, 58};            // D
            case 0x26: case 0x29: return {350, 1450, 3000, 0.05f, 1.0f, 62}; // K
            case 0x27: return {300, 900, 2600, 0.0f, 1.0f, 55};              // P
            case 0x28: return {300, 2100, 3300, 0.0f, 1.0f, 52};             // T
            case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E:
                return {500, 1600, 3000, 0.05f, 0.8f, 75};                   // H
            case 0x2F: return {350, 2400, 3600, 0.45f, 0.85f, 85};           // Z
            case 0x30: case 0x32: return {350, 3200, 4600, 0.0f, 1.0f, 90};  // S/SH
            case 0x31: return {350, 2200, 3400, 0.35f, 0.75f, 70};           // J
            case 0x33: return {350, 1300, 2600, 0.65f, 0.55f, 80};           // V
            case 0x34: case 0x36: return {400, 2600, 4200, 0.0f, 1.0f, 78};  // F/TH
            case 0x35: return {400, 1800, 3000, 0.55f, 0.55f, 78};           // THV
            case 0x37: return {250, 1000, 2100, 0.9f, 0.08f, 85};            // M
            case 0x38: return {300, 1400, 2500, 0.9f, 0.08f, 82};            // N
            case 0x39: return {300, 1100, 2300, 0.85f, 0.1f, 88};            // NG
            default: return {500, 1500, 2500, 0.0f, 0.0f, 55};               // pauses
        }
    }

    void latchModeAndInterrupts() {
        const uint8_t function = registers_[0] >> 6;
        if (function != 0) {
            current_function_ = function;
            interrupts_enabled_ = true;
        } else {
            interrupts_enabled_ = false;
        }
    }

    float pitchFromRegisters() const {
        // SSI-263 pitch is a 12-bit inflection word assembled as
        // {RATEINF[3], INFLECT[7:0], RATEINF[2:0]}. With a 1 MHz XCK the
        // data-sheet law is f0 = XCK / (8 * (4096 - I)).
        const uint16_t inflection = static_cast<uint16_t>(
            ((registers_[2] & 0x08) ? 0x0800 : 0x0000) |
            (static_cast<uint16_t>(registers_[1]) << 3) |
            (registers_[2] & 0x07));
        const float span = static_cast<float>(4096u - inflection);
        return std::clamp(125000.0f / span, 30.5f,
                          static_cast<float>(kSampleRate) * 0.45f);
    }

    float durationSpeed() const {
        const uint8_t duration = (current_function_ == 1)
            ? 3 : (registers_[0] >> 6);
        switch (duration) {
            case 1: return 1.25f; // canonical 1,1,2,1 update cadence
            case 2: return 2.0f;
            case 3: return 4.0f;
            default: return 1.0f;
        }
    }

    float rateSpeed() const {
        // Appletini's canonical backend advances six control units per
        // (16-RATE) ticks. RATE=$A is therefore the nominal 1x setting.
        const uint8_t rate = registers_[2] >> 4;
        return 6.0f / static_cast<float>(16u - rate);
    }

    uint32_t articulationSamples() const {
        uint8_t shift;
        switch ((registers_[3] >> 4) & 0x07) {
            case 0:
            case 1: shift = 5; break;
            case 2:
            case 3: shift = 4; break;
            case 4:
            case 5: shift = 3; break;
            case 6: shift = 2; break;
            default: shift = 1; break;
        }
        // The canonical core applies delta>>shift at its 20 kHz control tick.
        // This compact backend approximates that settling time in samples.
        return 64u << shift;
    }

    static float lerp(float from, float to, float amount) {
        return from + (to - from) * amount;
    }

    float transitionProgress(uint32_t elapsed) const {
        return std::min(1.0f, static_cast<float>(elapsed) /
            static_cast<float>(transition_samples_));
    }

    void configureFormants(float progress) {
        formants_[0].configure(lerp(previous_profile_.f1, profile_.f1, progress), 5.0f);
        formants_[1].configure(lerp(previous_profile_.f2, profile_.f2, progress), 7.0f);
        formants_[2].configure(lerp(previous_profile_.f3, profile_.f3, progress), 9.0f);
    }

    void startPhoneme(uint8_t phone) {
        phoneme_ = phone & 0x3F;
        previous_profile_ = profile_;
        profile_ = profileFor(phoneme_);

        const float playback_speed = durationSpeed() * rateSpeed();
        samples_total_ = std::max<uint32_t>(1, static_cast<uint32_t>(
            static_cast<float>(profile_.duration_ms) *
            static_cast<float>(kSampleRate) / (1000.0f * playback_speed)));
        samples_remaining_ = samples_total_;
        transition_samples_ = articulationSamples();
        formant_update_countdown_ = 0;
        active_ = true;
        ready_ = false;
        completion_pending_ = false;
        phase_ = 0.0f;

        pitch_start_hz_ = pitch_hz_;
        pitch_target_hz_ = pitchFromRegisters();
        if (current_function_ != 3) {
            pitch_start_hz_ = pitch_target_hz_;
        }

        configureFormants(0.0f);
        for (auto &filter : formants_) {
            filter.reset();
        }
    }

    float generateSample() {
        constexpr float two_pi = 6.28318530717958647692f;

        const uint32_t elapsed = samples_total_ - samples_remaining_;
        const float progress = static_cast<float>(elapsed) /
                               static_cast<float>(samples_total_);
        const float formant_progress = transitionProgress(elapsed);
        if (formant_update_countdown_ == 0) {
            configureFormants(formant_progress);
            formant_update_countdown_ = 31;
        } else {
            --formant_update_countdown_;
        }

        // Function 3 is transitioned-inflection mode. The three low RATEINF
        // bits select the slope; larger values settle toward the target more
        // quickly, matching the ordering of the canonical control law.
        float pitch_progress = 1.0f;
        if (current_function_ == 3) {
            const float slope = static_cast<float>(registers_[2] & 0x07);
            pitch_progress = std::min(1.0f, progress * (1.0f + slope * 0.75f));
        }
        pitch_hz_ = lerp(pitch_start_hz_, pitch_target_hz_, pitch_progress);

        phase_ += pitch_hz_ / static_cast<float>(kSampleRate);
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }

        // A softened glottal pulse retains harmonics for the formants without
        // the harsh aliasing of a raw square wave.
        const float glottal = 0.65f * std::sin(two_pi * phase_) +
                              0.25f * std::sin(two_pi * phase_ * 2.0f) +
                              0.10f * std::sin(two_pi * phase_ * 3.0f);

        const uint32_t feedback = ((noise_lfsr_ >> 0) ^ (noise_lfsr_ >> 2) ^
                                   (noise_lfsr_ >> 3) ^ (noise_lfsr_ >> 5)) & 1u;
        noise_lfsr_ = (noise_lfsr_ >> 1) | (feedback << 15);
        const float noise = (noise_lfsr_ & 1u) ? 1.0f : -1.0f;
        const float voiced = lerp(previous_profile_.voiced, profile_.voiced,
                                  formant_progress);
        const float noise_level = lerp(previous_profile_.noise, profile_.noise,
                                       formant_progress);
        const float excitation = glottal * voiced + noise * noise_level;

        float sample = formants_[0].process(excitation) * 1.00f +
                       formants_[1].process(excitation) * 0.72f +
                       formants_[2].process(excitation) * 0.42f;

        // Short attack/release eliminates clicks between phonemes.
        const float attack = std::min(1.0f, static_cast<float>(elapsed) / 180.0f);
        const float release = std::min(1.0f,
            static_cast<float>(samples_remaining_) / 220.0f);
        const float amplitude = static_cast<float>(registers_[3] & 0x0F) / 15.0f;
        sample *= attack * release * amplitude * 0.62f;

        // FILTER=$FF is the SSI-263 silence setting. The backend continues to
        // run so completion/D7 timing remains correct while output is muted.
        if (registers_[4] == 0xFF || (registers_[3] & 0x80) != 0 ||
            (registers_[3] & 0x0F) == 0) {
            sample = 0.0f;
        }

        if (--samples_remaining_ == 0) {
            active_ = false;
            ready_ = true;
            completion_pending_ = true;
        }
        return sample;
    }

    static float clampAudio(float value) {
        return std::clamp(value, -1.0f, 1.0f);
    }

    std::array<uint8_t, 5> registers_{};
    std::array<Resonator, 3> formants_{};
    Profile profile_{500, 1500, 2500, 0.0f, 0.0f, 45};
    Profile previous_profile_{500, 1500, 2500, 0.0f, 0.0f, 45};
    bool ready_ = false;
    bool active_ = false;
    bool completion_pending_ = false;
    bool interrupts_enabled_ = false;
    uint8_t current_function_ = 0;
    uint8_t phoneme_ = 0;
    uint32_t samples_remaining_ = 0;
    uint32_t samples_total_ = 0;
    uint32_t transition_samples_ = 1;
    uint32_t formant_update_countdown_ = 0;
    float phase_ = 0.0f;
    float pitch_hz_ = 30.5f;
    float pitch_start_hz_ = 30.5f;
    float pitch_target_hz_ = 30.5f;
    uint32_t noise_lfsr_ = 0x1ACEu;
};

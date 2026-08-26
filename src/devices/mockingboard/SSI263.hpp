/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "PhasorAudio.hpp"

// SSI-263A register interface and formant speech synthesizer.
//
// Audio is synthesized in real time from a glottal source and an LFSR noise
// source passed through the SC-01A/SSI-263 vocal-tract topology. No recorded
// phoneme samples are used.
class SSI263 {
public:
    // Appletini's verified formant coefficient set and audio pipeline run at
    // 48 kHz. Keep the host implementation on that same timebase so the
    // fixed-point filters are evaluating the identical transfer functions.
    static constexpr uint32_t kSampleRate = PhasorAudio::kSampleRate;
    static constexpr uint32_t kXckRate = 1000000;

    SSI263();
    ~SSI263();

    SSI263(const SSI263 &) = delete;
    SSI263 &operator=(const SSI263 &) = delete;
    SSI263(SSI263 &&) noexcept;
    SSI263 &operator=(SSI263 &&) noexcept;

    // A cold reset clears the power-up registers and all analog/digital
    // history. An SSI-263AP warm reset powers speech down while retaining the
    // programming registers and the transitioned-inflection state.
    void reset(bool cold_start = true);

    // Advance one effective SSI XCK rising edge. GSSquared's NClock callback
    // is already downstream of the Phasor Q3/DIV2 clocking and calls this once
    // per video cycle (about 1 MHz).
    void clockXck();

    // In Mockingboard-compatible mode the primary SSI socket is selected by
    // A6 over offsets $40-$7F. A2..A0 select the register, so registers 0..7
    // repeat throughout the window (4..7 are filter-frequency aliases).
    static constexpr bool isMockingboardWriteOffset(uint8_t offset) {
        return offset >= 0x40 && offset <= 0x7F;
    }

    static constexpr uint8_t registerForOffset(uint8_t offset) {
        return offset & 0x07;
    }

    // Registers 0..4 are DURPHON, INFLECT, RATEINF, CTTRAMP and FILFREQ.
    // The chip decodes registers 4..7 as aliases of FILFREQ.
    void write(uint8_t reg, uint8_t value);
    uint8_t read(uint8_t reg) const;

    bool ready() const;
    bool active() const;
    bool interruptsEnabled() const;
    uint8_t phoneme() const;
    uint32_t samplesRemaining() const;
    uint32_t samplesTotal() const;
    float pitchHz() const;
    float activePitchHz() const;

    // Exact native SSI-263A/SC-02 parameter ROM row (selector bytes 7..0).
    static uint64_t nativeParameterRow(uint8_t phone);

    bool takeCompletion();

    // Render exactly one mono sample at kSampleRate. Response/D7 timing remains
    // driven exclusively by clockXck().
    float renderSample();

    // Mix mono speech into an interleaved stereo Mockingboard frame.
    void mixSamples(std::vector<float> &stereo, uint32_t sample_count);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

// Integer phase clocks for AY tone, noise, and envelope.
// Pitch/period changes recompute the increment and leave phase (synth5).

namespace ay {

enum class Waveform : uint8_t {
    Square     = 0,
    SoftSquare = 1,
    WarmSine   = 2,
    Organ      = 3,
    Sawtooth   = 4,
    Count      = 5,
};

namespace detail {

constexpr double PI = 3.1415926535897932384626433832795;
constexpr double TWO_PI = 2.0 * PI;
constexpr uint64_t NYQUIST_PHASE_INC = 1ULL << 31;

// Fundamental amplitude of a [0, 1] square wave is 2/pi (~0.6366).
// Additive waveforms are bipolar around 0 so volume/mixer gating (silence = 0)
// does not thump. Live square ↔ additive switches will DC-pop; that is expected.
constexpr float SINE_FUND_AMP = static_cast<float>(2.0 / PI); // ~0.63662

// 1024-step (+1 guard entry) bipolar sine lookup table in [-1.0, 1.0],
// phase-aligned with the AY square wave:
//   [0, pi)   -> > 0 (peak +1 at pi/2)
//   [pi, 2pi) -> < 0 (trough -1 at 3pi/2)
struct SineTable {
    static constexpr uint32_t BITS = 10;
    static constexpr uint32_t SIZE = 1U << BITS;        // 1024
    static constexpr uint32_t SHIFT = 32 - BITS;        // 22
    static constexpr uint32_t FRAC_MASK = (1U << SHIFT) - 1U;
    static constexpr float INV_FRAC_SCALE = 1.0f / static_cast<float>(1U << SHIFT);

    float data[SIZE + 1];

    SineTable() {
        for (uint32_t i = 0; i < SIZE; ++i) {
            data[i] = static_cast<float>(
                std::sin(TWO_PI * static_cast<double>(i) / static_cast<double>(SIZE)));
        }
        data[SIZE] = data[0];
    }
};

inline float evalSinBipolar(uint32_t phase) {
    static const SineTable lut;
    const uint32_t idx = phase >> SineTable::SHIFT;
    const float frac = static_cast<float>(phase & SineTable::FRAC_MASK) * SineTable::INV_FRAC_SCALE;
    return lut.data[idx] + frac * (lut.data[idx + 1] - lut.data[idx]);
}

struct Harmonic {
    uint32_t order;
    float weight;
};

// Band-limited additive synthesis from the shared sine LUT. Each harmonic is
// included only while order * f_tone < sample_rate / 2, preventing aliasing.
template <size_t N>
inline float evalAdditive(uint32_t phase, uint64_t phase_inc,
                          const Harmonic (&harmonics)[N], float gain) {
    float sum = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        const uint32_t k = harmonics[i].order;
        if (phase_inc * k < NYQUIST_PHASE_INC) {
            sum += harmonics[i].weight * evalSinBipolar(phase * k);
        }
    }
    return gain * sum;
}

// Warm Sine: smooth sine body with gentle 2nd & 3rd harmonics for presence.
inline float evalWarmSine(uint32_t phase, uint64_t phase_inc) {
    static constexpr Harmonic kHarmonics[] = {
        { 1, 1.00f },
        { 2, 0.25f },
        { 3, 0.12f },
    };
    return evalAdditive(phase, phase_inc, kHarmonics, SINE_FUND_AMP);
}

// Organ: drawbar-style blend of fundamental through 8th harmonic.
inline float evalOrgan(uint32_t phase, uint64_t phase_inc) {
    static constexpr Harmonic kHarmonics[] = {
        { 1, 1.00f },
        { 2, 0.50f },
        { 3, 0.30f },
        { 4, 0.20f },
        { 5, 0.14f },
        { 6, 0.10f },
        { 7, 0.08f },
        { 8, 0.06f },
    };
    return evalAdditive(phase, phase_inc, kHarmonics, 0.56f);
}

// Soft Square: band-limited odd harmonics 1, 3, ..., 15 (Fourier series of square).
inline float evalSoftSquare(uint32_t phase, uint64_t phase_inc) {
    static constexpr Harmonic kHarmonics[] = {
        { 1,  1.0f },
        { 3,  1.0f / 3.0f },
        { 5,  1.0f / 5.0f },
        { 7,  1.0f / 7.0f },
        { 9,  1.0f / 9.0f },
        { 11, 1.0f / 11.0f },
        { 13, 1.0f / 13.0f },
        { 15, 1.0f / 15.0f },
    };
    return evalAdditive(phase, phase_inc, kHarmonics, SINE_FUND_AMP);
}

// Sawtooth: band-limited harmonics 1..8 (bright strings / harpsichord character).
inline float evalSawtooth(uint32_t phase, uint64_t phase_inc) {
    static constexpr Harmonic kHarmonics[] = {
        { 1,  1.0f },
        { 2, -1.0f / 2.0f },
        { 3,  1.0f / 3.0f },
        { 4, -1.0f / 4.0f },
        { 5,  1.0f / 5.0f },
        { 6, -1.0f / 6.0f },
        { 7,  1.0f / 7.0f },
        { 8, -1.0f / 8.0f },
    };
    return evalAdditive(phase, phase_inc, kHarmonics, 0.48f);
}

} // namespace detail

class Oscillator {
protected:
    uint32_t phase_counter_ = 0;
    uint64_t phase_increment_ = 0;
    uint32_t period_ = 0;
    uint64_t vid_per_sec_ = 1020484;
    uint32_t sample_rate_ = 44100;

    static constexpr uint32_t PHASE_COUNTER_HALF = 1UL << 31;
    static constexpr uint64_t TONE_CLOCK_DIV = 16;

    void calculatePhaseIncrement() {
        if (period_ == 0 || sample_rate_ == 0) {
            phase_increment_ = 0;
            return;
        }
        const uint64_t denom = TONE_CLOCK_DIV * (uint64_t)period_ * sample_rate_;
        phase_increment_ = (vid_per_sec_ << 32) / denom;
    }

public:
    virtual ~Oscillator() = default;

    void setClockRate(uint64_t vid_per_sec, uint32_t sample_rate) {
        vid_per_sec_ = vid_per_sec;
        sample_rate_ = sample_rate;
        calculatePhaseIncrement();
    }

    void setPeriod(uint32_t period) {
        period_ = period;
        calculatePhaseIncrement();
    }

    uint32_t getPeriod() const { return period_; }

    uint32_t getPhase() const { return phase_counter_; }

    void resetPhase() { phase_counter_ = 0; }

    bool output() const { return phase_counter_ < PHASE_COUNTER_HALF; }

    bool isAboveNyquist() const { return phase_increment_ >= PHASE_COUNTER_HALF; }

    void advance() {
        const uint64_t next = (uint64_t)phase_counter_ + phase_increment_;
        phase_counter_ = (uint32_t)next;
    }

    virtual int16_t generateSample() = 0;
};

class SquareWaveOscillator : public Oscillator {
public:
    bool step() {
        const bool high = output();
        advance();
        return high;
    }

    int16_t generateSample() override {
        return step() ? 1 : 0;
    }
};

class ToneOscillator : public Oscillator {
public:
    float sample(Waveform waveform) const {
        switch (waveform) {
            case Waveform::SoftSquare:
                return detail::evalSoftSquare(phase_counter_, phase_increment_);
            case Waveform::WarmSine:
                return detail::evalWarmSine(phase_counter_, phase_increment_);
            case Waveform::Organ:
                return detail::evalOrgan(phase_counter_, phase_increment_);
            case Waveform::Sawtooth:
                return detail::evalSawtooth(phase_counter_, phase_increment_);
            case Waveform::Square:
            default:
                return output() ? 1.0f : 0.0f;
        }
    }

    float step(Waveform waveform) {
        const float s = sample(waveform);
        advance();
        return s;
    }

    int16_t generateSample() override {
        return static_cast<int16_t>(step(Waveform::Square) * 32767.0f);
    }
};

class NoiseOscillator {
    uint32_t phase_counter_ = 0;
    uint64_t phase_increment_ = 0;
    uint32_t period_ = 1;
    uint32_t rng_ = 1;
    bool output_ = false;
    uint64_t vid_per_sec_ = 1020484;
    uint32_t sample_rate_ = 44100;

    static constexpr uint64_t NOISE_CLOCK_DIV = 16;

    void clockLfsr() {
        const uint32_t bit0 = rng_ & 1;
        const uint32_t bit3 = (rng_ >> 3) & 1;
        rng_ = (rng_ >> 1) | ((bit0 ^ bit3) << 16);
        output_ = (rng_ & 1) != 0;
    }

    void calculatePhaseIncrement() {
        const uint32_t np = period_ ? period_ : 1;
        if (sample_rate_ == 0) {
            phase_increment_ = 0;
            return;
        }
        const uint64_t denom = NOISE_CLOCK_DIV * (uint64_t)np * sample_rate_;
        phase_increment_ = (vid_per_sec_ << 32) / denom;
    }

public:
    void setClockRate(uint64_t vid_per_sec, uint32_t sample_rate) {
        vid_per_sec_ = vid_per_sec;
        sample_rate_ = sample_rate;
        calculatePhaseIncrement();
    }

    void setPeriod(uint32_t period) {
        period_ = period ? period : 1;
        calculatePhaseIncrement();
    }

    uint32_t getPeriod() const { return period_; }

    void resetPhase() {
        phase_counter_ = 0;
        rng_ = 1;
        output_ = false;
    }

    bool output() const { return output_; }

    bool step() {
        const uint64_t next = (uint64_t)phase_counter_ + phase_increment_;
        const uint32_t wraps = (uint32_t)(next >> 32);
        phase_counter_ = (uint32_t)next;
        for (uint32_t i = 0; i < wraps; i++) {
            clockLfsr();
        }
        return output_;
    }
};

class EnvelopeGenerator {
    uint64_t phase_counter_ = 0;
    uint64_t phase_increment_ = 0;
    uint32_t envelope_period_ = 0;
    uint64_t vid_per_sec_ = 1020484;
    uint32_t sample_rate_ = 44100;

    static constexpr uint64_t PHASE_COUNTER_MAX = 1ULL << 40;

    void calculatePhaseIncrement() {
        if (envelope_period_ == 0 || sample_rate_ == 0) {
            phase_increment_ = 0;
            return;
        }
        phase_increment_ = (PHASE_COUNTER_MAX * vid_per_sec_) /
                           ((uint64_t)sample_rate_ * envelope_period_ * 256);
    }

public:
    void setClockRate(uint64_t vid_per_sec, uint32_t sample_rate) {
        vid_per_sec_ = vid_per_sec;
        sample_rate_ = sample_rate;
        calculatePhaseIncrement();
    }

    void setEnvelopePeriod(uint32_t envelope_period) {
        envelope_period_ = envelope_period;
        calculatePhaseIncrement();
    }

    uint32_t getEnvelopePeriod() const { return envelope_period_; }

    void resetPhase() { phase_counter_ = 0; }

    // Advance one output sample. Returns how many of the 16 envelope
    // steps elapsed (0..15 for a single wrap).
    uint32_t step() {
        const uint8_t before = (uint8_t)(phase_counter_ >> 36);
        phase_counter_ += phase_increment_;
        if (phase_counter_ >= PHASE_COUNTER_MAX) {
            phase_counter_ -= PHASE_COUNTER_MAX;
        }
        const uint8_t after = (uint8_t)(phase_counter_ >> 36);
        if (after >= before) {
            return (uint32_t)(after - before);
        }
        return (uint32_t)(16 - before + after);
    }
};

} // namespace ay

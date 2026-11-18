#pragma once

#include <cstdint>

// Integer phase clocks for AY tone, noise, and envelope.
// Pitch/period changes recompute the increment and leave phase (synth5).

namespace ay {

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

    void resetPhase() { phase_counter_ = 0; }

    bool output() const { return phase_counter_ < PHASE_COUNTER_HALF; }

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

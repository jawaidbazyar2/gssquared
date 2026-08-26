/*
 *   Copyright (c) 2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace PhasorAudio {

// Appletini's verified SSI-263 fixed-point filter bank and the combined
// Phasor card stream both run on this timebase.  Keeping one definition avoids
// silently resampling the AY and speech halves of the same card differently.
inline constexpr uint32_t kSampleRate = 48000;

// The card synth and the host playback device are independent clocks. Feeding
// SDL exactly one video frame of audio per video frame leaves no useful margin
// for a normal 1024-frame device callback, scheduler jitter, or even small
// crystal-rate error. Keep a modest queue and let SDL's stream resampler absorb
// the clock difference; the SSI and AY generators themselves always remain on
// the exact 48 kHz timebase above.
class OutputClockRecovery {
public:
    static constexpr uint32_t kPrefillMilliseconds = 50;
    static constexpr uint32_t kTargetMilliseconds = 60;
    static constexpr float kMaximumRateAdjustment = 0.005f;
    static constexpr uint32_t kPrefillFrames =
        (kSampleRate * kPrefillMilliseconds) / 1000;
    static constexpr uint32_t kTargetFrames =
        (kSampleRate * kTargetMilliseconds) / 1000;

    void reset() {
        primed_ = false;
        ratio_ = 1.0f;
    }

    bool needsPrefill(uint32_t queued_frames) const {
        return !primed_ || queued_frames == 0;
    }

    void markPrefilled() { primed_ = true; }

    float update(uint32_t queued_frames) {
        if (!primed_ || kTargetFrames == 0) {
            ratio_ = 1.0f;
            return ratio_;
        }

        float error =
            (static_cast<float>(queued_frames) -
             static_cast<float>(kTargetFrames)) /
            static_cast<float>(kTargetFrames);
        if (error > 1.0f) error = 1.0f;
        if (error < -1.0f) error = -1.0f;

        // SDL consumes input faster for ratios above 1.0 and slower below
        // 1.0. A deep queue therefore speeds playback up slightly; a shallow
        // queue slows it slightly. The bound is deliberately inaudible while
        // comfortably covering ordinary audio-clock error.
        ratio_ = 1.0f + error * kMaximumRateAdjustment;
        return ratio_;
    }

    float ratio() const { return ratio_; }
    bool primed() const { return primed_; }

private:
    bool primed_ = false;
    float ratio_ = 1.0f;
};

// Appletini's Phasor output uses a fixed +8 warmth setting after the completed
// AY+speech card mix.  Its three one-poles run at 133.333 MHz; these Q1.31
// injection coefficients analytically collapse the average 2777.778 FPGA
// clocks between 48 kHz samples without changing the emulated sample rate.
// Per-clock fixed-point rounding can differ from this collapsed form by a few
// PCM LSBs; preserving the transfer in the sample domain avoids thousands of
// host operations per output sample.
// This is card coloration, not part of the SSI-263 synthesis backend.
class WarmthChannel {
public:
    static constexpr int32_t kLowInjectionQ31 = 89120856;
    static constexpr int32_t kWarmInjectionQ31 = 334906882;
    static constexpr int32_t kMidInjectionQ31 = 617599807;
    static constexpr int32_t kWarmthKnee = 20480;

    void reset() {
        low_q12_ = 0;
        warm_q12_ = 0;
        mid_q12_ = 0;
    }

    int16_t processPcm(int16_t input) {
        // Multiplication is defined for negative PCM values; left-shifting a
        // negative signed integer is undefined in C++.
        const int64_t target = static_cast<int64_t>(input) * 4096;
        low_q12_ = updatePole(low_q12_, target, kLowInjectionQ31);
        warm_q12_ = updatePole(warm_q12_, target, kWarmInjectionQ31);
        mid_q12_ = updatePole(mid_q12_, target, kMidInjectionQ31);

        const int32_t low = static_cast<int32_t>(floorDivPow2(low_q12_, 12));
        const int32_t warm = static_cast<int32_t>(floorDivPow2(warm_q12_, 12));
        const int32_t mid = static_cast<int32_t>(floorDivPow2(mid_q12_, 12));
        const int32_t warm_band = saturatePcm(warm - low);
        const int32_t treble_band = saturatePcm(
            static_cast<int32_t>(input) - mid);

        // With Appletini's forced +8 setting, the adjustable stage reduces to
        // base + warm band - one quarter of the treble band.
        const int32_t shaped = static_cast<int32_t>(input) + warm_band -
            static_cast<int32_t>(floorDivPow2(treble_band, 2));
        return saturatePcm(applyWarmthKnee(shaped));
    }

    float process(float input) {
        return static_cast<float>(processPcm(quantizePcm(input))) / 32768.0f;
    }

    static int16_t quantizePcm(float input) {
        if (input >= 1.0f) return 32767;
        if (input <= -1.0f) return -32768;
        return saturatePcm(static_cast<int32_t>(
            std::lround(static_cast<double>(input) * 32768.0)));
    }

    static constexpr int32_t applyWarmthKnee(int32_t sample) {
        if (sample > kWarmthKnee) {
            const int32_t excess = sample - kWarmthKnee;
            return kWarmthKnee +
                static_cast<int32_t>(floorDivPow2(excess, 1)) +
                static_cast<int32_t>(floorDivPow2(excess, 3));
        }
        if (sample < -kWarmthKnee) {
            const int32_t excess = -kWarmthKnee - sample;
            return -kWarmthKnee -
                static_cast<int32_t>(floorDivPow2(excess, 1)) -
                static_cast<int32_t>(floorDivPow2(excess, 3));
        }
        return sample;
    }

private:
    static constexpr int64_t floorDivPow2(int64_t value, unsigned shift) {
        if (value >= 0) return value >> shift;
        const int64_t magnitude = -value;
        return -((magnitude + ((int64_t{1} << shift) - 1)) >> shift);
    }

    static constexpr int16_t saturatePcm(int32_t sample) {
        return sample > 32767 ? 32767 :
               (sample < -32768 ? -32768 : static_cast<int16_t>(sample));
    }

    static int32_t updatePole(int32_t state, int64_t target,
                              int32_t injection_q31) {
        const int64_t delta = target - static_cast<int64_t>(state);
        return static_cast<int32_t>(static_cast<int64_t>(state) +
            floorDivPow2(delta * injection_q31, 31));
    }

    int32_t low_q12_ = 0;
    int32_t warm_q12_ = 0;
    int32_t mid_q12_ = 0;
};

struct StereoSample {
    float left;
    float right;
};

class WarmthFilter {
public:
    void reset() {
        left_.reset();
        right_.reset();
    }

    StereoSample process(float left, float right) {
        return {left_.process(left), right_.process(right)};
    }

private:
    WarmthChannel left_;
    WarmthChannel right_;
};

} // namespace PhasorAudio

/*
 * SSI-263A formant speech synthesis for GSSquared.
 *
 * The SC-01A digital-control structure, ROM decoding, analog vocal-tract
 * topology, and switched-capacitor filter equations are based on Olivier
 * Galibert's vsim and MAME Votrax work. The SSI-263 register controls and
 * SC-01A parameter mapping follow the Appletini SystemVerilog implementation.
 *
 * Copyright (c) 2015 Olivier Galibert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of vsim nor the names of its contributors may be used to
 *   endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "SSI263.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
// These are the two independent clocks in Appletini's verified backend. The
// digital parameter core advances at 20 kHz while the fixed-point audio path
// consumes one sample at 48 kHz.
constexpr double kControlClockHz = 20000.0;
constexpr double kFormantCapClockHz = 20000.0;
constexpr int kCoefficientFractionBits = 15;
constexpr int32_t kOutputSlewStep = 3000;
constexpr uint8_t kSsiOutputScale = 10;
constexpr uint8_t kNoiseShaperInputShift = 6;
constexpr uint8_t kF2NoiseInputGainShift = 3;

// SSI-263 phoneme/allophone starting targets in the compatible SC-01A tract
// parameter space.  Appletini's original inverse table left nineteen valid
// SSI codes mapped to STOP.  This explicit table covers every non-hold SSI
// symbol; the few places where the two chips allocate fricative energy
// differently are refined after lookup.  HV/HVC/HFC/HN are stateful and are
// handled before this table.
constexpr std::array<uint8_t, 64> kSsi263ToSc01a = {
    // This is Appletini's exact compatibility map. Native formant targets
    // still come from the SSI ROM; a mapped SC-01 row contributes only the
    // retained closure/delay traits.
    0x03, 0x2C, 0x00, 0x22, 0x3F, 0x29, 0x3F, 0x09,
    0x05, 0x21, 0x01, 0x02, 0x2E, 0x2F, 0x08, 0x15,
    0x13, 0x26, 0x3F, 0x16, 0x36, 0x3F, 0x28, 0x3F,
    0x33, 0x32, 0x31, 0x23, 0x3A, 0x2B, 0x3F, 0x3F,
    0x18, 0x3F, 0x3F, 0x2D, 0x0E, 0x1A, 0x1C, 0x25,
    0x04, 0x19, 0x3F, 0x3F, 0x1B, 0x3F, 0x3F, 0x07,
    0x1F, 0x3F, 0x10, 0x0F, 0x1D, 0x38, 0x39, 0x0C,
    0x0D, 0x14, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
};

// Native SSI-263A parameter ROM. Each 64-bit row contains selector bytes
// 7..0. These are the active first 512 bytes of the verified SC-02 dump
// (SHA-256 101d129a5f104e6190f2eca518bbf9ef65bf4ff92684d29eba56d9641aa02b0).
constexpr std::array<uint64_t, 64> kSsi263ParameterRom = {
    0x00000000C00A9070ULL, 0x0000C000E00EE120ULL,
    0x0000A000D00EE150ULL, 0x0000B000E00ED110ULL,
    0x00006000B00EC120ULL, 0x0000C000E00EE130ULL,
    0x00009000F00EF110ULL, 0x00008000C00EA150ULL,
    0x00008000B00EB160ULL, 0x00008000A00E9160ULL,
    0x00008000B00E8190ULL, 0x00008000B00E91A0ULL,
    0x00006000B00E91D0ULL, 0x00006000B00E71F0ULL,
    0x00006000B00E31F0ULL, 0x00007000B00E41F0ULL,
    0x00006000A00E21D0ULL, 0x00008000B00E1170ULL,
    0x00009000B00E1150ULL, 0x00008000A00E2180ULL,
    0x0000A000A00E6130ULL, 0x00009000A00E4140ULL,
    0x0000A000800E1130ULL, 0x0000A000700E0110ULL,
    0x0000A000B00E4180ULL, 0x00008000B00E31A0ULL,
    0x00006000B00E31C0ULL, 0x00007000B00E51C0ULL,
    0x00008000300E4160ULL, 0x00008000100E1130ULL,
    0x00008000400E3120ULL, 0x00006000900E6170ULL,
    0x00007000E00E3130ULL, 0x0000F000F00E5110ULL,
    0x00009000E00E1150ULL, 0x00008000900E0130ULL,
    0x00008000C00C3110ULL, 0x00008000E00C9110ULL,
    0x0000A000800EA130ULL, 0x00F00000800C2041ULL,
    0x00F00000E0049041ULL, 0x00400000800CA031ULL,
    0x00006000C00A9170ULL, 0x0000F000C0089170ULL,
    0x00800000C00A9071ULL, 0x00800000C0089071ULL,
    0x00004000C03A9170ULL, 0x00F02000D0062030ULL,
    0x00F00000C0067001ULL, 0x00F02000E00EB020ULL,
    0x00900000E00EB021ULL, 0x00806000900E3020ULL,
    0x00800000900E3021ULL, 0x00402000E0067030ULL,
    0x00600000A0068051ULL, 0x0000F000903E3100ULL,
    0x0000F000D03E8100ULL, 0x00008000E03EC120ULL,
    0x00008000A00E9170ULL, 0x00006000900E8120ULL,
    0x0000A000900E7110ULL, 0x0000A000A00E9100ULL,
    0x00007000A00E7160ULL, 0x0000F000E00E1110ULL,
};

constexpr uint8_t nativeTarget(uint8_t phone, uint8_t selector) {
    return static_cast<uint8_t>((kSsi263ParameterRom[phone & 0x3F] >>
        (static_cast<unsigned>(selector) * 8U + 4U)) & 0x0FULL);
}

constexpr uint8_t nativeF2ToSc01(uint8_t value) {
    // The F2 bank needs even five-bit retained indices. D maps to E and E/F
    // map to F after comparing normalized native and SC-01 capacitances.
    return value < 0x0D ? value : (value == 0x0D ? 0x0E : 0x0F);
}

// Decoded from sc01a.bin (CRC32 fc416227, SHA1
// 1d6da90b1807a01b5e186ef08476119a862b5e6d). These are control parameters,
// not audio samples.
constexpr std::array<uint64_t, 64> kSc01aRom = {
    0x0000036174688127ULL, 0x01000161D4688127ULL,
    0x020009A1C4688127ULL, 0x030000E0F0A050A4ULL,
    0x040000FB610316E8ULL, 0x0500016164C9C1A6ULL,
    0x060007A134C9C1A6ULL, 0x07000463F3CB546CULL,
    0x08000161C4E940A3ULL, 0x09000B61806191A6ULL,
    0x0A000A61906191A6ULL, 0x0B0009A1906191A6ULL,
    0x0C0007A366A58832ULL, 0x0D000A61E6241936ULL,
    0x0E00017390E19122ULL, 0x0F000163F7D36428ULL,
    0x10000163FB8B546CULL, 0x110009A2FB8B546CULL,
    0x120001639CD15860ULL, 0x130008A0706980A3ULL,
    0x140009A0D4084B36ULL, 0x150008A184E940A3ULL,
    0x160007A130498123ULL, 0x17000A2120498123ULL,
    0x180007A1F409D0A2ULL, 0x19000A721123642CULL,
    0x1A0000E8DB7B342CULL, 0x1B000162FD2204ACULL,
    0x1C000173E041C126ULL, 0x1D0007A265832CA8ULL,
    0x1E000B7C00E89126ULL, 0x1F000468489132E0ULL,
    0x20000A2184C9C1A6ULL, 0x210005617069D326ULL,
    0x22000A6164A01226ULL, 0x230000E3548981A3ULL,
    0x24000CC184E940A3ULL, 0x250007B2631324A8ULL,
    0x26000A2184E8C1A2ULL, 0x27000A21806191A6ULL,
    0x28000A2180E8C122ULL, 0x290007A164015326ULL,
    0x2A000172E81132E0ULL, 0x2B00046354084382ULL,
    0x2C000A207049D326ULL, 0x2D000A661460C122ULL,
    0x2E000A2074E880A7ULL, 0x2F0007A074E880A7ULL,
    0x30000461606980A3ULL, 0x31000163548981A3ULL,
    0x320007A1E48981A3ULL, 0x33000A21B48981A3ULL,
    0x34000A6134E8C1A2ULL, 0x350009A180E8C1A2ULL,
    0x36000366106083A2ULL, 0x3700046190E8C122ULL,
    0x38000A6388E15220ULL, 0x39000168183800A4ULL,
    0x3A0008A12448C382ULL, 0x3B000A2194688127ULL,
    0x3C0009A19049D326ULL, 0x3D000CC1B06980A3ULL,
    0x3E000A2300A050A4ULL, 0x3F0000F030A058A4ULL,
};

constexpr uint8_t bit(uint64_t word, unsigned position) {
    return static_cast<uint8_t>((word >> position) & 1ULL);
}

constexpr uint8_t bits4(uint64_t word, unsigned b3, unsigned b2,
                        unsigned b1, unsigned b0) {
    return static_cast<uint8_t>((bit(word, b3) << 3) |
                                (bit(word, b2) << 2) |
                                (bit(word, b1) << 1) |
                                bit(word, b0));
}

struct PhoneParameters {
    uint8_t fa = 0;
    uint8_t fc = 0;
    uint8_t va = 0;
    uint8_t f1 = 0;
    uint8_t f2 = 0;
    uint8_t f2q = 0;
    uint8_t f3 = 0;
    uint8_t closure_delay = 0;
    uint8_t voice_delay = 0;
    uint8_t duration = 0;
    bool closure = false;
    bool pause = false;
};

PhoneParameters decodePhone(uint8_t phone) {
    const uint64_t word = kSc01aRom[phone & 0x3F];
    PhoneParameters result;
    result.f1 = bits4(word, 0, 7, 14, 21);
    result.va = bits4(word, 1, 8, 15, 22);
    result.f2 = bits4(word, 2, 9, 16, 23);
    result.fc = bits4(word, 3, 10, 17, 24);
    result.f2q = bits4(word, 4, 11, 18, 25);
    result.f3 = bits4(word, 5, 12, 19, 26);
    result.fa = bits4(word, 6, 13, 20, 27);
    result.closure_delay = bits4(word, 34, 32, 30, 28);
    result.voice_delay = bits4(word, 35, 33, 31, 29);
    result.closure = bit(word, 36) != 0;
    result.duration = static_cast<uint8_t>(((!bit(word, 37)) << 6) |
        ((!bit(word, 38)) << 5) | ((!bit(word, 39)) << 4) |
        ((!bit(word, 40)) << 3) | ((!bit(word, 41)) << 2) |
        ((!bit(word, 42)) << 1) | (!bit(word, 43)));
    result.pause = phone == 0x03 || phone == 0x3E;
    return result;
}

PhoneParameters decodeNativeSsiPhone(uint8_t ssi_phone, uint8_t sc01_phone) {
    PhoneParameters result = decodePhone(sc01_phone);

    if (sc01_phone == 0x3F) {
        // An unmapped compatibility row must not leak SC-01 STOP closure or
        // delay traits onto an otherwise complete native SSI phone.
        result.closure_delay = 0;
        result.voice_delay = 0;
        result.closure = false;
    }

    // Keep the exact native phone identity for every SSI parameter bank. The
    // SC-01 row contributes only its retained closure/delay traits.
    result.f1 = nativeTarget(ssi_phone, 0);
    result.f2 = nativeF2ToSc01(nativeTarget(ssi_phone, 1));
    result.f2q = nativeTarget(ssi_phone, 2);
    result.f3 = nativeTarget(ssi_phone, 3);
    result.fc = 0x0F; // SSI has no SC-01 FC field; full scale is neutral.
    result.va = nativeTarget(ssi_phone, 5);
    result.fa = nativeTarget(ssi_phone, 6);

    result.pause = result.va == 0 && result.fa == 0;
    result.duration = 31; // Audio timing is owned by the native XCK counters.
    return result;
}

int64_t arithmeticShiftRight(int64_t value, unsigned bits) {
    if (value >= 0) {
        return value >> bits;
    }
    const int64_t magnitude = -value;
    return -((magnitude + (int64_t{1} << bits) - 1) >> bits);
}

int32_t sat24(int64_t value) {
    return static_cast<int32_t>(std::clamp<int64_t>(
        value, -(int64_t{1} << 23), (int64_t{1} << 23) - 1));
}

int16_t sat16(int64_t value) {
    return static_cast<int16_t>(std::clamp<int64_t>(value, -32768, 32767));
}

int16_t quantizeCoefficient(double value) {
    const int64_t quantized = static_cast<int64_t>(
        std::llround(value * static_cast<double>(1 << kCoefficientFractionBits)));
    return sat16(quantized);
}

template <size_t PreviousInputs, size_t PreviousOutputs>
class FixedFilter {
public:
    static constexpr size_t kCoefficientCount =
        1 + PreviousInputs + PreviousOutputs;

    void reset() {
        x_.fill(0);
        y_.fill(0);
    }

    int32_t process(
        int32_t input,
        const std::array<int16_t, kCoefficientCount> &coefficients) {
        int64_t total = static_cast<int64_t>(input) * coefficients[0];
        for (size_t i = 0; i < PreviousInputs; ++i) {
            total += static_cast<int64_t>(x_[i]) * coefficients[1 + i];
        }
        for (size_t i = 0; i < PreviousOutputs; ++i) {
            total += static_cast<int64_t>(y_[i]) *
                     coefficients[1 + PreviousInputs + i];
        }
        const int32_t output = sat24(arithmeticShiftRight(
            total, kCoefficientFractionBits));

        for (size_t i = PreviousInputs; i > 1; --i) {
            x_[i - 1] = x_[i - 2];
        }
        if constexpr (PreviousInputs != 0) {
            x_[0] = input;
        }
        for (size_t i = PreviousOutputs; i > 1; --i) {
            y_[i - 1] = y_[i - 2];
        }
        if constexpr (PreviousOutputs != 0) {
            y_[0] = output;
        }
        return output;
    }

private:
    std::array<int32_t, PreviousInputs> x_{};
    std::array<int32_t, PreviousOutputs> y_{};
};

double bitsToCaps(uint32_t value, std::initializer_list<double> caps) {
    double total = 0.0;
    for (double cap : caps) {
        if (value & 1U) {
            total += cap;
        }
        value >>= 1;
    }
    return total;
}

std::array<int16_t, 7> standardFilterCoefficients(
    double c1t, double c1b, double c2t, double c2b,
    double c3, double c4) {
    const double k0 = c1t != 0.0
        ? c1t / (kFormantCapClockHz * c1b) : 0.0;
    const double k1 = c2t != 0.0
        ? c4 * c2t / (kFormantCapClockHz * c1b * c3) : 0.0;
    const double k2 = c4 * c2b /
                      (kFormantCapClockHz * kFormantCapClockHz * c1b * c3);
    const double peak = std::sqrt(std::fabs(k0 * k1 - k2)) /
                        (2.0 * kPi * k2);
    const double zc = 2.0 * kPi * peak /
                      std::tan(kPi * peak / SSI263::kSampleRate);
    const double m0 = zc * k0;
    const double m1 = zc * k1;
    const double m2 = zc * zc * k2;
    const std::array<double, 4> a = {
        1.0 + m0, 3.0 + m0, 3.0 - m0, 1.0 - m0};
    const std::array<double, 4> b = {
        1.0 + m1 + m2, 3.0 + m1 - m2,
        3.0 - m1 - m2, 1.0 - m1 + m2};
    std::array<int16_t, 7> result{};
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = quantizeCoefficient(a[i] / b[0]);
    }
    for (size_t i = 1; i < b.size(); ++i) {
        result[3 + i] = quantizeCoefficient(-b[i] / b[0]);
    }
    return result;
}

std::array<int16_t, 2> lowpassFilterCoefficients(
    double c1t, double c1b) {
    const double k = c1b / (kFormantCapClockHz * c1t) * (150.0 / 4000.0);
    const double peak = 1.0 / (2.0 * kPi * k);
    const double zc = 2.0 * kPi * peak /
                      std::tan(kPi * peak / SSI263::kSampleRate);
    const double m = zc * k;
    const double b0 = 1.0 + m;
    return {quantizeCoefficient(1.0 / b0),
            quantizeCoefficient(-(1.0 - m) / b0)};
}

std::array<int16_t, 5> noiseShaperFilterCoefficients(
    double c1, double c2t, double c2b, double c3, double c4) {
    const double k0 = c2t * c3 * c2b / c4;
    const double k1 = c2t * (kFormantCapClockHz * c2b);
    const double k2 = c1 * c2t * c3 / (kFormantCapClockHz * c4);
    const double peak = std::sqrt(1.0 / k2) / (2.0 * kPi);
    const double zc = 2.0 * kPi * peak /
                      std::tan(kPi * peak / SSI263::kSampleRate);
    const double m0 = zc * k0;
    const double m1 = zc * k1;
    const double m2 = zc * zc * k2;
    const double b0 = 1.0 + m1 + m2;
    return {
        quantizeCoefficient(m0 / b0),
        0,
        quantizeCoefficient(-m0 / b0),
        quantizeCoefficient(-(2.0 - 2.0 * m2) / b0),
        quantizeCoefficient(-(1.0 - m1 + m2) / b0),
    };
}

struct FormantCoefficientTables {
    std::array<std::array<int16_t, 7>, 16> f1{};
    std::array<std::array<int16_t, 7>, 32 * 16> f2{};
    std::array<std::array<int16_t, 7>, 16> f3{};
    std::array<int16_t, 7> f4{};
    std::array<int16_t, 5> fn{};
    std::array<int16_t, 2> fx{};

    FormantCoefficientTables() {
        for (uint32_t index = 0; index < f1.size(); ++index) {
            f1[index] = standardFilterCoefficients(
                11247, 11797, 949, 52067,
                2280 + bitsToCaps(index, {2546, 4973, 9861, 19724}),
                166272);
        }
        for (uint32_t f2_index = 0; f2_index < 32; ++f2_index) {
            for (uint32_t f2q = 0; f2q < 16; ++f2q) {
                f2[(f2_index << 4) | f2q] = standardFilterCoefficients(
                    24840, 29154,
                    829 + bitsToCaps(f2q, {1390, 2965, 5875, 11297}),
                    38180,
                    2352 + bitsToCaps(
                        f2_index, {833, 1663, 3164, 6327, 12654}),
                    34270);
            }
        }
        for (uint32_t index = 0; index < f3.size(); ++index) {
            f3[index] = standardFilterCoefficients(
                0, 17594, 868, 18828,
                8480 + bitsToCaps(index, {2226, 4485, 9056, 18111}),
                50019);
        }
        f4 = standardFilterCoefficients(0, 28810, 1165, 21457, 8558, 7289);
        fn = noiseShaperFilterCoefficients(15500, 14854, 8450, 9523, 14083);
        fx = lowpassFilterCoefficients(1122, 23131);
    }
};

const FormantCoefficientTables &formantCoefficients() {
    static const FormantCoefficientTables tables;
    return tables;
}

uint16_t inflectionWord(const std::array<uint8_t, 5> &registers) {
    return static_cast<uint16_t>(((registers[2] & 0x08) ? 0x0800 : 0) |
        (static_cast<uint16_t>(registers[1]) << 3) | (registers[2] & 0x07));
}

uint16_t pitchPeriod(uint16_t inflection) {
    const uint32_t span = 4096U - inflection;
    return static_cast<uint16_t>(std::max<uint32_t>(1, (span * 5U) >> 5));
}

uint8_t articulationShift(uint8_t articulation) {
    switch (articulation & 0x07) {
        case 0:
        case 1: return 5;
        case 2:
        case 3: return 4;
        case 4:
        case 5: return 3;
        case 6: return 2;
        default: return 1;
    }
}

uint8_t interpolate8(uint8_t current, uint8_t target,
                     uint8_t articulation) {
    const uint16_t target_value = static_cast<uint16_t>(target) << 4;
    if (target_value == current) {
        return current;
    }
    const uint16_t delta = target_value > current
        ? target_value - current : current - target_value;
    const uint16_t step = std::max<uint16_t>(1,
        delta >> articulationShift(articulation));
    if (target_value > current) {
        return static_cast<uint8_t>(std::min<uint16_t>(255, current + step));
    }
    return static_cast<uint8_t>(step > current ? 0 : current - step);
}

struct FormantCore {
    PhoneParameters phone{};
    uint8_t sc01_phone = 0x3F;

    uint32_t control_accumulator = 0;
    uint8_t ticks = 0;
    uint8_t update_counter = 0;

    uint16_t pitch = 0;
    uint16_t pitch_limit = 255;
    bool pitch_noise_gate = false;
    uint16_t noise = 0;
    bool noise_bit = false;
    bool closure_active = true;
    uint8_t closure_age = 0;

    uint16_t target_inflection = 0;
    uint16_t active_inflection = 0;
    bool transitioned_inflection_seeded = false;

    uint8_t cur_fa = 0;
    uint8_t cur_fc = 0;
    uint8_t cur_va = 0;
    uint8_t cur_f1 = 0;
    uint8_t cur_f2 = 0;
    uint8_t cur_f2q = 0;
    uint8_t cur_f3 = 0;

    uint8_t filt_fa = 0;
    uint8_t filt_fc = 0;
    uint8_t filt_va = 0;
    uint8_t filt_f1 = 0;
    uint8_t filt_f2 = 0;
    uint8_t filt_f2q = 0;
    uint8_t filt_f3 = 0;

    bool filter_dirty = true;
    bool phone_done = false;

    uint8_t noiseStopBurstGain() const {
        if (ticks <= phone.voice_delay) {
            return 0;
        }
        switch (ticks - phone.voice_delay) {
            case 1:
            case 2: return 7;
            case 3: return 5;
            case 4: return 3;
            case 5: return 2;
            default: return 0;
        }
    }

    uint8_t voicedStopAttackGain() const {
        if (ticks < phone.closure_delay) {
            return 0;
        }
        switch (ticks - phone.closure_delay) {
            case 0:
            case 1: return 7;
            case 2: return 6;
            case 3: return 4;
            case 4: return 2;
            case 5: return 1;
            default: return 0;
        }
    }

    uint8_t closureGain() const {
        const bool voiced_stop = phone.closure && phone.fa == 0 && phone.va != 0;
        if (voiced_stop) {
            return voicedStopAttackGain();
        }
        if (filt_fa != 0 && filt_va == 0) {
            return phone.closure ? noiseStopBurstGain() : 7;
        }
        return static_cast<uint8_t>(7U ^ (closure_age >> 2));
    }

    void reset(bool cold_start) {
        const uint16_t retained_inflection = active_inflection;
        const bool retained_seed = transitioned_inflection_seeded;
        *this = FormantCore{};
        phone = decodePhone(0x3F);
        if (!cold_start) {
            active_inflection = retained_inflection;
            transitioned_inflection_seeded = retained_seed;
        }
    }

    static uint8_t inflectionSlopeStep(uint8_t slope) {
        constexpr std::array<uint8_t, 8> steps = {1, 2, 3, 4, 6, 8, 12, 16};
        return steps[slope & 0x07];
    }

    void start(uint8_t ssi_phone, uint8_t current_function,
               const std::array<uint8_t, 5> &registers) {
        const uint8_t requested_phone = ssi_phone & 0x3F;
        sc01_phone = kSsi263ToSc01a[requested_phone];
        phone = decodeNativeSsiPhone(requested_phone, sc01_phone);
        ticks = 0;
        phone_done = false;

        const uint16_t next_inflection = inflectionWord(registers);
        target_inflection = next_inflection;
        if (current_function != 3) {
            active_inflection = next_inflection;
        } else if (!transitioned_inflection_seeded) {
            // U65/U64 have no useful reset. Seed once from the first live
            // transitioned target rather than gliding from an artificial 0.
            active_inflection = next_inflection;
            transitioned_inflection_seeded = true;
        } else {
            active_inflection = static_cast<uint16_t>(
                (next_inflection & 0x083F) | (active_inflection & 0x07C0));
        }
        pitch_limit = pitchPeriod(active_inflection);
        if (phone.closure_delay == 0) {
            closure_active = phone.closure;
        }
    }

    void advanceDurationFrame() {
        if (ticks == 0x0F) {
            ticks = 0;
            phone_done = true;
            return;
        }

        ++ticks;
        if (ticks == phone.closure_delay) {
            closure_active = phone.closure;
        }
    }

    void advanceInflection(uint8_t current_function,
                           const std::array<uint8_t, 5> &registers) {
        const uint16_t live = inflectionWord(registers);
        target_inflection = live;
        uint16_t next = live;
        if (current_function == 3) {
            const uint8_t active_target = (active_inflection >> 6) & 0x1F;
            const uint8_t target = (live >> 6) & 0x1F;
            const uint8_t step = inflectionSlopeStep((live >> 3) & 0x07);
            uint8_t moved = active_target;
            if (active_target < target) {
                moved = static_cast<uint8_t>(active_target +
                    std::min<uint8_t>(target - active_target, step));
            } else if (active_target > target) {
                moved = static_cast<uint8_t>(active_target -
                    std::min<uint8_t>(active_target - target, step));
            }
            next = static_cast<uint16_t>((live & 0x083F) | (moved << 6));
        }
        active_inflection = next;
    }

    void commitFilters() {
        filt_fa = cur_fa >> 4;
        filt_fc = cur_fc >> 4;
        filt_va = cur_va >> 4;
        filt_f1 = cur_f1 >> 4;
        filt_f2 = cur_f2 >> 3;
        filt_f2q = cur_f2q >> 4;
        filt_f3 = cur_f3 >> 4;
        filter_dirty = true;
    }

    void advanceControl(const std::array<uint8_t, 5> &registers) {
        update_counter = update_counter == 47 ? 0 : update_counter + 1;
        const bool tick_625 = (update_counter & 0x0F) == 0;
        const bool tick_208 = update_counter == 0x28;
        const uint8_t articulation = (registers[3] >> 4) & 0x07;

        if (tick_208 &&
            (!phone.pause || !(filt_fa || filt_va))) {
            cur_fc = interpolate8(cur_fc, phone.fc, articulation);
            cur_f1 = interpolate8(cur_f1, phone.f1, articulation);
            cur_f2 = interpolate8(cur_f2, phone.f2, articulation);
            cur_f2q = interpolate8(cur_f2q, phone.f2q, articulation);
            cur_f3 = interpolate8(cur_f3, phone.f3, articulation);
        }
        if (tick_625) {
            if ((ticks & 0x0F) >= phone.voice_delay) {
                cur_fa = interpolate8(cur_fa, phone.fa, articulation);
            }
            if ((ticks & 0x0F) >= phone.closure_delay) {
                cur_va = interpolate8(cur_va, phone.va, articulation);
            }
        }

        if (!closure_active && (filt_fa || filt_va)) {
            closure_age = 0;
        } else if (closure_age != 28) {
            ++closure_age;
        }
    }

    void advancePitchNoise() {
        const uint16_t next = pitch + 1;
        pitch = next >= pitch_limit
            ? static_cast<uint16_t>(next - pitch_limit) : next;
        pitch_noise_gate = pitch >= (pitch_limit >> 1);

        if ((pitch & 0x3F9) == 0x008) {
            commitFilters();
        }

        const bool input = noise_bit && noise != 0x7FFF;
        noise = static_cast<uint16_t>(((noise << 1) & 0x7FFE) |
                                      (input ? 1 : 0));
        noise_bit = (((noise >> 14) ^ (noise >> 13)) & 1U) == 0;
    }

    void advanceSample(uint8_t current_function,
                       const std::array<uint8_t, 5> &registers) {
        phone_done = false;
        control_accumulator += static_cast<uint32_t>(kControlClockHz);
        if (control_accumulator < SSI263::kSampleRate) {
            return;
        }
        control_accumulator -= SSI263::kSampleRate;

        advanceInflection(current_function, registers);
        pitch_limit = pitchPeriod(active_inflection);
        // RATE controls the XCK response/duration counters only. Articulation
        // continues at the fixed 20 kHz digital-control cadence.
        advanceControl(registers);
        advancePitchNoise();
    }
};

class FormantSynthesizer {
public:
    void resetHistory() {
        f1_.reset();
        f2_voice_.reset();
        f2_noise_.reset();
        f3_.reset();
        f4_.reset();
        noise_shaper_.reset();
        output_filter_.reset();
        presence_low_ = 0;
    }

    void reset() {
        resetHistory();
        output_ = 0;
        visible_output_ = 0;
    }

    void startPhone() {
        // Appletini aborts any in-flight multi-cycle synthesis pipeline when
        // a new SSI phone starts. Its audio_q register consequently retains
        // the sample that was visible at the preceding audio tick. Host-side
        // rendering computes that next sample atomically, so discard the
        // hidden look-ahead value before masking the old phone's histories.
        output_ = visible_output_;
        resetHistory();
    }

    float render(const FormantCore &sample_core,
                 const FormantCore &filter_core, bool excitation,
                 uint8_t amplitude) {
        // The RTL mixer observes audio_q at the audio-tick edge, then the
        // backend computes the sample launched by that edge.  Preserve that
        // one-sample output-register latency instead of exposing the newly
        // synthesized value a tick early.
        const int16_t visible_output = output_;
        visible_output_ = visible_output;
        static constexpr std::array<int16_t, 9> glottal = {
            0, -4681, 8192, 7022, 5851, 4681, 3511, 2340, 1170,
        };
        const FormantCoefficientTables &coefficients = formantCoefficients();

        const int32_t voice_source = excitation && sample_core.pitch < 72
            ? glottal[sample_core.pitch >> 3] : 0;
        const int32_t noise_source = !excitation ? 0 :
            (sample_core.pitch_noise_gate && sample_core.noise_bit ? 8192 : -8192);

        const int32_t voice_input = scale4(voice_source, sample_core.filt_va);
        const int32_t noise_input = sat24(
            static_cast<int64_t>(scale4(noise_source, sample_core.filt_fa)) <<
            kNoiseShaperInputShift);

        const int32_t f1 = f1_.process(
            voice_input, coefficients.f1[filter_core.filt_f1 & 0x0F]);
        const int32_t f2 = f2_voice_.process(
            f1, coefficients.f2[((filter_core.filt_f2 & 0x1F) << 4) |
                                (filter_core.filt_f2q & 0x0F)]);
        const int32_t fn = noise_shaper_.process(noise_input, coefficients.fn);
        const int32_t f2_noise_input = sat24(
            // RTL evaluates the F2-noise scaling late in the pipeline from
            // the live FC latch, after a pending control commit can land.
            static_cast<int64_t>(scale4(fn, filter_core.filt_fc)) <<
            kF2NoiseInputGainShift);
        const int32_t f2_noise = f2_noise_.process(
            f2_noise_input,
            coefficients.f2[((filter_core.filt_f2 & 0x1F) << 4) |
                            (filter_core.filt_f2q & 0x0F)]);

        const int32_t voice_noise = sat24(static_cast<int64_t>(f2) + f2_noise);
        const int32_t f3 = f3_.process(
            voice_noise, coefficients.f3[filter_core.filt_f3 & 0x0F]);
        const int32_t mixed = sat24(
            static_cast<int64_t>(f3) +
            scale20(fn, static_cast<uint8_t>(
                5 + (0x0F ^ sample_core.filt_fc))));
        const int32_t f4 = f4_.process(mixed, coefficients.f4);
        const int32_t closed = scale7(f4, sample_core.closureGain());
        const int32_t lowpassed = output_filter_.process(closed, coefficients.fx);

        int32_t enhanced = lowpassed;
        if (chFricative(filter_core)) {
            const int64_t high = static_cast<int64_t>(closed) - lowpassed;
            enhanced = sat24(static_cast<int64_t>(lowpassed) +
                             arithmeticShiftRight(high, 1) +
                             arithmeticShiftRight(high, 2));
        } else if (filter_core.filt_fa != 0 &&
                   !filter_core.phone.closure) {
            const int64_t high = static_cast<int64_t>(closed) - lowpassed;
            enhanced = sat24(static_cast<int64_t>(closed) +
                             arithmeticShiftRight(high, 2));
        } else if (filter_core.filt_va != 0 &&
                   filter_core.filt_fa == 0) {
            const int64_t high = static_cast<int64_t>(closed) - lowpassed;
            enhanced = sat24(static_cast<int64_t>(lowpassed) +
                             arithmeticShiftRight(high, 1) +
                             arithmeticShiftRight(high, 2));
        }

        enhanced = consonantAttack(filter_core, enhanced);
        const int32_t scaled = scale4(enhanced, amplitude);
        const int64_t presence_delta =
            static_cast<int64_t>(scaled) - presence_low_;
        const int32_t presence = sat24(
            static_cast<int64_t>(scaled) +
            arithmeticShiftRight(presence_delta, 1));
        presence_low_ = sat24(
            static_cast<int64_t>(presence_low_) +
            arithmeticShiftRight(presence_delta, 3));

        const int32_t output_scaled = scale4(presence, kSsiOutputScale);
        const int32_t output_gain = sat24(
            static_cast<int64_t>(output_scaled) << 1);
        const int16_t target = softLimit16(output_gain);
        output_ = slewLimit16(output_, target);
        return static_cast<float>(visible_output) / 32768.0f;
    }

private:
    static int32_t scale4(int32_t sample, uint8_t gain) {
        switch (gain & 0x0F) {
            case 0: return 0;
            case 1: return static_cast<int32_t>(arithmeticShiftRight(sample, 4));
            case 2: return static_cast<int32_t>(arithmeticShiftRight(sample, 3));
            case 3: return static_cast<int32_t>(arithmeticShiftRight(sample, 3) +
                                                arithmeticShiftRight(sample, 4));
            case 4: return static_cast<int32_t>(arithmeticShiftRight(sample, 2));
            case 5: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) +
                                                arithmeticShiftRight(sample, 4));
            case 6: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) +
                                                arithmeticShiftRight(sample, 3));
            case 7: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) +
                                                arithmeticShiftRight(sample, 3) +
                                                arithmeticShiftRight(sample, 4));
            case 8: return static_cast<int32_t>(arithmeticShiftRight(sample, 1));
            case 9: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                arithmeticShiftRight(sample, 4));
            case 10: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                 arithmeticShiftRight(sample, 3));
            case 11: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                 arithmeticShiftRight(sample, 3) +
                                                 arithmeticShiftRight(sample, 4));
            case 12: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                 arithmeticShiftRight(sample, 2));
            case 13: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                 arithmeticShiftRight(sample, 2) +
                                                 arithmeticShiftRight(sample, 4));
            case 14: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                 arithmeticShiftRight(sample, 2) +
                                                 arithmeticShiftRight(sample, 3));
            default: return sample;
        }
    }

    static int32_t scale7(int32_t sample, uint8_t gain) {
        switch (gain & 0x07) {
            case 0: return 0;
            case 1: return static_cast<int32_t>(arithmeticShiftRight(sample, 3) +
                                                arithmeticShiftRight(sample, 6) +
                                                arithmeticShiftRight(sample, 9));
            case 2: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) +
                                                arithmeticShiftRight(sample, 5) +
                                                arithmeticShiftRight(sample, 8));
            case 3: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) +
                                                arithmeticShiftRight(sample, 3) +
                                                arithmeticShiftRight(sample, 5) +
                                                arithmeticShiftRight(sample, 6));
            case 4: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                arithmeticShiftRight(sample, 4));
            case 5: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) +
                                                arithmeticShiftRight(sample, 3) +
                                                arithmeticShiftRight(sample, 4) +
                                                arithmeticShiftRight(sample, 6));
            case 6: return static_cast<int32_t>(sample -
                                                arithmeticShiftRight(sample, 3) -
                                                arithmeticShiftRight(sample, 6));
            default: return sample;
        }
    }

    static int32_t scale20(int32_t sample, uint8_t gain) {
        switch (gain & 0x1F) {
            case 0: return 0;
            case 1: return static_cast<int32_t>(arithmeticShiftRight(sample, 4));
            case 2: return static_cast<int32_t>(arithmeticShiftRight(sample, 3));
            case 3: return static_cast<int32_t>(arithmeticShiftRight(sample, 3) + arithmeticShiftRight(sample, 5));
            case 4:
            case 5: return static_cast<int32_t>(arithmeticShiftRight(sample, 2));
            case 6: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) + arithmeticShiftRight(sample, 4) - arithmeticShiftRight(sample, 6));
            case 7: return static_cast<int32_t>(arithmeticShiftRight(sample, 2) + arithmeticShiftRight(sample, 3) - arithmeticShiftRight(sample, 5));
            case 8: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) - arithmeticShiftRight(sample, 3) + arithmeticShiftRight(sample, 5));
            case 9: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) - arithmeticShiftRight(sample, 4));
            case 10: return static_cast<int32_t>(arithmeticShiftRight(sample, 1));
            case 11: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) + arithmeticShiftRight(sample, 4));
            case 12: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) + arithmeticShiftRight(sample, 3) - arithmeticShiftRight(sample, 5));
            case 13: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) + arithmeticShiftRight(sample, 3) + arithmeticShiftRight(sample, 5));
            case 14: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) + arithmeticShiftRight(sample, 2) - arithmeticShiftRight(sample, 4));
            case 15: return static_cast<int32_t>(arithmeticShiftRight(sample, 1) + arithmeticShiftRight(sample, 2));
            case 16: return static_cast<int32_t>(sample - arithmeticShiftRight(sample, 3) - arithmeticShiftRight(sample, 4));
            case 17: return static_cast<int32_t>(sample - arithmeticShiftRight(sample, 3) - arithmeticShiftRight(sample, 5));
            case 18: return static_cast<int32_t>(sample - arithmeticShiftRight(sample, 3) + arithmeticShiftRight(sample, 5));
            case 19: return static_cast<int32_t>(sample - arithmeticShiftRight(sample, 4));
            default: return sample;
        }
    }

    static int16_t softLimit16(int32_t sample) {
        int64_t limited = sample;
        if (sample > 8192) {
            limited = 8192 + arithmeticShiftRight(sample - 8192, 2);
        } else if (sample < -8192) {
            limited = -8192 + arithmeticShiftRight(sample + 8192, 2);
        }
        return sat16(limited);
    }

    static int16_t slewLimit16(int16_t previous, int16_t target) {
        const int32_t difference =
            static_cast<int32_t>(target) - static_cast<int32_t>(previous);
        if (difference > kOutputSlewStep) {
            return sat16(static_cast<int32_t>(previous) + kOutputSlewStep);
        }
        if (difference < -kOutputSlewStep) {
            return sat16(static_cast<int32_t>(previous) - kOutputSlewStep);
        }
        return target;
    }

    static bool chFricative(const FormantCore &core) {
        return core.sc01_phone == 0x10 && core.filt_fa != 0 &&
               core.filt_va == 0 && !core.phone.closure;
    }

    static uint8_t consonantAttackLevel(const FormantCore &core) {
        if (chFricative(core) || (core.phone.fa == 0 && core.phone.va == 0)) {
            return 0;
        }
        const uint8_t start_tick =
            core.phone.closure && core.phone.closure_delay <= core.phone.voice_delay
                ? core.phone.closure_delay
                : (core.phone.fa != 0 ? core.phone.voice_delay
                                      : core.phone.closure_delay);
        if (core.ticks < start_tick) {
            return 0;
        }
        const uint8_t age = core.ticks - start_tick;
        return age < 3 ? static_cast<uint8_t>(3 - age) : 0;
    }

    static int32_t consonantAttack(const FormantCore &core, int32_t sample) {
        if (core.filt_fa == 0 && !core.phone.closure) {
            return sample;
        }
        switch (consonantAttackLevel(core)) {
            case 3: return sat24(static_cast<int64_t>(sample) + arithmeticShiftRight(sample, 1));
            case 2: return sat24(static_cast<int64_t>(sample) + arithmeticShiftRight(sample, 2));
            case 1: return sat24(static_cast<int64_t>(sample) + arithmeticShiftRight(sample, 3));
            default: return sample;
        }
    }

    FixedFilter<3, 3> f1_;
    FixedFilter<3, 3> f2_voice_;
    FixedFilter<3, 3> f2_noise_;
    FixedFilter<3, 3> f3_;
    FixedFilter<3, 3> f4_;
    FixedFilter<2, 2> noise_shaper_;
    FixedFilter<0, 1> output_filter_;
    int32_t presence_low_ = 0;
    int16_t output_ = 0;
    int16_t visible_output_ = 0;
};

} // namespace

class SSI263::Impl {
public:
    void reset(bool cold_start) {
        if (cold_start) {
            registers = {0xC0, 0x00, 0x00, 0x80, 0x00};
            current_function = 0;
            phoneme = 0;
        } else {
            // SSI-263AP PD/RST retains DURPHON, INFLECT, RATEINF and FILFREQ.
            // CTTRAMP itself returns to the powered-down value.
            registers[3] = 0x80;
            phoneme = registers[0] & 0x3F;
        }
        ready = false;
        active = false;
        completion_pending = false;
        interrupts_enabled = false;
        acknowledge_guard = false;
        response_active = false;
        duration_active = false;
        response_ticks_left = 0;
        duration_ticks_left = 0;
        response_slot = 0;
        duration_frame = 0;
        samples_remaining = 0;
        samples_total = 0;
        samples_elapsed = 0;
        core.reset(cold_start);
        synth.reset();
    }

    void controlPowerDown() {
        // CTL is a live audio/control gate, not the chip's AP reset input.
        // Clear externally visible response state and stop this host-side
        // timing phase, but retain the fixed-point pipeline so its already
        // registered output follows the same staged, slew-limited path to
        // zero as Appletini.  A later CTL falling edge starts the retained
        // DURPHON value and masks the old filter histories.
        ready = false;
        active = false;
        completion_pending = false;
        interrupts_enabled = false;
        acknowledge_guard = false;
        response_active = false;
        duration_active = false;
        response_ticks_left = 0;
        duration_ticks_left = 0;
        response_slot = 0;
        duration_frame = 0;
        samples_remaining = 0;
        samples_total = 0;
        samples_elapsed = 0;
    }

    void latchModeAndInterrupts() {
        const uint8_t function = registers[0] >> 6;
        if (function != 0) {
            current_function = function;
            interrupts_enabled = true;
        } else {
            interrupts_enabled = false;
        }
    }

    void startPhoneme(uint8_t phone) {
        phoneme = phone & 0x3F;
        core.start(phoneme, current_function, registers);
        // A new native phone selects new switched-filter coefficients. The
        // verified Appletini backend masks the old IIR delay-line values when
        // that coefficient set changes.
        synth.startPhone();
        active = true;
        response_active = true;
        duration_active = true;
        ready = false;
        completion_pending = false;
        response_slot = 0;
        duration_frame = 0;
        response_ticks_left = responseSlotTicks();
        duration_ticks_left = durationSlotTicks();
        samples_elapsed = 0;
        samples_total = estimateSamples();
        samples_remaining = samples_total;
    }

    uint32_t responseSlotTicks() const {
        const uint32_t rate = registers[2] >> 4;
        return (16U - rate) * 256U;
    }

    uint32_t durationSlotTicks() const {
        const uint32_t duration = registers[0] >> 6;
        return (4U - duration) * responseSlotTicks();
    }

    uint64_t responseIntervalTicks() const {
        if (current_function == 1) {
            return static_cast<uint64_t>(responseSlotTicks()) * 16U;
        }
        if (current_function == 2 || current_function == 3) {
            return static_cast<uint64_t>(durationSlotTicks()) * 16U;
        }
        return 0;
    }

    uint32_t estimateSamples() const {
        const uint64_t ticks = responseIntervalTicks();
        if (ticks == 0) {
            return 0;
        }
        return static_cast<uint32_t>((ticks * SSI263::kSampleRate +
            SSI263::kXckRate - 1U) / SSI263::kXckRate);
    }

    void raiseResponseRequest() {
        if ((registers[3] & 0x80) != 0 || acknowledge_guard) {
            return;
        }
        if (!ready) {
            ready = true;
            completion_pending = true;
        }
        samples_remaining = 0;
    }

    void clockXck() {
        bool response_boundary = false;

        if (active && response_active) {
            if (--response_ticks_left == 0) {
                // A live RATE write takes effect at this 1/16-frame reload,
                // never by rewriting the slot already in progress.
                response_ticks_left = responseSlotTicks();
                if (response_slot == 0x0F) {
                    response_slot = 0;
                    response_boundary = current_function == 1;
                } else {
                    ++response_slot;
                }
            }
        }

        if (active && duration_active && --duration_ticks_left == 0) {
            // D and live RATE are sampled only when an internal duration slot
            // reloads. The tract repeats at slot 16 instead of stopping.
            duration_ticks_left = durationSlotTicks();
            core.advanceDurationFrame();
            if (duration_frame == 0x0F) {
                duration_frame = 0;
                response_boundary = response_boundary ||
                    current_function == 2 || current_function == 3;
            } else {
                ++duration_frame;
            }
        }

        if (response_boundary) {
            raiseResponseRequest();
        }
        // A reg0/1/2 write in this emulated cycle wins over a coincident
        // boundary. The following XCK edge may assert a new request normally.
        acknowledge_guard = false;
    }

    float generateSample() {
        const uint8_t amplitude = static_cast<uint8_t>(registers[3] & 0x0F);
        // Appletini suppresses both excitation paths while CTL is high or
        // amplitude is zero.  Continuing to drive hidden filter history here
        // would make a later unmute start from a waveform the hardware never
        // synthesized.
        const bool excite = active && (registers[3] & 0x80) == 0 &&
                            amplitude != 0;
        // The RTL latches source gains/pitch at the audio-tick edge, then its
        // pending 20 kHz control update becomes visible while the multi-stage
        // filter pipeline is running. Keep both views so a coefficient commit
        // affects this sample's filters without retroactively changing the
        // excitation that was latched at its start.
        const FormantCore sample_core = core;
        if (active) {
            core.advanceSample(current_function, registers);
            core.filter_dirty = false;
        }
        const float sample = synth.render(
            sample_core, core, excite, excite ? amplitude : 0);

        if (active) {
            ++samples_elapsed;
            if (samples_remaining != 0) {
                --samples_remaining;
            }
        }

        return sample;
    }

    std::array<uint8_t, 5> registers{};
    FormantCore core;
    FormantSynthesizer synth;
    bool ready = false;
    bool active = false;
    bool completion_pending = false;
    bool interrupts_enabled = false;
    bool acknowledge_guard = false;
    bool response_active = false;
    bool duration_active = false;
    uint8_t current_function = 0;
    uint8_t phoneme = 0;
    uint8_t response_slot = 0;
    uint8_t duration_frame = 0;
    uint32_t response_ticks_left = 0;
    uint32_t duration_ticks_left = 0;
    uint32_t samples_remaining = 0;
    uint32_t samples_total = 0;
    uint32_t samples_elapsed = 0;
};

SSI263::SSI263() : impl_(std::make_unique<Impl>()) {
    reset(true);
}

SSI263::~SSI263() = default;
SSI263::SSI263(SSI263 &&) noexcept = default;
SSI263 &SSI263::operator=(SSI263 &&) noexcept = default;

void SSI263::reset(bool cold_start) {
    impl_->reset(cold_start);
}

void SSI263::clockXck() {
    impl_->clockXck();
}

void SSI263::write(uint8_t reg, uint8_t value) {
    if (reg > 7) {
        return;
    }
    if (reg >= 4) {
        impl_->registers[4] = value;
        return;
    }

    const uint8_t old_control = impl_->registers[3];
    impl_->registers[reg] = value;

    // Writes to DURPHON, INFLECT or RATEINF acknowledge A/!R.
    if (reg <= 2) {
        impl_->ready = false;
        impl_->completion_pending = false;
        impl_->acknowledge_guard = true;
    }

    switch (reg) {
        case 0:
            impl_->phoneme = value & 0x3F;
            if ((impl_->registers[3] & 0x80) == 0) {
                impl_->startPhoneme(impl_->phoneme);
            }
            break;

        case 1:
        case 2:
            // ACK only. The current phone and both XCK timing phases continue.
            break;

        case 3:
            if (value & 0x80) {
                impl_->controlPowerDown();
            } else if (old_control & 0x80) {
                impl_->latchModeAndInterrupts();
                impl_->startPhoneme(impl_->registers[0] & 0x3F);
            }
            break;

        default:
            break;
    }
}

uint8_t SSI263::read(uint8_t /*reg*/) const {
    return impl_->ready ? 0x80 : 0x00;
}

bool SSI263::ready() const { return impl_->ready; }
bool SSI263::active() const { return impl_->active; }
bool SSI263::interruptsEnabled() const { return impl_->interrupts_enabled; }
uint8_t SSI263::phoneme() const { return impl_->phoneme; }
uint32_t SSI263::samplesRemaining() const { return impl_->samples_remaining; }
uint32_t SSI263::samplesTotal() const { return impl_->samples_total; }

float SSI263::pitchHz() const {
    const uint16_t inflection = inflectionWord(impl_->registers);
    return static_cast<float>(kControlClockHz) /
        static_cast<float>(pitchPeriod(inflection));
}

float SSI263::activePitchHz() const {
    return static_cast<float>(kControlClockHz) /
        static_cast<float>(pitchPeriod(impl_->core.active_inflection));
}

uint64_t SSI263::nativeParameterRow(uint8_t phone) {
    return kSsi263ParameterRom[phone & 0x3F];
}

bool SSI263::takeCompletion() {
    const bool pending = impl_->completion_pending;
    impl_->completion_pending = false;
    return pending;
}

float SSI263::renderSample() {
    return impl_->generateSample();
}

void SSI263::mixSamples(std::vector<float> &stereo, uint32_t sample_count) {
    const size_t required = static_cast<size_t>(sample_count) * 2;
    if (stereo.size() < required) {
        stereo.resize(required, 0.0f);
    }

    for (uint32_t i = 0; i < sample_count; ++i) {
        const float speech = renderSample();
        const size_t index = static_cast<size_t>(i) * 2;
        stereo[index] = std::clamp(stereo[index] + speech, -1.0f, 1.0f);
        stereo[index + 1] = std::clamp(stereo[index + 1] + speech,
                                       -1.0f, 1.0f);
    }
}

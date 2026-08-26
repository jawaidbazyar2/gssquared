#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "devices/mockingboard/SSI263.hpp"
#include "devices/mockingboard/W6522.hpp"

uint64_t debug_level = 0;

namespace {

constexpr std::array<uint64_t, 64> kExpectedNativeRows = {
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

// Generated from Appletini commit 9ddcdc3's SSI-263 RTL wrapper. The wrapper
// is driven by a 2,040,968 Hz raw XCK whose internal divide-by-two produces
// this effective response clock. Audio is sampled before advancing XCK, just
// as the wrapper exposes the previously registered audio_q value.
constexpr uint32_t kAppletiniEffectiveXckRate = 1020484;
constexpr size_t kAppletiniGoldenSamples = 5000;
constexpr size_t kAppletiniChainSegmentSamples = 1440;
constexpr std::array<uint8_t, 6> kAppletiniChainPhones = {
    0x2D, 0x20, 0x0C, 0x30, 0x07, 0x29,
};
constexpr size_t kAppletiniChainSamples =
    kAppletiniChainSegmentSamples * kAppletiniChainPhones.size();
static_assert(SSI263::kSampleRate == 48000,
              "Appletini SSI-263 parity requires the 48 kHz audio timebase");

// SHA-256 of the full Appletini history-mask regression stream serialized as
// signed PCM16 little-endian. This is the same 8,640-sample, six-phone chain
// whose text-form reference has SHA-256
// CFBF54F71DF24B36D399AC90C5EFB56DAA9CE1A99D4751E895B9B896FCB5AEA7.
constexpr std::array<uint8_t, 32> kAppletiniChainPcmSha256 = {
    0xED, 0x81, 0xB5, 0x86, 0xDA, 0x16, 0x7F, 0xD0,
    0x3D, 0x2B, 0x7B, 0x86, 0xEE, 0x16, 0xAD, 0x47,
    0xAA, 0x3A, 0x67, 0x35, 0xA8, 0xC2, 0x11, 0xE5,
    0x44, 0x30, 0xAC, 0xB9, 0xD9, 0xC7, 0xD7, 0xB2,
};

constexpr size_t kP08WindowOffset = 2320;
constexpr std::array<int16_t, 64> kP08Window = {
       0,    0,    0,    0,    0,    0,    0,   -8,
      -8,  -14,  -16,  -24,  -34,  -58,  -94, -148,
    -226, -324, -450, -600, -770, -954,-1140,-1328,
   -1498,-1650,-1770,-1854,-1904,-1908,-1874,-1798,
   -1676,-1518,-1316,-1066, -778, -450,  -94,  276,
     654, 1014, 1340, 1620, 1832, 1980, 2054, 2066,
    2024, 1940, 1842, 1744, 1664, 1630, 1642, 1722,
    1870, 2090, 2380, 2730, 3130, 3554, 3990, 4410,
};

constexpr size_t kP30WindowOffset = 2896;
constexpr std::array<int16_t, 64> kP30Window = {
       0,    0,    0,    0,    0,   -6,   -6,  -10,
     -16,  -26,  -40,  -68, -104, -156, -218, -294,
    -368, -446, -506, -546, -550, -526, -458, -360,
    -236,  -98,   42,  166,  272,  342,  380,  380,
     350,  286,  210,  110,    2, -108, -218, -318,
    -406, -476, -518, -530, -516, -474, -406, -324,
    -236, -148,  -70,   -4,   36,   62,   66,   64,
      54,   44,   32,   30,   30,   30,   30,   22,
};

struct PcmMetrics {
    int64_t sum;
    uint64_t absolute_sum;
    uint64_t squared_sum;
    int16_t minimum;
    int16_t maximum;
    uint32_t nonzero_samples;
    uint64_t fnv1a;
};

constexpr PcmMetrics kP08Metrics = {
    724642, 3451754, 9267102988ULL, -4584, 8162, 2671,
    0x0ED7299E2F42CC19ULL,
};
constexpr PcmMetrics kP30Metrics = {
    -861916, 1322884, 1417842304ULL, -2930, 2094, 2095,
    0xFD8AC75853C3FCE4ULL,
};

int16_t pcm16(float sample) {
    return static_cast<int16_t>(sample * 32768.0f);
}

constexpr uint32_t rotateRight(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32U - count));
}

std::array<uint8_t, 32> pcmSha256(const std::vector<int16_t> &samples) {
    constexpr std::array<uint32_t, 64> round_constants = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
        0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
        0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
        0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
        0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
        0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
        0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
        0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
        0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
    };
    std::vector<uint8_t> message;
    message.reserve(samples.size() * 2 + 72);
    for (int16_t sample : samples) {
        const uint16_t bits = static_cast<uint16_t>(sample);
        message.push_back(static_cast<uint8_t>(bits));
        message.push_back(static_cast<uint8_t>(bits >> 8));
    }
    const uint64_t message_bits = static_cast<uint64_t>(message.size()) * 8U;
    message.push_back(0x80);
    while ((message.size() & 63U) != 56U) message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<uint8_t>(message_bits >> shift));
    }

    std::array<uint32_t, 8> hash = {
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
    };
    for (size_t block = 0; block < message.size(); block += 64) {
        std::array<uint32_t, 64> words{};
        for (size_t word = 0; word < 16; ++word) {
            const size_t offset = block + word * 4;
            words[word] = (static_cast<uint32_t>(message[offset]) << 24) |
                          (static_cast<uint32_t>(message[offset + 1]) << 16) |
                          (static_cast<uint32_t>(message[offset + 2]) << 8) |
                          static_cast<uint32_t>(message[offset + 3]);
        }
        for (size_t word = 16; word < words.size(); ++word) {
            const uint32_t s0 = rotateRight(words[word - 15], 7) ^
                                rotateRight(words[word - 15], 18) ^
                                (words[word - 15] >> 3);
            const uint32_t s1 = rotateRight(words[word - 2], 17) ^
                                rotateRight(words[word - 2], 19) ^
                                (words[word - 2] >> 10);
            words[word] = words[word - 16] + s0 + words[word - 7] + s1;
        }

        uint32_t a = hash[0];
        uint32_t b = hash[1];
        uint32_t c = hash[2];
        uint32_t d = hash[3];
        uint32_t e = hash[4];
        uint32_t f = hash[5];
        uint32_t g = hash[6];
        uint32_t h = hash[7];
        for (size_t round = 0; round < words.size(); ++round) {
            const uint32_t upper_sigma = rotateRight(e, 6) ^
                                         rotateRight(e, 11) ^
                                         rotateRight(e, 25);
            const uint32_t choose = (e & f) ^ (~e & g);
            const uint32_t temp1 = h + upper_sigma + choose +
                                   round_constants[round] + words[round];
            const uint32_t lower_sigma = rotateRight(a, 2) ^
                                         rotateRight(a, 13) ^
                                         rotateRight(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = lower_sigma + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::array<uint8_t, 32> digest{};
    for (size_t word = 0; word < hash.size(); ++word) {
        for (size_t byte = 0; byte < 4; ++byte) {
            digest[word * 4 + byte] = static_cast<uint8_t>(
                hash[word] >> (24U - static_cast<unsigned>(byte) * 8U));
        }
    }
    return digest;
}

void clockTicks(SSI263 &speech, uint64_t ticks);

std::vector<int16_t> renderAppletiniReference(uint8_t phone,
                                               uint8_t filter_frequency) {
    SSI263 speech;
    // Full cold-wrapper programming order: FILFREQ, RATEINF, INFLECT,
    // DURPHON, then CTTRAMP. DURPHON is written while CTL is still high.
    speech.write(4, filter_frequency);
    speech.write(2, 0xA8);
    speech.write(1, 0x40);
    speech.write(0, phone);
    speech.write(3, 0x5A);

    std::vector<int16_t> pcm;
    pcm.reserve(kAppletiniGoldenSamples);
    std::vector<float> mixer_sample(2, 0.0f);
    uint64_t xck_phase = 0;
    for (size_t i = 0; i < kAppletiniGoldenSamples; ++i) {
        mixer_sample[0] = mixer_sample[1] = 0.0f;
        speech.mixSamples(mixer_sample, 1);
        pcm.push_back(pcm16(mixer_sample[0]));

        xck_phase += kAppletiniEffectiveXckRate;
        const uint64_t ticks = xck_phase / SSI263::kSampleRate;
        xck_phase %= SSI263::kSampleRate;
        clockTicks(speech, ticks);
    }
    return pcm;
}

std::vector<int16_t> renderAppletiniChainReference() {
    SSI263 speech;
    // Match scripts/test_ssi263_history_mask.py: all six phones share these
    // registers, run for 30 ms, and start back-to-back without XCK edges.
    speech.write(4, 0xE6);
    speech.write(2, 0xB8);
    speech.write(1, 0x52);
    speech.write(0, kAppletiniChainPhones.front());
    speech.write(3, 0x7B);

    std::vector<int16_t> pcm;
    pcm.reserve(kAppletiniChainSamples);
    for (size_t phone = 0; phone < kAppletiniChainPhones.size(); ++phone) {
        if (phone != 0) speech.write(0, kAppletiniChainPhones[phone]);
        for (size_t sample = 0; sample < kAppletiniChainSegmentSamples;
             ++sample) {
            pcm.push_back(pcm16(speech.renderSample()));
        }
    }
    return pcm;
}

PcmMetrics pcmMetrics(const std::vector<int16_t> &samples) {
    PcmMetrics result = {
        0, 0, 0, samples.front(), samples.front(), 0,
        14695981039346656037ULL,
    };
    for (int16_t sample : samples) {
        const int32_t value = sample;
        result.sum += value;
        result.absolute_sum += value < 0
            ? static_cast<uint64_t>(-static_cast<int64_t>(value))
            : static_cast<uint64_t>(value);
        result.squared_sum += static_cast<uint64_t>(
            static_cast<int64_t>(value) * value);
        result.minimum = std::min(result.minimum, sample);
        result.maximum = std::max(result.maximum, sample);
        result.nonzero_samples += sample != 0;

        // FNV-1a/64 over signed PCM16 serialized little-endian.
        const uint16_t bits = static_cast<uint16_t>(sample);
        result.fnv1a ^= static_cast<uint8_t>(bits);
        result.fnv1a *= 1099511628211ULL;
        result.fnv1a ^= static_cast<uint8_t>(bits >> 8);
        result.fnv1a *= 1099511628211ULL;
    }
    return result;
}

bool sameMetrics(const PcmMetrics &actual, const PcmMetrics &expected) {
    return actual.sum == expected.sum &&
           actual.absolute_sum == expected.absolute_sum &&
           actual.squared_sum == expected.squared_sum &&
           actual.minimum == expected.minimum &&
           actual.maximum == expected.maximum &&
           actual.nonzero_samples == expected.nonzero_samples &&
           actual.fnv1a == expected.fnv1a;
}

template <size_t N>
bool verifyAppletiniReference(const char *label,
                              const std::vector<int16_t> &samples,
                              size_t window_offset,
                              const std::array<int16_t, N> &window,
                              const PcmMetrics &expected_metrics) {
    if (samples.size() != kAppletiniGoldenSamples ||
        window_offset + window.size() > samples.size()) {
        std::fprintf(stderr, "SSI-263 %s reference length is invalid\n", label);
        return false;
    }
    for (size_t i = 0; i < window.size(); ++i) {
        if (samples[window_offset + i] != window[i]) {
            std::fprintf(stderr,
                "SSI-263 %s Appletini PCM mismatch at sample %zu "
                "(actual=%d expected=%d)\n",
                label, window_offset + i, samples[window_offset + i],
                window[i]);
            return false;
        }
    }

    const PcmMetrics actual = pcmMetrics(samples);
    if (!sameMetrics(actual, expected_metrics)) {
        std::fprintf(stderr,
            "SSI-263 %s Appletini stream mismatch "
            "(sum=%lld abs=%llu sq=%llu min=%d max=%d nz=%u "
            "fnv=%016llX)\n",
            label, static_cast<long long>(actual.sum),
            static_cast<unsigned long long>(actual.absolute_sum),
            static_cast<unsigned long long>(actual.squared_sum),
            actual.minimum, actual.maximum, actual.nonzero_samples,
            static_cast<unsigned long long>(actual.fnv1a));
        return false;
    }
    return true;
}

void clockTicks(SSI263 &speech, uint64_t ticks) {
    for (uint64_t tick = 0; tick < ticks; ++tick) {
        speech.clockXck();
    }
}

bool hasAudibleSamples(const std::vector<float> &samples) {
    return std::any_of(samples.begin(), samples.end(), [](float sample) {
        return std::fabs(sample) > 0.00001f;
    });
}

bool allSamplesInRange(const std::vector<float> &samples) {
    return std::all_of(samples.begin(), samples.end(), [](float sample) {
        return std::isfinite(sample) && sample >= -1.0f && sample <= 1.0f;
    });
}

float sampleEnergy(const std::vector<float> &samples) {
    float energy = 0.0f;
    for (float sample : samples) {
        energy += std::fabs(sample);
    }
    return energy;
}

float peakSample(const std::vector<float> &samples) {
    float peak = 0.0f;
    for (float sample : samples) {
        peak = std::max(peak, std::fabs(sample));
    }
    return peak;
}

float waveformDifference(const std::vector<float> &a,
                         const std::vector<float> &b) {
    const size_t count = std::min(a.size(), b.size());
    double difference = 0.0;
    for (size_t i = 0; i < count; ++i) {
        difference += std::fabs(static_cast<double>(a[i]) - b[i]);
    }
    return static_cast<float>(difference);
}

void configureSpeech(SSI263 &speech, uint8_t duration_phone, uint8_t inflection,
                     uint8_t rate_inflection, uint8_t control,
                     uint8_t filter = 0xE8) {
    speech.write(3, 0x80);
    speech.write(0, duration_phone);
    speech.write(1, inflection);
    speech.write(2, rate_inflection);
    speech.write(3, control);
    speech.write(4, filter);
}

void advanceXckForSamples(SSI263 &speech, uint32_t sample_count,
                          uint64_t &xck_phase) {
    xck_phase += static_cast<uint64_t>(sample_count) * SSI263::kXckRate;
    const uint64_t ticks = xck_phase / SSI263::kSampleRate;
    xck_phase %= SSI263::kSampleRate;
    for (uint64_t tick = 0; tick < ticks; ++tick) {
        speech.clockXck();
    }
}

void mixClocked(SSI263 &speech, std::vector<float> &audio,
                uint32_t sample_count, uint64_t &xck_phase) {
    const size_t required = static_cast<size_t>(sample_count) * 2;
    if (audio.size() < required) {
        audio.resize(required, 0.0f);
    }

    std::vector<float> part(2, 0.0f);
    for (uint32_t rendered = 0; rendered < sample_count; ++rendered) {
        part[0] = part[1] = 0.0f;
        speech.mixSamples(part, 1);
        const size_t destination = static_cast<size_t>(rendered) * 2;
        audio[destination] = std::clamp(audio[destination] + part[0],
                                         -1.0f, 1.0f);
        audio[destination + 1] = std::clamp(
            audio[destination + 1] + part[1], -1.0f, 1.0f);
        advanceXckForSamples(speech, 1, xck_phase);
    }
}

void mixClocked(SSI263 &speech, std::vector<float> &audio,
                uint32_t sample_count) {
    uint64_t xck_phase = 0;
    mixClocked(speech, audio, sample_count, xck_phase);
}

bool finishPhoneme(SSI263 &speech, N6522 *completion_via = nullptr) {
    std::vector<float> audio(256 * 2, 0.0f);
    uint32_t generated = 0;
    uint64_t xck_phase = 0;
    while (!speech.ready() && generated < SSI263::kSampleRate * 2) {
        std::fill(audio.begin(), audio.end(), 0.0f);
        mixClocked(speech, audio, 256, xck_phase);
        generated += 256;
        if (speech.takeCompletion() && speech.interruptsEnabled() && completion_via) {
            completion_via->signal_ca1_falling_edge();
        }
    }
    return speech.ready();
}

} // namespace

int main() {
    SSI263 speech;

    if (speech.active() || speech.ready() || speech.read(0) != 0x00) {
        std::fprintf(stderr, "SSI-263 reset state is incorrect\n");
        return 1;
    }

    // Use the same staged-start sequence as the Appletini showcase software.
    configureSpeech(speech, 0xC1, 0x40, 0xA8, 0x5A);

    if (!speech.active() || speech.ready() || speech.phoneme() != 0x01 ||
        !speech.interruptsEnabled()) {
        std::fprintf(stderr, "SSI-263 did not start the staged phoneme\n");
        return 1;
    }

    std::vector<float> audio(2048 * 2, 0.0f);
    uint64_t main_xck_phase = 0;
    mixClocked(speech, audio, 2048, main_xck_phase);
    if (!hasAudibleSamples(audio) || !allSamplesInRange(audio)) {
        std::fprintf(stderr,
            "SSI-263 did not produce valid audible samples (energy=%g peak=%g)\n",
            sampleEnergy(audio), peakSample(audio));
        return 1;
    }

    unsigned generated = 2048;
    while (!speech.ready() && generated < SSI263::kSampleRate) {
        std::fill(audio.begin(), audio.end(), 0.0f);
        mixClocked(speech, audio, 2048, main_xck_phase);
        generated += 2048;
    }
    if (!speech.ready() || !speech.active() || speech.read(0) != 0x80) {
        std::fprintf(stderr, "SSI-263 did not assert D7 after completion\n");
        return 1;
    }

    // Writing a new duration/phoneme clears ready and starts immediately when
    // the control bit is low.
    speech.write(0, 0xF0); // S
    if (!speech.active() || speech.ready() || speech.read(0) != 0x00 ||
        speech.phoneme() != 0x30) {
        std::fprintf(stderr, "SSI-263 immediate start/ready clear failed\n");
        return 1;
    }

    std::fill(audio.begin(), audio.end(), 0.0f);
    mixClocked(speech, audio, 2048);
    if (!hasAudibleSamples(audio) || !allSamplesInRange(audio)) {
        std::fprintf(stderr, "SSI-263 fricative synthesis failed\n");
        return 1;
    }

    // CTL is a live gate. Appletini's mixer sees the output already registered
    // by the prior audio tick, then the fixed 3000-PCM slew limiter brings the
    // pipeline to zero. It must not jump to immediate silence.
    SSI263 power_down_speech;
    SSI263 uninterrupted_speech;
    configureSpeech(power_down_speech, 0x08, 0x40, 0xA8, 0x5A, 0x00);
    configureSpeech(uninterrupted_speech, 0x08, 0x40, 0xA8, 0x5A, 0x00);
    std::vector<float> power_down_lead(3000 * 2, 0.0f);
    std::vector<float> uninterrupted_lead(3000 * 2, 0.0f);
    mixClocked(power_down_speech, power_down_lead, 3000);
    mixClocked(uninterrupted_speech, uninterrupted_lead, 3000);
    if (!power_down_speech.active()) {
        std::fprintf(stderr, "SSI-263 power-down setup did not remain active\n");
        return 1;
    }
    power_down_speech.write(3, 0xDA);
    if (power_down_speech.active() || power_down_speech.ready()) {
        std::fprintf(stderr, "SSI-263 warm power-down did not stop timing\n");
        return 1;
    }

    const int16_t retained_output = pcm16(power_down_speech.renderSample());
    const int16_t uninterrupted_output =
        pcm16(uninterrupted_speech.renderSample());
    if (retained_output == 0 || retained_output != uninterrupted_output) {
        std::fprintf(stderr,
            "SSI-263 CTL did not retain the registered output "
            "(stopped=%d running=%d)\n",
            retained_output, uninterrupted_output);
        return 1;
    }

    int16_t previous_output = retained_output;
    unsigned silent_run = 0;
    for (unsigned sample = 0; sample < 4096 && silent_run < 64; ++sample) {
        const int16_t output = pcm16(power_down_speech.renderSample());
        const int32_t step = std::abs(static_cast<int32_t>(output) -
                                      static_cast<int32_t>(previous_output));
        if (step > 3000) {
            std::fprintf(stderr,
                "SSI-263 CTL slew exceeded 3000 PCM at sample %u "
                "(previous=%d output=%d)\n",
                sample, previous_output, output);
            return 1;
        }
        silent_run = output == 0 ? silent_run + 1 : 0;
        previous_output = output;
    }
    if (silent_run != 64 || power_down_speech.active() ||
        power_down_speech.ready()) {
        std::fprintf(stderr,
            "SSI-263 CTL pipeline did not settle silently "
            "(silent_run=%u)\n", silent_run);
        return 1;
    }
    power_down_speech.write(3, 0x5A);
    if (!power_down_speech.active() || power_down_speech.ready()) {
        std::fprintf(stderr, "SSI-263 power-down release did not restart\n");
        return 1;
    }

    // Mockingboard write decode is A6 plus A2..A0. It must cover the entire
    // $40-$7F region, not just the five canonical register spellings.
    for (unsigned offset = 0; offset <= 0xFF; ++offset) {
        const bool expected = offset >= 0x40 && offset <= 0x7F;
        if (SSI263::isMockingboardWriteOffset(static_cast<uint8_t>(offset)) != expected) {
            std::fprintf(stderr, "SSI-263 Mockingboard decode failed at $%02X\n", offset);
            return 1;
        }
        if (expected && SSI263::registerForOffset(static_cast<uint8_t>(offset)) !=
                            (offset & 0x07)) {
            std::fprintf(stderr, "SSI-263 register alias failed at $%02X\n", offset);
            return 1;
        }
    }

    // The native SSI table is data, not a lossy inverse SC-01 phone map.
    for (size_t phone = 0; phone < kExpectedNativeRows.size(); ++phone) {
        if (SSI263::nativeParameterRow(static_cast<uint8_t>(phone)) !=
            kExpectedNativeRows[phone]) {
            std::fprintf(stderr,
                "SSI-263 native parameter row mismatch at $%02zX\n", phone);
            return 1;
        }
    }

    // Every public response mode and RATE value uses exact effective-XCK
    // counts. D7 is a request boundary: ACK clears it without stopping or
    // restarting the phone, and the next full interval asserts it again.
    for (uint8_t mode = 1; mode <= 3; ++mode) {
        for (uint8_t rate = 0; rate <= 15; ++rate) {
            SSI263 timed;
            timed.write(3, 0x80);
            timed.write(0, static_cast<uint8_t>((mode << 6) | 0x0B));
            timed.write(2, static_cast<uint8_t>(rate << 4));
            timed.write(3, 0x0F);

            const uint64_t frame_ticks = 4096ULL * (16U - rate);
            const uint64_t expected = mode == 1 ? frame_ticks :
                static_cast<uint64_t>(4U - mode) * frame_ticks;
            clockTicks(timed, expected - 1);
            if (timed.ready()) {
                std::fprintf(stderr,
                    "SSI-263 D7 was early (DR=%u RATE=%u)\n", mode, rate);
                return 1;
            }
            timed.clockXck();
            if (!timed.ready() || !timed.active() ||
                !timed.takeCompletion() || timed.takeCompletion()) {
                std::fprintf(stderr,
                    "SSI-263 D7 boundary failed (DR=%u RATE=%u)\n", mode, rate);
                return 1;
            }

            timed.write(1, 0x40);
            if (timed.ready() || !timed.active()) {
                std::fprintf(stderr,
                    "SSI-263 ACK restarted/stopped the phone (DR=%u RATE=%u)\n",
                    mode, rate);
                return 1;
            }
            clockTicks(timed, expected - 1);
            if (timed.ready()) {
                std::fprintf(stderr,
                    "SSI-263 repeating D7 was early (DR=%u RATE=%u)\n",
                    mode, rate);
                return 1;
            }
            timed.clockXck();
            if (!timed.ready() || !timed.takeCompletion()) {
                std::fprintf(stderr,
                    "SSI-263 phone did not repeat (DR=%u RATE=%u)\n",
                    mode, rate);
                return 1;
            }
        }
    }

    // DR=00 masks the external request while retaining the prior response
    // function. D7 still records the retained mode-3 duration boundary.
    for (uint8_t rate = 0; rate <= 15; ++rate) {
        SSI263 disabled;
        configureSpeech(disabled, 0xCB, 0x40,
                        static_cast<uint8_t>(rate << 4), 0x0F);
        disabled.write(3, 0x80);
        disabled.write(0, 0x0B);
        disabled.write(3, 0x0F);
        const uint64_t expected = 4ULL * 4096ULL * (16U - rate);
        clockTicks(disabled, expected - 1);
        if (disabled.ready()) {
            std::fprintf(stderr, "SSI-263 DR00 D7 was early at RATE=%u\n", rate);
            return 1;
        }
        disabled.clockXck();
        if (!disabled.ready() || disabled.interruptsEnabled() ||
            !disabled.takeCompletion()) {
            std::fprintf(stderr,
                "SSI-263 DR00 retained response failed at RATE=%u\n", rate);
            return 1;
        }
    }

    // A pending D7 is level state. Repeated boundaries do not manufacture
    // another completion edge, and an ACK coincident with a boundary wins.
    SSI263 edge_speech;
    configureSpeech(edge_speech, 0xCB, 0x40, 0xF0, 0x0F);
    constexpr uint64_t fastest_phone_ticks = 4096;
    clockTicks(edge_speech, fastest_phone_ticks);
    if (!edge_speech.takeCompletion()) {
        std::fprintf(stderr, "SSI-263 first completion edge was missing\n");
        return 1;
    }
    clockTicks(edge_speech, fastest_phone_ticks);
    if (!edge_speech.ready() || edge_speech.takeCompletion()) {
        std::fprintf(stderr, "SSI-263 duplicated a pending D7 edge\n");
        return 1;
    }
    edge_speech.write(1, 0x40);
    clockTicks(edge_speech, fastest_phone_ticks - 1);
    edge_speech.write(2, 0xF0);
    edge_speech.clockXck();
    if (edge_speech.ready() || edge_speech.takeCompletion()) {
        std::fprintf(stderr, "SSI-263 coincident ACK did not win\n");
        return 1;
    }
    clockTicks(edge_speech, fastest_phone_ticks);
    if (!edge_speech.ready() || !edge_speech.takeCompletion()) {
        std::fprintf(stderr, "SSI-263 response did not recover after ACK\n");
        return 1;
    }

    // RATE becomes live only when the current 1/16-frame slot reloads.
    SSI263 live_rate;
    configureSpeech(live_rate, 0x4B, 0x40, 0xF0, 0x0F);
    clockTicks(live_rate, 128);
    live_rate.write(2, 0xE0);
    constexpr uint64_t changed_rate_boundary = 256 + 15 * 512;
    clockTicks(live_rate, changed_rate_boundary - 128 - 1);
    if (live_rate.ready()) {
        std::fprintf(stderr, "SSI-263 live RATE rewrote the active slot\n");
        return 1;
    }
    live_rate.clockXck();
    if (!live_rate.ready()) {
        std::fprintf(stderr, "SSI-263 live RATE missed the next slot reload\n");
        return 1;
    }

    // A warm CTL stop cancels the active phase but retains registers. Waiting
    // while stopped cannot shorten the first response after CTL falls again.
    SSI263 stopped;
    configureSpeech(stopped, 0xCB, 0x55, 0xF3, 0x0F, 0xE7);
    clockTicks(stopped, 1000);
    stopped.write(3, 0x80);
    clockTicks(stopped, 9000);
    if (stopped.active() || stopped.ready() ||
        std::fabs(stopped.pitchHz() - 20000.0f / 533.0f) > 0.001f) {
        std::fprintf(stderr, "SSI-263 warm reset did not retain registers\n");
        return 1;
    }
    stopped.write(3, 0x0F);
    clockTicks(stopped, fastest_phone_ticks - 1);
    if (stopped.ready()) {
        std::fprintf(stderr, "SSI-263 stopped phase leaked into restart\n");
        return 1;
    }
    stopped.clockXck();
    if (!stopped.ready()) {
        std::fprintf(stderr, "SSI-263 restart did not get a full interval\n");
        return 1;
    }

    // RATEINF[7:4] controls playback rate while the low nibble participates in
    // pitch. Keep the low nibble fixed and require a higher RATE to finish
    // sooner without changing the target pitch.
    SSI263 rate_slow;
    SSI263 rate_fast;
    configureSpeech(rate_slow, 0x81, 0x40, 0x08, 0x5A);
    configureSpeech(rate_fast, 0x81, 0x40, 0xF8, 0x5A);
    if (rate_fast.samplesTotal() >= rate_slow.samplesTotal() ||
        std::fabs(rate_fast.pitchHz() - rate_slow.pitchHz()) > 0.001f) {
        std::fprintf(stderr, "SSI-263 RATE high-nibble control is incorrect\n");
        return 1;
    }

    // With the high nibble held constant, RATEINF bit 3 changes the top bit of
    // the 12-bit inflection word. It must change pitch but not duration.
    SSI263 pitch_low;
    SSI263 pitch_high;
    configureSpeech(pitch_low, 0x81, 0x40, 0xA0, 0x5A);
    configureSpeech(pitch_high, 0x81, 0x40, 0xA8, 0x5A);
    if (pitch_low.samplesTotal() != pitch_high.samplesTotal() ||
        pitch_high.pitchHz() <= pitch_low.pitchHz() * 1.5f) {
        std::fprintf(stderr, "SSI-263 12-bit inflection composition is incorrect\n");
        return 1;
    }

    // The RTL oscillator period is max(1, ((4096 - I) * 5) >> 5) at the
    // 20 kHz control clock. These exact points catch the old 125 kHz/span
    // approximation, which made every voice substantially lower pitched.
    struct PitchPoint {
        uint16_t inflection;
        uint16_t period;
    };
    constexpr PitchPoint pitch_points[] = {
        {0x000, 640}, {0x800, 320}, {0xA80, 220}, {0xF00, 40},
    };
    for (const PitchPoint point : pitch_points) {
        SSI263 pitch_probe;
        pitch_probe.write(1, static_cast<uint8_t>(point.inflection >> 3));
        pitch_probe.write(2, static_cast<uint8_t>(
            (point.inflection & 0x07) |
            ((point.inflection & 0x0800) != 0 ? 0x08 : 0x00)));
        const float expected = 20000.0f / point.period;
        if (std::fabs(pitch_probe.pitchHz() - expected) > 0.001f) {
            std::fprintf(stderr,
                "SSI-263 pitch law mismatch at I=$%03X "
                "(actual=%g expected=%g)\n",
                point.inflection, pitch_probe.pitchHz(), expected);
            return 1;
        }
    }

    // The first transitioned start seeds the otherwise uninitialized counter.
    // Later starts retain its transitioned field across both phones and a warm
    // reset; only a cold reset permits another first-start seed.
    SSI263 transition;
    configureSpeech(transition, 0xC1, 0x50, 0x08, 0x5F);
    if (std::fabs(transition.activePitchHz() - transition.pitchHz()) > 0.001f) {
        std::fprintf(stderr, "SSI-263 first transitioned pitch was not seeded\n");
        return 1;
    }
    transition.write(1, 0xE0);
    transition.write(2, 0x07);
    transition.write(0, 0xC1);
    if (std::fabs(transition.activePitchHz() - transition.pitchHz()) < 0.001f) {
        std::fprintf(stderr, "SSI-263 transitioned pitch reseeded per phone\n");
        return 1;
    }
    transition.reset(false);
    transition.write(0, 0xC1);
    transition.write(1, 0xFF);
    transition.write(2, 0x0F);
    transition.write(3, 0x5F);
    if (std::fabs(transition.activePitchHz() - transition.pitchHz()) < 0.001f) {
        std::fprintf(stderr, "SSI-263 warm reset lost transitioned history\n");
        return 1;
    }
    transition.reset(true);
    configureSpeech(transition, 0xC1, 0xFF, 0x0F, 0x5F);
    if (std::fabs(transition.activePitchHz() - transition.pitchHz()) > 0.001f) {
        std::fprintf(stderr, "SSI-263 cold reset did not clear pitch history\n");
        return 1;
    }

    // Match the verified Appletini RTL sample-for-sample. FILFREQ is retained
    // as a register/alias on SSI-263, but Appletini's formant coefficients use
    // the fixed 20 kHz capacitor clock; fresh cold streams must therefore be
    // identical for every FILFREQ value rather than ear-tuned per value.
    const std::vector<int16_t> p08_ff00 =
        renderAppletiniReference(0x08, 0x00);
    const std::vector<int16_t> p08_ffe7 =
        renderAppletiniReference(0x08, 0xE7);
    const std::vector<int16_t> p08_ffff =
        renderAppletiniReference(0x08, 0xFF);
    const std::vector<int16_t> p30_ff00 =
        renderAppletiniReference(0x30, 0x00);
    if (p08_ff00 != p08_ffe7 || p08_ff00 != p08_ffff) {
        std::fprintf(stderr,
            "SSI-263 FILFREQ changed Appletini's fixed formant path\n");
        return 1;
    }
    if (!verifyAppletiniReference("P08", p08_ff00, kP08WindowOffset,
                                  kP08Window, kP08Metrics) ||
        !verifyAppletiniReference("P30", p30_ff00, kP30WindowOffset,
                                  kP30Window, kP30Metrics)) {
        return 1;
    }

    // A phone-start pulse aborts Appletini's in-flight multi-cycle sample but
    // leaves audio_q at the last sample already visible to the mixer. Exercise
    // every boundary in the upstream six-phone anti-crackle chain and hash the
    // complete PCM stream; isolated cold-start vectors cannot detect a hidden
    // one-sample look-ahead leaking across DURPHON writes.
    const std::vector<int16_t> chained = renderAppletiniChainReference();
    const std::array<uint8_t, 32> chained_digest = pcmSha256(chained);
    if (chained.size() != kAppletiniChainSamples ||
        chained_digest != kAppletiniChainPcmSha256) {
        std::fprintf(stderr,
            "SSI-263 chained Appletini PCM mismatch (samples=%zu sha256=",
            chained.size());
        for (uint8_t byte : chained_digest) {
            std::fprintf(stderr, "%02X", byte);
        }
        std::fprintf(stderr, ")\n");
        return 1;
    }
    for (size_t boundary = kAppletiniChainSegmentSamples;
         boundary < chained.size();
         boundary += kAppletiniChainSegmentSamples) {
        if (chained[boundary] != chained[boundary - 1]) {
            std::fprintf(stderr,
                "SSI-263 audio_q discontinuity at chained sample %zu "
                "(previous=%d started=%d)\n",
                boundary, chained[boundary - 1], chained[boundary]);
            return 1;
        }
    }

    // Registers 4..7 alias the same FILFREQ latch even though that latch does
    // not retune Appletini's fixed-coefficient audio backend.
    SSI263 filter_alias;
    filter_alias.write(7, 0xE7);
    filter_alias.write(2, 0xA8);
    filter_alias.write(1, 0x40);
    filter_alias.write(0, 0x08);
    filter_alias.write(3, 0x5A);
    std::vector<float> alias_audio(4096 * 2, 0.0f);
    mixClocked(filter_alias, alias_audio, 4096);
    if (!filter_alias.active() || !hasAudibleSamples(alias_audio) ||
        !allSamplesInRange(alias_audio)) {
        std::fprintf(stderr, "SSI-263 filter-register alias failed\n");
        return 1;
    }

    // The synthesizer must retain exact state across host audio chunk
    // boundaries. Render the same phone as one buffer and as irregular chunks.
    SSI263 contiguous_speech;
    SSI263 chunked_speech;
    configureSpeech(contiguous_speech, 0x08, 0xC0, 0xA8, 0x7F);
    configureSpeech(chunked_speech, 0x08, 0xC0, 0xA8, 0x7F);
    std::vector<float> contiguous(8192 * 2, 0.0f);
    std::vector<float> chunked;
    uint64_t contiguous_phase = 0;
    uint64_t chunked_phase = 0;
    mixClocked(contiguous_speech, contiguous, 8192, contiguous_phase);
    uint32_t rendered = 0;
    while (rendered < 8192) {
        const uint32_t count = std::min<uint32_t>(257, 8192 - rendered);
        std::vector<float> part(static_cast<size_t>(count) * 2, 0.0f);
        mixClocked(chunked_speech, part, count, chunked_phase);
        chunked.insert(chunked.end(), part.begin(), part.end());
        rendered += count;
    }
    if (waveformDifference(contiguous, chunked) != 0.0f) {
        std::fprintf(stderr, "SSI-263 synthesis changes at host chunk boundaries\n");
        return 1;
    }

    // The public mono primitive is exactly the sample source used by the
    // stereo mixer; it must not advance the independent XCK response clock.
    SSI263 single_sample_speech;
    SSI263 mixed_sample_speech;
    configureSpeech(single_sample_speech, 0x08, 0xC0, 0xA8, 0x7F);
    configureSpeech(mixed_sample_speech, 0x08, 0xC0, 0xA8, 0x7F);
    std::vector<float> single_sample_audio(4096 * 2, 0.0f);
    std::vector<float> mixed_sample_audio(4096 * 2, 0.0f);
    uint64_t single_sample_phase = 0;
    for (uint32_t i = 0; i < 4096; ++i) {
        const float sample = single_sample_speech.renderSample();
        single_sample_audio[static_cast<size_t>(i) * 2] = sample;
        single_sample_audio[static_cast<size_t>(i) * 2 + 1] = sample;
        advanceXckForSamples(single_sample_speech, 1, single_sample_phase);
    }
    uint64_t mixed_sample_phase = 0;
    mixClocked(mixed_sample_speech, mixed_sample_audio, 4096,
               mixed_sample_phase);
    if (waveformDifference(single_sample_audio, mixed_sample_audio) != 0.0f) {
        std::fprintf(stderr,
            "SSI-263 renderSample and mixSamples outputs differ\n");
        return 1;
    }

    // Completion from the primary SSI socket feeds CA1 of the $Cx80 VIA.
    // IFR must latch without IER, IER then gates IRQ, and PCR rising-edge mode
    // must suppress a falling completion edge.
    NClock clock;
    InterruptController irq_controller;
    N6522 via("SSI completion VIA", &clock, &irq_controller, 4, 0);
    via.write(MB_6522_PCR, 0x00);
    via.signal_ca1_falling_edge();
    if ((via.read(MB_6522_IFR) & 0x02) == 0 ||
        irq_controller.get_irq(IRQ_SLOT_0)) {
        std::fprintf(stderr, "6522 CA1 did not latch independently of IER\n");
        return 1;
    }
    via.write(MB_6522_IFR, 0x02);
    via.write(MB_6522_IER, 0x82);

    SSI263 irq_speech;
    configureSpeech(irq_speech, 0xC1, 0x40, 0xA8, 0x5A);
    if (!finishPhoneme(irq_speech, &via) ||
        (via.read(MB_6522_IFR) & 0x82) != 0x82 ||
        !irq_controller.get_irq(IRQ_SLOT_0)) {
        std::fprintf(stderr, "SSI-263 completion did not assert VIA CA1 IRQ\n");
        return 1;
    }

    (void)via.read(MB_6522_ORA_NH);
    via.write(MB_6522_ORA_NH, 0x5A);
    if ((via.read(MB_6522_IFR) & 0x82) != 0x82 ||
        !irq_controller.get_irq(IRQ_SLOT_0)) {
        std::fprintf(stderr, "6522 no-handshake Port A access cleared CA1 IRQ\n");
        return 1;
    }

    (void)via.read(MB_6522_ORA);
    if ((via.read(MB_6522_IFR) & 0x02) != 0 ||
        irq_controller.get_irq(IRQ_SLOT_0)) {
        std::fprintf(stderr, "6522 Port A access did not clear CA1 IRQ\n");
        return 1;
    }
    irq_speech.write(1, 0x41); // ready clear repeats the completed phoneme
    if (!finishPhoneme(irq_speech, &via) ||
        (via.read(MB_6522_IFR) & 0x82) != 0x82) {
        std::fprintf(stderr, "SSI-263 repeated completion did not reassert CA1\n");
        return 1;
    }

    via.write(MB_6522_IFR, 0x02);
    via.write(MB_6522_PCR, 0x01);
    irq_speech.write(2, 0xA8);
    if (!finishPhoneme(irq_speech, &via) ||
        (via.read(MB_6522_IFR) & 0x02) != 0 ||
        irq_controller.get_irq(IRQ_SLOT_0)) {
        std::fprintf(stderr, "6522 PCR did not reject SSI falling-edge IRQ\n");
        return 1;
    }

    std::puts("PASS: SSI-263 decode, controls, audio, D7, and CA1 IRQ checks passed");
    return 0;
}

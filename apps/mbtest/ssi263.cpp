#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "devices/mockingboard/SSI263.hpp"
#include "devices/mockingboard/W6522.hpp"

uint64_t debug_level = 0;

namespace {

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

bool allSamplesSilent(const std::vector<float> &samples) {
    return std::all_of(samples.begin(), samples.end(), [](float sample) {
        return sample == 0.0f;
    });
}

float sampleEnergy(const std::vector<float> &samples) {
    float energy = 0.0f;
    for (float sample : samples) {
        energy += std::fabs(sample);
    }
    return energy;
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

bool finishPhoneme(SSI263 &speech, N6522 *completion_via = nullptr) {
    std::vector<float> audio(256 * 2, 0.0f);
    uint32_t generated = 0;
    while (!speech.ready() && generated < SSI263::kSampleRate * 2) {
        std::fill(audio.begin(), audio.end(), 0.0f);
        speech.mixSamples(audio, 256);
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
    speech.mixSamples(audio, 2048);
    if (!hasAudibleSamples(audio) || !allSamplesInRange(audio)) {
        std::fprintf(stderr, "SSI-263 did not produce valid audible samples\n");
        return 1;
    }

    unsigned generated = 2048;
    while (!speech.ready() && generated < SSI263::kSampleRate) {
        std::fill(audio.begin(), audio.end(), 0.0f);
        speech.mixSamples(audio, 2048);
        generated += 2048;
    }
    if (!speech.ready() || speech.active() || speech.read(0) != 0x80) {
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
    speech.mixSamples(audio, 2048);
    if (!hasAudibleSamples(audio) || !allSamplesInRange(audio)) {
        std::fprintf(stderr, "SSI-263 fricative synthesis failed\n");
        return 1;
    }

    speech.write(3, 0xDA);
    if (speech.active() || speech.ready()) {
        std::fprintf(stderr, "SSI-263 control reset failed\n");
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

    // DURPHON[7:6] changes duration and also controls whether the completion
    // IRQ function is latched on the control falling edge.
    SSI263 duration_normal;
    SSI263 duration_fast;
    configureSpeech(duration_normal, 0x01, 0x40, 0xA8, 0x5A);
    configureSpeech(duration_fast, 0x81, 0x40, 0xA8, 0x5A);
    if (duration_fast.samplesTotal() >= duration_normal.samplesTotal() ||
        duration_normal.interruptsEnabled() || !duration_fast.interruptsEnabled()) {
        std::fprintf(stderr, "SSI-263 duration/function latch is incorrect\n");
        return 1;
    }

    // FILTER=$FF mutes the output without stopping phoneme timing. Registers
    // 4..7 alias the same latch, so writing register 7 must unmute it.
    SSI263 filtered;
    configureSpeech(filtered, 0x41, 0x40, 0xA8, 0x5A, 0xFF);
    std::vector<float> muted(256 * 2, 0.0f);
    filtered.mixSamples(muted, 256);
    if (!filtered.active() || !allSamplesSilent(muted)) {
        std::fprintf(stderr, "SSI-263 filter silence failed\n");
        return 1;
    }
    filtered.write(7, 0xE8);
    std::vector<float> unmuted(256 * 2, 0.0f);
    filtered.mixSamples(unmuted, 256);
    if (!hasAudibleSamples(unmuted)) {
        std::fprintf(stderr, "SSI-263 filter-register alias failed\n");
        return 1;
    }

    // CTL[6:4] controls formant articulation. Slow and fast transitions from
    // reset must produce measurably different early-phone envelopes.
    SSI263 articulation_slow;
    SSI263 articulation_fast;
    configureSpeech(articulation_slow, 0x41, 0x40, 0xA8, 0x0A);
    configureSpeech(articulation_fast, 0x41, 0x40, 0xA8, 0x7A);
    std::vector<float> slow_audio(512 * 2, 0.0f);
    std::vector<float> fast_audio(512 * 2, 0.0f);
    articulation_slow.mixSamples(slow_audio, 512);
    articulation_fast.mixSamples(fast_audio, 512);
    if (!hasAudibleSamples(slow_audio) || !hasAudibleSamples(fast_audio) ||
        std::fabs(sampleEnergy(slow_audio) - sampleEnergy(fast_audio)) < 0.001f) {
        std::fprintf(stderr, "SSI-263 articulation control has no effect\n");
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

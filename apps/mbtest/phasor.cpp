#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>

#include "devices/mockingboard/PhasorAudio.hpp"
#include "devices/mockingboard/PhasorLogic.hpp"

namespace PL = PhasorLogic;

static_assert(PL::modeSwitchBase(4) == 0xC0C0,
              "slot 4 mode switch base must be $C0C0");
static_assert(PL::updateModeLatch(0, 0xC0C5) == PL::kModePhasor,
              "$C0C5 selects native Phasor mode");
static_assert(PL::updateModeLatch(PL::kModePhasor, 0xC0C2) ==
                  PL::kModeEchoPlus,
              "mode bits accumulate when A3 is clear");
static_assert(PL::updateModeLatch(PL::kModeEchoPlus, 0xC0C8) ==
                  PL::kModeMockingboard,
              "A3 resets the mode latch");
static_assert(PL::updateModeLatch(PL::kModeEchoPlus, 0xC0CD) ==
                  PL::kModePhasor,
              "A3 resets before A2..A0 are applied");

namespace {

int failures = 0;

void expect(bool condition, const char *description) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", description);
    ++failures;
}

void expectVia(uint8_t mode, uint8_t offset, bool high, bool low,
               const char *description) {
    const PL::ViaHits hits = PL::decodeViaHits(mode, offset);
    expect(hits.high == high && hits.low == low, description);
    expect(PL::viaHit(hits, PL::kViaHigh) == high,
           "VIA-high indexed lookup agrees with decode");
    expect(PL::viaHit(hits, PL::kViaLow) == low,
           "VIA-low indexed lookup agrees with decode");
}

void expectSsiWrites(uint8_t mode, uint8_t offset, bool primary,
                     bool secondary, const char *description) {
    const PL::SsiSelects selects = PL::decodeSsiWrites(mode, offset);
    expect(selects.primary == primary && selects.secondary == secondary,
           description);
}

void expectSelection(PL::AySelection actual, bool primary, bool secondary,
                     const char *description) {
    expect(actual.primary == primary && actual.secondary == secondary,
           description);
}

void expectRoute(PL::AyRoute route, bool reset, bool primary,
                 bool secondary, bool selected_primary,
                 bool selected_secondary, const char *description) {
    expect(route.reset == reset &&
               route.drive_primary == primary &&
               route.drive_secondary == secondary &&
               route.next_selection.primary == selected_primary &&
               route.next_selection.secondary == selected_secondary,
           description);
}

void testModeLatch() {
    uint8_t mode = PL::kModeMockingboard;
    mode = PL::updateModeLatch(mode, 0xC0C0);
    expect(mode == PL::kModeMockingboard,
           "slot-4 $C0C0 leaves Mockingboard mode selected");
    mode = PL::updateModeLatch(mode, 0xC0C5);
    expect(mode == PL::kModePhasor,
           "slot-4 $C0C5 enters native Phasor mode");
    mode = PL::updateModeLatch(mode, 0xC0C2);
    expect(mode == PL::kModeEchoPlus,
           "slot-4 $C0C2 ORs mode 5 into Echo+ mode 7");
    mode = PL::updateModeLatch(mode, 0xC0CD);
    expect(mode == PL::kModePhasor,
           "slot-4 $C0CD resets and selects native mode 5");
    mode = PL::updateModeLatch(mode, 0xC0CF);
    expect(mode == PL::kModeEchoPlus,
           "slot-4 $C0CF resets and selects Echo+ mode 7");
    mode = PL::updateModeLatch(mode, 0xC0C8);
    expect(mode == PL::kModeMockingboard,
           "slot-4 $C0C8 returns to Mockingboard mode 0");
}

void testViaDecode() {
    expectVia(PL::kModeMockingboard, 0x00, false, true,
              "Mockingboard $Cn00 selects low VIA");
    expectVia(PL::kModeMockingboard, 0x7F, false, true,
              "Mockingboard $Cn7F selects low VIA");
    expectVia(PL::kModeMockingboard, 0x80, true, false,
              "Mockingboard $Cn80 selects high VIA");
    expectVia(PL::kModeMockingboard, 0xFF, true, false,
              "Mockingboard $CnFF selects high VIA");

    expectVia(PL::kModePhasor, 0x00, false, false,
              "native $Cn00 selects neither interleaved VIA");
    expectVia(PL::kModePhasor, 0x10, false, true,
              "native A4 selects low VIA");
    expectVia(PL::kModePhasor, 0x80, true, false,
              "native A7 selects high VIA");
    expectVia(PL::kModePhasor, 0x90, true, true,
              "native A7+A4 selects both VIAs");

    expectVia(PL::kModeEchoPlus, 0x00, true, false,
              "Echo+ aliases low addresses to high VIA");
    expectVia(PL::kModeEchoPlus, 0xFF, true, false,
              "Echo+ aliases high addresses to high VIA");
    expectVia(1, 0x90, false, false,
              "unsupported intermediate mode selects no VIA");
}

struct TimerProbeVia {
    uint8_t ticks = 0;
    uint8_t read_reg = 0xFF;

    void incr_cycle() { ++ticks; }
    uint8_t read(uint8_t reg) {
        read_reg = reg;
        return ticks;
    }
};

void testNativeTimerReadTiming() {
    TimerProbeVia t1;
    expect(PL::readVia(PL::kModePhasor, 0x94, t1) == 1 &&
               t1.ticks == 1 && t1.read_reg == 0x04,
           "native T1C-L read advances the selected VIA before sampling");

    TimerProbeVia t2;
    expect(PL::readVia(PL::kModePhasor, 0x98, t2) == 1 &&
               t2.ticks == 1 && t2.read_reg == 0x08,
           "native T2C-L read advances the selected VIA before sampling");

    TimerProbeVia high;
    expect(PL::readVia(PL::kModePhasor, 0x95, high) == 0 &&
               high.ticks == 0 && high.read_reg == 0x05,
           "native timer-high read has no extra tick");

    TimerProbeVia compatible;
    expect(PL::readVia(PL::kModeMockingboard, 0x84, compatible) == 0 &&
               compatible.ticks == 0 && compatible.read_reg == 0x04,
           "Mockingboard-mode T1C-L timing remains unchanged");

    TimerProbeVia echo;
    expect(PL::readVia(PL::kModeEchoPlus, 0x08, echo) == 0 &&
               echo.ticks == 0 && echo.read_reg == 0x08,
           "Echo+ T2C-L timing remains unchanged");
}

void testSpeechSampleTimeline() {
    constexpr uint64_t cycle_rate = 1020484;
    constexpr uint64_t sample_rate = PhasorAudio::kSampleRate;
    uint64_t phase = 0;
    uint64_t samples = 0;
    for (uint64_t cycle = 0; cycle < cycle_rate; ++cycle) {
        if (PL::advanceAudioSamplePhase(phase, sample_rate, cycle_rate)) {
            ++samples;
        }
    }
    expect(samples == sample_rate && phase == 0,
           "cycle-timeline speech renderer produces exactly 48k samples per second");
}

void testSpeechFrameContinuity() {
    constexpr uint64_t cycle_rate = 1020484;
    constexpr uint64_t cycles_per_frame = 17030;
    constexpr uint64_t sample_rate = PhasorAudio::kSampleRate;
    constexpr uint64_t frame_count = 600;
    constexpr uint64_t max_frame_samples =
        (cycles_per_frame * sample_rate + cycle_rate - 1) / cycle_rate + 1;

    uint64_t phase = cycle_rate - sample_rate;
    uint64_t generated_id = 0;
    uint64_t submitted_id = 0;
    bool bounded = true;
    bool contiguous = true;
    std::vector<uint64_t> frame;
    frame.reserve(static_cast<size_t>(max_frame_samples));

    for (uint64_t frame_index = 0; frame_index < frame_count;
         ++frame_index) {
        frame.clear();
        for (uint64_t cycle = 0; cycle < cycles_per_frame; ++cycle) {
            if (PL::advanceAudioSamplePhase(phase, sample_rate, cycle_rate)) {
                frame.push_back(generated_id++);
            }
        }
        if (frame.size() > max_frame_samples) bounded = false;
        for (uint64_t id : frame) {
            if (id != submitted_id++) contiguous = false;
        }
    }

    const uint64_t expected =
        ((cycle_rate - sample_rate) +
         frame_count * cycles_per_frame * sample_rate) / cycle_rate;
    expect(bounded,
           "normal video frames never trigger the speech backlog truncation guard");
    expect(contiguous && generated_id == submitted_id &&
               submitted_id == expected,
           "frame chunking preserves every synthesized sample exactly once");
}

struct QueueSimulationResult {
    uint32_t underruns = 0;
    double minimum_depth = 0.0;
    double maximum_depth = 0.0;
    bool producer_stall_applied = false;
    bool reset_reprimed = false;
};

QueueSimulationResult simulateOutputQueue(double device_rate_error,
                                          bool recovery_enabled,
                                          bool reset_halfway) {
    constexpr uint64_t cycle_rate = 1020484;
    constexpr uint64_t cycles_per_frame = 17030;
    constexpr uint64_t sample_rate = PhasorAudio::kSampleRate;
    constexpr double duration_seconds = 180.0;
    constexpr double callback_frames = 1024.0;
    constexpr std::array<double, 5> callback_jitter = {
        0.08, -0.08, 0.04, -0.04, 0.0,
    };

    PhasorAudio::OutputClockRecovery recovery;
    double queued = recovery_enabled
        ? PhasorAudio::OutputClockRecovery::kPrefillFrames
        : 802.0;
    if (recovery_enabled) recovery.markPrefilled();

    const double frame_period =
        static_cast<double>(cycles_per_frame) /
        static_cast<double>(cycle_rate);
    const double callback_period = callback_frames /
        (static_cast<double>(sample_rate) * (1.0 + device_rate_error));
    size_t jitter_index = 0;
    double next_frame = frame_period;
    double next_callback = callback_period *
        (1.0 + callback_jitter[jitter_index++ % callback_jitter.size()]);
    double ratio = 1.0;
    uint64_t phase = cycle_rate - sample_rate;

    QueueSimulationResult result;
    result.minimum_depth = queued;
    result.maximum_depth = queued;

    while ((next_frame < next_callback ? next_frame : next_callback) <
           duration_seconds) {
        if (recovery_enabled && !result.producer_stall_applied &&
            next_frame >= 30.0) {
            // Model one missed scheduling deadline. Synthesis still emits
            // every sample in order when the emulator resumes; only delivery
            // to the asynchronous host device is 25 ms late.
            next_frame += 0.025;
            result.producer_stall_applied = true;
        }

        const double next_event =
            next_frame < next_callback ? next_frame : next_callback;
        if (reset_halfway && !result.reset_reprimed &&
            next_event >= duration_seconds / 2.0) {
            recovery.reset();
            queued = 0.0;
            if (recovery.needsPrefill(0)) {
                queued =
                    PhasorAudio::OutputClockRecovery::kPrefillFrames;
                recovery.markPrefilled();
                result.reset_reprimed = true;
            }
        }

        if (next_frame <= next_callback) {
            if (recovery_enabled) {
                ratio = recovery.update(
                    static_cast<uint32_t>(queued < 0.0 ? 0.0 : queued));
            }

            const uint64_t accumulated =
                phase + cycles_per_frame * sample_rate;
            queued += static_cast<double>(accumulated / cycle_rate);
            phase = accumulated % cycle_rate;
            next_frame += frame_period;
        } else {
            const double consumed = callback_frames * ratio;
            if (queued < consumed) {
                ++result.underruns;
                queued = 0.0;
            } else {
                queued -= consumed;
            }
            next_callback += callback_period *
                (1.0 + callback_jitter[
                    jitter_index++ % callback_jitter.size()]);
        }

        if (queued < result.minimum_depth) result.minimum_depth = queued;
        if (queued > result.maximum_depth) result.maximum_depth = queued;
    }

    return result;
}

void testOutputClockRecovery() {
    using Recovery = PhasorAudio::OutputClockRecovery;
    constexpr float epsilon = 0.000001f;

    Recovery recovery;
    expect(Recovery::kPrefillFrames == 2400 &&
               Recovery::kTargetFrames == 2880,
           "Phasor output keeps a 50ms prefill and 60ms target at 48kHz");
    expect(recovery.needsPrefill(0) && !recovery.primed(),
           "new output-clock controller requests prefill");
    recovery.markPrefilled();
    expect(!recovery.needsPrefill(Recovery::kPrefillFrames),
           "prefilled output-clock controller is ready");
    expect(std::fabs(recovery.update(Recovery::kTargetFrames) - 1.0f) <
               epsilon,
           "target queue depth leaves playback rate exact");
    expect(std::fabs(recovery.update(0) - 0.995f) < epsilon &&
               std::fabs(recovery.update(Recovery::kTargetFrames * 2) -
                         1.005f) < epsilon,
           "queue correction is bounded to plus or minus 0.5 percent");
    recovery.reset();
    expect(recovery.needsPrefill(Recovery::kPrefillFrames) &&
               !recovery.primed() && recovery.ratio() == 1.0f,
           "device reset returns clock recovery to a clean prefill state");

    for (double drift : {-0.0005, 0.0, 0.0005}) {
        const QueueSimulationResult simulation =
            simulateOutputQueue(drift, true, true);
        expect(simulation.underruns == 0,
               "clock recovery survives 1024-frame callbacks, jitter, and 500ppm drift");
        expect(simulation.minimum_depth > 512.0 &&
                   simulation.maximum_depth <
                       Recovery::kTargetFrames * 2.0,
               "clock recovery keeps output queue inside its bounded safety window");
        expect(simulation.reset_reprimed,
               "device-format reset re-primes the output queue during simulation");
        expect(simulation.producer_stall_applied,
               "queue simulation includes a one-off 25ms producer stall");
    }

    const QueueSimulationResult legacy =
        simulateOutputQueue(0.0005, false, false);
    expect(legacy.underruns != 0,
           "one-frame prefill is insufficient for a 500ppm-fast host clock");
}

template <size_t N>
void expectWarmthVector(const std::array<int16_t, N> &input,
                        const std::array<int16_t, N> &expected,
                        const char *description) {
    PhasorAudio::WarmthChannel warmth;
    bool matches = true;
    for (size_t i = 0; i < N; ++i) {
        const int16_t actual = warmth.processPcm(input[i]);
        if (actual != expected[i]) {
            std::fprintf(stderr,
                "FAIL: %s at sample %zu (actual=%d expected=%d)\n",
                description, i, actual, expected[i]);
            matches = false;
            break;
        }
    }
    if (!matches) ++failures;
}

void testAppletiniWarmth() {
    // Exact Q1.31 golden vectors for the 48 kHz collapse of Appletini's
    // forced +8 card-output network. These lock the source-derived poles and
    // arithmetic-shift behavior so they cannot become subjective EQ knobs.
    constexpr std::array<int16_t, 16> impulse = {
        8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    constexpr std::array<int16_t, 16> impulse_expected = {
        7671, 1173, 897, 682, 514, 380, 275, 192,
        126, 73, 32, -2, -27, -48, -64, -76,
    };
    constexpr std::array<int16_t, 16> step = {
        8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192,
        8192, 8192, 8192, 8192, 8192, 8192, 8192, 8192,
    };
    constexpr std::array<int16_t, 16> step_expected = {
        7671, 8843, 9740, 10422, 10934, 11315, 11590, 11782,
        11907, 11981, 12014, 12013, 11985, 11937, 11873, 11797,
    };
    constexpr std::array<int16_t, 16> alternating = {
        12000, -12000, 12000, -12000, 12000, -12000, 12000, -12000,
        12000, -12000, 12000, -12000, 12000, -12000, 12000, -12000,
    };
    constexpr std::array<int16_t, 16> alternating_expected = {
        11236, -9519, 10833, -9834, 10587, -10028, 10432, -10150,
        10336, -10228, 10275, -10276, 10236, -10306, 10213, -10323,
    };
    expectWarmthVector(impulse, impulse_expected,
                       "Appletini warmth impulse vector");
    expectWarmthVector(step, step_expected,
                       "Appletini warmth step vector");
    expectWarmthVector(alternating, alternating_expected,
                       "Appletini warmth alternating vector");

    constexpr std::array<int32_t, 10> knee_input = {
        -32768, -30000, -20488, -20481, -20480,
         20480,  20481,  20488,  30000,  32767,
    };
    constexpr std::array<int32_t, 10> knee_expected = {
        -28160, -26430, -20485, -20480, -20480,
         20480,  20480,  20485,  26430,  28158,
    };
    bool knee_matches = true;
    for (size_t i = 0; i < knee_input.size(); ++i) {
        if (PhasorAudio::WarmthChannel::applyWarmthKnee(knee_input[i]) !=
            knee_expected[i]) {
            knee_matches = false;
            break;
        }
    }
    expect(knee_matches, "Appletini warmth soft-knee vector");

    PhasorAudio::WarmthFilter stereo;
    const PhasorAudio::StereoSample centered = stereo.process(0.25f, 0.25f);
    expect(centered.left == centered.right &&
               PhasorAudio::WarmthChannel::quantizePcm(centered.left) ==
                   impulse_expected[0],
           "centered speech remains bit-identical in both channels after warmth");

    PhasorAudio::WarmthFilter independent_stereo;
    const PhasorAudio::StereoSample split =
        independent_stereo.process(0.25f, 0.0f);
    const PhasorAudio::StereoSample split_tail =
        independent_stereo.process(0.0f, 0.0f);
    expect(PhasorAudio::WarmthChannel::quantizePcm(split.left) ==
               impulse_expected[0] && split.right == 0.0f &&
               PhasorAudio::WarmthChannel::quantizePcm(split_tail.left) ==
                   impulse_expected[1] && split_tail.right == 0.0f,
           "warmth keeps independent left and right filter histories");

    PhasorAudio::WarmthChannel reset_probe;
    const int16_t first = reset_probe.processPcm(8192);
    const int16_t continued = reset_probe.processPcm(8192);
    reset_probe.reset();
    const int16_t restarted = reset_probe.processPcm(8192);
    expect(first == impulse_expected[0] && continued == step_expected[1] &&
               restarted == first,
           "cold reset clears warmth state while uninterrupted audio preserves it");
    expect(PhasorAudio::WarmthChannel::quantizePcm(1.0f) == 32767 &&
               PhasorAudio::WarmthChannel::quantizePcm(-1.0f) == -32768,
           "warmth input quantization saturates both PCM endpoints");
}

void testSsiDecode() {
    expectSsiWrites(PL::kModeMockingboard, 0x40, true, false,
                    "Mockingboard A6 selects primary SSI");
    expectSsiWrites(PL::kModeMockingboard, 0x20, false, true,
                    "Mockingboard A5 selects secondary SSI");
    expectSsiWrites(PL::kModeMockingboard, 0x60, true, true,
                    "Mockingboard A6+A5 broadcasts to both SSIs");
    expectSsiWrites(PL::kModePhasor, 0x60, true, true,
                    "native A6+A5 broadcasts to both SSIs");
    expectSsiWrites(PL::kModeMockingboard, 0x00, false, false,
                    "SSI sockets are idle without A6 or A5");
    expectSsiWrites(PL::kModeEchoPlus, 0x60, false, false,
                    "Echo+ hides both SSI sockets");

    expect(PL::nativeStatusSocket(PL::kModePhasor, 0x40) ==
               PL::SsiSocket::Primary,
           "native A6 reads primary SSI D7");
    expect(PL::nativeStatusSocket(PL::kModePhasor, 0x20) ==
               PL::SsiSocket::Secondary,
           "native A5 reads secondary SSI D7");
    expect(PL::nativeStatusSocket(PL::kModePhasor, 0x60) ==
               PL::SsiSocket::Secondary,
           "secondary SSI has native D7 priority on A6+A5");
    expect(PL::nativeStatusSocket(PL::kModePhasor, 0x50) ==
               PL::SsiSocket::None,
           "native A4 VIA select suppresses SSI status reads");
    expect(PL::nativeStatusSocket(PL::kModePhasor, 0xC0) ==
               PL::SsiSocket::None,
           "native A7 VIA select suppresses SSI status reads");
    expect(PL::nativeStatusSocket(PL::kModeMockingboard, 0x40) ==
               PL::SsiSocket::None,
           "Mockingboard mode has no native D7 readback");
    expect(PL::nativeStatusSocket(PL::kModeEchoPlus, 0x40) ==
               PL::SsiSocket::None,
           "Echo+ mode has no SSI D7 readback");

    expect(PL::nativeStatusValue(0xD5, false) == 0x55,
           "not-ready status clears D7 and preserves floating low bits");
    expect(PL::nativeStatusValue(0x55, true) == 0xD5,
           "ready status sets D7 and preserves floating low bits");
}

void testAyDecode() {
    constexpr PL::AySelection none{false, false};
    constexpr PL::AySelection primary_only{true, false};
    constexpr PL::AySelection both{true, true};

    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x03, both),
                true, true, true, false, false,
                "AY reset drives both banks and clears selection");
    expectRoute(PL::decodeAyRoute(PL::kModeMockingboard, PL::kViaLow,
                                  0x1F, none),
                false, true, false, false, false,
                "Mockingboard ignores Phasor chip-select pins");

    const PL::AyRoute latch_primary =
        PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow, 0x0F, none);
    expectRoute(latch_primary, false, true, false, true, false,
                "native primary latch drives and selects primary AY");
    const PL::AyRoute latch_secondary =
        PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow, 0x17, none);
    expectRoute(latch_secondary, false, true, true, true, true,
                "native secondary latch is observed by both AYs");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x07, none),
                false, true, true, true, true,
                "native latch with both selects drives both AYs");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x1F, both),
                false, false, false, true, true,
                "native latch with neither select preserves selection");

    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x0D, primary_only),
                false, true, false, true, false,
                "native primary transfer reaches selected primary AY");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x15, both),
                false, false, true, true, true,
                "native secondary transfer reaches selected secondary AY");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x05, both),
                false, true, true, true, true,
                "native transfer with both selects broadcasts to both AYs");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x0D, both),
                false, true, true, true, true,
                "selected secondary AY follows a primary transfer");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x1D, both),
                false, false, false, true, true,
                "native transfer with neither chip select drives no AY");
    expectRoute(PL::decodeAyRoute(PL::kModePhasor, PL::kViaLow,
                                  0x0C, both),
                false, false, false, true, true,
                "native inactive bus command drives no AY");

    expectRoute(PL::decodeAyRoute(PL::kModeEchoPlus, PL::kViaLow,
                                  0x17, none),
                false, false, false, true, true,
                "Echo+ low VIA has no AY route");
    expectRoute(PL::decodeAyRoute(PL::kModeEchoPlus, PL::kViaHigh,
                                  0x17, none),
                false, true, true, true, true,
                "Echo+ high VIA secondary latch reaches both AYs");
    expectRoute(PL::decodeAyRoute(PL::kModeEchoPlus, PL::kViaHigh,
                                  0x0D, both),
                false, true, true, true, true,
                "Echo+ primary transfer can broadcast to selected secondary");

    expect(PL::combineAyRead(false, 0x12, false, 0x34) == 0xFF,
           "undriven AY read returns pull-ups");
    expect(PL::combineAyRead(true, 0x12, false, 0x34) == 0x12,
           "primary-only AY read returns primary data");
    expect(PL::combineAyRead(false, 0x12, true, 0x34) == 0x34,
           "secondary-only AY read returns secondary data");
    expect(PL::combineAyRead(true, 0x12, true, 0x81) == 0x93,
           "dual AY read wire-ORs both data sources");

    expectSelection(latch_primary.next_selection, true, false,
                    "primary latch selection is retained for transfers");
    expectSelection(latch_secondary.next_selection, true, true,
                    "secondary latch selection is retained for transfers");
}

void testStereoMapping() {
    expect(PL::ayChipForVia(PL::kViaLow) == PL::kStereoLeft,
           "$Cn00 low VIA AY bank renders left");
    expect(PL::ayChipForVia(PL::kViaHigh) == PL::kStereoRight,
           "$Cn80 high VIA AY bank renders right");
}

void testAudioMixer() {
    constexpr float epsilon = 0.000001f;

    const PL::StereoSample primary =
        PL::mixAudioSample(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    expect(std::fabs(primary.left - PL::kCenterPanGain) < epsilon &&
               std::fabs(primary.right - PL::kCenterPanGain) < epsilon,
           "primary SSI is constant-power centered");
    expect(std::fabs(primary.left * primary.left +
                     primary.right * primary.right - 1.0f) < epsilon,
           "center pan preserves single-SSI power");

    const PL::StereoSample secondary =
        PL::mixAudioSample(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    expect(std::fabs(secondary.left - primary.left) < epsilon &&
               std::fabs(secondary.right - primary.right) < epsilon,
           "secondary SSI uses the same centered route");

    // 0.6 + 0.6 - 0.6/sqrt(2) = ~0.7757. An intermediate clamp after the
    // two AY banks would instead produce ~0.5757, exposing source-order bias.
    const PL::StereoSample cancellation =
        PL::mixAudioSample(0.6f, 0.6f, 0.6f, 0.6f, 0.0f, -0.6f);
    constexpr float expected =
        1.2f - 0.6f * PL::kCenterPanGain;
    expect(std::fabs(cancellation.left - expected) < epsilon &&
               std::fabs(cancellation.right - expected) < epsilon,
           "AY and SSI sources sum before the single final limiter");

    const PL::StereoSample limited =
        PL::mixAudioSample(0.8f, -0.8f, 0.8f, -0.8f, 1.0f, 1.0f);
    expect(limited.left == 1.0f && limited.right > -0.2f,
           "completed card mix is limited once at the output");
}

} // namespace

int main() {
    testModeLatch();
    testViaDecode();
    testNativeTimerReadTiming();
    testSpeechSampleTimeline();
    testSpeechFrameContinuity();
    testOutputClockRecovery();
    testAppletiniWarmth();
    testSsiDecode();
    testAyDecode();
    testStereoMapping();
    testAudioMixer();

    if (failures != 0) {
        std::fprintf(stderr, "%d Phasor decoder test(s) failed\n", failures);
        return 1;
    }
    std::puts("Phasor decoder tests passed");
    return 0;
}

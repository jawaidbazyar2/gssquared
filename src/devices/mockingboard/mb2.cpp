/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_audio.h>

#include "AY8910-2.hpp"
#include "PhasorAudio.hpp"
#include "PhasorLogic.hpp"
#include "SSI263.hpp"
#include "W6522.hpp"

#include "gs2.hpp"
#include "mb2.hpp"
#include "debug.hpp"
#include "util/EventTimer.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"

const char *register_names[] = {
    "A_Tone_Low",
    "A_Tone_High",
    "B_Tone_Low",
    "B_Tone_High",
    "C_Tone_Low",
    "C_Tone_High",
    "Noise_Period",
    "Mixer_Control",
    "Ampl_A",
    "Ampl_B",
    "Ampl_C",
    "Envelope_Period_Low",
    "Envelope_Period_High",
    "Envelope_Shape",
    "Unknown 14",
    "Unknown 15",
};

void debug_register_change(double current_time, uint8_t chip_index, uint8_t reg, uint8_t value) {
    std::cout << "[" << current_time << "] Register " << register_names[reg] << " set to: " << static_cast<int>(value) << std::endl;
}

#if 0
DebugFormatter *debug_registers_6522(mb_cpu_data *mb_d) {
    DebugFormatter *df = new DebugFormatter();
    cpu_state *cpu = mb_d->computer->cpu;
    uint64_t m1_t1_diff = calc_cycle_diff_t1(&mb_d->d_6522[1], mb_d->clock->get_vid_cycles());
    uint64_t m1_t2_diff = calc_cycle_diff_t2(&mb_d->d_6522[1], mb_d->clock->get_vid_cycles());
    uint64_t m2_t1_diff = calc_cycle_diff_t1(&mb_d->d_6522[0], mb_d->clock->get_vid_cycles());
    uint64_t m2_t2_diff = calc_cycle_diff_t2(&mb_d->d_6522[0], mb_d->clock->get_vid_cycles());

    df->addLine("   6522 #2 (0x00)          |   6522 #1 (0x80)");
    df->addLine("-------------------------- | ---------------------------");
    df->addLine("DDRA: %02X    DDRB: %02X       | DDRA: %02X    DDRB: %02X", mb_d->d_6522[1].ddra, mb_d->d_6522[1].ddrb, mb_d->d_6522[0].ddra, mb_d->d_6522[0].ddrb);
    df->addLine("ORA : %02X    ORB : %02X       | ORA : %02X    ORB : %02X", mb_d->d_6522[1].ora, mb_d->d_6522[1].orb, mb_d->d_6522[0].ora, mb_d->d_6522[0].orb);
    df->addLine("IRA : %02X    IRB : %02X       | IRA : %02X    IRB : %02X", mb_d->d_6522[1].ira, mb_d->d_6522[1].irb, mb_d->d_6522[0].ira, mb_d->d_6522[0].irb);
    
    df->addLine("T1L : %04X  T1C: %04X      | T1L : %04X  T1C: %04X", mb_d->d_6522[1].t1_latch, m1_t1_diff, mb_d->d_6522[0].t1_latch, m2_t1_diff);
    df->addLine("T2L : %04X  T2C: %04X      | T2L : %04X  T2C: %04X", mb_d->d_6522[1].t2_latch, m1_t2_diff, mb_d->d_6522[0].t2_latch, m2_t2_diff);
    //df->addLine("T1C: %04X                 | T1C: %04X", m1_t1_diff, m2_t1_diff);
    //df->addLine("T2C: %04X                 | T2C: %04X", m1_t2_diff, m2_t2_diff);
    df->addLine("SR  : %02X                   | SR  : %02X", mb_d->d_6522[1].sr, mb_d->d_6522[0].sr);
    df->addLine("ACR : %02X                   | ACR : %02X", mb_d->d_6522[1].acr, mb_d->d_6522[0].acr);
    df->addLine("PCR : %02X                   | PCR : %02X", mb_d->d_6522[1].pcr, mb_d->d_6522[0].pcr);
    df->addLine("IFR : %02X    IER: %02X        | IFR : %02X    IER: %02X", mb_d->d_6522[1].ifr.value, mb_d->d_6522[1].ier.value|0x80, mb_d->d_6522[0].ifr.value, mb_d->d_6522[0].ier.value|0x80);
    //df->addLine("IER: %02X                   | IER: %02X", mb_d->d_6522[0].ier.value, mb_d->d_6522[1].ier.value);
    
    MockingboardEmulator *mb = mb_d->mockingboard;

    df->addLine("AY-8913 #0 registers:");
    df->addLine("--------------------------");
    df->addLine("Tone A: %04X  Tone B: %04X  Tone C: %04X", mb->chips[0].tone_channels[0].period,mb->chips[0].tone_channels[1].period, mb->chips[0].tone_channels[2].period);
    df->addLine("Noise: %04X  Mixer: %04X", mb->chips[0].noise_period, mb->chips[0].mixer_control);
    df->addLine("Ampl A: %02X  Ampl B: %02X  Ampl C: %02X", mb->read_register(0, Ampl_A), mb->read_register(0, Ampl_B), mb->read_register(0, Ampl_C));
    df->addLine("Env Period: %04X  Env Shape: %02X", mb->chips[0].envelope_period, mb->chips[0].envelope_shape);
    df->addLine("--------------------------------");
    // now do AY chips registers
    df->addLine("AY-8913 #1 registers:");
    df->addLine("--------------------------");
    df->addLine("Tone A: %04X  Tone B: %04X  Tone C: %04X", mb->chips[1].tone_channels[0].period, mb->chips[1].tone_channels[1].period, mb->chips[1].tone_channels[2].period);
    df->addLine("Noise: %04X  Mixer: %04X", mb->chips[1].noise_period, mb->chips[1].mixer_control);
    df->addLine("Ampl A: %02X  Ampl B: %02X  Ampl C: %02X", mb->read_register(1, Ampl_A), mb->read_register(1, Ampl_B), mb->read_register(1, Ampl_C));
    df->addLine("Env Period: %04X  Env Shape: %02X", mb->chips[1].envelope_period, mb->chips[1].envelope_shape);
    df->addLine("--------------------------------");
    df->addLine("Last event: %14.6f  Last time: %14.6f", mb->dbg_last_event, mb->dbg_last_time);
    return df;
}
#endif

class Mockingboard {
private:
    struct AudioResetCallbackState {
        Mockingboard *owner = nullptr;
    };

    N6522 *n6522[2];
    AY8910s *ay_primary;
    AY8910s *ay_secondary;
    SSI263 ssi_primary;
    SSI263 ssi_secondary;
    SDL_AudioStream *stream;
    uint64_t last_cycle;
    uint8_t slot;
    EventTimer *event_timer;
    InterruptController *irq_control = nullptr;
    InterruptController *local_irq_control = nullptr;
    AudioSystem *audio_system;
    bool shutting_down = false;

    uint8_t phasor_mode = PhasorLogic::kModeMockingboard;
    PhasorLogic::AySelection ay_selected[2] = {};

    uint64_t vid_cycles_rate;

    NClock *clock;

    // TODO: this is an undimensioned vector, which will be doing all kinds of memory allocation
    std::vector<float> audio_buffer;
    std::vector<float> secondary_audio_buffer;
    std::vector<float> speech_audio_buffer;
    uint64_t speech_sample_phase = 0;
    size_t max_speech_values_per_frame = 0;
    PhasorAudio::WarmthFilter warmth_filter;
    PhasorAudio::OutputClockRecovery output_clock_recovery;
    std::shared_ptr<AudioResetCallbackState> audio_reset_callback_state;

    static constexpr uint32_t kOutputChannels = 2;
    static constexpr uint32_t kOutputBytesPerFrame =
        kOutputChannels * sizeof(float);

    uint32_t queuedOutputFrames() const {
        if (!stream) return 0;
        const int queued_bytes = audio_system->get_stream_queued(stream);
        return queued_bytes > 0
            ? static_cast<uint32_t>(queued_bytes) / kOutputBytesPerFrame
            : 0;
    }

    void resetAndPrimeOutputStream() {
        if (!stream) return;

        SDL_ClearAudioStream(stream);
        SDL_SetAudioStreamFrequencyRatio(stream, 1.0f);
        output_clock_recovery.reset();

        const std::vector<float> silence(
            static_cast<size_t>(
                PhasorAudio::OutputClockRecovery::kPrefillFrames) *
                kOutputChannels,
            0.0f);
        if (audio_system->put_stream_data(
                stream, silence.data(),
                static_cast<uint32_t>(silence.size() * sizeof(float)))) {
            output_clock_recovery.markPrefilled();
        }
    }

    void updateOutputClockRecovery() {
        if (!stream) return;

        uint32_t queued_frames = queuedOutputFrames();
        if (output_clock_recovery.needsPrefill(queued_frames)) {
            // A drained stream has already inserted a discontinuity. Restart
            // it behind a silent safety margin instead of immediately
            // exposing another short frame to the device callback.
            resetAndPrimeOutputStream();
            queued_frames = queuedOutputFrames();
        }

        SDL_SetAudioStreamFrequencyRatio(
            stream, output_clock_recovery.update(queued_frames));
    }

    bool mockingboardMode() const {
        return PhasorLogic::isMockingboard(phasor_mode);
    }
    bool phasorNative() const {
        return PhasorLogic::isPhasorNative(phasor_mode);
    }
    bool echoPlus() const { return PhasorLogic::isEchoPlus(phasor_mode); }
    bool phasorExtended() const {
        return PhasorLogic::isExtended(phasor_mode);
    }

    bool directSpeechIrq() const {
        if (!phasorNative()) return false;
        return (ssi_primary.ready() && ssi_primary.interruptsEnabled()) ||
               (ssi_secondary.ready() && ssi_secondary.interruptsEnabled());
    }

    void updateCardIrq() {
        if (!irq_control) return;
        const bool asserted = !shutting_down &&
            ((local_irq_control && local_irq_control->any_irq_asserted()) ||
             directSpeechIrq());
        irq_control->set_irq(static_cast<device_irq_id>(slot), asserted);
    }

    void routeSpeechCompletion(SSI263 &ssi, uint8_t via) {
        if (ssi.takeCompletion() && mockingboardMode() &&
            ssi.interruptsEnabled()) {
            n6522[via]->signal_ca1_falling_edge();
        }
    }

    void clockDevices() {
        n6522[PhasorLogic::kViaHigh]->incr_cycle();
        n6522[PhasorLogic::kViaLow]->incr_cycle();

        // Render speech on the emulated cycle timeline, before advancing the
        // XCK state at this boundary. Bulk-rendering at the end of a video
        // frame would incorrectly apply the final phone/closure state to all
        // roughly 801 samples that came before it. The integer phase
        // accumulator converts the actual video-cycle rate to exactly 48 kHz
        // without accumulating floating-point drift.
        if (PhasorLogic::advanceAudioSamplePhase(
                speech_sample_phase, OUTPUT_SAMPLE_RATE_INT,
                vid_cycles_rate)) {
            const float secondary = ssi_secondary.renderSample();
            const float primary = ssi_primary.renderSample();
            // Single-step mode can suppress frame delivery while cycle
            // callbacks continue. Keep at most two frames of recent samples;
            // delivery below discards any older, now-inaudible history.
            if (max_speech_values_per_frame != 0 &&
                speech_audio_buffer.size() >=
                    max_speech_values_per_frame * 2) {
                speech_audio_buffer.erase(
                    speech_audio_buffer.begin(),
                    speech_audio_buffer.begin() +
                        max_speech_values_per_frame);
            }
            // Retain the two mono socket signals independently. The card
            // mixer centers each one at constant power after the AY banks
            // have rendered, rather than treating these as hard L/R samples.
            speech_audio_buffer.push_back(secondary);
            speech_audio_buffer.push_back(primary);
        }

        // NClock's video-cycle callback is the effective SSI XCK cadence
        // (~1.020484 MHz). Completion routing therefore happens at the exact
        // XCK boundary rather than being delayed until the next audio frame.
        ssi_primary.clockXck();
        ssi_secondary.clockXck();
        routeSpeechCompletion(ssi_primary, PhasorLogic::kViaHigh);
        routeSpeechCompletion(ssi_secondary, PhasorLogic::kViaLow);
        updateCardIrq();
    }

    void setAyClockRate() {
        const uint8_t multiplier = phasorNative() ? 2 : 1;
        ay_primary->setClockMultiplier(multiplier);
        ay_secondary->setClockMultiplier(multiplier);
    }

    void clearAySelections() {
        for (auto &selection : ay_selected) {
            selection = {false, false};
        }
    }

    void modeSwitch(uint32_t addr) {
        const uint8_t next_mode =
            PhasorLogic::updateModeLatch(phasor_mode,
                                         static_cast<uint16_t>(addr));
        if (next_mode == phasor_mode) return;

        phasor_mode = next_mode;
        setAyClockRate();
        if (!phasorExtended()) clearAySelections();

        // A/R is a level request. Re-route an already pending response when
        // software changes modes, just as the Phasor GAL does.
        if (mockingboardMode()) {
            if (ssi_primary.ready() && ssi_primary.interruptsEnabled()) {
                n6522[PhasorLogic::kViaHigh]->signal_ca1_falling_edge();
            }
            if (ssi_secondary.ready() && ssi_secondary.interruptsEnabled()) {
                n6522[PhasorLogic::kViaLow]->signal_ca1_falling_edge();
            }
        }
        updateCardIrq();
    }

    void ayBusCycle(uint8_t via) {
        // AY8910s renders chip 0 on the left and chip 1 on the right. Map
        // $Cn00/VIA0 to chip 0 and $Cn80/VIA1 to chip 1 even though the
        // legacy N6522 array uses the opposite index order.
        const uint8_t ay_chip = PhasorLogic::ayChipForVia(via);
        const uint8_t ddra = n6522[via]->get_ddra();
        const uint8_t pa = static_cast<uint8_t>(
            (n6522[via]->get_ora() & ddra) | static_cast<uint8_t>(~ddra));
        const uint8_t ddrb = n6522[via]->get_ddrb();
        const uint8_t pb = static_cast<uint8_t>(
            (n6522[via]->get_orb() & ddrb) | static_cast<uint8_t>(~ddrb));
        const double t = static_cast<double>(clock->get_vid_cycles()) /
                         static_cast<double>(vid_cycles_rate);

        const PhasorLogic::AyRoute route =
            PhasorLogic::decodeAyRoute(phasor_mode, via, pb,
                                       ay_selected[via]);

        if (route.reset) {
            ay_primary->busCycle(ay_chip, pa, pb, t);
            ay_secondary->busCycle(ay_chip, pa, pb, t);
            ay_selected[via] = route.next_selection;
            n6522[via]->set_ira(0xFF);
            return;
        }

        AyBusResult primary_result{false, 0};
        AyBusResult secondary_result{false, 0};
        if (route.drive_primary) {
            primary_result = ay_primary->busCycle(ay_chip, pa, pb, t);
        }
        if (route.drive_secondary) {
            secondary_result = ay_secondary->busCycle(ay_chip, pa, pb, t);
        }

        n6522[via]->set_ira(PhasorLogic::combineAyRead(
            primary_result.drove_data, primary_result.data,
            secondary_result.drove_data, secondary_result.data));
        ay_selected[via] = route.next_selection;
    }

public:
    Mockingboard(NClock *clock, InterruptController *irq_control, EventTimer *event_timer, AudioSystem *audio_system, uint8_t slot)
        : slot(slot), event_timer(event_timer), irq_control(irq_control),
          audio_system(audio_system), clock(clock) {
        // we need a local InterruptController to merge the IRQs from the two 6522 chips.
        local_irq_control = new InterruptController();
        local_irq_control->register_irq_receiver([this](bool) {
            updateCardIrq();
        });
        /* n6522[0] = new N6522("MB_6522 1 0x80", clock, local_irq_control, event_timer, slot, 0);
        n6522[1] = new N6522("MB_6522 2 0x00", clock, local_irq_control, event_timer, slot, 1); */
        n6522[0] = new N6522("MB_6522 1 0x80", clock, local_irq_control, slot, 0);
        n6522[1] = new N6522("MB_6522 2 0x00", clock, local_irq_control, slot, 1);

        // TODO: this doesn't need irq_control or slot
        ay_primary = new AY8910s(&audio_buffer, event_timer, clock, audio_system /* , irq_control, slot */);
        ay_secondary = new AY8910s(&secondary_audio_buffer, event_timer, clock, audio_system /* , irq_control, slot */);

        last_cycle = 0;

        vid_cycles_rate = clock->get_vid_cycles_per_second();
        speech_sample_phase = vid_cycles_rate - OUTPUT_SAMPLE_RATE_INT;
        const uint64_t max_frame_samples =
            (clock->get_vid_cycles_per_frame() * OUTPUT_SAMPLE_RATE_INT +
             vid_cycles_rate - 1) / vid_cycles_rate + 1;
        max_speech_values_per_frame =
            static_cast<size_t>(max_frame_samples) * 2;

        stream = audio_system->create_stream(OUTPUT_SAMPLE_RATE_INT, 2, SDL_AUDIO_F32LE, false);

        // AudioSystem clears bound streams when the default device format
        // changes. Re-prime this stream in the same event turn; otherwise its
        // first post-change video frame is smaller than a common host callback
        // and starts with an audible underrun. The indirection makes the
        // retained callback harmless after this card has been destroyed.
        audio_reset_callback_state =
            std::make_shared<AudioResetCallbackState>();
        audio_reset_callback_state->owner = this;
        audio_system->register_device_reset_callback(
            [state = std::weak_ptr<AudioResetCallbackState>(
                 audio_reset_callback_state)]() {
                if (const std::shared_ptr<AudioResetCallbackState> live =
                        state.lock();
                    live && live->owner) {
                    live->owner->resetAndPrimeOutputStream();
                }
            });

        // Port A pull-ups hold the bus high at power-on; match reset().
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);

        clock->set_cycle_handler([this]() { clockDevices(); });
    }
    ~Mockingboard() {
        audio_reset_callback_state->owner = nullptr;
        shutting_down = true;
        updateCardIrq();
        audio_system->destroy_stream(stream);

        delete n6522[0];
        delete n6522[1];
        delete ay_primary;
        delete ay_secondary;
        delete local_irq_control;
    }

    void insert_empty_frame() {
        // Keep synthesis at exactly 48 kHz, but start the asynchronous host
        // device behind enough silence to cover its callback quantum and
        // normal scheduling jitter.
        resetAndPrimeOutputStream();
    }
    
    void debug_registers();

    void accessModeSwitch(uint32_t addr) {
        modeSwitch(addr);
    }

    void write(uint32_t addr, uint8_t data) {   // this is address & 0xFF
        const uint8_t offset = addr & 0xFF;
        const uint8_t reg = offset & 0x0F;
        const PhasorLogic::ViaHits via_hits =
            PhasorLogic::decodeViaHits(phasor_mode, offset);

        for (uint8_t via = 0; via < 2; ++via) {
            if (!PhasorLogic::viaHit(via_hits, via)) continue;
            n6522[via]->write(reg, data);
            if (reg == MB_6522_ORB) ayBusCycle(via);
        }

        // SSI decoding is additional to (and may overlap) the VIA decode.
        // A6 selects the primary/right socket and A5 the secondary/left;
        // $60-$7F therefore broadcasts to both devices.
        const PhasorLogic::SsiSelects ssi_selects =
            PhasorLogic::decodeSsiWrites(phasor_mode, offset);
        const uint8_t ssi_reg = SSI263::registerForOffset(offset);
        if (ssi_selects.primary) ssi_primary.write(ssi_reg, data);
        if (ssi_selects.secondary) ssi_secondary.write(ssi_reg, data);
        updateCardIrq();
    }
    
    uint8_t read(uint32_t addr, uint8_t floating_bus) {
        const uint8_t offset = addr & 0xFF;

        // Native SSI reads exist only where neither interleaved VIA is
        // selected. D7 overlays the Apple floating bus; when both sockets
        // are selected, the secondary socket owns D7.
        const PhasorLogic::SsiSocket status_socket =
            PhasorLogic::nativeStatusSocket(phasor_mode, offset);
        if (status_socket != PhasorLogic::SsiSocket::None) {
            const bool d7 = status_socket == PhasorLogic::SsiSocket::Secondary
                                ? ssi_secondary.ready()
                                : ssi_primary.ready();
            return PhasorLogic::nativeStatusValue(floating_bus, d7);
        }

        const PhasorLogic::ViaHits via_hits =
            PhasorLogic::decodeViaHits(phasor_mode, offset);
        uint8_t result = 0;
        bool any_hit = false;
        for (uint8_t via = 0; via < 2; ++via) {
            if (!PhasorLogic::viaHit(via_hits, via)) continue;
            result |= PhasorLogic::readVia(phasor_mode, offset,
                                           *n6522[via]);
            any_hit = true;
        }
        return any_hit ? result : floating_bus;
    }
    
    void generate_frame() {
        static int frames = 0;

        if (max_speech_values_per_frame != 0 &&
            speech_audio_buffer.size() > max_speech_values_per_frame) {
            speech_audio_buffer.erase(
                speech_audio_buffer.begin(),
                speech_audio_buffer.end() - max_speech_values_per_frame);
        }

        // Use the exact count produced by the shared cycle-to-sample phase.
        // This keeps both SSI streams and both AY banks sample-aligned even
        // when the rational cadence alternates frame sizes.
        const uint32_t samples_this_frame = static_cast<uint32_t>(
            speech_audio_buffer.size() / 2);
    
        last_cycle = clock->get_vid_cycles();
    
        const uint8_t primary_mask = echoPlus() ? 0x02 : 0x03;
        const uint8_t secondary_mask = phasorNative() ? 0x03 :
                                       (echoPlus() ? 0x02 : 0x00);
        ay_primary->generateSamples(samples_this_frame, primary_mask);

        secondary_audio_buffer.clear();
        ay_secondary->generateSamples(samples_this_frame, secondary_mask);

        // Speech samples were produced on their original XCK timeline. Mix
        // both mono SSI sockets at constant-power center, preserving the AY
        // banks' existing stereo topology. Do one final saturation only after
        // every card source has contributed, then apply Appletini's fixed +8
        // card-level warmth network on the exact 48 kHz emulated timeline.
        const bool secondary_ay_audible = phasorNative() || echoPlus();
        const size_t value_count = std::min(
            audio_buffer.size(),
            std::min(secondary_audio_buffer.size(),
                     speech_audio_buffer.size()));
        for (size_t i = 0; i + 1 < value_count; i += 2) {
            const PhasorLogic::StereoSample mixed =
                PhasorLogic::mixAudioSample(
                    audio_buffer[i], audio_buffer[i + 1],
                    secondary_ay_audible ? secondary_audio_buffer[i] : 0.0f,
                    secondary_ay_audible ? secondary_audio_buffer[i + 1] : 0.0f,
                    speech_audio_buffer[i], speech_audio_buffer[i + 1]);
            const PhasorAudio::StereoSample shaped =
                warmth_filter.process(mixed.left, mixed.right);
            audio_buffer[i] = shaped.left;
            audio_buffer[i + 1] = shaped.right;
        }
        speech_audio_buffer.clear();
    
        // Clear the audio buffer after each frame to prevent memory buildup
        // Send the generated audio data to the SDL audio stream
        int abs = audio_buffer.size();
        if (abs > 0) {
            //printf("generate_mockingboard_frame: %zu\n", mb_d->audio_buffer.size());
            updateOutputClockRecovery();
            audio_system->put_stream_data(
                stream, audio_buffer.data(),
                static_cast<uint32_t>(audio_buffer.size() * sizeof(float)));
        }
        audio_buffer.clear();
    
        if (DEBUG(DEBUG_MOCKINGBOARD)) {
            if (frames++ > 60) {
                frames = 0;
                // Get the number of samples in SDL audio stream buffer
                int samples_in_buffer = 0;
                if (stream) {
                    samples_in_buffer = SDL_GetAudioStreamAvailable(stream) / sizeof(float);
                }
                printf("MB Status: buffer: %d, audio buffer size: %d, samples_per_frame: %d\n", samples_in_buffer, abs, samples_this_frame);
            }
        }
    }

    void reset(bool cold_start) {
        phasor_mode = PhasorLogic::kModeMockingboard;
        clearAySelections();
        setAyClockRate();
        n6522[0]->reset();
        n6522[1]->reset();
        ay_primary->reset();
        ay_secondary->reset();
        ssi_primary.reset(cold_start);
        ssi_secondary.reset(cold_start);
        if (cold_start) {
            speech_audio_buffer.clear();
            speech_sample_phase = vid_cycles_rate - OUTPUT_SAMPLE_RATE_INT;
            warmth_filter.reset();
        }
        // Port A pull-ups on the Mockingboard hold the bus high whenever
        // neither the AY nor the VIA is driving. Pre-seed IRA so the CPU
        // sees $FF on the very first ORA read (before any bus cycle).
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);
        updateCardIrq();
    }

    DebugFormatter *debug() {
        DebugFormatter *df = new DebugFormatter();
        n6522[0]->debug(df);
        n6522[1]->debug(df);
        df->addLine("Phasor mode: %u", phasor_mode);
        df->addLine("SSI-263 primary: phone=%02X active=%d ready=%d samples=%u",
                    ssi_primary.phoneme(), ssi_primary.active(), ssi_primary.ready(),
                    ssi_primary.samplesRemaining());
        df->addLine("SSI-263 secondary: phone=%02X active=%d ready=%d samples=%u",
                    ssi_secondary.phoneme(), ssi_secondary.active(), ssi_secondary.ready(),
                    ssi_secondary.samplesRemaining());
        // TODO: add AY-8910s debug
        return df;
    }
};

void mb_write_Cx00(void *context, uint32_t addr, uint8_t data) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    mb_d->mockingboard->write(addr, data);
}

uint8_t mb_read_Cx00(void *context, uint32_t addr) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    return mb_d->mockingboard->read(
        addr, mb_d->computer->mmu->floating_bus_read());
}

void mb_write_C0nx(void *context, uint32_t addr, uint8_t /*data*/) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    mb_d->mockingboard->accessModeSwitch(addr);
}

uint8_t mb_read_C0nx(void *context, uint32_t addr) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    const uint8_t floating_bus = mb_d->computer->mmu->floating_bus_read();
    mb_d->mockingboard->accessModeSwitch(addr);
    return floating_bus;
}

void init_slot_mockingboard(computer_t *computer, SlotType_t slot) {

    printf("init_slot_mockingboard: %d\n", slot);

    mb_cpu_data *mb_d = new mb_cpu_data;
    mb_d->id = DEVICE_ID_MOCKINGBOARD;
    mb_d->_slot = slot;
    mb_d->computer = computer;
    mb_d->clock = computer->clock;
    mb_d->audio_system = computer->audio_system;
    mb_d->irq_control = computer->irq_control;
    mb_d->event_timer = computer->vid_event_timer;
    mb_d->mockingboard = new Mockingboard(computer->clock, computer->irq_control, mb_d->event_timer, computer->audio_system, slot);
    
    mb_d->slot = slot;

    computer->mmu->map_c1cf_page_write_h(0xC0 + slot, { mb_write_Cx00, mb_d }, "MB_IO");
    computer->mmu->map_c1cf_page_read_h(0xC0 + slot, { mb_read_Cx00, mb_d }, "MB_IO");

    // Phasor's mode GAL observes any read or write at $C080 + slot*$10.
    // A3 first clears the mode latch, then A2..A0 are ORed into it.
    const uint16_t mode_base = PhasorLogic::modeSwitchBase(slot);
    for (uint16_t offset = 0; offset < 0x10; ++offset) {
        computer->mmu->set_C0XX_read_handler(
            mode_base + offset, { mb_read_C0nx, mb_d });
        computer->mmu->set_C0XX_write_handler(
            mode_base + offset, { mb_write_C0nx, mb_d });
    }

    mb_d->mockingboard->insert_empty_frame();

    // this can move to the class
    // set up a reset handler to reset the chips on mockingboard
    computer->register_reset_handler(
        [mb_d](bool cold_start) {
            mb_d->mockingboard->reset(cold_start);
            return true;
        });

    // this can move to the class
    // register a frame processor for the mockingboard.
    computer->device_frame_dispatcher->registerHandler([mb_d,computer]() {
        // if in single step, return.
        if (computer->execution_mode == EXEC_NORMAL) {
            mb_d->mockingboard->generate_frame();
        }
        return true;
    });

    computer->register_shutdown_handler([mb_d]() {
        delete mb_d->mockingboard;
        //SDL_DestroyAudioStream(mb_d->stream);
        delete mb_d;
        return true;
    });

    computer->register_debug_display_handler(
        "phasor",
        DH_MOCKINGBOARD, // unique ID for this, need to have in a header.
        [mb_d]() -> DebugFormatter * {
            DebugFormatter *df = mb_d->mockingboard->debug();
            
            mb_d->audio_system->getCurrentAudioFormat(df);            
            return df;
            //return debug_registers_6522(mb_d);
            //return nullptr;
        }
    );
}

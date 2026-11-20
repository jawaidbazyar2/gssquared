#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>
#include "debug.hpp"
#include "util/InterruptController.hpp"
#include "util/AudioSystem.hpp"
#include "NClock.hpp"
#include "AyPhase.hpp"
#include <cmath>

static constexpr uint8_t AY_SYNTH_RESET = 0xFF;

enum AY_Registers {
    A_Tone_Low = 0,
    A_Tone_High = 1,
    B_Tone_Low = 2,
    B_Tone_High = 3,
    C_Tone_Low = 4,
    C_Tone_High = 5,
    Noise_Period = 6,
    Mixer_Control = 7,
    Ampl_A = 8, // 10,
    Ampl_B = 9, // 11,
    Ampl_C = 10, // 12,
    Envelope_Period_Low = 11, // 13,
    Envelope_Period_High = 12, // 14,
    Envelope_Shape = 13, // 15,
    Unknown_14 = 14,
    Unknown_15 = 15,
};


struct filter_state {
    float last_sample;
    float alpha;
};

#define AY_8913_REGISTER_COUNT 16

// Result of a Mockingboard bus-cycle as seen by the AY chip.
// drove_data is true when the AY drove the data bus (read command),
// in which case `data` holds the byte to be latched into the VIA IRA.
struct AyBusResult {
    bool    drove_data;
    uint8_t data;
};

// Global constants
// must be based on 59.9227 fps rate just like Speaker code.
//constexpr double OUTPUT_SAMPLE_RATE = 44100.0;
constexpr uint32_t OUTPUT_SAMPLE_RATE_INT = 44100;
constexpr uint32_t SAMPLES_PER_FRAME_INT = 740;

// Event to represent register changes with video-cycle timestamps
struct RegisterEvent {
    uint64_t cycle;        // Video cycle when this event occurs
    uint8_t chip_index;    // Which AY chip (0 or 1)
    uint8_t register_num;  // Which register (0-13)
    uint8_t value;         // New value for the register
    
    bool operator<(const RegisterEvent& other) const {
        return cycle < other.cycle;
    }
};

#if 0
// Add a static array for the normalized volume levels
static const float normalized_levels[16] = {
    0.00000,
    0.00795,
    0.01123,
    0.01586,
    0.02241,
    0.03165,
    0.04470,
    0.06313,
    0.08917,
    0.12595,
    0.17790,
    0.25127,
    0.35489,
    0.50126,
    0.70800,
    1.00000,
};
#endif

#if 1
// Add a static array for the normalized volume levels
static const float normalized_levels[16] = {
    0x0000 / 65535.0f * 0.25f, 0x0385 / 65535.0f * 0.25f, 0x053D / 65535.0f * 0.25f, 0x0770 / 65535.0f * 0.25f,
    0x0AD7 / 65535.0f * 0.25f, 0x0FD5 / 65535.0f * 0.25f, 0x15B0 / 65535.0f * 0.25f, 0x230C / 65535.0f * 0.25f,
    0x2B4C / 65535.0f * 0.25f, 0x43C1 / 65535.0f * 0.25f, 0x5A4B / 65535.0f * 0.25f, 0x732F / 65535.0f * 0.25f,
    0x9204 / 65535.0f * 0.25f, 0xAFF1 / 65535.0f * 0.25f, 0xD921 / 65535.0f * 0.25f, 0xFFFF / 65535.0f * 0.25f
};

// Add a static array for the normalized volume levels
/* static const float normalized_levels[16] = {
    0x0000 / 65535.0f, 0x0385 / 65535.0f, 0x053D / 65535.0f, 0x0770 / 65535.0f,
    0x0AD7 / 65535.0f, 0x0FD5 / 65535.0f, 0x15B0 / 65535.0f, 0x230C / 65535.0f,
    0x2B4C / 65535.0f, 0x43C1 / 65535.0f, 0x5A4B / 65535.0f, 0x732F / 65535.0f,
    0x9204 / 65535.0f, 0xAFF1 / 65535.0f, 0xD921 / 65535.0f, 0xFFFF / 65535.0f
};
 */
#endif

 class AY8910s {
    private:
        static constexpr float FILTER_CUTOFF = 0.3f; // Filter coefficient (0-1) (lower is more aggressive)

        struct ToneChannel {
            uint16_t period = 0;
            ay::ToneOscillator osc;
            float output = 0.0f;
            float volume = 0;
            bool use_envelope = false;
        };

        struct AY3_8910 {
            ToneChannel tone_channels[3];
            uint16_t noise_period = 1;
            ay::NoiseOscillator noise;
            bool noise_output = false;
            uint8_t mixer_control = 0;
            uint16_t envelope_period = 0;
            ay::EnvelopeGenerator envelope;
            uint8_t envelope_shape = 0;
            uint8_t envelope_output = 0;
            bool envelope_hold = false;
            bool envelope_attack = false;
            uint8_t registers[AY_8913_REGISTER_COUNT] = {};
            uint8_t live_registers[AY_8913_REGISTER_COUNT] = {};
        };

    public:
        AY8910s(std::vector<float>* buffer, NClock *clock, AudioSystem *audio_system /* , InterruptController *irq_control, uint8_t slot */) 
            : clock_(clock),
              sample_vid_(clock ? clock->get_vid_cycles() : 0),
              sample_accum_(0),
              audio_buffer(buffer), audio_system(audio_system) {
            // Initialize per-chip bus address latch to "invalid / no register
            // selected" so writes without a preceding LATCH are ignored
            // (matches AY8913 power-on/reset behavior; audited by mb-audit
            // TestAYLatchAddress / TestAYReadHiZ).
            reg_num[0] = 0xFF;
            reg_num[1] = 0xFF;
            // Initialize chips
            for (int c = 0; c < 2; c++) {
                // Initialize registers to 0
                for (int r = 0; r < AY_8913_REGISTER_COUNT; r++) {
                    chips[c].registers[r] = 0;
                    chips[c].live_registers[r] = 0;
                }
                
                for (int i = 0; i < 3; i++) {
                    chips[c].tone_channels[i].period = 0;
                    chips[c].tone_channels[i].output = 0.0f;
                    chips[c].tone_channels[i].volume = 0;
                    chips[c].tone_channels[i].use_envelope = false;
                }
                chips[c].noise_period = 1;
                chips[c].noise_output = false;
                chips[c].mixer_control = 0;
                chips[c].envelope_period = 0;
                chips[c].envelope_shape = 0;
                chips[c].envelope_output = 0;
                chips[c].envelope_hold = false;
                chips[c].envelope_attack = false;
                resetChipClocks(chips[c]);
            }
            if (clock_) {
                setAllClockRates(clock_->get_vid_cycles_per_second(), OUTPUT_SAMPLE_RATE_INT);
            }
            for (int i = 0; i < 7; i++) {
                filters[i].last_sample = 0.0f;
            }
            for (int i = 0; i < 3; i++) {
                setChannelAlpha(0, i);
                setChannelAlpha(1, i);
            }
        }
        
        // Set the audio buffer
        void setAudioBuffer(std::vector<float>* buffer) {
            audio_buffer = buffer;
        }
        
        void reset() {
            for (int c = 0; c < 2; c++) {
                for (int r = 0; r < AY_8913_REGISTER_COUNT; r++) {
                    chips[c].registers[r] = 0;
                    chips[c].live_registers[r] = 0;
                }
                chips[c].registers[Mixer_Control] = 0x3F; // channels disabled
                chips[c].live_registers[Mixer_Control] = 0x3F; // channels disabled
                chips[c].mixer_control = 0x3F; // channels disabled
                // With tone and noise both disabled the mixer passes the level
                // straight through, so stale volumes would become a DC step.
                for (int i = 0; i < 3; i++) {
                    chips[c].tone_channels[i].period = 0;
                    chips[c].tone_channels[i].output = 0.0f;
                    chips[c].tone_channels[i].volume = 0;
                    chips[c].tone_channels[i].use_envelope = false;
                }
                chips[c].noise_period = 1;
                chips[c].noise_output = false;
                chips[c].envelope_period = 0;
                chips[c].envelope_shape = 0;
                chips[c].envelope_output = 0;
                chips[c].envelope_hold = false;
                chips[c].envelope_attack = false;
                resetChipClocks(chips[c]);
                // Invalidate the AY register-address latch on reset (see
                // constructor comment). Any value >= AY_8913_REGISTER_COUNT
                // serves as the sentinel for "no register selected".
                reg_num[c] = 0xFF;
            }
            pending_events.clear();
            if (clock_) {
                sample_vid_ = clock_->get_vid_cycles();
            }
            sample_accum_ = 0;
            // Zero the R-channel decorrelation delay line so reset produces
            // a clean output with no residual samples from the previous run.
            for (size_t i = 0; i < MONO_DECORR_DELAY; i++) r_delay_buf[i] = 0.0f;
            r_delay_idx = 0;
        }

        uint64_t sampleVid() const { return sample_vid_; }

        // Snap the sample horizon to the start of the current video frame
        // without zeroing chip state. Used after AudioSystem clears streams
        // on a device format change, so generateSamples does not dump a
        // catch-up burst into the empty queue.
        void syncHorizon() {
            pending_events.clear();
            sample_accum_ = 0;
            if (!clock_) {
                return;
            }
            const uint64_t frame_end_vid = clock_->get_vid_cycles();
            const uint64_t vid_per_frame = clock_->get_vid_cycles_per_frame();
            sample_vid_ = (frame_end_vid > vid_per_frame)
                              ? frame_end_vid - vid_per_frame
                              : 0;
        }

        // Handle one Mockingboard bus cycle as driven by the paired 6522's
        // Port B (control) and Port A (data) lines. This owns the AY bus
        // protocol (BDIR/BC1/~RESET) so the VIA stays a generic MOS 6522.
        //
        // pb bit layout as wired on a Mockingboard:
        //   bit 0 = BC1
        //   bit 1 = BDIR
        //   bit 2 = ~RESET (active low)
        // (BDIR, BC1): 00 inactive, 01 read, 10 write, 11 latch address.
        //
        // Returns { drove_data=true, data } on read cycles so the board
        // can latch the byte into the matching VIA IRA.
        AyBusResult busCycle(uint8_t chip_index, uint8_t pa, uint8_t pb, uint64_t cycle) {
            if (chip_index > 1) return { false, 0 };

            // ~RESET asserted: zero all 16 registers at this cycle and
            // invalidate the address latch. A subsequent WRITE (without a new
            // LATCH) must be ignored, and a subsequent READ must produce HiZ
            // (no data driven onto Port A).
            if ((pb & 0b100) == 0) {
                queueRegisterChange(cycle, chip_index, AY_SYNTH_RESET, 0);
                for (uint8_t r = 0; r < AY_8913_REGISTER_COUNT; r++) {
                    queueRegisterChange(cycle, chip_index, r, 0);
                }
                reg_num[chip_index] = 0xFF;
                return { false, 0 };
            }

            switch (pb & 0b011) {
                case 0b11: // latch address
                    // Real AY8913 decodes the full 8-bit address on Port A.
                    // Any value with the high nibble non-zero (or any value
                    // >= AY_8913_REGISTER_COUNT) is "unmatched" and leaves
                    // the chip with no register selected. Store the raw byte
                    // and let the WRITE/READ cases validate it.
                    reg_num[chip_index] = pa;
                    if (DEBUG(DEBUG_MOCKINGBOARD)) printf("AY busCycle: latch chip %u reg %02x\n", chip_index, reg_num[chip_index]);
                    return { false, 0 };
                case 0b10: // write
                    if (reg_num[chip_index] < AY_8913_REGISTER_COUNT) {
                        queueRegisterChange(cycle, chip_index, reg_num[chip_index], pa);
                        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("AY busCycle: write [%llu] chip %u reg %02x val %02x\n", (unsigned long long)cycle, chip_index, reg_num[chip_index], pa);
                    } else {
                        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("AY busCycle: write ignored (unlatched) chip %u latch=%02x val %02x\n", chip_index, reg_num[chip_index], pa);
                    }
                    return { false, 0 };
                case 0b01: { // read
                    if (reg_num[chip_index] < AY_8913_REGISTER_COUNT) {
                        uint8_t v = read_register(chip_index, reg_num[chip_index]);
                        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("AY busCycle: read chip %u reg %02x -> %02x\n", chip_index, reg_num[chip_index], v);
                        return { true, v };
                    }
                    // No register selected: AY data bus is HiZ, so the 6522
                    // Port A reads whatever is on the bus (typically $FF via
                    // pull-ups on the Mockingboard). Returning drove=false
                    // lets the board preserve the existing IRA / DDR state.
                    if (DEBUG(DEBUG_MOCKINGBOARD)) printf("AY busCycle: read HiZ (unlatched) chip %u latch=%02x\n", chip_index, reg_num[chip_index]);
                    return { false, 0 };
                }
                default: // 0b00 inactive
                    return { false, 0 };
            }
        }
    
        // Add a register change event
        void queueRegisterChange(uint64_t cycle, uint8_t chip_index, uint8_t reg, uint8_t value) {
            RegisterEvent event;
            event.cycle = cycle;
            event.chip_index = chip_index;
            event.register_num = reg;
            event.value = value;

            pending_events.push_back(event);

            if (event.register_num < AY_8913_REGISTER_COUNT) {
                AY3_8910& chip = chips[event.chip_index];
                chip.live_registers[event.register_num] = event.value;
            }
    
            dbg_last_event = event.cycle;
            dbg_last_time = sample_vid_;
    
            if (event.cycle < sample_vid_) {
                printf("[sample_vid: %llu] Event cycle is in the past: %llu\n",
                       (unsigned long long)sample_vid_, (unsigned long long)event.cycle);
            }
        }
        
        // Process a register change
        void processRegisterChange(const RegisterEvent& event) {
            if (event.chip_index > 1) {
                return;
            }
            if (event.register_num == AY_SYNTH_RESET) {
                resetChipClocks(chips[event.chip_index]);
                return;
            }
            if (event.register_num > 15) {
                return;
            }
            
            AY3_8910& chip = chips[event.chip_index];
            chip.registers[event.register_num] = event.value;
            
            if (DEBUG(DEBUG_MOCKINGBOARD)) display_registers();
            
            // Update internal state based on register change
            switch (event.register_num) {
                case A_Tone_Low:
                case A_Tone_High:
                    chip.tone_channels[0].period = ((chip.registers[A_Tone_High] & 0x0F) << 8) | chip.registers[A_Tone_Low];
                    chip.tone_channels[0].osc.setPeriod(chip.tone_channels[0].period);
                    setChannelAlpha(event.chip_index, 0);
                    break;
                case B_Tone_Low:
                case B_Tone_High:
                    chip.tone_channels[1].period = ((chip.registers[B_Tone_High] & 0x0F) << 8) | chip.registers[B_Tone_Low];
                    chip.tone_channels[1].osc.setPeriod(chip.tone_channels[1].period);
                    setChannelAlpha(event.chip_index, 1);
                    break;
                case C_Tone_Low:
                case C_Tone_High:
                    chip.tone_channels[2].period = ((chip.registers[C_Tone_High] & 0x0F) << 8) | chip.registers[C_Tone_Low];
                    chip.tone_channels[2].osc.setPeriod(chip.tone_channels[2].period);
                    setChannelAlpha(event.chip_index, 2);
                    break;

                    case Noise_Period: // Noise period
                    chip.noise_period = event.value & 0x1F;
                    if (chip.noise_period == 0) {
                        chip.noise_period = 1; // Avoid division by zero
                    }
                    chip.noise.setPeriod(chip.noise_period);
                    break;
                    
                case Mixer_Control: // Mixer control
                    chip.mixer_control = event.value;
                    break;
                    
                case Envelope_Period_Low: // Envelope period low bits
                    chip.envelope_period = (chip.envelope_period & 0xFF00) | event.value;
                    chip.envelope.setEnvelopePeriod(chip.envelope_period);
                    break;
                    
                case Envelope_Period_High: // Envelope period high bits
                    chip.envelope_period = (chip.envelope_period & 0x00FF) | (event.value << 8);
                    chip.envelope.setEnvelopePeriod(chip.envelope_period);
                    break;
                    
                case Envelope_Shape: // Envelope shape
                    chip.envelope_shape = event.value & 0x0F;
                    chip.envelope.resetPhase();
                    chip.envelope_hold = false;
                    // Start in the correct phase based on Attack bit
                    chip.envelope_attack = (chip.envelope_shape & 0x04) != 0;
                    // Set initial output value based on attack/decay
                    chip.envelope_output = chip.envelope_attack ? 0 : 15;
                    applyEnvelopeVolume(chip);
                    break;
                    
                case Ampl_A: // Channel A volume
                case Ampl_B: // Channel B volume
                case Ampl_C: // Channel C volume
                    {
                        int channel = event.register_num - Ampl_A;
                        uint16_t volsetting = event.value & 0x0F;
                        // Check if envelope is enabled (bit 4)
#if 0
                        chip.tone_channels[channel].use_envelope = (event.value & 0x10) != 0;
                        chip.tone_channels[channel].volume = normalized_levels[volsetting];
                        //printf("%llu|chip %d channel %d use_envelope %d volume %d/%f\n", (unsigned long long)event.cycle, event.chip_index, channel, chip.tone_channels[channel].use_envelope, volsetting, chip.tone_channels[channel].volume);
#endif
#if 1
                        if (event.value & 0x10) {
                            chip.tone_channels[channel].use_envelope = true;
                            // Set initial volume from current envelope output, normalized to 0-1
                            chip.tone_channels[channel].volume = normalized_levels[chip.envelope_output];
                        } else {
                            chip.tone_channels[channel].use_envelope = false;
                            // Convert direct volume to 0-1 range
                            chip.tone_channels[channel].volume = normalized_levels[event.value & 0x0F];
                        }
#endif
                    }
                    break;
            }
        }
        
        void applyDueEvents(uint64_t until_cycle) {
            while (!pending_events.empty() && pending_events.front().cycle <= until_cycle) {
                processRegisterChange(pending_events.front());
                pending_events.pop_front();
            }
        }

        void applyEnvelopeVolume(AY3_8910& chip) {
            const float level = normalized_levels[chip.envelope_output];
            for (int i = 0; i < 3; i++) {
                if (chip.tone_channels[i].use_envelope) {
                    chip.tone_channels[i].volume = level;
                }
            }
        }

        void setWaveform(ay::Waveform wf) {
            waveform_ = wf;
            if (audio_system) {
                audio_system->set_ay_waveform(wf);
            }
        }

        ay::Waveform getWaveform() const {
            return audio_system ? audio_system->get_ay_waveform() : waveform_;
        }

        void resetChipClocks(AY3_8910& chip) {
            for (int i = 0; i < 3; i++) {
                chip.tone_channels[i].osc.resetPhase();
                chip.tone_channels[i].osc.setPeriod(chip.tone_channels[i].period);
                chip.tone_channels[i].output = 0.0f;
            }
            chip.noise.resetPhase();
            chip.noise.setPeriod(chip.noise_period);
            chip.noise_output = false;
            chip.envelope.resetPhase();
            chip.envelope.setEnvelopePeriod(chip.envelope_period);
        }

        void setAllClockRates(uint64_t vid_per_sec, uint32_t sample_rate) {
            for (int c = 0; c < 2; c++) {
                AY3_8910& chip = chips[c];
                for (int i = 0; i < 3; i++) {
                    chip.tone_channels[i].osc.setClockRate(vid_per_sec, sample_rate);
                }
                chip.noise.setClockRate(vid_per_sec, sample_rate);
                chip.envelope.setClockRate(vid_per_sec, sample_rate);
            }
        }

        // One step of the AY envelope shape machine (hold / alternate / attack / continue).
        void stepEnvelopeMachine(AY3_8910& chip) {
            if (chip.envelope_hold) {
                return;
            }

            const bool hold = (chip.envelope_shape & 0x01) != 0;
            const bool alternate = (chip.envelope_shape & 0x02) != 0;
            const bool attack = (chip.envelope_shape & 0x04) != 0;
            const bool cont = (chip.envelope_shape & 0x08) != 0;

            if (chip.envelope_attack) {
                if (chip.envelope_output < 15) {
                    chip.envelope_output++;
                } else if (cont && hold) {
                    chip.envelope_output = (attack != alternate) ? 15 : 0;
                    chip.envelope_hold = true;
                } else if (hold) {
                    chip.envelope_hold = true;
                } else if (!cont) {
                    chip.envelope_output = 0;
                    chip.envelope_hold = true;
                } else if (alternate) {
                    chip.envelope_attack = false;
                } else {
                    chip.envelope_output = attack ? 0 : 15;
                }
            } else {
                if (chip.envelope_output > 0) {
                    chip.envelope_output--;
                } else if (cont && hold) {
                    chip.envelope_output = (attack != alternate) ? 15 : 0;
                    chip.envelope_hold = true;
                } else if (hold) {
                    chip.envelope_hold = true;
                } else if (!cont) {
                    chip.envelope_hold = true;
                } else if (alternate) {
                    chip.envelope_attack = true;
                } else {
                    chip.envelope_output = attack ? 0 : 15;
                }
            }
        }
        
        // Generate audio samples covering video cycles up to frame_end_vid.
        void generateSamples(uint64_t frame_end_vid) {
            if (!audio_buffer || !clock_) {
                return;
            }

            const uint64_t vid_per_sec = clock_->get_vid_cycles_per_second();
            const uint64_t vid_per_frame = clock_->get_vid_cycles_per_frame();

            // Ludicrous speed / stall: do not synthesize the missed span.
            setAllClockRates(vid_per_sec, OUTPUT_SAMPLE_RATE_INT);

            if (frame_end_vid > sample_vid_ &&
                (frame_end_vid - sample_vid_) > vid_per_frame * 3) {
                pending_events.clear();
                sample_vid_ = frame_end_vid - vid_per_frame;
                sample_accum_ = 0;
            }

            if (frame_end_vid <= sample_vid_) {
                return;
            }

            while (sample_vid_ < frame_end_vid) {
                sample_accum_ += vid_per_sec;
                uint64_t cycles = sample_accum_ / OUTPUT_SAMPLE_RATE_INT;
                sample_accum_ %= OUTPUT_SAMPLE_RATE_INT;
                if (cycles == 0) {
                    continue;
                }
                // Do not run the sample horizon past the live video clock.
                uint64_t sample_end = sample_vid_ + cycles;
                if (sample_end > frame_end_vid) {
                    sample_end = frame_end_vid;
                }

                applyDueEvents(sample_end);

                const ay::Waveform waveform = getWaveform();

                for (int c = 0; c < 2; c++) {
                    AY3_8910& chip = chips[c];
                    for (int ch = 0; ch < 3; ch++) {
                        chip.tone_channels[ch].output = chip.tone_channels[ch].osc.step(waveform);
                    }
                    chip.noise_output = chip.noise.step();
                    const uint32_t env_steps = chip.envelope.step();
                    for (uint32_t s = 0; s < env_steps; s++) {
                        stepEnvelopeMachine(chip);
                    }
                    if (env_steps) {
                        applyEnvelopeVolume(chip);
                    }
                }
                
                // Mix output from 3 channels of each chip separately.
                float mixed_output[2] = {0.0f, 0.0f};
    
                for (int c = 0; c < 2; c++) {
    
                    const AY3_8910& chip = chips[c];
                    // AY data sheet mixer: a channel conducts while its tone and
                    // noise gates are both open; a disabled source is open. With
                    // both disabled, the level register drives the output
                    // directly, which is how samples are played.
                    for (int channel = 0; channel < 3; channel++) {
                        const ToneChannel& tone = chip.tone_channels[channel];
                        bool tone_dis  = (chip.mixer_control >> channel) & 1;       // 1 = disabled
                        bool noise_dis = (chip.mixer_control >> (channel + 3)) & 1; // 1 = disabled
                        // The tone generator doesn't run at period 0, so tone.output is
                        // frozen; gating on it turns volume changes into clicks.
                        if (!tone_dis && tone.period == 0) {
                            continue;
                        }
                        if (chip.noise_output || noise_dis) {
                            const float tone_level = tone_dis ? 1.0f : tone.output;
                            mixed_output[c] += tone_level * tone.volume;
                        }
                    }
                    
                }
                // TODO: add checks here to detect overdriving/exceeding -1.0/1.0.
                // Append the mixed samples to the buffer.
                //
                // R-channel decorrelation delay (gated by AudioSystem): breaks
                // the L+R phase cancellation that occurs when a mono speaker
                // acoustically sums our stereo output. The two AY chips run
                // with independent tone-counter phases (as real hardware
                // does), so identical content on both chips can become 180
                // degrees out of phase per channel; a straight L+R sum would
                // then cancel to silence on a single-speaker device. A short
                // delay on R (well below the Haas echo threshold) makes the
                // summed amplitude 2|sin(pi*f*d)| > 0 at every audio
                // frequency while remaining imperceptible on true stereo
                // speakers.
                if (audio_system && audio_system->get_decorrelation()) {
                    float r_out = r_delay_buf[r_delay_idx];
                    r_delay_buf[r_delay_idx] = mixed_output[1];
                    r_delay_idx = (r_delay_idx + 1) % MONO_DECORR_DELAY;

                    audio_buffer->push_back(mixed_output[0]);
                    audio_buffer->push_back(r_out);
                } else {
                    audio_buffer->push_back(mixed_output[0]);
                    audio_buffer->push_back(mixed_output[1]);
                }

                sample_vid_ = sample_end;
            }
        }
        
        // Write audio samples to a WAV file
        bool writeToWav(const std::string& filename) {
            if (!audio_buffer) {
                return false;
            }
            
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                return false;
            }
            
            // WAV header
            file.write("RIFF", 4);                    // Chunk ID
            write32(file, 36 + audio_buffer->size() * 2);   // Chunk size (header + data)
            file.write("WAVE", 4);                    // Format
            
            // Format subchunk
            file.write("fmt ", 4);                    // Subchunk ID
            write32(file, 16);                        // Subchunk size
            write16(file, 1);                         // Audio format (1 = PCM)
            write16(file, 1);                         // Number of channels
            write32(file, static_cast<uint32_t>(OUTPUT_SAMPLE_RATE_INT));  // Sample rate
            write32(file, static_cast<uint32_t>(OUTPUT_SAMPLE_RATE_INT * 2));  // Byte rate
            write16(file, 2);                         // Block align
            write16(file, 16);                        // Bits per sample
            
            // Data subchunk
            file.write("data", 4);                    // Subchunk ID
            write32(file, audio_buffer->size() * 2);        // Subchunk size
            
            // Write audio data (convert float to 16-bit PCM)
            for (float sample : *audio_buffer) {
                // Clamp sample to [-1.0, 1.0] and convert to 16-bit PCM
                int16_t pcm_sample = static_cast<int16_t>(
                    std::max(-1.0f, std::min(1.0f, sample)) * 32767.0f
                );
                file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(pcm_sample));
            }
            
            return true;
        }
    
        // Display register values for both chips in a compact side-by-side format
        void display_registers() {
            printf("                Chip 0                |                Chip 1\n");
            printf("------------------------------------- | --------------------------------------\n");
            
            // Line 1: Tone registers
            printf("Tone A: %03X  Tone B: %03X  Tone C: %03X | Tone A: %03X  Tone B: %03X  Tone C: %03X\n",
                (chips[0].registers[A_Tone_High] << 8) | chips[0].registers[A_Tone_Low],
                (chips[0].registers[B_Tone_High] << 8) | chips[0].registers[B_Tone_Low],
                (chips[0].registers[C_Tone_High] << 8) | chips[0].registers[C_Tone_Low],
                (chips[1].registers[A_Tone_High] << 8) | chips[1].registers[A_Tone_Low],
                (chips[1].registers[B_Tone_High] << 8) | chips[1].registers[B_Tone_Low],
                (chips[1].registers[C_Tone_High] << 8) | chips[1].registers[C_Tone_Low]);
            
            // Line 3: Amplitude registers
            printf("Ampl A: %02X   Ampl B: %02X   Ampl C: %02X  | Ampl A: %02X   Ampl B: %02X    Ampl C: %02X\n",
                chips[0].registers[Ampl_A],
                chips[0].registers[Ampl_B],
                chips[0].registers[Ampl_C],
                chips[1].registers[Ampl_A],
                chips[1].registers[Ampl_B],
                chips[1].registers[Ampl_C]);
            
            // Line 2: Noise and Mixer
            printf("Noise: %02X  Mixer: %02X                  | Noise: %02X  Mixer: %02X\n",
                chips[0].registers[Noise_Period],
                chips[0].registers[Mixer_Control],
                chips[1].registers[Noise_Period],
                chips[1].registers[Mixer_Control]);
            
            // Line 4: Envelope registers
            printf("Env Period: %04X  Env Shape: %02X       | Env Period: %04X  Env Shape: %02X\n",
                (chips[0].registers[Envelope_Period_High] << 8) | chips[0].registers[Envelope_Period_Low],
                chips[0].registers[Envelope_Shape],
                (chips[1].registers[Envelope_Period_High] << 8) | chips[1].registers[Envelope_Period_Low],
                chips[1].registers[Envelope_Shape]);
            
            // Line 5: Unknown registers
            /* printf("Unknown: %02X %02X                        | Unknown: %02X %02X\n",
                chips[0].registers[Unknown_14],
                chips[0].registers[Unknown_15],
                chips[1].registers[Unknown_14],
                chips[1].registers[Unknown_15]); */
        }
    
        uint8_t read_register(int chip_index, uint8_t register_num) {
            return chips[chip_index].live_registers[register_num];
        }
    
    
    private:
        // Filter state
        filter_state filters[7] = {0.0f}; // One state per channel, and one for the mixed output

        // Emulator state
    public:
        AY3_8910 chips[2];
    
        uint64_t dbg_last_event = 0;
        uint64_t dbg_last_time = 0;

    private:
        NClock *clock_ = nullptr;
        uint64_t sample_vid_ = 0;
        uint64_t sample_accum_ = 0;
        std::deque<RegisterEvent> pending_events;
        std::vector<float>* audio_buffer;  // Pointer to external audio buffer
        AudioSystem *audio_system = nullptr;  // Shared audio settings (decorrelation, waveform, etc.)
        ay::Waveform waveform_ = ay::Waveform::Square;  // Fallback when audio_system is null
        float alpha;

        // Mono-compatibility decorrelation delay on the R output channel.
        //
        // The two AY chips run with independent tone-counter phases (as real
        // Mockingboard hardware does). When chiptune software drives both
        // chips with identical content, the two chip outputs can be close to
        // 180° out of phase on any given channel. On a single-speaker device
        // that acoustically sums L+R (e.g. Mac mini built-in speaker), the
        // two signals then cancel to silence, producing the "muted channel"
        // defect. True stereo speakers and mono-from-one-chip both avoid it.
        //
        // A short delay on R (16 samples @ 44.1kHz = ~363us) is far below the
        // Haas echo threshold (~30ms), so stereo listeners perceive no
        // difference, but it guarantees mono_sum(f) = 2|sin(pi*f*d)| > 0 at
        // every audio frequency -> no more structural cancellation.
        static constexpr size_t MONO_DECORR_DELAY = 256;
        float    r_delay_buf[MONO_DECORR_DELAY] = {0.0f};
        uint32_t r_delay_idx                    = 0;

        // Per-chip AY bus address latch (selected via "latch address" command).
        uint8_t reg_num[2];
    
    
        // Helper function to write a 16-bit value to a file
        void write16(std::ofstream& file, uint16_t value) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        
        // Helper function to write a 32-bit value to a file
        void write32(std::ofstream& file, uint32_t value) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        
    #if 0
        // Apply simple one-pole low-pass filter
        float applyLowPassFilter(float input, int filter_index) {
            filter_state& filter = filters[filter_index];
            float filtered_sample = filter.last_sample + FILTER_CUTOFF * (input - filter.last_sample);
            filter.last_sample = filtered_sample;
            return filtered_sample;
        }
    #endif
        float computeAlpha(float cutoffHz, float sampleRate) {
            float rc = 1.0f / (2.0f * M_PI * cutoffHz);
            float dt = 1.0f / sampleRate;
            float alpha = dt / (rc + dt);
            return alpha;
        }
    
        void setChannelAlpha(int chip_index, int channel) {
            int tone_period = chips[chip_index].tone_channels[channel].period;
            int tp = tone_period > 0 ? tone_period : 1;
    
            /* float cutofffreq = (63781.0f / tp) * 3;
            cutofffreq = cutofffreq < 19500.0f ? cutofffreq : 19500.0f; */
            float cutofffreq = 14000.0f;
    
            int filter_index = 1 + channel + chip_index * 3;  
            filter_state& filter = filters[filter_index];
      
            filter.alpha = computeAlpha(cutofffreq, OUTPUT_SAMPLE_RATE_INT);
            if (DEBUG(DEBUG_MOCKINGBOARD)) printf("setChannelAlpha(%d:%d): tonePeriod: %d (%d), cutoffHz: %f, sampleRate: %d, alpha: %f\n", 
                chip_index, channel, tone_period, tp, cutofffreq, OUTPUT_SAMPLE_RATE_INT, filter.alpha);
        }
    
        float applyLowPassFilter(float input, int chip_index, int channel /* , float alpha */) {
            int filter_index = 1 + channel + chip_index * 3;    
            filter_state& filter = filters[filter_index];
            filter.last_sample += filter.alpha * (input - filter.last_sample);
            return filter.last_sample;
        }
    };

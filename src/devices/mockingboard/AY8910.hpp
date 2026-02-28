#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <cmath>
#include "debug.hpp"

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

// Global constants
// must be based on 59.9227 fps rate just like Speaker code.
//constexpr double OUTPUT_SAMPLE_RATE = 44100.0;
constexpr uint32_t OUTPUT_SAMPLE_RATE_INT = 44100;
constexpr uint32_t SAMPLES_PER_FRAME_INT = 740;

// Event to represent register changes with timestamps
struct RegisterEvent {
    double timestamp;      // Time in seconds when this event occurs
    uint8_t chip_index;    // Which AY chip (0 or 1)
    uint8_t register_num;  // Which register (0-13)
    uint8_t value;         // New value for the register
    
    // For sorting events by timestamp
    bool operator<(const RegisterEvent& other) const {
        return timestamp < other.timestamp;
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

 class MockingboardEmulator {
    private:
        // Constants
        static constexpr double MASTER_CLOCK = /* 1020500.0 */ 1020484.0; // 1MHz
        static constexpr int CLOCK_DIVIDER = 16;
        static constexpr double CHIP_FREQUENCY = MASTER_CLOCK / CLOCK_DIVIDER; // 62.5kHz
        static constexpr int ENVELOPE_CLOCK_DIVIDER = 256;  // First stage divider for envelope
        static constexpr float FILTER_CUTOFF = 0.3f; // Filter coefficient (0-1) (lower is more aggressive)
        
        float get_volume(uint8_t volsetting) {
            return normalized_levels[volsetting]; /*  * 0.25f */;
        }

    public:
        MockingboardEmulator(std::vector<float>* buffer = nullptr) 
            : current_time(0.0), time_accumulator(0.0), envelope_time_accumulator(0.0), audio_buffer(buffer) {
            // Initialize chips
            for (int c = 0; c < 2; c++) {
                // Initialize registers to 0
                for (int r = 0; r < AY_8913_REGISTER_COUNT; r++) {
                    chips[c].registers[r] = 0;
                    chips[c].live_registers[r] = 0;
                }
                
                // Initialize tone channels
                for (int i = 0; i < 3; i++) {
                    chips[c].tone_channels[i].period = 0;
                    //chips[c].tone_channels[i].counter = 0; // TODO: was 1, for testing claude suggestion.
                    chips[c].tone_channels[i].counter = 1;
                    chips[c].tone_channels[i].output = false;
                    chips[c].tone_channels[i].volume = 0;
                    chips[c].tone_channels[i].use_envelope = false;
                }
                
                // Initialize noise generator
                chips[c].noise_period = 1;
                chips[c].noise_counter = 1;
                chips[c].noise_rng = 1;
                chips[c].noise_output = false;
                chips[c].mixer_control = 0;
                chips[c].envelope_period = 0;
                chips[c].envelope_shape = 0;
                chips[c].envelope_counter = 0;
                chips[c].envelope_output = 0;
                chips[c].envelope_hold = false;
                chips[c].envelope_attack = false;
                chips[c].current_envelope_level = 0.0f;
                chips[c].target_envelope_level = 0.0f;
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
            }
        }
    
        // Add a register change event
        void queueRegisterChange(double timestamp, uint8_t chip_index, uint8_t reg, uint8_t value) {
            RegisterEvent event;
            event.timestamp = timestamp;
            event.chip_index = chip_index;
            event.register_num = reg;
            event.value = value;
            
            pending_events.push_back(event);
            // Keep events sorted by timestamp
            //std::sort(pending_events.begin(), pending_events.end()); // TODO: say what now?
    
            AY3_8910& chip = chips[event.chip_index];
            chip.live_registers[event.register_num] = event.value;
    
            // for debugging, store the timestamp of event and current emulated time.
            dbg_last_event = event.timestamp;
            dbg_last_time = current_time;
    
            if (dbg_last_event < current_time) {
                printf("[Current Time: %12.6f] Event timestamp is in the past: %12.6f\n", current_time, dbg_last_event);
            }
        }
        
        // Process a register change
        void processRegisterChange(const RegisterEvent& event) {
            if (event.chip_index > 1 || event.register_num > 15) {
                return; // Invalid event
            }
            
            AY3_8910& chip = chips[event.chip_index];
            chip.registers[event.register_num] = event.value;
            
            //debug_register_change(current_time, event.chip_index, event.register_num, event.value);
            if (DEBUG(DEBUG_MOCKINGBOARD)) display_registers();
            
            // Update internal state based on register change
            switch (event.register_num) {
                case A_Tone_Low:
                case A_Tone_High:
                    chip.tone_channels[0].period = ((chip.registers[A_Tone_High] & 0x0F) << 8) | chip.registers[A_Tone_Low];
                    setChannelAlpha(event.chip_index, 0);
                    break;
                case B_Tone_Low:
                case B_Tone_High:
                    chip.tone_channels[1].period = ((chip.registers[B_Tone_High] & 0x0F) << 8) | chip.registers[B_Tone_Low];
                    setChannelAlpha(event.chip_index, 1);
                    break;
                case C_Tone_Low:
                case C_Tone_High:
                    chip.tone_channels[2].period = ((chip.registers[C_Tone_High] & 0x0F) << 8) | chip.registers[C_Tone_Low];
                    setChannelAlpha(event.chip_index, 2);
                    break;
#if 0
                case A_Tone_Low: // Tone period low bits for channel A
                case B_Tone_Low: // Tone period low bits for channel B
                case C_Tone_Low: // Tone period low bits for channel C
                    {
                        int channel = event.register_num / 2;
                        int high_bits = chip.registers[event.register_num + 1] & 0x0F;
                        // The AY-3-8910 frequency is calculated as: f = chip_frequency / (2 * period)
                        // So for a given frequency f, period = chip_frequency / (2 * f)
                        // For example, for 250Hz: period = 62500 / (2 * 250) = 125
                        chip.tone_channels[channel].period = 
                            (high_bits << 8) | event.value;
                        /* if (chip.tone_channels[channel].period == 0) {
                            chip.tone_channels[channel].period = 1; // Avoid division by zero
                        } */
                        setChannelAlpha(event.chip_index, channel);
                    }
                    break;
                    
                case A_Tone_High: // Tone period high bits for channel A
                case B_Tone_High: // Tone period high bits for channel B
                case C_Tone_High: // Tone period high bits for channel C
                    {
                        int channel = (event.register_num - 1) / 2;
                        int high_bits = event.value & 0x0F;
                        chip.tone_channels[channel].period = 
                            (high_bits << 8) | chip.registers[event.register_num - 1];
                        /* if (chip.tone_channels[channel].period == 0) {
                            chip.tone_channels[channel].period = 1; // Avoid division by zero
                        } */
                        setChannelAlpha(event.chip_index, channel);
                    }
                    break;
#endif
                case Noise_Period: // Noise period
                    chip.noise_period = event.value & 0x1F;
                    if (chip.noise_period == 0) {
                        chip.noise_period = 1; // Avoid division by zero
                    }
                    break;
                    
                case Mixer_Control: // Mixer control
                    chip.mixer_control = event.value;
                    break;
                    
                case Envelope_Period_Low: // Envelope period low bits
                    chip.envelope_period = (chip.envelope_period & 0xFF00) | event.value;
                    break;
                    
                case Envelope_Period_High: // Envelope period high bits
                    chip.envelope_period = (chip.envelope_period & 0x00FF) | (event.value << 8);
                    break;
                    
                case Envelope_Shape: // Envelope shape
                    chip.envelope_shape = event.value & 0x0F;
                    // Reset envelope state
                    chip.envelope_counter = 0;
                    chip.envelope_hold = false;
                    // Start in the correct phase based on Attack bit
                    chip.envelope_attack = (chip.envelope_shape & 0x04) != 0;
                    // Set initial output value based on attack/decay
                    chip.envelope_output = chip.envelope_attack ? 0 : 15;
                    chip.target_envelope_level = get_volume(chip.envelope_output);
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
                        //printf("%12.4f|chip %d channel %d use_envelope %d volume %d/%f\n", event.timestamp, event.chip_index, channel, chip.tone_channels[channel].use_envelope, volsetting, chip.tone_channels[channel].volume);
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
        
        // Process a single chip clock cycle
        void processChipCycle(double cycle_time) {
            // Process any pending register changes that should happen by this time
            while (!pending_events.empty() && pending_events.front().timestamp <= cycle_time) {
                processRegisterChange(pending_events.front());
                pending_events.pop_front();
            }
            
            // Process both chips
            for (int c = 0; c < 2; c++) {
                AY3_8910& chip = chips[c];
#if 1                
                // Process tone generators
                for (int i = 0; i < 3; i++) {
                    ToneChannel& channel = chip.tone_channels[i];
                    
                    if (channel.counter > 0) {
                        channel.counter--;
                        // Check for half period
                        if (channel.counter == channel.period / 2) {
                            channel.output = !channel.output;
                        }
                    } else {
                        // Reset counter and toggle output
                        channel.counter = channel.period;
                        channel.output = !channel.output;
                    }
                }
#endif            
#if 0    
                for (int i = 0; i < 3; i++) {
                    ToneChannel& ch = chip.tone_channels[i];
                    ch.counter++;
                    if (ch.counter >= ch.period/2) {
                        ch.counter = 0;
                        ch.output = !ch.output;
                    }
                }
#endif
                // Process noise generator (simplified)
                chip.noise_counter--;
                if (chip.noise_counter <= 0) {
                    chip.noise_counter = chip.noise_period;
                    
                    // Update noise RNG (simplified LFSR)
                    uint32_t bit0 = chip.noise_rng & 1;
                    uint32_t bit3 = (chip.noise_rng >> 3) & 1;
                    uint32_t new_bit = bit0 ^ bit3;
                    chip.noise_rng = (chip.noise_rng >> 1) | (new_bit << 16);
                    chip.noise_output = (chip.noise_rng & 1) != 0;  // Use LSB of RNG as noise output
                }
            }
        }
        
        // Process envelope generator at correct rate
        void processEnvelope(double time_step) {
            for (int c = 0; c < 2; c++) {
                AY3_8910& chip = chips[c];
                
                if (chip.envelope_period > 0) {
                    // Update envelope counter at the correct rate
                    chip.envelope_counter++;
                    if (chip.envelope_counter >= (chip.envelope_period / 16)) {
                        chip.envelope_counter = 0;
                        
                        // Extract control bits
                        bool hold = (chip.envelope_shape & 0x01) != 0;      // Bit 0 (inverted in hardware)
                        bool alternate = (chip.envelope_shape & 0x02) != 0; // Bit 1
                        bool attack = (chip.envelope_shape & 0x04) != 0;    // Bit 2
                        bool cont = (chip.envelope_shape & 0x08) != 0;      // Bit 3
                        
                        // State machine logic
                        if (chip.envelope_hold) {
                            // Do nothing when in hold state
                            return;
                        }
                        
                        if (chip.envelope_attack) {
                            // In attack (rising) phase
                            if (chip.envelope_output < 15) {
                                // Still rising
                                chip.envelope_output++;
                                chip.target_envelope_level = get_volume(chip.envelope_output);
                            } else {
                                // Reached peak, determine next state
                                // If continue and hold are both set, determine held value by (attack XOR alternate)
                                if (cont && hold) {
                                    bool held_at_15 = attack != alternate; // XOR operation
                                    chip.envelope_output = held_at_15 ? 15 : 0;
                                    chip.target_envelope_level = get_volume(chip.envelope_output);
                                    chip.envelope_hold = true;
                                }
                                // Regular processing for other cases
                                else if (hold) {
                                    chip.envelope_hold = true;
                                } else if (!cont) {
                                    chip.envelope_output = 0;
                                    chip.target_envelope_level = 0;
                                    chip.envelope_hold = true;
                                } else if (alternate) {
                                    chip.envelope_attack = false; // Switch to decay
                                } else {
                                    // Reset to start of phase
                                    chip.envelope_output = attack ? 0 : 15;
                                    chip.target_envelope_level = get_volume(chip.envelope_output);
                                }
                            }
                        } else {
                            // In decay (falling) phase
                            if (chip.envelope_output > 0) {
                                // Still falling
                                chip.envelope_output--;
                                chip.target_envelope_level = get_volume(chip.envelope_output);
                            } else {
                                // Reached zero, determine next state
                                // If continue and hold are both set, determine held value by (attack XOR alternate)
                                if (cont && hold) {
                                    bool held_at_15 = attack != alternate; // XOR operation
                                    chip.envelope_output = held_at_15 ? 15 : 0;
                                    chip.target_envelope_level = get_volume(chip.envelope_output);
                                    chip.envelope_hold = true;
                                }
                                // Regular processing for other cases
                                else if (hold) {
                                    chip.envelope_hold = true;
                                } else if (!cont) {
                                    chip.envelope_hold = true;
                                } else if (alternate) {
                                    chip.envelope_attack = true; // Switch to attack
                                } else {
                                    // Reset to start of phase
                                    chip.envelope_output = attack ? 0 : 15;
                                    chip.target_envelope_level = get_volume(chip.envelope_output);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Generate audio samples at 44.1kHz
        void generateSamples(int num_samples) {
            if (!audio_buffer) {
                return;  // No buffer to write to
            }
            
            // Debug output for envelope level at start of each call
            if (0 && DEBUG(DEBUG_MOCKINGBOARD)) std::cout << "[" << current_time << "] Envelope status - Level: " << static_cast<int>(chips[0].envelope_output) 
                      << " (counter: " << static_cast<int>(chips[0].envelope_counter) 
                      << "/" << static_cast<int>(chips[0].envelope_period) 
                      << ", shape: " << static_cast<int>(chips[0].envelope_shape)
                      << ", attack: " << (chips[0].envelope_attack ? "true" : "false")
                      << ", hold: " << (chips[0].envelope_hold ? "true" : "false")
                      << ")" << std::endl;
            
            const double output_time_step = 1.0f / OUTPUT_SAMPLE_RATE_INT;
            const double chip_time_step = 1.0 / CHIP_FREQUENCY;
            const double envelope_base_frequency = MASTER_CLOCK / ENVELOPE_CLOCK_DIVIDER;
            const double envelope_base_time_step = 1.0 / envelope_base_frequency;
            
            for (int i = 0; i < num_samples; i++) {
                // Track current time based on the sample index and output time step
                double sample_time = current_time + (i * output_time_step);
                time_accumulator += output_time_step;
                envelope_time_accumulator += output_time_step;
                
                // Process all chip cycles that should occur during this output sample
                while (time_accumulator >= chip_time_step) {
                    double cycle_time = sample_time - time_accumulator;
                    processChipCycle(cycle_time);
                    time_accumulator -= chip_time_step;
                }
                
                // Process envelope at the correct rate (master_clock / 256)
                while (envelope_time_accumulator >= envelope_base_time_step) {
                    processEnvelope(envelope_base_time_step);
                    envelope_time_accumulator -= envelope_base_time_step;
                }
                
                // Do envelope interpolation at audio rate. This makes changes to envelope levels transition smoothly on a per-sample basis.
                for (int c = 0; c < 2; c++) {
                    AY3_8910& chip = chips[c];
                    
                    // Base the interpolation speed on the envelope period
                    // Shorter periods need faster interpolation
                    float base_speed = 0.0003f;  // Our known good value for the test case
                    float period_factor = 1.0f;
                    if (chip.envelope_period > 0) {
                        // Scale interpolation speed inversely with period
                        // The larger the period, the slower the interpolation should be
                        period_factor = static_cast<float>(0x3000) / chip.envelope_period;  // 0x3000 is a reference period
                    }
                    float interpolation_speed = base_speed * period_factor;
                    
                    chip.current_envelope_level += (chip.target_envelope_level - chip.current_envelope_level) * interpolation_speed;
                    
                    // Update channel volumes with interpolated value
                    for (int i = 0; i < 3; i++) {
                        if (chip.tone_channels[i].use_envelope) {
                            // Keep as float, divide by 15 here to normalize to 0-1 range
                            chip.tone_channels[i].volume = chip.current_envelope_level; // target_envelope_level is already normalized to 0-1
                        }
                    }
                }
                
                // Mix output from 3 channels of each chip separately.
                float mixed_output[2] = {0.0f, 0.0f};
    
                for (int c = 0; c < 2; c++) {
    
                    const AY3_8910& chip = chips[c];
#if 0
                    for (int channel = 0; channel < 3; channel++) {
                        const ToneChannel& tone = chip.tone_channels[channel];
                        bool tone_dis  = (chip.mixer_control >> channel) & 1;       // 1 = disabled
                        bool noise_dis = (chip.mixer_control >> (channel + 3)) & 1; // 1 = disabled
                    
                        bool tone_gate  = tone.output  || tone_dis;   // disabled = forced open
                        bool noise_gate = chip.noise_output || noise_dis;
                    
                        bool gate = tone_gate && noise_gate;           // AND logic
                    
                        float channel_output = gate ? tone.volume : 0.0f;
                        mixed_output[c] += channel_output;
                    }
#endif
#if 1
                    for (int channel = 0; channel < 3; channel++) {
                        const ToneChannel& tone = chip.tone_channels[channel];
                        bool tone_enabled = !(chip.mixer_control & (1 << channel));
                        bool noise_enabled = !(chip.mixer_control & (1 << (channel + 3)));
    
                        // Only process if the channel has volume
                        // If either tone or noise is enabled for this channel
                        bool is_tone = tone_enabled && tone.period > 0 && (chip.registers[Ampl_A + channel] > 0);
                        bool is_noise = noise_enabled;
    
                        if (is_tone || is_noise) {
                            // For tone: true = +volume, false = -volume
                            float tone_contribution = tone.output ? tone.volume : 0 /* -tone.volume */;
                            
                            // For noise: true = +volume, false = -volume
                            float noise_contribution = (chip.noise_output ? tone.volume : 0 /* -tone.volume */) /* * 0.6f */;
                            float channel_output;
    
                            // If both are enabled, average them
                            if (is_tone && is_noise) {
                                channel_output = (tone_contribution + noise_contribution) /* * 0.5f */;
                            } else if (is_tone) {
                                channel_output = tone_contribution;
                            } else if (is_noise) {
                                channel_output = noise_contribution;
                            }
                            //channel_output = applyLowPassFilter(channel_output, c, channel);
                            mixed_output[c] += channel_output;
                        }
                    }
#endif
                    
                }
                // TODO: add checks here to detect overdriving/exceeding -1.0/1.0. 
                // Append the mixed samples to the buffer
                audio_buffer->push_back(mixed_output[0]);
                audio_buffer->push_back(mixed_output[1]);
            }
            
            // Update current_time for the next call
            current_time += num_samples * output_time_step;
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
        
        // State for each tone channel (3 per chip, 2 chips)
        struct ToneChannel {
            uint16_t period;     // Tone period (12 bits, 0-4095)
            uint16_t counter;    // Current counter value
            bool output;         // Current output state
            float volume;        // Volume (0-1)
            bool use_envelope;   // Whether to use envelope generator
        };
        
        // State for each chip
        struct AY3_8910 {
            ToneChannel tone_channels[3];
            uint16_t noise_period;
            uint16_t noise_counter;
            uint32_t noise_rng;
            bool noise_output;    // Add noise output state
            uint8_t mixer_control;
            uint16_t envelope_period;  // Changed to 16-bit
            uint8_t envelope_shape;
            uint16_t envelope_counter;  // Changed to 16-bit to handle larger counts at master clock rate
            uint8_t envelope_output;   // Current envelope value (0-15) integer.
            bool envelope_hold;        // Whether envelope is holding
            bool envelope_attack;      // Whether envelope is in attack phase
            float current_envelope_level;  // Interpolated envelope level
            float target_envelope_level;   // Target level we're interpolating towards
            uint8_t registers[AY_8913_REGISTER_COUNT];  // register states during playback
            uint8_t live_registers[AY_8913_REGISTER_COUNT];  // Live register values as the 6502 sees them
        };
        
        // Emulator state
    public:
        AY3_8910 chips[2];
    
        double dbg_last_event =0.0f;
        double dbg_last_time =0.0f;
    
    private:
        double current_time;
        double time_accumulator;
        double envelope_time_accumulator;  // New accumulator for envelope timing
        std::deque<RegisterEvent> pending_events;
        std::vector<float>* audio_buffer;  // Pointer to external audio buffer
        float alpha;
    
    
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

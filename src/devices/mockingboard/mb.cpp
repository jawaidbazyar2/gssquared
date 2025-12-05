/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_audio.h>
#include <cmath>

#include "gs2.hpp"
#include "cpu.hpp"
#include "mb.hpp"
#include "devices/speaker/speaker.hpp"
#include "debug.hpp"
#include "util/EventTimer.hpp"
#include "util/DebugFormatter.hpp"

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


struct filter_state {
    float last_sample;
    float alpha;
};

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
class MockingboardEmulator {
private:
    // Constants
    static constexpr double MASTER_CLOCK = /* 1020500.0 */ 1020484.0; // 1MHz
    static constexpr int CLOCK_DIVIDER = 16;
    static constexpr double CHIP_FREQUENCY = MASTER_CLOCK / CLOCK_DIVIDER; // 62.5kHz
    static constexpr int ENVELOPE_CLOCK_DIVIDER = 256;  // First stage divider for envelope
    static constexpr float FILTER_CUTOFF = 0.3f; // Filter coefficient (0-1) (lower is more aggressive)
    
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
                chip.target_envelope_level = normalized_levels[chip.envelope_output];
                break;
                
            case Ampl_A: // Channel A volume
            case Ampl_B: // Channel B volume
            case Ampl_C: // Channel C volume
                {
                    int channel = event.register_num - Ampl_A;
                    // Check if envelope is enabled (bit 4)
                    if (event.value & 0x10) {
                        chip.tone_channels[channel].use_envelope = true;
                        // Set initial volume from current envelope output, normalized to 0-1
                        chip.tone_channels[channel].volume = normalized_levels[chip.envelope_output];
                    } else {
                        chip.tone_channels[channel].use_envelope = false;
                        // Convert direct volume to 0-1 range
                        chip.tone_channels[channel].volume = normalized_levels[event.value & 0x0F];
                    }
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
                            chip.target_envelope_level = normalized_levels[chip.envelope_output];
                        } else {
                            // Reached peak, determine next state
                            // If continue and hold are both set, determine held value by (attack XOR alternate)
                            if (cont && hold) {
                                bool held_at_15 = attack != alternate; // XOR operation
                                chip.envelope_output = held_at_15 ? 15 : 0;
                                chip.target_envelope_level = normalized_levels[chip.envelope_output];
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
                                chip.target_envelope_level = normalized_levels[chip.envelope_output];
                            }
                        }
                    } else {
                        // In decay (falling) phase
                        if (chip.envelope_output > 0) {
                            // Still falling
                            chip.envelope_output--;
                            chip.target_envelope_level = normalized_levels[chip.envelope_output];
                        } else {
                            // Reached zero, determine next state
                            // If continue and hold are both set, determine held value by (attack XOR alternate)
                            if (cont && hold) {
                                bool held_at_15 = attack != alternate; // XOR operation
                                chip.envelope_output = held_at_15 ? 15 : 0;
                                chip.target_envelope_level = normalized_levels[chip.envelope_output];
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
                                chip.target_envelope_level = normalized_levels[chip.envelope_output];
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
            int active_channels[2] = {0, 0};

            for (int c = 0; c < 2; c++) {

                const AY3_8910& chip = chips[c];
                
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
                        float tone_contribution = tone.output ? tone.volume : -tone.volume;
                        
                        // For noise: true = +volume, false = -volume
                        float noise_contribution = (chip.noise_output ? tone.volume : -tone.volume) * 0.6f;
                        float channel_output;

                        // If both are enabled, average them
                        if (is_tone && is_noise) {
                            channel_output = (tone_contribution + noise_contribution) /* * 0.5f */;
                        } else if (is_tone) {
                            channel_output = tone_contribution;
                        } else if (is_noise) {
                            channel_output = noise_contribution;
                        }
                        channel_output = applyLowPassFilter(channel_output, c, channel);
                        mixed_output[c] += channel_output;
                        active_channels[c]++;
                    }
                }

                // Scale by number of active channels
                /* if (active_channels[c] > 0) {
                    mixed_output[c] /= active_channels[c];
                } */
                
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

        int filter_index = 1 + channel + chip_index * 2;  
        filter_state& filter = filters[filter_index];
  
        filter.alpha = computeAlpha(cutofffreq, OUTPUT_SAMPLE_RATE_INT);
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("setChannelAlpha(%d:%d): tonePeriod: %d (%d), cutoffHz: %f, sampleRate: %d, alpha: %f\n", 
            chip_index, channel, tone_period, tp, cutofffreq, OUTPUT_SAMPLE_RATE_INT, filter.alpha);
    }

    float applyLowPassFilter(float input, int chip_index, int channel /* , float alpha */) {
        int filter_index = 1 + channel + chip_index * 2;    
        filter_state& filter = filters[filter_index];
        filter.last_sample += filter.alpha * (input - filter.last_sample);
        return filter.last_sample;
    }
};

#ifdef STANDALONE
int main() {
    // Create audio buffer
    std::vector<float> audio_buffer;
    SDL_Event event;

    SDL_Init(SDL_INIT_AUDIO);
    int device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (device_id == 0) {
        printf("Couldn't open audio device: %s", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec spec = {
        .freq = 44100,
        .format = SDL_AUDIO_F32LE,
        .channels = 1
    };

    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, NULL);
    if (!stream) {
        printf("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(device_id, stream)) {  /* once bound, it'll start playing when there is data available! */
        printf("Failed to bind stream to device: %s", SDL_GetError());
    }

    // Create mockingboard with injected buffer
    MockingboardEmulator mockingboard(&audio_buffer);
    
    // Example: Queue some register changes to play a tone on chip 0, channel A
    // For 250Hz: period = 62500 / (2 * 250) = 125
    mockingboard.queueRegisterChange(0.0, 0, A_Tone_Low, 34);  // Low byte
    mockingboard.queueRegisterChange(0.0, 0, A_Tone_High, 1);   // High byte
    
    // Set up envelope generator
    // Shape 6: Continue=1, Attack=1, Alternate=0, Hold=1 (0b0110)
    mockingboard.queueRegisterChange(0.0, 0, Envelope_Period_Low, 0x00);  // Envelope period low
    mockingboard.queueRegisterChange(0.0, 0, Envelope_Period_High, 0x10);  // Envelope period high (0x1000 = 4096)
    mockingboard.queueRegisterChange(0.0, 0, Envelope_Shape, 0x06);  // Envelope shape 6
  
    // Enable envelope on channel A (bit 4 = 1)
    mockingboard.queueRegisterChange(0.0, 0, Ampl_A, 0x10);   // Channel A volume with envelope

    mockingboard.queueRegisterChange(0.0, 0, Mixer_Control, 0xF8); // Mixer control (bit 0 = 0 enables tone A)
    mockingboard.queueRegisterChange(0.0, 0, Ampl_B, 0x10);   // Channel B volume with envelope
    mockingboard.queueRegisterChange(0.0, 0, Ampl_C, 0x10);   // Channel B volume with envelope

#if 0
    // Enable tone on channel A
    mockingboard.queueRegisterChange(0.0, 0, Mixer_Control, 0xFE); // Mixer control (bit 0 = 0 enables tone A)
       //0b1111_1010

    mockingboard.queueRegisterChange(2.0, 0, C_Tone_Low, 244);  // Low byte
    mockingboard.queueRegisterChange(2.0, 0, C_Tone_High, 0);   // High byte
    mockingboard.queueRegisterChange(2.0, 0, Ampl_C, 0x10);    // Set channel C volume w envelope

    mockingboard.queueRegisterChange(4.0, 0, Mixer_Control, 0b11111010); // Mixer control (bit 0 = 0 enables tone A)

    mockingboard.queueRegisterChange(6.0, 0, Mixer_Control, 0b11111011); // Mixer control (bit 0 = 0 enables tone A)

    // Change the frequency after 4 seconds to 312.5Hz (period = 100)
    mockingboard.queueRegisterChange(4.0, 0, A_Tone_Low, 130);  // Lower period = higher frequency
    
    // Change the frequency after 5 seconds to 156.25Hz (period = 200)
    mockingboard.queueRegisterChange(5.0, 0, A_Tone_Low, 255);  // Higher period = lower frequency
    
    // Change the frequency after 6 seconds to 78.125Hz (period = 400)
    mockingboard.queueRegisterChange(6.0, 0, A_Tone_Low, 65);  // Low byte of 400
    mockingboard.queueRegisterChange(6.0, 0, A_Tone_High, 0);    // High byte of 400 (0x190)
    
    // Add noise on channel B for 1 second
    mockingboard.queueRegisterChange(7.0, 0, Noise_Period, 0x1F);  // Set noise period to maximum (31)
    mockingboard.queueRegisterChange(7.0, 0, Ampl_B, 15);    // Set channel B volume to max
    mockingboard.queueRegisterChange(7.0, 0, Mixer_Control, 0xE6);  // Enable noise on channel B (bits 3 and 1 = 0)
    mockingboard.queueRegisterChange(8.0, 0, Mixer_Control, 0xFA);  // Disable noise after 1 second, keep A going
    mockingboard.queueRegisterChange(8.0, 0, Ampl_B, 0);     // Set channel B volume to 0
#endif

    float iter = 1.0;
    for (int i = 65; i <= 1800; i *= 1.1) {

        uint8_t low = i & 0xFF;
        uint8_t hi = (i >> 8) & 0xFF;

        mockingboard.queueRegisterChange(iter, 0, A_Tone_Low, low);
        mockingboard.queueRegisterChange(iter, 0, A_Tone_High, hi);

        int fifth = i * 1.5;
        uint8_t low2 = fifth & 0xFF;
        uint8_t hi2 = (fifth >> 8) & 0xFF;

        mockingboard.queueRegisterChange(iter + 0.25, 0, B_Tone_Low, low2);
        mockingboard.queueRegisterChange(iter + 0.25, 0, B_Tone_High, hi2);

        int seventh = i * 1.5 * 1.2;
        uint8_t low3 = seventh & 0xFF;
        uint8_t hi3 = (seventh >> 8) & 0xFF;

        mockingboard.queueRegisterChange(iter + 0.5, 0, C_Tone_Low, low3);
        mockingboard.queueRegisterChange(iter + 0.5, 0, C_Tone_High, hi3);


        iter += 0.5;
    }

#define FRAME_TIME 16666666
#define PLAY_TIME 20000

    // Generate 6 seconds of audio (at 44.1kHz)
    const int samples_per_frame = static_cast<int>(735);  // 16.67ms worth of samples
    const int total_frames = PLAY_TIME / 16.67;  // 6 seconds / 16.67ms
    uint64_t start_time = SDL_GetTicksNS();
    uint64_t processing_time = 0;
    uint64_t avg_loop_time = 0;
    for (int i = 0; i < total_frames; i++) {
        // Process SDL events
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                // Exit the loop if the user closes the window
                i = total_frames;
                break;
            }
        }
        uint64_t frame_start = SDL_GetTicksNS();
        mockingboard.generateSamples(samples_per_frame);
        // Clear the audio buffer after each frame to prevent memory buildup
        // Send the generated audio data to the SDL audio stream
        if (audio_buffer.size() > 0) {
            SDL_PutAudioStreamData(stream, audio_buffer.data(), audio_buffer.size() * sizeof(float));
        }
        audio_buffer.clear();
        uint64_t frame_end = SDL_GetTicksNS();
        processing_time += frame_end - frame_start;

        avg_loop_time += frame_end - start_time;
        printf("frame %6d (st-end: %12lld)\r", i, avg_loop_time/i);
        fflush(stdout);
        if (frame_end - start_time < FRAME_TIME) {
            SDL_DelayNS(FRAME_TIME-(frame_end - start_time));
        }
        start_time += FRAME_TIME;
        
    }
    auto end_time = SDL_GetTicksNS();
    std::cout << "Average Frame Processing Time: " << processing_time / total_frames << " ns" << std::endl;
#if 0  
    // Write audio to WAV file
    if (mockingboard.writeToWav("mockingboard_output.wav")) {
        std::cout << "Successfully wrote audio to mockingboard_output.wav" << std::endl;
    } else {
        std::cout << "Failed to write audio file" << std::endl;
    }
#endif
while (1) {
     SDL_PollEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            // Exit the loop if the user closes the window
            break;
        }
}
    return 0;
}
#endif

void mb_6522_propagate_interrupt(mb_cpu_data *mb_d) {
    // for each chip, calculate the IFR bit 7.
    for (int i = 0; i < 2; i++) {
        mb_6522_regs *tc = &mb_d->d_6522[i];
        uint8_t irq = ((tc->ifr.value & tc->ier.value) & 0x7F) > 0;
        // set bit 7 of IFR to the result.
        tc->ifr.bits.irq = irq;
    }
    bool irq_to_slot = (mb_d->d_6522[0].ifr.value & mb_d->d_6522[0].ier.value & 0x7F) || (mb_d->d_6522[1].ifr.value & mb_d->d_6522[1].ier.value & 0x7F);
    //printf("irq_to_slot: %d %d\n", mb_d->slot, irq_to_slot);
    mb_d->computer->set_slot_irq(mb_d->slot, irq_to_slot);
}

void mb_t1_timer_callback(uint64_t instanceID, void *user_data) {
    mb_cpu_data *mb_d = (mb_cpu_data *)user_data;
    cpu_state *cpu = mb_d->computer->cpu;

    uint8_t slot = (instanceID & 0x0F00) >> 8;
    uint8_t chip = (instanceID & 0x000F);

    if (DEBUG(DEBUG_MOCKINGBOARD)) std::cout << "MB 6522 Timer callback " << slot << " " << chip << std::endl;
 
    // there are two chips; track each IRQ individually and update card IRQ line from that.
    mb_6522_regs *tc = &mb_d->d_6522[chip];
    
    if (tc->acr & 0x40) { // continuous mode
        mb_d->d_6522[chip].ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
        mb_6522_propagate_interrupt(mb_d);

        tc->t1_counter = tc->t1_latch;
        uint32_t next_counter = tc->t1_counter;
        if (next_counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
            next_counter = 65536;
        }
        tc->t1_triggered_cycles += next_counter; // TODO: testing.
        mb_d->event_timer->scheduleEvent(cpu->cycles + next_counter, mb_t1_timer_callback, instanceID , mb_d);
    } else {         // one-shot mode
        // if a T1 oneshot was pending, set interrupt status.
        if (tc->t1_oneshot_pending) {
            mb_d->d_6522[chip].ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
            mb_6522_propagate_interrupt(mb_d);
        }
        // We don't schedule a next interrupt - that is only done when writing to T1C-H.
        // and we don't reset the counter to the latch, we continue decrementing from 0.

        tc->t1_oneshot_pending = 0;
    }
}

// TODO: don't reschedule if interrupts are disabled.
void mb_t2_timer_callback(uint64_t instanceID, void *user_data) {
    mb_cpu_data *mb_d = (mb_cpu_data *)user_data;
    cpu_state *cpu = mb_d->computer->cpu;
    uint8_t slot = (instanceID & 0x0F00) >> 8;
    uint8_t chip = (instanceID & 0x000F);

    if (DEBUG(DEBUG_MOCKINGBOARD)) std::cout << "MB 6522 Timer callback " << slot << " " << chip << std::endl;


    // TODO: there are two chips; track each IRQ individually and update card IRQ line from that.
    mb_6522_regs *tc = &mb_d->d_6522[chip];
    
    if (0) {
        tc->t2_counter = tc->t2_latch;
        
        uint32_t counter = tc->t2_counter;
        if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
            counter = 65536;
        }
        
        mb_d->event_timer->scheduleEvent(cpu->cycles + counter, mb_t2_timer_callback, instanceID , mb_d);
    }
    if (1) { // one-shot mode
        // TODO: "after timing out, the counter will continue to decrement."
        // so do NOT reset the counter to the latch.
        // processor must rewrite T2C-H to enable setting of the interrupt flag.
        if (tc->t2_oneshot_pending) {
            mb_d->d_6522[chip].ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
            mb_6522_propagate_interrupt(mb_d);
        }
        tc->t2_oneshot_pending = 0;
    }
}

void mb_write_Cx00(void *context, uint32_t addr, uint8_t data) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    uint8_t slot = (addr & 0x0F00) >> 8;
    uint8_t alow = addr & 0x7F;
    uint8_t chip = (addr & 0x80) ? 0 : 1;

    if (DEBUG(DEBUG_MOCKINGBOARD)) printf("mb_write_Cx00: %02d %d %02x %02x\n", slot, chip, alow, data);
    mb_6522_regs *tc = &mb_d->d_6522[chip]; // which 6522 chip this is.
    uint64_t cpu_cycles = mb_d->computer->cpu->cycles;
    uint64_t cpu_14m_cycles = mb_d->computer->cpu->c_14M;

    switch (alow) {

        case MB_6522_DDRA:
            tc->ddra = data;
            break;
        case MB_6522_DDRB:
            tc->ddrb = data;
            break;
        case MB_6522_ORA: case MB_6522_ORA_NH: // TODO: what the heck is NH here??
            // TODO: need to mask with DDRA
            tc->ora = data;
            break;
        case MB_6522_ORB:
            // TODO: instead of having this here, we could put this logic into the AY/Mockingboard code.
            tc->orb = data;
            if ((data & 0b100) == 0) { // /RESET is low, hence assert reset, reset the chip.
                // reset the chip. Set all 16 registers to 0.
                for (int i = 0; i < 16; i++) {
                    // TODO: is there a better way to get this?
                    //double time = cpu_cycles / 1020500.0;
                    double time = (double)cpu_14m_cycles / mb_d->c14m_rate;
                    mb_d->mockingboard->queueRegisterChange(time, chip, i, 0);
                }
            } else if ((data & 0b111) == 7) { // this is the register number.
                tc->reg_num = tc->ora;
                if (DEBUG(DEBUG_MOCKINGBOARD)) printf("reg_num: %02x\n", tc->reg_num);
            } else if ((data & 0b111) == 6) { // write to the specified register
                // TODO: need to mask with ddrb
                //double time = cpu_cycles / 1020500.0;
                double time = (double)cpu_14m_cycles / mb_d->c14m_rate;
                mb_d->mockingboard->queueRegisterChange(time, chip, tc->reg_num, tc->ora);
                if (DEBUG(DEBUG_MOCKINGBOARD)) printf("queueRegisterChange: [%lf] chip: %d reg: %02x val: %02x\n", time, chip, tc->reg_num, tc->ora);
            } else if ((data & 0b111) == 5) { // read from the specified register
                // to read, the command is run through ORB, but the data goes back and forth on ORA/IRA.
                //printf("attempt to read with cmd 05 from PSG, unimplemented\n");
                tc->ira = mb_d->mockingboard->read_register(chip, tc->reg_num);
                printf("read register: %02X -> %02X\n", tc->reg_num, tc->ira);
            }
            break;
        case MB_6522_T1L_L: 
            /* 8 bits loaded into T1 low-order latches. This operation is no different than a write into REG 4 (2-42) */
            tc->t1_latch = (tc->t1_latch & 0xFF00) | data;
            break;
        case MB_6522_T1L_H:
            /* 8 bits loaded into T1 high-order latches. Unlike REG 4 OPERATION, no latch-to-counter transfers take place (2-42) */
            tc->t1_latch = (tc->t1_latch & 0x00FF) | (data << 8);
            break;
        case MB_6522_T1C_L:
            /* 8 bits loaded into T1 low-order latches. Transferred into low-order counter at the time the high-order counter is loaded (reg 5) */
            tc->t1_latch = (tc->t1_latch & 0xFF00) | data;
            break;
        case MB_6522_T1C_H:
            {
            /* 8 bits loaded into T1 high-order latch. Also both high-and-low order latches transferred into T1 Counter. T1 Interrupt flag is also reset (2-42) */
            // write of t1 counter high clears the interrupt.
            mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            mb_6522_propagate_interrupt(mb_d);
            tc->t1_latch = (tc->t1_latch & 0x00FF) | (data << 8);
            tc->t1_counter = tc->t1_latch /* ? tc->t1_latch : 65535 */;
            uint32_t next_counter = tc->t1_counter ? tc->t1_counter : 65536;
            tc->ifr.bits.timer1 = 0;
            tc->t1_triggered_cycles = cpu_cycles + next_counter + 1; // TODO: testing. this is icky. This might be 6502 cycle timing plus 6522 counter timing.
            tc->t1_oneshot_pending = 1;
            //if (tc->ier.bits.timer1) {
                mb_d->event_timer->scheduleEvent(tc->t1_triggered_cycles, mb_t1_timer_callback, 0x10000000 | (slot << 8) | chip , mb_d);
            /* } else {
                mb_d->event_timer->cancelEvents(0x10000000 | (slot << 8) | chip);
            } */
            }
            break;
        // TODO: T2C_L and T2C_H are not implemented.
        case MB_6522_T2C_L:
            tc->t2_latch = (tc->t2_latch & 0xFF00) | data;
            break;
        case MB_6522_T2C_H: {
            tc->t2_latch = (tc->t2_latch & 0x00FF) | (data << 8);
            tc->t2_counter = tc->t2_latch /* ? tc->t2_latch : 65535 */;
            uint32_t next_counter2 = tc->t2_counter ? tc->t2_counter : 65536;
            tc->ifr.bits.timer2 = 0;
            mb_6522_propagate_interrupt(mb_d);
            tc->t2_triggered_cycles = cpu_cycles;
            tc->t2_oneshot_pending = 1;
            //if (tc->ier.bits.timer2) {
                mb_d->event_timer->scheduleEvent(cpu_cycles + next_counter2, mb_t2_timer_callback, 0x10010000 | (slot << 8) | chip , mb_d);
            /* } else {
                mb_d->event_timer->cancelEvents(0x10010000 | (slot << 8) | chip);
            } */
            }
            break;

        case MB_6522_PCR:
            tc->pcr = data;
            break;
        case MB_6522_SR:
            tc->sr = data;
            break;
        case MB_6522_ACR:
            tc->acr = data;
            break;
        case MB_6522_IFR:
            {
                uint8_t wdata = data & 0x7F;
                // for any bit set in wdata, clear the corresponding bit in the IFR.
                // Pg 2-49 6522 Data Sheet
                tc->ifr.value &= ~wdata;
                mb_6522_propagate_interrupt(mb_d);
            }
            break;
        case MB_6522_IER:
            {
                // if bit 7 is a 0, then each 1 in bits 0-6 clears the corresponding bit in the IER
                // if bit 7 is a 1, then each 1 in bits 0-6 enables the corresponding interrupt.
                // Pg 2-49 6522 Data Sheet
                if (data & 0x80) {
                    tc->ier.value |= data & 0x7F;
                } else {
                    tc->ier.value &= ~data;
                }
                mb_6522_propagate_interrupt(mb_d);
                uint64_t instanceID = 0x10000000 | (slot << 8) | chip;
                // if timer1 interrupt is disabled, cancel any pending events. (Only enable + write to T1C will reschedule)
                /* if (!tc->ier.bits.timer1) {
                    mb_d->event_timer->cancelEvents(instanceID);
                } else */ 
                // if we set the counter/latch BEFORE we enable interrupts.
                uint64_t cycle_base = tc->t1_triggered_cycles == 0 ? mb_d->computer->cpu->cycles : tc->t1_triggered_cycles;
                uint32_t counter = tc->t1_counter;
                if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
                    counter = 65536;
                }
                mb_d->event_timer->scheduleEvent(cycle_base + counter, mb_t1_timer_callback, instanceID , mb_d);
                                
                instanceID = 0x10010000 | (slot << 8) | chip;
                /* if (!tc->ier.bits.timer2) {
                    mb_d->event_timer->cancelEvents(instanceID);
                } else */ { // if we set the counter/latch BEFORE we enable interrupts.
                    uint64_t cycle_base = tc->t2_triggered_cycles == 0 ? mb_d->computer->cpu->cycles : tc->t2_triggered_cycles;
                    uint32_t counter = tc->t2_counter;
                    if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
                        counter = 65536;
                    }
                    mb_d->event_timer->scheduleEvent(cycle_base + counter, mb_t2_timer_callback, instanceID , mb_d);
                }
            }
            break;
    }
}

inline uint64_t calc_cycle_diff_t1(mb_6522_regs *tc, uint64_t cycles) {
    // treat latch of 0 as 65535.
    uint32_t latchval = tc->t1_latch;
    if (latchval == 0) {
        latchval = 65536;
    }
    // we have to handle the two modes - one-shot (which will continue counting from 0 to FFFF, FFFE, etc.) and continuous 
    // which will reset the counter to the latch value.
#if 0
        return (latchval - ((cycles - tc->t1_triggered_cycles) % latchval)) & 0xFFFF;
#else
    if ((tc->acr & 0x40) == 0) {
        return (tc->t1_triggered_cycles - cycles) & 0xFFFF;
    } else {
        if (cycles < tc->t1_triggered_cycles) {
            return (tc->t1_triggered_cycles- cycles) & 0xFFFF;
        } else { // in continuous mode, use modulus of the latch value.
            return (latchval - ((cycles - tc->t1_triggered_cycles) % latchval)) & 0xFFFF;
        }
    }
#endif
    //    return latchval - ((cycles - tc->t1_triggered_cycles) % latchval);
}

inline uint64_t calc_cycle_diff_t2(mb_6522_regs *tc, uint64_t cycles) {
    // treat latch of 0 as 65535.
    uint32_t latchval = tc->t2_latch;
    if (latchval == 0) {
        latchval = 65536;
    }
    return latchval - ((cycles - tc->t2_triggered_cycles) % latchval);
}

uint8_t mb_read_Cx00(void *context, uint32_t addr) {
    mb_cpu_data *mb_d = (mb_cpu_data *)context;
    cpu_state *cpu = mb_d->computer->cpu;
    uint8_t slot = (addr & 0x0F00) >> 8;
    uint8_t alow = addr & 0x7F;
    uint8_t chip = (addr & 0x80) ? 0 : 1;

    if (DEBUG(DEBUG_MOCKINGBOARD)) printf("mb_read_Cx00: %02x => ", alow);
    uint8_t retval = 0xFF;
    switch (alow) {
        case MB_6522_DDRA:
            retval = mb_d->d_6522[chip].ddra;
            break;
        case MB_6522_DDRB:
            retval = mb_d->d_6522[chip].ddrb;
            break;
        case MB_6522_ORA: case MB_6522_ORA_NH: { // TODO: what the heck is NH here??
            uint8_t a_in = mb_d->d_6522[chip].ira & (~mb_d->d_6522[chip].ddra);
            uint8_t a_out = mb_d->d_6522[chip].ora & mb_d->d_6522[chip].ddra;
            retval = a_out | a_in;
            } 
            //retval = mb_d->d_6522[chip].ora;
            break;
        case MB_6522_ORB: {
            uint8_t b_in = mb_d->d_6522[chip].orb & (~mb_d->d_6522[chip].ddrb);
            uint8_t b_out = mb_d->d_6522[chip].orb & mb_d->d_6522[chip].ddrb;
            retval = b_out | b_in;
            } 
        //retval = mb_d->d_6522[chip].orb;
            break;
        case MB_6522_T1L_L:
            /* 8 bits from T1 low order latch transferred to mpu. unlike read T1 low counter, does not cause reset of T1 IFR6. */
            retval = mb_d->d_6522[chip].t1_latch & 0xFF;
            break;
        case MB_6522_T1L_H:     
            /* 8 bits from t1 high order latch transferred to mpu */
            retval = (mb_d->d_6522[chip].t1_latch >> 8) & 0xFF;
            break;
        case MB_6522_T1C_L:  {  // IFR Timer 1 flag cleared by read T1 counter low. pg 2-42
            mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            mb_6522_propagate_interrupt(mb_d);
            uint64_t cycle_diff = calc_cycle_diff_t1(&mb_d->d_6522[chip], cpu->cycles);
            //uint64_t cycle_diff = mb_d->d_6522[chip].t1_latch - ((cpu->cycles - mb_d->d_6522[chip].t1_triggered_cycles) % mb_d->d_6522[chip].t1_latch);
            retval = cycle_diff & 0xFF;
            break;
        }
        case MB_6522_T1C_H:    {  // IFR Timer 1 flag cleared by read T1 counter high. pg 2-42
            // read of t1 counter high DOES NOT clear interrupt; write does.
            //mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            //mb_6522_propagate_interrupt(cpu, mb_d);
            uint64_t cycle_diff = calc_cycle_diff_t1(&mb_d->d_6522[chip], cpu->cycles);
            //uint64_t cycle_diff = mb_d->d_6522[chip].t1_latch - ((cpu->cycles - mb_d->d_6522[chip].t1_triggered_cycles) % mb_d->d_6522[chip].t1_latch);
            retval = (cycle_diff >> 8) & 0xFF;
            break;
        }
        case MB_6522_T2C_L: { /* 8 bits from T2 low order counter transferred to mpu - t2 interrupt flag is reset. */
            //uint64_t cycle_diff = mb_d->d_6522[chip].t2_latch - ((cpu->cycles - mb_d->d_6522[chip].t2_triggered_cycles) % mb_d->d_6522[chip].t2_latch);
            mb_d->d_6522[chip].ifr.bits.timer2 = 0;
            mb_6522_propagate_interrupt(mb_d);

            uint64_t cycle_diff = calc_cycle_diff_t2(&mb_d->d_6522[chip], cpu->cycles);
            retval = (cycle_diff) & 0xFF;
            break;
        }
        case MB_6522_T2C_H: { /* 8 bits from T2 high order counter transferred to mpu */
            //uint64_t cycle_diff = mb_d->d_6522[chip].t2_latch - ((cpu->cycles - mb_d->d_6522[chip].t2_triggered_cycles) % mb_d->d_6522[chip].t2_latch);
            uint64_t cycle_diff = calc_cycle_diff_t2(&mb_d->d_6522[chip], cpu->cycles);
            retval = (cycle_diff >> 8) & 0xFF;            
            break;
        }
        case MB_6522_PCR:
            retval = mb_d->d_6522[chip].pcr;
            break;
        case MB_6522_SR:
            retval = mb_d->d_6522[chip].sr;
            break;
        case MB_6522_ACR:
            retval = mb_d->d_6522[chip].acr;
            break;
        case MB_6522_IFR:
            retval = mb_d->d_6522[chip].ifr.value;
            break;
        case MB_6522_IER:
            // if a read of this register is done, bit 7 will be "1" and all other bits will reflect their enable/disable state.
            retval = mb_d->d_6522[chip].ier.value | 0x80;
            break;
    }
    if (DEBUG(DEBUG_MOCKINGBOARD)) printf("%02x\n", retval);
    return retval;
}

void generate_mockingboard_frame(mb_cpu_data *mb_d) {
    static int frames = 0;

    mb_d->samples_accumulated += mb_d->samples_per_frame_remainder;
    uint32_t samples_this_frame = mb_d->samples_per_frame_int;
    if (mb_d->samples_accumulated >= 1.0f) {
        samples_this_frame = mb_d->samples_per_frame_int+1;
        mb_d->samples_accumulated -= 1.0f;
    }

    // TODO: We need to calculate number of samples based on cycles. (Does the buffer management below handle this, or is this for some other reason?)
    //int samples_per_frame = 735;
    //int samples_per_frame = SAMPLES_PER_FRAME_INT;

    /* if (mb_d->stream) {
        int samples_in_buffer = SDL_GetAudioStreamAvailable(mb_d->stream) / sizeof(float);
        if (samples_in_buffer < 1000) {
            //samples_per_frame = 736;
            samples_per_frame ++;
        } else if (samples_in_buffer > 2000) {
            //samples_per_frame = 734;
            samples_per_frame --;
        } else {
            //samples_per_frame = 735; no change
        }
    } */

    //uint64_t cycle_diff = mb_d->computer->cpu->cycles - mb_d->last_cycle;
    ////const int samples_per_frame = ((cycle_diff * 44100) / 1020500.0);

    mb_d->last_cycle = mb_d->computer->cpu->cycles;

    mb_d->mockingboard->generateSamples(samples_this_frame);

    // Clear the audio buffer after each frame to prevent memory buildup
    // Send the generated audio data to the SDL audio stream
    int abs = mb_d->audio_buffer.size();
    if (abs > 0) {
        //printf("generate_mockingboard_frame: %zu\n", mb_d->audio_buffer.size());
        SDL_PutAudioStreamData(mb_d->stream, mb_d->audio_buffer.data(), mb_d->audio_buffer.size() * sizeof(float));
    }
    mb_d->audio_buffer.clear();

    if (DEBUG(DEBUG_MOCKINGBOARD)) {
        if (frames++ > 60) {
            frames = 0;
            // Get the number of samples in SDL audio stream buffer
            int samples_in_buffer = 0;
            if (mb_d->stream) {
                samples_in_buffer = SDL_GetAudioStreamAvailable(mb_d->stream) / sizeof(float);
            }
            printf("MB Status: buffer: %d, audio buffer size: %d, samples_per_frame: %d\n", samples_in_buffer, abs, samples_this_frame);
        }
    }
}

void insert_empty_mockingboard_frame(mb_cpu_data *mb_d) {
    const float empty_frame[736*2] = {0.0f};
    SDL_PutAudioStreamData(mb_d->stream, empty_frame, 736 * 2 * sizeof(float));
}

void mb_reset(mb_cpu_data *mb_d) {
    if (mb_d == nullptr) return;
    mb_d->mockingboard->reset();
    for (int i = 0; i < 2; i++) {
        // counters keep going as-is on reset, but no interrupts
        //mb_d->d_6522[i].t1_counter = 0;
        //mb_d->d_6522[i].t2_counter = 0;
        //mb_d->d_6522[i].t1_triggered_cycles = 0;
        //mb_d->d_6522[i].t2_triggered_cycles = 0;
        //mb_d->d_6522[i].t1_latch = 0;
        //mb_d->d_6522[i].t2_latch = 0;
        mb_d->d_6522[i].acr = 0;
        mb_d->d_6522[i].ifr.value = 0;
        mb_d->d_6522[i].ier.value = 0;
    }   
    mb_6522_propagate_interrupt(mb_d); // this reads the slot number and does the right IRQ thing.    
}


DebugFormatter *debug_registers_6522(mb_cpu_data *mb_d) {
    DebugFormatter *df = new DebugFormatter();
    cpu_state *cpu = mb_d->computer->cpu;
    uint64_t m1_t1_diff = calc_cycle_diff_t1(&mb_d->d_6522[1], cpu->cycles);
    uint64_t m1_t2_diff = calc_cycle_diff_t2(&mb_d->d_6522[1], cpu->cycles);
    uint64_t m2_t1_diff = calc_cycle_diff_t1(&mb_d->d_6522[0], cpu->cycles);
    uint64_t m2_t2_diff = calc_cycle_diff_t2(&mb_d->d_6522[0], cpu->cycles);

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

void init_slot_mockingboard(computer_t *computer, SlotType_t slot) {

    uint16_t slot_base = 0xC080 + (slot * 0x10);
    printf("init_slot_mockingboard: %d\n", slot);

    mb_cpu_data *mb_d = new mb_cpu_data;
    mb_d->computer = computer;
    mb_d->id = DEVICE_ID_MOCKINGBOARD;
    mb_d->mockingboard = new MockingboardEmulator(&mb_d->audio_buffer);
    mb_d->last_cycle = 0;
    mb_d->slot = slot;
    for (int i = 0; i < 2; i++) { /* on init set to zeroes */
        mb_d->d_6522[i].t1_counter = 0;
        mb_d->d_6522[i].t2_counter = 0;
        mb_d->d_6522[i].t1_latch = 0;
        mb_d->d_6522[i].t2_latch = 0;
        mb_d->d_6522[i].ddra = 0x00;
        mb_d->d_6522[i].ddrb = 0x00;
        mb_d->d_6522[i].ora = 0x00;
        mb_d->d_6522[i].orb = 0x00;
        mb_d->d_6522[i].ira = 0x00;
        mb_d->d_6522[i].irb = 0x00;
        mb_d->d_6522[i].reg_num = 0x00;
        mb_d->d_6522[i].ifr.value = 0;
        mb_d->d_6522[i].ier.value = 0;
        mb_d->d_6522[i].acr = 0;
        mb_d->d_6522[i].pcr = 0;
        mb_d->d_6522[i].sr = 0;
        mb_d->d_6522[i].t1_triggered_cycles = 0;
        mb_d->d_6522[i].t2_triggered_cycles = 0;
    }
    mb_d->event_timer = computer->event_timer;

// TODO: create an "audiosystem" module and move this stuff to it like we did videosystem.
    speaker_state_t *speaker_d = (speaker_state_t *)get_module_state(computer->cpu, MODULE_SPEAKER);
    int dev_id = speaker_d->device_id;

    mb_d->frame_rate = (double)computer->clock->c14M_per_second / (double)computer->clock->c14M_per_frame;
    mb_d->samples_per_frame = (float)OUTPUT_SAMPLE_RATE_INT / mb_d->frame_rate;
    mb_d->samples_per_frame_int = (int32_t)mb_d->samples_per_frame;
    mb_d->samples_per_frame_remainder = mb_d->samples_per_frame - mb_d->samples_per_frame_int;

    mb_d->c14m_rate = computer->clock->c14M_per_second;

/** Init audio stream for the mockingboard device */
    SDL_AudioSpec spec;
    spec.freq = OUTPUT_SAMPLE_RATE_INT;
    spec.format = SDL_AUDIO_F32LE;
    spec.channels = 2;

    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, NULL);
    if (!stream) {
        printf("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(dev_id, stream)) {  /* once bound, it'll start playing when there is data available! */
        printf("Failed to bind stream to device: %s", SDL_GetError());
    }
    mb_d->stream = stream;

    set_slot_state(computer->cpu, slot, mb_d);
    computer->mmu->map_c1cf_page_write_h(0xC0 + slot, { mb_write_Cx00, mb_d }, "MB_IO");
    computer->mmu->map_c1cf_page_read_h(0xC0 + slot, { mb_read_Cx00, mb_d }, "MB_IO");

    insert_empty_mockingboard_frame(mb_d);

    mb_d->event_timer->scheduleEvent(computer->cpu->cycles + 65536, mb_t1_timer_callback, 0x10000000 | (slot << 8) | 0 , mb_d);
    mb_d->event_timer->scheduleEvent(computer->cpu->cycles + 65536, mb_t1_timer_callback, 0x10000000 | (slot << 8) | 1 , mb_d);


    // set up a reset handler to reset the chips on mockingboard
    computer->register_reset_handler(
        [mb_d]() {
            mb_reset(mb_d);
            return true;
        });

    // register a frame processor for the mockingboard.
    computer->device_frame_dispatcher->registerHandler([mb_d]() {
        generate_mockingboard_frame(mb_d);
        return true;
    });

    computer->register_shutdown_handler([mb_d, dev_id]() {
        SDL_DestroyAudioStream(mb_d->stream);
        delete mb_d;
        return true;
    });

    computer->register_debug_display_handler(
        "mockingboard",
        0x0000000000000001, // unique ID for this, need to have in a header.
        [mb_d]() -> DebugFormatter * {
            return debug_registers_6522(mb_d);
        }
    );

    // TODO: register a debug formatter handler, with a name, as a closure that will call here with the mb_d as context.
    // then user can say "debug mb_registers" to call this and get the formatter output to display in watch window.

}

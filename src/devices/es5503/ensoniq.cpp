#include "ensoniq.hpp"
#include "computer.hpp"
#include <algorithm>
#include <cstring>
#include <cassert>

#include "devices/speaker/speaker.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "device_irq_id.hpp"

// Useful constants from MAME implementation
static constexpr uint16_t wavesizes[8] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static constexpr uint32_t wavemasks[8] = { 0x1ff00, 0x1fe00, 0x1fc00, 0x1f800, 0x1f000, 0x1e000, 0x1c000, 0x18000 };
static constexpr uint32_t accmasks[8]  = { 0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff };
static constexpr int      resshifts[8] = { 9, 10, 11, 12, 13, 14, 15, 16 };

//==============================================================================
// ES5503 Implementation
//==============================================================================

ES5503::ES5503()
    : m_oscsenabled(1)
    , m_rege0(0xff)
    , m_channel_strobe(0)
    , m_output_channels(1)
    , m_clock_rate(7159090)  // Default Apple IIgs clock rate
    , m_sample_rate(48000)
    , m_wave_memory(nullptr)
    , m_sdl_stream(nullptr)
{
    reset();
}

ES5503::~ES5503() {
}

void ES5503::init(uint32_t clock_rate, uint32_t sample_rate, int output_channels) {
    m_clock_rate = clock_rate;
    m_sample_rate = sample_rate;
    m_output_channels = output_channels;
    
    // Calculate actual ES5503 output rate
    uint32_t es5503_output_rate = calculate_output_rate();
    
    // Allocate mix buffer for audio generation
    // Size needs to handle maximum samples per call
    m_mix_buffer.resize((es5503_output_rate / 50) * output_channels * 8);
    
    // Update SDL stream rate if stream is set
    if (m_sdl_stream) {
        update_sdl_stream_rate();
    }
    
    reset();
}

void ES5503::reset() {
    m_rege0 = 0xff;  // Initialize like MAME
    m_rege1 = 2;
    m_oscsenabled = 1;
    m_channel_strobe = 0;
    
    // Initialize all oscillators
    for (auto &osc : m_oscillators) {
        osc.freq = 0;
        osc.wtsize = 0;
        osc.control = 0;
        osc.vol = 0;
        osc.data = 0x80;
        osc.wavetblpointer = 0;
        osc.wavetblsize = 0;
        osc.resolution = 0;
        osc.accumulator = 0;
        osc.irqpend = 0;
    }
    update_sdl_stream_rate();
}

uint8_t ES5503::read_wave_byte(uint32_t address) {
    if (m_wave_memory) {
        // Mask to 64KB - addresses wrap around in Apple IIgs hardware
        return m_wave_memory[address & 0xFFFF];
    }
    return 0;
}

void ES5503::update_sdl_stream_rate() {
    if (!m_sdl_stream) {
        return;
    }
    
    uint32_t es5503_output_rate = calculate_output_rate();
    
    SDL_AudioSpec src_spec;
    src_spec.freq = es5503_output_rate;
    src_spec.format = SDL_AUDIO_S16LE;
    src_spec.channels = m_output_channels;
    
    // NULL destination means use device's native rate
    if (!SDL_SetAudioStreamFormat(m_sdl_stream, &src_spec, NULL)) {
        // Log error but don't fail
        printf("Warning: Failed to update ES5503 SDL stream rate to %u Hz: %s\n", 
               es5503_output_rate, SDL_GetError());
        assert(false);
    }
    printf("updated sdl stream rate to %u Hz\n", es5503_output_rate);
}

void ES5503::halt_osc(int onum, int type, uint32_t *accumulator, int resshift) {
    Oscillator *pOsc = &m_oscillators[onum];
    Oscillator *pPartner = &m_oscillators[onum ^ 1];
    int mode = (pOsc->control >> 1) & 3;
    const int partnerMode = (pPartner->control >> 1) & 3;

    // Check for sync mode
    if (mode == MODE_SYNCAM) {
        if (!(onum & 1)) {
            // We're even, so if the odd oscillator 1 below us is playing, restart it
            if (!(m_oscillators[onum - 1].control & 1)) {
                m_oscillators[onum - 1].accumulator = 0;
            }
        }
        // Loop this oscillator for both sync and AM
        mode = MODE_FREE;
    }

    // If 0 found in sample data or mode is not free-run, halt this oscillator
    if ((mode != MODE_FREE) || (type != 0)) {
        pOsc->control |= 1;
    } else {
        // Preserve the relative phase of the oscillator when looping
        const uint16_t wtsize = pOsc->wtsize;
        if ((*accumulator >> resshift) < wtsize) {
            *accumulator -= ((*accumulator >> resshift) << resshift);
        } else {
            *accumulator -= (wtsize << resshift);
        }
    }

    // If we're in swap mode, start the partner
    if (mode == MODE_SWAP) {
        pPartner->control &= ~1;    // Clear the halt bit
        pPartner->accumulator = 0;
    } else {
        // If we're the even oscillator and partner is swap, we retrigger
        if ((partnerMode == MODE_SWAP) && ((onum & 1) == 0)) {
            pOsc->control &= ~1;
            uint16_t wtsize = pOsc->wtsize - 1;
            *accumulator -= (wtsize << resshift);
        }
    }

    // IRQ enabled for this voice?
    if (pOsc->control & 0x08) {
        pOsc->irqpend = 1;
        int irq_osc = update_irq_status();
        /* if (m_irq_callback) {
            m_irq_callback(true);
        } */
    }
}

void ES5503::generate_samples(int16_t *buffer, int num_samples) {
    if (!m_wave_memory) {
        // No wave memory, output silence
        std::fill_n(buffer, num_samples * m_output_channels, 0);
        return;
    }

    // Clear mix buffer
    std::fill_n(&m_mix_buffer[0], num_samples * m_output_channels, 0);

    // Generate samples for each channel
    for (int chan = 0; chan < m_output_channels; chan++) {
        for (int osc = 0; osc < m_oscsenabled; osc++) {
            Oscillator *pOsc = &m_oscillators[osc];

            // Check if oscillator is enabled and assigned to this channel
            if (!(pOsc->control & 1) && ((pOsc->control >> 4) & (m_output_channels - 1)) == chan) {
                uint32_t wtptr = pOsc->wavetblpointer & wavemasks[pOsc->wavetblsize];
                uint32_t acc = pOsc->accumulator;
                const uint16_t wtsize = pOsc->wtsize - 1;
                uint8_t ctrl = pOsc->control;
                const uint16_t freq = pOsc->freq;
                int16_t vol = pOsc->vol;
                int8_t data = -128;
                const int resshift = resshifts[pOsc->resolution] - pOsc->wavetblsize;
                const uint32_t sizemask = accmasks[pOsc->wavetblsize];
                const int mode = (pOsc->control >> 1) & 3;
                int32_t *mixp = &m_mix_buffer[0] + chan;

                for (int snum = 0; snum < num_samples; snum++) {
                    uint32_t altram = acc >> resshift;
                    uint32_t ramptr = altram & sizemask;

                    acc += freq;

                    // Set channel strobe for banking
                    m_channel_strobe = (ctrl >> 4) & 0xf;
                    data = (int32_t)read_wave_byte(ramptr + wtptr) ^ 0x80;

                    if (read_wave_byte(ramptr + wtptr) == 0x00) {
                        halt_osc(osc, 1, &acc, resshift);
                        // Update ctrl from pOsc->control since halt_osc may have modified it
                        ctrl = pOsc->control;
                    } else {
                        if (mode != MODE_SYNCAM) {
                            *mixp += data * vol;
                            // Volume glitch for highest enabled oscillator
                            if (osc == (m_oscsenabled - 1)) {
                                *mixp += data * vol;
                                *mixp += data * vol;
                            }
                        } else {
                            // Sync/AM mode
                            if (osc & 1) {
                                // Odd oscillator modulates the next one up
                                if (osc < 31) {
                                    if (!(m_oscillators[osc + 1].control & 1)) {
                                        m_oscillators[osc + 1].vol = data ^ 0x80;
                                    }
                                }
                            } else {
                                // Even oscillator plays normally
                                *mixp += data * vol;
                                if (osc == (m_oscsenabled - 1)) {
                                    *mixp += data * vol;
                                    *mixp += data * vol;
                                }
                            }
                        }
                        mixp += m_output_channels;

                        // Check if we've reached or exceeded the wavetable size
                        // Calculate new position after incrementing to detect boundary crossing
                        uint32_t new_altram = acc >> resshift;
                        if (new_altram > wtsize) {
                            halt_osc(osc, 0, &acc, resshift);
                            // Update ctrl from pOsc->control since halt_osc may have modified it
                            ctrl = pOsc->control;
                        }
                    }

                    // If oscillator halted, no more samples to generate
                    // Check pOsc->control directly to ensure we see the latest state
                    if (pOsc->control & 1) {
                        ctrl = pOsc->control;
                        break;
                    }
                }

                pOsc->control = ctrl;
                pOsc->accumulator = acc;
                pOsc->data = data ^ 0x80;
            }
        }
    }

    // Convert mix buffer to output buffer
    int32_t *mixp = &m_mix_buffer[0];
    for (int i = 0; i < num_samples * m_output_channels; i++) {
        int32_t sample = *mixp++ / 8;  // Scale down
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buffer[i] = (int16_t)sample;
    }
}

/* the idea here is we actively update the E0 register whenever a OSC IRQ is asserted or cleared
    the return value is -1 if no IRQ pending, otherwise the oscillator number of the first IRQ pending */
int ES5503::update_irq_status() {
    // Check if any oscillators still need servicing
    int i = 0;
    bool any_irq_pending = false;
    for (i = 0; i < m_oscsenabled; i++) {
        if (m_oscillators[i].irqpend) {
            any_irq_pending = true;
            break;
        }
    }
    if (m_irq_callback) {
        m_irq_callback(any_irq_pending);
    }
    // rege0_ir is active-low: bit 7 = 0 means interrupt asserted, bit 7 = 1 means no interrupt
    rege0_ir = any_irq_pending ? 0 : 1;
    if (any_irq_pending) {
        rege0_osc = i;
        return i;
    }
    return -1;
}

uint8_t ES5503::read(uint8_t offset) {
    uint8_t retval;

    if (offset < 0xe0) {
        int osc = offset & 0x1f;

        switch (offset & 0xe0) {
            case 0x00:  // Freq lo
                return (m_oscillators[osc].freq & 0xff);

            case 0x20:  // Freq hi
                return (m_oscillators[osc].freq >> 8);

            case 0x40:  // Volume
                return m_oscillators[osc].vol;

            case 0x60:  // Data
                return m_oscillators[osc].data;

            case 0x80:  // Wavetable pointer
                return (m_oscillators[osc].wavetblpointer >> 8) & 0xff;

            case 0xa0:  // Oscillator control
                return m_oscillators[osc].control;

            case 0xc0:  // Bank select / wavetable size / resolution
                retval = 0;
                if (m_oscillators[osc].wavetblpointer & 0x10000) {
                    retval |= 0x40;
                }
                retval |= (m_oscillators[osc].wavetblsize << 3);
                retval |= m_oscillators[osc].resolution;
                return retval;
        }
    } else {
        // Global registers
        switch (offset) {
            case 0xe0:  { // Interrupt status
                // TODO: if there is no pending IRQ does the osc number remain unchanged?
                int irq_osc = update_irq_status();
                
                
                // Clear IRQ line immediately on read
                /* if (m_irq_callback) {
                    m_irq_callback(false);
                } */
                m_oscillators[irq_osc].irqpend = 0;
                return m_rege0;

                // Scan all oscillators, find first one with IRQ
                /* bool found_interrupt = false;
                for (int i = 0; i < m_oscsenabled; i++) {
                    if (m_oscillators[i].irqpend) {
                        // Signal this oscillator has an interrupt
                        // IR bit is active-low: bit 7 = 0 means interrupt asserted                        
                        retval = (i << 1);
                        
                        // Clear this oscillator's pending flag
                        m_oscillators[i].irqpend = 0;
                        found_interrupt = true;
                        break;
                    }
                } */

                // Check if any oscillators still need servicing
                /* bool has_more_pending = false;
                for (int i = 0; i < m_oscsenabled; i++) {
                    if (m_oscillators[i].irqpend) {
                        has_more_pending = true;
                        if (m_irq_callback) {
                            m_irq_callback(true);
                        }
                        break;
                    }
                } */
                
                // IR bit is active-low: bit 7 = 0 means interrupt asserted, bit 7 = 1 means no interrupt
                /* if (!has_more_pending) {
                    // No more pending interrupts: set IR bit to 1 (no interrupt)
                    m_rege0 |= 0x80;  // Set bit 7 (IR bit = 1 = no interrupt)
                    retval |= 0x80;   // Also set in return value
                } else if (found_interrupt) {
                    // We found an interrupt and there are more: keep IR bit clear (interrupt asserted)
                    m_rege0 &= ~0x80;  // Clear bit 7 (IR bit = 0 = interrupt asserted)
                    retval &= ~0x80;    // Also clear in return value
                }
                m_rege0 = retval | 0x41; // bits 0 and 6 are always set, plus oscillator number and IR bit
                // Return value: bits 0 and 6 are always set, plus oscillator number and IR bit
                return m_rege0; */
            }
            case 0xe1:  // Oscillator enable
                return m_rege1;
                //return (m_oscsenabled - 1) << 1;

            case 0xe2:  // A/D converter
                if (m_adc_callback) {
                    return m_adc_callback();
                }
                return 0;
        }
    }

    return 0;
}

void ES5503::write(uint8_t offset, uint8_t data) {
    if (offset < 0xe0) {
        int osc = offset & 0x1f;

        switch (offset & 0xe0) {
            case 0x00:  // Freq lo
                m_oscillators[osc].freq &= 0xff00;
                m_oscillators[osc].freq |= data;
                break;

            case 0x20:  // Freq hi
                m_oscillators[osc].freq &= 0x00ff;
                m_oscillators[osc].freq |= (data << 8);
                break;

            case 0x40:  // Volume
                m_oscillators[osc].vol = data;
                break;

            case 0x60:  // Data - ignore writes
                break;

            case 0x80:  // Wavetable pointer
                m_oscillators[osc].wavetblpointer = (data << 8);
                break;

            case 0xa0:  // Oscillator control
                // Key on?
                if ((m_oscillators[osc].control & 1) && (!(data & 1))) {
                    m_oscillators[osc].accumulator = 0;
                }

                // Handle CPU halting with swap mode
                if (!(m_oscillators[osc].control & 1) && ((data & 1)) && ((data >> 1) & 1)) {
                    halt_osc(osc, 0, &m_oscillators[osc].accumulator, 
                            resshifts[m_oscillators[osc].resolution]);
                }
                m_oscillators[osc].control = data;
                break;

            case 0xc0:  // Bank select / wavetable size / resolution
                if (data & 0x40) {
                    m_oscillators[osc].wavetblpointer |= 0x10000;
                } else {
                    m_oscillators[osc].wavetblpointer &= 0xffff;
                }

                m_oscillators[osc].wavetblsize = ((data >> 3) & 7);
                m_oscillators[osc].wtsize = wavesizes[m_oscillators[osc].wavetblsize];
                m_oscillators[osc].resolution = (data & 7);
                break;
        }
    } else {
        // Global registers
        switch (offset) {
            case 0xe0:  // Interrupt status - read only
                break;

            case 0xe1:  // Oscillator enable
                m_rege1 = data;
                m_oscsenabled = ((data >> 1) & 0x1f) + 1;
                // Update SDL stream rate when oscillators change
                update_sdl_stream_rate();
                break;

            case 0xe2:  // A/D converter - read only
                break;
        }
    }
}

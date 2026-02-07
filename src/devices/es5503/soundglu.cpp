#include <cstdint>
#include <cassert>
#include <SDL3/SDL.h>
#include "computer.hpp"
#include "devices/es5503/ensoniq.hpp"
#include "devices/speaker/speaker.hpp"
#include "soundglu.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "device_irq_id.hpp"
#include "cpu.hpp"
#include "NClock.hpp"

//==============================================================================
// Apple IIgs Interface (C03C-C03F)
//==============================================================================

/* updates the soundglu data register from the DOC RAM or the ES5503,
   taking into account that reads from GLU are slower than one apple II cycle */
void ensoniq_doc_data_read(ensoniq_state_t *st) {
    // if never done before, or we have not reached the completion time, don't change data yet.
    if (st->soundctl & 0x80) { // waiting for prior transaction to complete
        if (st->clock->get_c14m() >= st->doc_read_complete_time) {
            st->soundctl &= ~0x80; // clear busy bit
    
            if (st->soundctl & 0x40) {     // RAM mode - use full 16-bit address
                uint16_t full_address = (st->soundadrh << 8) | st->soundadrl;
                st->sounddata = st->doc_ram[full_address];
            } else {                       // Register mode - use only low byte
                st->sounddata = st->chip->read(st->soundadrl);
            }        
        } else {
            return; // don't change data yet.
        }
    } else {  // trigger new transaction
        st->soundctl |= 0x80; // set busy bit
        st->doc_read_complete_time = st->clock->get_c14m() + 4; // the diff between 1MHz and 895KHz.. it's actually gonna vary around a bunch.        
        return; // don't change data yet.
    }
    /* if ((st->doc_read_complete_time == 0) || (st->computer->cpu->c_14M < st->doc_read_complete_time)) {
        st->soundctl |= 0x80; // set busy bit
        // TODO: if busy is already set, don't redo this
        st->doc_read_complete_time = st->computer->cpu->c_14M + 16; // or whatever appropriate (subtract some for current cycle?)        
        return; // don't change data yet.
    } */
}

uint8_t ensoniq_read_C0xx(void *context, uint32_t address) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    if (!st->chip) return 0;
    
    switch (address) {
        case 0xC03C:  // Sound Control
            return st->soundctl;
            
        case 0xC03D: { // Sound Data
            uint16_t full_address = (st->soundadrh << 8) | st->soundadrl;
            
            // Bit 6 of control: 0 = access DOC registers, 1 = access DOC ram
            /* if (st->soundctl & 0x40) {
                // RAM mode - use full 16-bit address
                st->sounddata = st->doc_ram[full_address];
            } else {
                // Register mode - use only low byte
                st->sounddata = st->chip->read(st->soundadrl);
            } */
            ensoniq_doc_data_read(st);
            
            // Auto-increment if bit 5 is set
            if (st->soundctl & 0x20) {
                full_address++;
                st->soundadrl = full_address & 0xFF;
                st->soundadrh = (full_address >> 8) & 0xFF;
            }
            
            return st->sounddata;
        }
            
        case 0xC03E:  // Sound Address Low
            return st->soundadrl;
            
        case 0xC03F:  // Sound Address High
            return st->soundadrh;
            
        default:
            assert(false && "Invalid Ensoniq address");
            return 0;
    }
}

void ensoniq_write_C0xx(void *context, uint32_t address, uint8_t data) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    if (!st->chip) return;
    
    switch (address) {
        case 0xC03C:  // Sound Control
            st->soundctl = data;
            // TODO: handle volume changes here.
            st->audio_system->set_volume(data & 0x0F);
            break;
            
        case 0xC03D: { // Sound Data
            st->sounddata = data;
            uint16_t full_address = (st->soundadrh << 8) | st->soundadrl;
            
            // Bit 6 of control: 0 = access DOC RAM, 1 = access DOC registers
            if (st->soundctl & 0x40) {
                // RAM mode - use full 16-bit address
                st->doc_ram[full_address] = data;
            } else {
                // Register mode - use only low byte
                st->chip->write(st->soundadrl, data);
            }
            
            // Auto-increment if bit 5 is set
            if (st->soundctl & 0x20) {
                full_address++;
                st->soundadrl = full_address & 0xFF;
                st->soundadrh = (full_address >> 8) & 0xFF;
            }
            break;
        }
            
        case 0xC03E:  // Sound Address Low
            st->soundadrl = data;
            break;
            
        case 0xC03F:  // Sound Address High
            st->soundadrh = data;
            break;
            
        default:
            assert(false && "Invalid Ensoniq address");
            break;
    }
}

void generate_ensoniq_frame(ensoniq_state_t *st) {
    if (!st->chip || !st->stream) {
        return;
    }
    // TODO: is there a way to only generate an audio frame samples when we have actually finished a video frame?
    if (st->computer->cpu->execution_mode != EXEC_NORMAL) {
        return;
    }
    // Calculate samples per frame based on ES5503 output rate and frame rate
    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    float samples_per_frame_float = (float)es5503_output_rate / st->frame_rate;
    
    // Handle fractional samples per frame (like other audio devices)
    st->samples_accumulated += (samples_per_frame_float - (int)samples_per_frame_float);
    uint32_t samples_this_frame = (uint32_t)samples_per_frame_float;
    if (st->samples_accumulated >= 1.0f) {
        samples_this_frame++;
        st->samples_accumulated -= 1.0f;
    }
    
    // Ensure we don't exceed buffer size
    if (samples_this_frame > 16384) {
        samples_this_frame = 16384;
    }
    
    st->chip->generate_samples(st->audio_buffer, samples_this_frame);
    SDL_PutAudioStreamData(st->stream, st->audio_buffer, samples_this_frame * sizeof(int16_t));
}

DebugFormatter * debug_ensoniq(ensoniq_state_t *st) {
    DebugFormatter *df = new DebugFormatter();
    df->addLine("Control: %02X Address: %04X", st->soundctl, (st->soundadrh << 8) | st->soundadrl);
    //df->addLine("  Sound Data: %02X", st->sounddata);
    ES5503 *chip = st->chip;
    df->addLine("E0: %02X   E1: %02X   E2: %02X", chip->get_rege0(), chip->get_rege1(), 0  /* , chip->get_adc_callback() */ );

    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    df->addLine("OutRate: %u Hz  OSCs: %d", es5503_output_rate, chip->get_oscsenabled());

    df->addLine("Osc Freq WtSize Ctrl Vol Data WtPtr WtSize Res Acc       Irq");
    for (int o = 0; o < 32; o++) {
        Oscillator *osc = chip->get_oscillator(o);
        df->addLine(" %2d %04X   %04X  %02X  %02X  %02X   %04X  %02X    %02X  %08X %02X", 
            o, osc->freq, osc->wtsize, osc->control, osc->vol, 
            osc->data, osc->wavetblpointer, osc->wavetblsize, osc->resolution, 
            osc->accumulator, osc->irqpend);
    }
    return df;
}

void init_ensoniq_slot(computer_t *computer, SlotType_t slot) {
    ensoniq_state_t *st = new ensoniq_state_t();
    
    // Allocate 64KB DOC RAM
    st->doc_ram = new uint8_t[0x10000];
    std::memset(st->doc_ram, 0, 0x10000);

    st->audio_system = computer->audio_system;

    // Allocate buffer large enough for maximum samples per frame
    // Max rate ~298kHz at 59.92 fps = ~4972 samples, add some headroom
    st->audio_buffer = new int16_t[16384];
    
    // Create and initialize ES5503 chip
    st->chip = new ES5503();
    st->chip->init(7159090, 48000, 1);  // Apple IIgs clock rate, 48kHz stereo
    st->chip->set_wave_memory(st->doc_ram);
    st->computer = computer;
    st->clock = computer->clock;
    st->irq_control = computer->irq_control;

    // Set up IRQ callback to propagate interrupts to CPU
    st->chip->set_irq_callback([st](bool state) {
        if (st->irq_control && st->computer && st->computer->cpu) {
            st->irq_control->set_irq(IRQ_ID_SOUNDGLU, state);
        }
    });
    
    // Register I/O handlers for C03C-C03F
    for (uint32_t i = 0xC03C; i <= 0xC03F; i++) {
        computer->mmu->set_C0XX_write_handler(i, { ensoniq_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { ensoniq_read_C0xx, st });
    }

    speaker_state_t *speaker_d = (speaker_state_t *)get_module_state(computer->cpu, MODULE_SPEAKER);
    //int dev_id = speaker_d->device_id;

    // Calculate frame rate
    st->frame_rate = (double)computer->clock->get_c14m_per_second() / (double)computer->clock->get_c14m_per_frame();
    
    // Calculate ES5503 output rate and set up SDL stream
    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    
    // Calculate initial samples per frame
    st->samples_per_frame = (float)es5503_output_rate / st->frame_rate;
    st->samples_accumulated = 0.0f;
    st->stream = st->audio_system->create_stream(es5503_output_rate, 1, SDL_AUDIO_S16LE, true);
    // Set the stream pointer in the chip so it can update the rate when oscillators change
    st->chip->set_sdl_stream(st->stream);

#if 0
    SDL_AudioSpec spec;
    spec.freq = es5503_output_rate;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;

    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, NULL);
    if (!stream) {
        printf("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(dev_id, stream)) {  /* once bound, it'll start playing when there is data available! */
        printf("Failed to bind stream to device: %s", SDL_GetError());
    }
    st->stream = stream;
    // Set the stream pointer in the chip so it can update the rate when oscillators change
    st->chip->set_sdl_stream(stream);
#endif

    // register a frame processor for the mockingboard.
    computer->device_frame_dispatcher->registerHandler([st]() {
        generate_ensoniq_frame(st);
        return true;
    });

    computer->register_debug_display_handler(
        "es5503",
        DH_ES5503, // unique ID for this, need to have in a header.
        [st]() -> DebugFormatter * {
            return debug_ensoniq(st);
        }
    );

    computer->register_reset_handler([st]() {
        // this caused the audio to get badly delayed / out of sync. added calculate_output_rate() to reset() to fix.
        st->chip->reset();
        return true;
    });
}
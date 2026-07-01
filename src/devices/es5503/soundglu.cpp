#include <cstdint>
#include <cassert>
#include <SDL3/SDL.h>
#include "computer.hpp"
#include "devices/es5503/ensoniq.hpp"

#include "soundglu.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "device_irq_id.hpp"

#include "NClock.hpp"

//==============================================================================
// Fast-forward / catch-up
//==============================================================================

/* Advance ES5503 sample generation (and therefore oscillator IRQ delivery) up to
   the supplied 14M-clock time. This is the equivalent of MAME's m_stream->update():
   it is invoked on every register/RAM access and from the periodic cycle handler so
   that oscillator state and interrupts are caught up to "now" rather than only at the
   end of a video frame.

   Timing identity: one ES5503 output sample takes 16 * (oscsenabled + 2) 14M cycles
   (16 14M per ~895 kHz DOC cycle, (oscs+2) DOC cycles per sample). generate_samples()
   raises IRQs inside halt_osc()->update_irq_status()->m_irq_callback as oscillators
   halt, so generating the right number of samples also fires IRQs at the right time. */
static void ensoniq_catch_up(ensoniq_state_t *st, uint64_t now_c14m) {
    if (!st->chip || !st->stream) {
        return;
    }

    // Only generate during normal execution. While paused / single-stepping (or in
    // CLOCK_FREE_RUN, where c_14M is frozen) we just keep the time base pinned to "now"
    // so that resuming does not try to render a huge backlog of samples.
    if (st->computer->execution_mode != EXEC_NORMAL) {
        st->last_catchup_c14m = now_c14m;
        st->c14m_accum = 0;
        return;
    }

    // Guard against the clock going backwards (e.g. after a reset/resync).
    if (now_c14m <= st->last_catchup_c14m) {
        st->last_catchup_c14m = now_c14m;
        return;
    }

    st->c14m_accum += (now_c14m - st->last_catchup_c14m);
    st->last_catchup_c14m = now_c14m;

    const uint32_t c14m_per_sample = 16u * (st->chip->get_oscsenabled() + 2u);
    if (c14m_per_sample == 0) {
        return;
    }

    uint64_t samples_due = st->c14m_accum / c14m_per_sample;
    st->c14m_accum -= samples_due * c14m_per_sample;

    if (samples_due == 0) {
        return;
    }

    // Clamp pathological backlogs to the audio buffer size so we never overrun it.
    const uint64_t MAX_SAMPLES = 16384;
    if (samples_due > MAX_SAMPLES) {
        samples_due = MAX_SAMPLES;
    }

    const uint32_t BATCH = 1024;
    while (samples_due > 0) {
        uint32_t n = (samples_due > BATCH) ? BATCH : (uint32_t)samples_due;
        st->chip->generate_samples(st->audio_buffer, n);
        SDL_PutAudioStreamData(st->stream, st->audio_buffer, n * sizeof(int16_t));
        samples_due -= n;
    }
}

//==============================================================================
// Apple IIgs Interface (C03C-C03F)
//==============================================================================

void ensoniq_update_transaction(ensoniq_state_t *st) {
    if (st->soundctl & 0x80) { // if waiting for prior transaction to complete
        if (st->clock->get_c14m() >= st->doc_read_complete_time) {
            st->soundctl &= ~0x80; // clear busy bit
    
            if (st->soundctl & 0x40) {     // RAM mode - use full 16-bit address
                uint16_t full_address = (st->soundadrh << 8) | st->soundadrl;
                st->sounddata = st->doc_ram[full_address];
            } else {                       // Register mode - use only low byte
                st->sounddata = st->chip->read(st->soundadrl);
            }        
        }
    }
}

/* updates the soundglu data register from the DOC RAM or the ES5503,
   taking into account that reads from GLU are slower than one apple II cycle */
void ensoniq_doc_data_read(ensoniq_state_t *st) {
    // if never done before, or we have not reached the completion time, don't change data yet.
    if (st->soundctl & 0x80) { // waiting for prior transaction to complete
        ensoniq_update_transaction(st);
    } else {  // trigger new transaction
        st->soundctl |= 0x80; // set busy bit
        st->doc_read_complete_time = st->clock->get_c14m() + 4; // the diff between 1MHz and 895KHz.. it's actually gonna vary around a bunch.        
        return; // don't change data yet.
    }
}

uint8_t ensoniq_read_C0xx(void *context, uint32_t address) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    if (!st->chip) return 0;

    // Fast-forward to now so register reads (esp. E0 IRQ status) and deferred
    // DOC/RAM reads observe up-to-date oscillator state. Mirrors MAME read().
    ensoniq_catch_up(st, st->clock->get_c14m());

    switch (address) {
        case 0xC03C:  // Sound Control
            ensoniq_update_transaction(st); // update on every soundglu access
            return st->soundctl | 0xF; // this reg is write-only, low 4 bits are always 1111 on read
            
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
            ensoniq_update_transaction(st); // update on every soundglu access
            return st->soundadrl;
            
        case 0xC03F:  // Sound Address High
            ensoniq_update_transaction(st); // update on every soundglu access
            return st->soundadrh;
            
        default:
            assert(false && "Invalid Ensoniq address");
            return 0;
    }
}

void ensoniq_write_C0xx(void *context, uint32_t address, uint8_t data) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    if (!st->chip) return;

    // Fast-forward to now BEFORE applying the write, so prior samples are rendered
    // with the old register/RAM state. Mirrors MAME write(). Covers both DOC
    // register writes and DOC RAM writes.
    ensoniq_catch_up(st, st->clock->get_c14m());

    switch (address) {
        case 0xC03C:  // Sound Control
            // bit 7 (busy bit) is readonly
            st->soundctl = (data & 0x7F) | (st->soundctl & 0x80);
            //st->soundctl = data;
            // TODO: handle volume changes here.
            st->audio_system->set_volume(data & 0x0F);
            break;
            
        case 0xC03D: { // Sound Data
            // TODO: what should this do if the busy is already set?
            st->sounddata = data;
            uint16_t full_address = (st->soundadrh << 8) | st->soundadrl;
            
            // Bit 6 of control: 0 = access DOC RAM, 1 = access DOC registers
            if (st->soundctl & 0x40) {
                // RAM mode - use full 16-bit address
                st->doc_ram[full_address] = data;
            } else {
                // Register mode - use only low byte
                st->chip->write(st->soundadrl, data);
                // Writing the oscillator-enable register (0xE1) changes the number of
                // enabled oscillators and thus the output sample rate (chip->write()
                // already updated the SDL stream rate). The pre-write catch_up rendered
                // the backlog at the OLD rate; drop the stale fractional remainder and
                // re-base time here so subsequent samples use the NEW rate cleanly.
                if (st->soundadrl == 0xE1) {
                    st->c14m_accum = 0;
                    st->last_catchup_c14m = st->clock->get_c14m();
                }
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
    // Samples are now generated incrementally via ensoniq_catch_up() on every
    // register/RAM access and from the per-video-cycle clock handler. Here we just
    // perform a final catch-up to the current 14M time so the SDL stream stays fed
    // through the end of the frame (it will normally render ~0 samples, since the
    // cycle handler has already advanced to frame end).
    ensoniq_catch_up(st, st->clock->get_c14m());
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

    // Initialize the catch-up time base to "now".
    st->last_catchup_c14m = computer->clock->get_c14m();
    st->c14m_accum = 0;

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

    // Periodic catch-up: fires once per video cycle (~895 kHz DOC / ~1 MHz video),
    // delivering oscillator IRQs between register accesses. This is the equivalent of
    // MAME's delayed_stream_update timer. It is cheap when fewer than one sample is due
    // (just a delta/divide/compare). Requires the NClockIIgs cycle-handler dispatch.
    computer->clock->set_cycle_handler([st]() {
        ensoniq_catch_up(st, st->clock->get_c14m());
    });

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

    computer->register_reset_handler([st](bool cold_start) {
        // this caused the audio to get badly delayed / out of sync. added calculate_output_rate() to reset() to fix.
        st->chip->reset();
        // Re-base the catch-up time so we don't render a backlog after reset.
        st->last_catchup_c14m = st->clock->get_c14m();
        st->c14m_accum = 0;
        return true;
    });
}
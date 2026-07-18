#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <SDL3/SDL.h>
#include "computer.hpp"
#include "devices/es5503/ensoniq.hpp"
#include "Device_ID.hpp"
#include "Module_ID.hpp"

#include "soundglu.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "device_irq_id.hpp"

#include "NClock.hpp"

//==============================================================================
// Fast-forward / catch-up
//==============================================================================

// Stereo-card path maps even DOC channels → right and odd → left (TN #19).
// Mono software usually parks every voice on one channel (often 0 / right), so
// only one SDL side gets energy. If all non-halted enabled oscillators share the
// same CA0, mirror that side onto both L and R.
static void ensoniq_mirror_mono_to_stereo(ensoniq_state_t *st, uint32_t n_frames) {
    constexpr int ch = ensoniq_state_t::CHANNELS;
    if (ch != 2 || !st->chip || !st->audio_buffer || n_frames == 0) {
        return;
    }

    const int oscs = st->chip->get_oscsenabled();
    int seen = 0;
    int ca0 = 0;
    for (int o = 0; o < oscs; o++) {
        Oscillator *osc = st->chip->get_oscillator(o);
        if (osc->control & 1) {
            continue; // halted
        }
        const int osc_ca0 = (osc->control >> 4) & 1;
        if (seen == 0) {
            ca0 = osc_ca0;
            seen = 1;
        } else if (osc_ca0 != ca0) {
            return; // true stereo — leave L/R alone
        }
    }
    if (seen == 0) {
        return; // silence
    }

    // Same flip as ES5503::generate_samples: odd→L(0), even→R(1).
    const int live = ca0 ^ 1;
    for (uint32_t i = 0; i < n_frames; i++) {
        const int16_t s = st->audio_buffer[i * ch + live];
        st->audio_buffer[i * ch + 0] = s;
        st->audio_buffer[i * ch + 1] = s;
    }
}

// Resample staged DOC stereo frames to the host device rate and hand them to SDL
// in a single Put (normally once per video frame). Feeding SDL small chunks at
// the raw DOC rate (e.g. 27117 Hz) made SDL's internal chunk resampler glitch
// audibly, so the stream runs 1:1 at the device rate and we do the rate
// conversion here. Staging/resample counts are in frames; buffers are interleaved
// [L,R,L,R,...].
static void ensoniq_flush_sdl_staging(ensoniq_state_t *st) {
    if (!st->stream || !st->sdl_staging || st->sdl_staging_count == 0) {
        return;
    }

    constexpr int ch = ensoniq_state_t::CHANNELS;
    const uint32_t src_n = st->sdl_staging_count; // DOC frames
    const uint32_t src_rate = st->chip->calculate_output_rate();
    const uint32_t dst_rate = st->sdl_device_rate ? st->sdl_device_rate : 48000;
    if (src_rate == 0 || dst_rate == 0 || !st->sdl_resample_buf) {
        st->sdl_staging_count = 0;
        return;
    }

    const int queued_now = SDL_GetAudioStreamQueued(st->stream);

    // Keep ~60ms queued. The device callback pulls a whole buffer at once (e.g.
    // 1024 frames); if the queue dips below that, the callback pads with silence,
    // which is an audible dropout. Prefill on (re)start, then trim the resample
    // ratio ±0.5% to hold the target depth.
    const int target_bytes = (int)(dst_rate * ch * sizeof(int16_t) * 60 / 1000);
    if (queued_now == 0) {
        uint32_t pre = dst_rate / 20;  // 50ms of silence (frames)
        if (pre > ensoniq_state_t::SDL_STAGING_CAP) pre = ensoniq_state_t::SDL_STAGING_CAP;
        std::memset(st->sdl_resample_buf, 0, pre * ch * sizeof(int16_t));
        SDL_PutAudioStreamData(st->stream, st->sdl_resample_buf, (int)(pre * ch * sizeof(int16_t)));
    }
    double err = (double)(target_bytes - queued_now) / (double)target_bytes;
    if (err > 1.0) err = 1.0;
    if (err < -1.0) err = -1.0;
    const double ratio = 1.0 + 0.005 * err;

    // Linear resample DOC→device; fractional read position carries across frames.
    const double step = (double)src_rate / ((double)dst_rate * ratio);
    uint32_t out_n = 0; // output frames
    const uint32_t out_cap = ensoniq_state_t::SDL_STAGING_CAP;
    double pos = st->resample_pos;

    while (out_n < out_cap && pos < (double)src_n) {
        const uint32_t i0 = (uint32_t)pos;
        const double frac = pos - (double)i0;
        const uint32_t i1 = (i0 + 1 < src_n) ? i0 + 1 : i0;
        for (int c = 0; c < ch; c++) {
            const int16_t s0 = st->sdl_staging[i0 * ch + c];
            const int16_t s1 = st->sdl_staging[i1 * ch + c];
            st->sdl_resample_buf[out_n * ch + c] =
                (int16_t)((double)s0 + ((double)s1 - (double)s0) * frac);
        }
        out_n++;
        pos += step;
    }
    st->resample_pos = pos - (double)src_n;
    if (st->resample_pos < 0.0) {
        st->resample_pos = 0.0;
    }

    if (out_n > 0) {
        SDL_PutAudioStreamData(st->stream, st->sdl_resample_buf,
                               (int)(out_n * ch * sizeof(int16_t)));
    }
    st->sdl_staging_count = 0;
}

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
        st->sdl_staging_count = 0;
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
    if (samples_due == 0) {
        return;
    }

    // Cap how many samples we generate this call, but only consume c14m for
    // samples we actually produce — otherwise oscillators fall behind the clock
    // and the stream underruns / jumps.
    const uint64_t MAX_SAMPLES = 16384;
    if (samples_due > MAX_SAMPLES) {
        samples_due = MAX_SAMPLES;
    }
    st->c14m_accum -= samples_due * c14m_per_sample;

    constexpr int ch = ensoniq_state_t::CHANNELS;
    const uint32_t BATCH = 256;
    while (samples_due > 0) {
        uint32_t n = (samples_due > BATCH) ? BATCH : (uint32_t)samples_due;
        st->chip->generate_samples(st->audio_buffer, n);
        ensoniq_mirror_mono_to_stereo(st, n);

        // TEMP (MAME-like / stereo-card test): do not apply $C03C volume to DOC
        // output. Real motherboard amp uses this nibble; stereo cards tap DOC
        // channels pre-volume, and Alien Mind stereo sets the nibble to 0.
        // Also avoids double attenuation with host volume. Restore easily.
#if 0
        // Apply the $C03C volume nibble at emulated time. Routing it through
        // SDL_SetAudioStreamGain applied volume changes to whole ~23ms callback
        // chunks at playback time, smearing sub-ms hardware volume dips into
        // audible amplitude chops. Slew the gain with a ~20ms one-pole ramp:
        // firmware/demos flip the nibble (e.g. 15→5→15 for 0.6–7.7ms) around DOC
        // access bursts, and applying that instantly chops the waveform into
        // low-frequency distortion; the real analog volume path smooths it.
        // One gain step per DOC frame; same gain applied to L and R.
        {
            const float target = (float)st->audio_system->get_volume(); // 0..15
            if (st->vol_smooth < 0.0f) st->vol_smooth = target;
            const float dt = (float)c14m_per_sample / 14318181.0f; // seconds per DOC sample
            const float alpha = dt / 0.020f;
            float g = st->vol_smooth;
            for (uint32_t i = 0; i < n; i++) {
                g += alpha * (target - g);
                const float scale = g * (1.0f / 16.0f);
                for (int c = 0; c < ch; c++) {
                    st->audio_buffer[i * ch + c] =
                        (int16_t)((float)st->audio_buffer[i * ch + c] * scale);
                }
            }
            st->vol_smooth = g;
        }
#endif

        // Queue DOC frames only — SDL is fed once per frame in generate_ensoniq_frame.
        if (st->sdl_staging) {
            uint32_t off = 0; // frames into audio_buffer
            while (off < n) {
                uint32_t space = ensoniq_state_t::SDL_STAGING_CAP - st->sdl_staging_count;
                if (space == 0) {
                    // Pathological: more than one max-rate frame queued. Flush rather
                    // than drop (should not happen at 60Hz with 16k staging).
                    ensoniq_flush_sdl_staging(st);
                    space = ensoniq_state_t::SDL_STAGING_CAP;
                }
                uint32_t take = n - off;
                if (take > space) take = space;
                std::memcpy(st->sdl_staging + st->sdl_staging_count * ch,
                            st->audio_buffer + off * ch, take * ch * sizeof(int16_t));
                st->sdl_staging_count += take;
                off += take;
            }
        } else {
            SDL_PutAudioStreamData(st->stream, st->audio_buffer,
                                   (int)(n * ch * sizeof(int16_t)));
        }
        samples_due -= n;
    }
}

//==============================================================================
// Apple IIgs Interface (C03C-C03F)
//==============================================================================

void ensoniq_update_transaction(ensoniq_state_t *st) {
    // Legacy helper: complete any deferred read using the address latched when
    // the transaction started (not the live pointer — see C03D pipeline).
    if (st->soundctl & 0x80) {
        if (st->clock->get_c14m() >= st->doc_read_complete_time) {
            st->soundctl &= ~0x80;
            if (st->soundctl & 0x40) {
                st->sounddata = st->doc_ram[st->doc_read_latched_addr];
            } else {
                st->sounddata = st->chip->read(st->doc_read_latched_addr & 0xFF);
            }
        }
    }
}

/* Sound GLU $C03D read: one-access pipeline (MAME / real DOC lag).
   Return the previous latch value, then fetch from the current address into the
   latch. Callers such as NinjaTrackerPlus change $C03E between the priming read
   and the consuming read; the fetch must use the address at priming time, which
   this immediate pipeline does by fetching before the caller can store a new
   address. Busy is kept clear (MAME: ctrl writes force bit7=0). */
static uint8_t ensoniq_doc_data_read_pipeline(ensoniq_state_t *st) {
    const uint8_t ret = st->sounddata;
    const uint16_t full_address = (uint16_t)((st->soundadrh << 8) | st->soundadrl);

    if (st->soundctl & 0x40) {
        st->sounddata = st->doc_ram[full_address];
    } else {
        st->sounddata = st->chip->read(st->soundadrl);
    }

    st->soundctl &= ~0x80; // never leave DOC busy stuck after a data read
    st->doc_read_latched_addr = full_address;

    if (st->soundctl & 0x20) {
        const uint16_t next = (uint16_t)(full_address + 1);
        st->soundadrl = next & 0xFF;
        st->soundadrh = (next >> 8) & 0xFF;
    }
    return ret;
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
            
        case 0xC03D: // Sound Data
            // Bit 6 of control: 0 = DOC registers, 1 = DOC RAM (handled in pipeline).
            return ensoniq_doc_data_read_pipeline(st);
            
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
            // Bit 7 (busy) is read-only / always clear on write (MAME).
            st->soundctl = data & 0x7F;
            // DOC register mode: only low address byte is meaningful; clear high
            // like MAME so a prior RAM-mode pointer cannot leak into register ops.
            if (!(st->soundctl & 0x40)) {
                st->soundadrh = 0;
            }
            // System volume nibble; applied per-sample (slewed) in ensoniq_catch_up.
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
    // Deliver ~1 frame of DOC audio to SDL in one Put.
    ensoniq_flush_sdl_staging(st);
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

bool pack_ensoniq_state(ensoniq_state_t *st, std::vector<uint8_t> &out, std::string &err) {
    if (!st || !st->chip) {
        err = "no ensoniq";
        return false;
    }
    constexpr uint32_t kVersion = 1;
    constexpr size_t kHeaderSize = 16;
    constexpr size_t kOscSize = 24;
    constexpr size_t kBlobSize = kHeaderSize + 32 * kOscSize;
    out.assign(kBlobSize, 0);

    ES5503 *chip = st->chip;
    const uint32_t output_rate = chip->calculate_output_rate();
    std::memcpy(out.data() + 0, &kVersion, 4);
    out[4] = st->soundctl;
    out[5] = st->sounddata;
    out[6] = st->soundadrl;
    out[7] = st->soundadrh;
    out[8] = chip->get_rege0();
    out[9] = chip->get_rege1();
    out[10] = chip->get_oscsenabled();
    out[11] = 0;
    std::memcpy(out.data() + 12, &output_rate, 4);

    for (int o = 0; o < 32; ++o) {
        Oscillator *osc = chip->get_oscillator(o);
        uint8_t *rec = out.data() + kHeaderSize + static_cast<size_t>(o) * kOscSize;
        std::memcpy(rec + 0, &osc->freq, 2);
        std::memcpy(rec + 2, &osc->wtsize, 2);
        rec[4] = osc->control;
        rec[5] = osc->vol;
        rec[6] = osc->data;
        rec[7] = 0;
        std::memcpy(rec + 8, &osc->wavetblpointer, 4);
        rec[12] = osc->wavetblsize;
        rec[13] = osc->resolution;
        rec[14] = osc->irqpend;
        rec[15] = 0;
        std::memcpy(rec + 16, &osc->accumulator, 4);
        // bytes 20–23 already zero
    }
    return true;
}

void init_ensoniq_slot(computer_t *computer, SlotType_t slot) {
    ensoniq_state_t *st = new ensoniq_state_t();
    
    // Allocate 64KB DOC RAM
    st->doc_ram = new uint8_t[0x10000];
    std::memset(st->doc_ram, 0, 0x10000);

    computer->set_module_state(MODULE_ENSONIQ, st);

    st->audio_system = computer->audio_system;

    // Allocate buffers for interleaved stereo frames (L,R). Cap is in frames;
    // max DOC rate ~298kHz at 59.92 fps ≈ 4972 frames/frame, with headroom.
    constexpr int ch = ensoniq_state_t::CHANNELS;
    st->audio_buffer = new int16_t[16384 * ch];
    st->sdl_staging = new int16_t[ensoniq_state_t::SDL_STAGING_CAP * ch];
    st->sdl_resample_buf = new int16_t[ensoniq_state_t::SDL_STAGING_CAP * ch];
    st->sdl_staging_count = 0;
    st->resample_pos = 0.0;
    
    // Create and initialize ES5503 chip (2 host channels = TN #19 stereo card)
    st->chip = new ES5503();
    st->chip->init(7159090, 48000, ch);
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
    
    // SDL stream at host device rate; we resample DOC→device once per frame.
    st->sdl_device_rate = st->audio_system->get_device_sample_rate();
    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    st->samples_per_frame = (float)es5503_output_rate / st->frame_rate;
    st->samples_accumulated = 0.0f;
    // apply_volume=false: the $C03C volume is applied per-sample at generation
    // time in ensoniq_catch_up (SDL stream gain acts at playback time, which
    // smears sub-ms hardware volume dips across whole callback buffers).
    st->stream = st->audio_system->create_stream(st->sdl_device_rate, ch, SDL_AUDIO_S16LE, false);
    st->chip->set_sdl_stream(st->stream);

    // Initialize the catch-up time base to "now".
    st->last_catchup_c14m = computer->clock->get_c14m();
    st->c14m_accum = 0;

#if 0
    SDL_AudioSpec spec;
    spec.freq = es5503_output_rate;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = ch;

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

    computer->register_device_debug(DEVICE_ID_ENSONIQ,
        [st](uint32_t op, const std::vector<uint8_t> & /*req*/,
             std::vector<uint8_t> &reply, std::string &err) {
            if (op != DEVOP_STATE_GET) {
                err = "unsupported op";
                return false;
            }
            return pack_ensoniq_state(st, reply, err);
        });

    computer->register_reset_handler([st](bool cold_start) {
        // this caused the audio to get badly delayed / out of sync. added calculate_output_rate() to reset() to fix.
        st->chip->reset();
        // Re-base the catch-up time so we don't render a backlog after reset.
        st->last_catchup_c14m = st->clock->get_c14m();
        st->c14m_accum = 0;
        return true;
    });
}
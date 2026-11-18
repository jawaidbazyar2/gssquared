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

#include <iostream>
#include <vector>

#include <cstdint>
#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_audio.h>

#include "AY8910-2.hpp"
#include "W6522.hpp"
#include "Ssi263.hpp"

#include "gs2.hpp"
#include "mb2.hpp"
#include "debug.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"

#include <algorithm>
#include <cstddef>

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

void debug_register_change(uint64_t cycle, uint8_t chip_index, uint8_t reg, uint8_t value) {
    std::cout << "[" << cycle << "] Register " << register_names[reg] << " set to: " << static_cast<int>(value) << std::endl;
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
    df->addLine("Last event: %llu  Last time: %llu", (unsigned long long)mb->dbg_last_event, (unsigned long long)mb->dbg_last_time);
    return df;
}
#endif

class Mockingboard {
private:
    N6522 *n6522[2];
    AY8910s *ay8910s;
    Ssi263 *ssi263;
    SDL_AudioStream *stream;
    uint8_t slot;
    VidRail *vid;
    InterruptController *irq_control = nullptr;
    AudioSystem *audio_system;

    NClock *clock;

    // TODO: this is an undimensioned vector, which will be doing all kinds of memory allocation
    std::vector<float> audio_buffer;

    // SSI-263 sample horizon (vid-cycles), matching AY sample_vid_.
    uint64_t speech_vid_ = 0;
    uint64_t speech_accum_ = 0;
    std::vector<float> speech_pending;

    // Engine peaks ~0.24–0.54 at AMP=15 (RMS ~0.08–0.19). AY one channel
    // max is 0.25 (RMS ~0.18); speaker default vol 6 is 4125/32767 ≈ 0.13.
    // 0.85 puts loud phonemes next to a full-scale AY channel without
    // clipping the engine. Mixed to L+R: the card sums SSI with both AYs.
    static constexpr float SPEECH_MIX_GAIN = 0.85f;

    void note_ar() {
        // SSI-263 A/R is active-low, wired to CA1 of the 6522 at $Cn80.
        n6522[0]->set_ca1(!ssi263->request());
    }

    void catchup_speech() {
        if (!clock || !ssi263) {
            return;
        }
        const uint64_t now = clock->get_vid_cycles();
        const uint64_t vid_per_sec = clock->get_vid_cycles_per_second();
        const uint64_t vid_per_frame = clock->get_vid_cycles_per_frame();

        ssi263->set_xck((double)vid_per_sec);

        if (now <= speech_vid_) {
            note_ar();
            return;
        }

        // Same stall window as the AY: drop a multi-frame gap instead of
        // synthesizing a catch-up burst. Skip chip time so A/R stays honest.
        if ((now - speech_vid_) > vid_per_frame * 3) {
            const uint64_t new_vid = now - vid_per_frame;
            const double skip_s =
                (double)(new_vid - speech_vid_) / (double)vid_per_sec;
            ssi263->skip(skip_s);
            speech_pending.clear();
            speech_vid_ = new_vid;
            speech_accum_ = 0;
            note_ar();
        }

        if (now <= speech_vid_) {
            return;
        }

        long nsamples = 0;
        uint64_t vid = speech_vid_;
        uint64_t accum = speech_accum_;
        while (vid < now) {
            accum += vid_per_sec;
            uint64_t cycles = accum / OUTPUT_SAMPLE_RATE_INT;
            accum %= OUTPUT_SAMPLE_RATE_INT;
            if (cycles == 0) {
                continue;
            }
            uint64_t sample_end = vid + cycles;
            if (sample_end > now) {
                sample_end = now;
            }
            vid = sample_end;
            nsamples++;
        }
        speech_vid_ = vid;
        speech_accum_ = accum;

        if (nsamples > 0) {
            std::vector<float> tmp((size_t)nsamples + 1, 0.0f);
            long got = ssi263->run(nsamples, tmp.data());
            if (got < 0) {
                got = 0;
            }
            speech_pending.insert(speech_pending.end(),
                                  tmp.begin(), tmp.begin() + got);
        }
        note_ar();
    }

    void mix_speech() {
        const size_t nframes = audio_buffer.size() / 2;
        const size_t nspeech = speech_pending.size();
        const size_t n = std::min(nframes, nspeech);
        for (size_t i = 0; i < n; i++) {
            float s = speech_pending[i] * SPEECH_MIX_GAIN;
            if (s > 1.0f) {
                s = 1.0f;
            } else if (s < -1.0f) {
                s = -1.0f;
            }
            audio_buffer[i * 2] += s;
            audio_buffer[i * 2 + 1] += s;
        }
        if (n > 0) {
            speech_pending.erase(speech_pending.begin(),
                                 speech_pending.begin() + (std::ptrdiff_t)n);
        }
    }

public:
    Mockingboard(NClock *clock, InterruptController *irq_control, VidRail *vid, AudioSystem *audio_system, uint8_t slot) {
        // we need a local InterruptController to merge the IRQs from the two 6522 chips.
        InterruptController *local_irq_control = new InterruptController();
        local_irq_control->register_irq_receiver([this,slot](bool irq) {
            // propagate it up to the system IRQ controller
            this->irq_control->set_irq((device_irq_id)slot, irq);
        });
        /* n6522[0] = new N6522("MB_6522 1 0x80", clock, local_irq_control, event_timer, slot, 0);
        n6522[1] = new N6522("MB_6522 2 0x00", clock, local_irq_control, event_timer, slot, 1); */
        n6522[0] = new N6522("MB_6522 1 0x80", clock, local_irq_control, slot, 0);
        n6522[1] = new N6522("MB_6522 2 0x00", clock, local_irq_control, slot, 1);

        clock->set_cycle_handler([this]() {
            n6522[0]->incr_cycle();
            n6522[1]->incr_cycle();
        });

        // TODO: this doesn't need irq_control or slot
        ay8910s = new AY8910s(&audio_buffer, clock, audio_system /* , irq_control, slot */);

        this->clock = clock;
        this->irq_control = irq_control;
        this->vid = vid;
        this->slot = slot;
        this->audio_system = audio_system;

        const double xck = clock ? (double)clock->get_vid_cycles_per_second() : 1020484.0;
        ssi263 = new Ssi263(xck, (double)OUTPUT_SAMPLE_RATE_INT);
        speech_vid_ = clock ? clock->get_vid_cycles() : 0;
        speech_accum_ = 0;

        stream = audio_system->create_stream(OUTPUT_SAMPLE_RATE_INT, 2, SDL_AUDIO_F32LE, false);

        // Port A pull-ups hold the bus high at power-on; match reset().
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);
        n6522[0]->set_ca1(true); // A/R idle (pin high)
    }
    ~Mockingboard() {
        audio_system->destroy_stream(stream);

        delete n6522[0];
        delete n6522[1];
        delete ay8910s;
        delete ssi263;
    }

    void insert_empty_frame() {
        const float empty_frame[736*2] = {0.0f};
        SDL_PutAudioStreamData(stream, empty_frame, 736 * 2 * sizeof(float));
    }
    
    void debug_registers();

    void write(uint32_t addr, uint8_t data) {   // this is address & 0xFF
        catchup_speech();

        uint8_t reg = addr & 0x0F;
        uint8_t chip = (addr & 0x80) ? 0 : 1;
    
        n6522[chip]->write(reg, data);

        // Mockingboard wiring: on every write to this VIA's Port B (control
        // lines to the paired AY), re-evaluate the AY bus cycle. The VIA
        // stays generic; the AY owns the BDIR/BC1/~RESET decode.
        if (reg == MB_6522_ORB) {
            uint8_t pa = n6522[chip]->get_ora() & n6522[chip]->get_ddra();
            uint8_t pb = n6522[chip]->get_orb() & n6522[chip]->get_ddrb();
            AyBusResult r = ay8910s->busCycle(chip, pa, pb, clock->get_vid_cycles());
            if (r.drove_data) {
                n6522[chip]->set_ira(r.data);
            } else {
                // AY is tri-stated (INACTIVE, LATCH, WRITE, RESET, or READ
                // with no register latched). The Mockingboard's Port A bus
                // has pull-ups, so the VIA sees $FF on its PA pins. Model
                // this so that TestAYReadHiZ and similar audits see the
                // "pulled high" bus that real hardware produces.
                n6522[chip]->set_ira(0xFF);
            }
        }

        // Mockingboard C: A6 selects the primary SSI-263. The VIA is still
        // selected (aliased); this write is additional.
        if (addr & 0x40) {
            ssi263->write(addr & 0x07, data);
            note_ar();
        }
    }
    
    uint8_t read(uint32_t addr) {               // this is address & 0xFF
        catchup_speech();

        uint8_t reg = addr & 0x0F;
        uint8_t chip = (addr & 0x80) ? 0 : 1;
    
        return n6522[chip]->read(reg);
    }
    
    void generate_frame() {
        static int frames = 0;

        const uint64_t frame_end_vid = clock->get_vid_cycles();
        const uint64_t vid_per_frame = clock->get_vid_cycles_per_frame();
        const uint64_t sample_vid = ay8910s->sampleVid();

        // Speaker-style stall: a stale SDL backlog (sleep / paused device)
        // keeps the queue above MAX_QUEUE every frame and would permanently
        // block generation unless we discard it when the synth horizon is
        // this far behind. generateSamples still snaps sample_vid_ and
        // drops pending register events.
        if (frame_end_vid > sample_vid &&
            (frame_end_vid - sample_vid) > vid_per_frame * 3) {
            SDL_ClearAudioStream(stream);
        }

        // Use SDL_GetAudioStreamQueued (input-format bytes) rather than
        // SDL_GetAudioStreamAvailable (output-format bytes). Stereo F32:
        // two floats per frame. 4410 frames is ~100ms at 44.1kHz.
        const size_t MAX_QUEUE = 4410;
        size_t queued_frames = 0;
        if (stream) {
            queued_frames = audio_system->get_stream_queued(stream) /
                            (sizeof(float) * 2);
        }
        int abs = 0;
        if (queued_frames < MAX_QUEUE) {
            catchup_speech();
            ay8910s->generateSamples(frame_end_vid);
            mix_speech();

            abs = (int)audio_buffer.size();
            if (abs > 0) {
                audio_system->put_stream_data(stream, audio_buffer.data(),
                                              audio_buffer.size() * sizeof(float));
            }
            audio_buffer.clear();
        }

        if (DEBUG(DEBUG_MOCKINGBOARD)) {
            if (frames++ > 60) {
                frames = 0;
                if (stream) {
                    queued_frames = audio_system->get_stream_queued(stream) /
                                    (sizeof(float) * 2);
                }
                printf("MB Status: queued frames: %zu, audio buffer size: %d\n",
                       queued_frames, abs);
            }
        }
    }

    void syncHorizon() {
        ay8910s->syncHorizon();
        if (!clock || !ssi263) {
            return;
        }
        const uint64_t frame_end_vid = clock->get_vid_cycles();
        const uint64_t vid_per_frame = clock->get_vid_cycles_per_frame();
        const uint64_t vid_per_sec = clock->get_vid_cycles_per_second();
        const uint64_t new_vid = (frame_end_vid > vid_per_frame)
                                     ? frame_end_vid - vid_per_frame
                                     : 0;
        if (new_vid > speech_vid_ && vid_per_sec) {
            ssi263->skip((double)(new_vid - speech_vid_) / (double)vid_per_sec);
        }
        speech_pending.clear();
        speech_vid_ = new_vid;
        speech_accum_ = 0;
        note_ar();
    }

    void reset() {
        n6522[0]->reset();
        n6522[1]->reset();
        ay8910s->reset();
        const double xck = clock ? (double)clock->get_vid_cycles_per_second() : 1020484.0;
        ssi263->reset(xck, (double)OUTPUT_SAMPLE_RATE_INT);
        speech_pending.clear();
        speech_vid_ = clock ? clock->get_vid_cycles() : 0;
        speech_accum_ = 0;
        // Port A pull-ups on the Mockingboard hold the bus high whenever
        // neither the AY nor the VIA is driving. Pre-seed IRA so the CPU
        // sees $FF on the very first ORA read (before any bus cycle).
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);
        n6522[0]->set_ca1(true);
    }

    DebugFormatter *debug() {
        DebugFormatter *df = new DebugFormatter();
        n6522[0]->debug(df);
        n6522[1]->debug(df);
        df->addLine("====== SSI-263 (primary $Cn40) ======");
        df->addLine("R0: %02X  R1: %02X  R2: %02X  R3: %02X  A/R: %d  CA1 pin: %d",
                    ssi263->reg(0), ssi263->reg(1), ssi263->reg(2), ssi263->reg(3),
                    (int)ssi263->request(), (int)n6522[0]->get_ca1());
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
    return mb_d->mockingboard->read(addr);
}

void init_slot_mockingboard(computer_t *computer, SlotType_t slot) {

    printf("init_slot_mockingboard: %d\n", slot);

    mb_cpu_data *mb_d = new mb_cpu_data;
    mb_d->id = DEVICE_ID_MOCKINGBOARD;
    mb_d->computer = computer;
    mb_d->clock = computer->clock;
    mb_d->audio_system = computer->audio_system;
    mb_d->irq_control = computer->irq_control;
    mb_d->vid = &computer->clock->vid;
    mb_d->mockingboard = new Mockingboard(computer->clock, computer->irq_control, mb_d->vid, computer->audio_system, slot);
    
    mb_d->slot = slot;

    computer->mmu->map_c1cf_page_write_h(0xC0 + slot, { mb_write_Cx00, mb_d }, "MB_IO");
    computer->mmu->map_c1cf_page_read_h(0xC0 + slot, { mb_read_Cx00, mb_d }, "MB_IO");

    mb_d->mockingboard->insert_empty_frame();

    // When the audio device format changes, AudioSystem clears all streams
    // and then invokes these callbacks so each generator can snap its
    // timing state forward. Without this, the cleared stream would
    // immediately refill with catch-up audio.
    computer->audio_system->register_device_reset_callback([mb_d]() {
        mb_d->mockingboard->syncHorizon();
    });

    // this can move to the class
    // set up a reset handler to reset the chips on mockingboard
    computer->register_reset_handler(
        [mb_d](bool cold_start) {
            mb_d->mockingboard->reset();
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
        "mockingboard",
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

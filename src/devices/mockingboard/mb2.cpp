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
    N6522 *n6522[2];
    AY8910s *ay8910s;
    SDL_AudioStream *stream;
    uint64_t last_cycle;
    uint8_t slot;
    EventTimer *event_timer;
    InterruptController *irq_control = nullptr;
    AudioSystem *audio_system;

    float samples_accumulated = 0.0f;
    float frame_rate;
    float samples_per_frame;
    uint32_t samples_per_frame_int;
    float samples_per_frame_remainder;
    uint64_t vid_cycles_rate;

    NClock *clock;

    // TODO: this is an undimensioned vector, which will be doing all kinds of memory allocation
    std::vector<float> audio_buffer;


public:
    Mockingboard(NClock *clock, InterruptController *irq_control, EventTimer *event_timer, AudioSystem *audio_system, uint8_t slot) {
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
        ay8910s = new AY8910s(&audio_buffer, event_timer, clock, audio_system /* , irq_control, slot */);

        this->clock = clock;
        this->irq_control = irq_control;
        this->event_timer = event_timer;
        this->slot = slot;
        this->audio_system = audio_system;
        last_cycle = 0;

        frame_rate = (double)clock->get_vid_cycles_per_second() / (double)clock->get_vid_cycles_per_frame();
        samples_per_frame = (float)OUTPUT_SAMPLE_RATE_INT / frame_rate;
        samples_per_frame_int = (int32_t)samples_per_frame;
        samples_per_frame_remainder = samples_per_frame - samples_per_frame_int;
        vid_cycles_rate = clock->get_vid_cycles_per_second();

        stream = audio_system->create_stream(OUTPUT_SAMPLE_RATE_INT, 2, SDL_AUDIO_F32LE, false);

        // Port A pull-ups hold the bus high at power-on; match reset().
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);
    }
    ~Mockingboard() {
        audio_system->destroy_stream(stream);

        delete n6522[0];
        delete n6522[1];
        delete ay8910s;
    }

    void insert_empty_frame() {
        const float empty_frame[736*2] = {0.0f};
        SDL_PutAudioStreamData(stream, empty_frame, 736 * 2 * sizeof(float));
    }
    
    void debug_registers();

    void write(uint32_t addr, uint8_t data) {   // this is address & 0xFF
        uint8_t reg = addr & 0x0F;
        uint8_t chip = (addr & 0x80) ? 0 : 1;
    
        n6522[chip]->write(reg, data);

        // Mockingboard wiring: on every write to this VIA's Port B (control
        // lines to the paired AY), re-evaluate the AY bus cycle. The VIA
        // stays generic; the AY owns the BDIR/BC1/~RESET decode.
        if (reg == MB_6522_ORB) {
            uint8_t pa = n6522[chip]->get_ora() & n6522[chip]->get_ddra();
            uint8_t pb = n6522[chip]->get_orb() & n6522[chip]->get_ddrb();
            double  t  = (double)clock->get_vid_cycles() / (double)vid_cycles_rate;
            AyBusResult r = ay8910s->busCycle(chip, pa, pb, t);
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
    }
    
    uint8_t read(uint32_t addr) {               // this is address & 0xFF
        uint8_t reg = addr & 0x0F;
        uint8_t chip = (addr & 0x80) ? 0 : 1;
    
        return n6522[chip]->read(reg);
    }
    
    void generate_frame() {
        static int frames = 0;

        samples_accumulated += samples_per_frame_remainder;
        uint32_t samples_this_frame = samples_per_frame_int;
        if (samples_accumulated >= 1.0f) {
            samples_this_frame = samples_per_frame_int+1;
            samples_accumulated -= 1.0f;
        }
    
        last_cycle = clock->get_vid_cycles();
    
        ay8910s->generateSamples(samples_this_frame);
    
        // Clear the audio buffer after each frame to prevent memory buildup
        // Send the generated audio data to the SDL audio stream
        int abs = audio_buffer.size();
        if (abs > 0) {
            //printf("generate_mockingboard_frame: %zu\n", mb_d->audio_buffer.size());
            SDL_PutAudioStreamData(stream, audio_buffer.data(), audio_buffer.size() * sizeof(float));
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

    void reset() {
        n6522[0]->reset();
        n6522[1]->reset();
        ay8910s->reset();
        // Port A pull-ups on the Mockingboard hold the bus high whenever
        // neither the AY nor the VIA is driving. Pre-seed IRA so the CPU
        // sees $FF on the very first ORA read (before any bus cycle).
        n6522[0]->set_ira(0xFF);
        n6522[1]->set_ira(0xFF);
    }

    DebugFormatter *debug() {
        DebugFormatter *df = new DebugFormatter();
        n6522[0]->debug(df);
        n6522[1]->debug(df);
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
    mb_d->event_timer = computer->vid_event_timer;
    mb_d->mockingboard = new Mockingboard(computer->clock, computer->irq_control, mb_d->event_timer, computer->audio_system, slot);
    
    mb_d->slot = slot;

    computer->mmu->map_c1cf_page_write_h(0xC0 + slot, { mb_write_Cx00, mb_d }, "MB_IO");
    computer->mmu->map_c1cf_page_read_h(0xC0 + slot, { mb_read_Cx00, mb_d }, "MB_IO");

    mb_d->mockingboard->insert_empty_frame();

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

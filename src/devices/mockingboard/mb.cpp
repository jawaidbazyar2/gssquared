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

#include "AY8910.hpp"

#include "gs2.hpp"
#include "cpu.hpp"
#include "mb.hpp"
#include "devices/speaker/speaker.hpp"
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
    mb_d->irq_control->set_irq((device_irq_id)mb_d->slot, irq_to_slot);
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
        /* if (next_counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
            next_counter = 65536;
        } */
        tc->t1_triggered_cycles += next_counter; // TODO: testing.
        mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + next_counter+1, mb_t1_timer_callback, instanceID , mb_d);
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
    
#if 0
        tc->t2_counter = tc->t2_latch;
        
        uint32_t counter = tc->t2_counter;
        /* if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
            counter = 65536;
        } */
        
        mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + counter + 1, mb_t2_timer_callback, instanceID , mb_d);
#endif
    if (1) { // one-shot mode (T2 only has one-shot mode?)
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
    //uint64_t cpu_cycles = mb_d->clock->get_cycles();
    uint64_t vid_cycles = mb_d->clock->get_vid_cycles();

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
                    double time = (double)vid_cycles / mb_d->vid_cycles_rate;
                    mb_d->mockingboard->queueRegisterChange(time, chip, i, 0);
                }
            } else if ((data & 0b111) == 7) { // this is the register number.
                tc->reg_num = tc->ora;
                if (DEBUG(DEBUG_MOCKINGBOARD)) printf("reg_num: %02x\n", tc->reg_num);
            } else if ((data & 0b111) == 6) { // write to the specified register
                // TODO: need to mask with ddrb
                //double time = cpu_cycles / 1020500.0;
                double time = (double)vid_cycles / mb_d->vid_cycles_rate;
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
            /* 6522 doc doesn't say it, but AppleWin and UltimaV clear timer1 interrupt flag when T1L_H is written. */
            mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            mb_6522_propagate_interrupt(mb_d);       
        
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
            //mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            tc->ifr.bits.timer1 = 0;
            mb_6522_propagate_interrupt(mb_d);
            tc->t1_latch = (tc->t1_latch & 0x00FF) | (data << 8);
            tc->t1_counter = tc->t1_latch;
            uint32_t next_counter = tc->t1_counter/*  ? tc->t1_counter : 65536 */;
            
            tc->t1_triggered_cycles = vid_cycles + next_counter + 1; // TODO: testing. this is icky. This might be 6502 cycle timing plus 6522 counter timing.
            tc->t1_oneshot_pending = 1;
            //if (tc->ier.bits.timer1) {
                mb_d->event_timer->scheduleEvent(tc->t1_triggered_cycles, mb_t1_timer_callback, 0x10000000 | (slot << 8) | chip , mb_d);
            /* } else {
                mb_d->event_timer->cancelEvents(0x10000000 | (slot << 8) | chip);
            } */
            }
            break;

        case MB_6522_T2C_L:
            tc->t2_latch = (tc->t2_latch & 0xFF00) | data;
            break;
        case MB_6522_T2C_H: {
            tc->t2_latch = (tc->t2_latch & 0x00FF) | (data << 8);
            tc->t2_counter = tc->t2_latch;
            uint32_t next_counter2 = tc->t2_counter /* ? tc->t2_counter : 65536 */;
            tc->ifr.bits.timer2 = 0;
            mb_6522_propagate_interrupt(mb_d);
            tc->t2_triggered_cycles = vid_cycles + next_counter2 + 1;
            tc->t2_oneshot_pending = 1;
            //if (tc->ier.bits.timer2) {
                mb_d->event_timer->scheduleEvent(tc->t2_triggered_cycles, mb_t2_timer_callback, 0x10010000 | (slot << 8) | chip , mb_d);
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
                uint64_t cycle_base = tc->t1_triggered_cycles == 0 ? mb_d->clock->get_vid_cycles() : tc->t1_triggered_cycles;
                uint32_t counter = tc->t1_counter;
                /* if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
                    counter = 65536;
                } */
                mb_d->event_timer->scheduleEvent(cycle_base + counter+1, mb_t1_timer_callback, instanceID , mb_d);
                                
                instanceID = 0x10010000 | (slot << 8) | chip;
                /* if (!tc->ier.bits.timer2) {
                    mb_d->event_timer->cancelEvents(instanceID);
                } else */ { // if we set the counter/latch BEFORE we enable interrupts.
                    uint64_t cycle_base = tc->t2_triggered_cycles == 0 ? mb_d->clock->get_vid_cycles() : tc->t2_triggered_cycles;
                    uint32_t counter = tc->t2_counter;
                    /* if (counter == 0) { // if they enable interrupts before setting the counter (and it's zero) set it to 65535 to avoid infinite loop.
                        counter = 65536;
                    } */
                    mb_d->event_timer->scheduleEvent(cycle_base + counter+1, mb_t2_timer_callback, instanceID , mb_d);
                }
            }
            break;
    }
}

inline uint64_t calc_cycle_diff_t1(mb_6522_regs *tc, uint64_t cycles) {
    // treat latch of 0 as 65535.
    uint32_t latchval = tc->t1_latch;
    /* if (latchval == 0) {
        latchval = 65536;
    } */
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
            return (latchval - ((cycles - tc->t1_triggered_cycles) % (latchval+1))) & 0xFFFF;
        }
    }
#endif
    //    return latchval - ((cycles - tc->t1_triggered_cycles) % latchval);
}

inline uint64_t calc_cycle_diff_t2(mb_6522_regs *tc, uint64_t cycles) {
    // treat latch of 0 as 65535.
    uint32_t latchval = tc->t2_latch;
    /* if (latchval == 0) {
        latchval = 65536;
    } */
    if (tc->t2_triggered_cycles == 0) return ( 0 - cycles) & 0xFFFF; // never triggered, so the "tick" is the current cycle.
    return (latchval - ((cycles - tc->t2_triggered_cycles) % (latchval+1))) & 0xFFFF;
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
            uint64_t cycle_diff = calc_cycle_diff_t1(&mb_d->d_6522[chip], mb_d->clock->get_vid_cycles());
            retval = cycle_diff & 0xFF;
            break;
        }
        case MB_6522_T1C_H:    {  // IFR Timer 1 flag cleared by read T1 counter high. pg 2-42
            // read of t1 counter high DOES NOT clear interrupt; write does.
            //mb_d->d_6522[chip].ifr.bits.timer1 = 0;
            //mb_6522_propagate_interrupt(cpu, mb_d);
            uint64_t cycle_diff = calc_cycle_diff_t1(&mb_d->d_6522[chip], mb_d->clock->get_vid_cycles());
            retval = (cycle_diff >> 8) & 0xFF;
            break;
        }
        case MB_6522_T2C_L: { /* 8 bits from T2 low order counter transferred to mpu - t2 interrupt flag is reset. */
            mb_d->d_6522[chip].ifr.bits.timer2 = 0;
            mb_6522_propagate_interrupt(mb_d);

            uint64_t cycle_diff = calc_cycle_diff_t2(&mb_d->d_6522[chip], mb_d->clock->get_vid_cycles());
            retval = (cycle_diff) & 0xFF;
            break;
        }
        case MB_6522_T2C_H: { /* 8 bits from T2 high order counter transferred to mpu */
            uint64_t cycle_diff = calc_cycle_diff_t2(&mb_d->d_6522[chip], mb_d->clock->get_vid_cycles());
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

    mb_d->last_cycle = mb_d->clock->get_vid_cycles();

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

void init_slot_mockingboard(computer_t *computer, SlotType_t slot) {

    uint16_t slot_base = 0xC080 + (slot * 0x10);
    printf("init_slot_mockingboard: %d\n", slot);

    mb_cpu_data *mb_d = new mb_cpu_data;
    mb_d->computer = computer;
    mb_d->clock = computer->clock;
    mb_d->audio_system = computer->audio_system;
    mb_d->irq_control = computer->irq_control;

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
    
    mb_d->event_timer = computer->vid_event_timer;

// TODO: create an "audiosystem" module and move this stuff to it like we did videosystem.
    speaker_state_t *speaker_d = (speaker_state_t *)get_module_state(computer->cpu, MODULE_SPEAKER);
    //int dev_id = speaker_d->device_id;

    mb_d->frame_rate = (double)mb_d->clock->get_vid_cycles_per_second() / (double)mb_d->clock->get_vid_cycles_per_frame();
    mb_d->samples_per_frame = (float)OUTPUT_SAMPLE_RATE_INT / mb_d->frame_rate;
    mb_d->samples_per_frame_int = (int32_t)mb_d->samples_per_frame;
    mb_d->samples_per_frame_remainder = mb_d->samples_per_frame - mb_d->samples_per_frame_int;

    mb_d->vid_cycles_rate = mb_d->clock->get_vid_cycles_per_second();
    mb_d->stream = mb_d->audio_system->create_stream(OUTPUT_SAMPLE_RATE_INT, 2, SDL_AUDIO_F32LE, false);

    set_slot_state(computer->cpu, slot, mb_d);
    computer->mmu->map_c1cf_page_write_h(0xC0 + slot, { mb_write_Cx00, mb_d }, "MB_IO");
    computer->mmu->map_c1cf_page_read_h(0xC0 + slot, { mb_read_Cx00, mb_d }, "MB_IO");

    insert_empty_mockingboard_frame(mb_d);

    // Set up Timer T1 on both chips
    mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + 65536, mb_t1_timer_callback, 0x10000000 | (slot << 8) | 0 , mb_d);
    mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + 65536, mb_t1_timer_callback, 0x10000000 | (slot << 8) | 1 , mb_d);
    // Set up Timer T2 on both chips
    mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + 65536, mb_t2_timer_callback, 0x10010000 | (slot << 8) | 0 , mb_d);
    mb_d->event_timer->scheduleEvent(mb_d->clock->get_vid_cycles() + 65536, mb_t2_timer_callback, 0x10010000 | (slot << 8) | 1 , mb_d);


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

    computer->register_shutdown_handler([mb_d]() {
        mb_d->audio_system->destroy_stream(mb_d->stream);
        //SDL_DestroyAudioStream(mb_d->stream);
        delete mb_d;
        return true;
    });

    computer->register_debug_display_handler(
        "mockingboard",
        DH_MOCKINGBOARD, // unique ID for this, need to have in a header.
        [mb_d]() -> DebugFormatter * {
            return debug_registers_6522(mb_d);
        }
    );

    // TODO: register a debug formatter handler, with a name, as a closure that will call here with the mb_d as context.
    // then user can say "debug mb_registers" to call this and get the formatter output to display in watch window.

}

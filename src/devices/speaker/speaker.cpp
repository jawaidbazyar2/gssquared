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

#include <cstdio>
#include <SDL3/SDL.h>

#include "gs2.hpp"
#include "debug.hpp"
#include "devices/speaker/speaker.hpp"
#include "devices/speaker/SpeakerFX.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "NClock.hpp"
#include "util/printf_helper.hpp"

// Utility function to round up to the next power of 2
inline uint32_t next_power_of_2(uint32_t value) {
    if (value == 0) return 1;
    if ((value & (value - 1)) == 0) return value; // Already a power of 2
    
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    
    return value;
}

uint64_t audio_generate_frame(computer_t *computer, cpu_state *cpu, uint64_t end_frame_c14M ) {

    speaker_state_t *speaker_state = (speaker_state_t *)get_module_state(cpu, MODULE_SPEAKER);
    NClock *clock = speaker_state->clock;

    //static uint64_t last_hz_rate = 0;
    // start speaker playback after we've loaded this first frame of samples.
    if (!speaker_state->sp->started()) {
        speaker_state->sp->start();
    }

    if ((end_frame_c14M - speaker_state->sp->last_event_time) > (clock->get_c14m_per_frame() * 3)) {
        printf("Speaker skew: 14m: %16llu %13llu %13llu\n", u64_t(clock->get_c14m()), u64_t(end_frame_c14M - speaker_state->sp->last_event_time), u64_t(clock->get_c14m_per_frame()));
        // Resync to start of current frame so generate_and_queue can advance to end_frame_c14M.
        // Reset must pair with generate_samples skipping stale events (event_time <= last_event_time).
        speaker_state->sp->reset(end_frame_c14M - clock->get_c14m_per_frame());
    }

    // This really doesn't do much of anything now. C14m (frame rate) calc is always the same.
    if (speaker_state->last_clock_mode != clock->get_clock_mode()) { // this will always trigger the 1st time through.
        if (DEBUG(DEBUG_SPEAKER)) printf("Old clock mode: %d, New clock mode: %d\n", speaker_state->last_clock_mode, clock->get_clock_mode());
        //if (speaker_state->last_clock_mode == CLOCK_FREE_RUN) speaker_state->sp->reset(end_frame_c14M - computer->clock->c14M_per_frame); // coming out of LS.
        
        speaker_state->last_clock_mode = clock->get_clock_mode();
        double frame_rate = (double)clock->get_c14m_per_second() / (double)clock->get_c14m_per_frame();
    
        if (DEBUG(DEBUG_SPEAKER)) speaker_state->sp->print();
    }

    // we can only generate and queue whole number of samples. But each Apple II frame here is not a whole number of samples.
    // sometimes we need to emit 735 samples, sometimes 736. This is done by tracking the remainder of samples_per_frame calculation,
    // adding it to an accumulator each frame. If we ever exceed 1.0f, then we need 1 extra sample this frame.
    // This is only necessary when using Apple II frame rate of 59.9227 fps and a sample rate of 44100 Hz.
    // In an environment where the frame rate is e.g. 60Hz even, 44100Hz divides evenly into 60 (735 samples per frame) so
    // no remainder and you probably don't need to do this trick.
    // So if you're using this code in a hardware device you can just peg everything at 60fps and 44100Hz and not worry about it.

    size_t queued = speaker_state->sp->get_queued_samples();
    const size_t MAX_QUEUE = 4410;  // ~100ms at 44.1kHz, adjust as needed
    
    if (queued < MAX_QUEUE) {
        speaker_state->samples_accumulated += speaker_state->samples_per_frame_remainder;
        uint32_t samples_this_frame = speaker_state->samples_per_frame_int;
        if (speaker_state->samples_accumulated >= 1.0f) {
            samples_this_frame ++;
            speaker_state->samples_accumulated -= 1.0f;
        }

        // Generate and queue your 735 samples
        uint64_t samps = speaker_state->sp->generate_and_queue(samples_this_frame, end_frame_c14M);
        speaker_state->samples_added += samps;
        speaker_state->sample_frames ++;
        return samps;

    } else {
        // Skip this frame's audio generation, queue is full enough
        return 0;
    }

/*     // I need to know the actual end of the current frame in cycles; cpu->cycles is not right.
    uint64_t samps = speaker_state->sp->generate_and_queue(samples_this_frame, end_frame_c14M);
    speaker_state->samples_added += samps;
    speaker_state->sample_frames ++;

    return samps; */
}

inline void log_speaker_blip(speaker_state_t *speaker_state) {
    speaker_state->sp->event_buffer->add_event({speaker_state->clock->get_c14m(), (uint64_t)(speaker_state->audio_system->get_volume())});

    if (speaker_state->speaker_recording) {
        fprintf(speaker_state->speaker_recording, "%llu\n", u64_t(speaker_state->clock->get_cycles()));
    }
}

uint8_t speaker_memory_read(void *context, uint32_t address) {
    speaker_state_t *speaker_state = (speaker_state_t *)context;
    log_speaker_blip(speaker_state);
    
    return (speaker_state->mmu->floating_bus_read());
}

void speaker_memory_write(void *context, uint32_t address, uint8_t value) {
    speaker_state_t *speaker_state = (speaker_state_t *)context;
    log_speaker_blip(speaker_state);
}

DebugFormatter * debug_speaker(speaker_state_t *ds) {
    static uint32_t counter = 0;
    static uint16_t samplecounts[60] = {0};
    DebugFormatter *df = new DebugFormatter();
        
    //df->addLine("   Speaker ");
    uint16_t samples = ds->sp->get_queued_samples();

    uint64_t frame_index = (samples * 10) / ds->samples_per_frame;
    samplecounts[counter++] = samples;
    if (counter == 60) counter = 0;
    uint32_t samplesum = 0;
    for (uint32_t i = 0; i < 60; i++) {
        samplesum += samplecounts[i];
    }
    uint32_t samplesavg = samplesum / 60;
    uint64_t skew = ds->clock->get_c14m() - ds->sp->last_event_time;
    df->addLine("  Skew: %13llu", skew);
    df->addLine("  Samples: ---------+---------+---------+---------+---------+", samples);
    df->addLine("  %6d : %.*s|            ", samples, frame_index, "                                                  ");
    df->addLine("  Samples Avg: %6d %12.6f", samplesavg, (double)ds->samples_added / (double)ds->sample_frames);
    df->addLine("  Polarity: %10llu / %10llu", ds->sp->polarity, ds->sp->polarity_impulse);
    df->addLine("  last_event: %13llu Rect_Rem: %13llu::%13llu", ds->sp->last_event_time, ds->sp->rect_remain>>FRACTION_BITS, ds->sp->rect_remain & FRACTION_MASK);

    df->addLine("  Fr Rate: %12.8f   Samp/Fr: %12.7f   Cycle/Samp: %llu::%llu", ds->frame_rate, ds->samples_per_frame, u64_t(ds->sp->cycles_per_sample>>FRACTION_BITS), u64_t(ds->sp->cycles_per_sample & FRACTION_MASK));
    df->addLine("  Accumulated: %12.8f", ds->samples_accumulated);
    df->addLine("  Device Started: %6d", ds->sp->started() ? 1 : 0);
    // TODO: what else should this display here?
    return df;
}

void init_mb_speaker(computer_t *computer,  SlotType_t slot) {

    cpu_state *cpu = computer->cpu;

    speaker_state_t *speaker_state = new speaker_state_t;

    speaker_state->computer = computer;
    speaker_state->clock = computer->clock;
    speaker_state->audio_system = computer->audio_system;
    speaker_state->mmu = computer->mmu;

    double frame_rate = (double)speaker_state->clock->get_c14m_per_second() / (double)speaker_state->clock->get_c14m_per_frame();

    speaker_state->sp = new SpeakerFX(speaker_state->audio_system, speaker_state->clock->get_c14m_per_second(), 44100, 128*1024, 4096);
    speaker_state->event_buffer = speaker_state->sp->event_buffer;
    
    speaker_state->speaker_recording = nullptr;

    set_module_state(cpu, MODULE_SPEAKER, speaker_state);
	
    //speaker_state->device_id = speaker_state->sp->get_device_id();

    printf("frame rate: %f\n", frame_rate);
    speaker_state->frame_rate = frame_rate;
    speaker_state->samples_per_frame = 44100.0f / frame_rate;
    speaker_state->samples_per_frame_int = (int32_t)speaker_state->samples_per_frame;
    speaker_state->samples_per_frame_remainder = speaker_state->samples_per_frame - speaker_state->samples_per_frame_int;
    speaker_state->samples_accumulated = 0.0f;

    // prime the pump with a few frames of silence - make sure we use the correct frame size so we pass the check in generate.
    //memset(speaker_state->working_buffer, 0, speaker_state->sp->samples_per_frame * sizeof(int16_t));
    //speaker_state->sp->generate_and_queue(4410, 0);
    speaker_state->sp->prebuffer();

    if (DEBUG(DEBUG_SPEAKER)) fprintf(stdout, "init_speaker\n");
    uint16_t speaker_addr_max = (computer->platform->id == PLATFORM_APPLE_IIGS) ? 0xC030 : 0xC03F; // IIGS, speaker is ONLY at C030
    for (uint16_t addr = 0xC030; addr <= speaker_addr_max; addr++) {
        computer->mmu->set_C0XX_read_handler(addr, { speaker_memory_read, speaker_state });
        computer->mmu->set_C0XX_write_handler(addr, { speaker_memory_write, speaker_state });
    }
#if 0
    speaker_state->preFilter = new LowPassFilter();
    speaker_state->preFilter->setCoefficients(8000.0f, (double)1020500); // 1020500 is actual possible sample rate of input toggles.
    speaker_state->postFilter = new LowPassFilter();
    speaker_state->postFilter->setCoefficients(8000.0f, (double)SAMPLE_RATE);
#endif

    computer->register_shutdown_handler([speaker_state]() {
        speaker_state->sp->stop();
        delete speaker_state->sp;
        delete speaker_state;
        return true;
    });

    computer->register_debug_display_handler(
        "speaker",
        DH_SPEAKER, // unique ID for this, need to have in a header.
        [speaker_state]() -> DebugFormatter * {
            return debug_speaker(speaker_state);
        }
    );

    speaker_state->sp->start();
}


#if 0
/**
 * this is for debugging. This will write a log file with the following data:
 * the time in cycles of every 'speaker blip'.
 */

void toggle_speaker_recording(cpu_state *cpu)
{
    speaker_state_t *speaker_state = (speaker_state_t *)get_module_state(cpu, MODULE_SPEAKER);

    if (speaker_state->speaker_recording == nullptr) {
        speaker_state->speaker_recording = fopen("speaker_event_log.txt", "w");
    } else {
        fclose(speaker_state->speaker_recording);
        speaker_state->speaker_recording = nullptr;
    }
};
#endif
